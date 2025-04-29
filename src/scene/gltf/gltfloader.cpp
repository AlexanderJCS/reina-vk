#include "gltfloader.h"
#include "fastgltf/tools.hpp"

#include <glm/glm.hpp>

#include <filesystem>

#include <fastgltf/core.hpp>
#include <iostream>
#include <unordered_set>


fastgltf::Asset reina::scene::gltf::loadGltf(const std::string& filepath) {
    const std::filesystem::path path{filepath};

    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Failed to find glTF file: " + filepath);
    }

    // 1) Map the file into memory
    auto bufferOrErr = fastgltf::MappedGltfFile::FromPath(path);
    if (!bufferOrErr) {
        throw std::runtime_error(
                "Failed to open glTF file: " + filepath + "\nError: " +
                std::string(fastgltf::getErrorMessage(bufferOrErr.error()))
        );
    }

    // 2) Set up parser & options
    static constexpr auto supportedExts =
            fastgltf::Extensions::KHR_mesh_quantization |
            fastgltf::Extensions::KHR_texture_transform |
            fastgltf::Extensions::KHR_materials_variants;

    constexpr auto loadOpts =
            fastgltf::Options::DontRequireValidAssetMember |
            fastgltf::Options::AllowDouble |
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::LoadExternalImages |
            fastgltf::Options::GenerateMeshIndices;

    fastgltf::Parser parser(supportedExts);

    // 3) Auto-detect file type and parse
    auto fileType = fastgltf::determineGltfFileType(bufferOrErr.get());
    auto assetOrErr =
            (fileType == fastgltf::GltfType::GLB)
            ? parser.loadGltfBinary(bufferOrErr.get(), path.parent_path(), loadOpts)
            : parser.loadGltf     (bufferOrErr.get(), path.parent_path(), loadOpts);

    if (assetOrErr.error() != fastgltf::Error::None) {
        throw std::runtime_error(
                "Failed to parse glTF: " +
                std::string(fastgltf::getErrorMessage(assetOrErr.error()))
        );
    }

    // 4) Return the fully‐loaded Asset
    return std::move(assetOrErr.get());
}

void reina::scene::gltf::loadMeshTBNs(fastgltf::Asset& asset, std::vector<MeshTBN>& outMeshes) {
    // — Gather meshes used by default scene
    std::unordered_set<size_t> used;
    size_t sceneIdx = asset.defaultScene.value_or(0);
    fastgltf::iterateSceneNodes(asset, sceneIdx, fastgltf::math::fmat4x4(1.0f),
                                [&](fastgltf::Node& node, const fastgltf::math::fmat4x4&) {
                                    if (node.meshIndex.has_value()) used.insert(*node.meshIndex);
                                });

    // — For each used mesh → each primitive
    for (auto mi : used) {
        const auto& mesh = asset.meshes[mi];
        for (const auto& prim : mesh.primitives) {
            MeshTBN m;

            // - MATERIAL
            m.materialIdx = static_cast<int>(prim.materialIndex.value_or(-1));

            // — POSITION
            if (auto a = prim.findAttribute("POSITION")) {
                const auto& acc = asset.accessors[a->accessorIndex];
                m.vertices.resize(acc.count);
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset, acc,
                        [&](const fastgltf::math::fvec3& p, std::size_t i) {
                            m.vertices[i].position = p;
                        }
                );
            }
            // — NORMAL
            if (auto a = prim.findAttribute("NORMAL")) {
                const auto& acc = asset.accessors[a->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    asset, acc,
                    [&](fastgltf::math::fvec3 n, size_t i) {
                        m.vertices[i].normal = n;
                    });
            }
            // — TANGENT: may be Vec4 (x,y,z + w sign) or fallback Vec3
            if (auto tanIt = prim.findAttribute("TANGENT"); tanIt != prim.attributes.end()) {
                const auto& acc = asset.accessors[tanIt->accessorIndex];

                // Vec4 case: apply w as bitangent sign
                if (acc.type == fastgltf::AccessorType::Vec4) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                        asset, acc,
                        [&](const fastgltf::math::fvec4& t, std::size_t i) {
                            // tangent.xyz
                            m.vertices[i].tangent  = { t.x(), t.y(), t.z() };
                            // bitangent = cross(normal, tangent) * w
                            m.vertices[i].bitangent =
                                    fastgltf::math::cross(m.vertices[i].normal,
                                                          m.vertices[i].tangent)
                                    * t.w();
                        }
                    );
                }
                    // Vec3 fallback: no handedness, assume w == +1
                else if (acc.type == fastgltf::AccessorType::Vec3) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset, acc,
                        [&](const fastgltf::math::fvec3& t, std::size_t i) {
                            m.vertices[i].tangent   = t;
                            // bitangent = cross(normal, tangent)
                            m.vertices[i].bitangent =
                                    fastgltf::math::cross(m.vertices[i].normal,
                                                          m.vertices[i].tangent);
                        }
                    );
                }
                else {
                    // Unexpected accessor type—either skip or log a warning
                    std::cerr << "Warning: TANGENT accessor is neither Vec4 nor Vec3\n";
                }
            }
            // — TEXCOORD_0
            if (auto a = prim.findAttribute("TEXCOORD_0")) {
                if (a != prim.attributes.end()) {
                    const auto& acc = asset.accessors[a->accessorIndex];

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                            asset, acc,
                            [&](fastgltf::math::fvec2 uv, size_t i) {
                                m.vertices[i].uv = uv;
                            });
                } else {
                    // Fallback with UV (0, 0)
                    for (auto& vertex : m.vertices) {
                        vertex.uv = fastgltf::math::fvec2(0, 0);
                    }
                }
            }
            // — INDICES
            if (prim.indicesAccessor.has_value()) {
                const auto& acc = asset.accessors[*prim.indicesAccessor];
                m.indices.resize(acc.count);
                fastgltf::copyFromAccessor<uint32_t>(asset, acc, m.indices.data());
            }

            outMeshes.emplace_back(std::move(m));
        }
    }
}

