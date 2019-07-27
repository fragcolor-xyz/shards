import nimline
import os

const
  modulePath = currentSourcePath().splitPath().head
cppincludes(modulePath & "/../core")

emitc("/*INCLUDESECTION*/#include \"chainblocks.hpp\"")

type
  StbSeq*[T] = object
    stbSeq: ptr UncheckedArray[T]

proc setCapacity*(s: var StbSeq; newCap: int) {.inline.} =
  invokeFunction("stbds_arrsetcap", s.stbSeq, newCap).to(void)

proc push*[T](s: var StbSeq[T]; val: sink T) {.inline.} =
  invokeFunction("stbds_arrpush", s.stbSeq, val).to(void)

proc `=destroy`*[T](s: var StbSeq[T]) =
  when defined(testing): echo "=destroy called on: ", s
  if s.stbSeq != nil:
    for i in 0..<s.len:
      `=destroy`(s[i])
    invokeFunction("stbds_arrfree", s.stbSeq).to(void)
  s.stbSeq = nil

proc `=`*[T](a: var StbSeq[T]; b: StbSeq[T]) =
  when defined(testing): echo "= called on: ", a
  if a.stbSeq == b.stbSeq: return
  `=destroy`(a)
  a.stbSeq = nil
  a.setCapacity(b.len)
  for v in b:
    a.push(v)

proc `=move`*[T](a, b: var StbSeq[T]) =
  when defined(testing): echo "=move called on: ", a
  if a.stbSeq == b.stbSeq: return
  `=destroy`(a)
  a.stbSeq = b.stbSeq
  b.stbSeq = nil

proc `=sink`*[T](a: var StbSeq[T]; b: StbSeq[T]) =
  when defined(testing): echo "=sink called on: ", a
  if a.stbSeq == b.stbSeq: return
  `=destroy`(a)
  a.stbSeq = b.stbSeq

proc setLen*[T](s: var StbSeq[T]; newLen: int) {.inline.} =
  if s.len > newLen:
    for i in newLen..s.high:
      `=destroy`(s.stbSeq[i])
  invokeFunction("stbds_arrsetlen", s.stbSeq, newLen).to(void)
  zeroMem(addr s.stbSeq[0], sizeof(T) * newLen)

proc `[]`*[T](s: var StbSeq[T]; index: int): var T {.inline, noinit.} =
  assert index < s.len
  s.stbSeq[index]

proc `[]`*[T](s: StbSeq[T]; index: int): T {.inline, noinit.} =
  assert index < s.len
  s.stbSeq[index]

proc `[]=`*[T](s: var StbSeq[T]; index: int; value: sink T) {.inline.} =
  assert index < s.len
  s.stbSeq[index] = value

proc pop*[T](s: var StbSeq[T]): T {.inline, noinit.} = invokeFunction("stbds_arrpop", s.stbSeq).to(T)

proc len*(s: StbSeq): int {.inline.} = invokeFunction("stbds_arrlen", s.stbSeq).to(int)

proc high*(s: StbSeq): int {.inline.} = invokeFunction("stbds_arrlen", s.stbSeq).to(int) - 1

proc capacity*(s: StbSeq): int {.inline.} = invokeFunction("stbds_arrcap", s.stbSeq).to(int)

proc clear*(s: var StbSeq) {.inline.} = setLen(s, 0)

iterator mitems*[T](s: StbSeq[T]): var T {.inline.} =
  for i in 0..<s.len:
    yield s.stbSeq[i]

iterator items*[T](s: StbSeq[T]): T {.inline.} =
  for i in 0..<s.len:
    yield s.stbSeq[i]

proc `*@`*[IDX, T](a: array[IDX, T]): StbSeq[T] =
  result.setCapacity(a.len)
  for v in a:
    result.push(v)

proc `$`*[T](s: StbSeq[T]): string =
  result = "["
  for i in 0..<s.len:
    if i != 0:
      result &= ", " & $s[i]
    else:
      result &= $s[i]
  result &= "]"

when isMainModule:
  cppdefines("STB_DS_IMPLEMENTATION")
  proc run() =
    block:
      var
        x = *@[1, 2, 3, 4]
        y = *@[12, 22, 32, 42]
      let
        z = *@[13, 23, 33, 43]
      echo x
      x = *@[5, 6, 7]
      echo x
      x = *@[5, 6, 7, 8, 9, 10, 11, 12]
      echo x
      x = y
      echo y
      echo x
      x = z
      echo x
    echo "Done"
  run()