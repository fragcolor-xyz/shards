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
      (cs"Accept", (AllIntTypes + AllFloatTypes + { String, Color, Bool, ContextVar }, true #[seq]#)),
      (cs"IsRegex", ({ Bool }, false #[seq]#)),
      (cs"All", ({ Bool }, false #[seq]#))
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
          inval = val
          valCopy: CBVar
        quickcopy(valCopy, inval)
        var
          inseq = ~@[valCopy]
          sval: CBVar = inseq
        quickcopy(b.acceptedValues, sval)
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
      (cs"Reject", (AllIntTypes + AllFloatTypes + { String, Color, Bool, ContextVar }, true #[seq]#)),
      (cs"IsRegex", ({ Bool }, false #[seq]#)),
      (cs"All", ({ Bool }, false #[seq]#)) # must match all of the accepted
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
          inval = val
          valCopy: CBVar
        quickcopy(valCopy, inval)
        var
          inseq = ~@[valCopy]
          sval: CBVar = inseq
        quickcopy(b.acceptedValues, sval)
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
