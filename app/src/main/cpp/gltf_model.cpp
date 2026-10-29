#include "gltf_model.h"

#include <android/asset_manager.h>
#include <android/log.h>
#include <GLES3/gl3.h>

#include <vector>
#include <utility>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <cstdint>

#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf.h"

#define GLTF_LOG(...) \
    __android_log_print(ANDROID_LOG_ERROR, "LIFE_GLTF", __VA_ARGS__)

struct GltfVertex {
    float x, y, z;
    float nx, ny, nz;
};

struct CpuPrimitive {
    std::vector<GltfVertex> vertices;
    std::vector<uint32_t> indices;
    float color[4] = {0.7f, 0.7f, 0.7f, 1.0f};
};

struct GpuPrimitive {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei count = 0;
    float color[4] = {0.7f, 0.7f, 0.7f, 1.0f};
};

static std::vector<GpuPrimitive> gpuMeshes;
static GLuint gltfProgram = 0;
static bool modelLoaded = false;

static const char* gltfVertexShader = R"(
#version 300 es
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;

uniform mat4 mvp;
uniform mat4 model;

out vec3 vNormal;

void main() {
    vNormal = normalize(mat3(model) * normal);
    gl_Position = mvp * vec4(position, 1.0);
}
)";

static const char* gltfFragmentShader = R"(
#version 300 es
precision mediump float;

in vec3 vNormal;
uniform vec4 baseColor;
out vec4 outputColor;

void main() {
    vec3 lightDirection = normalize(vec3(-0.4, 0.8, 0.6));
    float diffuse = max(dot(normalize(vNormal), lightDirection), 0.0);
    float lighting = 0.35 + 0.65 * diffuse;
    outputColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
)";

