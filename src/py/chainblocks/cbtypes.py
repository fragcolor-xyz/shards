from enum import IntEnum, auto

class CBType(IntEnum):
  NoType = 0
  Any = auto()
  Object = auto()
  Enum = auto()
  Bool = auto()
  Int = auto()
  Int2 = auto()
  Int3 = auto()
  Int4 = auto()
  Int8 = auto()
  Int16 = auto()
  Float = auto()
  Float2 = auto()
  Float3 = auto()
  Float4 = auto()
  String = auto()
  Color = auto()
  Image = auto()
  Seq = auto()
  Table = auto()
  Chain = auto()
  Block = auto()
  ContextVar = auto()

def validateConnection(outputInfo, inputInfo):
  for iinfo in inputInfo:
    if iinfo[0] == CBType.Any:
      return True
    else:
      for oinfo in outputInfo:
        if oinfo[0] == CBType.Any:
          return True
        else:
          if iinfo[0] == oinfo[0] and iinfo[1] == oinfo[1]: # Types match, also sequenced
            return True

def cbcall(func):
  assert(callable(func))
  return (CBType.Object, (func, 1734439526, 2035317104))

def cbstring(v):
  assert(type(v) is str)
  return (CBType.String, v)

def cbbool(v):
  assert(type(v) is bool)
  return (CBType.Bool, v)

def cbint(v):
  assert(type(v) is int)
  return (CBType.Int, v)

def cbint2(v1, v2):
  assert(type(v1) is int and type(v2) is int)
  return (CBType.Int2, (v1, v2))

def cbint3(v1, v2, v3):
  assert(type(v1) is int and type(v2) is int and type(v3) is int)
  # TODO add range checks
  return (CBType.Int3, (v1, v2, v3))

def cbint4(v1, v2, v3, v4):
  assert(type(v1) is int and type(v2) is int and type(v3) is int and type(v4) is int)
  # TODO add range checks
  return (CBType.Int4, (v1, v2, v3, v4))

def cbint8(v1, v2, v3, v4, v5, v6, v7, v8):
  assert( type(v1) is int and type(v2) is int and type(v3) is int and type(v4) is int and
          type(v5) is int and type(v6) is int and type(v7) is int and type(v8) is int)
  # TODO add range checks
  return (CBType.Int8, (v1, v2, v3, v4, v5, v6, v7, v8))

def cbint16(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16):
  assert( type(v1) is int and type(v2) is int and type(v3) is int and type(v4) is int and
          type(v5) is int and type(v6) is int and type(v7) is int and type(v8) is int and
          type(v9) is int and type(v10) is int and type(v11) is int and type(v12) is int and
          type(v13) is int and type(v14) is int and type(v15) is int and type(v16) is int)
  # TODO add range checks
  return (CBType.Int16, (v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16))

def cbcolor(v1, v2, v3, v4):
  assert(type(v1) is int and type(v2) is int and type(v3) is int and type(v4) is int)
  return (CBType.Color, (v1, v2, v3, v4))

def cbfloat(v):
  assert(type(v) is float)
  return (CBType.Float, v)

def cbfloat2(v1, v2):
  assert(type(v1) is float and type(v2) is float)
  return (CBType.Float2, (v1, v2))

def cbfloat3(v1, v2, v3):
  assert(type(v1) is float and type(v2) is float and type(v3) is float)
  return (CBType.Float3, (v1, v2, v3))

def cbfloat4(v1, v2, v3, v4):
  assert(type(v1) is float and type(v2) is float and type(v3) is float and type(v4) is float)
  return (CBType.Float4, (v1, v2, v3, v4))