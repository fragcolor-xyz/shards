#include "gltf.hpp"
#include <algorithm>
#include <gfx/error_utils.hpp>
#include <gfx/filesystem.hpp>
#include <gfx/material.hpp>
#include <gfx/mesh.hpp>
#include <gfx/texture.hpp>
#include <gfx/texture_file/texture_file.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>
using namespace tinygltf;
#pragma GCC diagnostic pop

namespace gfx {

static WGPUAddressMode convertAddressMode(int mode) {
  switch (mode) {
  case TINYGLTF_TEXTURE_WRAP_REPEAT:
    return WGPUAddressMode_Repeat;
  case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
    return WGPUAddressMode_ClampToEdge;
  case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
    return WGPUAddressMode_MirrorRepeat;
  }
  assert(false);
  return WGPUAddressMode(~0);
}

static WGPUFilterMode convertFilterMode(int mode) {
  switch (mode) {
  case TINYGLTF_TEXTURE_FILTER_NEAREST:
    return WGPUFilterMode_Nearest;
  case TINYGLTF_TEXTURE_FILTER_LINEAR:
    return WGPUFilterMode_Linear;
    // NOTE: Mipmap filtering mode is ignored
  case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
    return WGPUFilterMode_Nearest;
  case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
    return WGPUFilterMode_Linear;
  case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
    return WGPUFilterMode_Nearest;
  case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
    return WGPUFilterMode_Linear;
  }
  assert(false);
  return WGPUFilterMode(~0);
}

static SamplerState convertSampler(const tinygltf::Sampler &sampler) {
  SamplerState samplerState{};
  samplerState.addressModeU = convertAddressMode(sampler.wrapS);
  samplerState.addressModeV = convertAddressMode(sampler.wrapT);
  samplerState.filterMode = convertFilterMode(sampler.minFilter);
  return samplerState;
}

static TextureFormat convertTextureFormat(const tinygltf::Image &image) {
  TextureFormat result{
      .type = TextureType::D2,
  };

  auto throwUnsupportedPixelFormat = [&]() {
    throw formatException("Unsupported pixel format for {} components: {} ", image.component, image.pixel_type);
  };
  if (image.component == 4) {
    if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
      result.pixelFormat = WGPUTextureFormat_RGBA8UnormSrgb;
    else if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_FLOAT)
      result.pixelFormat = WGPUTextureFormat_RGBA32Float;
    else
      throwUnsupportedPixelFormat();
  } else if (image.component == 2) {
    if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
      result.pixelFormat = WGPUTextureFormat_RG8Unorm;
    else if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_FLOAT)
      result.pixelFormat = WGPUTextureFormat_RG32Float;
    else
      throwUnsupportedPixelFormat();
  } else if (image.component == 1) {
    if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
      result.pixelFormat = WGPUTextureFormat_R8Unorm;
    else if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_FLOAT)
      result.pixelFormat = WGPUTextureFormat_R32Float;
    else
      throwUnsupportedPixelFormat();
  } else {
    throw formatException("{} component textures not supported", image.component);
  }
  return result;
}

static float4 convertFloat4Vec(const std::vector<double> &v) {
  assert(v.size() == 4);
  return float4(v[0], v[1], v[2], v[3]);
}

static float4x4 convertNodeTransform(const tinygltf::Node &node) {
  if (node.matrix.size() != 0) {
    return float4x4(double4x4(node.matrix.data()));
  } else {
    const auto t = linalg::translation_matrix(node.translation.size() != 0 ? double3(node.translation.data()) : double3());
    const auto r = linalg::rotation_matrix(node.rotation.size() != 0 ? double4(node.rotation.data()) : double4(0, 0, 0, 1));
    const auto s = linalg::scaling_matrix(node.scale.size() != 0 ? double3(node.scale.data()) : double3(1, 1, 1));
    return float4x4(linalg::mul(linalg::mul(t, r), s));
  }
}

struct Loader {
  struct Mesh {
    std::vector<DrawablePtr> primitives;
  };

  tinygltf::Model &model;
  std::vector<MeshPtr> primitiveMap;
  std::vector<Mesh> meshMap;
  std::vector<MaterialPtr> materialMap;
  std::vector<TexturePtr> textureMap;
  std::vector<DrawableHierarchyPtr> nodeMap;
  std::vector<DrawableHierarchyPtr> sceneMap;