static GLuint makeShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char message[1024] = {};
        glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
        GLTF_LOG("Shader compilation failed: %s", message);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static bool makeProgram() {
    GLuint vs = makeShader(GL_VERTEX_SHADER, gltfVertexShader);
    GLuint fs = makeShader(GL_FRAGMENT_SHADER, gltfFragmentShader);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    gltfProgram = glCreateProgram();
    glAttachShader(gltfProgram, vs);
    glAttachShader(gltfProgram, fs);
    glLinkProgram(gltfProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint success = 0;
    glGetProgramiv(gltfProgram, GL_LINK_STATUS, &success);

    if (!success) {
        char message[1024] = {};
        glGetProgramInfoLog(gltfProgram, sizeof(message), nullptr, message);
        GLTF_LOG("Program linking failed: %s", message);
        glDeleteProgram(gltfProgram);
        gltfProgram = 0;
        return false;
    }

    return true;
}

static cgltf_accessor* findAttribute(
    cgltf_primitive* primitive,
    cgltf_attribute_type type
) {
    for (cgltf_size i = 0; i < primitive->attributes_count; ++i) {
        if (primitive->attributes[i].type == type &&
            primitive->attributes[i].index == 0) {
            return primitive->attributes[i].data;
        }
    }

    return nullptr;
}

static void transformPosition(
    const cgltf_float* matrix,
    float x, float y, float z,
    float& ox, float& oy, float& oz
) {
    ox = matrix[0] * x + matrix[4] * y +
         matrix[8] * z + matrix[12];

    oy = matrix[1] * x + matrix[5] * y +
         matrix[9] * z + matrix[13];

    oz = matrix[2] * x + matrix[6] * y +
         matrix[10] * z + matrix[14];
}

bool initGltfModel(AAssetManager* assets) {
    modelLoaded = false;
    gpuMeshes.clear();

    if (!assets) {
        GLTF_LOG("Asset manager is null");
        return false;
    }

    AAsset* asset = AAssetManager_open(
        assets,
        "models/WorkerMale.glb",
        AASSET_MODE_BUFFER
    );

    if (!asset) {
        GLTF_LOG("Could not open models/WorkerMale.glb");
        return false;
    }

    const off_t fileSize = AAsset_getLength(asset);

    if (fileSize <= 0) {
        AAsset_close(asset);
        GLTF_LOG("GLB file is empty");
        return false;
    }

    std::vector<unsigned char> fileData(
        static_cast<size_t>(fileSize)
    );

    const int bytesRead = AAsset_read(
        asset, fileData.data(), fileData.size()
    );

    AAsset_close(asset);

    if (bytesRead != fileSize) {
        GLTF_LOG("Could not read complete GLB file");
        return false;
    }

    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse(
        &options,
        fileData.data(),
        fileData.size(),
        &data
    );

    if (result != cgltf_result_success || !data) {
        GLTF_LOG("GLB parsing failed: %d", (int)result);
        return false;
    }

    result = cgltf_load_buffers(
        &options, data, "models/WorkerMale.glb"
    );

    if (result != cgltf_result_success) {
        GLTF_LOG("GLB buffer loading failed: %d", (int)result);
        cgltf_free(data);
        return false;
    }

    std::vector<CpuPrimitive> cpuMeshes;

    float minX = 1.0e30f, minY = 1.0e30f, minZ = 1.0e30f;
    float maxX = -1.0e30f, maxY = -1.0e30f, maxZ = -1.0e30f;

    for (cgltf_size ni = 0; ni < data->nodes_count; ++ni) {
        cgltf_node* node = &data->nodes[ni];

        if (!node->mesh) continue;

        cgltf_float world[16];
        cgltf_node_transform_world(node, world);

        for (cgltf_size pi = 0; pi < node->mesh->primitives_count; ++pi) {
            cgltf_primitive* primitive = &node->mesh->primitives[pi];

            if (primitive->type != cgltf_primitive_type_triangles)
                continue;

            cgltf_accessor* positions = findAttribute(
                primitive, cgltf_attribute_type_position
            );

            if (!positions || positions->count == 0)
                continue;

            cgltf_accessor* normals = findAttribute(
                primitive, cgltf_attribute_type_normal
            );

            CpuPrimitive output;

            if (primitive->material &&
                primitive->material->has_pbr_metallic_roughness) {
                for (int c = 0; c < 4; ++c) {
                    output.color[c] =
                        primitive->material->pbr_metallic_roughness
                        .base_color_factor[c];
                }
            }

            output.vertices.reserve(positions->count);

            for (cgltf_size vi = 0; vi < positions->count; ++vi) {
                cgltf_float p[3] = {0, 0, 0};
                cgltf_float n[3] = {0, 1, 0};

                if (!cgltf_accessor_read_float(
                        positions, vi, p, 3)) {
                    continue;
                }

                if (normals) {
                    cgltf_accessor_read_float(normals, vi, n, 3);
                }

                GltfVertex vertex{};

                transformPosition(
                    world, p[0], p[1], p[2],
                    vertex.x, vertex.y, vertex.z
                );

                float nx = world[0] * n[0] +
                           world[4] * n[1] +
                           world[8] * n[2];

                float ny = world[1] * n[0] +
                           world[5] * n[1] +
                           world[9] * n[2];

                float nz = world[2] * n[0] +
                           world[6] * n[1] +
                           world[10] * n[2];

                float length = std::sqrt(nx*nx + ny*ny + nz*nz);

                if (length > 0.00001f) {
                    nx /= length;
                    ny /= length;
                    nz /= length;
                }

                vertex.nx = nx;
                vertex.ny = ny;
                vertex.nz = nz;

                minX = std::min(minX, vertex.x);
                minY = std::min(minY, vertex.y);
                minZ = std::min(minZ, vertex.z);
                maxX = std::max(maxX, vertex.x);
                maxY = std::max(maxY, vertex.y);
                maxZ = std::max(maxZ, vertex.z);

                output.vertices.push_back(vertex);
            }

            if (primitive->indices) {
                output.indices.reserve(primitive->indices->count);

                for (cgltf_size ii = 0;
                     ii < primitive->indices->count; ++ii) {
                    output.indices.push_back(
                        (uint32_t)cgltf_accessor_read_index(
                            primitive->indices, ii
                        )
                    );
                }
            } else {
                for (uint32_t ii = 0;
                     ii < output.vertices.size(); ++ii) {
                    output.indices.push_back(ii);
                }
            }

            if (!output.vertices.empty() &&
                !output.indices.empty()) {
                cpuMeshes.push_back(std::move(output));
            }
        }
    }

    cgltf_free(data);

    if (cpuMeshes.empty() || maxY <= minY) {
        GLTF_LOG("No usable triangle meshes found");
        return false;
    }

    const float modelHeight = maxY - minY;
    const float scale = 1.8f / modelHeight;
    const float centerX = (minX + maxX) * 0.5f;
    const float centerZ = (minZ + maxZ) * 0.5f;

    for (CpuPrimitive& mesh : cpuMeshes) {
        for (GltfVertex& vertex : mesh.vertices) {
            vertex.x = (vertex.x - centerX) * scale;
            vertex.y = (vertex.y - minY) * scale;
            vertex.z = (vertex.z - centerZ) * scale;
        }

        GpuPrimitive gpu;
        gpu.count = (GLsizei)mesh.indices.size();

        for (int c = 0; c < 4; ++c)
            gpu.color[c] = mesh.color[c];

        glGenVertexArrays(1, &gpu.vao);
        glBindVertexArray(gpu.vao);

        glGenBuffers(1, &gpu.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            mesh.vertices.size() * sizeof(GltfVertex),
            mesh.vertices.data(),
            GL_STATIC_DRAW
        );

        glGenBuffers(1, &gpu.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            mesh.indices.size() * sizeof(uint32_t),
            mesh.indices.data(),
            GL_STATIC_DRAW
        );

        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE,
            sizeof(GltfVertex),
            (void*)offsetof(GltfVertex, x)
        );
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE,
            sizeof(GltfVertex),
            (void*)offsetof(GltfVertex, nx)
        );
        glEnableVertexAttribArray(1);

        gpuMeshes.push_back(gpu);
    }

    glBindVertexArray(0);

    if (!makeProgram()) {
        destroyGltfModel();
        return false;
    }

    modelLoaded = true;

    GLTF_LOG("Loaded GLB model: %d primitives",
             (int)gpuMeshes.size());

    return true;
}

