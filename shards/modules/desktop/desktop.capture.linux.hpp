/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SH_DESKTOP_CAPTURE_LINUX
#define SH_DESKTOP_CAPTURE_LINUX

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>
#include <spa/debug/types.h>
#include <spa/param/video/type-info.h>

// DRM format modifier constants — stable kernel ABI, avoids libdrm dependency
#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif
#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0ULL
#endif

#include <shards/log/log.hpp>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <sys/mman.h>
#include <vector>

namespace Desktop {

static inline shards::logging::Logger getCaptureLogger() {
  static auto logger = shards::logging::getOrCreate("Desktop.Capture");
  return logger;
}

class PipeWireCapture {
public:
  PipeWireCapture() = default;

  ~PipeWireCapture() { shutdown(); }

  PipeWireCapture(const PipeWireCapture &) = delete;
  PipeWireCapture &operator=(const PipeWireCapture &) = delete;

  bool init(int pipewireFd, uint32_t nodeId) {
    return initInternal(nodeId, pipewireFd);
  }

  // Direct PipeWire connection — bypasses portal, connects to PipeWire daemon directly.
  // Use for headless/automated scenarios where no portal consent dialog is possible.
  bool initDirect(uint32_t nodeId) {
    return initInternal(nodeId, -1, true);
  }

private:
  bool initInternal(uint32_t nodeId, int pipewireFd, bool direct = false) {
    pw_init(nullptr, nullptr);

    _loop = pw_thread_loop_new("shards-capture", nullptr);
    if (!_loop) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Failed to create PipeWire thread loop");
      return false;
    }

    _context = pw_context_new(pw_thread_loop_get_loop(_loop), nullptr, 0);
    if (!_context) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Failed to create PipeWire context");
      pw_thread_loop_destroy(_loop);
      _loop = nullptr;
      return false;
    }

