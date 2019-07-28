import ../../../types
import ../../../chainblocks
import nimline

when true:
  type
    CBDummy = object
  
  template inputTypes*(b: CBDummy): CBTypesInfo = ({ Any }, true)
  template outputTypes*(b: CBDummy): CBTypesInfo = ({ Any }, true)
  template activate*(blk: var CBDummy; context: CBContext; input: CBVar): CBVar =
    pause(1.0)
    input

  chainblock CBDummy, "Dummy", "Dummy"
