/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright © 2019 Fragcolor Pte. Ltd. */
/* Copyright (C) 2015 Joel Martin <github@martintribe.org> */

#include "Environment.h"
#include "MAL.h"
#include "StaticList.h"
#include "Types.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

static String printValues(malValueIter begin, malValueIter end,
                          const String &sep, bool readably);

static StaticList<malBuiltIn *> handlers;

#define ARG(type, name) type *name = VALUE_CAST(type, *argsBegin++)

#define FUNCNAME(uniq) builtIn##uniq
#define HRECNAME(uniq) handler##uniq
#define BUILTIN_DEF(uniq, symbol)                                              \
  static malBuiltIn::ApplyFunc FUNCNAME(uniq);                                 \
  static StaticList<malBuiltIn *>::Node HRECNAME(uniq)(                        \
      handlers, new malBuiltIn(symbol, FUNCNAME(uniq)));                       \
  malValuePtr FUNCNAME(uniq)(const String &name, malValueIter argsBegin,       \
                             malValueIter argsEnd)

#define BUILTIN(symbol) BUILTIN_DEF(__LINE__, symbol)

#define BUILTIN_ISA(symbol, type)                                              \
  BUILTIN(symbol) {                                                            \
    CHECK_ARGS_IS(1);                                                          \
    return mal::boolean(DYNAMIC_CAST(type, *argsBegin));                       \
  }

#define BUILTIN_IS(op, constant)                                               \
  BUILTIN(op) {                                                                \
    CHECK_ARGS_IS(1);                                                          \
    return mal::boolean(*argsBegin == mal::constant());                        \
  }

#define BUILTIN_INTOP(op, checkDivByZero)                                      \
  BUILTIN(#op) {                                                               \
    CHECK_ARGS_IS(2);                                                          \
    ARG(malNumber, lhs);                                                       \
    ARG(malNumber, rhs);                                                       \
    if (checkDivByZero) {                                                      \
      MAL_CHECK(rhs->value() != 0, "Division by zero");                        \
    }                                                                          \
    return mal::number(lhs->value() op rhs->value(),                           \
                       lhs->isInteger() && rhs->isInteger());                  \
  }

BUILTIN_ISA("atom?", malAtom);
BUILTIN_ISA("keyword?", malKeyword);
BUILTIN_ISA("list?", malList);
BUILTIN_ISA("map?", malHash);
BUILTIN_ISA("number?", malNumber);
BUILTIN_ISA("sequential?", malSequence);
BUILTIN_ISA("string?", malString);
BUILTIN_ISA("symbol?", malSymbol);
BUILTIN_ISA("vector?", malVector);

BUILTIN_INTOP(+, false);
BUILTIN_INTOP(/, true);
BUILTIN_INTOP(*, false);

BUILTIN_IS("true?", trueValue);
BUILTIN_IS("false?", falseValue);
BUILTIN_IS("nil?", nilValue);

BUILTIN("-") {
  int argCount = CHECK_ARGS_BETWEEN(1, 2);
  ARG(malNumber, lhs);
  if (argCount == 1) {
    return mal::number(-lhs->value(), lhs->isInteger());
  }

  ARG(malNumber, rhs);
  return mal::number(lhs->value() - rhs->value(),
                     lhs->isInteger() && rhs->isInteger());
}

BUILTIN("%") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, lhs);
  ARG(malNumber, rhs);

  int64_t l = static_cast<int64_t>(lhs->value());
  int64_t r = static_cast<int64_t>(rhs->value());
  MAL_CHECK(r != 0, "Division by zero");
  return mal::number(l % r, true);
}

BUILTIN("<=") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, lhs);
  ARG(malNumber, rhs);

  return mal::boolean(lhs->value() <= rhs->value());
}

BUILTIN(">=") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, lhs);
  ARG(malNumber, rhs);

  return mal::boolean(lhs->value() >= rhs->value());
}

BUILTIN("<") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, lhs);
  ARG(malNumber, rhs);

  return mal::boolean(lhs->value() < rhs->value());
}