bool isGltfModelLoaded() {
    return modelLoaded;
}

void drawGltfModel(const float* viewProjection, float playerX, float playerZ, float playerYaw) {
    if (!modelLoaded || !gltfProgram) return;

    const float c = std::cos(playerYaw);
    const float sn = std::sin(playerYaw);

    float model[16] = {
         c, 0, -sn, 0,
         0, 1,   0, 0,
        sn, 0,   c, 0,
        playerX, 0, playerZ, 1
    };

    float mvp[16] = {};

    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            for (int k = 0; k < 4; ++k) {
                mvp[c*4+r] +=
                    viewProjection[k*4+r] * model[c*4+k];
            }
        }
    }

    glUseProgram(gltfProgram);

    glUniformMatrix4fv(
        glGetUniformLocation(gltfProgram, "model"),
        1, GL_FALSE, model
    );

    glUniformMatrix4fv(
        glGetUniformLocation(gltfProgram, "mvp"),
        1, GL_FALSE, mvp
    );

    for (const GpuPrimitive& mesh : gpuMeshes) {
        glUniform4fv(
            glGetUniformLocation(gltfProgram, "baseColor"),
            1, mesh.color
        );

        glBindVertexArray(mesh.vao);

        glDrawElements(
            GL_TRIANGLES,
            mesh.count,
            GL_UNSIGNED_INT,
            nullptr
        );
    }

    glBindVertexArray(0);
}

void destroyGltfModel() {
    for (GpuPrimitive& mesh : gpuMeshes) {
        if (mesh.ebo) glDeleteBuffers(1, &mesh.ebo);
        if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
        if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
    }

    gpuMeshes.clear();

    if (gltfProgram) {
        glDeleteProgram(gltfProgram);
        gltfProgram = 0;
    }

    modelLoaded = false;
}
