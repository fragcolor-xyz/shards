/* SPDX-License-Identifier: MPL-2.0 */
/* Copyright © 2019 Fragcolor Pte. Ltd. */
/* Copyright (C) 2015 Joel Martin <github@martintribe.org> */

#include "MAL.h"

#include "Environment.h"
#ifndef NO_MAL_MAIN
#include "ReadLine.h"
#endif
#include "Types.h"

#include <boost/filesystem.hpp>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>

namespace fs = boost::filesystem;

malValuePtr READ(const String &input);
String PRINT(malValuePtr ast);
static void installFunctions(malEnvPtr env);
//  Installs functions, macros and constants implemented in MAL.

static void makeArgv(malEnvPtr env, int argc, const char *argv[]);
static String safeRep(const String &input, malEnvPtr env, bool *failed = nullptr);
static malValuePtr quasiquote(malValuePtr obj);
static malValuePtr macroExpand(malValuePtr obj, malEnvPtr env);

#ifndef NO_MAL_MAIN
static ReadLine *s_readLine{nullptr};
#endif

static thread_local malEnvPtr currentEnv{};

void malinit(malEnvPtr env, const char *exePath, const char *scriptPath) {
  assert(env);
  currentEnv = env;
  installCore(env);
  installFunctions(env);
  installCBCore(env, exePath, scriptPath);
}

malValuePtr maleval(const char *str, malEnvPtr env) {
  assert(env);
  currentEnv = env;
  return EVAL(READ(str), env);
}

extern malEnvPtr malenv() { return currentEnv; }

int malmain(int argc, const char *argv[]) {
  malEnvPtr replEnv(new malEnv());

  // do the following before malinit

  auto cblAbsPath = fs::weakly_canonical(argv[0]);
  replEnv->set("*cbl*", mal::string(cblAbsPath.string()));

  auto exeDirPath = cblAbsPath.parent_path();
  auto scriptDirPath = exeDirPath;
  if (argc > 1) {
    fs::path scriptPath = fs::weakly_canonical(fs::path(argv[1]));
    scriptDirPath = scriptPath.parent_path();
  }

  malinit(replEnv, exeDirPath.string().c_str(), scriptDirPath.string().c_str());

  makeArgv(replEnv, argc - 2, argv + 2);
  bool failed = false;

  if (argc > 1) {
    if (strcmp(argv[1], "-e") == 0 && argc == 3) {
      String out = safeRep(argv[2], replEnv, &failed);
      std::cout << out << "\n";
    } else {
      auto scriptFilePath = fs::path(argv[1]);
      auto fileonly = scriptFilePath.filename().string();
      String filename = escape(fileonly);
      String out = safeRep(STRF("(load-file %s)", filename.c_str()), replEnv, &failed);
      if (out.length() > 0 && out != "nil")
        std::cout << out << "\n";
    }
  } else {
#ifndef NO_MAL_MAIN
    s_readLine = new ReadLine("./chainblocks-history.txt");
    String prompt = "user> ";
    String input;
    rep("(println (str \"Mal [\" *host-language* \"]\"))", replEnv);
    while (s_readLine->get(prompt, input)) {
      String out = safeRep(input, replEnv, &failed);
      if (out.length() > 0)
        std::cout << out << "\n";
    }
    delete s_readLine;
#endif
  }
  if (currentEnv == replEnv)
    currentEnv = nullptr;

  return failed ? -1 : 0;
}

#ifndef NO_MAL_MAIN

int main(int argc, const char *argv[]) { return malmain(argc, argv); }

#endif

static String safeRep(const String &input, malEnvPtr env, bool *failed) {
  try {
    return rep(input, env);
  } catch (malEmptyInputException &) {
    if (failed)
      *failed = true;
    return String();
  } catch (malValuePtr &mv) {
    if (failed)
      *failed = true;
    return "Error: " + mv->print(true) + " Line: " + std::to_string(mv->line);
  } catch (String &s) {
    if (failed)
      *failed = true;
    return "Error: " + s;
  } catch (std::exception &e) {
    if (failed)
      *failed = true;
    return std::string("Error: ") + e.what();
  };
}

static void makeArgv(malEnvPtr env, int argc, const char *argv[]) {
  malValueVec *args = new malValueVec();
  for (int i = 0; i < argc; i++) {
    args->push_back(mal::string(argv[i]));
  }
  env->set("*command-line-args*", mal::list(args));
}

String rep(const String &input, malEnvPtr env) { return PRINT(EVAL(READ(input), env)); }

malValuePtr READ(const String &input) { return readStr(input); }

