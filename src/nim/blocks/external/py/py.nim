import ../../../types
import ../../../chainblocks
import nimpy/py_types
import nimpy/py_lib
import nimpy
import dynlib, tables, os, times

type
  Scripting* = object

var
  py: PyObject
  pySys: PyObject

when true:
  type
    CBPython = object
      filename*: string
      stringStorage*: CBString
      pymod*: PyObject
      instance*: PyObject
      loaded*: bool
      seqStorage*: CBSeq
      pySuspendRes*: CBVar
  
  template cleanup*(b: CBPython) =
    b.instance = nil
    b.pymod = nil
    b.loaded = false
  template setup*(b: CBPython) =
    b.stringStorage = newString("")
    initSeq(b.seqStorage)
  template destroy*(b: CBPython) =
    b.stringStorage.freeString()
    freeSeq(b.seqStorage)
  template inputTypes*(b: CBPython): CBTypesInfo = ({ Any }, true)
  template outputTypes*(b: CBPython): CBTypesInfo = ({ Any }, true)
  template parameters*(b: CBPython): CBParametersInfo = @[("File", { String })]
  template setParam*(b: CBPython; index: int; val: CBVar) =
    cleanup(b) # force reload of python module
    b.filename = val.stringValue
  template getParam*(b: CBPython; index: int): CBVar =
    b.filename
  
  proc var2Py*(input: CBVar): PPyObject =
    case input.valueType
    of None, Any, ContextVar: result = newPyNone()
    of Object:
      result = toPyObjectArgument(
        (py_lib.pyLib.PyCapsule_New(cast[pointer](input.objectValue), nil, nil), input.objectVendorId, input.objectTypeId)
      )
    of Bool: result = toPyObjectArgument input.boolValue
    of Int: result = toPyObjectArgument input.intValue
    of Int2: result = toPyObjectArgument (input.int2Value[0], input.int2Value[1])
    of Int3: result = toPyObjectArgument (input.int3Value[0], input.int3Value[1], input.int3Value[2])
    of Int4: result = toPyObjectArgument (input.int4Value[0], input.int4Value[1], input.int4Value[2], input.int4Value[3])
    of Float: result = toPyObjectArgument input.floatValue
    of Float2: result = toPyObjectArgument (input.float2Value[0], input.float2Value[1])
    of Float3: result = toPyObjectArgument (input.float3Value[0], input.float3Value[1], input.float3Value[2])
    of Float4: result = toPyObjectArgument (input.float4Value[0], input.float4Value[1], input.float4Value[2], input.float4Value[3])
    of String: result = toPyObjectArgument input.stringValue.string
    of Color: result = toPyObjectArgument (input.colorValue.r, input.colorValue.g, input.colorValue.b, input.colorValue.a)
    of Image:
      result = toPyObjectArgument(
        (
          input.imageValue.width,
          input.imageValue.height,
          input.imageValue.channels,
          py_lib.pyLib.PyCapsule_New(cast[pointer](input.imageValue.data), nil, nil)
        )
      )
    of BoolOp: result = toPyObjectArgument input.boolOpValue.int
    of Seq:
      var pySeq = newSeq[PPyObject]()
      for item in input.seqValue.mitems:
        pySeq.add var2Py(item)
      result = toPyObjectArgument pySeq
    of Chain: result = py_lib.pyLib.PyCapsule_New(cast[pointer](input.chainValue), nil, nil)
  
  proc py2Var*(input: PyObject; blk: var CBPython): CBVar =
    let
      tupRes = input.to(tuple[valueType: int; value: PyObject])
      valueType = tupRes.valueType.CBType
    result.valueType = valueType
    case valueType
    of None, Any, ContextVar: result = Empty
    of Object:
      let
        objValue = tupRes.value.to(tuple[capsule: PPyObject; vendor, typeid: int])
      result.objectValue = py_lib.pyLib.PyCapsule_GetPointer(objValue.capsule, nil)
      result.objectVendorId = objValue.vendor.int32
      result.objectTypeId = objValue.typeid.int32
    of Bool: result = tupRes.value.to(bool)
    of Int: result = tupRes.value.to(int64)
    of Int2: result = tupRes.value.to(tuple[a, b: int64])
    of Int3: result = tupRes.value.to(tuple[a, b, c: int32])
    of Int4: result = tupRes.value.to(tuple[a, b, c, d: int32])
    of Float: result = tupRes.value.to(float64)
    of Float2: result = tupRes.value.to(tuple[a, b: float64])
    of Float3: result = tupRes.value.to(tuple[a, b, c: float32])
    of Float4: result = tupRes.value.to(tuple[a, b, c, d: float32])
    of String:
      blk.stringStorage = blk.stringStorage.setString(tupRes.value.to(string))
      result = blk.stringStorage
    of Color: result = tupRes.value.to(tuple[r, g, b, a: uint8])
    of Image:
      let
        img = tupRes.value.to(tuple[w,h,c: int; data: PPyObject])
      result.imageValue.width = img.w.int32
      result.imageValue.height = img.h.int32
      result.imageValue.channels = img.c.int32
      result.imageValue.data = cast[ptr UncheckedArray[uint8]](py_lib.pyLib.PyCapsule_GetPointer(img.data, nil))
    of BoolOp: result = tupRes.value.to(int).CBBoolOp
    of Seq:
      var pyseq = tupRes.value.to(seq[PyObject])   
      for pyvar in pyseq.items:
        let sub = py2Var(pyvar, blk)
        push(blk.seqStorage, sub)
    of Chain: result = cast[ptr CBChainPtr](py_lib.pyLib.PyCapsule_GetPointer(tupRes.value.to(PPyObject), nil))

  template activate*(blk: var CBPython; context: CBContext; input: CBVar): CBVar =
    var res = StopChain
    try:
      if py == nil or pySys == nil:
        py = pyBuiltinsModule()
        pySys = pyImport("sys")
        doAssert py != nil and pySys != nil
      
      if blk.instance == nil:
        # Create a empty object to attach to this block instance
        blk.instance = py.dict()
        blk.instance["suspend"] = proc(seconds: float64): bool =
          blk.pySuspendRes = suspend(seconds)
          if blk.pySuspendRes.chainState != Continue:
            return false
          return true
      
      if not blk.loaded and blk.filename != "" and fileExists(blk.filename):
        # Load, but might also be in memory!
        let (dir, name, _) = blk.filename.splitFile()
        pySys.path.append(dir).to(void)
        blk.pymod = pyImport(name)

        # Also actually force a reload, to support changes in the module during runtime
        let
          majorVer = pySys.version_info[0].to(int)
          minorVer = pySys.version_info[1].to(int)
        if majorVer < 3:
          py.reload(blk.pymod).to(void)
        elif majorVer == 3 and minorVer <= 4:
          let imp = pyImport("imp")
          imp.reload(blk.pymod).to(void)
        else:
          let imp = pyImport("importlib")
          imp.reload(blk.pymod).to(void)
              
        blk.loaded = true
          
      if blk.loaded:
          let
            pyinput = var2Py(input)
            pyres = blk.pymod.callMethod("activate", blk.instance, pyinput)
          if blk.pySuspendRes.chainState != Continue:
            res = blk.pySuspendRes
          else:
            blk.seqStorage.clear()
            res = py2Var(pyres, blk)
    except:
      context.setError(getCurrentExceptionMsg())
    res

  chainblock CBPython, "Py", "Scripting"

when false:
  import os
  
  putEnv("PYTHONHOME", "C:/ProgramData/Miniconda3")
  putEnv("PYTHONPATH", "C:/ProgramData/Miniconda3")
  
  chainblocks.init()
  
  Const "Hello"
  Scripting.Py "./pytest.py"
  Log()
  
  chainblocks.start(true)
  chainblocks.tick()
  chainblocks.stop()
  chainblocks.start(true)
  while true:
    chainblocks.tick()
    sleep 300