  Loader(tinygltf::Model &model) : model(model) {}

  std::string translateAttributeName(const std::string &in) {
    std::string nameLower;
    nameLower.resize(in.size());
    std::transform(in.begin(), in.end(), nameLower.begin(), [](unsigned char c) { return std::tolower(c); });

    const char texcoordPrefix[] = "texcoord_";
    const char colorPrefix[] = "color_";
    if (nameLower.starts_with(texcoordPrefix)) {
      std::string numberSuffix = nameLower.substr(sizeof(texcoordPrefix) - 1);
      return std::string("texCoord") + (numberSuffix.empty() ? "0" : numberSuffix.c_str());
    } else if (nameLower.starts_with(colorPrefix)) {
      return std::string("color") + nameLower.substr(sizeof(colorPrefix) - 1);
    } else {
      return nameLower;
    }
  }

  void reorderVertexAttributes(std::vector<const tinygltf::Accessor *> &accessors, std::vector<size_t> &offsets,
                               MeshFormat &format) {
    int positionIndex = -1;
    for (size_t i = 0; i < format.vertexAttributes.size(); i++) {
      if (format.vertexAttributes[i].name == "position") {
        positionIndex = i;
        break;
      }
    }

    // Make position the first attribute to aid renderdoc debugging
    if (positionIndex >= 0 && positionIndex != 0) {
      std::swap(accessors[0], accessors[positionIndex]);
      std::swap(format.vertexAttributes[0], format.vertexAttributes[positionIndex]);

      // Recompute offsets
      size_t offset = 0;
      for (size_t i = 0; i < format.vertexAttributes.size(); i++) {
        auto &attrib = format.vertexAttributes[i];
        offsets[i] = offset;
        offset += getVertexAttributeTypeSize(attrib.type) * attrib.numComponents;
      }
    }
  }

  DrawablePtr loadPrimitive(tinygltf::Primitive &primitive) {
    std::vector<const tinygltf::Accessor *> accessors;
    std::vector<size_t> offsets;
    size_t vertexStride{};
    size_t numVertices{};
    MeshFormat format;

    for (auto &gltfAttrib : primitive.attributes) {
      auto &gltfAccessor = model.accessors[gltfAttrib.second];

      MeshVertexAttribute &attrib = format.vertexAttributes.emplace_back();
      attrib.name = translateAttributeName(gltfAttrib.first);
      attrib.numComponents = tinygltf::GetNumComponentsInType(gltfAccessor.type);
      switch (gltfAccessor.componentType) {
      case TINYGLTF_COMPONENT_TYPE_BYTE:
        attrib.type = VertexAttributeType::Int8;
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        attrib.type = VertexAttributeType::UInt8;
        break;
      case TINYGLTF_COMPONENT_TYPE_SHORT:
        attrib.type = VertexAttributeType::Int16;
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        attrib.type = VertexAttributeType::UInt16;
        break;
      case TINYGLTF_COMPONENT_TYPE_INT:
        attrib.type = VertexAttributeType::Int32;
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        attrib.type = VertexAttributeType::UInt32;
        break;
      case TINYGLTF_COMPONENT_TYPE_FLOAT:
        attrib.type = VertexAttributeType::Float32;
        break;
      case TINYGLTF_COMPONENT_TYPE_DOUBLE:
        throw formatException("Accessor {}: double component type is not supported", gltfAttrib.second);
        break;
      }
      accessors.push_back(&gltfAccessor);
      offsets.push_back(vertexStride);
      vertexStride += getVertexAttributeTypeSize(attrib.type) * attrib.numComponents;

      if (numVertices == 0) {
        numVertices = gltfAccessor.count;
      } else {
        assert(gltfAccessor.count == numVertices);
      }
    }

    reorderVertexAttributes(accessors, offsets, format);

    std::vector<uint8_t> vertexBuffer;
    vertexBuffer.resize(vertexStride * numVertices);
    for (size_t attribIndex{}; attribIndex < accessors.size(); attribIndex++) {
      size_t dstOffset = offsets[attribIndex];
      const tinygltf::Accessor &accessor = *accessors[attribIndex];
      const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
      const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
      const MeshVertexAttribute &attrib = format.vertexAttributes[attribIndex];

      size_t elementSize = getVertexAttributeTypeSize(attrib.type) * attrib.numComponents;
      int srcStride = accessor.ByteStride(bufferView);

      const uint8_t *srcPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
      uint8_t *dstPtr = vertexBuffer.data() + dstOffset;
      for (size_t i = 0; i < numVertices; i++) {
        memcpy(dstPtr, srcPtr, elementSize);
        srcPtr += srcStride;
        dstPtr += vertexStride;
      }
    }

    std::vector<uint8_t> indexBuffer;
    if (primitive.indices >= 0) {
      const tinygltf::Accessor &accessor = model.accessors[primitive.indices];
      const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
      const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
      switch (accessor.componentType) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        format.indexFormat = IndexFormat::UInt16;
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        format.indexFormat = IndexFormat::UInt32;
        break;
      default:
        throw formatException("Accessor {}: Unsupported index format", primitive.indices);
      }

      size_t elementSize = getIndexFormatSize(format.indexFormat);
      int srcStride = accessor.ByteStride(bufferView);

      indexBuffer.resize(elementSize * accessor.count);
      uint8_t *dstPtr = indexBuffer.data();
      const uint8_t *srcPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
      for (size_t i = 0; i < accessor.count; i++) {
        memcpy(dstPtr, srcPtr, elementSize);
        dstPtr += elementSize;
        srcPtr += srcStride;
      }
    }