malValuePtr EVAL(malValuePtr ast, malEnvPtr env) {
  if (!env) {
    env = currentEnv;
    assert(env);
  }
  // not quite sure, but for now lets not update currentenv here
  // limit it only to root entry points
  while (1) {
    const malList *list = DYNAMIC_CAST(malList, ast);
    if (!list || (list->count() == 0)) {
      return ast->eval(env);
    }

    ast = macroExpand(ast, env);
    list = DYNAMIC_CAST(malList, ast);
    if (!list || (list->count() == 0)) {
      return ast->eval(env);
    }

    // From here on down we are evaluating a non-empty list.
    // First handle the special forms.
    if (const malSymbol *symbol = DYNAMIC_CAST(malSymbol, list->item(0))) {
      String special = symbol->value();
      int argCount = list->count() - 1;

      if (special == "def!") {
        checkArgsIs("def!", 2, argCount, list->item(0));
        const malSymbol *id = VALUE_CAST(malSymbol, list->item(1));
        auto rootEnv = env->getRoot();
        auto e = rootEnv != nullptr ? rootEnv : env;
        return e->set(id->value(), EVAL(list->item(2), e));
      }

      if (special == "deflocal!") {
        checkArgsIs("deflocal!", 2, argCount, list->item(0));
        const malSymbol *id = VALUE_CAST(malSymbol, list->item(1));
        return env->set(id->value(), EVAL(list->item(2), env));
      }

      if (special == "defmacro!") {
        checkArgsIs("defmacro!", 2, argCount, list->item(0));

        const malSymbol *id = VALUE_CAST(malSymbol, list->item(1));
        malValuePtr body = EVAL(list->item(2), env);
        const malLambda *lambda = VALUE_CAST(malLambda, body);
        auto rootEnv = env->getRoot();
        auto e = rootEnv != nullptr ? rootEnv : env;
        return e->set(id->value(), mal::macro(*lambda));
      }

      if (special == "do") {
        checkArgsAtLeast("do", 1, argCount, list->item(0));

        for (int i = 1; i < argCount; i++) {
          EVAL(list->item(i), env);
        }
        ast = list->item(argCount);
        continue; // TCO
      }

      if (special == "fn*") {
        checkArgsIs("fn*", 2, argCount, list->item(0));

        const malSequence *bindings = VALUE_CAST(malSequence, list->item(1));
        StringVec params;
        for (int i = 0; i < bindings->count(); i++) {
          const malSymbol *sym = VALUE_CAST(malSymbol, bindings->item(i));
          params.push_back(sym->value());
        }

        return mal::lambda(params, list->item(2), env);
      }

      if (special == "if") {
        checkArgsBetween("if", 2, 3, argCount, list->item(0));

        bool isTrue = EVAL(list->item(1), env)->isTrue();
        if (!isTrue && (argCount == 2)) {
          return mal::nilValue();
        }
        ast = list->item(isTrue ? 2 : 3);
        continue; // TCO
      }

      if (special == "let*") {
        checkArgsIs("let*", 2, argCount, list->item(0));
        const malSequence *bindings = VALUE_CAST(malSequence, list->item(1));
        int count = checkArgsEven("let*", bindings->count(), list->item(0));
        malEnvPtr inner(new malEnv(env));
        for (int i = 0; i < count; i += 2) {
          const malSymbol *var = VALUE_CAST(malSymbol, bindings->item(i));
          inner->set(var->value(), EVAL(bindings->item(i + 1), inner));
        }
        ast = list->item(2);
        env = inner;
        continue; // TCO
      }

      if (special == "macroexpand") {
        checkArgsIs("macroexpand", 1, argCount, list->item(0));
        return macroExpand(list->item(1), env);
      }

      if (special == "quasiquote") {
        checkArgsIs("quasiquote", 1, argCount, list->item(0));
        ast = quasiquote(list->item(1));
        continue; // TCO
      }

      if (special == "quote") {
        checkArgsIs("quote", 1, argCount, list->item(0));
        return list->item(1);
      }

      if (special == "try*") {
        malValuePtr tryBody = list->item(1);

        if (argCount == 1) {
          ast = EVAL(tryBody, env);
          continue; // TCO
        }
        checkArgsIs("try*", 2, argCount, list->item(0));
        const malList *catchBlock = VALUE_CAST(malList, list->item(2));

        checkArgsIs("catch*", 2, catchBlock->count() - 1, list->item(2));
        MAL_CHECK(VALUE_CAST(malSymbol, catchBlock->item(0))->value() == "catch*", "catch block must begin with catch*");

        // We don't need excSym at this scope, but we want to check
        // that the catch block is valid always, not just in case of
        // an exception.
        const malSymbol *excSym = VALUE_CAST(malSymbol, catchBlock->item(1));

        malValuePtr excVal;

        try {
          ast = EVAL(tryBody, env);
        } catch (String &s) {
          excVal = mal::string(s);
        } catch (malEmptyInputException &) {
          // Not an error, continue as if we got nil
          ast = mal::nilValue();
        } catch (malValuePtr &o) {
          excVal = o;
        };

        if (excVal) {
          // we got some exception
          env = malEnvPtr(new malEnv(env));
          env->set(excSym->value(), excVal);
          ast = catchBlock->item(2);
        }
        continue; // TCO
      }
    }

    // Now we're left with the case of a regular list to be evaluated.
    std::unique_ptr<malValueVec> items(list->evalItems(env));
    malValuePtr op = items->at(0);
    if (const malLambda *lambda = DYNAMIC_CAST(malLambda, op)) {
      ast = lambda->getBody();
      env = lambda->makeEnv(items->begin() + 1, items->end());
      continue; // TCO
    } else {
      return APPLY(op, items->begin() + 1, items->end());
    }
  }
}

