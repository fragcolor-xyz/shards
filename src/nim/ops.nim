proc `/`*(a,b: CBVar): CBVar {.inline.} =
  if a.valueType != b.valueType: doAssert(false, astToStr(`/`) & " Not supported between different types " & $a.valueType & " and " & $b.valueType)
  case a.valueType
  of None, Any: throwCBException("None, Any, End, Stop " & astToStr(`/`) & " Not supported")
  of Object: throwCBException("Object " & astToStr(`/`) & " Not supported")
  of Bool: throwCBException("Bool " & astToStr(`/`) & " Not supported")
  of Int: return `div`(a.intValue, b.intValue).toCBVar()
  of Int2: return `/`(a.int2Value.toCpp, b.int2Value.toCpp).to(CBInt2).toCBVar()
  of Int3: return `/`(a.int3Value.toCpp, b.int3Value.toCpp).to(CBInt3).toCBVar()
  of Int4: return `/`(a.int4Value.toCpp, b.int4Value.toCpp).to(CBInt4).toCBVar()
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
  of Chain: throwCBException("Chain " & astToStr(`/`) & " Not supported")
  of Enum: throwCBException("Enum " & astToStr(`/`) & " Not supported")

template mathBinaryOp(op: untyped): untyped =
  proc op*(a,b: CBVar): CBVar {.inline.} =
    if a.valueType != b.valueType: doAssert(false, astToStr(op) & " Not supported between different types " & $a.valueType & " and " & $b.valueType)
    case a.valueType
    of None, Any: throwCBException("None, Any, End, Stop " & astToStr(op) & " Not supported")
    of Object: throwCBException("Object " & astToStr(op) & " Not supported")
    of Bool: throwCBException("Bool " & astToStr(op) & " Not supported")
    of Int:  return op(a.intValue, b.intValue).toCBVar()
    of Int2: return op(a.int2Value.toCpp, b.int2Value.toCpp).to(CBInt2).toCBVar()
    of Int3: return op(a.int3Value.toCpp, b.int3Value.toCpp).to(CBInt3).toCBVar()
    of Int4: return op(a.int4Value.toCpp, b.int4Value.toCpp).to(CBInt4).toCBVar()
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
    of Chain: throwCBException("Chain " & astToStr(op) & " Not supported")
    of Enum: throwCBException("Enum " & astToStr(op) & " Not supported")

template mathIntBinaryOp(op: untyped): untyped =
  proc op*(a,b: CBVar): CBVar {.inline.} =
    if a.valueType != b.valueType: doAssert(false, astToStr(op) & " Not supported between different types " & $a.valueType & " and " & $b.valueType)
    case a.valueType
    of None, Any: throwCBException("None, Any, End, Stop " & astToStr(op) & " Not supported")
    of Object: throwCBException("Object " & astToStr(op) & " Not supported")
    of Bool: throwCBException("Bool " & astToStr(op) & " Not supported")
    of Int:  return op(a.intValue, b.intValue).int64.toCBVar()
    of Int2: return op(a.int2Value.toCpp, b.int2Value.toCpp).to(CBInt2).toCBVar()
    of Int3: return op(a.int3Value.toCpp, b.int3Value.toCpp).to(CBInt3).toCBVar()
    of Int4: return op(a.int4Value.toCpp, b.int4Value.toCpp).to(CBInt4).toCBVar()
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
    of Chain: throwCBException("Chain " & astToStr(op) & " Not supported")
    of Enum: throwCBException("Enum " & astToStr(op) & " Not supported")

mathBinaryOp(`+`)
mathBinaryOp(`-`)
mathBinaryOp(`*`)
mathIntBinaryOp(`xor`)
mathIntBinaryOp(`and`)
mathIntBinaryOp(`or`)
mathIntBinaryOp(`mod`)
mathIntBinaryOp(`shl`)
mathIntBinaryOp(`shr`)

