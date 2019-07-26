const
  stringsModulePath = currentSourcePath().splitPath().head
cppincludes(stringsModulePath & "")

emitc("#include \"utf8.h\"")

# MatchText
when true:
  type
    CBMatchText* = object
      results*: CBSeq
      stringPool*: StbSeq[GbString]
      regexStr*: GbString
      regex*: StdRegex
  
  template setup*(b: CBMatchText) = initSeq(b.results)
  template destroy*(b: CBMatchText) = freeSeq(b.results)
  template inputTypes*(b: CBMatchText): CBTypesInfo = ({ String }, false #[seq]#)
  template outputTypes*(b: CBMatchText): CBTypesInfo = ({ String }, true #[seq]#)
  template parameters*(b: CBMatchText): CBParametersInfo = 
    *@[
      (cs"Regex", ({ String }, false #[seq]#), false #[context]#)
    ]
  template setParam*(b: CBMatchText; index: int; val: CBVar) =
    b.regexStr = val.stringValue
    let regex = StdRegex.cppinit(b.regexStr.cstring)
    b.regex = regex
  template getParam*(b: CBMatchText; index: int): CBVar = b.regexStr
  template activate*(b: CBMatchText; context: CBContext; input: CBVar): CBVar =
    # TODO not super sure on memory allocations here!
    var
      res = RestartChain
      matches {.noinit.} = StdSmatch.cppinit()
      subject {.noinit.} = StdString.cppinit(input.stringValue.cstring)
    if invokeFunction("std::regex_match", subject, matches, b.regex).to(bool):
      let
        count = matches.size().to(int)
      
      b.stringPool.setLen(count) # Should allocate more str if needed or keep existing hopefully
      b.results.clear()

      for i in 0..<count:
        let
          subm = matches[i].to(StdSSubMatch)
          str = subm.str().c_str().to(cstring)
        
        b.stringPool[i].clear()
        b.stringPool[i] &= str
        b.results.push(b.stringPool[i])
      
      res = b.results
    res

  chainblock CBMatchText, "MatchText", "":
    withChain testMatchText:
      Const """baz.dat"""
      MatchText """([a-z]+)\.([a-z]+)"""
      Log()
    
    testMatchText.start()
    doAssert $testMatchText.get() == "[baz.dat, baz, dat]"
    testMatchText.start()
    doAssert $testMatchText.get() == "[baz.dat, baz, dat]"
    testMatchText.start()
    doAssert $testMatchText.get() == "[baz.dat, baz, dat]"

    destroy testMatchText

# ReplaceText
when true:
  type
    CBReplaceText* = object
      strCache*: GbString
      regexStr*: GbString
      regex*: StdRegex
      replacement*: GbString
  
  template inputTypes*(b: CBReplaceText): CBTypesInfo = ({ String }, false #[seq]#)
  template outputTypes*(b: CBReplaceText): CBTypesInfo = ({ String }, false #[seq]#)
  template parameters*(b: CBReplaceText): CBParametersInfo = 
    *@[
      (cs"Regex", ({ String }, false #[seq]#), false #[context]#),
      (cs"Replacement", ({ String }, false #[seq]#), false #[context]#)
    ]
  template setParam*(b: CBReplaceText; index: int; val: CBVar) =
    case index
    of 0:
      b.regexStr = val.stringValue
      let regex = StdRegex.cppinit(b.regexStr.cstring)
      b.regex = regex
    of 1:
      b.replacement = val.stringValue
    else:
      assert(false)
  template getParam*(b: CBReplaceText; index: int): CBVar =
    case index
    of 0: b.regexStr.CBVar
    of 1: b.replacement
    else: assert(false); Empty
  template activate*(b: CBReplaceText; context: CBContext; input: CBVar): CBVar =
    let replaced = invokeFunction("std::regex_replace", input.stringValue, b.regex, b.replacement.cstring).to(StdString)
    b.strCache.clear()
    b.strCache &= replaced.c_str().to(cstring)
    b.strCache

  chainblock CBReplaceText, "ReplaceText", "":
    withChain testReplaceText:
      Const """Quick brown fox"""
      ReplaceText:
        Regex: """a|e|i|o|u"""
        Replacement: "[$&]"
      Log()
    
    testReplaceText.start()
    doAssert $testReplaceText.get() == "Q[u][i]ck br[o]wn f[o]x"
    testReplaceText.start()
    doAssert $testReplaceText.get() == "Q[u][i]ck br[o]wn f[o]x"
    testReplaceText.start()
    doAssert $testReplaceText.get() == "Q[u][i]ck br[o]wn f[o]x"

    destroy testReplaceText

# ToUppercase
when true:
  type
    CBToUppercase* = object
      strCache*: GbString
  
  template inputTypes*(b: CBToUppercase): CBTypesInfo = ({ String }, false #[seq]#)
  template outputTypes*(b: CBToUppercase): CBTypesInfo = ({ String }, false #[seq]#)
  template activate*(b: CBToUppercase; context: CBContext; input: CBVar): CBVar =
    b.strCache.clear()
    b.strCache &= input.stringValue.cstring
    global.utf8upr(b.strCache.cstring).to(void)
    b.strCache

  chainblock CBToUppercase, "ToUppercase", "":
    withChain testToUppercase:
      Const "Quick brown fox"
      ToUppercase
      Log()
    
    testToUppercase.start()
    doAssert $testToUppercase.get() == "QUICK BROWN FOX"
    testToUppercase.start()
    doAssert $testToUppercase.get() == "QUICK BROWN FOX"
    testToUppercase.start()
    doAssert $testToUppercase.get() == "QUICK BROWN FOX"

    destroy testToUppercase

# ToLowercase
when true:
  type
    CBToLowercase* = object
      strCache*: GbString
  
  template inputTypes*(b: CBToLowercase): CBTypesInfo = ({ String }, false #[seq]#)
  template outputTypes*(b: CBToLowercase): CBTypesInfo = ({ String }, false #[seq]#)
  template activate*(b: CBToLowercase; context: CBContext; input: CBVar): CBVar =
    b.strCache.clear()
    b.strCache &= input.stringValue.cstring
    global.utf8lwr(b.strCache.cstring).to(void)
    b.strCache

  chainblock CBToLowercase, "ToLowercase", "":
    withChain testToUppercase:
      Const "QUICK BROWN FOX"
      ToLowercase
      Log()
    
    testToUppercase.start()
    doAssert $testToUppercase.get() == "quick brown fox"
    testToUppercase.start()
    doAssert $testToUppercase.get() == "quick brown fox"
    testToUppercase.start()
    doAssert $testToUppercase.get() == "quick brown fox"

    destroy testToUppercase
