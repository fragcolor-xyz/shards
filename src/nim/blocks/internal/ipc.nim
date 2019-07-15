import nimline
import ../../chainblocks

type IPC* = object

defineCppType(ManagedSharedMem, "boost::interprocess::managed_shared_memory", "<boost/interprocess/managed_shared_memory.hpp>")
defineCppType(LockFreeRingbuffer, "boost::lockfree::spsc_queue<CBVar, boost::lockfree::capacity<500>>", "<boost/lockfree/spsc_queue.hpp>")
defineCppType(MemHandle, "boost::interprocess::managed_shared_memory::handle_t", "<boost/interprocess/managed_shared_memory.hpp>")

template removeShmObject(name: string) = invokeFunction("boost::interprocess::shared_memory_object::remove", name.cstring).to(void)

var create_only {.importc: "boost::interprocess::create_only", nodecl.}: int
var open_only {.importc: "boost::interprocess::open_only", nodecl.}: int

# Push - pushes a Var into the shared ringbuffer, a producer
when true:
  type
    CBIpcPush* = object
      name: string
      segment: ptr ManagedSharedMem
      buffer: ptr LockFreeRingbuffer
  
  template cleanup*(b: CBIpcPush) =
    if b.segment != nil:
      cppdel b.segment
      b.segment = nil
    # also force ensure removal!
    # this might crash any current user if existing btw
    removeShmObject(b.name)
  template inputTypes*(b: CBIpcPush): CBTypesInfo = ({ None, Bool, Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color, Enum }, true #[seq]#)
  template outputTypes*(b: CBIpcPush): CBTypesInfo = ({ None, Bool, Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color, Enum }, true #[seq]#)
  template parameters*(b: CBIpcPush): CBParametersInfo = @[("Name", { String })]
  template setParam*(b: CBIpcPush; index: int; val: CBVar) =
    b.name = val.stringValue
    cleanup(b)
  template getParam*(b: CBIpcPush; index: int): CBVar = b.name
  template activate*(b: var CBIpcPush; context: CBContext; input: CBVar): CBVar =
    if b.segment == nil:
      cppnew(b.segment, ManagedSharedMem, ManagedSharedMem, create_only, b.name.cstring, 1048576) # 1MB of data, 500 vars queue
      b.buffer = b.segment[].invoke("find_or_construct<boost::lockfree::spsc_queue<CBVar, boost::lockfree::capacity<500>>>(\"queue\")").to(ptr LockFreeRingbuffer)
    
    case input.valueType
    of String:
      let strlen = input.stringValue.cstring.len
      # Borrow memory from the shared region and copy the string there
      var
        newInput = input
        stringBuffer = b.segment[].allocate(strlen + 1).to(ptr UncheckedArray[uint8])
      copyMem(stringBuffer, input.stringValue.cstring, strlen)
      stringBuffer[strlen] = 0
      # swap the buffer pointer to point to shared memory
      newInput.stringValue = cast[CBString](stringBuffer)
      # also we need to write the handle to this memory in order to free it on the consumer side, we use reserved bytes in the CBVar
      var handle = b.segment[].get_handle_from_address(stringBuffer).to(MemHandle)
      copyMem(addr newInput.reserved[0], addr handle, sizeof(MemHandle))
      while not b.buffer[].invoke("push", newInput).to(bool):
        # Pause a if the queue is full
        pause(0.0)
    of Seq:
      discard
    else:
      while not b.buffer[].invoke("push", input).to(bool):
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
      buffer: ptr LockFreeRingbuffer
      stringCache: string
  
  template cleanup*(b: CBIpcPush) =
    if b.segment != nil:
      cppdel b.segment
      b.segment = nil
  template inputTypes*(b: CBIpcPop): CBTypesInfo = { Any }
  template outputTypes*(b: CBIpcPop): CBTypesInfo = ({ None, Bool, Int, Int2, Int3, Int4, Float, Float2, Float3, Float4, String, Color, Enum }, true #[seq]#)
  template parameters*(b: CBIpcPop): CBParametersInfo = @[("Name", { String })]
  template setParam*(b: CBIpcPop; index: int; val: CBVar) =
    b.name = val.stringValue
    cleanup(b)
  template getParam*(b: CBIpcPop; index: int): CBVar = b.name
  template activate*(b: var CBIpcPop; context: CBContext; input: CBVar): CBVar =
    if b.segment == nil:
      # given that our CBVar is 48, this allows us to buffer 2500 (actually 2450 due to other mem used internally) of them (see LockFreeRingbuffer), of course roughly, given strings might use more mem
      cppnew(b.segment, ManagedSharedMem, ManagedSharedMem, open_only, b.name.cstring)
      b.buffer = b.segment[].invoke("find_or_construct<boost::lockfree::spsc_queue<CBVar, boost::lockfree::capacity<500>>>(\"queue\")").to(ptr LockFreeRingbuffer)
    
    var output: CBVar
    while b.buffer[].invoke("pop", output).to(int) == 0:
      # Pause if not avail yet
      pause(0.0)
    
    case output.valueType
    of String:
      # This is a shared piece of memory, let's use our cache from now
      b.stringCache.setLen(0)
      b.stringCache &= $output.stringValue.cstring
      # Fetch the handle of the mem
      var handle: MemHandle
      copyMem(addr handle, addr output.reserved[0], sizeof(MemHandle))
      # Free up the shared memory
      b.segment[].deallocate(b.segment[].get_address_from_handle(handle)).to(void)
      # Replace the string memory with the local cache
      output.stringValue = b.stringCache.cstring.CBString
    else:
      discard
    
    output
  
  chainblock CBIpcPop, "Pop", "IPC"

when isMainModule:
  withChain testProd:
    Const "Hello world"
    IPC.Push "ipctest"
    Sleep 0.2
  
  withChain testCons:
    IPC.Pop "ipctest"
    Log()
  
  testProd.start(true)
  testCons.start(true)

  for _ in 0..50:
    testProd.tick()
    testCons.tick()
    sleep 0.015
  
  discard testProd.stop()
  discard testCons.stop()

  destroy testCons
  destroy testProd