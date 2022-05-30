/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include "eval.hpp"
#include "print.hpp"
#include <fstream>
#include <streambuf>
#include <string>

using namespace shards::edn::eval;

int main(int argc, const char **argv) {
  Program p;
  shards::edn::document doc;

  if (argc > 1) {
    std::ifstream f(argv[1]);
    std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto res = p.eval(str);

    switch (res.index()) {
    case 0: {
      auto v = std::get<SHVarValue>(res);
      std::cout << v.pr_str(doc) << "\n";
    } break;
    case 1: {
      auto v = std::get<Lambda>(res);
      std::cout << v.pr_str(doc) << "\n";
    } break;
    case 2: {
      auto v = std::get<Mesh>(res);
      std::cout << v.pr_str(doc) << "\n";
    } break;
    case 3: {
      auto v = std::get<shards::edn::form::Form>(res);
      std::cout << ::shards::edn::pr_str(doc, v) << "\n";
    } break;
    default:
      break;
    }

  } else {
    auto mesh = p.eval("(def Root (Mesh))");
    auto call = p.eval("(def callme (fn [] Root))");
    auto res = p.eval("(callme)");
  }
  return 0;
}
