# SetVariable - sets a context variable
when true:
  type
    CBSetVariable* = object
      # INLINE BLOCK, CORE STUB PRESENT
      target: ptr CBVar
      name: GbString
      exposedInfo: CBExposedTypesInfo
  
  template cleanup*(b: CBSetVariable) =
    if b.target != nil:
      `~quickcopy` b.target[]
    b.target = nil
  template inputTypes*(b: CBSetVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBSetVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBSetVariable): CBParametersInfo = *@[(cs"Name", { String })]
  template setParam*(b: CBSetVariable; index: int; val: CBVar) = b.name = val.stringValue; cleanup(b)
  template getParam*(b: CBSetVariable; index: int): CBVar = b.name
  
  proc inferTypes(b: var CBSetVariable; inputType: CBTypeInfo; consumables: CBExposedTypesInfo): CBTypeInfo =  
    result = inputType
    # Fix exposed type
    freeSeq(b.exposedInfo); initSeq(b.exposedInfo)
    b.exposedInfo.push(CBExposedTypeInfo(name: b.name.cstring, exposedType: result))
    
  proc exposedVariables*(b: var CBSetVariable): CBExposedTypesInfo = b.exposedInfo

  template activate*(b: CBSetVariable; context: CBContext; input: CBVar): CBVar =
    if b.target == nil:
      b.target = context.contextVariable(b.name)
      
    var src = input
    quickcopy(b.target[], src)
    
    input

  chainblock CBSetVariable, "SetVariable"

# GetVariable - sets a context variable
when true:
  type
    CBGetVariable* = object
      # INLINE BLOCK, CORE STUB PRESENT
      target*: ptr CBVar
      name*: GbString
  
  template cleanup*(b: CBGetVariable) = b.target = nil
  template inputTypes*(b: CBGetVariable): CBTypesInfo = ({ None })
  template outputTypes*(b: CBGetVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBGetVariable): CBParametersInfo = *@[(cs"Name", { String })]
  template setParam*(b: CBGetVariable; index: int; val: CBVar) = b.name = val.stringValue; cleanup(b)
  template getParam*(b: CBGetVariable; index: int): CBVar = b.name
  proc consumedVariables*(b: var CBGetVariable): CBExposedTypesInfo =
    initSeq(result) # assume cached
    result.push(CBExposedTypeInfo(name: b.name.cstring, exposedType: Any))
  proc inferTypes(b: var CBGetVariable; inputType: CBTypeInfo; consumables: CBExposedTypesInfo): CBTypeInfo =
    result = None
    for consumable in consumables.mitems:
      if consumable.name == b.name:
        result = consumable.exposedType
  template activate*(b: CBGetVariable; context: CBContext; input: CBVar): CBVar =
    if b.target == nil: b.target = context.contextVariable(b.name)
    b.target[]

  chainblock CBGetVariable, "GetVariable"

# SwapVariables - sets a context variable
when true:
  type
    CBSwapVariables* = object
      # INLINE BLOCK, CORE STUB PRESENT
      targeta*: ptr CBVar
      targetb*: ptr CBVar
      name1*: GbString
      name2*: GbString
  
  template cleanup*(b: CBSwapVariables) =
    b.targeta = nil
    b.targetb = nil
  template inputTypes*(b: CBSwapVariables): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBSwapVariables): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBSwapVariables): CBParametersInfo =
    *@[
      (cs"Name1", { String }),
      (cs"Name2", { String })
    ]
  template setParam*(b: CBSwapVariables; index: int; val: CBVar) =
    case index
    of 0: b.name1 = val.stringValue
    of 1: b.name2 = val.stringValue
    else: assert(false)
    cleanup(b)
  template getParam*(b: CBSwapVariables; index: int): CBVar =
    case index
    of 0: b.name1.CBVar
    of 1: b.name2
    else: assert(false); Empty
  proc consumedVariables*(b: var CBSwapVariables): CBExposedTypesInfo =
    initSeq(result)
    result.push(CBExposedTypeInfo(name: b.name1.cstring, exposedType: Any))
    result.push(CBExposedTypeInfo(name: b.name2.cstring, exposedType: Any))
  template activate*(b: CBSwapVariables; context: CBContext; input: CBVar): CBVar =
    if b.targeta == nil: b.targeta = context.contextVariable(b.name1)
    if b.targetb == nil: b.targetb = context.contextVariable(b.name2)
    
    let tmp = b.targeta[]
    b.targeta[] = b.targetb[]
    b.targetb[] = tmp

    input

  chainblock CBSwapVariables, "SwapVariables"

