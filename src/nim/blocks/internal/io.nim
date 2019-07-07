import ../chainblocks
import ../../error
import tables, streams, parsecsv, strscans

# Namespace
type IO* = object

# WriteLine - Appends input variables to a file (CSV style), first line is CSV headers
when true:
  type
    CBWriteLine* = object
      filename*: string
      fstream*: FileStream

  template cleanup*(b: CBWriteLine) =
    if b.fstream != nil:
      b.fstream.close()
      b.fstream = nil
  template preChain*(b: CBWriteLine; context: CBContext) =
    if b.fstream == nil:
      b.fstream = openFileStream(b.filename, fmAppend)
  template inputTypes*(b: CBWriteLine): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBWriteLine): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBWriteLine): CBParametersInfo = @[("Filename", { String })]
  template setParam*(b: CBWriteLine; index: int; val: CBVar) =
    b.filename = val.stringValue
    cleanup(b) # also reset
  template getParam*(b: CBWriteLine; index: int): CBVar = b.filename
  template activate*(b: var CBWriteLine; context: CBContext; input: CBVar): CBVar =
    assert b.fstream != nil
    b.fstream.writeLine($input)
    input

  chainblock CBWriteLine, "WriteLine", "IO"

# ReadLine - Reads a CSV line from file, first raw is assumed to be headers
when true:
  type
    CBReadLine* = object
      filename*: string
      opened*: bool
      csv*: CsvParser
      looped*: bool

  template cleanup*(b: CBReadLine) =
    if b.opened:
      b.csv.close()
      b.opened = false
  template preChain*(b: CBReadLine; context: CBContext) =
    if not b.opened:
      b.csv.open(b.filename)
      b.opened = true
      # Test if failing raises something
      b.csv.readHeaderRow()
  template inputTypes*(b: CBReadLine): CBTypesInfo = { None }
  template outputTypes*(b: CBReadLine): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBReadLine): CBParametersInfo =
    @[
      ("Filename", { String }),
      ("Looped", { Bool })
    ]
  template setParam*(b: CBReadLine; index: int; val: CBVar) =
    case index
    of 0:
      b.filename = val.stringValue
      cleanup(b) # also reset
    of 1:
      b.looped = val.boolValue
    else:
      assert(false)
  template getParam*(b: var CBReadLine; index: int): CBVar =
    case index
    of 0:
      CBVar(valueType: String, stringValue: b.filename)
    of 1:
      CBVar(valueType: Bool, boolValue: b.looped)
    else:
      assert(false); 
      Empty
  proc activate*(b: var CBReadLine; context: CBContext; input: CBVar): CBVar {.inline.} =
    assert b.opened
    while true:
      if b.csv.readRow():
        if b.csv.headers.len > 1:
          var varseq = newSeqOfCap[CBVar](b.csv.headers.len)
          for col in items(b.csv.headers):
            var
              vi: int
              vf: float
            if scanf(col, "$s$i$s", vi):
              varseq.add vi
            elif scanf(col, "$s$f$s", vf):
              varseq.add vf
            else:
              varseq.add col
          return varseq
        else:
          for col in items(b.csv.headers):
            var
              vi: int
              vf: float
            if scanf(col, "$s$i$s", vi):
              return vi
            elif scanf(col, "$s$f$s", vf):
              return vf
            else:
              return col
        break
      elif b.looped:
        b.csv.close()
        b.csv.open(b.filename)
        b.csv.readHeaderRow()
      else:
        break
    
    return Empty

  chainblock CBReadLine, "ReadLine", "IO"

when isMainModule:
  import os

  chainblocks.init()

  withChain writer:
    Const (10, 20, 30, 40)
    IO.WriteLine "io.csv"
    Msg "Wrote"
  
  withChain writeit:
    Repeat:
      Times: 4
      Chain: writer
  
  withChain reader:
    IO.ReadLine:
      Filename: "io.csv"
      Looped: true
    Log()
  
  withChain readit:
    Repeat:
      Times: 4
      Chain: reader
  
  writeit.start(true)
  writeit.tick()
  writeit.stop()

  readit.start(true)
  while true:
    readit.tick()
    sleep 100
  readit.stop()