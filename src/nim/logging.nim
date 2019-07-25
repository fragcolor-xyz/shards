import nimline

proc dlog*[T](args: varargs[T]) {.inline.} =
  when not defined(release):
    for arg in args:
      let carg = ($arg).cstring
      emitc("(DLOG(DEBUG) << ", `carg`, ");")
  else:
    discard

proc log*[T](args: varargs[T]) {.inline.} =
  for arg in args:
    let carg = ($arg).cstring
    emitc("(LOG(INFO) << ", `carg`, ");")

proc dlogs*(msg: cstring) {.inline.} =
  when not defined(release):
    emitc("(DLOG(DEBUG) << ", `msg`, ");")
  else:
    discard

proc logs*(msg: cstring) {.inline.} =
  emitc("(LOG(INFO) << ", `msg`, ");")