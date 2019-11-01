/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#pragma once

#include "blocks/shared.hpp"
#include "runtime.hpp"
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdlib>

using namespace chainblocks;

namespace Desktop {
constexpr uint32_t windowCC = 'hwnd';
static TypeInfo windowType = TypeInfo::Object(FragCC, windowCC);
static TypesInfo windowInfo = TypesInfo(windowType);

template <typename T> class WindowBase {
public:
  void cleanup() {
    // reset to default
    // force finding it again next run
    _window = WindowDefault();
  }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::noneInfo); }

  static CBParametersInfo parameters() {
    return CBParametersInfo(windowParams);
  }

  virtual void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _winName = value.payload.stringValue;
      break;
    case 1:
      _winClass = value.payload.stringValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    auto res = CBVar();
    switch (index) {
    case 0:
      return Var(_winName);
    case 1:
      return Var(_winClass);
    default:
      break;
    }
    return res;
  }

protected:
  static inline ParamsInfo windowParams = ParamsInfo(
      ParamsInfo::Param("Title", "The title of the window to look for.",
                        CBTypesInfo(SharedTypes::strInfo)),
      ParamsInfo::Param("Class",
                        "An optional and platform dependent window class.",
                        CBTypesInfo(SharedTypes::strInfo)));

  static T WindowDefault();
  std::string _winName;
  std::string _winClass;
  T _window;
};

struct ActiveBase {
  static CBTypesInfo inputTypes() { return CBTypesInfo(windowInfo); }
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::boolInfo);
  }
};

struct PIDBase {
  static CBTypesInfo inputTypes() { return CBTypesInfo(windowInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::intInfo); }
};

struct WinOpBase {
  static CBTypesInfo inputTypes() { return CBTypesInfo(windowInfo); }
  static CBTypesInfo outputTypes() { return CBTypesInfo(windowInfo); }
};

struct SizeBase {
  static CBTypesInfo inputTypes() { return CBTypesInfo(windowInfo); }
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::int2Info);
  }
};

struct ResizeWindowBase : public WinOpBase {
  static inline ParamsInfo sizeParams =
      ParamsInfo(ParamsInfo::Param("Width", "The desired width.",
                                   CBTypesInfo(SharedTypes::intInfo)),
                 ParamsInfo::Param("Height", "The desired height.",
                                   CBTypesInfo(SharedTypes::intInfo)));

  int _width;
  int _height;

  static CBParametersInfo parameters() { return CBParametersInfo(sizeParams); }

  virtual void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _width = value.payload.intValue;
      break;
    case 1:
      _height = value.payload.intValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    auto res = CBVar();
    switch (index) {
    case 0:
      return Var(_width);
    case 1:
      return Var(_height);
    default:
      break;
    }
    return res;
  }
};

typedef boost::interprocess::allocator<
    char, boost::interprocess::managed_shared_memory::segment_manager>
    ShmemAllocator;
typedef boost::interprocess::basic_string<char, std::char_traits<char>,
                                          ShmemAllocator>
    SharedString;

struct InjectHookBase : public WinOpBase {
  static inline boost::interprocess::managed_shared_memory *SharedMemory =
      nullptr;
  static void ensureSharedMemory() {
    std::string memName;
#ifdef _WIN32
    memName = getenv("USERNAME");
#else
    memName = getenv("USER");
#endif
    memName += "_Desktop.InjectHook.Memory";
    // We do this cos we cannot create a static inline object by default,
    // cos will conflict with loaded dll
    if (!SharedMemory) {
      SharedMemory = new boost::interprocess::managed_shared_memory(
          boost::interprocess::open_or_create, memName.c_str(), 1048576);
    }
  }

  static std::string getRemoteCode(std::string &codeId) {
    std::string memName;
#ifdef _WIN32
    memName = getenv("USERNAME");
#else
    memName = getenv("USER");
#endif
    memName += "_Desktop.InjectHook.Memory";

    boost::interprocess::managed_shared_memory segment(
        boost::interprocess::open_only, memName.c_str());
    const ShmemAllocator alloc_inst(segment.get_segment_manager());
    auto code =
        segment.find_or_construct<SharedString>(codeId.c_str())(alloc_inst);
    if (code->size() > 0) {
      return std::string(code->c_str());
    } else {
      return "";
    }
  }

