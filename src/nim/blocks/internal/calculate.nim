type Math* = object

when true:
  template calculateUnaryCMath(typeName: untyped, shortName, cname, cnamef: string): untyped =
    type
      typeName* = object
        # INLINE BLOCK, CORE STUB PRESENT
        seqCache*: CBSeq

    template setup*(b: typename) = initSeq(b.seqCache)
    template destroy*(b: typename) = freeSeq(b.seqCache)
    template inputTypes*(b: typeName): CBTypesInfo = ({ Float, Float2, Float3, Float4 }, true)
    template outputTypes*(b: typeName): CBTypesInfo = ({ Float, Float2, Float3, Float4 }, true)
    proc inferTypes(b: var typeName; inputType: CBTypeInfo; consumables: CBExposedTypesInfo): CBTypeInfo =  
      result = inputType
    template activate*(b: typeName; context: CBContext; input: CBVar): CBVar =
      # THIS CODE WON'T BE EXECUTED
      # THIS BLOCK IS OPTIMIZED INLINE IN THE C++ CORE
      if input.valueType == Seq:
        b.seqCache.clear()
        for val in input.seqValue:
          case val.valueType
          of Float:
            b.seqCache.push(invokeFunction(cname, val.floatValue).to(float64))
          of Float2:
            var f2: CBFloat2
            for i in 0..1:
              f2[i] = invokeFunction(cname, val.float2Value[i]).to(float64)
            b.seqCache.push(f2)
          of Float3:
            var f3: CBFloat3
            for i in 0..2:
              f3[i] = invokeFunction(cnamef, val.float3Value[i]).to(float32)
            b.seqCache.push(f3)
          of Float4:
            var f4: CBFloat4
            for i in 0..3:
              f4[i] = invokeFunction(cnamef, val.float4Value[i]).to(float32)
            b.seqCache.push(f4)
          else:
            assert(false)
        b.seqCache.CBVar
      else:
        case input.valueType
        of Float:
          invokeFunction(cname, input.floatValue).to(float64).CBVar
        of Float2:
          var f2: CBFloat2
          for i in 0..1:
            f2[i] = invokeFunction(cname, input.float2Value[i]).to(float64)
          f2
        of Float3:
          var f3: CBFloat3
          for i in 0..2:
            f3[i] = invokeFunction(cnamef, input.float3Value[i]).to(float32)
          f3
        of Float4:
          var f4: CBFloat4
          for i in 0..3:
            f4[i] = invokeFunction(cnamef, input.float4Value[i]).to(float32)
          f4
        else:
          assert(false)
          Empty

    chainblock typeName, shortName, "Math"

  calculateUnaryCMath(CBMathAbs, "Abs", "fabs", "fabsf")
  calculateUnaryCMath(CBMathExp, "Exp", "exp", "expf")
  calculateUnaryCMath(CBMathExp2, "Exp2", "exp2", "exp2f")
  calculateUnaryCMath(CBMathExpm1, "Expm1", "expm1", "expm1f")
  calculateUnaryCMath(CBMathLog, "Log", "log", "logf")
  calculateUnaryCMath(CBMathLog10, "Log10", "log10", "log10f")
  calculateUnaryCMath(CBMathLog2, "Log2", "log2", "log2f")
  calculateUnaryCMath(CBMathLog1p, "Log1p", "log1p", "log1pf")
  calculateUnaryCMath(CBMathSqrt, "Sqrt", "sqrt", "sqrtf")
  calculateUnaryCMath(CBMathCbrt, "Cbrt", "cbrt", "cbrt")
  calculateUnaryCMath(CBMathSin, "Sin", "sin", "sinf")
  calculateUnaryCMath(CBMathCos, "Cos", "cos", "cosf")
  calculateUnaryCMath(CBMathTan, "Tan", "tan", "tanf")
  calculateUnaryCMath(CBMathAsin, "Asin", "asin", "asinf")
  calculateUnaryCMath(CBMathAcos, "Acos", "acos", "acosf")
  calculateUnaryCMath(CBMathAtan, "Atan", "atan", "atanf")
  calculateUnaryCMath(CBMathSinh, "Sinh", "sinh", "sinhf")
  calculateUnaryCMath(CBMathCosh, "Cosh", "cosh", "coshf")
  calculateUnaryCMath(CBMathTanh, "Tanh", "tanh", "tanhf")
  calculateUnaryCMath(CBMathAsinh, "Asinh", "asinh", "asinhf")
  calculateUnaryCMath(CBMathAcosh, "Acosh", "acosh", "acoshf")
  calculateUnaryCMath(CBMathAtanh, "Atanh", "atanh", "atanhf")
  calculateUnaryCMath(CBMathErf, "Erf", "erf", "erff")
  calculateUnaryCMath(CBMathErfc, "Erfc", "erfc", "erfcf")
  calculateUnaryCMath(CBMathTGamma, "TGamma", "tgamma", "tgammaf")
  calculateUnaryCMath(CBMathLGamma, "LGamma", "lgamma", "lgammaf")
  calculateUnaryCMath(CBMathCeil, "Ceil", "ceil", "ceilf")
  calculateUnaryCMath(CBMathFloor, "Floor", "floor", "floorf")
  calculateUnaryCMath(CBMathTrunc, "Trunc", "trunc", "truncf")
  calculateUnaryCMath(CBMathRound, "Round", "round", "roundf")
