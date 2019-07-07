# from: https://github.com/sinkingsugar/packedjson originally: https://github.com/Araq/packedjson

#
#
#            Nim's Runtime Library
#        (c) Copyright 2018 Nim contributors
#
#    See the file "copying.txt", included in this
#    distribution, for details about the copyright.
#

## Packedjson is an alternative JSON tree implementation that takes up far
## less space than Nim's stdlib JSON implementation. The space savings can be as much
## as 80%. It can be faster or much slower than the stdlib's JSON, depending on the
## workload.

##[ **Note**: This library distinguishes between ``JsonTree`` and ``JsonNode``
types. Only ``JsonTree`` can be mutated and accessors like ``[]`` return a
``JsonNode`` which is merely an immutable view into a ``JsonTree``. This
prevents most forms of unsupported aliasing operations like:

.. code-block:: nim
    var arr = newJArray()
    arr.add newJObject()
    var x = arr[0]
    # Error: 'x' is of type JsonNode and cannot be mutated:
    x["field"] = %"value"

(You can use the ``copy`` operation to create an explicit copy that then
can be mutated.)

A ``JsonTree`` that is added to another ``JsonTree`` gets copied:

.. code-block:: nim
    var x = newJObject()
    var arr = newJArray()
    arr.add x
    x["field"] = %"value"
    assert $arr == "[{}]"

These semantics also imply that code like ``myobj["field"]["nested"] = %4``
needs instead be written as ``myobj["field", "nested"] = %4`` so that the
changes are end up in the tree.
]##

import parsejson, parseutils, streams, strutils, macros
from unicode import toUTF8, Rune

import std / varints

type
  JsonNodeKind* = enum ## possible JSON node types
    JNull,
    JBool,
    JInt,
    JFloat,
    JString,
    JObject,
    JArray

# An atom is always prefixed with its kind and for JString, JFloat, JInt also
# with a length information. The length information is either part of the first byte
# or a variable length integer. An atom is everything
# that is not an object or an array. Since we don't know the elements in an array
# or object upfront during parsing, these are not stored with any length
# information. Instead the 'opcodeEnd' marker is produced. This means that during
# traversal we have to be careful of the nesting and always jump over the payload
# of an atom. However, this means we can save key names unescaped which speeds up
# most operations.

const
  opcodeBits = 3

  opcodeNull = ord JNull
  opcodeBool = ord JBool
  opcodeFalse = opcodeBool
  opcodeTrue = opcodeBool or 0b0000_1000
  opcodeInt = ord JInt
  opcodeFloat = ord JFloat
  opcodeString = ord JString
  opcodeObject = ord JObject
  opcodeArray = ord JArray
  opcodeEnd = 7

  opcodeMask = 0b111

proc storeAtom(buf: var seq[byte]; k: JsonNodeKind; data: string) =
  if data.len < 0b1_1111:
    # we can store the length in the upper bits of the kind field.
    buf.add byte(data.len shl 3) or byte(k)
  else:
    # we need to store the kind field and the length separately:
    buf.add 0b1111_1000u8 or byte(k)
    # ensure we have the space:
    let start = buf.len
    buf.setLen start + maxVarIntLen
    let realVlen = writeVu64(toOpenArray(buf, start, start + maxVarIntLen - 1), uint64 data.len)
    buf.setLen start + realVlen
  for i in 0..high(data):
    buf.add byte(data[i])

proc beginContainer(buf: var seq[byte]; k: JsonNodeKind) =
  buf.add byte(k)

proc endContainer(buf: var seq[byte]) = buf.add byte(opcodeEnd)

type
  JsonStorage = ref seq[byte]
  JsonNode* = object
    k: JsonNodeKind
    a, b: int
    t: JsonStorage

  JsonTree* = distinct JsonNode ## a JsonTree is a JsonNode that can be mutated.

  JsonContext = object
    cache: seq[owned(JsonStorage)]

var mainContext {.threadvar.}: JsonContext

proc purgeJsonCache*() =
  when defined nimV2:
    mainContext.cache = @[] # SetLen and newruntime seem not to work properly
  else:
    mainContext.cache.setLen(0)

proc newStorage(): JsonStorage =
  var storage = new JsonStorage
  result = unown storage
  mainContext.cache.add(storage)

converter toJsonNode*(x: JsonTree): JsonNode {.inline.} = JsonNode(x)

proc newJNull*(): JsonNode =
  ## Creates a new `JNull JsonNode`.
  result.k = JNull

template newBody(kind, x) =
  result.t = newStorage()
  result.t[] = @[]
  storeAtom(result.t[], kind, x)
  result.a = 0
  result.b = high(result.t[])
  result.k = kind

proc newJString*(s: string): JsonNode =
  ## Creates a new `JString JsonNode`.
  newBody JString, s

