#include "gfx.hpp"
#include "drawable_utils.hpp"
#include <shards/common_types.hpp>
#include "shards_types.hpp"
#include <shards/core/foundation.hpp>
#include <shards/linalg_shim.hpp>
#include <shards/shards.h>
#include <shards/shards.hpp>
#include <array>
#include <gfx/texture.hpp>
#include <gfx/error_utils.hpp>
#include <gfx/render_target.hpp>
#include <gfx/gpu_read_buffer.hpp>
#include <gfx/renderer.hpp>
#include <shards/core/params.hpp>
#include <stdexcept>
#include <vector>

using namespace shards;
namespace gfx {

enum ComponentType {
  Float,
  Int8,
  Int16,
};

struct TextureFormatException : public std::runtime_error {
  TextureFormatException(ComponentType componentType, ShardsTypes::TextureType_ asType)
      : std::runtime_error(formatError(componentType, asType)) {}

  static std::string formatError(ComponentType componentType, ShardsTypes::TextureType_ asType) {
    return fmt::format("Image with component type '{}' can not be converted to texture type '{}'",
                       magic_enum::enum_name(componentType), magic_enum::enum_name(asType));
  }
};

struct TextureShard {
  using TextureType = ShardsTypes::TextureType_;

  static inline shards::Types InputTypes{{CoreInfo::ImageType, CoreInfo::NoneType, CoreInfo::AnyType}};
  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHTypesInfo outputTypes() { return ShardsTypes::TextureTypes; }
  static SHOptionalString help() { return SHCCSTR("Creates a texture from an image. Or as a render target"); }

  TexturePtr texture;
  OwnedVar textureVar;
  bool _createFromImage{};

