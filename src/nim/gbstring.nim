import nimline
import os

const
  modulePath = currentSourcePath().splitPath().head
cppincludes(modulePath & "/../core")

emitc("/*INCLUDESECTION*/#define GB_STRING_IMPLEMENTATION")
emitc("/*INCLUDESECTION*/#include \"chainblocks.hpp\"")

type
  GbString* = object
    gbstr*: cstring

proc `=destroy`*(s: var GbString) =
  when defined(testing): echo "=destroy called on: ", s
  if s.gbstr != nil:
    invokeFunction("gb_free_string", s.gbstr).to(void)
  s.gbstr = nil

proc `=`*(a: var GbString; b: GbString) =
  when defined(testing): echo "= called on: ", a
  if a.gbstr == b.gbstr: return
  `=destroy`(a)
  a.gbstr = invokeFunction("gb_duplicate_string", b.gbstr).to(cstring)

proc `=move`*(a, b: var GbString) =
  when defined(testing): echo "=move called on: ", a
  if a.gbstr == b.gbstr: return
  `=destroy`(a)
  a.gbstr = b.gbstr

proc `=sink`*(a: var GbString; b: GbString) =
  when defined(testing): echo "=sink called on: ", a
  if a.gbstr == b.gbstr: return
  `=destroy`(a)
  a.gbstr = b.gbstr

converter toGbString*(nstr: string): GbString {.inline, noinit.} =
  result.gbstr = invokeFunction("gb_make_string_length", nstr.cstring, nstr.len).to(cstring)

proc len*(s: GbString): int {.inline.} =
  if s.gbstr == nil: return 0
  invokeFunction("gb_string_length", s.gbstr).to(int)

proc capacity*(s: GbString): int {.inline.} =
  if s.gbstr == nil: return 0
  invokeFunction("gb_string_capacity", s.gbstr).to(int)

proc clear*(s: var GbString) {.inline.} =
  if s.gbstr == nil: s.gbstr = invokeFunction("gb_make_string", "").to(cstring)
  else: invokeFunction("gb_clear_string", s.gbstr).to(void)

proc setLen*(s: var GbString; len: int) {.inline.} =
  if s.gbstr == nil: s.gbstr = invokeFunction("gb_make_string", "").to(cstring)
  s.gbstr = invokeFunction("gb_string_make_space_for", s.gbstr, len).to(cstring)

proc `&=`*(a: var GbString, b: GbString) {.inline.} =
  if a.gbstr == nil: a.gbstr = invokeFunction("gb_make_string", "").to(cstring)
  a.gbstr = invokeFunction("gb_append_string", a.gbstr, b.gbstr).to(cstring)

proc `&`*(a: GbString, b: GbString): GbString {.inline.} =
  result.gbstr = invokeFunction("gb_duplicate_string", a.gbstr).to(cstring)
  result.gbstr = invokeFunction("gb_append_string", result.gbstr, b.gbstr).to(cstring)

when isMainModule:
  var
    gs: GbString = "Hello world"
    g1: GbString = " and "
    g2: GbString = "Buonanotte"
  
  echo gs
  echo gs & g1 & g2
  gs &= g1
  gs &= g2
