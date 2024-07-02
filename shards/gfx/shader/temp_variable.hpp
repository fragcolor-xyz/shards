#ifndef ABF29E23_C6D2_4C90_9125_E5A84B399530
#define ABF29E23_C6D2_4C90_9125_E5A84B399530

#include <string>
#include <spdlog/fmt/fmt.h>

namespace gfx::shader {
struct TempVariableAllocator {
private:
  size_t counter{};
  std::string tempVariableName;
  std::string baseId;

public:
  TempVariableAllocator(std::string &&baseId = "_tmp") : baseId(std::move(baseId)) {}
  TempVariableAllocator(const char *baseId) : baseId(baseId) {}

  void reset() { counter = 0; }

  size_t stateGet() const { return counter; }
  void stateSet(size_t value) { counter = value; }
  void stateSet(TempVariableAllocator& other) { counter = other.counter; }

  const std::string &get(const std::string_view &hint = std::string_view()) {
    tempVariableName.clear();
    if (!hint.empty()) {
      fmt::format_to(std::back_inserter(tempVariableName), "{}_{}{}", baseId, hint, counter++);
    } else {
      fmt::format_to(std::back_inserter(tempVariableName), "{}{}", baseId, counter++);
    }
    return tempVariableName;
  }
};
} // namespace gfx::shader

#endif /* ABF29E23_C6D2_4C90_9125_E5A84B399530 */
