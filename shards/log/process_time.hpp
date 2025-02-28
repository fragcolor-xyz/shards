#ifndef A3D46BB0_72BD_48CD_AC40_44503BFF61DB
#define A3D46BB0_72BD_48CD_AC40_44503BFF61DB

#include <spdlog/pattern_formatter.h>
#include <chrono>
#include <iomanip>
#include <sstream>

class TimeKeeper {
public:
  TimeKeeper() { startTime = std::chrono::high_resolution_clock::now(); }

  double getElapsedTime() {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = now - startTime;
    return std::chrono::duration<double>(elapsed).count();
  }

  std::chrono::high_resolution_clock::time_point startTime;
};

class ProcessTimeFlag : public spdlog::custom_flag_formatter {
public:
  ProcessTimeFlag(std::shared_ptr<TimeKeeper> timeKeeper) : timeKeeper(timeKeeper) {}

  void format(const spdlog::details::log_msg &, const std::tm &, spdlog::memory_buf_t &dest) override {
    // Calculate elapsed time
    auto elapsed = timeKeeper->getElapsedTime();

    // Format with 2 decimal places
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << elapsed;
    std::string time_str = ss.str();

    dest.append(time_str.data(), time_str.data() + time_str.size());
  }

  std::unique_ptr<spdlog::custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<ProcessTimeFlag>(timeKeeper);
  }

private:
  std::shared_ptr<TimeKeeper> timeKeeper;
};

#endif /* A3D46BB0_72BD_48CD_AC40_44503BFF61DB */
