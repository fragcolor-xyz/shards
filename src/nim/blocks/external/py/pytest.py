def activate(self, input):
  if "i" in self:
    self["i"] = self["i"] + 1
  else:
    self["i"] = 0
  # 12 = String type
  return (11, input + " world " + str(self["i"]))