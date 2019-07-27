proc `/`*(a,b: CBVar): CBVar {.inline.} =
  if a.valueType != b.valueType: throwCBException(astToStr(`/`) & " Not supported between different types " & $a.valueType & " and " & $b.valueType)
  case a.valueType
  of None, Any, EndOfBlittableTypes: throwCBException("None, Any, End, Stop " & astToStr(`/`) & " Not supported")
  of Object: throwCBException("Object " & astToStr(`/`) & " Not supported")
  of Bool: throwCBException("Bool " & astToStr(`/`) & " Not supported")
  of Int: return `div`(a.intValue, b.intValue).toCBVar()
  of Int2: return `/`(a.int2Value.toCpp, b.int2Value.toCpp).to(CBInt2).toCBVar()
  of Int3: return `/`(a.int3Value.toCpp, b.int3Value.toCpp).to(CBInt3).toCBVar()
  of Int4: return `/`(a.int4Value.toCpp, b.int4Value.toCpp).to(CBInt4).toCBVar()
  of Int8: return `/`(a.int8Value.toCpp, b.int8Value.toCpp).to(CBInt8).toCBVar()
  of Int16: return `/`(a.int16Value.toCpp, b.int16Value.toCpp).to(CBInt16).toCBVar()
  of Float: return `/`(a.floatValue, b.floatValue).toCBVar()
  of Float2: return `/`(a.float2Value.toCpp, b.float2Value.toCpp).to(CBFloat2).toCBVar()
  of Float3: return `/`(a.float3Value.toCpp, b.float3Value.toCpp).to(CBFloat3).toCBVar()
  of Float4: return `/`(a.float4Value.toCpp, b.float4Value.toCpp).to(CBFloat4).toCBVar()
  of String: throwCBException("String " & astToStr(`/`) & " Not supported")
  of ContextVar: throwCBException("ContextVar " & astToStr(`/`) & " Not supported")
  of Color: return (
      a.colorValue.r div b.colorValue.r, 
      a.colorValue.g div b.colorValue.g,
      a.colorValue.b div b.colorValue.b,
      a.colorValue.a div b.colorValue.a
    )
  of CBType.Image: throwCBException("Image " & astToStr(`/`) & " Not supported")
  of Seq: throwCBException("Seq " & astToStr(`/`) & " Not supported")
  of Table: throwCBException("Table " & astToStr(`/`) & " Not supported")
  of Chain: throwCBException("Chain " & astToStr(`/`) & " Not supported")
  of Enum: throwCBException("Enum " & astToStr(`/`) & " Not supported")
  of Block: throwCBException("Block " & astToStr(`/`) & " Not supported")

template mathBinaryOp(op: untyped): untyped =
  proc op*(a,b: CBVar): CBVar {.inline.} =
    if a.valueType != b.valueType: throwCBException(astToStr(op) & " Not supported between different types " & $a.valueType & " and " & $b.valueType)
    case a.valueType
    of None, Any, EndOfBlittableTypes: throwCBException("None, Any, End, Stop " & astToStr(op) & " Not supported")
    of Object: throwCBException("Object " & astToStr(op) & " Not supported")
    of Bool: throwCBException("Bool " & astToStr(op) & " Not supported")
    of Int:  return op(a.intValue, b.intValue).toCBVar()
    of Int2: return op(a.int2Value.toCpp, b.int2Value.toCpp).to(CBInt2).toCBVar()
    of Int3: return op(a.int3Value.toCpp, b.int3Value.toCpp).to(CBInt3).toCBVar()
    of Int4: return op(a.int4Value.toCpp, b.int4Value.toCpp).to(CBInt4).toCBVar()
    of Int8: return op(a.int8Value.toCpp, b.int8Value.toCpp).to(CBInt8).toCBVar()
    of Int16: return op(a.int16Value.toCpp, b.int16Value.toCpp).to(CBInt16).toCBVar()
    of Float:  return op(a.floatValue, b.floatValue).toCBVar()
    of Float2: return op(a.float2Value.toCpp, b.float2Value.toCpp).to(CBFloat2).toCBVar()
    of Float3: return op(a.float3Value.toCpp, b.float3Value.toCpp).to(CBFloat3).toCBVar()
    of Float4: return op(a.float4Value.toCpp, b.float4Value.toCpp).to(CBFloat4).toCBVar()
    of String: throwCBException("String " & astToStr(op) & " Not supported")
    of ContextVar: throwCBException("ContextVar " & astToStr(op) & " Not supported")
    of Color: return (
        op(a.colorValue.r, b.colorValue.r), 
        op(a.colorValue.g, b.colorValue.g),
        op(a.colorValue.b, b.colorValue.b),
        op(a.colorValue.a, b.colorValue.a)
      )
    of CBType.Image: throwCBException("Image " & astToStr(op) & " Not supported")
    of Seq: throwCBException("Seq " & astToStr(op) & " Not supported")
    of Table: throwCBException("Table " & astToStr(op) & " Not supported")
    of Chain: throwCBException("Chain " & astToStr(op) & " Not supported")
    of Enum: throwCBException("Enum " & astToStr(op) & " Not supported")
    of Block: throwCBException("Block " & astToStr(op) & " Not supported")