  PARAM_VAR(_interpretAs, "InterpretAs",
            "Type to interpret image data as. (From image only, Default: UNormSRGB for RGBA8 images, UNorm for other formats)",
            {ShardsTypes::TextureTypeEnumInfo::Type});
  PARAM_PARAMVAR(_format, "Format",
                 "The format to use to create the texture. The texture will be usable as a render target. (Render target only)",
                 {ShardsTypes::TextureFormatEnumInfo::Type, Type::VariableOf(ShardsTypes::TextureFormatEnumInfo::Type)});
  PARAM_PARAMVAR(_resolution, "Resolution", "The resolution of the texture to create. (Render target only)",
                 {CoreInfo::Int2Type, Type::VariableOf(CoreInfo::Int2Type)});
  PARAM_PARAMVAR(_mipLevels, "MipLevels", "The number of mip levels to create. (Render target only)",
                 {CoreInfo::IntType, Type::VariableOf(CoreInfo::IntType)});
  PARAM_VAR(_dimension, "Dimension", "The type of texture to create. (Render target only)",
            {ShardsTypes::TextureDimensionEnumInfo::Type});
  PARAM_PARAMVAR(_addressing, "Addressing", "For sampling, sets the address modes.",
                 {ShardsTypes::TextureAddressingEnumInfo::Type, Type::SeqOf(ShardsTypes::TextureAddressingEnumInfo::Type)});
  PARAM_PARAMVAR(_filtering, "Filtering", "For sampling, sets the filter mode.", {ShardsTypes::TextureFilteringEnumInfo::Type});
  PARAM_IMPL(PARAM_IMPL_FOR(_interpretAs), PARAM_IMPL_FOR(_format), PARAM_IMPL_FOR(_resolution), PARAM_IMPL_FOR(_mipLevels),
             PARAM_IMPL_FOR(_dimension), PARAM_IMPL_FOR(_addressing), PARAM_IMPL_FOR(_filtering));

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
      textureVar = Var::Object(&texture, gfx::VendorId, ShardsTypes::TextureTypeId);
      break;
    case gfx::TextureDimension::Cube:
      textureVar = Var::Object(&texture, gfx::VendorId, ShardsTypes::TextureCubeTypeId);
      break;
    }
  }

  void cleanup(SHContext* context) {
    PARAM_CLEANUP(context);
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
      return ShardsTypes::TextureCube;
    else
      return ShardsTypes::Texture;
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
      if (image.channels == 3 || image.channels == 4)
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
    ImmutableSharedBuffer isb{};
    if (image.channels == 3) {
      std::vector<uint8_t> imageDataRGBA = convertToRGBA(image);
      isb = ImmutableSharedBuffer(std::move(imageDataRGBA));
    } else {
      isb = ImmutableSharedBuffer(image.data, imageSize);
    }
    texture->init(TextureDesc{.format = format, .resolution = int2(image.width, image.height), .data = std::move(isb)});
  }

  std::vector<uint8_t> convertToRGBA(const SHImage &image) {
    std::vector<uint8_t> rgbaData(image.width * image.height * 4);

    for (size_t y = 0; y < image.height; ++y) {
      for (size_t x = 0; x < image.width; ++x) {
        size_t srcIndex = (y * image.width + x) * 3;
        size_t dstIndex = (y * image.width + x) * 4;

        rgbaData[dstIndex] = image.data[srcIndex];
        rgbaData[dstIndex + 1] = image.data[srcIndex + 1];
        rgbaData[dstIndex + 2] = image.data[srcIndex + 2];

        rgbaData[dstIndex + 3] = 255;
      }
    }

    SHLOG_TRACE("RGB conversion completed");
    return rgbaData;
  }

  void activateRenderableTexture() {
    Var resolutionVar{_resolution.get()};
    Var mipLevelsVar{_mipLevels.get()};
    Var formatVar{_format.get()};

    // NOTE: Use existing resolution if unspecified here to avoid resetting the texture
    int2 resolution = !resolutionVar.isNone() ? (int2)toInt2(resolutionVar) : int2(0);
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
  static SHTypesInfo outputTypes() { return ShardsTypes::RenderTarget; }
  static SHOptionalString help() {
    return SHCCSTR("Groups a collection of textures into a render target that can be rendered into");
  }

  static inline std::array<SHVar, 2> AttachmentTableKeys{Var("Texture"), Var("Name")};
  static inline shards::Types AttachmentTableTypes{{CoreInfo::StringType}};
  static inline shards::Type AttachmentTable = Type::TableOf(AttachmentTableTypes, AttachmentTableKeys);

  PARAM_VAR(_attachments, "Attachments", "The list of attachements to create.", {Type::TableOf(AttachmentTable)});
  PARAM_IMPL(PARAM_IMPL_FOR(_attachments));

  SHRenderTarget _renderTarget;
  OwnedVar _renderTargetVar{};

  RenderTargetShard() {}

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _renderTarget.renderTarget = std::make_shared<RenderTarget>();
    _renderTargetVar = Var::Object(&_renderTarget, gfx::VendorId, ShardsTypes::RenderTargetTypeId);
  }
  void cleanup(SHContext* context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &rt = _renderTarget.renderTarget;
    auto &attachments = rt->attachments;
    auto &table = _attachments.payload.tableValue;
    attachments.clear();
    ForEach(table, [&](SHVar &k, SHVar &v) {
      if (k.valueType != SHType::String)
        throw formatException("Invalid attachment name: {}", k);
      TexturePtr texture = varAsObjectChecked<TexturePtr>(v, ShardsTypes::Texture);
      std::string ks(SHSTRVIEW(k));
      attachments.emplace(std::move(ks), texture);
    });

    return _renderTargetVar;
  }
};

struct RenderTargetTextureShard {
  static SHTypesInfo inputTypes() { return ShardsTypes::RenderTarget; }
  static SHTypesInfo outputTypes() { return ShardsTypes::Texture; }
  static SHOptionalString help() { return SHCCSTR("Retrieve a named attachment from a render target"); }

  PARAM_PARAMVAR(_nameVar, "Name", "Name of the attachment to retrieve", {CoreInfo::StringOrNone})
  PARAM_IMPL(PARAM_IMPL_FOR(_nameVar));

  TexturePtr texture;
  OwnedVar textureVar;

  std::string _name;
  bool isNameStatic = false;

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    isNameStatic = !_nameVar.isVariable();
    if (isNameStatic) {
      if (_nameVar->valueType == SHType::String) {
        auto sv = SHSTRVIEW(_nameVar.get());
        _name = sv;
      } else {
        _name = "color";
      }
    }
  }

  void cleanup(SHContext* context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &renderTarget = varAsObjectChecked<RenderTargetPtr>(input, ShardsTypes::RenderTarget);

    if (!isNameStatic) {
      auto sv = SHSTRVIEW(_nameVar.get());
      _name = sv;
    }

    texture = renderTarget->getAttachment(_name);
    textureVar = Var::Object(&texture, gfx::VendorId, ShardsTypes::TextureTypeId);

    return textureVar;
  }
};