BUILTIN(">") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, lhs);
  ARG(malNumber, rhs);

  return mal::boolean(lhs->value() > rhs->value());
}

BUILTIN("=") {
  CHECK_ARGS_IS(2);
  const malValue *lhs = (*argsBegin++).ptr();
  const malValue *rhs = (*argsBegin++).ptr();

  return mal::boolean(lhs->isEqualTo(rhs));
}

BUILTIN("apply") {
  CHECK_ARGS_AT_LEAST(2);
  malValuePtr op = *argsBegin++; // this gets checked in APPLY

  // Copy the first N-1 arguments in.
  malValueVec args(argsBegin, argsEnd - 1);

  // Then append the argument as a list.
  const malSequence *lastArg = VALUE_CAST(malSequence, *(argsEnd - 1));
  for (int i = 0; i < lastArg->count(); i++) {
    args.push_back(lastArg->item(i));
  }

  return APPLY(op, args.begin(), args.end());
}

BUILTIN("assoc") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malHash, hash);

  return hash->assoc(argsBegin, argsEnd);
}

BUILTIN("atom") {
  CHECK_ARGS_IS(1);

  return mal::atom(*argsBegin);
}

BUILTIN("concat") {
  int count = 0;
  for (auto it = argsBegin; it != argsEnd; ++it) {
    const malSequence *seq = VALUE_CAST(malSequence, *it);
    count += seq->count();
  }

  malValueVec *items = new malValueVec(count);
  int offset = 0;
  for (auto it = argsBegin; it != argsEnd; ++it) {
    const malSequence *seq = STATIC_CAST(malSequence, *it);
    std::copy(seq->begin(), seq->end(), items->begin() + offset);
    offset += seq->count();
  }

  return mal::list(items);
}

BUILTIN("conj") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malSequence, seq);

  return seq->conj(argsBegin, argsEnd);
}

BUILTIN("cons") {
  CHECK_ARGS_IS(2);
  malValuePtr first = *argsBegin++;
  if (*argsBegin == mal::nilValue()) {
    malValueVec *items = new malValueVec(1);
    items->at(0) = first;
    return mal::list(items);
  }
  ARG(malSequence, rest);

  malValueVec *items = new malValueVec(1 + rest->count());
  items->at(0) = first;
  std::copy(rest->begin(), rest->end(), items->begin() + 1);

  return mal::list(items);
}

BUILTIN("contains?") {
  CHECK_ARGS_IS(2);
  if (*argsBegin == mal::nilValue()) {
    return *argsBegin;
  }
  ARG(malHash, hash);
  return mal::boolean(hash->contains(*argsBegin));
}

BUILTIN("count") {
  CHECK_ARGS_IS(1);
  if (*argsBegin == mal::nilValue()) {
    return mal::number(0, true);
  }

  ARG(malSequence, seq);
  return mal::number(seq->count(), true);
}

BUILTIN("deref") {
  CHECK_ARGS_IS(1);
  ARG(malAtom, atom);

  return atom->deref();
}

BUILTIN("dissoc") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malHash, hash);

  return hash->dissoc(argsBegin, argsEnd);
}

BUILTIN("empty?") {
  CHECK_ARGS_IS(1);
  ARG(malSequence, seq);

  return mal::boolean(seq->isEmpty());
}

BUILTIN("eval") {
  CHECK_ARGS_IS(1);
  return EVAL(*argsBegin, NULL);
}

template <typename I>
std::string n2hexstr(I w, size_t hex_len = sizeof(I) << 1) {
  static const char *digits = "0123456789ABCDEF";
  std::string rc(hex_len, '0');
  for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
    rc[i] = digits[(w >> j) & 0x0f];
  return rc;
}

BUILTIN("hex") {
  CHECK_ARGS_IS(1);
  ARG(malNumber, num);
  if (num->isInteger()) {
    return mal::string("0x" + n2hexstr(static_cast<uint64_t>(num->value())));
  } else {
    auto dval = num->value();
    return mal::string("0x" + n2hexstr(*reinterpret_cast<uint64_t *>(&dval)));
  }
}

