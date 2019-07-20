import ../../../types
import ../../../chainblocks
import ../../../../py/varspy
import nimpy/py_types
import nimpy/py_lib
import nimpy
import tables, os, sets

type
  Scripting* = object

var
  py: PyObject
  pySys: PyObject

when true:
  type
    CBPython = object
      filename*: string
      pymod*: PyObject
      instance*: PyObject
      pyresult*: PyObject # need to keep it alive
      loaded*: bool
      pySuspendRes*: CBVar
      inputSeqCache*: seq[PPyObject]
      inputTableCache*: Table[string, PPyObject]
      stringStorage*: string
      stringsStorage*: seq[string]
      seqStorage*: CBSeq
      tableStorage*: CBTable
      outputTableKeyCache*: HashSet[cstring]
  
  template cleanup*(b: CBPython) =
    b.instance = nil
    b.pymod = nil
    b.loaded = false
  template setup*(b: CBPython) =
    initSeq(b.seqStorage)
    initTable(b.tableStorage)
    b.outputTableKeyCache = initHashSet[cstring]()
    b.inputTableCache = initTable[string, PPyObject]()
  template destroy*(b: CBPython) =
    freeSeq(b.seqStorage)
    freeTable(b.tableStorage)
  template inputTypes*(b: CBPython): CBTypesInfo = ({ Any }, true)
  template outputTypes*(b: CBPython): CBTypesInfo = ({ Any }, true)
  template parameters*(b: CBPython): CBParametersInfo = @[("File", { String })]
  template setParam*(b: CBPython; index: int; val: CBVar) =
    cleanup(b) # force reload of python module
    b.filename = val.stringValue
  template getParam*(b: CBPython; index: int): CBVar =
    b.filename

  template activate*(blk: CBPython; context: CBContext; input: CBVar): CBVar =
    var res = StopChain
    try:
      if py == nil or pySys == nil:
        py = pyBuiltinsModule()
        pySys = pyImport("sys")
        doAssert py != nil and pySys != nil
      
      if blk.instance == nil:
        # Create a empty object to attach to this block instance
        blk.instance = py.dict()
        blk.instance["suspend"] = proc(seconds: float64): bool =
          blk.pySuspendRes = suspend(seconds)
          if blk.pySuspendRes.chainState != Continue:
            return false
          return true
      
      if not blk.loaded and blk.filename != "" and fileExists(blk.filename):
        # Load, but might also be in memory!
        let (dir, name, _) = blk.filename.splitFile()
        pySys.path.append(dir).to(void)
        blk.pymod = pyImport(name)

        # Also actually force a reload, to support changes in the module during runtime
        let
          majorVer = pySys.version_info[0].to(int)
          minorVer = pySys.version_info[1].to(int)
        if majorVer < 3:
          py.reload(blk.pymod).to(void)
        elif majorVer == 3 and minorVer <= 4:
          let imp = pyImport("imp")
          imp.reload(blk.pymod).to(void)
        else:
          let imp = pyImport("importlib")
          imp.reload(blk.pymod).to(void)
              
        blk.loaded = true
          
      if blk.loaded:
          let pyinput = var2Py(input, blk.inputSeqCache, blk.inputTableCache)
          blk.pyresult = blk.pymod.callMethod("activate", blk.instance, pyinput)
          if blk.pySuspendRes.chainState != Continue:
            res = blk.pySuspendRes
          else:
            blk.seqStorage.clear()
            res = py2Var(blk.pyresult, blk.stringStorage, blk.stringsStorage, blk.seqStorage, blk.tableStorage, blk.outputTableKeyCache)
    except:
      context.setError(getCurrentExceptionMsg())
    res

  chainblock CBPython, "Py", "Scripting"

when false:
  import os
  
  putEnv("PYTHONHOME", "C:/ProgramData/Miniconda3")
  putEnv("PYTHONPATH", "C:/ProgramData/Miniconda3")
  
  chainblocks.init()
  
  Const "Hello"
  Scripting.Py "./pytest.py"
  Log()
  
  chainblocks.start(true)
  chainblocks.tick()
  chainblocks.stop()
  chainblocks.start(true)
  while true:
    chainblocks.tick()
    sleep 300