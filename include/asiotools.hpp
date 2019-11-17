/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_ASIOTOOLS_HPP
#define CB_ASIOTOOLS_HPP

// remember this has to go before Windows.h on windows
#include <boost/asio.hpp>
#include <iostream>
#include <thread>

namespace chainblocks {
namespace Asio {
class IOContext {
private:
  boost::asio::io_context _io_context;
  std::thread _io_thread;

public:
  IOContext() {
    _io_thread = std::thread([this] {
      // Force run to run even without work
      boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
          g = boost::asio::make_work_guard(_io_context);
      try {
        _io_context.run();
      } catch (...) {
        std::cerr << "Boost asio context run failed.\n";
      }
    });
  }

  ~IOContext() {
    // defer all in the context or we will crash!
    _io_context.post([this]() {
      // allow end/thread exit
      _io_context.stop();
    });
    // wait thread exit
    _io_thread.join();
  }

  boost::asio::io_context &operator()() { return _io_context; }
};
}; // namespace Asio
}; // namespace chainblocks

#endif