proc newJInt*(n: BiggestInt): JsonNode =
  ## Creates a new `JInt JsonNode`.
  newBody JInt, $n

proc newJFloat*(n: float): JsonNode =
  ## Creates a new `JFloat JsonNode`.
  newBody JFloat, formatFloat(n)

proc newJBool*(b: bool): JsonNode =
  ## Creates a new `JBool JsonNode`.
  result.k = JBool
  result.t = newStorage()
  result.t[] = @[if b: byte(opcodeTrue) else: byte(opcodeFalse)]
  result.a = 0
  result.b = high(result.t[])

proc newJObject*(): JsonTree =
  ## Creates a new `JObject JsonNode`
  JsonNode(result).k = JObject
  JsonNode(result).t = newStorage()
  JsonNode(result).t[] = @[byte opcodeObject, byte opcodeEnd]
  JsonNode(result).a = 0
  JsonNode(result).b = high(JsonNode(result).t[])

proc newJArray*(): JsonTree =
  ## Creates a new `JArray JsonNode`
  JsonNode(result).k = JArray
  JsonNode(result).t = newStorage()
  JsonNode(result).t[] = @[byte opcodeArray, byte opcodeEnd]
  JsonNode(result).a = 0
  JsonNode(result).b = high(JsonNode(result).t[])

proc kind*(x: JsonNode): JsonNodeKind = x.k

proc extractLen(x: seq[byte]; pos: int): int =
  if (x[pos] and 0b1111_1000u8) != 0b1111_1000u8:
    result = int(x[pos]) shr 3
  else:
    # we had an overflow for inline length information,
    # so extract the variable sized integer:
    var varint: uint64
    let varintLen = readVu64(toOpenArray(x, pos+1, min(pos + 1 + maxVarIntLen, x.high)), varint)
    result = int(varint) + varintLen

proc extractSlice(x: seq[byte]; pos: int): (int, int) =
  if (x[pos] and 0b1111_1000u8) != 0b1111_1000u8:
    result = (pos + 1, int(x[pos]) shr 3)
  else:
    # we had an overflow for inline length information,
    # so extract the variable sized integer:
    var varint: uint64
    let varintLen = readVu64(toOpenArray(x, pos+1, min(pos + 1 + maxVarIntLen, x.high)), varint)
    result = (pos + 1 + varintLen, int(varint))

proc skip(x: seq[byte]; start: int; elements: var int): int =
  var nested = 0
  var pos = start
  while true:
    let k = x[pos] and opcodeMask
    var nextPos = pos + 1
    case k
    of opcodeNull, opcodeBool:
      if nested == 0: inc elements
    of opcodeInt, opcodeFloat, opcodeString:
      let L = extractLen(x, pos)
      nextPos = pos + 1 + L
      if nested == 0: inc elements
    of opcodeObject, opcodeArray:
      if nested == 0: inc elements
      inc nested
    of opcodeEnd:
      if nested == 0: return nextPos
      dec nested
    else: discard
    pos = nextPos

iterator items*(x: JsonNode): JsonNode =
  ## Iterator for the items of `x`. `x` has to be a JArray.
  assert x.kind == JArray
  var pos = x.a+1
  var dummy: int
  while pos <= x.b:
    let k = x.t[pos] and opcodeMask
    var nextPos = pos + 1
    case k
    of opcodeNull, opcodeBool: discard
    of opcodeInt, opcodeFloat, opcodeString:
      let L = extractLen(x.t[], pos)
      nextPos = pos + 1 + L
    of opcodeObject, opcodeArray:
      nextPos = skip(x.t[], pos+1, dummy)
    of opcodeEnd: break
    else: discard
    yield JsonNode(k: JsonNodeKind(k), a: pos, b: nextPos-1, t: x.t)
    pos = nextPos

iterator pairs*(x: JsonNode): (string, JsonNode) =
  ## Iterator for the pairs of `x`. `x` has to be a JObject.
  assert x.kind == JObject
  var pos = x.a+1
  var dummy: int
  var key = newStringOfCap(60)
  while pos <= x.b:
    let k2 = x.t[pos] and opcodeMask
    if k2 == opcodeEnd: break

    assert k2 == opcodeString, $k2
    let (start, L) = extractSlice(x.t[], pos)
    key.setLen L
    for i in 0 ..< L: key[i] = char(x.t[start+i])
    pos = start + L

    let k = x.t[pos] and opcodeMask
    var nextPos = pos + 1
    case k
    of opcodeNull, opcodeBool: discard
    of opcodeInt, opcodeFloat, opcodeString:
      let L = extractLen(x.t[], pos)
      nextPos = pos + 1 + L
    of opcodeObject, opcodeArray:
      nextPos = skip(x.t[], pos+1, dummy)
    of opcodeEnd: doAssert false, "unexpected end of object"
    else: discard
    yield (key, JsonNode(k: JsonNodeKind(k), a: pos, b: nextPos-1, t: x.t))
    pos = nextPos

