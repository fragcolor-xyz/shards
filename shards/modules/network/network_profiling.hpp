#ifndef DCC7EFA0_6B23_46A3_9662_69171A673B89
#define DCC7EFA0_6B23_46A3_9662_69171A673B89

#include <tracy/Wrapper.hpp>

#ifdef TRACY_ENABLE
namespace shards {
struct NetworkProfiler {
  std::thread thread_;
  std::atomic_bool running_ = true;

  std::atomic_uint32_t bytesSent = 0;
  std::atomic_uint32_t bytesReceived = 0;

  std::atomic_uint32_t kcpPollNext = 0;
  std::atomic_uint32_t kcpPollCount = 0;

  NetworkProfiler() : thread_(&NetworkProfiler::run, this) {}
  ~NetworkProfiler() {
    running_ = false;
    if (thread_.joinable()) {
      thread_.join();
    }
  }
  void run() {
    TracyPlotConfig("Network.BytesSent", tracy::PlotFormatType::Memory, true, true, 0xFF0000FF);
    TracyPlotConfig("Network.BytesReceived", tracy::PlotFormatType::Memory, true, true, 0xFF00FF00);
    TracyPlotConfig("Network.KcpPollNext", tracy::PlotFormatType::Number, true, true, 0xFFFFFFFF);

    do {
      uint32_t bytesSentNow = bytesSent.exchange(0);
      TracyPlot("Network.BytesSent", int64_t(bytesSentNow));
      uint32_t bytesReceivedNow = bytesReceived.exchange(0);
      TracyPlot("Network.BytesReceived", int64_t(bytesReceivedNow));

      uint32_t kcpPollNextNow = kcpPollNext.exchange(0);
      uint32_t kcpPollCountNow = kcpPollCount.exchange(0);

      if (kcpPollCountNow > 0) {
        double average = double(kcpPollNextNow) / double(kcpPollCountNow);
        TracyPlot("Network.KcpPollNext", int64_t(average));
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (running_);
  }
  static NetworkProfiler &instance() {
    static NetworkProfiler profiler;
    return profiler;
  }
};
} // namespace shards
#endif

#endif /* DCC7EFA0_6B23_46A3_9662_69171A673B89 */