BUILTIN("first") {
  CHECK_ARGS_IS(1);
  if (*argsBegin == mal::nilValue()) {
    return mal::nilValue();
  }
  ARG(malSequence, seq);
  return seq->first();
}

BUILTIN("fn?") {
  CHECK_ARGS_IS(1);
  malValuePtr arg = *argsBegin++;

  // Lambdas are functions, unless they're macros.
  if (const malLambda *lambda = DYNAMIC_CAST(malLambda, arg)) {
    return mal::boolean(!lambda->isMacro());
  }
  // Builtins are functions.
  return mal::boolean(DYNAMIC_CAST(malBuiltIn, arg));
}

BUILTIN("get") {
  CHECK_ARGS_IS(2);
  if (*argsBegin == mal::nilValue()) {
    return *argsBegin;
  }
  ARG(malHash, hash);
  return hash->get(*argsBegin);
}

BUILTIN("hash-map") { return mal::hash(argsBegin, argsEnd, true); }

BUILTIN("keys") {
  CHECK_ARGS_IS(1);
  ARG(malHash, hash);
  return hash->keys();
}

BUILTIN("keyword") {
  CHECK_ARGS_IS(1);
  ARG(malString, token);
  return mal::keyword(":" + token->value());
}

BUILTIN("list") { return mal::list(argsBegin, argsEnd); }

BUILTIN("macro?") {
  CHECK_ARGS_IS(1);

  // Macros are implemented as lambdas, with a special flag.
  const malLambda *lambda = DYNAMIC_CAST(malLambda, *argsBegin);
  return mal::boolean((lambda != NULL) && lambda->isMacro());
}

BUILTIN("map") {
  CHECK_ARGS_IS(2);
  malValuePtr op = *argsBegin++; // this gets checked in APPLY
  ARG(malSequence, source);

  const int length = source->count();
  malValueVec *items = new malValueVec(length);
  auto it = source->begin();
  for (int i = 0; i < length; i++) {
    items->at(i) = APPLY(op, it + i, it + i + 1);
  }

  return mal::list(items);
}

BUILTIN("remove") {
  CHECK_ARGS_IS(2);
  malValuePtr op = *argsBegin++; // this gets checked in APPLY
  ARG(malSequence, source);

  const int length = source->count();
  malValueVec *items = new malValueVec();
  auto it = source->begin();
  for (int i = 0; i < length; i++) {
    auto item = it + i;
    auto bres = APPLY(op, item, item + 1);
    if (bres == mal::falseValue())
      items->emplace_back(*item);
  }

  return mal::list(items);
}

BUILTIN("meta") {
  CHECK_ARGS_IS(1);
  malValuePtr obj = *argsBegin++;

  return obj->meta();
}

BUILTIN("nth") {
  CHECK_ARGS_IS(2);
  ARG(malSequence, seq);
  ARG(malNumber, index);

  int i = index->value();
  MAL_CHECK(i >= 0 && i < seq->count(), "Index out of range");

  return seq->item(i);
}

BUILTIN("reverse") {
  CHECK_ARGS_IS(1);
  ARG(malSequence, seq);

  auto rseq = new malList(std::begin(*seq), std::end(*seq));

  std::reverse(std::begin(*rseq), std::end(*rseq));

  return malValuePtr(rseq);
}

BUILTIN("pr-str") {
  return mal::string(printValues(argsBegin, argsEnd, " ", true));
}

BUILTIN("println") {
  std::cout << printValues(argsBegin, argsEnd, " ", false) << "\n";
  return mal::nilValue();
}

BUILTIN("prn") {
  std::cout << printValues(argsBegin, argsEnd, " ", true) << "\n";
  return mal::nilValue();
}

BUILTIN("read-string") {
  CHECK_ARGS_IS(1);
  ARG(malString, str);

  return readStr(str->value());
}

