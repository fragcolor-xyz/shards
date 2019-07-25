import nimline

proc dlog*(args: varargs[string, `$`]) {.inline.} =
  when not defined(release):
    var msg = ""
    for arg in args: msg &= arg
    let cmsg = msg.cstring
    emitc("(DLOG(DEBUG) << ", `cmsg`, ");")
  else:
    discard

proc log*(args: varargs[string, `$`]) {.inline.} =
  var msg = ""
  for arg in args: msg &= arg
  let cmsg = msg.cstring
  emitc("(LOG(INFO) << ", `cmsg`, ");")

proc dlogs*(msg: cstring) {.inline.} =
  when not defined(release):
    emitc("(DLOG(DEBUG) << ", `msg`, ");")
  else:
    discard

proc logs*(msg: cstring) {.inline.} =
  emitc("(LOG(INFO) << ", `msg`, ");")