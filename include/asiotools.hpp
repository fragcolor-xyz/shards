/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifndef CB_ASIOTOOLS_HPP
#define CB_ASIOTOOLS_HPP

// remember this has to go before Windows.h on windows
#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>
#include <iostream>
#include <thread>

namespace chainblocks {
namespace Asio {
template <typename T> class IOPool {
private:
  boost::lockfree::queue<T *> _empty_queue{16};

public:
  ~IOPool() {
    // drain and cleanup queues
    while (!_empty_queue.empty()) {
      T *msg;
      if (_empty_queue.pop(msg)) {
        delete msg;
      }
    }
  }

  T *borrow() {
    T *o;
    if (!_empty_queue.empty()) {
      _empty_queue.pop(o);
    } else {
      o = new T();
    }
    return o;
  }

  void forfeit(T *o) { _empty_queue.push(o); }
};

// DEPRECATED/NOT USEFUL, just call .poll() on io_context
class IOContext {
private:
  boost::asio::io_context _io_context;
  std::thread _io_thread;
  bool _running = true;

public:
  IOContext() {
    _io_thread = std::thread([this] {
      while (_running) {
        // Force run to run even without work
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
            g = boost::asio::make_work_guard(_io_context);
        try {
          _io_context.run();
        } catch (std::exception &e) {
          std::cerr << "Boost asio context run failed with exception: "
                    << e.what() << "\n";
        }
      }
    });
  }

  ~IOContext() {
    _running = false;

    // defer all in the context or we will crash!
    boost::asio::post(_io_context, [this]() {
      // allow end/thread exit
      _io_context.stop();
    });

    if (_io_thread.joinable()) {
      // wait thread exit
      _io_thread.join();
    }
  }

  boost::asio::io_context &operator()() { return _io_context; }
};
}; // namespace Asio
}; // namespace chainblocks

#endif