# AddVariable - creates or adds to Seq a variable, send Empty/None as input to reset!
when true:
  type
    CBAddVariable* = object
      target*: ptr CBVar
      name*: GbString
      maxSize: int
      exposedInfo: CBExposedTypesInfo
  
  template cleanup*(b: CBAddVariable) =
    if b.target != nil and b.target[].valueType == Seq:
      # Reset to max size to avoid leaking any var that might passed by
      b.target[].seqValue.setLen(b.maxSize)
      for val in b.target[].seqValue.mitems:
        `~quickcopy` val
      freeSeq(b.target[].seqValue)
    
    b.target = nil
  template inputTypes*(b: CBAddVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBAddVariable): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBAddVariable): CBParametersInfo = *@[(cs"Name", { String })]
  template setParam*(b: CBAddVariable; index: int; val: CBVar) = b.name = val.stringValue; cleanup(b)
  template getParam*(b: CBAddVariable; index: int): CBVar = b.name
  
  proc inferTypes(b: var CBAddVariable; inputType: CBTypeInfo; consumables: CBExposedTypesInfo): CBTypeInfo =
    result = inputType
    result.sequenced = true # we return input type in a seq
    
    # Fix exposed type
    freeSeq(b.exposedInfo); initSeq(b.exposedInfo)
    b.exposedInfo.push(CBExposedTypeInfo(name: b.name.cstring, exposedType: result))
    
  proc exposedVariables*(b: var CBAddVariable): CBExposedTypesInfo = b.exposedInfo

  template activate*(b: CBAddVariable; context: CBContext; input: CBVar): CBVar =
    if b.target == nil: b.target = context.contextVariable(b.name)
    
    # Init the seq in this case/beginning
    if b.target[].valueType == None:
      b.target[].valueType = Seq
      b.target[].seqLen = -1
      initSeq(b.target[].seqValue)

    if input.valueType == None:
      # Store max size in order to be able to clean properly
      b.maxSize = max(b.maxSize, b.target[].seqValue.len)
      b.target[].seqValue.clear()
    else:
      # Update values etc
      if input.valueType == Seq:
        for item in input.seqValue.mitems:
          var cp: CBVar
          quickcopy(cp, item)
          b.target[].seqValue.push(cp)
      else:
        var
          inpt = input
          cp: CBVar
        quickcopy(cp, inpt)
        b.target[].seqValue.push(cp)
    
    input
  
  chainblock CBAddVariable, "AddVariable"

