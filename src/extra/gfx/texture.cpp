#include "../gfx.hpp"
#include "common_types.hpp"
#include "extra/gfx/shards_types.hpp"
#include "foundation.hpp"
#include "linalg_shim.hpp"
#include "shards.h"
#include "shards.hpp"
#include <array>
#include <gfx/texture.hpp>
#include <gfx/error_utils.hpp>
#include <gfx/render_target.hpp>
#include <params.hpp>
#include <stdexcept>

using namespace shards;
namespace gfx {

enum ComponentType {
  Float,
  Int8,
  Int16,
};

struct TextureFormatException : public std::runtime_error {
  TextureFormatException(ComponentType componentType, Types::TextureType_ asType)
      : std::runtime_error(formatError(componentType, asType)) {}

  static std::string formatError(ComponentType componentType, Types::TextureType_ asType) {
    return fmt::format("Image with component type '{}' can not be converted to texture type '{}'",
                       magic_enum::enum_name(componentType), magic_enum::enum_name(asType));
  }
};

struct TextureShard {
  using TextureType = Types::TextureType_;

  static inline shards::Types InputTypes{{CoreInfo::ImageType, CoreInfo::NoneType, CoreInfo::AnyType}};
  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHTypesInfo outputTypes() { return Types::TextureTypes; }
  static SHOptionalString help() { return SHCCSTR("Creates a texture from an image. Or as a render target"); }

  TexturePtr texture;
  OwnedVar textureVar;
  bool _createFromImage{};

  PARAM_VAR(_interpretAs, "InterpretAs",
            "Type to interpret image data as. (From image only, Default: UNormSRGB for RGBA8 images, UNorm for other formats)",
            {Types::TextureTypeEnumInfo::Type});
  PARAM_PARAMVAR(_format, "Format",
                 "The format to use to create the texture. The texture will be usable as a render target. (Render target only)",
                 {Types::TextureFormatEnumInfo::Type, Type::VariableOf(Types::TextureFormatEnumInfo::Type)});
  PARAM_PARAMVAR(_resolution, "Resolution", "The resolution of the texture to create. (Render target only)",
                 {CoreInfo::Int2Type, Type::VariableOf(CoreInfo::Int2Type)});
  PARAM_PARAMVAR(_mipLevels, "MipLevels", "The number of mip levels to create. (Render target only)",
                 {CoreInfo::IntType, Type::VariableOf(CoreInfo::IntType)});
  PARAM_VAR(_dimension, "Dimension", "The type of texture to create. (Render target only)",
            {Types::TextureDimensionEnumInfo::Type});
  PARAM_PARAMVAR(_addressing, "Addressing", "For sampling, sets the address modes.",
                 {Types::TextureAddressingEnumInfo::Type, Type::SeqOf(Types::TextureAddressingEnumInfo::Type)});
  PARAM_PARAMVAR(_filtering, "Filtering", "For sampling, sets the filter mode.", {Types::TextureFilteringEnumInfo::Type});
  PARAM_IMPL(TextureShard, PARAM_IMPL_FOR(_interpretAs), PARAM_IMPL_FOR(_format), PARAM_IMPL_FOR(_resolution),
             PARAM_IMPL_FOR(_mipLevels), PARAM_IMPL_FOR(_dimension), PARAM_IMPL_FOR(_addressing), PARAM_IMPL_FOR(_filtering));

  TextureShard() {}

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    texture = std::make_shared<Texture>();

    // Set TypeId based on texture dimension so we can check bindings at compose time
    gfx::TextureDimension dim = getTextureDimension();
    switch (dim) {
    case gfx::TextureDimension::D1:
      throw formatException("TextureDimension.D1 not supported");
    case gfx::TextureDimension::D2:
      textureVar = Var::Object(&texture, gfx::VendorId, Types::TextureTypeId);
      break;
    case gfx::TextureDimension::Cube:
      textureVar = Var::Object(&texture, gfx::VendorId, Types::TextureCubeTypeId);
      break;
    }
  }

  void cleanup() {
    PARAM_CLEANUP();
    texture.reset();
  }

  gfx::TextureDimension getTextureDimension() const {
    return !_dimension->isNone() ? gfx::TextureDimension(_dimension.payload.enumValue) : gfx::TextureDimension::D2;
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (!_interpretAs->isNone()) {
      if (_format->valueType != SHType::None)
        throw ComposeError("Can not specify Format and InterpretAs parameters at the same time");
      if (!_dimension->isNone())
        throw ComposeError("Can not specify Dimension and InterpretAs parameters at the same time");
      if (_resolution->valueType != SHType::None)
        throw ComposeError("Can not specify Resolution and InterpretAs parameters at the same time");
      if (_mipLevels->valueType != SHType::None)
        throw ComposeError("Can not specify MipLevels and InterpretAs parameters at the same time");
    }

    if (_format->valueType != SHType::None) {
      _createFromImage = false;
    } else {
      if (data.inputType != CoreInfo::ImageType) {
        throw ComposeError("Format is required when not creating a texture from an image");
      }
      _createFromImage = true;
    }

    auto dim = getTextureDimension();
    if (dim == gfx::TextureDimension::Cube)
      return Types::TextureCube;
    else
      return Types::Texture;
  }

