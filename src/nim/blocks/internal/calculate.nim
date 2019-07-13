type Math* = object

when true:
  template calculateBinaryOp(typeName: untyped, shortName: string, op: untyped): untyped =
    type
      typeName* = object
        # INLINE BLOCK, CORE STUB PRESENT
        operand*: CBVar
        ctxOperand*: ptr CBVar
        seqCache*: CBSeq
    
    template cleanup*(b: typeName) = b.ctxOperand = nil
    template setup*(b: typename) = initSeq(b.seqCache)
    template destroy*(b: typename) = freeSeq(b.seqCache)
    template inputTypes*(b: typeName): CBTypesInfo = ({ Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, Color}, true)
    template outputTypes*(b: typeName): CBTypesInfo = ({ Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, Color }, true)
    template parameters*(b: typeName): CBParametersInfo =
      @[("Operand", { Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, Color }, true #[usesContext]#)]
    template setParam*(b: typeName; index: int; val: CBVar) =
      b.operand = val
      # will refresh on next activate
      cleanup(b)
    template getParam*(b: typeName; index: int): CBVar =
      b.operand  
    template activate*(b: typeName; context: CBContext; input: CBVar): CBVar =
      if b.operand.valueType == ContextVar and b.ctxOperand == nil:
        b.ctxOperand = context.contextVariable(b.operand.stringValue)

      # THIS CODE WON'T BE EXECUTED
      # THIS BLOCK IS OPTIMIZED INLINE IN THE C++ CORE
      let operand = if b.operand.valueType == ContextVar: b.ctxOperand[] else: b.operand
      if unlikely(operand.valueType == None):
        halt(context, "Could not find the operand variable!")
      elif input.valueType == Seq:
        b.seqCache.clear()
        for val in input.seqValue:
          b.seqCache.push(op(val, b.operand))
        b.seqCache.CBVar
      else:
        op(input, b.operand)
    
    chainblock typeName, shortName, "Math"

  calculateBinaryOp(CBMathAdd,      "Add",      `+`)
  calculateBinaryOp(CBMathSubtract, "Subtract", `-`)
  calculateBinaryOp(CBMathMultiply, "Multiply", `*`)
  calculateBinaryOp(CBMathDivide,   "Divide",   `/`)
  calculateBinaryOp(CBMathXor,      "Xor",      `xor`)
  calculateBinaryOp(CBMathAnd,      "And",      `and`)
  calculateBinaryOp(CBMathOr,       "Or",       `or`)
  calculateBinaryOp(CBMathMod,      "Mod",      `mod`)
  calculateBinaryOp(CBMathLShift,   "LShift",   `shl`)
  calculateBinaryOp(CBMathRShift,   "RShift",   `shr`)