# MatchText
when true:
  type
    CBMatchText* = object
      results*: CBSeq
      regexStr*: string
      regex*: StdRegex
  
  template setup*(b: CBMatchText) = initSeq(b.results)
  template destroy*(b: CBMatchText) = freeSeq(b.results, true)
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
    var
      res = RestartChain
      matches {.noinit.} = StdSmatch.cppinit()
      subject {.noinit.} = StdString.cppinit(input.stringValue.cstring)
    if invokeFunction("std::regex_match", subject, matches, b.regex).to(bool):
      b.results.clear()
      for i in 0..<matches.size().to(int):
        let
          subm = matches[i].to(StdSSubMatch)
          str = subm.str().c_str().to(cstring)
          cbstr = makeString(str)
        b.results.push(cbstr)
      res = b.results
    res

  chainblock CBMatchText, "MatchText"