struct ReadTextureShard {
  static SHTypesInfo inputTypes() { return ShardsTypes::TextureTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::ImageType; }
  static SHOptionalString help() {
    return SHCCSTR("Adds a render step that reads back the rendered textures into a images, the returned images ");
  }

  static inline Type OutputSeqType = Type::SeqOf(CoreInfo::StringType);

  PARAM_VAR(_wait, "Wait", "Wait for read to complete", {CoreInfo::BoolType});
  // OwnedVar _outputs;
  PARAM_IMPL(PARAM_IMPL_FOR(_wait));

  RequiredGraphicsContext _requiredGraphicsContext;

  GpuTextureReadBufferPtr _readBuffer = makeGpuTextureReadBuffer();
  SHVar _image;
  std::vector<uint8_t> _imageBuffer;

  ReadTextureShard() { _wait = Var(false); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _requiredVariables.push_back(RequiredGraphicsContext::getExposedTypeInfo());
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _requiredGraphicsContext.warmup(context);
  }

  void cleanup(SHContext* context) {
    PARAM_CLEANUP(context);
    _requiredGraphicsContext.cleanup(context);
  }

  bool isSupportedFormat(WGPUTextureFormat format) const {
    auto &fmtDesc = getTextureFormatDescription(format);
    return isIntegerStorageType(fmtDesc.storageType) || // or float
           getStorageTypeSize(fmtDesc.storageType) == 4;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    TexturePtr texture = varToTexture(input);
    _requiredGraphicsContext->renderer->copyTexture(TextureSubResource(texture), _readBuffer, (bool)*_wait);

    // Poll for previously queued operation completion
    if (!*_wait)
      _requiredGraphicsContext->renderer->pollBufferCopies();

    _image.valueType = SHType::Image;
    auto &outImage = _image.payload.imageValue;
    if (_readBuffer->pixelFormat == WGPUTextureFormat_Undefined) {
      outImage.data = nullptr;
      outImage.flags = outImage.width = outImage.height = outImage.channels = 0;
    } else {
      if (isSupportedFormat(_readBuffer->pixelFormat)) {
      auto &fmtDesc = getTextureFormatDescription(_readBuffer->pixelFormat);
        size_t componentSize = getStorageTypeSize(fmtDesc.storageType);
        outImage.channels = fmtDesc.numComponents;
        if (!isIntegerStorageType(fmtDesc.storageType)) {
          outImage.flags = SHIMAGE_FLAGS_32BITS_FLOAT;
        } else if (componentSize == 2) {
          outImage.flags = SHIMAGE_FLAGS_16BITS_INT;
        } else {
        outImage.flags = SHIMAGE_FLAGS_NONE;
        }
        outImage.width = _readBuffer->size.x;
        outImage.height = _readBuffer->size.y;
        size_t rowLength = fmtDesc.pixelSize * fmtDesc.numComponents * _readBuffer->size.x;
        size_t imageSize = fmtDesc.pixelSize * fmtDesc.numComponents * _readBuffer->size.x * _readBuffer->size.y;
        _imageBuffer.resize(imageSize);
        outImage.data = _imageBuffer.data();
        for (size_t y = 0; y < _readBuffer->size.y; ++y) {
          memcpy(_imageBuffer.data() + rowLength * y, _readBuffer->data.data() + _readBuffer->stride * y, rowLength);
        }
      } else {
        throw formatException("Unsupported image storage type {}", magic_enum::enum_name(_readBuffer->pixelFormat));
      }
    }

    return _image;
  }
};

void registerTextureShards() {
  REGISTER_SHARD("GFX.Texture", TextureShard);
  REGISTER_SHARD("GFX.RenderTarget", RenderTargetShard);
  REGISTER_SHARD("GFX.RenderTargetTexture", RenderTargetTextureShard);
  REGISTER_SHARD("GFX.ReadTexture", ReadTextureShard);
}
} // namespace gfx
