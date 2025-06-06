#include "gltfloader.h"
#include "fastgltf/tools.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

#include <fastgltf/core.hpp>
#include <iostream>
#include <unordered_set>

#include "mikktspace.h"


static int getNumFaces(const SMikkTSpaceContext* ctx) {
    auto* mesh = static_cast<reina::scene::gltf::Primitive*>(ctx->m_pUserData);
    // 3 verts per face → #faces = totalVerts / 3
    return static_cast<int>(mesh->vertices.size() / 3);
}

static int getNumVertsOfFace(const SMikkTSpaceContext* ctx, int /*iFace*/) {
    return 3;
}

static void getPosition(const SMikkTSpaceContext* ctx, float outPos[],
                        int iFace, int iVert) {
    auto* mesh = static_cast<reina::scene::gltf::Primitive*>(ctx->m_pUserData);
    int idx = iFace * 3 + iVert;
    outPos[0] = mesh->vertices[idx].position[0];
    outPos[1] = mesh->vertices[idx].position[1];
    outPos[2] = mesh->vertices[idx].position[2];
}

static void getNormal(const SMikkTSpaceContext* ctx, float outNorm[],
                      int iFace, int iVert) {
    auto* mesh = static_cast<reina::scene::gltf::Primitive*>(ctx->m_pUserData);
    int idx = iFace * 3 + iVert;
    outNorm[0] = mesh->vertices[idx].normal[0];
    outNorm[1] = mesh->vertices[idx].normal[1];
    outNorm[2] = mesh->vertices[idx].normal[2];
}

static void getTexCoord(const SMikkTSpaceContext* ctx, float outUV[],
                        int iFace, int iVert) {
    auto* mesh = static_cast<reina::scene::gltf::Primitive*>(ctx->m_pUserData);
    int idx = iFace * 3 + iVert;
    outUV[0] = mesh->vertices[idx].uv[0];
    outUV[1] = mesh->vertices[idx].uv[1];
}

static void setTSpace(const SMikkTSpaceContext* ctx,
                      const float tangent[], const float bitangent[],
                      const float /*magS*/, const float /*magT*/,
                      const int isOrientationPreserving,
                      int iFace, int iVert) {
    auto* mesh = static_cast<reina::scene::gltf::Primitive*>(ctx->m_pUserData);
    int idx = iFace * 3 + iVert;

    mesh->vertices[idx].tangent[0] = tangent[0];
    mesh->vertices[idx].tangent[1] = tangent[1];
    mesh->vertices[idx].tangent[2] = tangent[2];

    mesh->vertices[idx].bitangent[0] = bitangent[0];
    mesh->vertices[idx].bitangent[1] = bitangent[1];
    mesh->vertices[idx].bitangent[2] = bitangent[2];
}


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
            fastgltf::Extensions::KHR_materials_variants |
            fastgltf::Extensions::KHR_materials_transmission |
            fastgltf::Extensions::KHR_materials_clearcoat |
            fastgltf::Extensions::KHR_materials_emissive_strength;

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

