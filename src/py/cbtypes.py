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
