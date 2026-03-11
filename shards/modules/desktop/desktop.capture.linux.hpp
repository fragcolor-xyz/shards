/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#ifndef SH_DESKTOP_CAPTURE_LINUX
#define SH_DESKTOP_CAPTURE_LINUX

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/debug/types.h>
#include <spa/param/video/type-info.h>

#include <shards/log/log.hpp>
#include <atomic>
#include <cstring>
#include <mutex>
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

    // Connect core using the portal's fd
    _core = pw_context_connect_fd(_context, pipewireFd, nullptr, 0);
    if (!_core) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Failed to connect PipeWire core with fd {}", pipewireFd);
      pw_context_destroy(_context);
      _context = nullptr;
      pw_thread_loop_destroy(_loop);
      _loop = nullptr;
      return false;
    }

    // Create stream
    auto props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture",
                                   PW_KEY_MEDIA_ROLE, "Screen", nullptr);

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

    // Build format params
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    auto *params = static_cast<const struct spa_pod *>(spa_pod_builder_add_object(
        &b, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat, SPA_FORMAT_mediaType,
        SPA_POD_Id(SPA_MEDIA_TYPE_video), SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format,
        SPA_POD_CHOICE_ENUM_Id(4, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_BGRA,
                               SPA_VIDEO_FORMAT_RGBA),
        SPA_FORMAT_VIDEO_size,
        SPA_POD_CHOICE_RANGE_Rectangle(&SPA_RECTANGLE(1920, 1080), &SPA_RECTANGLE(1, 1),
                                       &SPA_RECTANGLE(8192, 8192)),
        SPA_FORMAT_VIDEO_framerate,
        SPA_POD_CHOICE_RANGE_Fraction(&SPA_FRACTION(30, 1), &SPA_FRACTION(0, 1), &SPA_FRACTION(144, 1))));

    pw_thread_loop_lock(_loop);

    if (pw_thread_loop_start(_loop) < 0) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Failed to start PipeWire thread loop");
      pw_thread_loop_unlock(_loop);
      shutdown();
      return false;
    }

    auto connectFlags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
    if (pw_stream_connect(_stream, PW_DIRECTION_INPUT, nodeId, connectFlags, &params, 1) < 0) {
      SPDLOG_LOGGER_ERROR(getCaptureLogger(), "Failed to connect PipeWire stream to node {}", nodeId);
      pw_thread_loop_unlock(_loop);
      shutdown();
      return false;
    }

    pw_thread_loop_unlock(_loop);

    SPDLOG_LOGGER_INFO(getCaptureLogger(), "PipeWire capture initialized for node {}", nodeId);
    return true;
  }

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
  bool hasFrame() const { return !_readBuffer.empty(); }

private:
  static void onStateChanged(void *data, enum pw_stream_state old, enum pw_stream_state state, const char *error) {
    auto self = static_cast<PipeWireCapture *>(data);
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
        &b, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers, SPA_PARAM_BUFFERS_buffers,
        SPA_POD_CHOICE_RANGE_Int(2, 1, 8), SPA_PARAM_BUFFERS_size, SPA_POD_Int(frameSize),
        SPA_PARAM_BUFFERS_stride, SPA_POD_Int(info.size.width * 4), SPA_PARAM_BUFFERS_dataType,
        SPA_POD_CHOICE_FLAGS_Int(1 << SPA_DATA_MemPtr)));

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
      pw_stream_queue_buffer(self->_stream, buf);
      return;
    }

    const uint8_t *srcData = static_cast<const uint8_t *>(spa_buf->datas[0].data);
    uint32_t srcStride = spa_buf->datas[0].chunk->stride;
    uint32_t srcSize = spa_buf->datas[0].chunk->size;

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
      for (int y = 0; y < h; y++) {
        const uint8_t *srcRow = srcData + y * srcStride;
        uint8_t *dstRow = self->_writeBuffer.data() + y * dstStride;

        switch (self->_spaFormat) {
        case SPA_VIDEO_FORMAT_BGRx:
        case SPA_VIDEO_FORMAT_BGRA:
          // Already BGRA layout, just copy (set alpha to 255 for BGRx)
          std::memcpy(dstRow, srcRow, dstStride);
          if (self->_spaFormat == SPA_VIDEO_FORMAT_BGRx) {
            for (int x = 0; x < w; x++)
              dstRow[x * 4 + 3] = 255;
          }
          break;
        case SPA_VIDEO_FORMAT_RGBx:
        case SPA_VIDEO_FORMAT_RGBA:
          // Swap R and B
          for (int x = 0; x < w; x++) {
            dstRow[x * 4 + 0] = srcRow[x * 4 + 2]; // B
            dstRow[x * 4 + 1] = srcRow[x * 4 + 1]; // G
            dstRow[x * 4 + 2] = srcRow[x * 4 + 0]; // R
            dstRow[x * 4 + 3] = (self->_spaFormat == SPA_VIDEO_FORMAT_RGBA) ? srcRow[x * 4 + 3] : 255;
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
};

} // namespace Desktop

#endif // SH_DESKTOP_CAPTURE_LINUX