# GetItems - gets an item from a Seq
when false:
  type
    CBGetItems* = object
      indices*: CBVar
      cachedResult*: CBSeq
  
  template setup*(b: CBGetItems) = initSeq(b.cachedResult)
  template destroy*(b: CBGetItems) = freeSeq(b.cachedResult)
  template inputTypes*(b: CBGetItems): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBGetItems): CBTypesInfo = ({ Any }, true #[seq]#)
  proc inferTypes(b: var CBGetItems; inputType: CBTypeInfo; consumables: CBExposedTypesInfo): CBTypeInfo =
    result = inputType
    if b.indices.valueType == Seq and b.indices.seqValue.len > 1:
      result.sequenced = true # we return input type in a seq
    else:
      result.sequenced = false
  template parameters*(b: CBGetItems): CBParametersInfo = *@[(cs"Index", ({ Int }, true #[seq]#), false #[context]#)]
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

# RunChain - Runs a sub chain, ends the parent chain if the sub chain fails
when true:
  type
    CBRunChain* = object
      chain: CBChainPtr
      once: bool
      done: bool
      passthrough: bool
      detach: bool

  template cleanup*(b: CBRunChain) =
    if b.chain != nil:
      b.chain.stop()
    b.done = false # reset this on stop/cleanup
  template inputTypes*(b: CBRunChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBRunChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBRunChain): CBParametersInfo =
    *@[
      (cs"Chain", cs"The chain to run.", { Chain, None }),
      (cs"Once", cs"Runs this sub-chain only once within the parent chain execution cycle.", { Bool }),
      (cs"Passthrough", cs"The input of this block will be the output. Always on if Detached.", { Bool }),
      (cs"Detached", cs"Runs the sub-chain as a completely separate parallel chain in the same node.", { Bool })
    ]
  template setParam*(b: CBRunChain; index: int; val: CBVar) =
    case index
    of 0:
      if val.valueType == None:
        b.chain = nil
      else:
        b.chain = val.chainValue
    of 1:
      b.once = val.boolValue
    of 2:
      b.passthrough = val.boolValue
      if b.detach: # force when detached
        b.passthrough = true
    of 3:
      b.detach = val.boolValue
      if b.detach: # force when detached
        b.passthrough = true
    else:
      assert(false)
  template getParam*(b: CBRunChain; index: int): CBVar =
    case index
    of 0:
      if b.chain != nil:
        b.chain.CBVar
      else:
        CBVar(valueType: None)  
    of 1:
      b.once
    of 2:
      b.passthrough
    of 3:
      b.detach
    else:
      CBVar(valueType: None)
  proc inferTypes*(b: var CBRunChain; inputType: CBTypeInfo; consumables: CBExposedTypesInfo): CBTypeInfo =
    if b.passthrough or b.chain == nil:
      return inputType
    else:
      # We need to validate the sub chain to figure it out!
      var failed = false
      let subResult = validateConnections(
        b.chain,
        proc(blk: ptr CBRuntimeBlock; error: cstring; nonfatalWarning: bool; userData: pointer) {.cdecl.} =
          var failedPtr = cast[ptr bool](userData)
          if not nonfatalWarning:
            failedPtr[] = true,
        addr failed,
        inputType
      )
      if not failed:
        return subResult
      else:
        return None
  template activate*(b: CBRunChain; context: CBContext; input: CBVar): CBVar =
    if b.chain == nil:
      input
    elif not b.done:
      if b.once: # flag immediately in this case, easy
          b.done = true
      
      if b.detach:
        if not b.chain.isRunning():
          getCurrentChain().node.schedule(b.chain, input)
        input
      else:
        # Run within the root flow, just call runChain
        b.chain[].finished = false
        let
          runRes = runChain(b.chain, context, input)
          noerrors = runRes.state != RunChainOutputState.Failed
          output = runRes.output
        b.chain[].finishedOutput = output
        b.chain[].finished = true
        if not noerrors or context[].aborted:
          b.chain[].finishedOutput = StopChain
          b.chain[].finishedOutput # not succeeded means Stop/Exception/Error so we propagate the stop
        elif b.passthrough: # Second priority to passthru property
          input
        else: # Finally process the actual output
          output
    else:
      input

  chainblock CBRunChain, "RunChain"

# WaitChain
when true:
  type
    CBWaitChain* = object
      chain: CBChainPtr
      once: bool
      done: bool
      passthrough: bool
  
  template help*(b: CBWaitChain): cstring = "Waits a chain executed with RunChain, returns the output of that chain or Stop/Restart signals."
  template inputTypes*(b: CBWaitChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBWaitChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBWaitChain): CBParametersInfo =
    *@[
      (cs"Chain", { Chain }), 
      (cs"Once", { Bool }),
      (cs"Passthrough", { Bool })
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
      b.chain.CBVar
    of 1:
      CBVar(valueType: Bool, payload: CBVarPayload(boolValue: b.once))
    of 2:
      CBVar(valueType: Bool, payload: CBVarPayload(boolValue: b.passthrough))
    else:
      CBVar(valueType: None)
  proc inferTypes*(b: var CBWaitChain; inputType: CBTypeInfo; consumables: CBExposedTypesInfo): CBTypeInfo =
    if b.passthrough or b.chain == nil:
      return inputType
    else:
      # We need to validate the sub chain to figure it out!
      var failed = false
      let subResult = validateConnections(
        b.chain,
        proc(blk: ptr CBRuntimeBlock; error: cstring; nonfatalWarning: bool; userData: pointer) {.cdecl.} =
          var failedPtr = cast[ptr bool](userData)
          if not nonfatalWarning:
            failedPtr[] = true,
        addr failed,
        inputType
      )
      if not failed:
        return subResult
      else:
        return None
  template activate*(b: CBWaitChain; context: CBContext; input: CBVar): CBVar =
    if b.chain == nil:
      input
    elif not b.done:
      if b.once:
        b.done = true
      
      while not b.chain[].finished:
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
      name*: GbString
      target*: ptr CBVar
  
  template cleanup*(b: CBSetGlobal) =
    b.target = nil
  template inputTypes*(b: CBSetGlobal): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBSetGlobal): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBSetGlobal): CBParametersInfo = *@[(cs"Name", { String })]
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
      name*: GbString
      ctarget*, gtarget*: ptr CBVar
  
  template cleanup*(b: CBUseGlobal) =
    b.ctarget = nil
    b.gtarget = nil
  template inputTypes*(b: CBUseGlobal): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBUseGlobal): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBUseGlobal): CBParametersInfo = *@[(cs"Name", { String })]
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

# Const - passes a const value
when true:
  type
    CBlockConst* = object
      # INLINE BLOCK, CORE STUB PRESENT
      value*: CBVar
  
  template destroy*(b: CBlockConst) = `~quickcopy` b.value
  template inputTypes*(b: CBlockConst): CBTypesInfo = { None }
  template outputTypes*(b: CBlockConst): CBTypesInfo = (AllIntTypes + AllFloatTypes + { None, String, Color }, true)
  template parameters*(b: CBlockConst): CBParametersInfo = 
    *@[(cs"Value", (AllIntTypes + AllFloatTypes + { None, String, Color }, true #[seq]#))]
  template setParam*(b: CBlockConst; index: int; val: CBVar) =
    `~quickcopy` b.value
    var inval = val
    quickcopy(b.value, inval)
  template getParam*(b: CBlockConst; index: int): CBVar = b.value
  template inferTypes*(b: CBlockConst; inputType: CBTypeInfo; consumables: CBExposedTypesInfo): CBTypeInfo =
    b.value.valueType
    # what if it's seq or table? need to add that info in! TODO
  template activate*(b: CBlockConst; context: CBContext; input: CBVar): CBVar =
    # THIS CODE WON'T BE EXECUTED
    # THIS BLOCK IS OPTIMIZED INLINE IN THE C++ CORE
    b.value

  chainblock CBlockConst, "Const"

# Restart - ends a chain iteration, applies up to the root chain
when true:
  type
    CBEndChain* = object

  template inputTypes*(b: CBEndChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBEndChain): CBTypesInfo = { None }
  template activate*(b: var CBEndChain; context: CBContext; input: CBVar): CBVar =
    context.restarted = true
    RestartChain

  chainblock CBEndChain, "ChainRestart"

# Stop - stops a chain, applies up to the root chain
when true:
  type
    CBStopChain* = object

  template inputTypes*(b: CBStopChain): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStopChain): CBTypesInfo = { None }
  template activate*(b: var CBStopChain; context: CBContext; input: CBVar): CBVar =
    context.aborted = true
    StopChain

  chainblock CBStopChain, "ChainStop"

# Log - a debug value logger
when true:
  type
    CBlockLog* = object

  template inputTypes*(b: CBlockLog): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBlockLog): CBTypesInfo = ({ Any }, true #[seq]#)
  template activate*(b: var CBlockLog; context: CBContext; input: CBVar): CBVar =
    if input.valueType == ContextVar:
      let ctxVal = context.contextVariable(input.stringValue)[]
      emitc("LOG(INFO) << ", `ctxVal`, ";")
    else:
      emitc("LOG(INFO) << ", `input`, ";")
    input

  chainblock CBlockLog, "Log"

# Msg - a debug log log
when true:
  type
    CBlockMsg* = object
      msg*: CBVar

  template destroy*(b: CBlockMsg) = `~quickcopy` b.msg
  template inputTypes*(b: CBlockMsg): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBlockMsg): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBlockMsg): CBParametersInfo = *@[(cs"Message", { String })]
  template setParam*(b: CBlockMsg; index: int; val: CBVar) =
    `~quickcopy` b.msg
    var inval = val
    quickcopy(b.msg, inval)
  template getParam*(b: CBlockMsg; index: int): CBVar = b.msg
  template activate*(b: CBlockMsg; context: CBContext; input: CBVar): CBVar =
    logs($b.msg)
    input

  chainblock CBlockMsg, "Msg"

# Sleep (coroutine)
when true:
  type
    CBlockSleep* = object
      # INLINE BLOCK, CORE STUB PRESENT
      time: float64

  template inputTypes*(b: CBlockSleep): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBlockSleep): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBlockSleep): CBParametersInfo = *@[(cs"Time", { Float })]
  template setParam*(b: CBlockSleep; index: int; val: CBVar) = b.time = val.floatValue
  template getParam*(b: CBlockSleep; index: int): CBVar = b.time.CBVar
  template activate*(b: CBlockSleep; context: CBContext; input: CBVar): CBVar =
    # THIS CODE WON'T BE EXECUTED
    # THIS BLOCK IS OPTIMIZED INLINE IN THE C++ CORE
    pause(b.time)
    input

  chainblock CBlockSleep, "Sleep"

