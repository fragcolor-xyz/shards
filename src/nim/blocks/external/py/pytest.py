def activate(self, input):
  if "i" in self:
    self["i"] = self["i"] + 1
  else:
    self["i"] = 0
  
  if not self["suspend"](0.5):
    return # if false, return asap

  # 13 = String type
  return (13, input + " world " + str(self["i"]))