proc rawGet(x: JsonNode; name: string): JsonNode =
  assert x.kind == JObject
  var pos = x.a+1
  var dummy: int
  while pos <= x.b:
    let k2 = x.t[pos] and opcodeMask
    if k2 == opcodeEnd: break

    assert k2 == opcodeString, $k2
    let (start, L) = extractSlice(x.t[], pos)
    # compare for the key without creating the temp string:
    var isMatch = name.len == L
    if isMatch:
      for i in 0 ..< L:
        if name[i] != char(x.t[start+i]):
          isMatch = false
          break
    pos = start + L

    let k = x.t[pos] and opcodeMask
    var nextPos = pos + 1
    case k
    of opcodeNull, opcodeBool: discard
    of opcodeInt, opcodeFloat, opcodeString:
      let L = extractLen(x.t[], pos)
      nextPos = pos + 1 + L
    of opcodeObject, opcodeArray:
      nextPos = skip(x.t[], pos+1, dummy)
    of opcodeEnd: doAssert false, "unexpected end of object"
    else: discard
    if isMatch:
      return JsonNode(k: JsonNodeKind(k), a: pos, b: nextPos-1, t: x.t)
    pos = nextPos
  result.a = -1

proc `[]`*(x: JsonNode; name: string): JsonNode =
  ## Gets a field from a `JObject`.
  ## If the value at `name` does not exist, raises KeyError.
  result = rawGet(x, name)
  if result.a < 0:
    raise newException(KeyError, "key not found in object: " & name)

proc len*(n: JsonNode): int =
  ## If `n` is a `JArray`, it returns the number of elements.
  ## If `n` is a `JObject`, it returns the number of pairs.
  ## Else it returns 0.
  if n.k notin {JArray, JObject}: return 0
  discard skip(n.t[], n.a+1, result)
  # divide by two because we counted the pairs wrongly:
  if n.k == JObject: result = result shr 1

proc rawAdd(obj: var JsonNode; child: seq[byte]; a, b: int) =
  let pa = obj.b
  let L = b - a + 1
  let oldfull = obj.t[].len
  setLen(obj.t[], oldfull+L)
  # now move the tail to the new end so that we can insert effectively
  # into the middle:
  for i in countdown(oldfull+L-1, pa+L):
    shallowCopy(obj.t[][i], obj.t[][i-L])
  # insert into the middle:
  for i in 0 ..< L:
    obj.t[][pa + i] = child[a + i]
  inc obj.b, L

proc rawAddWithNull(parent: var JsonNode; child: JsonNode) =
  if child.k == JNull:
    let pa = parent.b
    let oldLen = parent.t[].len
    setLen(parent.t[], oldLen + 1)
    for i in pa .. oldLen-1:
      parent.t[][1 + i] = parent.t[][i]
    parent.t[][pa] = byte opcodeNull
    inc parent.b, 1
  else:
    rawAdd(parent, child.t[], child.a, child.b)

proc add*(parent: var JsonTree; child: JsonNode) =
  doAssert parent.kind == JArray, "parent is not a JArray"
  rawAddWithNull(JsonNode(parent), child)

proc add*(obj: var JsonTree, key: string, val: JsonNode) =
  ## Sets a field from a `JObject`. **Warning**: It is currently not checked
  ## but assumed that the object does not yet have a field named `key`.
  assert obj.kind == JObject
  let k = newJstring(key)
  # XXX optimize this further!
  rawAdd(JsonNode obj, k.t[], 0, high(k.t[]))
  rawAddWithNull(JsonNode obj, val)
  when false:
    discard "XXX assert that the key does not exist yet"

proc rawPut(obj: var JsonNode, oldval: JsonNode, key: string, val: JsonNode): int =
  let oldlen = oldval.b - oldval.a + 1
  let newlen = val.b - val.a + 1
  result = newlen - oldlen
  if result == 0:
    for i in 0 ..< newlen:
      obj.t[][oldval.a + i] = (if val.k == JNull: byte opcodeNull else: val.t[][i])
  else:
    let oldfull = obj.t[].len
    if newlen > oldlen:
      setLen(obj.t[], oldfull+result)
      # now move the tail to the new end so that we can insert effectively
      # into the middle:
      for i in countdown(oldfull+result-1, oldval.a+newlen): shallowCopy(obj.t[][i], obj.t[][i-result])
    else:
      for i in countup(oldval.a+newlen, oldfull+result-1): shallowCopy(obj.t[][i], obj.t[][i-result])
      # cut down:
      setLen(obj.t[], oldfull+result)
    # overwrite old value:
    for i in 0 ..< newlen:
      obj.t[][oldval.a + i] = (if val.k == JNull: byte opcodeNull else: val.t[][i])

