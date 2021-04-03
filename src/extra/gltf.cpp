/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "./bgfx.hpp"
#include "blocks/shared.hpp"
#include "linalg_shim.hpp"
#include "runtime.hpp"

#include <deque>
#include <filesystem>
#include <optional>
namespace fs = std::filesystem;
using LastWriteTime = decltype(fs::last_write_time(fs::path()));

#include <boost/algorithm/string.hpp>

#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_write.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_USE_CPP14
// #define TINYGLTF_ENABLE_DRACO
#include <tinygltf/tiny_gltf.h>
using GLTFModel = tinygltf::Model;
using GLTFImage = tinygltf::Image;
using namespace tinygltf;
#undef Model

/*
TODO: GLTF.Simulate - depending on physics simulation blocks
TODO; GLTF.Animate - to play animations
*/
namespace chainblocks {
namespace gltf {
struct GFXShader {
  GFXShader(bgfx::ProgramHandle handle) : handle(handle) {}

  bgfx::ProgramHandle handle = BGFX_INVALID_HANDLE;

  GFXShader(GFXShader &&other) { std::swap(handle, other.handle); }

  ~GFXShader() {
    if (handle.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(handle);
    }
  }
};

struct GFXTexture {
  uint16_t width;
  uint16_t height;
  bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;

  GFXTexture(uint16_t width, uint16_t height) : width(width), height(height) {}

  GFXTexture(GFXTexture &&other) {
    std::swap(width, other.width);
    std::swap(height, other.height);
    std::swap(handle, other.handle);
  }

  ~GFXTexture() {
    if (handle.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(handle);
    }
  }
};

struct GFXMaterial {
  std::string name;
  size_t hash;
  bool doubleSided{false};

  // textures and parameters
  std::shared_ptr<GFXTexture> baseColorTexture;
  std::array<float, 4> baseColor;

  std::shared_ptr<GFXTexture> metallicRoughnessTexture;
  float metallicFactor;
  float roughnessFactor;

  std::shared_ptr<GFXTexture> normalTexture;

  std::shared_ptr<GFXTexture> occlusionTexture;

  std::shared_ptr<GFXTexture> emissiveTexture;
  std::array<float, 3> emissiveColor;

  std::shared_ptr<GFXShader> shader;
};

using GFXMaterialRef = std::reference_wrapper<GFXMaterial>;

struct GFXPrimitive {
  bgfx::VertexBufferHandle vb = BGFX_INVALID_HANDLE;
  bgfx::IndexBufferHandle ib = BGFX_INVALID_HANDLE;
  bgfx::VertexLayout layout{};
  uint64_t stateFlags{0};
  std::optional<GFXMaterialRef> material;

  GFXPrimitive() {}

  GFXPrimitive(GFXPrimitive &&other) {
    std::swap(vb, other.vb);
    std::swap(ib, other.ib);
    std::swap(layout, other.layout);
    std::swap(material, other.material);
  }

  ~GFXPrimitive() {
    if (vb.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(vb);
    }
    if (ib.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(ib);
    }
  }
};

using GFXPrimitiveRef = std::reference_wrapper<GFXPrimitive>;

struct GFXMesh {
  std::string name;
  std::vector<GFXPrimitiveRef> primitives;
};

using GFXMeshRef = std::reference_wrapper<GFXMesh>;

struct Node;
using NodeRef = std::reference_wrapper<Node>;

struct Node {
  std::string name;
  Mat4 transform;

  std::optional<GFXMeshRef> mesh;

  std::vector<NodeRef> children;
};

struct Model {
  std::deque<Node> nodes;
  std::deque<GFXMesh> gfxMeshes;
  std::deque<GFXPrimitive> gfxPrimitives;
  std::deque<GFXMaterial> gfxMaterials;
  std::vector<NodeRef> rootNodes;
};

bool LoadImageData(GLTFImage *image, const int image_idx, std::string *err,
                   std::string *warn, int req_width, int req_height,
                   const unsigned char *bytes, int size, void *user_pointer) {
  return tinygltf::LoadImageData(image, image_idx, err, warn, req_width,
                                 req_height, bytes, size, user_pointer);
}

struct Load : public BGFX::BaseConsumer {
  static constexpr uint32_t ModelCC = 'gltf';
  static inline Type ModelType{
      {CBType::Object, {.object = {.vendorId = CoreCC, .typeId = ModelCC}}}};
  static inline Type ModelVarType = Type::VariableOf(ModelType);
  static inline ObjectVar<Model> ModelVar{"GLTF-Model", CoreCC, ModelCC};

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return ModelType; }

  static inline Parameters Params{
      {"Textures",
       CBCCSTR("If the textures linked to this model should be loaded."),
       {CoreInfo::BoolType}},
      {"sRGB",
       CBCCSTR("If diffuse color textures should be loaded as an sRGB format "
               "(only valid for 8 bit per color textures)."),
       {CoreInfo::BoolType}},
      {"Shaders",
       CBCCSTR("If the shaders linked to this model should be compiled and/or "
               "loaded."),
       {CoreInfo::BoolType}},
      {"TransformBefore",
       CBCCSTR("An optional global transformation matrix to apply before the "
               "model root nodes transformations."),
       {CoreInfo::Float4x4Type, CoreInfo::NoneType}},
      {"TransformAfter",
       CBCCSTR("An optional global transformation matrix to apply after the "
               "model root nodes transformations."),
       {CoreInfo::Float4x4Type, CoreInfo::NoneType}}};
  static CBParametersInfo parameters() { return Params; }