    switch (primitive.mode) {
    case TINYGLTF_MODE_TRIANGLES:
      format.primitiveType = PrimitiveType::TriangleList;
      break;
    case TINYGLTF_MODE_TRIANGLE_STRIP:
      format.primitiveType = PrimitiveType::TriangleStrip;
      break;
    default:
      throw formatException("Unsupported primitive mode ({})", primitive.mode);
    }

    MeshPtr mesh = std::make_shared<gfx::Mesh>();
    mesh->update(format, std::move(vertexBuffer), std::move(indexBuffer));

    DrawablePtr result = std::make_shared<Drawable>(mesh);
    if (primitive.material >= 0) {
      result->material = materialMap[primitive.material];
    }

    return result;
  }

  void loadMeshes() {
    size_t numMeshes = model.meshes.size();
    meshMap.resize(numMeshes);

    for (size_t i = 0; i < numMeshes; i++) {
      auto &mesh = meshMap[i];

      auto &gltfMesh = model.meshes[i];
      for (auto &gltfPrim : gltfMesh.primitives) {
        DrawablePtr primitive = loadPrimitive(gltfPrim);
        mesh.primitives.push_back(primitive);
      }
    }
  }

  void initNode(size_t nodeIndex) {
    DrawableHierarchyPtr &node = nodeMap[nodeIndex];
    node = std::make_shared<DrawableHierarchy>();

    const Node &gltfNode = model.nodes[nodeIndex];
    node->label = gltfNode.name;

    if (gltfNode.mesh >= 0) {
      for (auto &prim : meshMap[gltfNode.mesh].primitives) {
        node->drawables.push_back(prim);
      }
    }

    node->transform = convertNodeTransform(gltfNode);
    assert(node->transform != float4x4());

    for (size_t i = 0; i < gltfNode.children.size(); i++) {
      int childNodeIndex = gltfNode.children[i];
      DrawableHierarchyPtr &childNode = nodeMap[childNodeIndex];
      if (!childNode) {
        childNode = std::make_shared<DrawableHierarchy>();
        initNode(childNodeIndex);
      }
      node->children.push_back(childNode);
    }
  }

  void loadNodes() {
    size_t numNodes = model.nodes.size();
    nodeMap.resize(numNodes);
    for (size_t i = 0; i < numNodes; i++) {
      if (!nodeMap[i])
        initNode(i);
    }
  }

  void loadTextures() {
    size_t numTextures = model.textures.size();
    textureMap.resize(numTextures);
    for (size_t i = 0; i < numTextures; i++) {
      const tinygltf::Texture &gltfTexture = model.textures[i];
      const tinygltf::Image &gltfImage = model.images[gltfTexture.source];

      TexturePtr &texture = textureMap[i];
      texture = std::make_shared<Texture>();

      TextureFormat format = convertTextureFormat(gltfImage);
      SamplerState samplerState{};
      if (gltfTexture.sampler >= 0) {
        convertSampler(model.samplers[gltfTexture.sampler]);
      }

      int2 resolution(gltfImage.width, gltfImage.height);
      texture->init(format, resolution, samplerState, ImmutableSharedBuffer(gltfImage.image.data(), gltfImage.image.size()));
    }
  }

  void loadMaterials() {
    size_t numMaterials = model.materials.size();
    materialMap.resize(numMaterials);
    for (size_t i = 0; i < model.materials.size(); i++) {
      MaterialPtr &material = materialMap[i];
      material = std::make_shared<Material>();

      const tinygltf::Material &gltfMaterial = model.materials[i];

      auto convertTextureParam = [&](const char *name, const TextureInfo &textureInfo) {
        if (textureInfo.index >= 0) {
          TexturePtr texture = textureMap[textureInfo.index];
          material->parameters.set(name, TextureParameter(texture, textureInfo.texCoord));
        }
      };

      auto convertOptionalFloat4Param = [&](const char *name, const std::vector<double> &param, const float4 &defaultValue) {
        float4 f4val = convertFloat4Vec(param);
        if (f4val != defaultValue) {
          material->parameters.set(name, f4val);
        }
      };
      auto convertOptionalFloatParam = [&](const char *name, const float &param, const float &defaultValue) {
        if (param != defaultValue) {
          material->parameters.set(name, param);
        }
      };

      convertOptionalFloat4Param("baseColor", gltfMaterial.pbrMetallicRoughness.baseColorFactor, float4(1, 1, 1, 1));
      convertOptionalFloatParam("roughness", gltfMaterial.pbrMetallicRoughness.roughnessFactor, 1.0f);
      convertOptionalFloatParam("metallic", gltfMaterial.pbrMetallicRoughness.metallicFactor, 0.0f);
      convertTextureParam("baseColor", gltfMaterial.pbrMetallicRoughness.baseColorTexture);
      convertTextureParam("metallicRougness", gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture);
    }
  }

  void load() {
    loadTextures();
    loadMaterials();
    loadMeshes();
    loadNodes();

    size_t numScenes = model.scenes.size();
    sceneMap.resize(numScenes);
    for (size_t i = 0; i < numScenes; i++) {
      const Scene &gltfScene = model.scenes[i];

      DrawableHierarchyPtr &scene = sceneMap[i];
      scene = std::make_shared<DrawableHierarchy>();

      for (size_t i = 0; i < gltfScene.nodes.size(); i++) {
        int nodeIndex = gltfScene.nodes[i];
        DrawableHierarchyPtr node = nodeMap[nodeIndex];
        scene->children.push_back(node);
      }
    }
  }
};