# When - a filter block, that let's inputs pass only when the condition is true
when true:
  type
    CBWhen* = object
      acceptedValues*: CBVar
      isregex*: bool
      all*: bool
  
  template destroy*(b: CBWhen) = `~quickcopy` b.acceptedValues
  template inputTypes*(b: CBWhen): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBWhen): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBWhen): CBParametersInfo = 
    *@[
      (cs"Accept", (AllIntTypes + AllFloatTypes + { String, Color }, true #[seq]#), true #[context]#),
      (cs"IsRegex", ({ Bool }, false #[seq]#), false #[context]#),
      (cs"All", ({ Bool }, false #[seq]#), false #[context]#) # must match all of the accepted
    ]
  template setParam*(b: CBWhen; index: int; val: CBVar) =
    case index
    of 0:
      `~quickcopy` b.acceptedValues
      if val.valueType == Seq:
        var inval = val
        quickcopy(b.acceptedValues, inval)
      else:
        var
          inseq = ~@[val]
          inval: CBVar = inseq
        quickcopy(b.acceptedValues, inval)
    of 1:
      b.isregex = val.boolValue
    of 2:
      b.all = val.boolValue
    else:
      assert(false)
  template getParam*(b: CBWhen; index: int): CBVar =
    case index
    of 0:
        b.acceptedValues
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
    for accepted in b.acceptedValues.seqValue:
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
    if b.all and matches == b.acceptedValues.seqValue.len:
      res = input
    res

  chainblock CBWhen, "When", "":
    withChain regexMatchTest:
      Const """j["hello"] ="""
      When:
        Accept: """j\[\".*\"\]\s="""
        IsRegex: true
      Msg "Yes! Regex!"
    
    regexMatchTest.start()
    doAssert regexMatchTest.get() == """j["hello"] =""".CBVar
    destroy regexMatchTest

    withChain regexWontMatchTest:
      Const """j["hello] ="""
      When:
        Accept: """j\[\".*\"\]\s="""
        IsRegex: true
      Msg "Yes! Regex!"
    
    regexWontMatchTest.start()
    doAssert regexWontMatchTest.get() == RestartChain
    destroy regexWontMatchTest

# WhenNot - a filter block, that let's inputs pass only when the condition is false
when true:
  type
    CBWhenNot* = object
      acceptedValues*: CBVar
      isregex*: bool
      all*: bool
  
  template destroy*(b: CBWhenNot) = `~quickcopy` b.acceptedValues
  template inputTypes*(b: CBWhenNot): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBWhenNot): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBWhenNot): CBParametersInfo = 
    *@[
      (cs"Reject", (AllIntTypes + AllFloatTypes + { String, Color }, true #[seq]#), true #[context]#),
      (cs"IsRegex", ({ Bool }, false #[seq]#), false #[context]#),
      (cs"All", ({ Bool }, false #[seq]#), false #[context]#) # must match all of the accepted
    ]
  template setParam*(b: CBWhenNot; index: int; val: CBVar) =
    case index
    of 0:
      `~quickcopy` b.acceptedValues
      if val.valueType == Seq:
        var inval = val
        quickcopy(b.acceptedValues, inval)
      else:
        var
          inseq = ~@[val]
          inval: CBVar = inseq
        quickcopy(b.acceptedValues, inval)
    of 1:
      b.isregex = val.boolValue
    of 2:
      b.all = val.boolValue
    else:
      assert(false)
  template getParam*(b: CBWhenNot; index: int): CBVar =
    case index
    of 0:
        b.acceptedValues
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
    for accepted in b.acceptedValues.seqValue:
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
    if b.all and matches == b.acceptedValues.seqValue.len:
      res = RestartChain
    res

  chainblock CBWhenNot, "WhenNot", "":
    withChain regexMatchNotTest:
      Const """j["hello] ="""
      WhenNot:
        Reject: """j\[\".*\"\]\s="""
        IsRegex: true
      Msg "No! Regex!"
    
    regexMatchNotTest.start()
    doAssert regexMatchNotTest.get() == """j["hello] =""".CBVar
    destroy regexMatchNotTest

# If
when true:
  const
    BoolOpCC* = toFourCC("bool")
  
  var
    boolOpLabelsSeq = *@[cs"Equal", cs"More", cs"Less", cs"MoreEqual", cs"LessEqual"]
    boolOpLabels = boolOpLabelsSeq.toCBStrings()
  
  let
    boolOpInfo = CBTypeInfo(basicType: Enum, enumVendorId: FragCC.int32, enumTypeId: BoolOpCC.int32)
  
  registerEnumType(FragCC, BoolOpCC, CBEnumInfo(name: "Boolean operation", labels: boolOpLabels))

  type
    CBBoolOp* = enum
      # INLINE BLOCK, CORE STUB PRESENT
      Equal
      More
      Less
      MoreEqual
      LessEqual

  type
    CBlockIf* = object
      # INLINE BLOCK, CORE STUB PRESENT
      op: uint8
      match: CBVar
      matchCtx: ptr CBVar
      trueBlocks: CBSeq
      falseBlocks: CBSeq
      passthrough: bool      
  
  template setup*(b: CBlockIf) = 
    initSeq(b.trueBlocks)
    initSeq(b.falseBlocks)
  template destroy*(b: CBlockIf) =
    for blk in b.trueBlocks.mitems:
      blk.blockValue.cleanup(blk.blockValue)
      blk.blockValue.destroy(blk.blockValue)
    freeSeq(b.trueBlocks)
    for blk in b.falseBlocks.mitems:
      blk.blockValue.cleanup(blk.blockValue)
      blk.blockValue.destroy(blk.blockValue)
    freeSeq(b.falseBlocks)
  template cleanup*(b: CBlockIf) =
    for blk in b.trueBlocks.mitems:
      blk.blockValue.cleanup(blk.blockValue)
    for blk in b.falseBlocks.mitems:
      blk.blockValue.cleanup(blk.blockValue)
  template destroy*(b: CBlockIf) = `~quickcopy` b.match
  template inputTypes*(b: CBlockIf): CBTypesInfo = (AllIntTypes + AllFloatTypes + { String, Color }, true #[seq]#)
  template outputTypes*(b: CBlockIf): CBTypesInfo = (AllIntTypes + AllFloatTypes + { String, Color }, true #[seq]#)
  template parameters*(b: CBlockIf): CBParametersInfo =
    *@[
      (cs"Operator", toCBTypesInfo(*@[boolOpInfo]), false),
      (cs"Operand", (AllIntTypes + AllFloatTypes + { String, Color }, true #[seq]#).toCBTypesInfo(), true #[context]#),
      (cs"True", ({ Block, None }, true).toCBTypesInfo(), false),
      (cs"False", ({ Block, None }, true).toCBTypesInfo(), false),
      (cs"Passthrough", ({ Bool }, false).toCBTypesInfo(), false)
    ]
  template setParam*(b: CBlockIf; index: int; val: CBVar) =
    case index
    of 0:
      b.op = val.enumValue.uint8
    of 1:
      `~quickcopy` b.match
      var inval = val
      quickcopy(b.match, inval)
      b.matchCtx = nil
    of 2:
      for blk in b.trueBlocks.mitems:
        blk.blockValue.cleanup(blk.blockValue)
        blk.blockValue.destroy(blk.blockValue)
      b.trueBlocks.clear()

      case val.valueType
      of Block:
        b.trueBlocks.push(val.blockValue)
      of Seq:
        for blk in val.seqValue:
          b.trueBlocks.push(blk.blockValue)
      else:
        discard
    of 3:
      for blk in b.falseBlocks.mitems:
        blk.blockValue.cleanup(blk.blockValue)
        blk.blockValue.destroy(blk.blockValue)
      b.falseBlocks.clear()

      case val.valueType
      of Block:
        b.falseBlocks.push(val.blockValue)
      of Seq:
        for blk in val.seqValue:
          b.falseBlocks.push(blk.blockValue)
      else:
        discard
    of 4:
      b.passthrough = val.boolValue
    else:
      assert(false)
  template getParam*(b: CBlockIf; index: int): CBVar =
    case index
    of 0:
      CBVar(valueType: Enum, payload: CBVarPayload(enumValue: b.op.CBEnum, enumVendorId: FragCC.int32, enumTypeId: BoolOpCC.int32))
    of 1:
      b.match
    of 2:
      b.trueBlocks
    of 3:
      b.falseBlocks
    of 4:
      b.passthrough
    else:
      Empty
  proc inferTypes*(b: var CBlockIf; inputType: CBTypeInfo; consumables: CBExposedTypesInfo): CBTypeInfo =
    if b.passthrough:
      return inputType
    else:
      # We need to validate the sub chain to figure it out!
      var
        failed = false
        trueChain: CBRuntimeBlocks = nil
        falseChain: CBRuntimeBlocks = nil
      for blk in b.trueBlocks.mitems: trueChain.push(blk.blockValue)
      for blk in b.falseBlocks.mitems: falseChain.push(blk.blockValue)
      
      let
        trueResult = validateConnections(
          trueChain,
          proc(blk: ptr CBRuntimeBlock; error: cstring; nonfatalWarning: bool; userData: pointer) {.cdecl.} =
            var failedPtr = cast[ptr bool](userData)
            if not nonfatalWarning:
              failedPtr[] = true,
          addr failed,
          inputType
        )
        falseResult = validateConnections(
          falseChain,
          proc(blk: ptr CBRuntimeBlock; error: cstring; nonfatalWarning: bool; userData: pointer) {.cdecl.} =
            var failedPtr = cast[ptr bool](userData)
            if not nonfatalWarning:
              failedPtr[] = true,
          addr failed,
          inputType
        )
      
      freeSeq(trueChain)
      freeSeq(falseChain)
      
      if not failed and trueResult.basicType == falseResult.basicType: # TODO add better strict matching...
        return trueResult
      else:
        return None
  template activate*(b: CBlockIf; context: CBContext; input: CBVar): CBVar =
    if b.match.valueType == ContextVar and b.matchCtx == nil:
      b.matchCtx = context.contextVariable(b.match.stringValue)

    # PARTIALLY HANDLED INLINE!    
    let
      match = if b.match.valueType == ContextVar: b.matchCtx[] else: b.match
    var
      res = false
    template operation(operator: untyped): untyped =
      if operator(input, match):
        var output = Empty
        for blk in b.trueBlocks.mitems:
          var activationInput = if output.valueType == None: input else: output
          activateBlock(blk.blockValue, context, activationInput, output)
        
        if context.aborted:
          # We need to check and propagate abort signal as first priority
          StopChain
        elif b.passthrough:
          input
        else:
          output
      else:
        var output = Empty
        for blk in b.falseBlocks.mitems:
          var activationInput = if output.valueType == None: input else: output
          activateBlock(blk.blockValue, context, activationInput, output)
        
        if context.aborted:
          # We need to check and propagate abort signal as first priority
          StopChain
        elif b.passthrough:
          input
        else:
          output
    
    case b.op.CBBoolOp
    of Equal: res = operation(`==`)
    of More: res = operation(`>`)
    of Less: res = operation(`<`)
    of MoreEqual: res = operation(`>=`)
    of LessEqual: res = operation(`<=`)
    res

  chainblock CBlockIf, "If", "":
    block:
      var constTrue = createBlock("Const")
      constTrue[].setParam(constTrue, 0, true)
      var constFalse = createBlock("Const")
      constFalse[].setParam(constFalse, 0, false)
      
      withChain ifTest:
        Const 2.0
        If:
          Operator: CBVar(valueType: Enum, payload: CBVarPayload(enumValue: MoreEqual.CBEnum, enumVendorId: FragCC.int32, enumTypeId: BoolOpCC.int32))
          Operand: 1.5
          True: constTrue
          False: constFalse
      
      ifTest.start()
      doAssert ifTest.get() == true
    
    block:
      var constTrue = createBlock("Const")
      constTrue[].setParam(constTrue, 0, true)
      var constFalse = createBlock("Const")
      constFalse[].setParam(constFalse, 0, false)

      withChain ifTest:
        Const 2.0
        If:
          Operator: CBVar(valueType: Enum, payload: CBVarPayload(enumValue: MoreEqual.CBEnum, enumVendorId: FragCC.int32, enumTypeId: BoolOpCC.int32))
          Operand: 1.5
          True: constTrue
          False: constFalse
          Passthrough: true
      
      ifTest.start()
      doAssert ifTest.get() == 2.0

# Repeat - repeats the Chain parameter Times n
when true:
  type
    CBRepeat* = object
      # INLINE BLOCK, CORE STUB PRESENT
      forever: bool
      times: int32
      blocks: CBSeq
  
  template setup*(b: CBRepeat) = 
    initSeq(b.blocks)
  template destroy*(b: CBRepeat) =
    for blk in b.blocks.mitems:
      blk.blockValue.cleanup(blk.blockValue)
      blk.blockValue.destroy(blk.blockValue)
    freeSeq(b.blocks)
  template cleanup*(b: CBRepeat) =
    for blk in b.blocks.mitems:
      blk.blockValue.cleanup(blk.blockValue)
  template inputTypes*(b: CBRepeat): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBRepeat): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBRepeat): CBParametersInfo =
    *@[
      (cs"Action", ({ Block, None }, true).toCBTypesInfo(), false),
      (cs"Times", ({ Int }).toCBTypesInfo(), false),
      (cs"Forever", ({ Bool }).toCBTypesInfo(), false),
    ]
  template setParam*(b: CBRepeat; index: int; val: CBVar) =
    case index
    of 0:
      for blk in b.blocks.mitems:
        blk.blockValue.cleanup(blk.blockValue)
        blk.blockValue.destroy(blk.blockValue)
      b.blocks.clear()

      case val.valueType
      of Block:
        b.blocks.push(val.blockValue)
      of Seq:
        for blk in val.seqValue:
          b.blocks.push(blk.blockValue)
      else:
        discard
    of 1:
      b.times = val.intValue.int32
    of 2:
      b.forever = val.boolValue  
    else:
      assert(false)
  proc getParam*(b: CBRepeat; index: int): CBVar {.inline.} =
    case index
    of 0:
      b.blocks.CBVar
    of 1:
      b.times.int64.CBVar
    of 2:
      b.forever.CBVar
    else:
      Empty
  template activate*(b: var CBRepeat; context: CBContext; input: CBVar): CBVar =
    # THIS CODE WON'T BE EXECUTED
    # THIS BLOCK IS OPTIMIZED INLINE IN THE C++ CORE
    var
      res = input
      repeats = if b.forever: 1 else: b.times
    while repeats > 0:
      var output = Empty
      for blk in b.blocks.mitems:
        var activationInput = if output.valueType == None: input else: output
        activateBlock(blk.blockValue, context, activationInput, output)
      
      if context.aborted:
        # We need to check and propagate abort signal as first priority
        res = StopChain
      # rest we ignore
      
      if not b.forever:
        dec repeats
    
    res

  chainblock CBRepeat, "Repeat", "":
    var repLog = createBlock("Log")
    withChain testRepeat:
      Const "Repeating..."
      Repeat:
        Times: 5
        Action: repLog
    
    testRepeat.start()
    testRepeat.stop()
    destroy testRepeat

# ToString - converts the input to a string
when true:
  type
    CBToString* = object
      cachedStr*: GbString
  
  template inputTypes*(b: CBToString): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBToString): CBTypesInfo = { String }
  template activate*(b: var CBToString; context: CBContext; input: CBVar): CBVar =
    b.cachedStr.clear()
    if input.valueType == ContextVar:
      b.cachedStr &= $(context.contextVariable(input.stringValue)[])
    else:
      b.cachedStr &= $input
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
      for y in 0..<input.imageValue.height.int:
        for x in 0..<input.imageValue.width.int:
          var storage: array[4, uint8]
          input.imageValue.getPixel(x, y, storage, 0, input.imageValue.channels.int - 1)
          case input.imageValue.channels
          of 1:
            b.cachedSeq.push storage[0].float64.CBVar
          of 2:
            b.cachedSeq.push (storage[0].float64, storage[1].float64).CBVar
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
      w*,h*,c*: int
      imgCache*: Image[uint8]

  template setup*(b: CBToImage) =
    b.w = 256
    b.h = 256
    b.c = 3
  
  template destroy*(b: CBToImage) =
    if b.imgCache.data != nil:
      dealloc(b.imgCache.data)
      b.imgCache.data = nil
  
  template inputTypes*(b: CBToImage): CBTypesInfo = ({ Float, Float2, Float3, Float4, Int, Int2, Int3, Int4, Color }, true)
  template outputTypes*(b: CBToImage): CBTypesInfo = { CBType.Image }
  template parameters*(b: CBToImage): CBParametersInfo =
    *@[
      (cs"Width", { Int }),
      (cs"Height", { Int }),
      (cs"Channels", { Bool })
    ]
  template setParam*(b: CBToImage; index: int; val: CBVar) =
    case index
    of 0:
      b.w = val.intValue.int
    of 1:
      b.h = val.intValue.int
    of 2:
      b.c = val.intValue.int
    else:
      assert(false)
  template getParam*(b: CBToImage; index: int): CBVar =
    case index
    of 0:
      b.w.CBVar
    of 1:
      b.h.CBVar
    of 2:
      b.c.CBVar
    else:
      Empty
  template activate*(blk: var CBToImage; context: CBContext; input: CBVar): CBVar =
    if  blk.imgCache.data == nil or 
        blk.imgCache.width != blk.w or 
        blk.imgCache.height != blk.h or
        blk.imgCache.channels != blk.c:
      if blk.imgCache.data != nil:
        dealloc(blk.imgCache.data)
      
      blk.imgCache.width = blk.w
      blk.imgCache.height = blk.h
      blk.imgCache.channels = blk.c
      let size = blk.w * blk.h * blk.c
      blk.imgCache.data = cast[ptr UncheckedArray[uint8]](alloc0(size))
    
    let maxLen = blk.imgCache.width * blk.imgCache.height * blk.imgCache.channels

    if input.valueType != Seq:
      halt(context, "ToImage always expects values in a sequence.")
    else:
      var index = 0
      for i in 0..min(maxLen - 1, input.seqValue.len - 1):
        let val = input.seqValue[i]
        case val.valueType
        of Float:
          blk.imgCache.data[index] = val.floatValue.clamp(0, 255).uint8
          inc index
        of Float2:
          for i in 0..blk.imgCache.channels:
            blk.imgCache.data[index] = val.float2Value[i].clamp(0, 255).uint8
            inc index
        of Float3:
          for i in 0..blk.imgCache.channels:
            blk.imgCache.data[index] = val.float3Value[i].clamp(0, 255).uint8
            inc index
        of Float4:
          for i in 0..blk.imgCache.channels:
            blk.imgCache.data[index] = val.float4Value[i].clamp(0, 255).uint8
            inc index
        of Int:
          blk.imgCache.data[index] = val.intValue.clamp(0, 255).uint8
          inc index
        of Int2:
          for i in 0..blk.imgCache.channels:
            blk.imgCache.data[index] = val.int2Value[i].clamp(0, 255).uint8
            inc index
        of Int3:
          for i in 0..blk.imgCache.channels:
            blk.imgCache.data[index] = val.int3Value[i].clamp(0, 255).uint8
            inc index
        of Int4:
          for i in 0..blk.imgCache.channels:
            blk.imgCache.data[index] = val.int4Value[i].clamp(0, 255).uint8
            inc index
        of Color:
          blk.imgCache.data[index] = val.colorValue.r
          inc index
          if blk.imgCache.channels > 1:
            blk.imgCache.data[index] = val.colorValue.g
            inc index
          if blk.imgCache.channels > 2:
            blk.imgCache.data[index] = val.colorValue.b
            inc index
          if blk.imgCache.channels > 3:
            blk.imgCache.data[index] = val.colorValue.a
            inc index
        else:
          assert(false)

      CBVar(valueType: CBType.Image, payload: CBVarPayload(imageValue: blk.imgCache))

  chainblock CBToImage, "ToImage"