mathBinaryOp(`+`)
mathBinaryOp(`-`)
mathBinaryOp(`*`)

template mathIntBinaryOp(op: untyped): untyped =
  proc op*(a,b: CBVar): CBVar {.inline.} =
    if a.valueType != b.valueType: throwCBException(astToStr(op) & " Not supported between different types " & $a.valueType & " and " & $b.valueType)
    case a.valueType
    of None, Any, EndOfBlittableTypes: throwCBException("None, Any, End, Stop " & astToStr(op) & " Not supported")
    of Object: throwCBException("Object " & astToStr(op) & " Not supported")
    of Bool: throwCBException("Bool " & astToStr(op) & " Not supported")
    of Int:  return op(a.intValue, b.intValue).int64.toCBVar()
    of Int2: return op(a.int2Value.toCpp, b.int2Value.toCpp).to(CBInt2).toCBVar()
    of Int3: return op(a.int3Value.toCpp, b.int3Value.toCpp).to(CBInt3).toCBVar()
    of Int4: return op(a.int4Value.toCpp, b.int4Value.toCpp).to(CBInt4).toCBVar()
    of Int8: return op(a.int8Value.toCpp, b.int8Value.toCpp).to(CBInt8).toCBVar()
    of Int16: return op(a.int16Value.toCpp, b.int16Value.toCpp).to(CBInt16).toCBVar()
    of Float:  throwCBException("Float " & astToStr(op) & " Not supported")
    of Float2: throwCBException("Float2 " & astToStr(op) & " Not supported")
    of Float3: throwCBException("Float3 " & astToStr(op) & " Not supported")
    of Float4: throwCBException("Float4 " & astToStr(op) & " Not supported")
    of String: throwCBException("String " & astToStr(op) & " Not supported")
    of ContextVar: throwCBException("ContextVar " & astToStr(op) & " Not supported")
    of Color: return (
        op(a.colorValue.r, b.colorValue.r), 
        op(a.colorValue.g, b.colorValue.g),
        op(a.colorValue.b, b.colorValue.b),
        op(a.colorValue.a, b.colorValue.a)
      )
    of CBType.Image: throwCBException("Image " & astToStr(op) & " Not supported")
    of Seq: throwCBException("Seq " & astToStr(op) & " Not supported")
    of Table: throwCBException("Table " & astToStr(op) & " Not supported")
    of Chain: throwCBException("Chain " & astToStr(op) & " Not supported")
    of Enum: throwCBException("Enum " & astToStr(op) & " Not supported")
    of Block: throwCBException("Block " & astToStr(op) & " Not supported")

mathIntBinaryOp(`xor`)
mathIntBinaryOp(`and`)
mathIntBinaryOp(`or`)
mathIntBinaryOp(`mod`)
mathIntBinaryOp(`shl`)
mathIntBinaryOp(`shr`)

proc `==`*(a: CBString, b: string): bool {.inline.} = a.cstring == b.cstring
proc `<=`*(a: CBString, b: string): bool {.inline.} = a.cstring <= b.cstring
proc `<`*(a: CBString, b: string): bool {.inline.} = a.cstring < b.cstring
proc `==`*(a: string, b: CBString): bool {.inline.} = a.cstring == b.cstring
proc `<=`*(a: string, b: CBString): bool {.inline.} = a.cstring <= b.cstring
proc `<`*(a: string, b: CBString): bool {.inline.} = a.cstring < b.cstring