    // Connect core — either via portal fd or directly to PipeWire daemon
    if (pipewireFd >= 0) {
      _core = pw_context_connect_fd(_context, pipewireFd, nullptr, 0);
    } else {
      _core = pw_context_connect(_context, nullptr, 0);
    }
    if (!_core) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Failed to connect PipeWire core{}",
                          pipewireFd >= 0 ? " with fd " + std::to_string(pipewireFd) : " (direct)");
      pw_context_destroy(_context);
      _context = nullptr;
      pw_thread_loop_destroy(_loop);
      _loop = nullptr;
      return false;
    }

    // Create stream
    auto props =
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Screen", nullptr);

    _stream = pw_stream_new(_core, "shards-screen-capture", props);
    if (!_stream) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Failed to create PipeWire stream");
      shutdown();
      return false;
    }

    // Set up stream events
    static const pw_stream_events streamEvents = {
        .version = PW_VERSION_STREAM_EVENTS,
        .state_changed = onStateChanged,
        .param_changed = onParamChanged,
        .process = onProcess,
    };

    pw_stream_add_listener(_stream, &_streamListener, &streamEvents, this);

    // Build format params — one per video format, each with DMA-BUF modifier support.
    // PipeWire requires separate params per format when modifiers are involved (like OBS does).
    // Also include one SHM-only fallback param with all formats but no modifier.
    uint8_t buffer[8192];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    struct spa_rectangle sizeDefault = SPA_RECTANGLE(1920, 1080);
    struct spa_rectangle sizeMin = SPA_RECTANGLE(1, 1);
    struct spa_rectangle sizeMax = SPA_RECTANGLE(8192, 8192);
    struct spa_fraction fpsDefault = SPA_FRACTION(30, 1);
    struct spa_fraction fpsMin = SPA_FRACTION(0, 1);
    struct spa_fraction fpsMax = SPA_FRACTION(144, 1);

    // All common 32-bit RGBA permutations
    static const uint32_t formats[] = {SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBx,
                                       SPA_VIDEO_FORMAT_RGBA, SPA_VIDEO_FORMAT_xBGR, SPA_VIDEO_FORMAT_ABGR,
                                       SPA_VIDEO_FORMAT_xRGB, SPA_VIDEO_FORMAT_ARGB};
    static const int numFormats = sizeof(formats) / sizeof(formats[0]);

    // Build params list. For portal capture (DMA-BUF compositors like Hyprland), DMA-BUF
    // params come first. For direct capture (SHM sources like GStreamer), SHM-only params
    // come first since the source won't have DMA-BUF support.
    const struct spa_pod *paramsList[numFormats + 1];
    int paramIdx = 0;

    // SHM params: all formats, no modifier
    auto *shmParam = static_cast<const struct spa_pod *>(spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
        SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), SPA_FORMAT_VIDEO_format,
        SPA_POD_CHOICE_ENUM_Id(8, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA,
                               SPA_VIDEO_FORMAT_xBGR, SPA_VIDEO_FORMAT_ABGR, SPA_VIDEO_FORMAT_xRGB, SPA_VIDEO_FORMAT_ARGB),
        SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&sizeDefault, &sizeMin, &sizeMax), SPA_FORMAT_VIDEO_framerate,
        SPA_POD_CHOICE_RANGE_Fraction(&fpsDefault, &fpsMin, &fpsMax)));

    // DMA-BUF params: one per format WITH modifier
    const struct spa_pod *dmabufParams[numFormats];
    for (int i = 0; i < numFormats; i++) {
      struct spa_pod_frame f;
      spa_pod_builder_push_object(&b, &f, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);
      spa_pod_builder_add(&b, SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), SPA_FORMAT_mediaSubtype,
                          SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw), SPA_FORMAT_VIDEO_format, SPA_POD_Id(formats[i]),
                          SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&sizeDefault, &sizeMin, &sizeMax),
                          SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&fpsDefault, &fpsMin, &fpsMax), 0);
      // Modifier as Choice enum with MANDATORY|DONT_FIXATE — accept any modifier
      spa_pod_builder_prop(&b, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
      struct spa_pod_frame fChoice;
      spa_pod_builder_push_choice(&b, &fChoice, SPA_CHOICE_Enum, 0);
      spa_pod_builder_long(&b, DRM_FORMAT_MOD_INVALID); // default
      spa_pod_builder_long(&b, DRM_FORMAT_MOD_INVALID); // any
      spa_pod_builder_long(&b, DRM_FORMAT_MOD_LINEAR);  // linear (unmodified)
      spa_pod_builder_pop(&b, &fChoice);
      dmabufParams[i] = static_cast<const struct spa_pod *>(spa_pod_builder_pop(&b, &f));
    }

    int numParams;
    if (direct) {
      // Direct capture: SHM only (no DMA-BUF from non-compositor sources)
      paramsList[0] = shmParam;
      numParams = 1;
    } else {
      // Portal capture: DMA-BUF first (compositors prefer it), SHM fallback last
      for (int i = 0; i < numFormats; i++)
        paramsList[paramIdx++] = dmabufParams[i];
      paramsList[paramIdx++] = shmParam;
      numParams = numFormats + 1;
    }

    pw_thread_loop_lock(_loop);

    if (pw_thread_loop_start(_loop) < 0) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Failed to start PipeWire thread loop");
      pw_thread_loop_unlock(_loop);
      shutdown();
      return false;
    }

    auto connectFlags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
    if (pw_stream_connect(_stream, PW_DIRECTION_INPUT, nodeId, connectFlags, paramsList, numParams) < 0) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Failed to connect PipeWire stream to node {}", nodeId);
      pw_thread_loop_unlock(_loop);
      shutdown();
      return false;
    }

    pw_thread_loop_unlock(_loop);

    SPDLOG_LOGGER_INFO(getCaptureLogger(), "PipeWire capture initialized for node {}", nodeId);
    return true;
  }

public:
  void shutdown() {
    if (_loop)
      pw_thread_loop_lock(_loop);

    if (_stream) {
      pw_stream_destroy(_stream);
      _stream = nullptr;
    }

    if (_core) {
      pw_core_disconnect(_core);
      _core = nullptr;
    }

    if (_context) {
      pw_context_destroy(_context);
      _context = nullptr;
    }

    if (_loop) {
      pw_thread_loop_unlock(_loop);
      pw_thread_loop_destroy(_loop);
      _loop = nullptr;
    }
  }

  // Swap buffers (called from main thread via CaptureFrame)
  void update() {
    std::lock_guard<std::mutex> lock(_bufferMutex);
    if (_newFrame) {
      std::swap(_readBuffer, _writeBuffer);
      _readWidth = _writeWidth;
      _readHeight = _writeHeight;
      _newFrame = false;
    }
  }

  const uint8_t *image() const { return _readBuffer.empty() ? nullptr : _readBuffer.data(); }
  int width() const { return _readWidth; }
  int height() const { return _readHeight; }
  bool hasFrame() const { return !_readBuffer.empty() && _readWidth > 0 && _readHeight > 0; }

