import nimline
import ../chainblocks
import ../../error
import tables
import fragments/serialization

when defined windows:
  type
    W32HANDLE {.importc: "HANDLE", header: "windows.h".} = distinct int
    W32OVERLAPPED {.importc: "OVERLAPPED", header: "windows.h".} = object

  var invalidHandle {.importc: "INVALID_HANDLE_VALUE", nodecl.}: W32HANDLE

type
  NamedPipe* = object
    isServer*: bool
    when defined windows:
      namedPipe*: W32HANDLE

const
  pipeServerMaxBuffer = 32 * 1024
  payloadMaxSize = 256 + sizeof(CBVar)

const
  PipeCC* = toFourCC("pipe")

let
  PipeInfo* = CBTypeInfo(basicType: Object, objectVendorId: FragCC, objectTypeId: PipeCC)

registerObjectType(FragCC, PipeCC, CBObjectInfo(name: "IPC Pipe"))

template isPipeObject*(v: CBVar): bool = (v.valueType == Object and v.objectVendorId == FragCC and v.objectTypeId == PipeCC)
converter toCBVar*(v: ptr NamedPipe): CBVar {.inline.} = v.pointer.asCBVar(FragCC, PipeCC)

# PipeServer - opens a named pipe server and resumes flow
when true:
  type
    CBPipeServer* = object
      name*: string
      connected*: bool
      pipe*: NamedPipe
      when defined windows:
        overlapped*: W32OVERLAPPED

  template setup*(b: CBPipeServer) =
    when defined windows:
      b.pipe.namedPipe = invalidHandle
      b.pipe.isServer = true
    else:
      discard
  template cleanup*(b: CBPipeServer) =
    when defined windows:
      if b.pipe.namedPipe.int != invalidHandle.int:
        global.CloseHandle(b.pipe.namedPipe).to(void)
        b.pipe.namedPipe = invalidHandle
    else:
      discard
  template inputTypes*(b: CBPipeServer): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBPipeServer): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBPipeServer): CBParametersInfo = @[("Name", { String, ContextVar })]
  template exposedVariables*(b: CBPipeServer): CBParametersInfo = @[("CBPipe." & b.name, @[PipeInfo])]
  template setParam*(b: CBPipeServer; index: int; val: CBVar) =
    b.name = val.stringValue
    b.cleanup() # Also reset the pipe if we have one
  template getParam*(b: CBPipeServer; index: int): CBVar = b.name
  template activate*(b: var CBPipeServer; context: CBContext; input: CBVar): CBVar =
    when defined windows:
      if b.pipe.namedPipe.int == invalidHandle.int:
        b.connected = false
        if b.name != "":
          let name = "\\\\.\\pipe\\" & b.name
          b.pipe.namedPipe = global.CreateNamedPipeA(name.cstring, 
            global.PIPE_ACCESS_DUPLEX.to(int) or global.FILE_FLAG_OVERLAPPED.to(int) or global.FILE_FLAG_FIRST_PIPE_INSTANCE.to(int),
            global.PIPE_TYPE_MESSAGE.to(int) or global.PIPE_READMODE_MESSAGE.to(int) or global.PIPE_WAIT.to(int),
            1,
            pipeServerMaxBuffer,
            pipeServerMaxBuffer,
            1,
            global.NULL).to(W32HANDLE)
          if b.pipe.namedPipe.int != invalidHandle.int:
            zeroMem(addr b.overlapped, sizeof(W32OVERLAPPED))
            global.ConnectNamedPipe(b.pipe.namedPipe, addr b.overlapped).to(void)
          else:
            raise newException(CBRuntimeException, "Failed to create pipe, maybe already exists.")
      
      if b.pipe.namedPipe.int != invalidHandle.int: # Compiler will optimize this, keep it sane in terms of flow
        if not b.connected:
          var bytesTransfered: cint
          while not global.GetOverlappedResult(b.pipe.namedPipe, addr b.overlapped, addr bytesTransfered, global.FALSE).to(bool):
            pause(0.0)
          
          echo b.name & " connected."
          b.connected = true
          context.variables["CBPipe." & b.name] = addr b.pipe
      
    input

  chainblock CBPipeServer, "PipeServer"