  Model *_model{nullptr};
  TinyGLTF _loader;
  bool _srgb{false};
  bool _withTextures{true};
  bool _withShaders{true};
  uint32_t _numLights{0};
  std::unique_ptr<IShaderCompiler> _shaderCompiler;
  std::optional<Mat4> _before;
  std::optional<Mat4> _after;

  void setup() { _loader.SetImageLoader(&LoadImageData, this); }

  CBTypeInfo compose(const CBInstanceData &data) {
    // Skip base compose, we don't care about that check
    // BGFX::BaseConsumer::compose(data);
    return ModelType;
  }
  template <typename T> struct Cache {
    std::shared_ptr<T> find(uint64_t hash) {
      std::shared_ptr<T> res;
      std::unique_lock lock(_mutex);
      auto it = _cache.find(hash);
      if (it != _cache.end()) {
        res = it->second.lock();
      }
      return res;
    }

    void add(uint64_t hash, const std::shared_ptr<T> &data) {
      std::unique_lock lock(_mutex);
      _cache[hash] = data;
    }

  private:
    std::mutex _mutex;
    // weak pointers in order to avoid holding in memory
    std::unordered_map<uint64_t, std::weak_ptr<T>> _cache;
  };

  struct TextureCache : public Cache<GFXTexture> {
    static uint64_t hashTexture(const Texture &gltexture,
                                const GLTFImage &glsource) {
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret,
                                   XXH_SECRET_DEFAULT_SIZE);

      XXH3_64bits_update(&hashState, glsource.image.data(),
                         glsource.image.size());

      return XXH3_64bits_digest(&hashState);
    }
  };

  struct ShadersCache : public Cache<GFXShader> {
    static uint64_t hashShader(const std::string &defines) {
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret,
                                   XXH_SECRET_DEFAULT_SIZE);

      XXH3_64bits_update(&hashState, defines.data(), defines.size());

