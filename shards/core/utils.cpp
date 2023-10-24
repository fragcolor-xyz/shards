#include "utils.hpp"
namespace shards {
std::list<std::string> &getThreadNameStack() {
  thread_local std::list<std::string> stack;
  return stack;
}
} // namespace shards