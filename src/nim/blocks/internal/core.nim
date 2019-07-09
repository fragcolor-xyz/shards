import strutils

# RunChain - runs a sub chain, ends the parent chain if the sub chain fails
when true:
  type
    CBRunChain* = object
      chain: ptr CBChainPtr
      once: bool
      done: bool
      passthrough: bool

  template cleanup*(b: CBRunChain) =
    if b.chain != nil and b.chain[] != nil:
      discard b.chain[].stop()
  template inputTypes*(b: CBRunChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBRunChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBRunChain): CBParametersInfo =
    @[
      ("Chain", { Chain }), 
      ("Once", { Bool }),
      ("Passthrough", { Bool })
    ]
  template setParam*(b: CBRunChain; index: int; val: CBVar) =
    case index
    of 0:
      b.chain = val.chainValue
    of 1:
      b.once = val.boolValue
    of 2:
      b.passthrough = val.boolValue
    else:
      assert(false)
  template getParam*(b: CBRunChain; index: int): CBVar =
    case index
    of 0:
      if b.chain == nil:
        CBVar(valueType: None)
      else:
        b.chain
    of 1:
      CBVar(valueType: Bool, boolValue: b.once)
    of 2:
      CBVar(valueType: Bool, boolValue: b.passthrough)
    else:
      CBVar(valueType: None)
  template activate*(b: CBRunChain; context: CBContext; input: CBVar): CBVar =
    if b.chain == nil:
      input
    elif b.chain[] == nil: # Chain is required but not avai! , stop!
      halt(context, "A required sub-chain was not found, stopping!")
    elif not b.done:
      if b.once:
        b.done = true
      b.chain[][].finished = false
      let
        resTuple = runChain(b.chain[], context, input)
        noerrors = cppTupleGet[bool](0, resTuple.toCpp)
        output = cppTupleGet[CBVar](1, resTuple.toCpp)
      b.chain[][].finished = true
      if not noerrors or context[].aborted:
        b.chain[][].finishedOutput = StopChain
        b.chain[][].finishedOutput # not succeeded means Stop/Exception/Error so we propagate the stop
      elif b.passthrough: # Second priority to passthru property
        b.chain[][].finishedOutput = output
        input
      else: # Finally process the actual output
        b.chain[][].finishedOutput = output
        output
    else:
      input

  chainblock CBRunChain, "RunChain"

# WaitChain - Waits a chain executed with RunChain, returns the output of that chain or Stop/End signals
when true:
  type
    CBWaitChain* = object
      chain: ptr CBChainPtr
      once: bool
      done: bool
      passthrough: bool

  template inputTypes*(b: CBWaitChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBWaitChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBWaitChain): CBParametersInfo =
    @[
      ("Chain", { Chain }), 
      ("Once", { Bool }),
      ("Passthrough", { Bool })
    ]
  template setParam*(b: CBWaitChain; index: int; val: CBVar) =
    case index
    of 0:
      b.chain = val.chainValue
    of 1:
      b.once = val.boolValue
    of 2:
      b.passthrough = val.boolValue
    else:
      assert(false)
  template getParam*(b: CBWaitChain; index: int): CBVar =
    case index
    of 0:
      if b.chain == nil:
        CBVar(valueType: None)
      else:
        b.chain
    of 1:
      CBVar(valueType: Bool, boolValue: b.once)
    of 2:
      CBVar(valueType: Bool, boolValue: b.passthrough)
    else:
      CBVar(valueType: None)
  template activate*(b: var CBWaitChain; context: CBContext; input: CBVar): CBVar =
    if b.chain == nil:
      input
    elif b.chain[] == nil: # Chain is required but not avai! , stop!
      halt(context, "A required sub-chain was not found, stopping!")
    elif not b.done:
      if b.once:
        b.done = true
      
      while not b.chain[][].finished:
        pause(0.0)
      
      if b.passthrough:
        input
      else:
        b.chain.finishedOutput
    else:
      input

  chainblock CBWaitChain, "WaitChain"