      return XXH3_64bits_digest(&hashState);
    }
  };

  Shared<TextureCache> _texturesCache;
  Shared<ShadersCache> _shadersCache;

  std::mutex _shadersMutex;
  static inline std::string _shadersVarying;
  static inline std::string _shadersVSEntry;
  static inline std::string _shadersPSEntry;

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _withTextures = value.payload.boolValue;
      break;
    case 1:
      _srgb = value.payload.boolValue;
      break;
    case 2:
      _withShaders = value.payload.boolValue;
      break;
    case 3:
      if (value.valueType == CBType::None) {
        _before.reset();
      } else {
        // navigating c++ operators for Mat4
        Mat4 m;
        m = value;
        _before = m;
      }
      break;
    case 4:
      if (value.valueType == CBType::None) {
        _after.reset();
      } else {
        // navigating c++ operators for Mat4
        Mat4 m;
        m = value;
        _after = m;
      }
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_withTextures);
    case 1:
      return Var(_srgb);
    case 2:
      return Var(_withShaders);
    case 3:
      if (_before)
        return *_before;
      else
        return Var::Empty;
    case 4:
      if (_after)
        return *_after;
      else
        return Var::Empty;
    default:
      throw InvalidParameterIndex();
    }
  }

  void cleanup() {
    if (_model) {
      ModelVar.Release(_model);
      _model = nullptr;
    }

    if (_shaderCompiler) {
      _shaderCompiler = nullptr;
    }
  }

  std::shared_ptr<GFXTexture>
  getOrLoadTexture(const GLTFModel &gltf, const Texture &gltexture, bool srgb) {
    if (_withTextures && gltexture.source != -1) {
      const auto &image = gltf.images[gltexture.source];
      const auto imgHash = TextureCache::hashTexture(gltexture, image);
      auto texture = _texturesCache().find(imgHash);
      if (!texture) {
        texture = std::make_shared<GFXTexture>(uint16_t(image.width),
                                               uint16_t(image.height));

        BGFX_TEXTURE2D_CREATE(image.bits, image.component, texture, srgb);

        auto mem = bgfx::copy(image.image.data(), image.image.size());
        bgfx::updateTexture2D(texture->handle, 0, 0, 0, 0, texture->width,
                              texture->height, mem);

        _texturesCache().add(imgHash, texture);
      }
      // we are sure we added the texture but still return it from the table
      // as there might be some remote chance of racing and both threads loading
      // the same in such case we rather have one destroyed
      return _texturesCache().find(imgHash);
    } else {
      return nullptr;
    }
  }

  GFXMaterial processMaterial(CBContext *context, const GLTFModel &gltf,
                              const Material &glmaterial,
                              std::unordered_set<std::string> &shaderDefines,
                              const std::string &varyings) {
    GFXMaterial material{glmaterial.name,
                         std::hash<std::string_view>()(glmaterial.name),
                         glmaterial.doubleSided};

    // do an extension check
    for (const auto &[name, val] : glmaterial.extensions) {
      if (name == "KHR_materials_unlit") {
        shaderDefines.insert("CB_UNLIT");
      }
    }

    if (_numLights > 0) {
      shaderDefines.insert("CB_NUM_LIGHTS" + std::to_string(_numLights));
    }

    if (glmaterial.pbrMetallicRoughness.baseColorTexture.index != -1) {
      material.baseColorTexture = getOrLoadTexture(
          gltf,
          gltf.textures[glmaterial.pbrMetallicRoughness.baseColorTexture.index],
          _srgb);
      shaderDefines.insert("CB_PBR_COLOR_TEXTURE");
    }
    const auto colorFactorSize =
        glmaterial.pbrMetallicRoughness.baseColorFactor.size();
    if (colorFactorSize > 0) {
      for (size_t i = 0; i < colorFactorSize && i < 4; i++) {
        material.baseColor[i] =
            glmaterial.pbrMetallicRoughness.baseColorFactor[i];
      }
      shaderDefines.insert("CB_PBR_COLOR_FACTOR");
    }

    material.metallicFactor = glmaterial.pbrMetallicRoughness.metallicFactor;
    material.roughnessFactor = glmaterial.pbrMetallicRoughness.roughnessFactor;
    if (glmaterial.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
      material.metallicRoughnessTexture =
          getOrLoadTexture(gltf,
                           gltf.textures[glmaterial.pbrMetallicRoughness
                                             .metallicRoughnessTexture.index],
                           false);
      shaderDefines.insert("CB_PBR_METALLIC_ROUGHNESS_TEXTURE");
    }

    if (glmaterial.normalTexture.index != -1) {
      material.normalTexture = getOrLoadTexture(
          gltf, gltf.textures[glmaterial.normalTexture.index], false);
      shaderDefines.insert("CB_NORMAL_TEXTURE");
    }

    if (_withShaders) {
      std::string shaderDefinesStr;
      for (const auto &define : shaderDefines) {
        shaderDefinesStr += define + ";";
      }
      auto hash = ShadersCache::hashShader(shaderDefinesStr);
      auto shader = _shadersCache().find(hash);
      if (!shader) {
        // compile or fetch cached shaders
        // vertex
        bgfx::ShaderHandle vsh;
        {
          auto bytecode = _shaderCompiler->compile(
              varyings, _shadersVSEntry, "v", shaderDefinesStr, context);
          auto mem = bgfx::copy(bytecode.payload.bytesValue,
                                bytecode.payload.bytesSize);
          vsh = bgfx::createShader(mem);
        }
        // pixel
        bgfx::ShaderHandle psh;
        {
          auto bytecode = _shaderCompiler->compile(
              varyings, _shadersPSEntry, "f", shaderDefinesStr, context);
          auto mem = bgfx::copy(bytecode.payload.bytesValue,
                                bytecode.payload.bytesSize);
          psh = bgfx::createShader(mem);
        }
        shader =
            std::make_shared<GFXShader>(bgfx::createProgram(vsh, psh, true));
        _shadersCache().add(hash, shader);
      }
      material.shader = shader;
    }

    return material;
  }

  void warmup(CBContext *ctx) {
    _shaderCompiler = makeShaderCompiler();

    // lazily fill out varying and entry point shaders
    if (_shadersVarying.size() == 0) {
      std::unique_lock lock(_shadersMutex);
      // check again after unlock
      if (_shadersVarying.size() == 0) {
        auto failed = false;
        {
          std::ifstream stream("shaders/lib/gltf/varying.txt");
          if (!stream.is_open())
            failed = true;
          std::stringstream buffer;
          buffer << stream.rdbuf();
          _shadersVarying.assign(buffer.str());
        }
        {
          std::ifstream stream("shaders/lib/gltf/vs_entry.h");
          if (!stream.is_open())
            failed = true;
          std::stringstream buffer;
          buffer << stream.rdbuf();
          _shadersVSEntry.assign(buffer.str());
        }
        {
          std::ifstream stream("shaders/lib/gltf/ps_entry.h");
          if (!stream.is_open())
            failed = true;
          std::stringstream buffer;
          buffer << stream.rdbuf();
          _shadersPSEntry.assign(buffer.str());
        }
        if (failed) {
          CBLOG_FATAL("shaders library is missing");
        }
      }
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    return awaitne(context, [&]() {
      GLTFModel gltf;
      std::string err;
      std::string warn;
      bool success = false;
      const auto filename = input.payload.stringValue;
      fs::path filepath(filename);
      const auto &ext = filepath.extension();

      if (!fs::exists(filepath)) {
        throw ActivationError("GLTF model file does not exist.");
      }

      if (_model) {
        ModelVar.Release(_model);
        _model = nullptr;
      }
      _model = ModelVar.New();

      if (ext == ".glb") {
        success = _loader.LoadBinaryFromFile(&gltf, &err, &warn, filename);
      } else {
        success = _loader.LoadASCIIFromFile(&gltf, &err, &warn, filename);
      }

      if (!warn.empty()) {
        CBLOG_WARNING("GLTF warning: {}", warn);
      }
      if (!err.empty()) {
        CBLOG_ERROR("GLTF error: {}", err);
      }
      if (!success) {
        throw ActivationError("Failed to parse GLTF.");
      }

      if (gltf.defaultScene == -1) {
        throw ActivationError("GLTF model had no default scene.");
      }

      const auto &scene = gltf.scenes[gltf.defaultScene];
      for (const int gltfNodeIdx : scene.nodes) {
        const auto &glnode = gltf.nodes[gltfNodeIdx];
        const std::function<NodeRef(const tinygltf::Node)> processNode =
            [this, &gltf, &processNode, context](const tinygltf::Node &glnode) {
              Node node{glnode.name};

              if (glnode.matrix.size() != 0) {
                node.transform = Mat4::FromVector(glnode.matrix);
              } else {
                const auto t = linalg::translation_matrix(
                    glnode.translation.size() != 0
                        ? Vec3::FromVector(glnode.translation)
                        : Vec3());
                const auto r = linalg::rotation_matrix(
                    glnode.rotation.size() != 0
                        ? Vec4::FromVector(glnode.rotation)
                        : Vec4::Quaternion());
                const auto s = linalg::scaling_matrix(
                    glnode.scale.size() != 0 ? Vec3::FromVector(glnode.scale)
                                             : Vec3(1.0, 1.0, 1.0));
                node.transform = linalg::mul(linalg::mul(t, r), s);
              }

              if (_before) {
                node.transform = linalg::mul(*_before, node.transform);
              }

              if (_after) {
                node.transform = linalg::mul(node.transform, *_after);
              }

              if (glnode.mesh != -1) {
                const auto &glmesh = gltf.meshes[glnode.mesh];
                GFXMesh mesh{glmesh.name};
                for (const auto &glprims : glmesh.primitives) {
                  GFXPrimitive prims{};
                  std::unordered_set<std::string> shaderDefines;
                  std::string varyings = _shadersVarying;
                  // we gotta do few things here
                  // build a layout
                  // populate vb and ib
                  std::vector<std::pair<bgfx::Attrib::Enum,
                                        std::reference_wrapper<Accessor>>>
                      accessors;
                  uint32_t vertexSize = 0;
                  for (const auto &[attributeName, attributeIdx] :
                       glprims.attributes) {
                    if (attributeName == "POSITION") {
                      accessors.emplace_back(bgfx::Attrib::Position,
                                             gltf.accessors[attributeIdx]);
                      vertexSize += sizeof(float) * 3;
                    } else if (attributeName == "NORMAL") {
                      accessors.emplace_back(bgfx::Attrib::Normal,
                                             gltf.accessors[attributeIdx]);
                      vertexSize += sizeof(float) * 3;
                      shaderDefines.insert("CB_HAS_NORMAL");
                      varyings.append("vec3 a_normal : NORMAL;\n");
                    } else if (attributeName == "TANGENT") {
                      accessors.emplace_back(bgfx::Attrib::Tangent,
                                             gltf.accessors[attributeIdx]);
                      shaderDefines.insert("CB_HAS_TANGENT");
                      varyings.append("vec4 a_tangent : TANGENT;\n");
                      vertexSize += sizeof(float) * 4;
                    } else if (boost::starts_with(attributeName, "TEXCOORD_")) {
                      int strIndex = std::stoi(attributeName.substr(9));
                      if (strIndex >= 8) {
                        throw ActivationError("GLTF TEXCOORD_ limit exceeded.");
                      }
                      auto texcoord = decltype(bgfx::Attrib::TexCoord0)(
                          int(bgfx::Attrib::TexCoord0) + strIndex);
                      accessors.emplace_back(texcoord,
                                             gltf.accessors[attributeIdx]);
                      vertexSize += sizeof(float) * 2;
                      shaderDefines.insert("CB_HAS_" + attributeName);
                      auto idxStr = std::to_string(strIndex);
                      varyings.append("vec2 a_texcoord" + idxStr +
                                      " : TEXCOORD" + idxStr + ";\n");
                    } else if (boost::starts_with(attributeName, "COLOR_")) {
                      int strIndex = std::stoi(attributeName.substr(6));
                      if (strIndex >= 4) {
                        throw ActivationError("GLTF COLOR_ limit exceeded.");
                      }
                      auto color = decltype(bgfx::Attrib::Color0)(
                          int(bgfx::Attrib::Color0) + strIndex);
                      accessors.emplace_back(color,
                                             gltf.accessors[attributeIdx]);
                      vertexSize += 4;
                      shaderDefines.insert("CB_HAS_" + attributeName);
                      auto idxStr = std::to_string(strIndex);
                      varyings.append("vec4 a_color" + idxStr + " : COLOR" +
                                      idxStr + ";\n");
                    } else {
                      // TODO JOINTS_ and WEIGHTS_ etc
                      CBLOG_WARNING("Ignored a primitive attribute: {}",
                                    attributeName);
                    }
                  }

                  // lay our data following enum order, pos, normals etc
                  std::sort(accessors.begin(), accessors.end(),
                            [](const auto &a, const auto &b) {
                              return a.first < b.first;
                            });

                  if (accessors.size() > 0) {
                    const auto &[_, ar] = accessors[0];
                    const auto vertexCount = ar.get().count;
                    const auto totalSize = uint32_t(vertexCount) * vertexSize;
                    auto vbuffer = bgfx::alloc(totalSize);
                    auto offsetSize = 0;
                    prims.layout.begin();
                    for (const auto &[attrib, accessorRef] : accessors) {
                      const auto &accessor = accessorRef.get();
                      const auto &view = gltf.bufferViews[accessor.bufferView];
                      const auto &buffer = gltf.buffers[view.buffer];
                      const auto dataBeg = buffer.data.begin() +
                                           view.byteOffset +
                                           accessor.byteOffset;
                      const auto stride = accessor.ByteStride(view);
                      switch (attrib) {
                      case bgfx::Attrib::Position:
                      case bgfx::Attrib::Normal: {
                        const auto size = sizeof(float) * 3;
                        auto vbufferOffset = offsetSize;
                        offsetSize += size;

                        if (accessor.componentType !=
                                TINYGLTF_COMPONENT_TYPE_FLOAT ||
                            accessor.type != TINYGLTF_TYPE_VEC3) {
                          throw ActivationError(
                              "Position/Normal vector data was not "
                              "a float32 vector of 3");
                        }

                        size_t idx = 0;
                        auto it = dataBeg;
                        while (idx < vertexCount) {
                          const uint8_t *chunk = &(*it);
                          memcpy(vbuffer->data + vbufferOffset, chunk, size);

                          vbufferOffset += vertexSize;
                          it += stride;
                          idx++;
                        }

                        // also update layout
                        prims.layout.add(attrib, 3, bgfx::AttribType::Float);
                      } break;
                        break;
                      case bgfx::Attrib::Tangent: {
                        // w is handedness
                        const auto size = sizeof(float) * 4;
                        auto vbufferOffset = offsetSize;
                        offsetSize += size;

                        if (accessor.componentType !=
                                TINYGLTF_COMPONENT_TYPE_FLOAT ||
                            accessor.type != TINYGLTF_TYPE_VEC4) {
                          throw ActivationError("Tangent vector data was not a "
                                                "float32 vector of 4");
                        }

                        size_t idx = 0;
                        auto it = dataBeg;
                        while (idx < vertexCount) {
                          const float *chunk = (float *)&(*it);
                          memcpy(vbuffer->data + vbufferOffset, chunk, size);

                          vbufferOffset += vertexSize;
                          it += stride;
                          idx++;
                        }

                        // also update layout
                        prims.layout.add(attrib, 4, bgfx::AttribType::Float);
                      } break;
                      case bgfx::Attrib::Color0:
                      case bgfx::Attrib::Color1:
                      case bgfx::Attrib::Color2:
                      case bgfx::Attrib::Color3: {
                        const auto elemSize = [&]() {
                          if (accessor.componentType ==
                              TINYGLTF_COMPONENT_TYPE_FLOAT)
                            return 4;
                          else if (accessor.componentType ==
                                   TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                            return 2;
                          else // BYTE
                            return 1;
                        }();
                        const auto elemNum = [&]() {
                          if (accessor.type == TINYGLTF_TYPE_VEC3)
                            return 3;
                          else // VEC4
                            return 4;
                        }();
                        const auto osize = 4;
                        auto vbufferOffset = offsetSize;
                        offsetSize += osize;

                        size_t idx = 0;
                        auto it = dataBeg;
                        while (idx < vertexCount) {
                          switch (elemNum) {
                          case 3: {
                            switch (elemSize) {
                            case 4: { // float
                              const float *chunk = (float *)&(*it);
                              uint8_t *buf = vbuffer->data + vbufferOffset;
                              buf[0] = uint8_t(chunk[0] * 255);
                              buf[1] = uint8_t(chunk[1] * 255);
                              buf[2] = uint8_t(chunk[2] * 255);
                              buf[3] = 255;
                            } break;
                            case 2: {
                              const uint16_t *chunk = (uint16_t *)&(*it);
                              uint8_t *buf = vbuffer->data + vbufferOffset;
                              buf[0] = uint8_t(
                                  (float(chunk[0]) / float(UINT16_MAX)) * 255);
                              buf[1] = uint8_t(
                                  (float(chunk[1]) / float(UINT16_MAX)) * 255);
                              buf[2] = uint8_t(
                                  (float(chunk[2]) / float(UINT16_MAX)) * 255);
                              buf[3] = 255;
                            } break;
                            case 1: {
                              const uint8_t *chunk = (uint8_t *)&(*it);
                              uint8_t *buf = vbuffer->data + vbufferOffset;
                              memcpy(buf, chunk, 3);
                              buf[3] = 255;
                            } break;
                            default:
                              CBLOG_FATAL("invalid state");
                              break;
                            }
                          } break;
                          case 4: {
                            switch (elemSize) {
                            case 4: { // float
                              const float *chunk = (float *)&(*it);
                              uint8_t *buf = vbuffer->data + vbufferOffset;
                              buf[0] = uint8_t(chunk[0] * 255);
                              buf[1] = uint8_t(chunk[1] * 255);
                              buf[2] = uint8_t(chunk[2] * 255);
                              buf[3] = uint8_t(chunk[3] * 255);
                            } break;
                            case 2: {
                              const uint16_t *chunk = (uint16_t *)&(*it);
                              uint8_t *buf = vbuffer->data + vbufferOffset;
                              buf[0] = uint8_t(
                                  (float(chunk[0]) / float(UINT16_MAX)) * 255);
                              buf[1] = uint8_t(
                                  (float(chunk[1]) / float(UINT16_MAX)) * 255);
                              buf[2] = uint8_t(
                                  (float(chunk[2]) / float(UINT16_MAX)) * 255);
                              buf[3] = uint8_t(
                                  (float(chunk[3]) / float(UINT16_MAX)) * 255);
                            } break;
                            case 1: {
                              const uint8_t *chunk = (uint8_t *)&(*it);
                              uint8_t *buf = vbuffer->data + vbufferOffset;
                              memcpy(buf, chunk, 4);
                            } break;
                            default:
                              CBLOG_FATAL("invalid state");
                              ;
                              break;
                            }
                          } break;
                          default:
                            CBLOG_FATAL("invalid state");
                            ;
                            break;
                          }

                          vbufferOffset += vertexSize;
                          it += stride;
                          idx++;
                        }

                        prims.layout.add(attrib, 4, bgfx::AttribType::Uint8,
                                         true);
                      } break;
                      case bgfx::Attrib::TexCoord0:
                      case bgfx::Attrib::TexCoord1:
                      case bgfx::Attrib::TexCoord2:
                      case bgfx::Attrib::TexCoord3:
                      case bgfx::Attrib::TexCoord4:
                      case bgfx::Attrib::TexCoord5:
                      case bgfx::Attrib::TexCoord6:
                      case bgfx::Attrib::TexCoord7: {
                        const auto elemSize = [&]() {
                          if (accessor.componentType ==
                              TINYGLTF_COMPONENT_TYPE_FLOAT)
                            return 4;
                          else if (accessor.componentType ==
                                   TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                            return 2;
                          else // BYTE
                            return 1;
                        }();
                        const auto osize = sizeof(float) * 2;
                        auto vbufferOffset = offsetSize;
                        offsetSize += osize;

                        size_t idx = 0;
                        auto it = dataBeg;
                        while (idx < vertexCount) {
                          switch (elemSize) {
                          case 4: { // float
                            const float *chunk = (float *)&(*it);
                            float *buf =
                                (float *)(vbuffer->data + vbufferOffset);
                            buf[0] = chunk[0];
                            buf[1] = chunk[1];
                          } break;
                          case 2: {
                            const uint16_t *chunk = (uint16_t *)&(*it);
                            float *buf =
                                (float *)(vbuffer->data + vbufferOffset);
                            buf[0] = float(chunk[0]) / float(UINT16_MAX);
                            buf[1] = float(chunk[1]) / float(UINT16_MAX);
                          } break;
                          case 1: {
                            const uint8_t *chunk = (uint8_t *)&(*it);
                            float *buf =
                                (float *)(vbuffer->data + vbufferOffset);
                            buf[0] = float(chunk[0]) / float(255);
                            buf[1] = float(chunk[1]) / float(255);
                          } break;
                          default:
                            CBLOG_FATAL("invalid state");
                            ;
                            break;
                          }

                          vbufferOffset += vertexSize;
                          it += stride;
                          idx++;
                        }

                        prims.layout.add(attrib, 2, bgfx::AttribType::Float);
                      } break;
                      default:
                        throw std::runtime_error("Invalid attribute.");
                        break;
                      }
                    }
                    // wrap up layout
                    prims.layout.end();
                    assert(prims.layout.getSize(1) == vertexSize);
                    prims.vb = bgfx::createVertexBuffer(vbuffer, prims.layout);

                    // check if we have indices
                    if (glprims.indices != -1) {
                      // alright we also use the IB
                      const auto &indices = gltf.accessors[glprims.indices];
                      const auto count = indices.count;
                      int esize;
                      if (count < UINT16_MAX) {
                        esize = 2;
                      } else {
                        esize = 4;
                      }
                      auto ibuffer = bgfx::alloc(esize * count);
                      auto offset = 0;
                      int gsize;
                      // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#indices
                      if (indices.componentType ==
                          TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        gsize = 4;
                      } else if (indices.componentType ==
                                 TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        gsize = 2;
                      } else {
                        gsize = 1;
                      }
                      const auto &view = gltf.bufferViews[indices.bufferView];
                      const auto &buffer = gltf.buffers[view.buffer];
                      const auto dataBeg = buffer.data.begin() +
                                           view.byteOffset + indices.byteOffset;
                      const auto stride = indices.ByteStride(view);

#define FILL_INDEX                                                             \
  if (esize == 4) {                                                            \
    uint32_t *ibuf = (uint32_t *)(ibuffer->data + offset);                     \
    *ibuf = uint32_t(*chunk);                                                  \
  } else {                                                                     \
    uint16_t *ibuf = (uint16_t *)(ibuffer->data + offset);                     \
    *ibuf = uint16_t(*chunk);                                                  \
  }
                      size_t idx = 0;
                      auto it = dataBeg;
                      while (idx < count) {
                        switch (gsize) {
                        case 4: {
                          const uint32_t *chunk = (uint32_t *)&(*it);
                          FILL_INDEX;
                        } break;
                        case 2: {
                          const uint16_t *chunk = (uint16_t *)&(*it);
                          FILL_INDEX;
                        } break;
                        case 1: {
                          const uint8_t *chunk = (uint8_t *)&(*it);
                          FILL_INDEX;
                        } break;
                        default:
                          CBLOG_FATAL("invalid state");
                          ;
                          break;
                        }

                        offset += esize;
                        it += stride;
                        idx++;
                      }
#undef FILL_INDEX
                      prims.ib = bgfx::createIndexBuffer(
                          ibuffer, esize == 4 ? BGFX_BUFFER_INDEX32 : 0);
                    } else {
                      prims.stateFlags = BGFX_STATE_PT_TRISTRIP;
                    }

                    const auto determinant =
                        linalg::determinant(node.transform);
                    if (!(determinant < 0.0)) {
                      prims.stateFlags |= BGFX_STATE_FRONT_CCW;
                    }

                    if (glprims.material != -1) {
                      prims.material =
                          _model->gfxMaterials.emplace_back(processMaterial(
                              context, gltf, gltf.materials[glprims.material],
                              shaderDefines, varyings));
                    }

                    mesh.primitives.emplace_back(
                        _model->gfxPrimitives.emplace_back(std::move(prims)));
                  }
                }
                node.mesh = _model->gfxMeshes.emplace_back(std::move(mesh));
              }

              for (const auto childIndex : glnode.children) {
                const auto &subglnode = gltf.nodes[childIndex];
                node.children.emplace_back(processNode(subglnode));
              }

              return NodeRef(_model->nodes.emplace_back(std::move(node)));
            };
        _model->rootNodes.emplace_back(processNode(glnode));
      }

      return ModelVar.Get(_model);
    });
  }
};

struct Draw : public BGFX::BaseConsumer {
  ParamVar _model{};
  ParamVar _materials{};
  CBVar *_bgfxContext{nullptr};
  std::array<CBExposedTypeInfo, 5> _required;
  std::unordered_map<size_t,
                     std::optional<std::pair<const CBVar *, const CBVar *>>>
      _matsCache;

  static inline Types MaterialTableValues3{{BGFX::Texture::SeqType}};
  static inline std::array<CBString, 1> MaterialTableKeys3{"Textures"};
  static inline Type MaterialTableType3 =
      Type::TableOf(MaterialTableValues3, MaterialTableKeys3);
  static inline Type MaterialsTableType3 = Type::TableOf(MaterialTableType3);
  static inline Type MaterialsTableVarType3 =
      Type::VariableOf(MaterialsTableType3);

  static inline Types MaterialTableValues2{
      {BGFX::ShaderHandle::ObjType, BGFX::Texture::SeqType}};
  static inline std::array<CBString, 2> MaterialTableKeys2{"Shader",
                                                           "Textures"};
  static inline Type MaterialTableType2 =
      Type::TableOf(MaterialTableValues2, MaterialTableKeys2);
  static inline Type MaterialsTableType2 = Type::TableOf(MaterialTableType2);
  static inline Type MaterialsTableVarType2 =
      Type::VariableOf(MaterialsTableType2);

  static inline Types MaterialTableValues{{BGFX::ShaderHandle::ObjType}};
  static inline std::array<CBString, 1> MaterialTableKeys{"Shader"};
  static inline Type MaterialTableType =
      Type::TableOf(MaterialTableValues, MaterialTableKeys);
  static inline Type MaterialsTableType = Type::TableOf(MaterialTableType);
  static inline Type MaterialsTableVarType =
      Type::VariableOf(MaterialsTableType);

  static CBTypesInfo inputTypes() { return CoreInfo::Float4x4Types; }
  static CBTypesInfo outputTypes() { return CoreInfo::Float4x4Types; }

  static inline Parameters Params{
      {"Model",
       CBCCSTR("The GLTF model to render."),
       {Load::ModelType, Load::ModelVarType}},
      {"Materials",
       CBCCSTR("The materials override table, to override the default PBR "
               "metallic-roughness by primitive material name. The table must "
               "be like {Material-Name <name> {Shader <shader> Textures "
               "[<texture>]}} - Textures can be omitted."),
       {CoreInfo::NoneType, MaterialsTableType, MaterialsTableVarType,
        MaterialsTableType2, MaterialsTableVarType2, MaterialsTableType3,
        MaterialsTableVarType3}}};
  static CBParametersInfo parameters() { return Params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _model = value;
      break;
    case 1:
      _materials = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _model;
    case 1:
      return _materials;
    default:
      throw InvalidParameterIndex();
    }
  }

  CBExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = BGFX::BaseConsumer::ContextInfo;
    idx++;

    if (_model.isVariable()) {
      _required[idx].name = _model.variableName();
      _required[idx].help = CBCCSTR("The required model.");
      _required[idx].exposedType = Load::ModelType;
      idx++;
    }
    if (_materials.isVariable()) {
      _required[idx].name = _materials.variableName();
      _required[idx].help = CBCCSTR("The required materials table.");
      _required[idx].exposedType = MaterialsTableType;
      idx++;
      // OR
      _required[idx].name = _materials.variableName();
      _required[idx].help = CBCCSTR("The required materials table.");
      _required[idx].exposedType = MaterialsTableType2;
      idx++;
      // OR
      _required[idx].name = _materials.variableName();
      _required[idx].help = CBCCSTR("The required materials table.");
      _required[idx].exposedType = MaterialsTableType3;
      idx++;
    }
    return {_required.data(), uint32_t(idx), 0};
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    BGFX::BaseConsumer::compose(data);

    if (data.inputType.seqTypes.elements[0].basicType == CBType::Seq) {
      // TODO
      OVERRIDE_ACTIVATE(data, activate);
    } else {
      OVERRIDE_ACTIVATE(data, activateSingle);
    }
    return data.inputType;
  }

  void warmup(CBContext *context) {
    _model.warmup(context);
    _materials.warmup(context);
    _bgfxContext = referenceVariable(context, "GFX.Context");
  }

  void cleanup() {
    _matsCache.clear();
    _model.cleanup();
    _materials.cleanup();
    if (_bgfxContext) {
      releaseVariable(_bgfxContext);
      _bgfxContext = nullptr;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    throw ActivationError("Not yet implemented.");
    return input;
  }

  void renderNode(BGFX::Context *ctx, const Node &node,
                  const linalg::aliases::float4x4 &parentTransform,
                  const CBTable *mats) {
    const auto transform = linalg::mul(parentTransform, node.transform);
    if (node.mesh) {
      for (const auto &primsRef : node.mesh->get().primitives) {
        const auto &prims = primsRef.get();
        if (prims.vb.idx != bgfx::kInvalidHandle) {
          const auto currentView = ctx->currentView();

          uint64_t state = prims.stateFlags | BGFX_STATE_WRITE_RGB |
                           BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
                           BGFX_STATE_DEPTH_TEST_LESS;

          if (!prims.material || !(*prims.material).get().doubleSided) {
            if constexpr (BGFX::CurrentRenderer == BGFX::Renderer::OpenGL) {
              // workaround for flipped Y render to textures
              if (currentView.id > 0) {
                state |= BGFX_STATE_CULL_CCW;
              } else {
                state |= BGFX_STATE_CULL_CW;
              }
            } else {
              state |= BGFX_STATE_CULL_CW;
            }
          }

          bgfx::setState(state);

          bgfx::setVertexBuffer(0, prims.vb);
          if (prims.ib.idx != bgfx::kInvalidHandle) {
            bgfx::setIndexBuffer(prims.ib);
          }

          bgfx::ProgramHandle handle = BGFX_INVALID_HANDLE;

          if (prims.material) {
            const auto &material = (*prims.material).get();
            const CBVar *pshader = nullptr;
            const CBVar *ptextures = nullptr;

            if (mats) {
              const auto it = _matsCache.find(material.hash);
              if (it != _matsCache.end()) {
                if (it->second) {
                  pshader = it->second->first;
                  ptextures = it->second->second;
                }
              } else {
                // not found, let's cache it as well
                const auto override =
                    mats->api->tableAt(*mats, material.name.c_str());
                if (override->valueType == CBType::Table) {
                  const auto &records = override->payload.tableValue;
                  pshader = records.api->tableAt(records, "Shader");
                  ptextures = records.api->tableAt(records, "Textures");
                  // add to quick cache map by integer
                  _matsCache[material.hash] =
                      std::make_pair(pshader, ptextures);
                } else {
                  _matsCache[material.hash].reset();
                }
              }
            }

            // try override shader, if not use the default PBR shader
            if (pshader && pshader->valueType == CBType::Object) {
              // we got the shader
              const auto shader = reinterpret_cast<BGFX::ShaderHandle *>(
                  pshader->payload.objectValue);
              handle = shader->handle;
            } else if (material.shader) {
              handle = material.shader->handle;
            }

            // if textures are empty, use the ones we loaded during Load
            if (ptextures && ptextures->valueType == CBType::Seq &&
                ptextures->payload.seqValue.len > 0) {
              const auto textures = ptextures->payload.seqValue;
              for (uint32_t i = 0; i < textures.len; i++) {
                const auto texture = reinterpret_cast<BGFX::Texture *>(
                    textures.elements[i].payload.objectValue);
                bgfx::setTexture(uint8_t(i), ctx->getSampler(i),
                                 texture->handle);
              }
            } else {
              uint8_t samplerSlot = 0;
              if (material.baseColorTexture) {
                bgfx::setTexture(samplerSlot, ctx->getSampler(samplerSlot),
                                 material.baseColorTexture->handle);
                samplerSlot++;
              }
              if (material.normalTexture) {
                bgfx::setTexture(samplerSlot, ctx->getSampler(samplerSlot),
                                 material.normalTexture->handle);
                samplerSlot++;
              }
            }
          }

          float mat[16];
          memcpy(&mat[0], &transform.x, sizeof(float) * 4);
          memcpy(&mat[4], &transform.y, sizeof(float) * 4);
          memcpy(&mat[8], &transform.z, sizeof(float) * 4);
          memcpy(&mat[12], &transform.w, sizeof(float) * 4);
          bgfx::setTransform(mat);

          if (handle.idx == bgfx::kInvalidHandle) {
            std::string *matName = nullptr;
            if (prims.material) {
              const auto &material = (*prims.material).get();
              matName = &material.name;
            }
            CBLOG_ERROR("Rendering a primitive with invalid shader handle, "
                        "mesh: {} material: {} mats table: {}",
                        node.mesh->get().name,
                        (matName ? matName->c_str() : "<no material>"),
                        _materials.get());
            throw ActivationError(
                "Rendering a primitive with invalid shader handle");
          }

          bgfx::submit(currentView.id, handle);
        }
      }
    }

    for (const auto &snode : node.children) {
      renderNode(ctx, snode, transform, mats);
    }
  }

  CBVar activateSingle(CBContext *context, const CBVar &input) {
    auto *ctx =
        reinterpret_cast<BGFX::Context *>(_bgfxContext->payload.objectValue);

    const auto &modelVar = _model.get();
    const auto model = reinterpret_cast<Model *>(modelVar.payload.objectValue);
    const auto &matsVar = _materials.get();
    const auto mats = matsVar.valueType != CBType::None
                          ? &matsVar.payload.tableValue
                          : nullptr;

    auto rootTransform =
        reinterpret_cast<Mat4 *>(&input.payload.seqValue.elements[0]);
    for (const auto &nodeRef : model->rootNodes) {
      renderNode(ctx, nodeRef.get(), *rootTransform, mats);
    }
    return input;
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("GLTF.Load", Load);
  REGISTER_CBLOCK("GLTF.Draw", Draw);
}
} // namespace gltf
} // namespace chainblocks

#ifdef CB_INTERNAL_TESTS
#include "gltf_tests.cpp"
#endif