proc `<`*(a,b: CBVar): bool {.inline.} =
  if a.valueType != b.valueType: throwCBException("`<` Not supported between different types " & $a.valueType & " and " & $b.valueType)
  case a.valueType
  of None, Any, EndOfBlittableTypes: return true
  of Bool: return a.boolValue < b.boolValue
  of Int: return a.intValue < b.intValue
  of Int2: return (a.int2Value.toCpp < b.int2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Int3: return (a.int3Value.toCpp < b.int3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Int4: return (a.int4Value.toCpp < b.int4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of Int8: return (a.int8Value.toCpp < b.int8Value.toCpp).to(CBInt8).elems.allit(it != 0)
  of Int16: return (a.int16Value.toCpp < b.int16Value.toCpp).to(CBInt16).elems.allit(it != 0)
  of Float: return a.floatValue < b.floatValue
  of Float2: return (a.float2Value.toCpp < b.float2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Float3: return (a.float3Value.toCpp < b.float3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Float4: return (a.float4Value.toCpp < b.float4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of String, ContextVar: return a.stringValue.string < b.stringValue.string
  of Color: throwCBException("Color `<` Not supported")
  of CBType.Image: throwCBException("Image `<` Not supported")
  of Seq: throwCBException("Seq `<` Not supported")
  of Table: throwCBException("Table `<` Not supported")
  of Chain: throwCBException("Chain `<` Not supported")
  of Enum: throwCBException("Enum `<` Not supported")
  of Object: throwCBException("Object `<` Not supported")
  of Block: throwCBException("Block `<` Not supported")

proc `<=`*(a,b: CBVar): bool {.inline.} =
  if a.valueType != b.valueType: throwCBException("`<=` Not supported between different types " & $a.valueType & " and " & $b.valueType)
  case a.valueType
  of None, Any, EndOfBlittableTypes: return true
  of Bool: return a.boolValue <= b.boolValue
  of Int: return a.intValue <= b.intValue
  of Int2: return (a.int2Value.toCpp <= b.int2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Int3: return (a.int3Value.toCpp <= b.int3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Int4: return (a.int4Value.toCpp <= b.int4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of Int8: return (a.int8Value.toCpp <= b.int8Value.toCpp).to(CBInt8).elems.allit(it != 0)
  of Int16: return (a.int16Value.toCpp <= b.int16Value.toCpp).to(CBInt16).elems.allit(it != 0)
  of Float: return a.floatValue <= b.floatValue
  of Float2: return (a.float2Value.toCpp <= b.float2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Float3: return (a.float3Value.toCpp <= b.float3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Float4: return (a.float4Value.toCpp <= b.float4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of String, ContextVar: return a.stringValue.string <= b.stringValue.string
  of Color: throwCBException("Color `<=` Not supported")
  of CBType.Image: throwCBException("Image `<=` Not supported")
  of Seq: throwCBException("Seq `<=` Not supported")
  of Table: throwCBException("Table `<=` Not supported")
  of Chain: throwCBException("Chain `<=` Not supported")
  of Enum: throwCBException("Enum `<=` Not supported")
  of Object: throwCBException("Object `<=` Not supported")
  of Block: throwCBException("Block `<=` Not supported")

template `>`*(a,b: CBVar): bool = b < a
template `>=`*(a,b: CBVar): bool = b <= a

proc `==`*(a,b: CBVar): bool {.inline.} =
  if a.valueType != b.valueType: return false
  case a.valueType
  of None, Any, EndOfBlittableTypes: return true
  of Bool: return a.boolValue == b.boolValue
  of Int: return a.intValue == b.intValue
  of Int2: return (a.int2Value.toCpp == b.int2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Int3: return (a.int3Value.toCpp == b.int3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Int4: return (a.int4Value.toCpp == b.int4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of Int8: return (a.int8Value.toCpp == b.int8Value.toCpp).to(CBInt8).elems.allit(it != 0)
  of Int16: return (a.int16Value.toCpp == b.int16Value.toCpp).to(CBInt16).elems.allit(it != 0)
  of Float: return a.floatValue == b.floatValue
  # On purpose converting to CBInt/s
  of Float2: return (a.float2Value.toCpp == b.float2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Float3: return (a.float3Value.toCpp == b.float3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Float4: return (a.float4Value.toCpp == b.float4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of String, ContextVar: return a.stringValue.cstring == b.stringValue.cstring
  of Color: return a.colorValue == b.colorValue
  of CBType.Image:
    if  a.imageValue.width != b.imageValue.width or
        a.imageValue.height != b.imageValue.height or
        a.imageValue.channels != b.imageValue.channels or
        (a.imageValue.data == nil) and (b.imageValue.data != nil) or
        (a.imageValue.data != nil) and (b.imageValue.data == nil): return false
    if a.imageValue.data == nil and b.imageValue.data == nil: return true
    return equalMem(a.imageValue.data, b.imageValue.data, a.imageValue.width * a.imageValue.height * a.imageValue.channels)
  of Seq: 
    if a.seqValue.len != b.seqValue.len: return false
    for i in 0..<a.seqValue.len:
      if a.seqValue[i] != b.seqValue[i]: return false
    return true
  of Table: 
    if a.tableValue.len != b.tableValue.len: return false
    for i in 0..<a.tableValue.len:
      if a.tableValue[i].key != b.tableValue[i].key: return false
      if a.tableValue[i].value != b.tableValue[i].value: return false
    return true
  of Chain: return a.chainValue == b.chainValue
  of Enum:  return a.enumValue.int32  == b.enumValue.int32 and a.enumVendorId == b.enumVendorId and a.enumTypeId == b.enumTypeId
  of Object:
    if  a.objectVendorId.int32 != b.objectVendorId.int32 or
        a.objectTypeId.int32 != b.objectTypeId.int32 or
        a.objectValue != b.objectValue: return false
    return true
  of Block: return a.blockValue.pointer == b.blockValue.pointer

proc `$`*(a: CBVar): string {.inline.} =
  case a.valueType
  of None, Any, EndOfBlittableTypes: return $a.valueType
  of Bool: return $a.boolValue
  of Int: return $a.intValue
  of Int2: return $a.int2Value.elems
  of Int3: return $a.int3Value.elems
  of Int4: return $a.int4Value.elems
  of Int8: return $a.int8Value.elems
  of Int16: return $a.int16Value.elems
  of Float: return $a.floatValue
  of Float2: return $a.float2Value.elems
  of Float3: return $a.float3Value.elems
  of Float4: return $a.float4Value.elems
  of String, ContextVar:
    if a.stringValue.pointer != nil:
      return a.stringValue
    else:
      return ""
  of Color: return $a.colorValue
  of CBType.Image: return $a.imageValue
  of Seq:
    result = "["
    let len = a.seqValue.len
    var idx = 0
    for item in a.seqValue.mitems:
      if idx == len - 1:
        result &= $item
      else:
        result &= $item & ", "
      inc idx
    result &= "]"
  of Table:
    result = "{"
    let len = a.tableValue.len
    var idx = 0
    for item in a.tableValue.mitems:
      if idx == len - 1:
        result &= $item.key & ": " & $item.value
      else:
        result &= $item.key & ": " & $item.value & ", "
      inc idx
    result &= "}"
  of Chain: return $a.chainValue
  of Enum:  return $a.enumValue.int32
  of Object:
    return "Object - addr: 0x" & $cast[int](a.objectValue).toHex() & 
    " vendorId: 0x" & a.objectVendorId.toHex() & " typeId: 0x" & a.objectTypeId.toHex() 
  of Block: return "Block: " & $a.blockValue[].name(a.blockValue)

proc `==`*(a: CBVar, b: CBVarConst): bool {.inline.} = a == b.value
proc `<=`*(a: CBVar, b: CBVarConst): bool {.inline.} = a <= b.value
proc `<`*(a: CBVar, b: CBVarConst): bool {.inline.} = a < b.value
proc `==`*(a: CBVarConst, b: CBVar): bool {.inline.} = a.value == b
proc `<=`*(a: CBVarConst, b: CBVar): bool {.inline.} = a.value <= b
proc `<`*(a: CBVarConst, b: CBVar): bool {.inline.} = a.value < b
template `>`*(a: CBVar; b: CBVarConst): bool = b < a
template `>=`*(a: CBVar; b: CBVarConst): bool = b <= a
template `>`*(a: CBVarConst; b: CBVar): bool = b < a
template `>=`*(a: CBVarConst; b: CBVar): bool = b <= a
template `$`*(a: CBVarConst): string = $a.value