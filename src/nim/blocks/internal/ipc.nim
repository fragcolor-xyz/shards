import os
import nimline
# import ../../chainblocks

when defined linux:
  # LINUX WORKAROUND FOR -static build and pthread
  # Might need this on other OSes too maybe
  # https://stackoverflow.com/questions/47905554/segmentation-fault-appears-when-i-use-shared-memory-only-from-statically-build-p
  emitc("""
  extern "C" {
    #include <stdlib.h>
    #include <string.h>

    /* This avoids a segfault when code using shm_open()
      is compiled statically. (For some reason, compiling
      the code statically causes the __shm_directory()
      function calls in librt.a to not reach the implementation
      in libpthread.a. Implementing the function ourselves
      fixes this issue.)
    */

    #ifndef  SHM_MOUNT
    #define  SHM_MOUNT "/dev/shm/"
    #endif
    static const char  shm_mount[] = SHM_MOUNT;

    const char *__shm_directory(size_t *len)
    {
        if (len)
            *len = strlen(shm_mount);
        return shm_mount;
    }
  };
  """)

type IPC* = object

const
  modulePath = currentSourcePath().splitPath().head
cppincludes(modulePath & "")

defineCppType(ManagedSharedMem, "boost::interprocess::managed_shared_memory", "<boost/interprocess/managed_shared_memory.hpp>")
defineCppType(MemHandle, "boost::interprocess::managed_shared_memory::handle_t", "<boost/interprocess/managed_shared_memory.hpp>")
defineCppType(SPSCQueue, "rigtorp::SPSCQueue<CBVar>", "SPSCQueue.h")

template removeShmObject(name: string) = invokeFunction("boost::interprocess::shared_memory_object::remove", name.cstring).to(void)

var create_only {.importc: "boost::interprocess::create_only", nodecl.}: int
var open_only {.importc: "boost::interprocess::open_only", nodecl.}: int