  void activateFromImage(const SHImage &image) {
    ComponentType componentType;
    TextureType asType;
    if (image.flags & SHIMAGE_FLAGS_32BITS_FLOAT) {
      asType = TextureType::Float;
      componentType = ComponentType::Float;
    } else if (image.flags & SHIMAGE_FLAGS_16BITS_INT) {
      asType = TextureType::UInt;
      componentType = ComponentType::Int16;
    } else {
      componentType = ComponentType::Int8;
      if (image.channels == 4)
        asType = TextureType::UNormSRGB;
      else
        asType = TextureType::UNorm;
    }

    TextureType paramAsType = !_interpretAs->isNone() ? (TextureType)_interpretAs.payload.enumValue : TextureType::Default;
    if (paramAsType != TextureType::Default) {
      asType = paramAsType;
    }

    TextureFormat format{};
    switch (image.channels) {
    case 1:
      switch (componentType) {
      case ComponentType::Float:
        if (asType == TextureType::Float)
          format.pixelFormat = WGPUTextureFormat_R32Float;
        break;
      case ComponentType::Int16:
        if (asType == TextureType::UInt)
          format.pixelFormat = WGPUTextureFormat_R16Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_R16Sint;
        break;
      case ComponentType::Int8:
        if (asType == TextureType::UNorm)
          format.pixelFormat = WGPUTextureFormat_R8Unorm;
        else if (asType == TextureType::SNorm)
          format.pixelFormat = WGPUTextureFormat_R8Snorm;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_R8Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_R8Sint;
        break;
      }
      break;
    case 2:
      switch (componentType) {
      case ComponentType::Float:
        if (asType == TextureType::Float)
          format.pixelFormat = WGPUTextureFormat_RG32Float;
        break;
      case ComponentType::Int16:
        if (asType == TextureType::UInt)
          format.pixelFormat = WGPUTextureFormat_RG16Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_RG16Sint;
        break;
      case ComponentType::Int8:
        if (asType == TextureType::UNorm)
          format.pixelFormat = WGPUTextureFormat_RG8Unorm;
        else if (asType == TextureType::SNorm)
          format.pixelFormat = WGPUTextureFormat_RG8Snorm;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_RG8Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_RG8Sint;
        break;
      }
      break;
    case 3:
      throw formatException("RGB textures not supported");
      
      // TODO
      // switch (componentType) {
      // case ComponentType::Float:
      //   if (asType == TextureType::Float)
      //     /* code */
      //   break;
      // case ComponentType::Int16:
      //   if (asType == TextureType::UInt)
      //     /* code */
      //   else if (asType == TextureType::Int)
      //     /* code */
      // case ComponentType::Int8:
      //   if (asType == TextureType::UNorm)
      //     /* code */
      //   else if (asType == TextureType::UNormSRGB)
      //     /* code */
      //   else if (asType == TextureType::SNorm)
      //     /* code */
      //   else if (asType == TextureType::UInt)
      //     /* code */
      //   else if (asType == TextureType::Int)
      //     /* code */
      //   break;
      // }
      // break;
      
    case 4:
      switch (componentType) {
      case ComponentType::Float:
        if (asType == TextureType::Float)
          format.pixelFormat = WGPUTextureFormat_RGBA32Float;
        break;
      case ComponentType::Int16:
        if (asType == TextureType::UInt)
          format.pixelFormat = WGPUTextureFormat_RGBA16Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_RGBA16Sint;
        break;
      case ComponentType::Int8:
        if (asType == TextureType::UNorm)
          format.pixelFormat = WGPUTextureFormat_RGBA8Unorm;
        else if (asType == TextureType::UNormSRGB)
          format.pixelFormat = WGPUTextureFormat_RGBA8UnormSrgb;
        else if (asType == TextureType::SNorm)
          format.pixelFormat = WGPUTextureFormat_RGBA8Snorm;
        else if (asType == TextureType::UInt)
          format.pixelFormat = WGPUTextureFormat_RGBA8Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_RGBA8Sint;
        break;
      }
      break;
    }

    if (format.pixelFormat == WGPUTextureFormat_Undefined)
      throw TextureFormatException(componentType, asType);

    auto &inputFormat = getTextureFormatDescription(format.pixelFormat);
    size_t imageSize = inputFormat.pixelSize * image.width * image.height;

    // Copy the data since we can't keep a reference to the image variable
    ImmutableSharedBuffer isb(image.data, imageSize);
    texture->init(TextureDesc{.format = format, .resolution = int2(image.width, image.height), .data = std::move(isb)});
  }