reina::scene::ModelData reina::scene::gltf::MeshTBN::toModelData() {
    reina::scene::ModelData modelData;

    for (const VertexTBN& vertex : vertices) {
        modelData.vertices.push_back(vertex.position.x());
        modelData.vertices.push_back(vertex.position.y());
        modelData.vertices.push_back(vertex.position.z());
        modelData.vertices.push_back(1.0f);

        glm::mat3 tbn = glm::mat3(
                glm::vec3(vertex.tangent.x(), vertex.tangent.y(), vertex.tangent.z()),
                glm::vec3(vertex.bitangent.x(), vertex.bitangent.y(), vertex.bitangent.z()),
                glm::vec3(vertex.normal.x(), vertex.normal.y(), vertex.normal.z())
                );

        modelData.tbns.push_back(tbn);

        modelData.texCoords.push_back(vertex.uv.x());
        modelData.texCoords.push_back(vertex.uv.y());
    }

    for (uint32_t idx : indices) {
        modelData.indices.push_back(idx);
        modelData.tbnsIndices.push_back(idx);
        modelData.texIndices.push_back(idx);
    }

    return modelData;
}

std::unordered_map<uint32_t, uint32_t> addTexturesToScene(fastgltf::Asset& asset, reina::scene::Scene& scene) {
    std::unordered_map<uint32_t, uint32_t> gltfIdToSceneId;

    for (uint32_t i = 0; i < asset.images.size(); i++) {
        // https://vkguide.dev/docs/new_chapter_5/gltf_textures/
        fastgltf::Image& img = asset.images[i];

        std::visit(fastgltf::visitor{
            [](auto& arg) {
                throw std::runtime_error("Could not parse texture; internal gLTF data type not supported");
            },
            [&](fastgltf::sources::URI& uri) {
                if (uri.fileByteOffset != 0) {
                    throw std::runtime_error("URI image must have no offset");
                } if (!uri.uri.isLocalPath()) {
                    throw std::runtime_error("Non-local URIs are not supported for texture loading");
                }

                std::string path = uri.uri.fspath().string();

                uint32_t texIDScene = scene.defineTexture(path);
                gltfIdToSceneId[i] = texIDScene;
            },
            [&](fastgltf::sources::Vector& vector) {
                uint32_t texIDScene = scene.defineTexture(vector.bytes.data(), vector.bytes.size());
                gltfIdToSceneId[i] = texIDScene;
            },
            [&](fastgltf::sources::Array& array) {
                uint32_t texIDScene = scene.defineTexture(array.bytes.data(), array.bytes.size());
                gltfIdToSceneId[i] = texIDScene;
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor{
                        [](auto&&) {
                            throw std::runtime_error("Unsupported buffer data source");
                        },
                        [&](fastgltf::sources::Vector& vector) {
                            auto ptr = vector.bytes.data() + bufferView.byteOffset;
                            uint32_t texIDScene = scene.defineTexture(ptr, bufferView.byteLength);
                            gltfIdToSceneId[i] = texIDScene;
                        },
                        [&](fastgltf::sources::Array& array) {
                            auto ptr = array.bytes.data() + bufferView.byteOffset;

                            uint32_t texIDScene = scene.defineTexture(ptr, bufferView.byteLength);
                            gltfIdToSceneId[i] = texIDScene;
                        }
                }, buffer.data);
            },
        }, img.data);
    }

    return gltfIdToSceneId;
}