proc `[]=`*(obj: var JsonTree, key: string, val: JsonNode) =
  let oldval = rawGet(obj, key)
  if oldval.a < 0:
    add(obj, key, val)
  else:
    let diff = rawPut(JsonNode obj, oldval, key, val)
    inc JsonNode(obj).b, diff

macro `[]=`*(obj: var JsonTree, keys: varargs[typed], val: JsonNode): untyped =
  ## keys can be strings or integers for the navigation.
  result = newStmtList()
  template t0(obj, key) {.dirty.} =
    var oldval = obj[key]

  template ti(key) {.dirty.} =
    oldval = oldval[key]

  template tput(obj, finalkey, val) =
    let diff = rawPut(JsonNode obj, oldval, finalkey, val)
    inc JsonNode(obj).b, diff

  result.add getAst(t0(obj, keys[0]))
  for i in 1..<len(keys):
    result.add getAst(ti(keys[i]))
  result.add getAst(tput(obj, keys[len(keys)-1], val))
  result = newBlockStmt(result)

  when false:
    var oldval = rawGet(obj, keys[0])
    if oldval.a < 0:
      raise newException(KeyError, "key not found in object: " & keys[0])
    for i in 1..high(keys):
      oldval = rawGet(oldval, keys[i])
      if oldval.a < 0:
        raise newException(KeyError, "key not found in object: " & keys[i])

    let diff = rawPut(JsonNode obj, oldval, keys[high(keys)], val)
    inc JsonNode(obj).b, diff

proc rawDelete(x: var JsonNode, key: string) =
  assert x.kind == JObject
  var pos = x.a+1
  var dummy: int
  while pos <= x.b:
    let k2 = x.t[pos] and opcodeMask
    if k2 == opcodeEnd: break

    assert k2 == opcodeString, $k2
    let begin = pos
    let (start, L) = extractSlice(x.t[], pos)
    # compare for the key without creating the temp string:
    var isMatch = key.len == L
    if isMatch:
      for i in 0 ..< L:
        if key[i] != char(x.t[start+i]):
          isMatch = false
          break
    pos = start + L

    let k = x.t[pos] and opcodeMask
    var nextPos = pos + 1
    case k
    of opcodeNull, opcodeBool: discard
    of opcodeInt, opcodeFloat, opcodeString:
      let L = extractLen(x.t[], pos)
      nextPos = pos + 1 + L
    of opcodeObject, opcodeArray:
      nextPos = skip(x.t[], pos+1, dummy)
    of opcodeEnd: doAssert false, "unexpected end of object"
    else: discard
    if isMatch:
      let diff = nextPos - begin
      let oldfull = x.t[].len
      for i in countup(begin, oldfull-diff-1): shallowCopy(x.t[][i], x.t[][i+diff])
      setLen(x.t[], oldfull-diff)
      dec x.b, diff
      return
    pos = nextPos
  # for compatibility with json.nim, we need to raise an exception
  # here. Not sure it's good idea.
  raise newException(KeyError, "key not in object: " & key)

proc delete*(x: var JsonTree, key: string) =
  ## Deletes ``x[key]``.
  rawDelete(JsonNode x, key)

proc `%`*(s: string): JsonNode =
  ## Generic constructor for JSON data. Creates a new `JString JsonNode`.
  newJString(s)

proc `%`*(n: BiggestInt): JsonNode =
  ## Generic constructor for JSON data. Creates a new `JInt JsonNode`.
  newJInt(n)

proc `%`*(n: float): JsonNode =
  ## Generic constructor for JSON data. Creates a new `JFloat JsonNode`.
  result = newJFloat(n)

proc `%`*(b: bool): JsonNode =
  ## Generic constructor for JSON data. Creates a new `JBool JsonNode`.
  result = newJBool(b)

template `%`*(j: JsonNode): JsonNode = j

proc `%`*(keyVals: openArray[tuple[key: string, val: JsonNode]]): JsonTree =
  ## Generic constructor for JSON data. Creates a new `JObject JsonNode`
  if keyvals.len == 0: return newJArray()
  result = newJObject()
  for key, val in items(keyVals): result.add key, val

proc `%`*[T](elements: openArray[T]): JsonTree =
  ## Generic constructor for JSON data. Creates a new `JArray JsonNode`
  result = newJArray()
  for elem in elements: result.add(%elem)

proc `%`*(o: object): JsonTree =
  ## Generic constructor for JSON data. Creates a new `JObject JsonNode`
  result = newJObject()
  for k, v in o.fieldPairs: result[k] = %v

proc `%`*(o: ref object): JsonTree =
  ## Generic constructor for JSON data. Creates a new `JObject JsonNode`
  if o.isNil:
    result = newJNull()
  else:
    result = %(o[])