std::unordered_map<uint32_t, std::vector<reina::scene::gltf::Primitive>> reina::scene::gltf::loadPrimitives(fastgltf::Asset& asset) {
    // — Gather meshes used by default scene
    std::unordered_set<size_t> used;

    if (asset.scenes.empty()) {
        throw std::runtime_error("No scenes supplied in gLTF file");
    }

    size_t sceneIdx = asset.defaultScene.value_or(0);
    fastgltf::iterateSceneNodes(asset, sceneIdx, fastgltf::math::fmat4x4(1.0f),
                                [&](fastgltf::Node& node, const fastgltf::math::fmat4x4&) {
                                    if (node.meshIndex.has_value()) used.insert(*node.meshIndex);
                                });

    std::unordered_map<uint32_t, std::vector<Primitive>> meshIdToPrimitives;

    // — For each used mesh → each primitive
    for (auto mi : used) {
        const auto& mesh = asset.meshes[mi];
        for (const auto& prim : mesh.primitives) {
            Primitive m;

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
                if (a == prim.attributes.end()) {
                    throw std::runtime_error("Meshes without vertex normals are not supported");
                }

                const auto& acc = asset.accessors[a->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    asset, acc,
                    [&](fastgltf::math::fvec3 n, size_t i) {
                        m.vertices[i].normal = n;
                    });
            }

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
            } else {
                // Tangents not provided; compute them yourself
                SMikkTSpaceInterface interface = {
                        .m_getNumFaces         = getNumFaces,
                        .m_getNumVerticesOfFace= getNumVertsOfFace,
                        .m_getPosition         = getPosition,
                        .m_getNormal           = getNormal,
                        .m_getTexCoord         = getTexCoord,
                        .m_setTSpace           = setTSpace
                };

                SMikkTSpaceContext ctx = {
                        .m_pInterface = &interface,
                        .m_pUserData = &m
                };

                genTangSpaceDefault(&ctx);
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
                    std::cerr << "Warning: falling back to UV coords (0, 0) since none were found" << std::endl;
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

            meshIdToPrimitives[mi].emplace_back(std::move(m));
        }
    }

    return meshIdToPrimitives;
}