# Push - pushes a Var into the shared ringbuffer, a producer
when true:
  type
    CBIpcPush* = object
      name: string
      segment: ptr ManagedSharedMem
      buffer: ptr SPSCQueue
  
  template setup*(b: CBIpcPush) =
    b.segment = nil
    b.buffer = nil

  template cleanup*(b: CBIpcPush) =
    if b.segment != nil:
      cppdel b.segment
      b.segment = nil
    
    # also force ensure removal!
    if b.name != "":
      removeShmObject(b.name)
  
  template inputTypes*(b: CBIpcPush): CBTypesInfo = (AllIntTypes + AllFloatTypes + { None, Bool, String, Color, Enum }, true #[seq]#)
  template outputTypes*(b: CBIpcPush): CBTypesInfo = (AllIntTypes + AllFloatTypes + { None, Bool, String, Color, Enum }, true #[seq]#)
  template parameters*(b: CBIpcPush): CBParametersInfo = @[("Name", { String })]
  template setParam*(b: CBIpcPush; index: int; val: CBVar) = b.name = val.stringValue; cleanup(b)
  template getParam*(b: CBIpcPush; index: int): CBVar = b.name

  proc outgoingString(b: var CBIpcPush; input: CBVar): CBVar {.inline.} =
    result = input

    # Borrow memory from the shared region and copy the string there
    let strlen = input.stringValue.cstring.len
    var stringBuffer = b.segment[].allocate(strlen + 1).to(ptr UncheckedArray[uint8])
    copyMem(stringBuffer, input.stringValue.cstring, strlen)
    stringBuffer[strlen] = 0

    # swap the buffer pointer to point to shared memory
    result.payload.stringValue = cast[CBString](stringBuffer)
    
    # also we need to write the handle to this memory in order to free it on the consumer side, we use reserved bytes in the CBVar
    var handle = b.segment[].get_handle_from_address(stringBuffer).to(MemHandle)
    copyMem(addr result.reserved[0], addr handle, sizeof(MemHandle))
  
  proc outgoingSeq(b: var CBIpcPush; input: CBVar): CBVar {.inline.} =
    result = input

    # Borrow memory from the shared region and copy the while array there
    let seqlen = input.seqValue.len
    var seqBuffer = b.segment[].allocate(seqlen * sizeof(CBVar)).to(ptr UncheckedArray[CBVar])
    copyMem(seqBuffer, input.seqValue, seqlen * sizeof(CBVar))

    # swap the buffer pointer to point to shared memory
    result.payload.seqValue = cast[CBSeq](seqBuffer)

    # set manually the len of the seq
    result.payload.seqLen = seqlen.int32

    # fix up any possible string var
    for i in 0..<result.seqLen:
      var item = result.seqValue[i]
      if item.valueType == String:
        result.seqValue[i] = outgoingString(b, item)
    
    # also we need to write the handle to this memory in order to free it on the consumer side, we use reserved bytes in the CBVar
    var handle = b.segment[].get_handle_from_address(seqBuffer).to(MemHandle)
    copyMem(addr result.reserved[0], addr handle, sizeof(MemHandle))

  template activate*(b: CBIpcPush; context: CBContext; input: CBVar): CBVar =
    if b.segment == nil:
      cppnew(b.segment, ManagedSharedMem, ManagedSharedMem, create_only, b.name.cstring, 1048576) # 1MB of data, 500 vars queue
      b.buffer = b.segment[].invoke("find_or_construct<rigtorp::SPSCQueue<CBVar>>(\"queue\")", 500).to(ptr SPSCQueue)
    
    case input.valueType
    of String:
      var newInput = outgoingString(b, input)
      while not b.buffer[].invoke("try_push", newInput).to(bool):
        # Pause a if the queue is full
        pause(0.0)
    of Seq:
      var newInput = outgoingSeq(b, input)
      while not b.buffer[].invoke("try_push", newInput).to(bool):
        # Pause a if the queue is full
        pause(0.0)
    else:
      while not b.buffer[].invoke("try_push", input).to(bool):
        # Pause a if the queue is full
        pause(0.0)
      
    input
  
  chainblock CBIpcPush, "Push", "IPC"

# Pop - pops a Var from the shared ringbuffer, a consumer
when true:
  type
    CBIpcPop* = object
      name: string
      segment: ptr ManagedSharedMem
      buffer: ptr SPSCQueue
      seqCache: CBSeq
      stringsCache: seq[string]
  
  template setup*(b: CBIpcPop) =
    initSeq(b.seqCache)
    b.segment = nil
    b.buffer = nil
  
  template cleanup*(b: CBIpcPop) =
    if b.segment != nil:
      cppdel b.segment
      b.segment = nil
  
  template inputTypes*(b: CBIpcPop): CBTypesInfo = { Any }
  template outputTypes*(b: CBIpcPop): CBTypesInfo = (AllIntTypes + AllFloatTypes + { None, Bool, String, Color, Enum }, true #[seq]#)
  template parameters*(b: CBIpcPop): CBParametersInfo = @[("Name", { String })]
  template setParam*(b: CBIpcPop; index: int; val: CBVar) = b.name = val.stringValue; cleanup(b)
  template getParam*(b: CBIpcPop; index: int): CBVar = b.name

  proc incomingString(b: var CBIpcPop; stringCache: var string; output: var CBVar) {.inline.} =
    # Fetch the handle of the mem
    var handle: MemHandle
    copyMem(addr handle, addr output.reserved[0], sizeof(MemHandle))

    # Find the address in the local process space
    var address = b.segment[].get_address_from_handle(handle).to(pointer)

    # This is a shared piece of memory, let's use our cache from now
    stringCache.setLen(0)
    stringCache &= $cast[cstring](address)

    # Replace the string memory with the local cache
    output.payload.stringValue = stringCache.cstring.CBString

    # Free up the shared memory
    b.segment[].deallocate(address).to(void)
  
  proc incomingSeq(b: var CBIpcPop; output: var CBVar) {.inline.} =
    # Fetch the handle of the mem
    var handle: MemHandle
    copyMem(addr handle, addr output.reserved[0], sizeof(MemHandle))
    
    # Find the address in the local process space
    var address = b.segment[].get_address_from_handle(handle).to(pointer)
    output.payload.seqValue = cast[CBSeq](address)

    # resets cache
    b.seqCache.clear()
    b.stringsCache.setLen(output.seqLen)
    
    # copy the cache seq
    for i in 0..<output.seqLen:
      var item = output.seqValue[i]
      if item.valueType == String:
        incomingString(b, b.stringsCache[i], item)
      b.seqCache.push(item)

    # Free up the shared memory
    b.segment[].deallocate(address).to(void)

    # Replace the seq withour cache
    output.payload.seqValue = b.seqCache
    # also flag it as dynamic
    output.payload.seqLen = -1 
  
  template activate*(b: CBIpcPop; context: CBContext; input: CBVar): CBVar =
    if b.segment == nil:
      cppnew(b.segment, ManagedSharedMem, ManagedSharedMem, open_only, b.name.cstring)
      b.buffer = b.segment[].invoke("find_or_construct<rigtorp::SPSCQueue<CBVar>>(\"queue\")", 500).to(ptr SPSCQueue)
    
    var output: CBVar
    while true:
      var outputPtr = b.buffer[].invoke("front").to(ptr CBVar)
      
      if outputPtr == nil:
        # not ready, yield
        pause(0.0)
      else:
        output = outputPtr[]
        b.buffer[].invoke("pop").to(void)
        break
    
    case output.valueType
    of String:
      b.stringsCache.setLen(1)
      incomingString(b, b.stringsCache[0], output)
    of Seq:
      incomingSeq(b, output)
    else:
      discard
    
    output
  
  chainblock CBIpcPop, "Pop", "IPC"

when defined(blocksTesting):
# when isMainModule:
  var
    str = "Hello world"
    stringSeq: CBSeq
  stringSeq.push(str)
  stringSeq.push(str)
  stringSeq.push(str)
  stringSeq.push(str)
  stringSeq.push(str)

  withChain testProd:
    Const stringSeq
    IPC.Push "ipctest"
    Sleep 0.2
  
  withChain testCons:
    IPC.Pop "ipctest"
    Log()
  
  testProd.start(true)
  testCons.start(true)

  for _ in 0..50:
  # while true:
    testProd.tick()
    testCons.tick()
    sleep 0.015
  
  testProd.stop()
  testCons.stop()

  destroy testCons
  destroy testProd