# SetGlobal - sets a global variable
when true:
  type
    CBSetGlobal* = object
      name*: string
      target*: ptr CBVar
  
  template cleanup*(b: CBSetGlobal) =
    b.target = nil
  template inputTypes*(b: CBSetGlobal): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBSetGlobal): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBSetGlobal): CBParametersInfo = @[("Name", { String })]
  template setParam*(b: CBSetGlobal; index: int; val: CBVar) =
    b.name = val.stringValue
    cleanup(b)
  template getParam*(b: CBSetGlobal; index: int): CBVar = b.name
  template activate*(b: CBSetGlobal; context: CBContext; input: CBVar): CBVar =
    if b.target == nil:
      b.target = globalVariable(b.name)
    b.target[] = input
    input

  chainblock CBSetGlobal, "SetGlobal"

# WaitGlobal - waits and puts a global variable in the local chain context
when true:
  type
    CBUseGlobal* = object
      name*: string
      ctarget*, gtarget*: ptr CBVar
  
  template cleanup*(b: CBUseGlobal) =
    b.ctarget = nil
    b.gtarget = nil
  template inputTypes*(b: CBUseGlobal): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBUseGlobal): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBUseGlobal): CBParametersInfo = @[("Name", { String })]
  template setParam*(b: CBUseGlobal; index: int; val: CBVar) =
    b.name = val.stringValue
    cleanup(b)
  template getParam*(b: CBUseGlobal; index: int): CBVar = b.name
  template activate*(b: var CBUseGlobal; context: CBContext; input: CBVar): CBVar =
    if b.ctarget == nil:
      b.ctarget = context.contextVariable(b.name)
    if b.gtarget == nil:
      while not hasGlobalVariable(b.name):
        pause(0.0) # Wait until we find it
      b.gtarget = globalVariable(b.name)
    b.ctarget[] = b.gtarget[]
    input

  chainblock CBUseGlobal, "WaitGlobal"

# Const - a debug value logger
when true:
  type
    CBlockConst* = object
      value: CBVar

  template inputTypes*(b: CBlockConst): CBTypesInfo = { None }
  template outputTypes*(b: CBlockConst): CBTypesInfo = ({ None, Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color }, true)
  template parameters*(b: CBlockConst): CBParametersInfo = 
    @[("Value", ({ None, Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color }, true #[seq]#))]
  template setParam*(b: CBlockConst; index: int; val: CBVar) =
    b.value.free() # might have some value stored!
    b.value = val.clone()
  template getParam*(b: CBlockConst; index: int): CBVar = b.value
  template activate*(b: CBlockConst; context: CBContext; input: CBVar): CBVar =
    b.value
  template destroy*(b: CBlockConst) = b.value.free()

  chainblock CBlockConst, "Const"

# Restart - ends a chain iteration, applies up to the root chain
when true:
  type
    CBEndChain* = object

  template inputTypes*(b: CBEndChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBEndChain): CBTypesInfo = { None }
  template activate*(b: var CBEndChain; context: CBContext; input: CBVar): CBVar =
    context.restarted = true
    CBVar(valueType: None, chainState: Restart)

  chainblock CBEndChain, "ChainRestart"

# Stop - stops a chain, applies up to the root chain
when true:
  type
    CBStopChain* = object

  template inputTypes*(b: CBStopChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStopChain): CBTypesInfo = { None }
  template activate*(b: var CBStopChain; context: CBContext; input: CBVar): CBVar =
    context.aborted = true
    CBVar(valueType: None, chainState: Stop)

  chainblock CBStopChain, "ChainStop"

# Log - a debug value logger
when true:
  type
    CBlockLog* = object

  template inputTypes*(b: CBlockLog): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBlockLog): CBTypesInfo = ({ Any }, true #[seq]#)
  template activate*(b: var CBlockLog; context: CBContext; input: CBVar): CBVar =
    if input.valueType == ContextVar:
      echo context.contextVariable(input.stringValue)[]
    else:
      echo input
    input

  chainblock CBlockLog, "Log"

# Msg - a debug log echo
when true:
  type
    CBlockMsg* = object
      msg*: CBVar

  template inputTypes*(b: CBlockMsg): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBlockMsg): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBlockMsg): CBParametersInfo = @[("Message", { String })]
  template setParam*(b: CBlockMsg; index: int; val: CBVar) = b.msg = val.clone()
  template getParam*(b: CBlockMsg; index: int): CBVar = b.msg
  template activate*(b: CBlockMsg; context: CBContext; input: CBVar): CBVar =
    echo b.msg
    input

  chainblock CBlockMsg, "Msg"

# Sleep (coroutine)
when true:
  type
    CBlockSleep* = object
      time: float

  template inputTypes*(b: CBlockSleep): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBlockSleep): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBlockSleep): CBParametersInfo = @[("Time", { Float })]
  template setParam*(b: CBlockSleep; index: int; val: CBVar) =
    assert index == 0
    assert val.valueType == Float
    b.time = val.floatValue
  template getParam*(b: CBlockSleep; index: int): CBVar =
    assert index == 0
    CBVar(valueType: Float, floatValue: b.time)
  template activate*(b: CBlockSleep; context: CBContext; input: CBVar): CBVar =
    pause(b.time)
    input

  chainblock CBlockSleep, "Sleep"