  static void setRemoteCode(std::string &codeId, std::string &codeStr) {
    ensureSharedMemory();
    const ShmemAllocator alloc_inst(SharedMemory->get_segment_manager());
    SharedMemory->find_or_construct<SharedString>(codeId.c_str())(
        codeStr.c_str(), alloc_inst);
  }

  static void deleteRemoteCode(std::string &codeId) {
    ensureSharedMemory();
    SharedMemory->destroy<SharedString>(codeId.c_str());
  }

  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Code", "The code to load and run once hooked.",
                        CBTypesInfo(SharedTypes::strInfo)));

  std::string _code;

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  virtual void setParam(int index, CBVar value) {
    _code = value.payload.stringValue;
  }

  CBVar getParam(int index) { return Var(_code); }
};

struct MoveWindowBase : public WinOpBase {
  static inline ParamsInfo posParams =
      ParamsInfo(ParamsInfo::Param("X", "The desired horizontal coordinates.",
                                   CBTypesInfo(SharedTypes::intInfo)),
                 ParamsInfo::Param("Y", "The desired vertical coordinates.",
                                   CBTypesInfo(SharedTypes::intInfo)));

  int _x;
  int _y;

  static CBParametersInfo parameters() { return CBParametersInfo(posParams); }

  virtual void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _x = value.payload.intValue;
      break;
    case 1:
      _y = value.payload.intValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    auto res = CBVar();
    switch (index) {
    case 0:
      return Var(_x);
    case 1:
      return Var(_y);
    default:
      break;
    }
    return res;
  }
};

struct SetTitleBase : public WinOpBase {
  static inline ParamsInfo windowParams = ParamsInfo(
      ParamsInfo::Param("Title", "The title of the window to look for.",
                        CBTypesInfo(SharedTypes::strInfo)));

  std::string _title;

  static CBParametersInfo parameters() {
    return CBParametersInfo(windowParams);
  }

  CBVar getParam(int index) { return Var(_title); }

  virtual void setParam(int index, CBVar value) {
    _title = value.payload.stringValue;
  }
};

struct WaitKeyEventBase {
  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::noneInfo); }
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::int2Info);
  }

  static const char *help() {
    return "### Pauses the chain and waits for keyboard events.\n#### The "
           "output of this block will be a Int2.\n * The first integer will be "
           "0 for Key down/push events and 1 for Key up/release events.\n * "
           "The second integer will the scancode of the key.\n";
  }
};

struct SendKeyEventBase {
  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Window",
                        "None or a window variable if we wish to send the "
                        "event only to a specific target window.",
                        CBTypesInfo(SharedTypes::ctxOrNoneInfo)));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::int2Info); }
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::int2Info);
  }

  static const char *help() {
    return "### Sends the input key event.\n#### The input of this block will "
           "be a Int2.\n * The first integer will be 0 for Key down/push "
           "events and 1 for Key up/release events.\n * The second integer "
           "will the scancode of the key.\n";
  }

  std::string _windowVarName;
  ExposedInfo _exposedInfo;

  CBExposedTypesInfo consumedVariables() {
    if (_windowVarName.size() == 0) {
      return nullptr;
    } else {
      return CBExposedTypesInfo(_exposedInfo);
    }
  }

  void setParam(int index, CBVar value) {
    if (value.valueType == None) {
      _windowVarName.clear();
    } else {
      _windowVarName = value.payload.stringValue;
      _exposedInfo = ExposedInfo(ExposedInfo::Variable(
          _windowVarName.c_str(), "The window to send events to.",
          CBTypeInfo(windowInfo)));
    }
  }

  CBVar getParam(int index) {
    if (_windowVarName.size() == 0) {
      return Empty;
    } else {
      return Var(_windowVarName);
    }
  }
};

struct MousePosBase {
  ContextableVar _window{};
  ExposedInfo _consuming{};

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Window", "None or a window variable we wish to use as relative origin.",
      CBTypesInfo(SharedTypes::ctxOrNoneInfo)));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::noneInfo); }
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::int2Info);
  }

  CBExposedTypesInfo consumedVariables() {
    if (_window.isVariable()) {
      _consuming = ExposedInfo(ExposedInfo::Variable(
          _window.variableName(), "The window.", CBTypeInfo(windowInfo)));
      return CBExposedTypesInfo(_consuming);
    } else {
      return nullptr;
    }
  }

  void setParam(int index, CBVar value) { _window.setParam(value); }

  CBVar getParam(int index) { return _window.getParam(); }
};
}; // namespace Desktop