proc `>=`*(a,b: CBVar): bool {.inline.} =
  if a.valueType != b.valueType: doAssert(false, "`>=` Not supported between different types " & $a.valueType & " and " & $b.valueType)
  case a.valueType
  of None, Any: return true
  of Bool: return a.boolValue >= b.boolValue
  of Int: return a.intValue >= b.intValue
  of Int2: return (a.int2Value.toCpp >= b.int2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Int3: return (a.int3Value.toCpp >= b.int3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Int4: return (a.int4Value.toCpp >= b.int4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of Float: return a.floatValue >= b.floatValue
  of Float2: return (a.float2Value.toCpp >= b.float2Value.toCpp).to(CBFloat2).elems.allit(it != 0)
  of Float3: return (a.float3Value.toCpp >= b.float3Value.toCpp).to(CBFloat3).elems.allit(it != 0)
  of Float4: return (a.float4Value.toCpp >= b.float4Value.toCpp).to(CBFloat4).elems.allit(it != 0)
  of String, ContextVar: return $a.stringValue.string >= b.stringValue.string
  of Color: throwCBException("Color `>=` Not supported")
  of CBType.Image: throwCBException("Image `>=` Not supported")
  of Seq: throwCBException("Seq `>=` Not supported")
  of Chain: throwCBException("Chain `>=` Not supported")
  of Enum: throwCBException("Enum `>=` Not supported")
  of Object: throwCBException("Object `>=` Not supported")

proc `>`*(a,b: CBVar): bool {.inline.} =
  if a.valueType != b.valueType: doAssert(false, "`>` Not supported between different types " & $a.valueType & " and " & $b.valueType)
  case a.valueType
  of None, Any: return true
  of Bool: return a.boolValue > b.boolValue
  of Int: return a.intValue > b.intValue
  of Int2: return (a.int2Value.toCpp > b.int2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Int3: return (a.int3Value.toCpp > b.int3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Int4: return (a.int4Value.toCpp > b.int4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of Float: return a.floatValue > b.floatValue
  of Float2: return (a.float2Value.toCpp > b.float2Value.toCpp).to(CBFloat2).elems.allit(it != 0)
  of Float3: return (a.float3Value.toCpp > b.float3Value.toCpp).to(CBFloat3).elems.allit(it != 0)
  of Float4: return (a.float4Value.toCpp > b.float4Value.toCpp).to(CBFloat4).elems.allit(it != 0)
  of String, ContextVar: return a.stringValue.string > b.stringValue.string
  of Color: throwCBException("Color `>` Not supported")
  of CBType.Image: throwCBException("Image `>` Not supported")
  of Seq: throwCBException("Seq `>` Not supported")
  of Chain: throwCBException("Chain `>` Not supported")
  of Enum: throwCBException("Enum `>` Not supported")
  of Object: throwCBException("Object `>` Not supported")

proc `<`*(a,b: CBVar): bool {.inline.} =
  if a.valueType != b.valueType: doAssert(false, "`<` Not supported between different types " & $a.valueType & " and " & $b.valueType)
  case a.valueType
  of None, Any: return true
  of Bool: return a.boolValue < b.boolValue
  of Int: return a.intValue < b.intValue
  of Int2: return (a.int2Value.toCpp < b.int2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Int3: return (a.int3Value.toCpp < b.int3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Int4: return (a.int4Value.toCpp < b.int4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of Float: return a.floatValue < b.floatValue
  of Float2: return (a.float2Value.toCpp < b.float2Value.toCpp).to(CBFloat2).elems.allit(it != 0)
  of Float3: return (a.float3Value.toCpp < b.float3Value.toCpp).to(CBFloat3).elems.allit(it != 0)
  of Float4: return (a.float4Value.toCpp < b.float4Value.toCpp).to(CBFloat4).elems.allit(it != 0)
  of String, ContextVar: return a.stringValue.string < b.stringValue.string
  of Color: throwCBException("Color `<` Not supported")
  of CBType.Image: throwCBException("Image `<` Not supported")
  of Seq: throwCBException("Seq `<` Not supported")
  of Chain: throwCBException("Chain `<` Not supported")
  of Enum: throwCBException("Enum `<` Not supported")
  of Object: throwCBException("Object `<` Not supported")

proc `<=`*(a,b: CBVar): bool {.inline.} =
  if a.valueType != b.valueType: doAssert(false, "`<=` Not supported between different types " & $a.valueType & " and " & $b.valueType)
  case a.valueType
  of None, Any: return true
  of Bool: return a.boolValue <= b.boolValue
  of Int: return a.intValue <= b.intValue
  of Int2: return (a.int2Value.toCpp <= b.int2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Int3: return (a.int3Value.toCpp <= b.int3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Int4: return (a.int4Value.toCpp <= b.int4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of Float: return a.floatValue <= b.floatValue
  of Float2: return (a.float2Value.toCpp <= b.float2Value.toCpp).to(CBFloat2).elems.allit(it != 0)
  of Float3: return (a.float3Value.toCpp <= b.float3Value.toCpp).to(CBFloat3).elems.allit(it != 0)
  of Float4: return (a.float4Value.toCpp <= b.float4Value.toCpp).to(CBFloat4).elems.allit(it != 0)
  of String, ContextVar: return a.stringValue.string <= b.stringValue.string
  of Color: throwCBException("Color `<=` Not supported")
  of CBType.Image: throwCBException("Image `<=` Not supported")
  of Seq: throwCBException("Seq `<=` Not supported")
  of Chain: throwCBException("Chain `<=` Not supported")
  of Enum: throwCBException("Enum `<=` Not supported")
  of Object: throwCBException("Object `<=` Not supported")

proc `==`*(a,b: CBVar): bool {.inline.} =
  if a.valueType != b.valueType: return false
  case a.valueType
  of None, Any: return true
  of Bool: return a.boolValue == b.boolValue
  of Int: return a.intValue == b.intValue
  of Int2: return (a.int2Value.toCpp == b.int2Value.toCpp).to(CBInt2).elems.allit(it != 0)
  of Int3: return (a.int3Value.toCpp == b.int3Value.toCpp).to(CBInt3).elems.allit(it != 0)
  of Int4: return (a.int4Value.toCpp == b.int4Value.toCpp).to(CBInt4).elems.allit(it != 0)
  of Float: return a.floatValue == b.floatValue
  of Float2: return (a.float2Value.toCpp == b.float2Value.toCpp).to(CBFloat2).elems.allit(it != 0)
  of Float3: return (a.float3Value.toCpp == b.float3Value.toCpp).to(CBFloat3).elems.allit(it != 0)
  of Float4: return (a.float4Value.toCpp == b.float4Value.toCpp).to(CBFloat4).elems.allit(it != 0)
  of String, ContextVar: return a.stringValue.string == b.stringValue.string
  of Color: return a.colorValue == b.colorValue
  of CBType.Image:
    if  a.imageValue.width != b.imageValue.width or
        a.imageValue.height != b.imageValue.height or
        a.imageValue.channels != b.imageValue.channels or
        (a.imageValue.data == nil) and (b.imageValue.data != nil) or
        (a.imageValue.data != nil) and (b.imageValue.data == nil): return false
    if a.imageValue.data == nil and b.imageValue.data == nil: return true
    return equalMem(a.imageValue.data, b.imageValue.data, a.imageValue.width * a.imageValue.height * a.imageValue.channels)
  of Seq: return a.seqValue == b.seqValue
  of Chain: return a.chainValue == b.chainValue
  of Enum:  return a.enumValue.int32  == b.enumValue.int32 and a.enumVendorId == b.enumVendorId and a.enumTypeId == b.enumTypeId
  of Object:
    if  a.objectVendorId.int32 != b.objectVendorId.int32 or
        a.objectTypeId.int32 != b.objectTypeId.int32 or
        a.objectValue != b.objectValue: return false
    return true

proc `$`*(a: CBVar): string {.inline.} =
  case a.valueType
  of None, Any: return $a.valueType
  of Bool: return $a.boolValue
  of Int: return $a.intValue
  of Int2: return $a.int2Value.toCpp[0].to(int64) & ", " & $a.int2Value.toCpp[1].to(int64)
  of Int3: return $a.int3Value.toCpp[0].to(int64) & ", " & $a.int3Value.toCpp[1].to(int64) & ", " & $a.int3Value.toCpp[2].to(int64)
  of Int4: return $a.int4Value.toCpp[0].to(int64) & ", " & $a.int4Value.toCpp[1].to(int64) & ", " & $a.int4Value.toCpp[2].to(int64) & ", " & $a.int4Value.toCpp[3].to(int64)
  of Float: return $a.floatValue
  of Float2: return $a.float2Value.toCpp[0].to(float64) & ", " & $a.float2Value.toCpp[1].to(float64)
  of Float3: return $a.float3Value.toCpp[0].to(float64) & ", " & $a.float3Value.toCpp[1].to(float64) & ", " & $a.float3Value.toCpp[2].to(float64)
  of Float4: return $a.float4Value.toCpp[0].to(float64) & ", " & $a.float4Value.toCpp[1].to(float64) & ", " & $a.float4Value.toCpp[2].to(float64) & ", " & $a.float4Value.toCpp[3].to(float64)
  of String, ContextVar:
    if a.stringValue.pointer != nil:
      return a.stringValue
    else:
      return ""
  of Color: return $a.colorValue
  of CBType.Image: return $a.imageValue
  of Seq:
    result = ""
    let len = a.seqValue.len
    var idx = 0
    for item in a.seqValue.items:
      if idx == len - 1:
        result &= $item
      else:
        result &= $item & ", "
      inc idx
  of Chain: return $a.chainValue
  of Enum:  return $a.enumValue.int32
  of Object: return "Object - addr: 0x" & $cast[int](a.objectValue).toHex() & " vendorId: 0x" & a.objectVendorId.toHex() & " typeId: 0x" & a.objectTypeId.toHex() 