proc `%`*(o: enum): JsonNode =
  ## Construct a JsonNode that represents the specified enum value as a
  ## string. Creates a new ``JString JsonNode``.
  result = %($o)

proc toJson(x: NimNode): NimNode {.compiletime.} =
  case x.kind
  of nnkBracket: # array
    if x.len == 0: return newCall(bindSym"newJArray")
    result = newNimNode(nnkBracket)
    for i in 0 ..< x.len:
      result.add(toJson(x[i]))
    result = newCall(bindSym"%", result)
  of nnkTableConstr: # object
    if x.len == 0: return newCall(bindSym"newJObject")
    result = newNimNode(nnkStmtListExpr)
    var res = gensym(nskVar, "cons")
    result.add newVarStmt(res, newCall(bindSym"newJObject"))
    for i in 0 ..< x.len:
      x[i].expectKind nnkExprColonExpr
      result.add newCall(bindSym"[]=", res, x[i][0], toJson(x[i][1]))
    result.add res
  of nnkCurly: # empty object
    x.expectLen(0)
    result = newCall(bindSym"newJObject")
  of nnkNilLit:
    result = newCall(bindSym"newJNull")
  of nnkPar:
    if x.len == 1: result = toJson(x[0])
    else: result = newCall(bindSym"%", x)
  else:
    result = newCall(bindSym"%", x)

macro `%*`*(x: untyped): untyped =
  ## Convert an expression to a JsonNode directly, without having to specify
  ## `%` for every element.
  result = toJson(x)

proc copy*(n: JsonNode): JsonTree =
  ## Performs a deep copy of `a`.
  JsonNode(result).k = n.k
  JsonNode(result).a = n.a
  JsonNode(result).b = n.b
  if n.t != nil:
    JsonNode(result).t = newStorage()
    JsonNode(result).t[] = n.t[]

proc getStr*(n: JsonNode, default: string = ""): string =
  ## Retrieves the string value of a `JString JsonNode`.
  ##
  ## Returns ``default`` if ``n`` is not a ``JString``.
  if n.kind != JString: return default

  let (start, L) = extractSlice(n.t[], n.a)
  # XXX use copyMem here:
  result = newString(L)
  for i in 0 ..< L:
    result[i] = char(n.t[start+i])

proc myParseInt(s: seq[byte]; first, last: int): BiggestInt =
  var i = first
  var isNegative = false
  if i < last:
    case chr(s[i])
    of '+': inc(i)
    of '-':
      isNegative = true
      inc(i)
    else: discard
  if i <= last and chr(s[i]) in {'0'..'9'}:
    var res = 0u64
    while i <= last and chr(s[i]) in {'0'..'9'}:
      let c = uint64(ord(s[i]) - ord('0'))
      if res <= (0xFFFF_FFFF_FFFF_FFFFu64 - c) div 10:
        res = res * 10u64 + c
      inc(i)
    if isNegative:
      if res >= uint64(high(BiggestInt))+1:
        result = low(BiggestInt)
      else:
        result = -BiggestInt(res)
    else:
      if res >= uint64(high(BiggestInt)):
        result = high(BiggestInt)
      else:
        result = BiggestInt(res)

proc getInt*(n: JsonNode, default: int = 0): int =
  ## Retrieves the int value of a `JInt JsonNode`.
  ##
  ## Returns ``default`` if ``n`` is not a ``JInt``, or if ``n`` is nil.
  if n.kind != JInt: return default
  let (start, L) = extractSlice(n.t[], n.a)
  result = int(myParseInt(n.t[], start, start + L - 1))

proc getBiggestInt*(n: JsonNode, default: BiggestInt = 0): BiggestInt =
  ## Retrieves the BiggestInt value of a `JInt JsonNode`.
  ##
  ## Returns ``default`` if ``n`` is not a ``JInt``, or if ``n`` is nil.
  if n.kind != JInt: return default
  let (start, L) = extractSlice(n.t[], n.a)
  result = myParseInt(n.t[], start, start + L - 1)

proc getFloat*(n: JsonNode, default: float = 0.0): float =
  ## Retrieves the float value of a `JFloat JsonNode`.
  ##
  ## Returns ``default`` if ``n`` is not a ``JFloat`` or ``JInt``, or if ``n`` is nil.
  case n.kind
  of JFloat, JInt:
    let (start, L) = extractSlice(n.t[], n.a)
    if parseFloat(cast[string](n.t[]), result, start) != L:
      # little hack ahead: If parsing failed because of following control bytes,
      # patch the control byte, do the parsing and patch the control byte back:
      let old = n.t[][start+L]
      n.t[][start+L] = 0
      doAssert parseFloat(cast[string](n.t[]), result, start) == L
      n.t[][start+L] = old
  else:
    result = default

