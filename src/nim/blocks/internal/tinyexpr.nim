import ../chainblocks
import ../../error
import fragments/math/vectors
import tables

when true:
  {.compile: "tinyexpr.c".}
  
  type
    TeExpr* {.importc: "te_expr", header: "tinyexpr.h".} = object
    TeVar* {.importc: "te_variable", header: "tinyexpr.h".} = object
      name: cstring
      address: pointer
  proc te_compile*(expression: cstring; variables: ptr TeVar; variablesCount: cint; error: ptr cint): ptr TeExpr {.importc, header: "tinyexpr.h".}
  proc te_eval*(expression: ptr TeExpr): cdouble {.importc, header: "tinyexpr.h".}
  proc te_free*(expression: ptr TeExpr) {.importc, header: "tinyexpr.h".}

  type
    CBExpr* = object
      expressionString*: string
      expression*: ptr TeExpr
      inputVar*: float64
      variables*: seq[CBVar]
      variablesStorage*: seq[float64]
      variablesInternal*: seq[TeVar]

  template cleanup*(b: CBExpr) =
    if b.expression != nil:
      te_free(b.expression)
      b.expression = nil
  template preChain*(b: CBExpr; context: CBContext) =
    if b.expression == nil and b.expressionString != "":
      # need to build likely..
      var error: cint
      b.expression = te_compile(b.expressionString, addr b.variablesInternal[0], b.variablesInternal.len.cint, addr error)
      if error != 0:
        halt(context, "Syntax error in math expression.")
  template inputTypes*(b: CBExpr): CBTypesInfo = { Float }
  template outputTypes*(b: CBExpr): CBTypesInfo = { Float }
  template parameters*(b: CBExpr): CBParametersInfo =
    @[
      ("Expression", ({ String }, false), false),
      ("Variables", ({ Float}, true), true)
    ]
  template setParam*(b: CBExpr; index: int; val: CBVar) =
    case index
    of 0:
      b.expressionString = val.stringValue
    of 1:
      b.variables = val.seqValue
    else:
      assert(false)
    
    block init:
      # always re-init all
      cleanup(b)
      b.variablesInternal.setLen(0)
      b.variablesStorage.setLen(0)

      # add `input` variable
      b.variablesInternal.add TeVar(name: "input", address: addr b.inputVar)
      
      # add any other variable with names: v0, v1, v2 etc
      var idx = 0
      for variable in b.variables.mitems:
        if variable.valueType == ContextVar:
          b.variablesStorage.add 0.0
        else:
          b.variablesStorage.add variable.floatValue
        
        b.variablesInternal.add TeVar(name: "v" & $idx, address: addr b.variablesStorage[idx])
        
        inc idx
  template getParam*(b: CBExpr; index: int): CBVar =
    case index
    of 0:
      b.expressionString.CBVar
    of 1:
      b.variables
    else:
      assert(false)
      Empty
  template activate*(b: CBExpr; context: CBContext; input: CBVar): CBVar =
    if b.expression != nil:
      # copy the input value to the input location
      b.inputVar = input.floatValue
      # also fixup context variables
      for i, variable in b.variables:
        if variable.valueType == ContextVar:
          let v = context.variables.getOrDefault(variable.stringValue)
          if v.valueType == Float:
            b.variablesStorage[i] = v.floatValue
      te_eval(b.expression)
    else:
      halt(context, "Math expression is not valid!")

  chainblock CBExpr, "Expr", "Math"

when isMainModule:
  chainblocks.init()
  proc run() =
    Math.Expr "input + 3"
    Log()
    Const 10.0
    SetVariable "ten"
    Const 4.0
    Math.Expr:
      Expression: "(input * 2) + v0 + v1 * v2"
      Variables: @[2.0.CBVar, 3.0.CBVar, ~~"ten"]
    Log()

    chainblocks.start()
    var input: CBVar = 1.0
    chainblocks.tick(input)
    chainblocks.stop()
  run()