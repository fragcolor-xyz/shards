# MatchText
when true:
  type
    CBMatchText* = object
      results*: CBSeq
      stringPool*: seq[CBString]
      regexStr*: string
      regex*: StdRegex
  
  template setup*(b: CBMatchText) = initSeq(b.results)
  template destroy*(b: CBMatchText) =
    freeSeq(b.results, true)
    for s in b.stringPool: freeString(s)
  template inputTypes*(b: CBMatchText): CBTypesInfo = ({ String }, false #[seq]#)
  template outputTypes*(b: CBMatchText): CBTypesInfo = ({ String }, true #[seq]#)
  template parameters*(b: CBMatchText): CBParametersInfo = 
    @[
      ("Regex", ({ String }, false #[seq]#), false #[context]#)
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
        cacheLen = b.results.len
      
      for i in 0..<cacheLen:
        b.stringPool.add(b.results[i].stringValue)
      b.results.clear()

      for i in 0..<count:
        let
          subm = matches[i].to(StdSSubMatch)
          str = subm.str().c_str().to(cstring)
        
        if b.stringPool.len > 0:
          var s = b.stringPool.pop()
          s = setString(s, str)
          b.results.push(s)
        else:
          var s = makeString(str)
          b.results.push(s)
      
      res = b.results
    res

  chainblock CBMatchText, "MatchText", "":
    withChain testMatchText:
      Const """baz.dat"""
      MatchText """([a-z]+)\.([a-z]+)"""
      Log()
    
    testMatchText.start()
    doAssert $testMatchText.stop() == "baz.dat, baz, dat"
    testMatchText.start()
    doAssert $testMatchText.stop() == "baz.dat, baz, dat"
    testMatchText.start()
    doAssert $testMatchText.stop() == "baz.dat, baz, dat"

    destroy testMatchText

# ReplaceText
when true:
  type
    CBReplaceText* = object
      strCache*: CBstring
      regexStr*: string
      regex*: StdRegex
      replacement*: string
  
  template setup*(b: CBReplaceText) = b.strCache = makeString("")
  template destroy*(b: CBReplaceText) = freeString(b.strCache)
  template inputTypes*(b: CBReplaceText): CBTypesInfo = ({ String }, false #[seq]#)
  template outputTypes*(b: CBReplaceText): CBTypesInfo = ({ String }, false #[seq]#)
  template parameters*(b: CBReplaceText): CBParametersInfo = 
    @[
      ("Regex", ({ String }, false #[seq]#), false #[context]#),
      ("Replacement", ({ String }, false #[seq]#), false #[context]#)
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
    # The following has an allocation per activation! TODO
    let replaced = invokeFunction("std::regex_replace", input.stringValue, b.regex, b.replacement.cstring).to(StdString)
    b.strCache = setString(b.strCache, replaced.c_str().to(cstring))
    b.strCache.CBVar

  chainblock CBReplaceText, "ReplaceText", "":
    withChain testReplaceText:
      Const """Quick brown fox"""
      ReplaceText:
        Regex: """a|e|i|o|u"""
        Replacement: "[$&]"
      Log()
    
    testReplaceText.start()
    doAssert $testReplaceText.stop() == "Q[u][i]ck br[o]wn f[o]x"
    testReplaceText.start()
    doAssert $testReplaceText.stop() == "Q[u][i]ck br[o]wn f[o]x"
    testReplaceText.start()
    doAssert $testReplaceText.stop() == "Q[u][i]ck br[o]wn f[o]x"

    destroy testReplaceText

# ToUppercase
when true:
  type
    CBToUppercase* = object
      strCache*: string
  
  template inputTypes*(b: CBToUppercase): CBTypesInfo = ({ String }, false #[seq]#)
  template outputTypes*(b: CBToUppercase): CBTypesInfo = ({ String }, false #[seq]#)
  template activate*(b: CBToUppercase; context: CBContext; input: CBVar): CBVar =
    b.strCache = input.stringValue.string.toUpper
    b.strCache

  chainblock CBToUppercase, "ToUppercase", "":
    withChain testToUppercase:
      Const "Quick brown fox"
      ToUppercase
      Log()
    
    testToUppercase.start()
    doAssert $testToUppercase.stop() == "QUICK BROWN FOX"
    testToUppercase.start()
    doAssert $testToUppercase.stop() == "QUICK BROWN FOX"
    testToUppercase.start()
    doAssert $testToUppercase.stop() == "QUICK BROWN FOX"

    destroy testToUppercase