proc getBool*(n: JsonNode, default: bool = false): bool =
  ## Retrieves the bool value of a `JBool JsonNode`.
  ##
  ## Returns ``default`` if ``n`` is not a ``JBool``, or if ``n`` is nil.
  if n.kind == JBool: result = (n.t[n.a] shr opcodeBits) == 1
  else: result = default

proc isEmpty(n: JsonNode): bool =
  assert n.kind in {JArray, JObject}
  result = n.t[n.a+1] == opcodeEnd

template escape(result, c) =
  case c
  of '\L': result.add("\\n")
  of '\b': result.add("\\b")
  of '\f': result.add("\\f")
  of '\t': result.add("\\t")
  of '\r': result.add("\\r")
  of '"': result.add("\\\"")
  of '\\': result.add("\\\\")
  else: result.add(c)

proc escapeJson*(s: string; result: var string) =
  ## Converts a string `s` to its JSON representation.
  ## Appends to ``result``.
  result.add("\"")
  for c in s: escape(result, c)
  result.add("\"")

proc escapeJson*(s: string): string =
  ## Converts a string `s` to its JSON representation.
  result = newStringOfCap(s.len + s.len shr 3)
  escapeJson(s, result)

proc indent(s: var string, i: int) =
  for _ in 1..i: s.add ' '

proc newIndent(curr, indent: int, ml: bool): int =
  if ml: return curr + indent
  else: return indent

proc nl(s: var string, ml: bool) =
  s.add(if ml: '\L' else: ' ')

proc emitAtom(result: var string, n: JsonNode) =
  let (start, L) = extractSlice(n.t[], n.a)
  if n.k == JString: result.add("\"")
  for i in 0 ..< L:
    let c = char(n.t[start+i])
    escape(result, c)
  if n.k == JString: result.add("\"")

proc toPretty(result: var string, n: JsonNode, indent = 2, ml = true,
              lstArr = false, currIndent = 0) =
  case n.kind
  of JObject:
    if lstArr: result.indent(currIndent) # Indentation
    if not n.isEmpty:
      result.add("{")
      result.nl(ml) # New line
      var i = 0
      for key, val in pairs(n):
        if i > 0:
          result.add(",")
          result.nl(ml) # New Line
        inc i
        # Need to indent more than {
        result.indent(newIndent(currIndent, indent, ml))
        escapeJson(key, result)
        result.add(": ")
        toPretty(result, val, indent, ml, false,
                 newIndent(currIndent, indent, ml))
      result.nl(ml)
      result.indent(currIndent) # indent the same as {
      result.add("}")
    else:
      result.add("{}")
  of JString, JInt, JFloat:
    if lstArr: result.indent(currIndent)
    emitAtom(result, n)
  of JBool:
    if lstArr: result.indent(currIndent)
    result.add(if n.getBool: "true" else: "false")
  of JArray:
    if lstArr: result.indent(currIndent)
    if not n.isEmpty:
      result.add("[")
      result.nl(ml)
      var i = 0
      for x in items(n):
        if i > 0:
          result.add(",")
          result.nl(ml) # New Line
        toPretty(result, x, indent, ml,
            true, newIndent(currIndent, indent, ml))
        inc i
      result.nl(ml)
      result.indent(currIndent)
      result.add("]")
    else: result.add("[]")
  of JNull:
    if lstArr: result.indent(currIndent)
    result.add("null")

proc pretty*(node: JsonNode, indent = 2): string =
  ## Returns a JSON Representation of `node`, with indentation and
  ## on multiple lines.
  result = ""
  toPretty(result, node, indent)

proc toUgly*(result: var string, node: JsonNode) =
  ## Converts `node` to its JSON Representation, without
  ## regard for human readability. Meant to improve ``$`` string
  ## conversion performance.
  ##
  ## JSON representation is stored in the passed `result`
  ##
  ## This provides higher efficiency than the ``pretty`` procedure as it
  ## does **not** attempt to format the resulting JSON to make it human readable.
  var comma = false
  case node.kind:
  of JArray:
    result.add "["
    for child in node:
      if comma: result.add ","
      else: comma = true
      result.toUgly child
    result.add "]"
  of JObject:
    result.add "{"
    for key, value in pairs(node):
      if comma: result.add ","
      else: comma = true
      key.escapeJson(result)
      result.add ":"
      result.toUgly value
    result.add "}"
  of JString, JInt, JFloat:
    emitAtom(result, node)
  of JBool:
    result.add(if node.getBool: "true" else: "false")
  of JNull:
    result.add "null"

proc `$`*(node: JsonNode): string =
  ## Converts `node` to its JSON Representation on one line.
  result = newStringOfCap(node.len shl 1)
  toUgly(result, node)

