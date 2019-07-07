# Namespace
type Stack* = object

# StackPush - pushes the input to the stack
when true:
  type
    CBStackPush* = object

  template inputTypes*(b: CBStackPush): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStackPush): CBTypesInfo = ({ Any }, true #[seq]#)
  template activate*(b: var CBStackPush; context: CBContext; input: CBVar): CBVar =
    context.stack.push(input)
    input

  chainblock CBStackPush, "Push", "Stack"

# StackPop - pops a variable from the stack
when true:
  type
    CBStackPop* = object

  template inputTypes*(b: CBStackPop): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStackPop): CBTypesInfo = ({ Any }, true #[seq]#)
  template activate*(b: var CBStackPop; context: CBContext; input: CBVar): CBVar =
    if context.stack.len >= 1:
      context.stack.pop()
    else:
      halt(context, "Stack imbalance")

  chainblock CBStackPop, "Pop", "Stack"

# Stack - reads a variable from the stack
when true:
  type
    CBStack* = object
      index: int

  template inputTypes*(b: CBStack): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStack): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBStack): CBParametersInfo = @[("Index", { Int })]
  template setParam*(b: CBStack; index: int; val: CBVar) =
    b.index = val.intValue.int
  template getParam*(b: CBStack; index: int): CBVar =
    CBVar(valueType: Int, intValue: b.index)
  template activate*(b: var CBStack; context: CBContext; input: CBVar): CBVar =
    context.stack[(context.stack.len - 1) - b.index]

  chainblock CBStack, "Peek", "Stack"

# StackPushUnpacked - pushes the input vector unpacking it into separate values
when true:
  type
    CBStackPushUnpacked* = object

  template inputTypes*(b: CBStackPushUnpacked): CBTypesInfo = { Int, Int2, Int3, Int4, Float, Float2, Float3, Float4 }
  template outputTypes*(b: CBStackPushUnpacked): CBTypesInfo = { Int, Int2, Int3, Int4, Float, Float2, Float3, Float4 }
  template activate*(b: var CBStackPushUnpacked; context: CBContext; input: CBVar): CBVar =
    case input.valueType
    of Int:
      context.stack.push(input.intValue)
    of Int2:
      context.stack.push(input.int2Value[1])
      context.stack.push(input.int2Value[0])
    of Int3:
      context.stack.push(input.int3Value[2])
      context.stack.push(input.int3Value[1])
      context.stack.push(input.int3Value[0])
    of Int4:
      context.stack.push(input.int4Value[3])
      context.stack.push(input.int4Value[2])
      context.stack.push(input.int4Value[1])
      context.stack.push(input.int4Value[0])
    of Float:
      context.stack.push(input.floatValue)
    of Float2:
      context.stack.push(input.float2Value[1])
      context.stack.push(input.float2Value[0])
    of Float3:
      context.stack.push(input.float3Value[2])
      context.stack.push(input.float3Value[1])
      context.stack.push(input.float3Value[0])
    of Float4:
      context.stack.push(input.float4Value[3])
      context.stack.push(input.float4Value[2])
      context.stack.push(input.float4Value[1])
      context.stack.push(input.float4Value[0])
    else:
      assert(false)
    input

  chainblock CBStackPushUnpacked, "PushUnpacked", "Stack"

# StackPopInt2 - pops a Int2 variable from the stack
when true:
  type
    CBStackPopInt2* = object

  template inputTypes*(b: CBStackPopInt2): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStackPopInt2): CBTypesInfo = { Int2 }
  template activate*(b: var CBStackPopInt2; context: CBContext; input: CBVar): CBVar =
    if context.stack.len >= 2:
      let
        x = context.stack.pop()
        y = context.stack.pop()
      
      (x.intValue.int64, y.intValue.int64).CBVar
    else:
      halt(context, "Stack imbalance")

  chainblock CBStackPopInt2, "PopInt2", "Stack"