private:
  static void onStateChanged(void *data, enum pw_stream_state old, enum pw_stream_state state, const char *error) {
    (void)data;
    SPDLOG_LOGGER_INFO(getCaptureLogger(), "Stream state: {} -> {}", pw_stream_state_as_string(old),
                       pw_stream_state_as_string(state));
    if (state == PW_STREAM_STATE_ERROR && error) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Stream error: {}", error);
    }
  }

  static void onParamChanged(void *data, uint32_t id, const struct spa_pod *param) {
    if (!param || id != SPA_PARAM_Format)
      return;

    auto self = static_cast<PipeWireCapture *>(data);

    struct spa_video_info_raw info;
    if (spa_format_video_raw_parse(param, &info) < 0) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Failed to parse video format");
      return;
    }

    self->_spaFormat = info.format;
    SPDLOG_LOGGER_INFO(getCaptureLogger(), "Negotiated format: {}x{} fmt={}", info.size.width, info.size.height,
                       spa_debug_type_find_name(spa_type_video_format, info.format));

    // Allocate buffers
    size_t frameSize = info.size.width * info.size.height * 4; // BGRA
    {
      std::lock_guard<std::mutex> lock(self->_bufferMutex);
      self->_writeWidth = info.size.width;
      self->_writeHeight = info.size.height;
      self->_writeBuffer.resize(frameSize);
      self->_readBuffer.resize(frameSize);
    }

    // Tell PipeWire about buffer requirements
    uint8_t paramsBuffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(paramsBuffer, sizeof(paramsBuffer));

    auto *bufferParams = static_cast<const struct spa_pod *>(spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers, SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(2, 1, 8),
        SPA_PARAM_BUFFERS_size, SPA_POD_Int(frameSize), SPA_PARAM_BUFFERS_stride, SPA_POD_Int(info.size.width * 4),
        SPA_PARAM_BUFFERS_dataType,
        SPA_POD_CHOICE_FLAGS_Int((1 << SPA_DATA_MemPtr) | (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_DmaBuf))));

    pw_stream_update_params(self->_stream, &bufferParams, 1);
  }

  static void onProcess(void *data) {
    auto self = static_cast<PipeWireCapture *>(data);

    struct pw_buffer *buf = pw_stream_dequeue_buffer(self->_stream);
    if (!buf) {
      SPDLOG_LOGGER_TRACE(getCaptureLogger(), "No buffer available");
      return;
    }

    struct spa_buffer *spa_buf = buf->buffer;
    if (!spa_buf->datas[0].data) {
      SPDLOG_LOGGER_DEBUG(getCaptureLogger(), "Buffer data is NULL (type={}, fd={}, size={})",
                          spa_buf->datas[0].type, spa_buf->datas[0].fd, spa_buf->datas[0].maxsize);
      // DMA-BUF: try mmap via fd if MAP_BUFFERS didn't handle it
      if (spa_buf->datas[0].type == SPA_DATA_DmaBuf && spa_buf->datas[0].fd >= 0) {
        size_t mapSize = spa_buf->datas[0].maxsize;
        if (mapSize == 0)
          mapSize = self->_writeWidth * self->_writeHeight * 4;
        void *mapped = mmap(nullptr, mapSize, PROT_READ, MAP_SHARED, spa_buf->datas[0].fd, 0);
        if (mapped != MAP_FAILED) {
          spa_buf->datas[0].data = mapped;
          spa_buf->datas[0].maxsize = mapSize;
          self->_dmabufMapped = mapped;
          self->_dmabufMapSize = mapSize;
        } else {
          SPDLOG_LOGGER_TRACE(getCaptureLogger(), "DMA-BUF mmap failed: {}", strerror(errno));
          pw_stream_queue_buffer(self->_stream, buf);
          return;
        }
      } else {
        pw_stream_queue_buffer(self->_stream, buf);
        return;
      }
    }

    const uint8_t *srcData = static_cast<const uint8_t *>(spa_buf->datas[0].data);
    uint32_t srcStride = spa_buf->datas[0].chunk->stride;

    {
      std::lock_guard<std::mutex> lock(self->_bufferMutex);

      int w = self->_writeWidth;
      int h = self->_writeHeight;
      uint32_t dstStride = w * 4;

      if (self->_writeBuffer.size() < (size_t)(w * h * 4)) {
        pw_stream_queue_buffer(self->_stream, buf);
        return;
      }

      // Copy and convert to BGRA if needed
      // Memory layout for each format (byte order in memory):
      //   BGRx/BGRA: B G R A — already our target layout
      //   RGBx/RGBA: R G B A — swap [0]↔[2]
      //   xBGR/ABGR: A B G R — rotate: dst[0]=src[1], dst[1]=src[2], dst[2]=src[3], dst[3]=src[0]
      //   xRGB/ARGB: A R G B — dst[0]=src[3], dst[1]=src[2], dst[2]=src[1], dst[3]=src[0]
      for (int y = 0; y < h; y++) {
        const uint8_t *srcRow = srcData + y * srcStride;
        uint8_t *dstRow = self->_writeBuffer.data() + y * dstStride;

        switch (self->_spaFormat) {
        case SPA_VIDEO_FORMAT_BGRx:
        case SPA_VIDEO_FORMAT_BGRA:
          std::memcpy(dstRow, srcRow, dstStride);
          if (self->_spaFormat == SPA_VIDEO_FORMAT_BGRx) {
            for (int x = 0; x < w; x++)
              dstRow[x * 4 + 3] = 255;
          }
          break;
        case SPA_VIDEO_FORMAT_RGBx:
        case SPA_VIDEO_FORMAT_RGBA:
          for (int x = 0; x < w; x++) {
            dstRow[x * 4 + 0] = srcRow[x * 4 + 2]; // B
            dstRow[x * 4 + 1] = srcRow[x * 4 + 1]; // G
            dstRow[x * 4 + 2] = srcRow[x * 4 + 0]; // R
            dstRow[x * 4 + 3] = (self->_spaFormat == SPA_VIDEO_FORMAT_RGBA) ? srcRow[x * 4 + 3] : 255;
          }
          break;
        case SPA_VIDEO_FORMAT_xBGR:
        case SPA_VIDEO_FORMAT_ABGR:
          // src: [A/x, B, G, R] → dst: [B, G, R, A]
          for (int x = 0; x < w; x++) {
            dstRow[x * 4 + 0] = srcRow[x * 4 + 1]; // B
            dstRow[x * 4 + 1] = srcRow[x * 4 + 2]; // G
            dstRow[x * 4 + 2] = srcRow[x * 4 + 3]; // R
            dstRow[x * 4 + 3] = (self->_spaFormat == SPA_VIDEO_FORMAT_ABGR) ? srcRow[x * 4 + 0] : 255;
          }
          break;
        case SPA_VIDEO_FORMAT_xRGB:
        case SPA_VIDEO_FORMAT_ARGB:
          // src: [A/x, R, G, B] → dst: [B, G, R, A]
          for (int x = 0; x < w; x++) {
            dstRow[x * 4 + 0] = srcRow[x * 4 + 3]; // B
            dstRow[x * 4 + 1] = srcRow[x * 4 + 2]; // G
            dstRow[x * 4 + 2] = srcRow[x * 4 + 1]; // R
            dstRow[x * 4 + 3] = (self->_spaFormat == SPA_VIDEO_FORMAT_ARGB) ? srcRow[x * 4 + 0] : 255;
          }
          break;
        default:
          std::memcpy(dstRow, srcRow, dstStride);
          break;
        }
      }

      self->_newFrame = true;
    }

    pw_stream_queue_buffer(self->_stream, buf);

    // Unmap DMA-BUF if we mapped it manually
    if (self->_dmabufMapped) {
      munmap(self->_dmabufMapped, self->_dmabufMapSize);
      spa_buf->datas[0].data = nullptr; // clear so we re-map next time
      self->_dmabufMapped = nullptr;
      self->_dmabufMapSize = 0;
    }
  }

  struct pw_thread_loop *_loop = nullptr;
  struct pw_context *_context = nullptr;
  struct pw_core *_core = nullptr;
  struct pw_stream *_stream = nullptr;
  struct spa_hook _streamListener{};

  std::mutex _bufferMutex;
  std::vector<uint8_t> _readBuffer;
  std::vector<uint8_t> _writeBuffer;
  int _readWidth = 0;
  int _readHeight = 0;
  int _writeWidth = 0;
  int _writeHeight = 0;
  bool _newFrame = false;
  uint32_t _spaFormat = SPA_VIDEO_FORMAT_BGRx;
  void *_dmabufMapped = nullptr;
  size_t _dmabufMapSize = 0;
};

} // namespace Desktop

#endif // SH_DESKTOP_CAPTURE_LINUX
