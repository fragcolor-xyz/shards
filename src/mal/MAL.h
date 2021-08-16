#ifndef INCLUDE_MAL_H
#define INCLUDE_MAL_H

#include "Debug.h"
#include "RefCountedPtr.h"
#include "String.h"

#include <vector>

class malValue;
typedef RefCountedPtr<malValue> malValuePtr;
typedef std::vector<malValuePtr> malValueVec;
typedef malValueVec::iterator malValueIter;

class malEnv;
typedef RefCountedPtr<malEnv> malEnvPtr;

// step*.cpp
extern malValuePtr APPLY(malValuePtr op, malValueIter argsBegin,
                         malValueIter argsEnd);
extern malValuePtr READ(const String &input);
extern malValuePtr EVAL(malValuePtr ast, malEnvPtr env);
extern malValuePtr readline(const String &prompt);
extern String rep(const String &input, malEnvPtr env);

// Core.cpp
extern void installCore(malEnvPtr env);
extern void installCBCore(const malEnvPtr &env, const char *exePath,
                          const char *scriptPath);

// Reader.cpp
extern malValuePtr readStr(const String &input);

// Extras
extern void malinit(malEnvPtr env, const char *exePath, const char *scriptPath);
extern malValuePtr maleval(const char *str, malEnvPtr env);

#define MAL_CHECK(condition, ...)                                              \
  if (!(condition)) {                                                          \
    throw STRF(__VA_ARGS__);                                                   \
  } else {                                                                     \
  }

#define MAL_FAIL(...) MAL_CHECK(false, __VA_ARGS__)

extern int checkArgsIs(const char *name, int expected, int got,
                       malValuePtr val);
extern int checkArgsBetween(const char *name, int min, int max, int got,
                            malValuePtr val);
extern int checkArgsAtLeast(const char *name, int min, int got,
                            malValuePtr val);
extern int checkArgsEven(const char *name, int got, malValuePtr val);

#define CHECK_ARGS_IS(expected)                                                \
  checkArgsIs(name.c_str(), expected, std::distance(argsBegin, argsEnd),       \
              *argsBegin)

#define CHECK_ARGS_BETWEEN(min, max)                                           \
  checkArgsBetween(name.c_str(), min, max, std::distance(argsBegin, argsEnd),  \
                   *argsBegin)

#define CHECK_ARGS_AT_LEAST(expected)                                          \
  checkArgsAtLeast(name.c_str(), expected, std::distance(argsBegin, argsEnd),  \
                   *argsBegin)

#endif // INCLUDE_MAL_H