# StackPopInt3 - pops a Int3 variable from the stack
when true:
  type
    CBStackPopInt3* = object

  template inputTypes*(b: CBStackPopInt3): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStackPopInt3): CBTypesInfo = { Int3 }
  template activate*(b: var CBStackPopInt3; context: CBContext; input: CBVar): CBVar =
    if context.stack.len >= 3:
      let
        x = context.stack.pop()
        y = context.stack.pop()
        z = context.stack.pop()
      
      (x.intValue.int64, y.intValue.int64, z.intValue.int64).CBVar
    else:
      halt(context, "Stack imbalance")

  chainblock CBStackPopInt3, "PopInt3", "Stack"

# StackPopInt4 - pops a Int4 variable from the stack
when true:
  type
    CBStackPopInt4* = object

  template inputTypes*(b: CBStackPopInt4): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStackPopInt4): CBTypesInfo = { Int4 }
  template activate*(b: var CBStackPopInt4; context: CBContext; input: CBVar): CBVar =
    if context.stack.len >= 4:
      let
        x = context.stack.pop()
        y = context.stack.pop()
        z = context.stack.pop()
        w = context.stack.pop()
            
      (x.intValue.int64, y.intValue.int64, z.intValue.int64, w.intValue.int64).CBVar
    else:
      halt(context, "Stack imbalance")

  chainblock CBStackPopInt4, "PopInt4", "Stack"

# StackPopFloat2 - pops a Float2 variable from the stack
when true:
  type
    CBStackPopFloat2* = object

  template inputTypes*(b: CBStackPopFloat2): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStackPopFloat2): CBTypesInfo = { Float2 }
  template activate*(b: var CBStackPopFloat2; context: CBContext; input: CBVar): CBVar =
    if context.stack.len >= 2:
      let
        x = context.stack.pop()
        y = context.stack.pop()
      
      (x.floatValue.float64, y.floatValue.float64).CBVar
    else:
      halt(context, "Stack imbalance")

  chainblock CBStackPopFloat2, "PopFloat2", "Stack"

# StackPopFloat3 - pops a Float3 variable from the stack
when true:
  type
    CBStackPopFloat3* = object

  template inputTypes*(b: CBStackPopFloat3): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStackPopFloat3): CBTypesInfo = { Float3 }
  template activate*(b: var CBStackPopFloat3; context: CBContext; input: CBVar): CBVar =
    if context.stack.len >= 3:
      let
        x = context.stack.pop()
        y = context.stack.pop()
        z = context.stack.pop()
      
      (x.floatValue.float64, y.floatValue.float64, z.floatValue.float64).CBVar
    else:
      halt(context, "Stack imbalance")

  chainblock CBStackPopFloat3, "PopFloat3", "Stack"

# StackPopFloat4 - pops a Float4 variable from the stack
when true:
  type
    CBStackPopFloat4* = object

  template inputTypes*(b: CBStackPopFloat4): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBStackPopFloat4): CBTypesInfo = { Float4 }
  template activate*(b: var CBStackPopFloat4; context: CBContext; input: CBVar): CBVar =
    if context.stack.len >= 4:
      let
        x = context.stack.pop()
        y = context.stack.pop()
        z = context.stack.pop()
        w = context.stack.pop()
      
      (x.floatValue.float64, y.floatValue.float64, z.floatValue.float64, w.floatValue.float64).CBVar
    else:
      halt(context, "Stack imbalance")

  chainblock CBStackPopFloat4, "PopFloat4", "Stack"

template stackBinaryOp(typeName: untyped, name: string, op: untyped): untyped =
  type
    typeName* = object

  template inputTypes*(b: typeName): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: typeName): CBTypesInfo = ({ Any }, true #[seq]#)
  template activate*(b: var typeName; context: CBContext; input: CBVar): CBVar =
    if context.stack.len >= 1:
      let x = context.stack.pop()
      op(input, x)
    else:
      halt(context, "Stack imbalance")

  chainblock typeName, name, "Stack"

stackBinaryOp(CBStackPopAdd, "PopAdd", `+`)
stackBinaryOp(CBStackPopSubtract, "PopSubtract", `-`)
stackBinaryOp(CBStackPopMultiply, "PopMultiply", `*`)
stackBinaryOp(CBStackPopDivide, "PopDivide", `/`)
