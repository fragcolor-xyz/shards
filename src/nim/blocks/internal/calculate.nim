type Math* = object

when true:
  template calculateBinaryOp(typeName: untyped, shortName: string, op: untyped): untyped =
    type
      typeName* = object
        operand*: CBVar
        seqCache*: seq[CBVar]

    template inputTypes*(b: typeName): CBTypesInfo = ({ Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, Color}, true)
    template outputTypes*(b: typeName): CBTypesInfo = ({ Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, Color }, true)
    template parameters*(b: typeName): CBParametersInfo =
      @[("Operand", { Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, Color }, true)]
    template setParam*(b: typeName; index: int; val: CBVar) = b.operand = val
    template getParam*(b: typeName; index: int): CBVar = b.operand
    template activate*(b: typeName; context: CBContext; input: CBVar): CBVar =
      if input.valueType == Seq:
        b.seqCache.setLen(0)
        for val in input.seqValue:
          b.seqCache.add op(val, b.operand)
        b.seqCache.CBVar
      else:
        op(input, b.operand)

    chainblock typeName, shortName, "Math"

  calculateBinaryOp(CBMathAdd, "Add", `+`)
  calculateBinaryOp(CBMathSubtract, "Subtract", `-`)
  calculateBinaryOp(CBMathMultiply, "Multiply", `*`)
  calculateBinaryOp(CBMathDivide, "Divide", `/`)