reina::scene::ModelData reina::scene::gltf::Primitive::toModelData() const {
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
        // Modified from: https://vkguide.dev/docs/new_chapter_5/gltf_textures/
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

std::unordered_map<uint32_t, std::vector<uint32_t>> reina::scene::gltf::addMeshesToScene(reina::scene::Scene& scene, const std::unordered_map<uint32_t, std::vector<reina::scene::gltf::Primitive>>& meshIdToPrimitive) {
    std::unordered_map<uint32_t, std::vector<uint32_t>> meshIdToSceneObjectId;

    for (const auto& [meshID, primitives] : meshIdToPrimitive) {
        for (const Primitive& primitive : primitives) {
            uint32_t sceneID = scene.defineObject(primitive.toModelData());
            meshIdToSceneObjectId[meshID].push_back(sceneID);
        }
    }

    return meshIdToSceneObjectId;
}

void reina::scene::gltf::addInstancesToScene(fastgltf::Asset &asset, reina::scene::Scene& scene,
                                             const std::unordered_map<uint32_t, std::vector<uint32_t>>& gltfIdToSceneId,
                                             const std::unordered_map<uint32_t, std::vector<reina::scene::Material>>& gltfModelIdToMaterials) {
    size_t sceneIdx = asset.defaultScene.value_or(0);

    fastgltf::iterateSceneNodes(
            asset,
            sceneIdx,
            fastgltf::math::fmat4x4(1.0f),
            [&](fastgltf::Node& node, const auto& matrix) {
                if (!node.meshIndex.has_value()) {
                    return;
                };

                std::vector<uint32_t> objectIDsScene = gltfIdToSceneId.at(*node.meshIndex);
                std::vector<Material> materials = gltfModelIdToMaterials.at(*node.meshIndex);

                glm::mat4 glmMat = glm::make_mat4(matrix.data());

                for (size_t primitiveIdx = 0; primitiveIdx < objectIDsScene.size(); primitiveIdx++) {
                    scene.addInstance(objectIDsScene[primitiveIdx], glmMat, materials[primitiveIdx]);
                }
            });
}

inline glm::vec3 toGlm(const fastgltf::math::nvec3& v) {
    return glm::vec3{ v.x(), v.y(), v.z() };
}

std::unordered_map<uint32_t, std::vector<reina::scene::Material>> reina::scene::gltf::materialsFromMeshTBNs(fastgltf::Asset &asset, const std::unordered_map<uint32_t, std::vector<reina::scene::gltf::Primitive>>& meshIdToPrimitives, std::unordered_map<uint32_t, uint32_t> gltfTexIdToSceneId) {
    std::unordered_map<uint32_t, std::vector<Material>> meshIdToMaterials;

    for (const auto& [meshID, primitives] : meshIdToPrimitives) {
        for (const Primitive& primitive : primitives) {
            Material material{3, -1, -1, -1, glm::vec3(1.0f), glm::vec3(0.0f), 0.0f, 1.5f, true, 0.0f, false, 0.0f, 0.0f, 0.0f, glm::vec3(1.0f), glm::vec3(1.0f), 0.0f, 0.0f, 0.0f, 0.0f};

            if (primitive.materialIdx != -1) {
                const auto& gltfMaterial = asset.materials[primitive.materialIdx];

                if (glm::any(glm::greaterThan(toGlm(gltfMaterial.emissiveFactor), glm::vec3(0))) && !gltfMaterial.emissiveTexture.has_value()) {
                    // emission maps are not supported yet so do not include any emissive materials that require an emission map
                    std::cout << "emissive factor: " << gltfMaterial.emissiveFactor.x() << " " << gltfMaterial.emissiveFactor.y() << " " << gltfMaterial.emissiveFactor.z() << " | strength: " << gltfMaterial.emissiveStrength << "\n";
                    material.emission = toGlm(gltfMaterial.emissiveFactor) * gltfMaterial.emissiveStrength;
                }

                material.metallic = gltfMaterial.pbrData.metallicFactor;
                material.roughness = fmax(fmin(gltfMaterial.pbrData.roughnessFactor, 0.7f), 0.1f);
                material.albedo = glm::vec3(glm::make_vec4(gltfMaterial.pbrData.baseColorFactor.data()));  // TODO: support alpha/transparency for albedo
                material.cullBackface = !gltfMaterial.doubleSided;
                material.ior = gltfMaterial.ior;

                if (gltfMaterial.transmission) {
                    material.specularTransmission = gltfMaterial.transmission->transmissionFactor;
                    material.cullBackface = false;   // thin transmissive materials are not supported
                }

                // TODO: support clearcoat, transmission, sheen

                if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
                    try {
                        material.textureID = static_cast<int>(gltfTexIdToSceneId.at(static_cast<uint32_t>(asset.textures[gltfMaterial.pbrData.baseColorTexture.value().textureIndex].imageIndex.value())));
                    } catch (const std::out_of_range& e) {
                        std::cerr << "Warning: Texture ID not found. Exception: " << e.what() << std::endl;
                        material.textureID = -1;  // Fallback
                    }
                } if (gltfMaterial.normalTexture.has_value()) {
                    try {
                        material.normalMapID = static_cast<int>(gltfTexIdToSceneId.at(static_cast<uint32_t>(asset.textures[gltfMaterial.normalTexture.value().textureIndex].imageIndex.value())));
                    }  catch (const std::out_of_range& e) {
                        std::cerr << "Warning: Normal Texture ID not found. Exception: " << e.what() << std::endl;
                        material.normalMapID = -1;  // Fallback
                    }
                }
            }

            meshIdToMaterials[meshID].push_back(material);
        }
    }

    return meshIdToMaterials;
}

reina::scene::Scene reina::scene::gltf::loadScene(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::string& filepath) {
    auto asset = loadGltf(filepath);

    auto meshIdToPrimitives = loadPrimitives(asset);

    Scene scene;
    auto gltfModelIdToSceneId = addMeshesToScene(scene, meshIdToPrimitives);
    auto gltfTexIdToSceneId = addTexturesToScene(asset, scene);
    auto gltfModelIdToMaterials = materialsFromMeshTBNs(asset, meshIdToPrimitives, gltfTexIdToSceneId);
    addInstancesToScene(asset, scene, gltfModelIdToSceneId, gltfModelIdToMaterials);
    reina::scene::Material lightMaterial{0, -1, -1, -1, glm::vec3(0.9f), glm::vec3(16.0f), 0.0f, 0.0f, false, 0.0f, true};

//    scene.addObject("models/cornell_light.obj", glm::mat4(1.0f), lightMaterial);
    scene.build(logicalDevice, physicalDevice, cmdPool, queue);

    return scene;
}
