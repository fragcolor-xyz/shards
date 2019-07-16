import ../nim/chainblocks
import nimpy
import nimpy/[py_types, py_lib]
import tables, sets

proc var2Py*(input: CBVar; inputSeqCache: var seq[PPyObject]; inputTableCache: var Table[string, PPyObject]): PPyObject =
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
  of Enum: result = toPyObjectArgument (input.enumValue.int32, input.enumVendorId, input.enumTypeId)
  of Seq: # Will flatten the seq anyway!
    inputSeqCache.setLen(0)
    for item in input.seqValue.mitems:
      inputSeqCache.add var2Py(item, inputSeqCache, inputTableCache)
    result = toPyObjectArgument inputSeqCache
  of CBType.Table:
    inputTableCache.clear()
    for item in input.tableValue.mitems:
      inputTableCache.add($item.key, var2Py(item.value, inputSeqCache,inputTableCache))
    result = toPyObjectArgument inputTableCache
  of Chain: result = py_lib.pyLib.PyCapsule_New(cast[pointer](input.chainValue), nil, nil)
  of Block: assert(false) # TODO

proc py2Var*(input: PyObject; stringStorage: var string; seqStorage: var CBSeq; tableStorage: var CBTable; outputTableKeyCache: var HashSet[cstring]): CBVar =
  let
    tupRes = input.to(tuple[valueType: int; value: PyObject])
    valueType = tupRes.valueType.CBType
  result.valueType = valueType
  case valueType
  of None, Any, ContextVar: result = Empty
  of Object:
    let
      objValue = tupRes.value.to(tuple[capsule: PPyObject; vendor, typeid: int32])
    result.objectValue = py_lib.pyLib.PyCapsule_GetPointer(objValue.capsule, nil)
    result.objectVendorId = objValue.vendor
    result.objectTypeId = objValue.typeid
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
    stringStorage.setLen(0)
    stringStorage &= tupRes.value.to(string)
    result = stringStorage
  of Color: result = tupRes.value.to(tuple[r, g, b, a: uint8])
  of Image:
    let
      img = tupRes.value.to(tuple[w,h,c: int; data: PPyObject])
    result.imageValue.width = img.w.int32
    result.imageValue.height = img.h.int32
    result.imageValue.channels = img.c.int32
    result.imageValue.data = cast[ptr UncheckedArray[uint8]](py_lib.pyLib.PyCapsule_GetPointer(img.data, nil))
  of Enum:
    let
      enumTup = tupRes.value.to(tuple[enumVal, vendor, typeid: int32])
    result.enumValue = enumTup.enumVal.CBEnum
    result.enumVendorId = enumTup.vendor
    result.enumTypeId = enumTup.typeId
  of Seq:
    var pyseq = tupRes.value.to(seq[PyObject])   
    for pyvar in pyseq.mitems:
      let sub = py2Var(pyvar, stringStorage, seqStorage, tableStorage, outputTableKeyCache)
      seqStorage.push(sub)
    result = seqStorage
  of CBType.Table:
    # keep a list of all current keys, later remove all that disappeared!
    outputTableKeyCache.clear()
    for item in tableStorage.mitems:
      outputTableKeyCache.incl(item.key)

    var pytab = tupRes.value.to(tables.Table[string, PyObject])
    for k, v in pytab.mpairs:
      let sub = py2Var(v, stringStorage, seqStorage, tableStorage, outputTableKeyCache)
      tableStorage.incl(k.cstring, sub)
      outputTableKeyCache.excl(k.cstring)
    
    # Remove from the cache anything that has disappeared
    for key in outputTableKeyCache:
      outputTableKeyCache.excl(key)

    result = tableStorage
  of Chain: result = cast[ptr CBChainPtr](py_lib.pyLib.PyCapsule_GetPointer(tupRes.value.to(PPyObject), nil))
  of Block: assert(false) # TODO