# When - a filter block, that let's inputs pass only when the condition is true
when true:
  type
    CBWhen* = object
      acceptedValues*: CBSeq
      isregex*: bool
      all*: bool
  
  template setup*(b: CBWhen) = initSeq(b.acceptedValues)
  template destroy*(b: CBWhen) = freeSeq(b.acceptedValues, true)
  template inputTypes*(b: CBWhen): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBWhen): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBWhen): CBParametersInfo = 
    @[
      ("Accept", ({ Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color }, true #[seq]#), true #[context]#),
      ("IsRegex", ({ Bool }, false #[seq]#), false #[context]#),
      ("All", ({ Bool }, false #[seq]#), false #[context]#) # must match all of the accepted
    ]
  template setParam*(b: CBWhen; index: int; val: CBVar) =
    case index
    of 0:
      if val.valueType == Seq:
        b.acceptedValues = val.seqValue.clone()
      else:
        b.acceptedValues.push(val.clone())
    of 1:
      b.isregex = val.boolValue
    of 2:
      b.all = val.boolValue
    else:
      assert(false)
  template getParam*(b: CBWhen; index: int): CBVar =
    case index
    of 0:
      if b.acceptedValues.len == 1:
        b.acceptedValues[0]
      elif b.acceptedValues.len > 1:
        b.acceptedValues
      else:
        CBVar(valueType: None)
    of 1:
      b.isregex
    of 2:
      b.all
    else:
      assert(false)
      Empty
  template activate*(b: CBWhen; context: CBContext; input: CBVar): CBVar =
    var res = RestartChain
    var matches = 0
    for accepted in b.acceptedValues:
      let val = if accepted.valueType == ContextVar: context.contextVariable(accepted.stringValue)[] else: accepted
      if b.isregex and val.valueType == String and input.valueType == String:
        let regex = StdRegex.cppinit(val.stringValue.cstring)
        if invokeFunction("std::regex_match", input.stringValue.cstring, regex).to(bool):
          if not b.all:
            res = input
            break
          else:
            inc matches
      elif input == val:
        if not b.all:
          res = input
          break
        else:
          inc matches
    if b.all and matches == b.acceptedValues.len:
      res = input
    res

  chainblock CBWhen, "When"

# WhenNot - a filter block, that let's inputs pass only when the condition is false
when true:
  type
    CBWhenNot* = object
      acceptedValues*: CBSeq
      isregex*: bool
      all*: bool
  
  template setup*(b: CBWhenNot) = initSeq(b.acceptedValues)
  template destroy*(b: CBWhenNot) = freeSeq(b.acceptedValues, true)
  template inputTypes*(b: CBWhenNot): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBWhenNot): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBWhenNot): CBParametersInfo = 
    @[
      ("Reject", ({ Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color }, true #[seq]#), true #[context]#),
      ("IsRegex", ({ Bool }, false #[seq]#), false #[context]#),
      ("All", ({ Bool }, false #[seq]#), false #[context]#) # must match all of the accepted
    ]
  template setParam*(b: CBWhenNot; index: int; val: CBVar) =
    case index
    of 0:
      if val.valueType == Seq:
        b.acceptedValues = val.seqValue.clone()
      else:
        b.acceptedValues.push(val.clone())
    of 1:
      b.isregex = val.boolValue
    of 2:
      b.all = val.boolValue
    else:
      assert(false)
  template getParam*(b: CBWhenNot; index: int): CBVar =
    case index
    of 0:
      if b.acceptedValues.len == 1:
        b.acceptedValues[0]
      elif b.acceptedValues.len > 1:
        b.acceptedValues
      else:
        CBVar(valueType: None)
    of 1:
      b.isregex
    of 2:
      b.all
    else:
      assert(false)
      Empty
  template activate*(b: CBWhenNot; context: CBContext; input: CBVar): CBVar =
    var res = input
    var matches = 0
    for accepted in b.acceptedValues:
      let val = if accepted.valueType == ContextVar: context.contextVariable(accepted.stringValue)[] else: accepted
      if b.isregex and val.valueType == String and input.valueType == String:
        let regex = StdRegex.cppinit(val.stringValue.cstring)
        if invokeFunction("std::regex_match", input.stringValue.cstring, regex).to(bool):
          if not b.all:
            res = RestartChain
            break
          else:
            inc matches
      elif input == val:
        if not b.all:
          res = RestartChain
          break
        else:
          inc matches
    if b.all and matches == b.acceptedValues.len:
      res = RestartChain
    res

  chainblock CBWhenNot, "WhenNot"

# If
when true:
  const
    BoolOpCC* = toFourCC("bool")
  
  var
    boolOpLabelsSeq = @["Equal", "More", "Less", "MoreEqual", "LessEqual"]
    boolOpLabels = boolOpLabelsSeq.toCBStrings()
  
  let
    boolOpInfo = CBTypeInfo(basicType: Enum, enumVendorId: FragCC.int32, enumTypeId: BoolOpCC.int32)
  
  registerEnumType(FragCC, BoolOpCC, CBEnumInfo(name: "Boolean operation", labels: boolOpLabels))

  type
    CBBoolOp* = enum
      Equal,
      More,
      Less,
      MoreEqual,
      LessEqual

  type
    CBlockIf* = object
      Op: CBBoolOp
      Match: CBVar
      True: ptr CBChainPtr
      False: ptr CBChainPtr
  
  template cleanup*(b: CBlockIf) =
    if b.True != nil and b.True[] != nil:
      discard b.True[].stop()
    if b.False != nil and b.False[] != nil:
      discard b.False[].stop()
  template destroy*(b: CBlockIf) = b.Match.free()
  template inputTypes*(b: CBlockIf): CBTypesInfo = ({ Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color }, true #[seq]#)
  template outputTypes*(b: CBlockIf): CBTypesInfo = ({ Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color }, true #[seq]#)
  template parameters*(b: CBlockIf): CBParametersInfo =
    @[
      ("Operator", @[boolOpInfo].toCBTypesInfo(), false),
      ("Operand", ({ Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color }, true #[seq]#).toCBTypesInfo(), true #[context]#),
      ("True", ({ Chain, None }, false).toCBTypesInfo(), false),
      ("False", ({ Chain, None }, false).toCBTypesInfo(), false)
    ]
  template setParam*(b: CBlockIf; index: int; val: CBVar) =
    case index
    of 0:
      b.Op = val.enumValue.CBBoolOp
    of 1:
      b.Match.free()
      b.Match = val.clone()
    of 2:
      case val.valueType
      of Chain:
        b.True = val.chainValue
      of None:
        b.True = nil
      else:
        discard
    of 3:
      case val.valueType
      of Chain:
        b.False = val.chainValue
      of None:
        b.False = nil
      else:
        discard
    else:
      assert(false)
  template getParam*(b: CBlockIf; index: int): CBVar =
    case index
    of 0:
      CBVar(valueType: Enum, enumValue: b.Op.CBEnum, enumVendorId: FragCC.int32, enumTypeId: BoolOpCC.int32)
    of 1:
      b.Match
    of 2:
      if b.True == nil:
        CBVar(valueType: None)
      else:
        b.True
    of 3:
      if b.False == nil:
        CBVar(valueType: None)
      else:
        b.False
    else:
      Empty
  template activate*(b: CBlockIf; context: CBContext; input: CBVar): CBVar =
    let
      match = if b.Match.valueType == ContextVar: context.contextVariable(b.Match.stringValue)[] else: b.Match
    var
      noerrors = false
      res: CBVar
    template operation(operator: untyped): untyped =
      if operator(input, match):
        if b.True != nil:
          if b.True[] == nil:
            return halt(context, "A required sub-chain was not found, stopping!")
          let
            resTuple = runChain(b.True[], context, input)
            noerrors = cppTupleGet[bool](0, resTuple.toCpp)
            res = cppTupleGet[CBVar](1, resTuple.toCpp)
          # While we ignore the results from the chain we need to check for global cancelation
          if not noerrors or context[].aborted:
            return StopChain
          elif context[].restarted:
            return RestartChain
      else:
        if b.False != nil:
          if b.False[] == nil:
            return halt(context, "A required sub-chain was not found, stopping!")
          let
            resTuple = runChain(b.False[], context, input)
            noerrors = cppTupleGet[bool](0, resTuple.toCpp)
            res = cppTupleGet[CBVar](1, resTuple.toCpp)
          # While we ignore the results from the chain we need to check for global cancelation
          if not noerrors or context[].aborted:
            return StopChain
          elif context[].restarted:
            return RestartChain

    case b.Op
    of Equal: operation(`==`)
    of More: operation(`>`)
    of Less: operation(`<`)
    of MoreEqual: operation(`>=`)
    of LessEqual: operation(`<=`)

    input

  chainblock CBlockIf, "If"

# Repeat - repeats the Chain parameter Times n
when true:
  type
    CBRepeat* = object
      times: int64
      chain: ptr CBChainPtr
  
  template cleanup*(b: CBRepeat) =
    if b.chain != nil and b.chain[] != nil:
      discard b.chain[].stop()
  template inputTypes*(b: CBRepeat): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBRepeat): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBRepeat): CBParametersInfo =
    @[
      ("Times", { Int }),
      ("Chain", { Chain })
    ]
  template setParam*(b: CBRepeat; index: int; val: CBVar) =
    case index
    of 0:
      b.times = val.intValue
    of 1:
      b.chain = val.chainValue
    else:
      assert(false)
  proc getParam*(b: CBRepeat; index: int): CBVar {.inline.} =
    case index
    of 0:
      b.times.CBVar
    of 1:
      b.chain.CBVar
    else:
      Empty
  template activate*(b: var CBRepeat; context: CBContext; input: CBVar): CBVar =
    if b.chain == nil:
      input
    elif b.chain[] == nil:
      halt(context, "A required sub-chain was not found, stopping!")
    else:
      for i in 0..<b.times:
        let
          resTuple = runChain(b.chain[], context, input)
          noerrors = cppTupleGet[bool](0, resTuple.toCpp)
        # While we ignore the results from the chain we need to check for global cancelation
        if not noerrors or context[].aborted:
          return StopChain
        elif context[].restarted:
          return RestartChain
      input

  chainblock CBRepeat, "Repeat"

# ToString - converts the input to a string
when true:
  type
    CBToString* = object
      cachedStr*: CBString
  
  template setup*(b: CBToString) = b.cachedStr = makeString("")
  template destroy*(b: CBToString) = freeString(b.cachedStr)
  template inputTypes*(b: CBToString): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBToString): CBTypesInfo = { String }
  template activate*(b: var CBToString; context: CBContext; input: CBVar): CBVar =
    if input.valueType == ContextVar:
      b.cachedStr = b.cachedStr.setString($(context.contextVariable(input.stringValue)[]))
    else:
      b.cachedStr = b.cachedStr.setString($input)
    b.cachedStr

  chainblock CBToString, "ToString"

# ToFloat - converts the input into floats
when true:
  type
    CBToFloat* = object
      cachedSeq*: CBSeq

  template setup*(b: CBToFloat) = initSeq(b.cachedSeq)
  template destroy*(b: CBToFloat) = freeSeq(b.cachedSeq)
  template inputTypes*(b: CBToFloat): CBTypesInfo = ({ Float, Float2, Float3, Float4, String, Int, Int2, Int3, Int4, Color }, true)
  template outputTypes*(b: CBToFloat): CBTypesInfo = ({ Float, Float2, Float3, Float4 }, true)
  proc unpackToFloats(input: CBVar): CBVar {.inline, noinit.} =
    case input.valueType
    of String: return parseFloat(input.stringValue).CBVar
    of Int: return input.intValue.CBInt.CBVar
    of Int2:
      var f2 = CBVar(valueType: Float2)
      f2.float2Value[0] = input.int2Value[0].CBFloat
      f2.float2Value[1] = input.int2Value[1].CBFloat
      return f2
    of Int3:
      var f3 = CBVar(valueType: Float3)
      f3.float3Value[0] = input.int3Value[0].CBFloat
      f3.float3Value[1] = input.int3Value[1].CBFloat
      f3.float3Value[2] = input.int3Value[2].CBFloat
      return f3
    of Int4:
      var f4 = CBVar(valueType: Float4)
      f4.float4Value[0] = input.int4Value[0].CBFloat
      f4.float4Value[1] = input.int4Value[1].CBFloat
      f4.float4Value[2] = input.int4Value[2].CBFloat
      f4.float4Value[3] = input.int4Value[3].CBFloat
      return f4
    of Color:
      var f4 = CBVar(valueType: Float4)
      f4.float4Value[0] = input.colorValue.r.CBFloat
      f4.float4Value[1] = input.colorValue.g.CBFloat
      f4.float4Value[2] = input.colorValue.b.CBFloat
      f4.float4Value[3] = input.colorValue.a.CBFloat
      return f4
    of Float, Float2, Float3, Float4:
      return input
    else: return 0.0.CBVar
  template activate*(b: var CBToFloat; context: CBContext; input: CBVar): CBVar =
    if input.valueType == Seq:
      b.cachedSeq.clear()
      for inputv in input.seqValue.items:
        b.cachedSeq.push unpackToFloats(inputv)
      b.cachedSeq.CBVar
    elif input.valueType == CBType.Image:
      b.cachedSeq.clear()
      for y in 0..<input.imageValue.height:
        for x in 0..<input.imageValue.width:
          var storage: array[4, uint8]
          input.imageValue.getPixel(x, y, storage, 0, input.imageValue.channels - 1)
          case input.imageValue.channels
          of 1:
            b.cachedSeq.push storage[0].float64.CBVar
          of 3:
            b.cachedSeq.push (storage[0].float32, storage[1].float32, storage[2].float32).CBVar
          of 4:
            b.cachedSeq.push (storage[0].float32, storage[1].float32, storage[2].float32, storage[3].float32).CBVar
          else:
            assert(false, "Unsupported image channels number")
      b.cachedSeq.CBVar
    else:
      unpackToFloats(input)

  chainblock CBToFloat, "ToFloat"

# ToImage - converts the input into an image
when true:
  type
    CBToImage* = object
      w*,h*: int
      alpha*: bool
      imgCache*: Image[uint8]

  template destroy*(b: CBToImage) =
    if b.imgCache.data != nil:
      dealloc(b.imgCache.data)
      b.imgCache.data = nil
  template inputTypes*(b: CBToImage): CBTypesInfo = ({ Float, Float2, Float3, Float4, Int, Int2, Int3, Int4, Color }, true)
  template outputTypes*(b: CBToImage): CBTypesInfo = { CBType.Image }
  template parameters*(b: CBToImage): CBParametersInfo =
    @[
      ("Width", { Int }),
      ("Height", { Int }),
      ("Alpha", { Bool })
    ]
  template setParam*(b: CBToImage; index: int; val: CBVar) =
    case index
    of 0:
      b.w = val.intValue.int
    of 1:
      b.h = val.intValue.int
    of 2:
      b.alpha = val.boolValue
    else:
      assert(false)
  template getParam*(b: CBToImage; index: int): CBVar =
    case index
    of 0:
      b.w.CBVar
    of 1:
      b.h.CBVar
    of 2:
      b.alpha.CBVar
    else:
      Empty
  template activate*(blk: var CBToImage; context: CBContext; input: CBVar): CBVar =
    if  blk.imgCache.data == nil or 
        blk.imgCache.width != blk.w or 
        blk.imgCache.height != blk.h or
        (blk.alpha and blk.imgCache.channels == 3) or
        (not blk.alpha and blk.imgCache.channels == 4):
      if blk.imgCache.data != nil:
        dealloc(blk.imgCache.data)
      
      blk.imgCache.channels = if blk.alpha: 4 else: 3
      let size = blk.w * blk.h * blk.imgCache.channels
      blk.imgCache.data = cast[ptr UncheckedArray[uint8]](alloc0(size))
      blk.imgCache.width = blk.w
      blk.imgCache.height = blk.h
    
    let maxLen = blk.imgCache.width * blk.imgCache.height * blk.imgCache.channels

    if input.valueType != Seq:
      halt(context, "ToImage always expects a sequence.")
    else:
      var index = 0
      for i in 0..min(maxLen - 1, input.seqValue.len - 1):
        let val = input.seqValue[i]
        case val.valueType
        of Float:
          blk.imgCache.data[index] = val.floatValue.clamp(0, 255).uint8
          inc index
        of Float2:
          blk.imgCache.data[index] = val.float2Value[0].clamp(0, 255).uint8
          inc index
          blk.imgCache.data[index] = val.float2Value[1].clamp(0, 255).uint8
          inc index
        of Float3:
          blk.imgCache.data[index] = val.float3Value[0].clamp(0, 255).uint8
          inc index
          blk.imgCache.data[index] = val.float3Value[1].clamp(0, 255).uint8
          inc index
          blk.imgCache.data[index] = val.float3Value[2].clamp(0, 255).uint8
          inc index
        of Float4:
          blk.imgCache.data[index] = val.float4Value[0].clamp(0, 255).uint8
          inc index
          blk.imgCache.data[index] = val.float4Value[1].clamp(0, 255).uint8
          inc index
          blk.imgCache.data[index] = val.float4Value[2].clamp(0, 255).uint8
          inc index
          if blk.imgCache.channels > 3:
            blk.imgCache.data[index] = val.float4Value[3].clamp(0, 255).uint8
            inc index
        of Int:
          blk.imgCache.data[index] = val.intValue.clamp(0, 255).uint8
          inc index
        of Int2:
          blk.imgCache.data[index] = val.int2Value[0].clamp(0, 255).uint8
          inc index
          blk.imgCache.data[index] = val.int2Value[1].clamp(0, 255).uint8
          inc index
        of Int3:
          blk.imgCache.data[index] = val.int3Value[0].clamp(0, 255).uint8
          inc index
          blk.imgCache.data[index] = val.int3Value[1].clamp(0, 255).uint8
          inc index
          blk.imgCache.data[index] = val.int3Value[2].clamp(0, 255).uint8
          inc index
        of Int4:
          blk.imgCache.data[index] = val.int4Value[0].clamp(0, 255).uint8
          inc index
          blk.imgCache.data[index] = val.int4Value[1].clamp(0, 255).uint8
          inc index
          blk.imgCache.data[index] = val.int4Value[2].clamp(0, 255).uint8
          inc index
          if blk.imgCache.channels > 3:
            blk.imgCache.data[index] = val.int4Value[3].clamp(0, 255).uint8
            inc index
        of Color:
          blk.imgCache.data[index] = val.colorValue.r
          inc index
          blk.imgCache.data[index] = val.colorValue.g
          inc index
          blk.imgCache.data[index] = val.colorValue.b
          inc index
          if blk.imgCache.channels > 3:
            blk.imgCache.data[index] = val.colorValue.a
            inc index
        else:
          assert(false)

      CBVar(valueType: CBType.Image, imageValue: blk.imgCache)

  chainblock CBToImage, "ToImage"

# SetVariable - sets a context variable
when true:
  type
    CBSetVariable* = object
      name*: string
      target*: ptr CBVar
  
  template cleanup*(b: CBSetVariable) =
    b.target = nil
  template inputTypes*(b: CBSetVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBSetVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBSetVariable): CBParametersInfo = @[("Name", { String })]
  template setParam*(b: CBSetVariable; index: int; val: CBVar) =
    b.name = val.stringValue
    cleanup(b)
  template getParam*(b: CBSetVariable; index: int): CBVar = b.name
  template activate*(b: var CBSetVariable; context: CBContext; input: CBVar): CBVar =
    if b.target == nil:
      b.target = context.contextVariable(b.name)

    if b.target[].valueType == Seq and input == Empty:
      b.target[].seqValue.clear() # quick clean, no deallocations!
    else:
      b.target[] = input
    input

  chainblock CBSetVariable, "SetVariable"

# GetVariable - sets a context variable
when true:
  type
    CBGetVariable* = object
      name*: string
      target*: ptr CBVar
  
  template cleanup*(b: CBGetVariable) =
    b.target = nil
  template inputTypes*(b: CBGetVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBGetVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBGetVariable): CBParametersInfo = @[("Name", { String })]
  template setParam*(b: CBGetVariable; index: int; val: CBVar) =
    b.name = val.stringValue
    cleanup(b)
  template getParam*(b: CBGetVariable; index: int): CBVar = b.name
  template activate*(b: var CBGetVariable; context: CBContext; input: CBVar): CBVar =
    if b.target == nil:
      b.target = context.contextVariable(b.name)
    b.target[]

  chainblock CBGetVariable, "GetVariable"

# AddVariable - adds a variable to a context variable
when true:
  type
    CBAddVariable* = object
      name*: string
      target*: ptr CBVar
  
  template cleanup*(b: CBAddVariable) =
    if b.target != nil:
      # also we turned the target into a sequence, so we are responsible for the memory
      b.target[].free()
    b.target = nil
  template inputTypes*(b: CBAddVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBAddVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBAddVariable): CBParametersInfo = @[("Name", { String })]
  template setParam*(b: CBAddVariable; index: int; val: CBVar) =
    b.name = val.stringValue
    cleanup(b)
  template getParam*(b: CBAddVariable; index: int): CBVar = b.name
  template activate*(b: var CBAddVariable; context: CBContext; input: CBVar): CBVar =
    if b.target == nil:
      b.target = context.contextVariable(b.name)
    
    if b.target[].valueType != Seq:
      b.target[].valueType = Seq
      initSeq(b.target[].seqValue)

    if input.valueType == Seq:
      for item in input.seqValue.items:
        b.target[].seqValue.push(item)
    else:
      b.target[].seqValue.push(input)
    
    input

  chainblock CBAddVariable, "AddVariable"

# GetItems - gets an item from a Seq
when true:
  type
    CBGetItems* = object
      indices*: CBVar
      cachedResult*: CBSeq
  
  template setup*(b: CBGetItems) = initSeq(b.cachedResult)
  template destroy*(b: CBGetItems) = freeSeq(b.cachedResult)
  template inputTypes*(b: CBGetItems): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBGetItems): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBGetItems): CBParametersInfo = @[("Index", ({ Int }, true #[seq]#), false #[context]#)]
  template setParam*(b: CBGetItems; index: int; val: CBVar) = b.indices = val
  template getParam*(b: CBGetItems; index: int): CBVar = b.indices
  template activate*(b: var CBGetItems; context: CBContext; input: CBVar): CBVar =
    if input.valueType != Seq:
      halt(context, "GetItems expected a sequence!")
    elif b.indices.valueType == Int:
      if b.indices.intValue.int >= input.seqValue.len.int:
        halt(context, "GetItems out of range! len: " & $input.seqValue.len & " wanted index: " & $b.indices.intValue)
      else:
        input.seqValue[b.indices.intValue.int]
    elif b.indices.valueType == Seq:
      b.cachedResult.clear()
      for index in b.indices.seqValue:
        b.cachedResult.push(input.seqValue[index.intValue.int])
      b.cachedResult
    else:
      assert(false)
      Empty

  chainblock CBGetItems, "GetItems"
