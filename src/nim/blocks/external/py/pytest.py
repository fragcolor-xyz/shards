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
  Color = auto()
  Chain = auto()
  Block = auto()
  EndOfBlittableTypes = auto()
  String = auto()
  ContextVar = auto()
  Image = auto()
  Seq = auto()
  Table = auto()

def activate(self, input):
  if "i" in self:
    self["i"] = self["i"] + 1
  else:
    self["i"] = 0
  
  if not self["suspend"](0.5):
    return # if false, return asap

  # 13 = String type
  return (CBType.String, input + " world " + str(self["i"]))