# PipeClient - opens a named pipe client and resumes flow on success
when true:
  type
    CBPipeClient* = object
      name*: string
      pipe*: NamedPipe

  template setup*(b: CBPipeClient) =
    when defined windows:
      b.pipe.namedPipe = invalidHandle
      b.pipe.isServer = false
    else:
      discard
  template cleanup*(b: CBPipeClient) =
    when defined windows:
      if b.pipe.namedPipe.int != invalidHandle.int:
        global.CloseHandle(b.pipe.namedPipe).to(void)
        b.pipe.namedPipe = invalidHandle
    else:
      discard
  template inputTypes*(b: CBPipeClient): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBPipeClient): CBTypesInfo = ({ Any }, true #[seq]#)
  template parameters*(b: CBPipeClient): CBParametersInfo = @[("Name", { String, ContextVar })]
  template exposedVariables*(b: CBPipeClient): CBParametersInfo = @[("CBPipe." & b.name, @[PipeInfo])]
  template setParam*(b: CBPipeClient; index: int; val: CBVar) =
    b.name = val.stringValue
    b.cleanup() # Also disconnect if we have a open pipe
  template getParam*(b: CBPipeClient; index: int): CBVar = b.name
  template activate*(b: var CBPipeClient; context: CBContext; input: CBVar): CBVar =
    when defined windows:
      while b.pipe.namedPipe.int == invalidHandle.int:
        if b.name != "":
          let name = "\\\\.\\pipe\\" & b.name
          b.pipe.namedPipe = global.CreateFileA(name.cstring, 
            global.GENERIC_READ.to(int) or global.GENERIC_WRITE.to(int),
            0,
            global.NULL,
            global.OPEN_EXISTING,
            global.FILE_FLAG_OVERLAPPED,
            global.NULL).to(W32HANDLE)
          if b.pipe.namedPipe.int != invalidHandle.int:
            echo b.name & " opened."
            context.variables["CBPipe." & b.name] = addr b.pipe
          else:
            # Keep trying to open if fails!
            pause(0.0)
    
    input

  chainblock CBPipeClient, "PipeClient"

# WritePipe - writes to a named pipe
when true:
  type
    CBWritePipe* = object
      name*: string

  template inputTypes*(b: CBWritePipe): CBTypesInfo = { None, Bool, Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color, BoolOp }
  template outputTypes*(b: CBWritePipe): CBTypesInfo = { None, Bool, Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color, BoolOp }
  template parameters*(b: CBWritePipe): CBParametersInfo = @[("Name", { String, ContextVar })]
  template consumedVariables*(b: CBWritePipe): CBParametersInfo = @[("CBPipe." & b.name, @[PipeInfo])]
  template setParam*(b: CBWritePipe; index: int; val: CBVar) = b.name = val.stringValue
  template getParam*(b: CBWritePipe; index: int): CBVar = b.name
  template activate*(b: var CBWritePipe; context: CBContext; input: CBVar): CBVar =
    var pipeInt = context.variables.getOrDefault("CBPipe." & b.name)
    if pipeInt.isPipeObject():
      var pipe = cast[ptr NamedPipe](pipeInt.objectValue)
      when defined windows:
        var
          inputVar = input
          written: cint
          overlapped: W32OVERLAPPED
          payload: array[payloadMaxSize, uint8]
          payloadSize = sizeof(CBVar)
        
        copyMem(addr payload[0], addr inputVar, sizeof(CBVar))
        
        if inputVar.valueType == String:
          var cs = inputVar.stringValue.cstring
          copyMem(addr payload[sizeof(CBVar)], cs, input.stringValue.len)
          payloadSize += input.stringValue.len
        
        if not global.WriteFile(pipe.namedPipe, addr payload[0], payloadSize, addr written, addr overlapped).to(bool):
          when not defined release:
            echo "WriteFile / WritePipe failed"
          # Close the pipe so server/client might re-open it eventually
          global.CloseHandle(pipe.namedPipe).to(void)
          pipe.namedPipe = invalidHandle
          context.variables.del("CBPipe." & b.name)
    input

  chainblock CBWritePipe, "WritePipe"

# ReadPipe - reads from a named pipe
when true:
  type
    CBReadPipe* = object
      name*: string
      waiting*: bool
      payload*: array[payloadMaxSize, uint8]
      read*: cint
      when defined windows:
        overlapped*: W32OVERLAPPED

  template inputTypes*(b: CBReadPipe): CBTypesInfo = ({ Any }, true #[seq]#)
  template outputTypes*(b: CBReadPipe): CBTypesInfo = { None, Bool, Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color, BoolOp }
  template parameters*(b: CBReadPipe): CBParametersInfo = @[("Name", { String, ContextVar })]
  template consumedVariables*(b: CBReadPipe): CBParametersInfo = @[("CBPipe." & b.name, @[PipeInfo])]
  template setParam*(b: CBReadPipe; index: int; val: CBVar) = b.name = val.stringValue
  template getParam*(b: CBReadPipe; index: int): CBVar = b.name
  template activate*(b: var CBReadPipe; context: CBContext; input: CBVar): CBVar =
    var
      pipeInt = context.variables.getOrDefault("CBPipe." & b.name)
      res = RestartChain
    if pipeInt.isPipeObject():
      var pipe = cast[ptr NamedPipe](pipeInt.objectValue)
      when defined windows:
        if b.waiting:
          var bytesTransfered: cint
          while not global.GetOverlappedResult(pipe.namedPipe, addr b.overlapped, addr bytesTransfered, global.FALSE).to(bool):
            pause(0.0)
          b.waiting = false
          
          var output = cast[ptr CBVar](addr b.payload[0])
          if output.valueType != String:
            assert bytesTransfered == sizeof(CBVar)
          else:
            var cs = cast[cstring](addr b.payload[sizeof(CBVar)])
            output.stringValue = $cs
            assert bytesTransfered == sizeof(CBVar) + output.stringValue.len
          res = output[]
        else:
          static:
            assert sizeof(b.payload) == payloadMaxSize
          
          # reset our buffer
          zeroMem(addr b.payload[0], sizeof(b.payload))

          # queue a read
          if global.ReadFile(pipe.namedPipe, addr b.payload[0], sizeof(b.payload), addr b.read, addr b.overlapped).to(bool):
            var output = cast[ptr CBVar](addr b.payload[0])
            res = output[]
          else:
            var err = global.GetLastError().to(int)
            if err == global.ERROR_IO_PENDING.to(int):
              b.waiting = true
            else:
              when not defined release:
                echo "ReadFile / ReadPipe failed"
              # Close the pipe so server/client might re-open it eventually
              global.CloseHandle(pipe.namedPipe).to(void)
              pipe.namedPipe = invalidHandle
              context.variables.del("CBPipe." & b.name)
    
    res

  chainblock CBReadPipe, "ReadPipe"

when isMainModule:
  import os
  chainblocks.init()
  proc run() =
    withChain server:
      PipeServer "AutomaPipe"
      ReadPipe "AutomaPipe"
      Log()
    
    withChain client:
      PipeClient "AutomaPipe"
      Const "Hello world"
      WritePipe "AutomaPipe"
      Sleep 1.0
    
    server.start(true)
    client.start(true)

    while true:
      server.tick()
      client.tick()
      sleep 15
  run()