proc `[]`*(node: JsonNode, index: int): JsonNode =
  ## Gets the node at `index` in an Array. Result is undefined if `index`
  ## is out of bounds, but as long as array bound checks are enabled it will
  ## result in an exception.
  assert(node.kind == JArray)
  var i = index
  for x in items(node):
    if i == 0: return x
    dec i
  raise newException(IndexError, "index out of bounds")

proc contains*(node: JsonNode, key: string): bool =
  ## Checks if `key` exists in `node`.
  assert(node.kind == JObject)
  let x = rawGet(node, key)
  result = x.a >= 0

proc hasKey*(node: JsonNode, key: string): bool =
  ## Checks if `key` exists in `node`.
  assert(node.kind == JObject)
  result = node.contains(key)

proc `{}`*(node: JsonNode, keys: varargs[string]): JsonNode =
  ## Traverses the node and gets the given value. If any of the
  ## keys do not exist, returns ``JNull``. Also returns ``JNull`` if one of the
  ## intermediate data structures is not an object.
  result = node
  for kk in keys:
    if result.kind != JObject: return newJNull()
    block searchLoop:
      for k, v in pairs(result):
        if k == kk:
          result = v
          break searchLoop
      return newJNull()

proc `{}`*(node: JsonNode, indexes: varargs[int]): JsonNode =
  ## Traverses the node and gets the given value. If any of the
  ## indexes do not exist, returns ``JNull``. Also returns ``JNull`` if one of the
  ## intermediate data structures is not an array.
  result = node
  for j in indexes:
    if result.kind != JArray: return newJNull()
    block searchLoop:
      var i = j
      for x in items(result):
        if i == 0:
          result = x
          break searchLoop
        dec i
      return newJNull()

proc `{}=`*(node: var JsonTree, keys: varargs[string], value: JsonNode) =
  ## Traverses the node and tries to set the value at the given location
  ## to ``value``. If any of the keys are missing, they are added.
  if keys.len == 1:
    node[keys[0]] = value
  elif keys.len != 0:
    var v = value
    for i in countdown(keys.len-1, 1):
      var x = newJObject()
      x[keys[i]] = v
      v = x
    node[keys[0]] = v

proc getOrDefault*(node: JsonNode, key: string): JsonNode =
  ## Gets a field from a `node`. If `node` is nil or not an object or
  ## value at `key` does not exist, returns JNull
  for k, v in pairs(node):
    if k == key: return v
  result = newJNull()

proc `==`*(x, y: JsonNode): bool =
  # Equality for two JsonNodes. Note that the order in field
  # declarations is also part of the equality requirement as
  # everything else would be too costly to implement.
  if x.k != y.k: return false
  if x.k == JNull: return true
  if x.b - x.a != y.b - y.a: return false
  for i in 0 ..< x.b - x.a + 1:
    if x.t[][x.a + i] != y.t[][i + y.a]: return false
  return true

proc parseJson(p: var JsonParser; buf: var seq[byte]) =
  ## Parses JSON from a JSON Parser `p`. We break the abstraction here
  ## and construct the low level representation directly for speed.
  case p.tok
  of tkString:
    storeAtom(buf, JString, p.a)
    discard getTok(p)
  of tkInt:
    storeAtom(buf, JInt, p.a)
    discard getTok(p)
  of tkFloat:
    storeAtom(buf, JFloat, p.a)
    discard getTok(p)
  of tkTrue:
    buf.add opcodeTrue
    discard getTok(p)
  of tkFalse:
    buf.add opcodeFalse
    discard getTok(p)
  of tkNull:
    buf.add opcodeNull
    discard getTok(p)
  of tkCurlyLe:
    beginContainer(buf, JObject)
    discard getTok(p)
    while p.tok != tkCurlyRi:
      if p.tok != tkString:
        raiseParseErr(p, "string literal as key")
      storeAtom(buf, JString, p.a)
      discard getTok(p)
      eat(p, tkColon)
      parseJson(p, buf)
      if p.tok != tkComma: break
      discard getTok(p)
    eat(p, tkCurlyRi)
    endContainer(buf)
  of tkBracketLe:
    beginContainer(buf, JArray)
    discard getTok(p)
    while p.tok != tkBracketRi:
      parseJson(p, buf)
      if p.tok != tkComma: break
      discard getTok(p)
    eat(p, tkBracketRi)
    endContainer(buf)
  of tkError, tkCurlyRi, tkBracketRi, tkColon, tkComma, tkEof:
    raiseParseErr(p, "{")