  void activateRenderableTexture() {
    Var resolutionVar{_resolution.get()};
    Var mipLevelsVar{_mipLevels.get()};
    Var formatVar{_format.get()};

    // NOTE: Use existing resultion if unspecified here to avoid resetting the texture
    int2 resolution = !resolutionVar.isNone() ? toInt2(resolutionVar) : int2(0);
    if (resolutionVar.isNone())
      resolution = texture->getDesc().resolution;

    uint8_t mipLevels = uint8_t(!mipLevelsVar.isNone() ? int(mipLevelsVar) : 1);

    texture->init(TextureDesc{
        .format =
            TextureFormat{
                .dimension = getTextureDimension(),
                .flags = TextureFormatFlags::RenderAttachment,
                .pixelFormat = (WGPUTextureFormat)formatVar.payload.enumValue,
                .mipLevels = mipLevels,
            },
        .resolution = resolution,
    });
  }

  void applySamplerSettings() {
    SamplerState samplerState;

    Var addressingModes{_addressing.get()};
    if (addressingModes.valueType == SHType::Seq) {
      auto &seq = addressingModes.payload.seqValue;
      if (seq.len >= 3) {
        throw formatException("Number of texture addressing modes are too many ({}, max: 3)", seq.len);
      }
      for (size_t i = 0; i < seq.len; i++) {
        (&samplerState.addressModeU)[i] = WGPUAddressMode(seq.elements[i].payload.enumValue);
      }
    } else if (!addressingModes.isNone()) {
      samplerState.addressModeU = WGPUAddressMode(addressingModes.payload.enumValue);
      samplerState.addressModeV = samplerState.addressModeU;
      samplerState.addressModeW = samplerState.addressModeU;
    }

    Var filteringVar{_filtering.get()};
    if (!filteringVar.isNone()) {
      samplerState.filterMode = WGPUFilterMode(filteringVar.payload.enumValue);
    }

    texture->initWithSamplerState(samplerState);
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    if (_createFromImage)
      activateFromImage(input.payload.imageValue);
    else
      activateRenderableTexture();

    applySamplerSettings();

    return textureVar;
  }
};

struct RenderTargetShard {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return Types::RenderTarget; }
  static SHOptionalString help() {
    return SHCCSTR("Groups a collection of textures into a render target that can be rendered into");
  }

  static inline std::array<SHString, 2> AttachmentTableKeys{"Texture", "Name"};
  static inline shards::Types AttachmentTableTypes{{CoreInfo::StringType}};
  static inline shards::Type AttachmentTable = Type::TableOf(AttachmentTableTypes, AttachmentTableKeys);

  PARAM_VAR(_attachments, "Attachments", "The list of attachements to create.", {Type::TableOf(AttachmentTable)});
  PARAM_IMPL(RenderTargetShard, PARAM_IMPL_FOR(_attachments));

  SHRenderTarget _renderTarget;
  OwnedVar _renderTargetVar{};

  RenderTargetShard() {}

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _renderTarget.renderTarget = std::make_shared<RenderTarget>();
    _renderTargetVar = Var::Object(&_renderTarget, gfx::VendorId, Types::RenderTargetTypeId);
  }
  void cleanup() { PARAM_CLEANUP(); }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &rt = _renderTarget.renderTarget;
    auto &attachments = rt->attachments;
    auto &table = _attachments.payload.tableValue;
    attachments.clear();
    ForEach(table, [&](SHString &k, SHVar &v) {
      TexturePtr texture = varAsObjectChecked<TexturePtr>(v, Types::Texture);
      attachments.emplace(k, texture);
    });

    return _renderTargetVar;
  }
};

struct RenderTargetTextureShard {
  static SHTypesInfo inputTypes() { return Types::RenderTarget; }
  static SHTypesInfo outputTypes() { return Types::Texture; }
  static SHOptionalString help() { return SHCCSTR("Retrieve a named attachment from a render target"); }

  PARAM_PARAMVAR(_nameVar, "Name", "Name of the attachment to retrieve", {CoreInfo::StringOrNone})
  PARAM_IMPL(RenderTargetTextureShard, PARAM_IMPL_FOR(_nameVar));

  TexturePtr texture;
  OwnedVar textureVar;

  std::string _name;
  bool isNameStatic = false;

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    isNameStatic = !_nameVar.isVariable();
    if (isNameStatic) {
      if (_nameVar->valueType == SHType::String) {
        _name = (const char *)Var(_nameVar.get());
      } else {
        _name = "color";
      }
    }
  }

  void cleanup() { PARAM_CLEANUP(); }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &renderTarget = varAsObjectChecked<RenderTargetPtr>(input, Types::RenderTarget);

    if (!isNameStatic) {
      _name = (const char *)(Var(_nameVar.get()));
    }

    texture = renderTarget->getAttachment(_name);
    textureVar = Var::Object(&texture, gfx::VendorId, Types::TextureTypeId);

    return textureVar;
  }
};

void registerTextureShards() {
  REGISTER_SHARD("GFX.Texture", TextureShard);
  REGISTER_SHARD("GFX.RenderTarget", RenderTargetShard);
  REGISTER_SHARD("GFX.RenderTargetTexture", RenderTargetTextureShard);
}
} // namespace gfx