template <typename T> DrawableHierarchyPtr load(T loader) {
  tinygltf::Model model;
  loader(model);

  if (model.defaultScene == -1) {
    throw formatException("glTF model has no default scene");
  }

  Loader gfxLoader(model);
  gfxLoader.load();

  return gfxLoader.sceneMap[model.defaultScene];
}

DrawableHierarchyPtr loadGltfFromFile(const char *inFilepath) {
  tinygltf::TinyGLTF context;
  auto loader = [&](tinygltf::Model &model) {
    fs::path filepath(inFilepath);
    const auto &ext = filepath.extension();

    if (!fs::exists(filepath)) {
      throw formatException("glTF model file \"{}\" does not exist", filepath.string());
    }

    std::string err;
    std::string warn;
    bool success{};
    if (ext == ".glb") {
      success = context.LoadBinaryFromFile(&model, &err, &warn, filepath.string());
    } else {
      success = context.LoadASCIIFromFile(&model, &err, &warn, filepath.string());
    }

    if (!success) {
      throw formatException("Failed to load glTF model \"{}\"\nErrors: {}\nWarnings: {}", filepath.string(), err, warn);
    }
  };

  return load(loader);
}

DrawableHierarchyPtr loadGltfFromMemory(const uint8_t *data, size_t dataLength) {
  tinygltf::TinyGLTF context;
  auto loader = [&](tinygltf::Model &model) {
    std::string err;
    std::string warn;
    bool success{};
    success = context.LoadBinaryFromMemory(&model, &err, &warn, data, dataLength);

    if (!success) {
      throw formatException("Failed to load glTF model \nErrors: {}\nWarnings: {}", err, warn);
    }
  };

  return load(loader);
}

} // namespace gfx