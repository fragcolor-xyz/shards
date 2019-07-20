import ../types
import ../chainblocks

import unittest, streams

proc run() =
  var
    junitStream = openFileStream("tcbvar.xml", fmWrite)
    formatter = newJUnitOutputFormatter(junitStream)
  addOutputFormatter(formatter)

  suite "CBVar converters and ops":
    test "Float":
      var
        v:  CBVar = 1.0
        v1: CBVar = 1.0
        v2: CBVar = 2.0
        v3: CBVar = 0.0
      check v.valueType == Float
      check v.floatValue == 1.0
      check v == v1
      check v != v2
      check v >= v1
      check v <= v1
      check v  < v2
      check v  > v3

    test "Float2":
      var
        v:  CBVar = (1.0, 2.0)
        v1: CBVar = (1.0, 2.0)
        v2: CBVar = (2.0, 3.0)
        v3: CBVar = (0.0, 1.0)
        v4: CBVar = (1.0, 3.0)
        v5: CBVar = (0.0, 2.0)
      check v.valueType == Float2
      check v.float2Value[0] == 1.0
      check v.float2Value[1] == 2.0
      check v == v1
      check v != v2
      check v >= v1
      check v <= v1
      check v  < v2
      check v  > v3
      check v4 >= v
      check v5 <= v
      check not (v4  > v)
      check not (v5  < v)
    
    test "Float3":
      var
        v:  CBVar = (1.0, 2.0, 3.0)
        v1: CBVar = (1.0, 2.0, 3.0)
        v2: CBVar = (2.0, 3.0, 4.0)
        v3: CBVar = (0.0, 1.0, 2.0)
        v4: CBVar = (1.0, 2.0, 4.0)
        v5: CBVar = (0.0, 2.0, 3.0)
      check v.valueType == Float3
      check v.float3Value[0] == 1.0
      check v.float3Value[1] == 2.0
      check v.float3Value[2] == 3.0
      check v == v1
      check v != v2
      check v >= v1
      check v <= v1
      check v  < v2
      check v  > v3
      check v4 >= v
      check v5 <= v
      check not (v4  > v)
      check not (v5  < v)

    test "Float4":
      var
        v: CBVar = (1.0, 2.0, 3.0, 4.0)
      check v.valueType == Float4
      check v.float4Value[0] == 1.0
      check v.float4Value[1] == 2.0
      check v.float4Value[2] == 3.0
      check v.float4Value[3] == 4.0
    
    test "Int":
      var v: CBVar = 11
      check v.valueType == Int
      check v.intValue == 11
    
    test "Int2":
      var v: CBVar = (11, 22)
      check v.valueType == Int2
      check v.int2Value[0] == 11
      check v.int2Value[1] == 22
    
    test "Int3":
      var v: CBVar = (11, 22, 33)
      check v.valueType == Int3
      check v.int3Value[0] == 11
      check v.int3Value[1] == 22
      check v.int3Value[2] == 33
    
    test "Int4":
      var
        v: CBVar = (11, 22, 33, 44)
        v1: CBVar = (11, 22, 33, 44)
        v2: CBVar = (11, 22, 35, 44)
        v3: CBVar = (12, 23, 34, 45)
        v4: CBVar = (10, 20, 30, 40)
      check v.valueType == Int4
      check v.int4Value[0] == 11
      check v.int4Value[1] == 22
      check v.int4Value[2] == 33
      check v.int4Value[3] == 44
      check v == v1
      check v != v2
      check v < v3
      check v > v4
    
    test "Int8":
      var v: CBVar = (11, 22, 33, 44, 55, 66, 77, 88)
      check v.valueType == Int8
      for i in 0..<8:
        check v.int8Value[i] == 11 * (i + 1)
    
    test "Int16":
      expect(RangeError):
        var v: CBVar = (11, 22, 33, 44, 55, 66, 77, 88, 99, 110, 121, 132, 143, 154, 165, 176)
        check v.valueType == Int16
        for i in 0..<16:
          check v.int16Value[i] == 11 * (i + 1)
      
      var
        v: CBVar = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)
        v1: CBVar = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)
        v2: CBVar = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17)
        v3: CBVar = (2, 4, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17)
      check v.valueType == Int16
      for i in 0..<16:
        check v.int16Value[i] == (i + 1)
      check v == v1
      check v <= v1
      check v >= v1
      check v != v2
      check v3 >= v
      check v3 > v
    
    test "Color":
      var v: CBVar = (11'u8, 22'u8, 33'u8, 44'u8)
      var v1: CBVar = (11'u8, 22'u8, 33'u8, 44'u8)
      var v2: CBVar = (12'u8, 22'u8, 33'u8, 44'u8)
      var v3: CBVar = (11'u8, 22'u8, 33'u8, 45'u8)
      check v.valueType == Color
      check v.colorValue.r == 11'u8
      check v.colorValue.g == 22'u8
      check v.colorValue.b == 33'u8
      check v.colorValue.a == 44'u8
      check v1 == v
      check v2 != v
      check v3 != v
    
    test "String const":
      var
        v1: CBVar = "Hello world"
        v2: CBVar = "Hello world"
      check v1.valueType == String
      check v1.stringValue == "Hello world"
      check v1 == v2
      check v1.stringValue.pointer == v2.stringValue.pointer
  
  suite "Quick deepcopy and `~deepcopy`":
    test "Blittable":
      var
        src: CBVar = 1.0
        dst: CBVar
      check quickcopy(dst, src) == 0
      check src == dst
      check src.floatValue == 1.0'f64

  formatter.close()

run()