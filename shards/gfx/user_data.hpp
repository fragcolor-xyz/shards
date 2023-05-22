#ifndef GFX_USER_DATA
#define GFX_USER_DATA

#include <nameof.hpp>
#include <string>

#ifndef NDEBUG
#define GFX_USER_DATA_CHECKS 1
#endif

namespace gfx {
// Container for user data while validating expected type
/// <div rustbindgen opaque></div>
struct TypedUserData {
private:
  void *data;
#ifdef GFX_USER_DATA_CHECKS
  std::string typeName;
#endif

public:
  template <typename T> void set(T *inData) {
    data = inData;
#ifdef GFX_USER_DATA_CHECKS
    typeName = NAMEOF_TYPE(T);
#endif
  }

  template <typename T> T *get() {
#ifdef GFX_USER_DATA_CHECKS
    if (data) {
      if (typeName != NAMEOF_TYPE(T)) {
        assert(false);
        return nullptr;
      }
    }
#endif
    return (T *)data;
  }
};
} // namespace gfx

#endif // GFX_USER_DATA