proc parseJson*(s: Stream, filename: string = ""): JsonTree =
  ## Parses from a stream `s` into a `JsonNode`. `filename` is only needed
  ## for nice error messages.
  ## If `s` contains extra data, it will raise `JsonParsingError`.
  var p: JsonParser
  p.open(s, filename)
  JsonNode(result).t = newStorage()
  JsonNode(result).t[] = newSeqOfCap[byte](64)
  try:
    discard getTok(p) # read first token
    p.parseJson(JsonNode(result).t[])
    JsonNode(result).a = 0
    JsonNode(result).b = high(JsonNode(result).t[])
    JsonNode(result).k = JsonNodeKind(JsonNode(result).t[][0] and opcodeMask)
    eat(p, tkEof) # check if there is no extra data
  finally:
    p.close()

proc parseJson*(buffer: string): JsonTree =
  ## Parses JSON from `buffer`.
  ## If `buffer` contains extra data, it will raise `JsonParsingError`.
  result = parseJson(newStringStream(buffer), "input")

proc parseFile*(filename: string): JsonTree =
  ## Parses `file` into a `JsonNode`.
  ## If `file` contains extra data, it will raise `JsonParsingError`.
  var stream = newFileStream(filename, fmRead)
  if stream == nil:
    raise newException(IOError, "cannot read from file: " & filename)
  result = parseJson(stream, filename)

when isMainModule:
  when false:
    var b: seq[byte] = @[]
    storeAtom(b, JString, readFile("packedjson.nim"))
    let (start, L) = extractSlice(b, 0)
    var result = newString(L)
    for i in 0 ..< L:
      result[i] = char(b[start+i])
    echo result

  template test(a, b) =
    let x = a
    if x != b:
      echo "test failed ", astToStr(a), ":"
      echo x
      echo b

  let testJson = parseJson"""{ "a": [1, 2, 3, 4], "b": "asd", "c": "\ud83c\udf83", "d": "\u00E6"}"""
  test $testJson{"a"}[3], "4"

  var moreStuff = %*{"abc": 3, "more": 6.6, "null": nil}
  test $moreStuff, """{"abc":3,"more":6.600000000000000,"null":null}"""

  moreStuff["more"] = %"foo bar"
  test $moreStuff, """{"abc":3,"more":"foo bar","null":null}"""

  moreStuff["more"] = %"a"
  moreStuff["null"] = %678

  test $moreStuff, """{"abc":3,"more":"a","null":678}"""

  moreStuff.delete "more"
  test $moreStuff, """{"abc":3,"null":678}"""

  moreStuff{"keyOne", "keyTwo", "keyThree"} = %*{"abc": 3, "more": 6.6, "null": nil}

  test $moreStuff, """{"abc":3,"null":678,"keyOne":{"keyTwo":{"keyThree":{"abc":3,"more":6.600000000000000,"null":null}}}}"""

  moreStuff["alias"] = newJObject()
  when false:
    # now let's test aliasing works:
    var aa = moreStuff["alias"]

    aa["a"] = %1
    aa["b"] = %3

  when true:
    delete moreStuff, "keyOne"
    test $moreStuff, """{"abc":3,"null":678,"alias":{}}"""
  moreStuff["keyOne"] = %*{"keyTwo": 3} #, "more": 6.6, "null": nil}
  #echo moreStuff

  moreStuff{"keyOne", "keyTwo", "keyThree"} = %*{"abc": 3, "more": 6.6, "null": nil}
  moreStuff["keyOne", "keyTwo"] = %"ZZZZZ"
  #echo moreStuff

  block:
    var x = newJObject()
    var arr = newJArray()
    arr.add x
    x["field"] = %"value"
    assert $arr == "[{}]"

  block:
    var x = newJObject()
    x["field"] = %"value"
    var arr = newJArray()
    arr.add x
    assert arr == %*[{"field":"value"}]

  when false:
    var arr = newJArray()
    arr.add newJObject()
    var x = arr[0]
    x["field"] = %"value"
    assert $arr == """[{"field":"value"}]"""

  block:
    var testJson = parseJson"""{ "a": [1, 2, {"key": [4, 5]}, 4]}"""
    testJson["a", 2, "key"] = %10
    test $testJson, """{"a":[1,2,{"key":10},4]}"""

  block:
    var mjson = %*{"properties":{"subnet":"a","securitygroup":"b"}}
    mjson["properties","subnet"] = %""
    mjson["properties","securitygroup"] = %""
    test $mjson, """{"properties":{"subnet":"","securitygroup":""}}"""

  block:
    # bug #1
    var msg = %*{
      "itemId":25,
      "cityId":15,
      "less": low(BiggestInt),
      "more": high(BiggestInt),
      "million": 1_000_000
    }
    var itemId = msg["itemId"].getInt
    var cityId = msg["cityId"].getInt
    assert itemId == 25
    assert cityId == 15
    doAssert msg["less"].getBiggestInt == low(BiggestInt)
    doAssert msg["more"].getBiggestInt == high(BiggestInt)
    doAssert msg["million"].getBiggestInt == 1_000_000
  
  echo "Done"
  # Expect dangling..