#ifndef NO_MAL_MAIN

BUILTIN("readline") {
  CHECK_ARGS_IS(1);
  ARG(malString, str);

  return readline(str->value());
}

#endif

BUILTIN("reset!") {
  CHECK_ARGS_IS(2);
  ARG(malAtom, atom);
  return atom->reset(*argsBegin);
}

BUILTIN("rest") {
  CHECK_ARGS_IS(1);
  if (*argsBegin == mal::nilValue()) {
    return mal::list(new malValueVec(0));
  }
  ARG(malSequence, seq);
  return seq->rest();
}

BUILTIN("seq") {
  CHECK_ARGS_IS(1);
  malValuePtr arg = *argsBegin++;
  if (arg == mal::nilValue()) {
    return mal::nilValue();
  }
  if (const malSequence *seq = DYNAMIC_CAST(malSequence, arg)) {
    return seq->isEmpty() ? mal::nilValue()
                          : mal::list(seq->begin(), seq->end());
  }
  if (const malString *strVal = DYNAMIC_CAST(malString, arg)) {
    const String str = strVal->value();
    int length = str.length();
    if (length == 0)
      return mal::nilValue();

    malValueVec *items = new malValueVec(length);
    for (int i = 0; i < length; i++) {
      (*items)[i] = mal::string(str.substr(i, 1));
    }
    return mal::list(items);
  }
  MAL_FAIL("%s is not a string or sequence", arg->print(true).c_str());
}

BUILTIN("slurp") {
  CHECK_ARGS_IS(1);
  ARG(malString, filename);

  auto filepath = std::filesystem::path(filename->value());

  std::ifstream file(filepath.c_str(), std::ios::binary);
  MAL_CHECK(!file.fail(), "Cannot open %s", filename->value().c_str());

  String data;
  data.assign(std::istreambuf_iterator<char>(file), {});

  return mal::string(data);
}

BUILTIN("str") {
  return mal::string(printValues(argsBegin, argsEnd, "", false));
}

BUILTIN("swap!") {
  CHECK_ARGS_AT_LEAST(2);
  ARG(malAtom, atom);

  malValuePtr op = *argsBegin++; // this gets checked in APPLY

  malValueVec args(1 + argsEnd - argsBegin);
  args[0] = atom->deref();
  std::copy(argsBegin, argsEnd, args.begin() + 1);

  malValuePtr value = APPLY(op, args.begin(), args.end());
  return atom->reset(value);
}

BUILTIN("symbol") {
  CHECK_ARGS_IS(1);
  ARG(malString, token);
  return mal::symbol(token->value());
}

BUILTIN("throw") {
  CHECK_ARGS_IS(1);
  throw *argsBegin;
}

BUILTIN("time-ms") {
  CHECK_ARGS_IS(0);

  using namespace std::chrono;
  milliseconds ms = duration_cast<milliseconds>(
      high_resolution_clock::now().time_since_epoch());

  return mal::number(ms.count(), true);
}

BUILTIN("vals") {
  CHECK_ARGS_IS(1);
  ARG(malHash, hash);
  return hash->values();
}

BUILTIN("vector") { return mal::vector(argsBegin, argsEnd); }

BUILTIN("with-meta") {
  CHECK_ARGS_IS(2);
  malValuePtr obj = *argsBegin++;
  malValuePtr meta = *argsBegin++;
  return obj->withMeta(meta);
}

void installCore(malEnvPtr env) {
  for (auto it = handlers.begin(), end = handlers.end(); it != end; ++it) {
    malBuiltIn *handler = *it;
    env->set(handler->name(), handler);
  }
}

static String printValues(malValueIter begin, malValueIter end,
                          const String &sep, bool readably) {
  String out;

  if (begin != end) {
    out += (*begin)->print(readably);
    ++begin;
  }

  for (; begin != end; ++begin) {
    out += sep;
    out += (*begin)->print(readably);
  }

  return out;
}
