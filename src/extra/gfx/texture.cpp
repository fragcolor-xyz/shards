#include "../gfx.hpp"
#include "common_types.hpp"
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
  static SHTypesInfo outputTypes() { return Types::Texture; }
  static SHOptionalString help() { return SHCCSTR("Creates a texture from an image. Or as a render target"); }

  TexturePtr texture;
  OwnedVar textureVar;
  bool _createFromImage{};

  PARAM_VAR(_interpretAs, "InterpretAs",
            "Type to interpret image data as. (From image only, Default: UNormSRGB for RGBA8 images, UNorm for other formats)",
            {Types::TextureTypeEnumInfo::Type});
  PARAM_VAR(_format, "Format",
            "The format to use to create the texture. The texture will be usable as a render target. (Render target only)",
            {Types::TextureFormatEnumInfo::Type});
  PARAM_VAR(_resolution, "Resolution", "The resolution of the texture to create. (Render target only)", {CoreInfo::Int2Type});
  PARAM_IMPL(TextureShard, PARAM_IMPL_FOR(_interpretAs), PARAM_IMPL_FOR(_format), PARAM_IMPL_FOR(_resolution));

  TextureShard() {}

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    texture = std::make_shared<Texture>();
    textureVar = Var::Object(&texture, gfx::VendorId, Types::TextureTypeId);
  }

  void cleanup() {
    PARAM_CLEANUP();
    texture.reset();
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (!_format.isNone() && !_interpretAs.isNone()) {
      throw ComposeError("Can not specify both InterpretAs and Format parameters at the same time");
    }

    if (!_format.isNone()) {
      if (!_interpretAs.isNone()) {
        throw ComposeError("InterpretAs can only be used for image inputs");
      }
      _createFromImage = false;
    } else {
      if (data.inputType != CoreInfo::ImageType) {
        throw ComposeError("Format is required when not creating a texture from an image");
      }
      _createFromImage = true;
    }

    return outputTypes().elements[0];
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

    TextureType paramAsType = !_interpretAs.isNone() ? (TextureType)_interpretAs.payload.enumValue : TextureType::Default;
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
    int2 resolution = !_resolution.isNone() ? toInt2(_resolution) : int2(0);
    texture->init(TextureDesc{
        .format =
            TextureFormat{
                .flags = TextureFormatFlags::RenderAttachment,
                .pixelFormat = (WGPUTextureFormat)_format.payload.enumValue,
            },
        .resolution = resolution,
    });
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    if (_createFromImage)
      activateFromImage(input.payload.imageValue);
    else
      activateRenderableTexture();

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
      TexturePtr texture = *varAsObjectChecked<TexturePtr>(v, Types::Texture);
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
    auto &renderTarget = *varAsObjectChecked<RenderTargetPtr>(input, Types::RenderTarget);

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