String PRINT(malValuePtr ast) { return ast->print(true); }

malValuePtr APPLY(malValuePtr op, malValueIter argsBegin, malValueIter argsEnd) {
  const malApplicable *handler = DYNAMIC_CAST(malApplicable, op);
  MAL_CHECK(handler != NULL, "\"%s\" is not applicable, line: %u", op->print(true).c_str(), op->line);

  return handler->apply(argsBegin, argsEnd);
}

static bool isSymbol(malValuePtr obj, const String &text) {
  const malSymbol *sym = DYNAMIC_CAST(malSymbol, obj);
  return sym && (sym->value() == text);
}

static const malSequence *isPair(malValuePtr obj) {
  const malSequence *list = DYNAMIC_CAST(malSequence, obj);
  return list && !list->isEmpty() ? list : NULL;
}

static malValuePtr quasiquote(malValuePtr obj) {
  const malSequence *seq = isPair(obj);
  if (!seq) {
    return mal::list(mal::symbol("quote"), obj);
  }

  if (isSymbol(seq->item(0), "unquote")) {
    // (qq (uq form)) -> form
    checkArgsIs("unquote", 1, seq->count() - 1, obj);
    return seq->item(1);
  }

  const malSequence *innerSeq = isPair(seq->item(0));
  if (innerSeq && isSymbol(innerSeq->item(0), "splice-unquote")) {
    checkArgsIs("splice-unquote", 1, innerSeq->count() - 1, obj);
    // (qq (sq '(a b c))) -> a b c
    return mal::list(mal::symbol("concat"), innerSeq->item(1), quasiquote(seq->rest()));
  } else {
    // (qq (a b c)) -> (list (qq a) (qq b) (qq c))
    // (qq xs     ) -> (cons (qq (car xs)) (qq (cdr xs)))
    return mal::list(mal::symbol("cons"), quasiquote(seq->first()), quasiquote(seq->rest()));
  }
}

static const malLambda *isMacroApplication(malValuePtr obj, malEnvPtr env) {
  if (const malSequence *seq = isPair(obj)) {
    if (malSymbol *sym = DYNAMIC_CAST(malSymbol, seq->first())) {
      if (malEnvPtr symEnv = env->find(sym->value())) {
        malValuePtr value = sym->eval(symEnv);
        if (malLambda *lambda = DYNAMIC_CAST(malLambda, value)) {
          return lambda->isMacro() ? lambda : NULL;
        }
      }
    }
  }
  return NULL;
}

static malValuePtr macroExpand(malValuePtr obj, malEnvPtr env) {
  while (const malLambda *macro = isMacroApplication(obj, env)) {
    const malSequence *seq = STATIC_CAST(malSequence, obj);
    obj = macro->apply(seq->begin() + 1, seq->end());
  }
  return obj;
}

static const char *malFunctionTable[] = {
    "(defmacro! cond (fn* (& xs) (if (> (count xs) 0) (list 'if (first xs) (if "
    "(> (count xs) 1) (nth xs 1) (throw \"odd number of forms to cond\")) "
    "(cons 'cond (rest (rest xs)))))))",
    "(def! not (fn* (cond) (if cond false true)))",
    "(def! load-file (fn* (filename) \
        (eval (read-string (str \"(do \" (slurp filename) \"\nnil)\")))))",
    "(def! *host-language* \"C++\")",
};

static void installFunctions(malEnvPtr env) {
  for (auto &function : malFunctionTable) {
    rep(function, env);
  }
}

#ifndef NO_MAL_MAIN

// Added to keep the linker happy at step A
malValuePtr readline(const String &prompt) {
  String input;
  if (s_readLine->get(prompt, input)) {
    return mal::string(input);
  }
  return mal::nilValue();
}

#endif