std::unordered_map<uint32_t, uint32_t> reina::scene::gltf::addMeshesToScene(reina::scene::Scene& scene, std::vector<reina::scene::gltf::MeshTBN> modelData) {
    std::unordered_map<uint32_t, uint32_t> gltfIdToSceneId;

    for (uint32_t i = 0; i < modelData.size(); i++) {
        uint32_t sceneID = scene.defineObject(modelData[i].toModelData());
        gltfIdToSceneId[i] = sceneID;
    }

    return gltfIdToSceneId;
}

glm::mat4 toGlmMat4(const fastgltf::math::fmat4x4& mat) {
    glm::mat4 result(1.0f);  // identity

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result[col][row] = mat[row][col];  // glm is column-major
        }
    }

    return result;
}

void reina::scene::gltf::addInstancesToScene(fastgltf::Asset &asset, reina::scene::Scene& scene,
                                             const std::unordered_map<uint32_t, uint32_t>& gltfIdToSceneId,
                                             const std::vector<reina::scene::Material>& materials) {
    size_t sceneIdx = asset.defaultScene.value_or(0);

    fastgltf::iterateSceneNodes(
            asset,
            sceneIdx,
            fastgltf::math::fmat4x4(),
            [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix) {
                if (node.meshIndex.has_value()) {
                    uint32_t sceneID = gltfIdToSceneId.at(static_cast<uint32_t>(node.meshIndex.value()));
                    Material material = materials[node.meshIndex.value()];

                    scene.addInstance(sceneID, toGlmMat4(matrix), material);
                }
            });
}

std::vector<reina::scene::Material> reina::scene::gltf::materialsFromMeshTBNs(fastgltf::Asset &asset, const std::vector<MeshTBN>& meshes, std::unordered_map<uint32_t, uint32_t> gltfTexIdToSceneId) {
    std::vector<Material> materials;

    for (const MeshTBN& mesh : meshes) {
        const auto& gltfMaterial = asset.materials[mesh.materialIdx];

        Material material{0, -1, -1, -1, glm::vec3(0.9f), glm::vec3(0.0f), 0.0f, false, 0.0f, true};

        if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
            std::cout << gltfMaterial.pbrData.baseColorTexture.value().textureIndex << "\n";

            try {
                material.textureID = static_cast<int>(
                        gltfTexIdToSceneId.at(
                                static_cast<uint32_t>(gltfMaterial.pbrData.baseColorTexture.value().textureIndex)
                        )
                );
            } catch (const std::out_of_range& e) {
                std::cerr << "Warning: Texture ID not found. Exception: " << e.what() << std::endl;
                material.textureID = -1;  // Fallback
            }
        }

        materials.push_back(material);
    }

    return materials;
}

reina::scene::Scene reina::scene::gltf::loadScene(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::string& filepath) {
    auto asset = loadGltf(filepath);

    std::vector<MeshTBN> meshes;
    loadMeshTBNs(asset, meshes);

    Scene scene;
    auto gltfModelIdToSceneId = addMeshesToScene(scene, meshes);
    auto gltfTexIdToSceneId = addTexturesToScene(asset, scene);
    auto materials = materialsFromMeshTBNs(asset, meshes, gltfTexIdToSceneId);
    addInstancesToScene(asset, scene, gltfModelIdToSceneId, materials);
    reina::scene::Material lightMaterial{0, -1, -1, -1, glm::vec3(0.9f), glm::vec3(16.0f), 0.0f, false, 0.0f, true};

    scene.addObject("models/cornell_light.obj", glm::mat4(1.0f), lightMaterial);
    scene.build(logicalDevice, physicalDevice, cmdPool, queue);

    return scene;
}
