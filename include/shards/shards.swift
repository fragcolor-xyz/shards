/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

// WIP: This is a work in progress

/*
 final class MyShard1 : IShard {
     static var name: StaticString = "MyShard1"
     static var help: StaticString = ""

     var inputTypes: [SHTypeInfo] = [
         VarType.AnyValue.asSHTypeInfo()
     ]
     var outputTypes: [SHTypeInfo] = [
         VarType.AnyValue.asSHTypeInfo()
     ]

     var parameters: [SHParameterInfo] = []
     func setParam(idx: Int, value: SHVar) -> Result<Void, ShardError> {
         .failure(ShardError(message: "Not implemented"))
     }
     func getParam(idx: Int) -> SHVar {
         SHVar()
     }

     var exposedVariables: [SHExposedTypeInfo] = []
     var requiredVariables: [SHExposedTypeInfo] = []

     func compose(data: SHInstanceData) -> Result<SHTypeInfo, ShardError> {
         .success(data.inputType)
     }

     func warmup(context: Context) -> Result<Void, ShardError> {
         .success(())
     }

     func cleanup(context: Context) -> Result<Void, ShardError> {
         .success(())
     }

     func activate(context: Context, input: SHVar) -> Result<SHVar, ShardError> {
         guard input.type == .Int else {
             return .failure(ShardError(message: "Expected Int input"))
         }
         let result = input.int * 2
         return .success(SHVar(value: result))
     }

     // -- DON'T EDIT THE FOLLOWING --
     typealias ShardType = MyShard1
     static var inputTypesCFunc: SHInputTypesProc {{ bridgeInputTypes(ShardType.self, shard: $0) }}
     static var outputTypesCFunc: SHInputTypesProc {{ bridgeOutputTypes(ShardType.self, shard: $0) }}
     static var destroyCFunc: SHDestroyProc {{ bridgeDestroy(ShardType.self, shard: $0) }}
     static var nameCFunc: SHNameProc {{ _ in bridgeName(ShardType.self) }}
     static var hashCFunc: SHHashProc {{ _ in bridgeHash(ShardType.self) }}
     static var helpCFunc: SHHelpProc {{ _ in bridgeHelp(ShardType.self) }}
     static var parametersCFunc: SHParametersProc {{ bridgeParameters(ShardType.self, shard: $0) }}
     static var setParamCFunc: SHSetParamProc {{ bridgeSetParam(ShardType.self, shard: $0, idx: $1, input: $2)}}
     static var getParamCFunc: SHGetParamProc {{ bridgeGetParam(ShardType.self, shard: $0, idx: $1)}}
     static var exposedVariablesCFunc: SHExposedVariablesProc {{ bridgeExposedVariables(ShardType.self, shard: $0) }}
     static var requiredVariablesCFunc: SHRequiredVariablesProc {{ bridgeRequiredVariables(ShardType.self, shard: $0) }}
     static var composeCFunc: SHComposeProc {{ bridgeCompose(ShardType.self, shard: $0, data: $1) }}
     static var warmupCFunc: SHWarmupProc {{ bridgeWarmup(ShardType.self, shard: $0, ctx: $1) }}
     static var cleanupCFunc: SHCleanupProc {{ bridgeCleanup(ShardType.self, shard: $0, ctx: $1) }}
     static var activateCFunc: SHActivateProc {{ bridgeActivate(ShardType.self, shard: $0, ctx: $1, input: $2) }}
     var errorCache: ContiguousArray<CChar> = []
     var output: SHVar = SHVar()
 }

 and register with:
 RegisterShard(MyShard1.name.utf8Start.withMemoryRebound(to: Int8.self, capacity: 1) { $0 }, { createSwiftShard(MyShard1.self) })
 */

import Foundation
import shards

public struct Globals {
    public var Core: UnsafeMutablePointer<SHCore>

    init() {
        // Get current directory and check if writable
        let currentPath = FileManager.default.currentDirectoryPath
        let isWritable = FileManager.default.isWritableFile(atPath: currentPath)

        // If not writable, try to set to documents directory
        if !isWritable {
            if let documentsPath = FileManager.default.urls(
                for: .documentDirectory, in: .userDomainMask
            ).first?.path {
                FileManager.default.changeCurrentDirectoryPath(documentsPath)
            }
        }

        // Finally init Shards
        Core = shardsInterface(UInt32(SHARDS_CURRENT_ABI))
        _ = Core.pointee.`init`()
    }
}

public var G = Globals()

public var RegisterShard: SHRegisterShard = G.Core.pointee.registerShard

public enum VarType: UInt8, CustomStringConvertible, CaseIterable {
    // Blittables
    case NoValue
    case AnyValue
    case Enum
    case Bool
    case Int // A 64bits int
    case Int2 // A vector of 2 64bits ints
    case Int3 // A vector of 3 32bits ints
    case Int4 // A vector of 4 32bits ints
    case Int8 // A vector of 8 16bits ints
    case Int16 // A vector of 16 8bits ints
    case Float // A 64bits float
    case Float2 // A vector of 2 64bits floats
    case Float3 // A vector of 3 32bits floats
    case Float4 // A vector of 4 32bits floats
    case Color // A vector of 4 uint8

    // Internal use only
    case EndOfBlittableTypes = 50 // anything below this is not blittable (ish)

    // Non Blittables
    case Bytes // pointer + size
    case String
    case Path // An OS filesystem path
    case ContextVar // A string label to find from SHContext variables
    case Image
    case Seq
    case Table
    case Wire
    case ShardRef // a shard, useful for future introspection shards!
    case Object = 60
    // Array, // Notice: of just blittable types - Reserved for future use - 61
    // Set, // Reserved for future use - 62
    case Audio = 63
    case TypeInfo // Describes a type
    case Trait // A wire trait

    public var description: String {
        switch self {
        case .NoValue:
            return "None"
        case .AnyValue:
            return "Any"
        case .Enum:
            return "Enum"
        case .Bool:
            return "Bool"
        case .Int:
            return "Int"
        case .Int2:
            return "Int2"
        case .Int3:
            return "Int3"
        case .Int4:
            return "Int4"
        case .Int8:
            return "Int8"
        case .Int16:
            return "Int16"
        case .Float:
            return "Float"
        case .Float2:
            return "Float2"
        case .Float3:
            return "Float3"
        case .Float4:
            return "Float4"
        case .Color:
            return "Color"
        case .Bytes:
            return "Bytes"
        case .String:
            return "String"
        case .Path:
            return "Path"
        case .ContextVar:
            return "ContextVar"
        case .Image:
            return "Image"
        case .Seq:
            return "Seq"
        case .Table:
            return "Table"
        case .Wire:
            return "Wire"
        case .Object:
            return "Object"
        case .ShardRef:
            return "ShardRef"
        case .Audio:
            return "Audio"
        case .TypeInfo:
            return "Type"
        case .Trait:
            return "Trait"
        case .EndOfBlittableTypes:
            return "EndOfBlittableTypes"
        }
    }

    func uxInlineable() -> Bool {
        return self == VarType.NoValue
            || self == VarType.AnyValue
            || self == VarType.Int
            || self == VarType.Int2
            || self == VarType.Int3
            || self == VarType.Int4
            || self == VarType.Int8
            || self == VarType.Int16
            || self == VarType.Float
            || self == VarType.Float2
            || self == VarType.Float3
            || self == VarType.Float4
    }

    func asSHType() -> SHType {
        SHType(rawValue: rawValue)
    }

    func asSHTypeInfo() -> SHTypeInfo {
        var info = SHTypeInfo()
        info.basicType = asSHType()
        return info
    }
}

extension SHVar: CustomStringConvertible, Hashable, Equatable {
    public var description: String {
        switch type {
        case .NoValue:
            return "None"
        case .AnyValue:
            return "Any"
        case .Bool:
            return "Bool(\(Bool(payload.boolValue)))"
        case .Int:
            return "Int(\(Int(payload.intValue)))"
        case .Int2:
            let v = payload.int2Value
            return "Int2(\(v.x), \(v.y))"
        case .Int3:
            let v = payload.int3Value
            return "Int3(\(v.x), \(v.y), \(v.z))"
        case .Int4:
            let v = payload.int4Value
            return "Int4(\(v.x), \(v.y), \(v.z), \(v.w))"
        case .Int8:
            let v = payload.int8Value
            return "Int8(\(v[0]), \(v[1]), \(v[2]), \(v[3]), \(v[4]), \(v[5]), \(v[6]), \(v[7]))"
        case .Int16:
            let v = payload.int16Value
            return "Int16(\(v[0]), \(v[1]), \(v[2]), \(v[3]), \(v[4]), \(v[5]), \(v[6]), \(v[7]), \(v[8]), \(v[9]), \(v[10]), \(v[11]), \(v[12]), \(v[13]), \(v[14]), \(v[15]))"
        case .Float:
            return "Float(\(Double(payload.floatValue)))"
        case .Float2:
            let v = payload.float2Value
            return "Float2(\(v.x), \(v.y))"
        case .Float3:
            let v = payload.float3Value
            return "Float3(\(v.x), \(v.y), \(v.z))"
        case .Float4:
            let v = payload.float4Value
            return "Float4(\(v.x), \(v.y), \(v.z), \(v.w))"
        case .Color:
            let c = payload.colorValue
            return "Color(r:\(c.r), g:\(c.g), b:\(c.b), a:\(c.a))"
        case .Bytes:
            return "Bytes(\(payload.bytesSize) bytes)"
        case .String:
            return "String(\"\(string)\")"
        case .Path:
            return "Path(\"\(string)\")"
        case .ContextVar:
            return "ContextVar(\"\(string)\")"
        case .Image:
            if let img = payload.imageValue {
                return "Image(\(img.pointee.width)x\(img.pointee.height), channels:\(img.pointee.channels), flags:\(img.pointee.flags))"
            }
            return "Image(nil)"
        case .Seq:
            return "Seq(\(payload.seqValue.len) items)"
        case .Table:
            return "Table"
        case .Wire:
            return "Wire"
        case .ShardRef:
            if let shard = payload.shardValue {
                if let nameFunc = shard.pointee.name {
                    if let name = nameFunc(shard) {
                        return "ShardRef(\(String(cString: name)))"
                    }
                }
            }
            return "ShardRef"
        case .Object:
            return "Object(vendor:\(payload.objectVendorId), type:\(payload.objectTypeId))"
        case .Enum:
            return "Enum(vendor:\(payload.enumVendorId), type:\(payload.enumTypeId), value:\(payload.enumValue))"
        case .Audio:
            return "Audio(\(payload.audioValue.nsamples) samples, channels:\(payload.audioValue.channels), rate:\(payload.audioValue.sampleRate))"
        case .TypeInfo:
            return "TypeInfo"
        case .Trait:
            return "Trait"
        case .EndOfBlittableTypes:
            return "EndOfBlittableTypes"
        }
    }

    public var typename: String {
        type.description
    }

    public var type: VarType {
        VarType(rawValue: valueType.rawValue)!
    }

    public mutating func Clone(dst: inout SHVar) {
        G.Core.pointee.cloneVar(&dst, &self)
    }

    public mutating func Clone() -> SHVar {
        var v = SHVar()
        G.Core.pointee.cloneVar(&v, &self)
        return v
    }

    public mutating func Destroy() {
        G.Core.pointee.destroyVar(&self)
    }

    init(value: Bool) {
        var v = SHVar()
        v.valueType = Bool
        v.payload.boolValue = SHBool(value)
        self = v
    }

    public var bool: Bool {
        get {
            assert(type == .Bool, "Bool variable expected!")
            return Bool(payload.boolValue)
        }
        set {
            assert(type == .Bool, "Bool variable expected!")
            payload.boolValue = SHBool(newValue)
        }
    }

    public var maybeBool: Bool? {
        if type != .Bool {
            return nil
        }
        return Bool(payload.boolValue)
    }

    public init(value: Int) {
        var v = SHVar()
        v.valueType = Int
        v.payload.intValue = SHInt(value)
        self = v
    }

    public var int: Int {
        get {
            assert(type == .Int, "Int variable expected!")
            return Int(payload.intValue)
        }
        set {
            assert(type == .Int, "Int variable expected!")
            payload.intValue = SHInt(newValue)
        }
    }

    public var maybeInt: Int? {
        if type != .Int {
            return nil
        }
        return Int(payload.intValue)
    }

    public var wire: SHWireRef {
        get {
            assert(type == .Wire, "Wire variable expected!")
            return payload.wireValue
        }
        set {
            assert(type == .Wire, "Wire variable expected!")
            payload.wireValue = newValue
        }
    }

    init(x: Int64, y: Int64) {
        var v = SHVar()
        v.valueType = Int2
        v.payload.int2Value = SHInt2(x: x, y: y)
        self = v
    }

    init(r: UInt8, g: UInt8, b: UInt8, a: UInt8) {
        var v = SHVar()
        v.valueType = Color
        v.payload.colorValue.r = r
        v.payload.colorValue.g = g
        v.payload.colorValue.b = b
        v.payload.colorValue.a = a
        self = v
    }

    init(value: SIMD2<Int64>) {
        var v = SHVar()
        v.valueType = Int2
        v.payload.int2Value = value
        self = v
    }

    init(value: SIMD3<Float>) {
        var v = SHVar()
        v.valueType = Float3
        v.payload.float3Value = SIMD4<Float>(value, 0)
        self = v
    }

    init(value: SIMD4<Float>) {
        var v = SHVar()
        v.valueType = Float4
        v.payload.float4Value = value
        self = v
    }

    public init(value: Float) {
        var v = SHVar()
        v.valueType = Float
        v.payload.floatValue = SHFloat(value)
        self = v
    }

    public init(value: Double) {
        var v = SHVar()
        v.valueType = Float
        v.payload.floatValue = SHFloat(value)
        self = v
    }

    init(value: SIMD2<Double>) {
        var v = SHVar()
        v.valueType = Float2
        v.payload.float2Value = value
        self = v
    }

    init(value: SIMD16<Int8>) {
        var v = SHVar()
        v.valueType = Int16
        v.payload.int16Value = value
        self = v
    }

    public static func object(vendorId: Int32, typeId: Int32, value: UnsafeMutableRawPointer?) -> SHVar {
        var v = SHVar()
        v.valueType = Object
        v.payload.objectVendorId = vendorId
        v.payload.objectTypeId = typeId
        v.payload.objectValue = value
        return v
    }

    public static func object(vendorId: Int32, typeId: Int32, value: UnsafeMutableRawPointer?, objectInfo: inout SHObjectInfo) -> SHVar {
        var v = SHVar()
        v.valueType = Object
        v.payload.objectVendorId = vendorId
        v.payload.objectTypeId = typeId
        v.payload.objectValue = value
        v.flags |= UInt16(SHVAR_FLAGS_USES_OBJINFO)
        v.objectInfo = withUnsafeMutablePointer(to: &objectInfo) { $0 }
        return v
    }

    public static func enumVar(vendorId: Int32, typeId: Int32, value: Int32) -> SHVar {
        var v = SHVar()
        v.valueType = Enum
        v.payload.enumVendorId = vendorId
        v.payload.enumTypeId = typeId
        v.payload.enumValue = value
        return v
    }

    public var enumValue: Int32 {
        get {
            assert(type == .Enum, "Enum variable expected!")
            return payload.enumValue
        }
        set {
            assert(type == .Enum, "Enum variable expected!")
            payload.enumValue = newValue
        }
    }

    public var float: Float {
        get {
            assert(type == .Float, "Float variable expected!")
            return Float(payload.floatValue)
        }
        set {
            assert(type == .Float, "Float variable expected!")
            payload.floatValue = SHFloat(newValue)
        }
    }

    public var double2: SIMD2<Double> {
        get {
            assert(type == .Float2, "Float2 variable expected!")
            return payload.float2Value
        }
        set {
            assert(type == .Float2, "Float2 variable expected!")
            payload.float2Value = newValue
        }
    }

    public var float4: SIMD4<Float> {
        get {
            assert(type == .Float4, "Float4 variable expected!")
            return payload.float4Value
        }
        set {
            assert(type == .Float4, "Float4 variable expected!")
            payload.float4Value = newValue
        }
    }

    public var double: Double {
        get {
            assert(type == .Float, "Double variable expected!")
            return Double(payload.floatValue)
        }
        set {
            assert(type == .Float, "Double variable expected!")
            payload.floatValue = SHFloat(newValue)
        }
    }

    public var int16: SIMD16<Int8> {
        get {
            assert(type == .Int16, "Int16 variable expected!")
            return payload.int16Value
        }
        set {
            assert(type == .Int16, "Float4 variable expected!")
            payload.int16Value = newValue
        }
    }

    init(string: StaticString) {
        var v = SHVar()
        v.valueType = String
        v.payload.stringValue = string.withUTF8Buffer { buffer in
            unsafeBitCast(buffer.baseAddress, to: UnsafePointer<CChar>.self)
        }
        v.payload.stringLen = UInt32(string.utf8CodeUnitCount)
        v.payload.stringCapacity = 0
        self = v
    }

    public var string: String {
        assert(type == .String || type == .ContextVar, "String variable expected!")
        guard let stringPtr = payload.stringValue else {
            return ""
        }
        let length = Int(payload.stringLen)
        guard length > 0 else {
            return ""
        }
        // Cast `CChar` (Int8) to `UInt8` for decoding
        let buffer = UnsafeBufferPointer(start: stringPtr, count: length).map {
            UInt8(bitPattern: $0)
        }
        return String(decoding: buffer, as: UTF8.self)
    }

    public var maybeString: String? {
        if type != .String && type != .ContextVar {
            return nil
        }
        return string
    }

    public var maybeFloat: Float? {
        if type != .Float {
            return nil
        }
        return float
    }

    public var maybeDouble: Double? {
        if type != .Float {
            return nil
        }
        return double
    }

    public var maybeDouble2: SIMD2<Double>? {
        if type != .Float2 {
            return nil
        }
        return double2
    }

    public var maybeFloat4: SIMD4<Float>? {
        if type != .Float4 {
            return nil
        }
        return float4
    }

    public var maybe: SHVar? {
        if type != .NoValue {
            return self
        } else {
            return nil
        }
    }

    public var maybeImage: UnsafeMutablePointer<SHImage>? {
        if type != .Image {
            return nil
        }
        return payload.imageValue
    }

    public var bytes: ContiguousArray<UInt8> {
        assert(type == .Bytes, "Bytes variable expected!")
        guard let bytesPtr = payload.bytesValue else {
            return ContiguousArray()
        }
        let length = Int(payload.bytesSize)
        guard length > 0 else {
            return ContiguousArray()
        }
        let buffer = UnsafeBufferPointer(start: bytesPtr, count: length)
        return ContiguousArray(buffer)
    }

    public var maybeBytes: ContiguousArray<UInt8>? {
        if type != .Bytes {
            return nil
        }
        return bytes
    }

    public var int2: SIMD2<Int64> {
        get {
            assert(type == .Int2, "Int2 variable expected!")
            return SIMD2<Int64>(payload.int2Value.x, payload.int2Value.y)
        }
        set {
            assert(type == .Int2, "Int2 variable expected!")
            payload.int2Value = newValue
        }
    }

    init(value: ShardPtr) {
        var v = SHVar()
        v.valueType = SHType(rawValue: VarType.ShardRef.rawValue)
        v.payload.shardValue = value
        self = v
    }

    init(value: SHWireRef) {
        var v = SHVar()
        v.valueType = SHType(rawValue: VarType.Wire.rawValue)
        v.payload.wireValue = value
        self = v
    }

    public var shard: ShardPtr {
        get {
            assert(type == .ShardRef, "Shard variable expected!")
            return payload.shardValue
        }
        set {
            self = .init(value: newValue)
        }
    }

    init(value: UnsafeMutableBufferPointer<SHVar>) {
        var v = SHVar()
        v.valueType = Seq
        v.payload.seqValue.elements = value.baseAddress
        v.payload.seqValue.len = UInt32(value.count)
        v.payload.seqValue.cap = 0
        self = v
    }

    public var seq: UnsafeMutableBufferPointer<SHVar> {
        get {
            assert(type == .Seq, "Seq variable expected!")
            return .init(start: payload.seqValue.elements, count: Int(payload.seqValue.len))
        }
        set {
            self = .init(value: newValue)
        }
    }

    init(pointer: UnsafeMutableRawPointer, vendorId: Int32, typeId: Int32) {
        var v = SHVar()
        v.valueType = Object
        v.payload.objectVendorId = vendorId
        v.payload.objectTypeId = typeId
        v.payload.objectValue = pointer
        self = v
    }

    func isNone() -> Bool {
        return type == .NoValue
    }

    mutating func addRef() {
        refcount += 1
        flags |= UInt16(SHVAR_FLAGS_REF_COUNTED)
    }

    mutating func releaseRef() {
        assert(refcount > 0, "Refcount must be positive!")
        refcount -= 1
        if refcount == 0 {
            flags &= ~UInt16(SHVAR_FLAGS_REF_COUNTED)
            withUnsafeMutablePointer(to: &self) { ptr in
                G.Core.pointee.destroyVar(ptr)
            }
        }
    }

    public func hash(into hasher: inout Hasher) {
        let hash = withUnsafePointer(to: self) { ptr in
            G.Core.pointee.hashVar(ptr)
        }
        hasher.combine(hash.int2)
    }

    public static func == (lhs: SHVar, rhs: SHVar) -> Bool {
        withUnsafePointer(to: lhs) { lhsPtr in
            withUnsafePointer(to: rhs) { rhsPtr in
                G.Core.pointee.isEqualVar(lhsPtr, rhsPtr)
            }
        }
    }
}

class OwnedVar: Hashable, Equatable {
    var v: SHVar
    var borrowed = false

    init() {
        v = SHVar()
    }

    init(cloning: SHVar) {
        v = SHVar()

        withUnsafePointer(to: cloning) { ptr in
            G.Core.pointee.cloneVar(&v, UnsafeMutablePointer(mutating: ptr))
        }
    }

    init(borrowing: SHVar) {
        v = borrowing
        borrowed = true
    }

    init(string: String) {
        v = SHVar()
        set(string: string)
    }

    init(variable: String) {
        v = SHVar()
        set(variable: variable)
    }

    init(bytes: ContiguousArray<UInt8>) {
        v = SHVar()
        set(bytes: bytes)
    }

    init(bool: Bool) {
        v = SHVar()
        set(bool: bool)
    }

    init(float: Double) {
        v = SHVar()
        set(float: float)
    }

    init(int: Int) {
        v = SHVar()
        set(int: int)
    }

    deinit {
        if borrowed { return }

        withUnsafeMutablePointer(to: &v) { ptr in
            G.Core.pointee.destroyVar(ptr)
        }
    }

    func ptr() -> UnsafeMutablePointer<SHVar> {
        return withUnsafeMutablePointer(to: &v) { ptr in
            UnsafeMutablePointer(mutating: ptr)
        }
    }

    func set(string: String) {
        string.withCString { cString in
            var tmp = SHVar()
            tmp.valueType = VarType.String.asSHType()
            tmp.payload.stringValue = cString
            let length = string.lengthOfBytes(using: .utf8)
            tmp.payload.stringLen = UInt32(length)
            G.Core.pointee.cloneVar(&v, &tmp)
        }
    }

    func set(variable: String) {
        variable.withCString { cString in
            var tmp = SHVar()
            tmp.valueType = VarType.ContextVar.asSHType()
            tmp.payload.stringValue = cString
            let length = variable.lengthOfBytes(using: .utf8)
            tmp.payload.stringLen = UInt32(length)
            G.Core.pointee.cloneVar(&v, &tmp)
        }
    }

    func set(bool: Bool) {
        v.valueType = VarType.Bool.asSHType()
        v.payload.boolValue = bool
    }

    func set(float: Double) {
        v.valueType = VarType.Float.asSHType()
        v.payload.floatValue = float
    }

    func set(int: Int) {
        v.valueType = VarType.Int.asSHType()
        v.payload.intValue = Int64(int)
    }

    func set(int: Int64) {
        v.valueType = VarType.Int.asSHType()
        v.payload.intValue = int
    }

    func set(bytes: ContiguousArray<UInt8>) {
        bytes.withUnsafeBufferPointer { buffer in
            let length = buffer.count
            var tmp = SHVar()
            tmp.valueType = VarType.Bytes.asSHType()
            tmp.payload.bytesValue = UnsafeMutablePointer(mutating: buffer.baseAddress)
            tmp.payload.bytesSize = UInt32(length)
            G.Core.pointee.cloneVar(&v, &tmp)
        }
    }

    func assign(other: SHVar) {
        withUnsafePointer(to: other) { ptr in
            G.Core.pointee.cloneVar(&v, UnsafeMutablePointer(mutating: ptr))
        }
    }

    func hash(into hasher: inout Hasher) {
        let hash = withUnsafePointer(to: &v) { ptr in
            G.Core.pointee.hashVar(ptr)
        }
        hasher.combine(hash.int2)
    }

    static func == (lhs: OwnedVar, rhs: OwnedVar) -> Bool {
        withUnsafePointer(to: lhs.v) { lhsPtr in
            withUnsafePointer(to: rhs.v) { rhsPtr in
                G.Core.pointee.isEqualVar(lhsPtr, rhsPtr)
            }
        }
    }
}

class TableVar: OwnedVar, Sequence {
    override init() {
        super.init()
        v.valueType = VarType.Table.asSHType()
        v.payload.tableValue = G.Core.pointee.tableNew()
    }

    override init(cloning: SHVar) {
        super.init(cloning: cloning)
        assert(cloning.valueType == VarType.Table.asSHType())
    }

    override init(borrowing: SHVar) {
        super.init(borrowing: borrowing)
        borrowed = true
        assert(borrowing.valueType == VarType.Table.asSHType())
    }

    func insertOrUpdate(key: SHVar, cloning: SHVar) {
        let vPtr = v.payload.tableValue.api.pointee.tableAt(v.payload.tableValue, key)
        withUnsafePointer(to: cloning) { ptr in
            G.Core.pointee.cloneVar(vPtr, UnsafeMutablePointer(mutating: ptr))
        }
    }

    func insertOrUpdate(string: StaticString, cloning: SHVar) {
        insertOrUpdate(key: SHVar(string: string), cloning: cloning)
    }

    func get(key: SHVar) -> SHVar {
        let vPtr = v.payload.tableValue.api.pointee.tableAt(v.payload.tableValue, key)
        return vPtr!.pointee
    }

    func get(key: StaticString) -> SHVar {
        return get(key: SHVar(string: key))
    }

    func maybeGet(key: StaticString) -> SHVar? {
        let result = get(key: SHVar(string: key))
        if result.valueType != VarType.NoValue.asSHType() {
            return result
        } else {
            return nil
        }
    }
    
    func maybeGet(key: SHVar) -> SHVar? {
        let result = get(key: key)
        if result.valueType != VarType.NoValue.asSHType() {
            return result
        } else {
            return nil
        }
    }

    func clear() {
        v.payload.tableValue.api.pointee.tableClear(v.payload.tableValue)
        // also increase version
        v.version += 1
    }

    func contains(key: SHVar) -> Bool {
        return v.payload.tableValue.api.pointee.tableContains(v.payload.tableValue, key)
    }

    func contains(string: StaticString) -> Bool {
        return contains(key: SHVar(string: string))
    }

    struct Iterator: IteratorProtocol {
        let table: SHTable
        // could not find a better solution... anyway why not...
        var iterator:
            (
                CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar,
                CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar,
                CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar,
                CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar,
                CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar, CChar,
                CChar, CChar, CChar, CChar
            ) = (
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0
            )

        init(table: SHTable) {
            self.table = table
            table.api.pointee.tableGetIterator(table, &iterator)
        }

        mutating func next() -> (key: SHVar, value: SHVar)? {
            var k = SHVar()
            var v = SHVar()

            if table.api.pointee.tableNext(table, &iterator, &k, &v) {
                return (k, v)
            }
            return nil
        }
    }

    func makeIterator() -> Iterator {
        Iterator(table: v.payload.tableValue)
    }

    // Convenience method similar to the C++ ForEach
    func forEach(_ body: (SHVar, SHVar) throws -> Void) rethrows {
        for (key, value) in self {
            try body(key, value)
        }
    }

    // override assign to assign other table
    override func assign(other: SHVar) {
        assert(other.valueType == VarType.Table.asSHType())
        super.assign(other: other)
    }
}

class SeqVar: OwnedVar {
    override init() {
        super.init()
        v.valueType = VarType.Seq.asSHType()
    }

    override init(cloning: SHVar) {
        super.init(cloning: cloning)
        assert(cloning.valueType == VarType.Seq.asSHType())
    }

    override init(borrowing: SHVar) {
        super.init(borrowing: borrowing)
        borrowed = true
        assert(borrowing.valueType == VarType.Seq.asSHType())
    }

    func resize(size: Int) {
        withUnsafeMutablePointer(to: &v.payload.seqValue) { ptr in
            G.Core.pointee.seqResize(ptr, UInt32(size))
        }
    }

    func clear() {
        resize(size: 0)
    }

    func size() -> Int {
        return Int(v.payload.seqValue.len)
    }

    func pushRaw(value: SHVar) {
        let index = size()
        resize(size: index + 1)
        v.payload.seqValue.elements[index] = value
    }

    func pushCloning(value: OwnedVar) {
        let index = size()
        resize(size: index + 1)
        withUnsafePointer(to: value.v) { ptr in
            G.Core.pointee.cloneVar(
                &v.payload.seqValue.elements[index], UnsafeMutablePointer(mutating: ptr)
            )
        }
    }

    // Private cos it is actually unsafe-ish, prone to life time errors in release mode
    private func pushCloning(value: SHVar) {
        let index = size()
        resize(size: index + 1)
        withUnsafePointer(to: value) { ptr in
            G.Core.pointee.cloneVar(
                &v.payload.seqValue.elements[index], UnsafeMutablePointer(mutating: ptr)
            )
        }
    }

    func push(string: String) {
        string.utf8CString.withUnsafeBufferPointer { buffer in
            var tmp = SHVar()
            tmp.valueType = VarType.String.asSHType()
            tmp.payload.stringValue = buffer.baseAddress
            tmp.payload.stringLen = UInt32(buffer.count - 1) // Subtract 1 to exclude null terminator
            pushCloning(value: tmp)
        }
    }

    func push(bytes: ContiguousArray<UInt8>) {
        bytes.withUnsafeBufferPointer { buffer in
            var tmp = SHVar()
            tmp.valueType = VarType.Bytes.asSHType()
            tmp.payload.bytesValue = UnsafeMutablePointer(mutating: buffer.baseAddress)
            tmp.payload.bytesSize = UInt32(bytes.count)
            pushCloning(value: tmp)
        }
    }

    // Notice that the memory of the result is still owned by SeqVar as when we destroy we destroy capacity!
    // So a further push will reuse same memory!
    @discardableResult func popRaw() -> SHVar {
        assert(size() > 0)
        let index = size() - 1
        resize(size: index)
        return v.payload.seqValue.elements[index]
    }

    func at(index: Int) -> SHVar {
        assert(index >= 0 && index < size())
        return v.payload.seqValue.elements[index]
    }

    func set(index: Int, value: SHVar) {
        assert(index >= 0 && index < size())
        withUnsafePointer(to: value) { ptr in
            G.Core.pointee.cloneVar(
                &v.payload.seqValue.elements[index], UnsafeMutablePointer(mutating: ptr)
            )
        }
    }

    func remove(index: Int) {
        assert(index >= 0 && index < size())
        withUnsafeMutablePointer(to: &v.payload.seqValue) { ptr in
            G.Core.pointee.seqSlowDelete(ptr, UInt32(index))
        }
    }

    func removeFast(index: Int) {
        assert(index >= 0 && index < size())
        withUnsafeMutablePointer(to: &v.payload.seqValue) { ptr in
            G.Core.pointee.seqFastDelete(ptr, UInt32(index))
        }
    }
}

extension SeqVar: Sequence {
    struct Iterator: IteratorProtocol {
        let seq: SeqVar
        var currentIndex: Int = 0
        mutating func next() -> SHVar? {
            if currentIndex < seq.size() {
                let element = seq.at(index: currentIndex)
                currentIndex += 1
                return element
            }
            return nil
        }
    }

    func makeIterator() -> Iterator {
        return Iterator(seq: self, currentIndex: 0)
    }
}

class ParamVar {
    private var parameter: OwnedVar
    private var pointee: UnsafeMutablePointer<SHVar>?
    private var requiredTypes = ExposedTypes()

    init(parameter: OwnedVar) {
        self.parameter = parameter
    }

    func compose(help: String, requiredType: TypeInfo) {
        if isVariable() {
            let reqInfo = ExposedTypeInfo(name: getName()!, help: help, exposedType: requiredType)
            requiredTypes = .init(types: [reqInfo])
        } else {
            requiredTypes = .init(types: [])
        }
    }

    func cleanup() {
        if parameter.v.valueType == VarType.ContextVar.asSHType() {
            G.Core.pointee.releaseVariable(pointee)
        }
        pointee = nil
    }

    func warmup(context: Context) {
        if parameter.v.valueType == VarType.ContextVar.asSHType() {
            assert(pointee == nil)
            var swl = SHStringWithLen()
            swl.string = parameter.v.payload.stringValue
            swl.len = UInt64(parameter.v.payload.stringLen)
            pointee = G.Core.pointee.referenceVariable(context.context, swl)
        } else {
            withUnsafeMutablePointer(to: &parameter.v) {
                pointee = UnsafeMutablePointer($0)
            }
        }
        assert(pointee != nil)
    }

    func setFastUnsafe(value: inout SHVar) {
        assert(pointee != nil)
        // store flags and rc
        let rc = pointee!.pointee.refcount
        let flags = pointee!.pointee.flags
        // assign
        pointee!.pointee = value
        // restore flags and rc
        pointee!.pointee.flags = flags
        pointee!.pointee.refcount = rc
    }

    func setCloning(value: inout SHVar) {
        assert(pointee != nil)
        withUnsafeMutablePointer(to: &value) { ptr in
            G.Core.pointee.cloneVar(pointee, ptr)
        }
    }

    func get() -> SHVar {
        assert(pointee != nil)
        return pointee!.pointee
    }

    func maybeGet() -> SHVar? {
        return pointee?.pointee
    }

    func assignParam(value: SHVar) {
        parameter = .init(cloning: value)
    }

    func getParam() -> SHVar {
        parameter.v
    }

    func isVariable() -> Bool {
        parameter.v.valueType == VarType.ContextVar.asSHType()
    }

    func isNone() -> Bool {
        parameter.v.valueType == VarType.NoValue.asSHType()
    }

    func setName(name: String) {
        parameter = .init(string: name)
    }

    func getName() -> String? {
        if isVariable() {
            return parameter.v.string
        }
        return nil
    }

    func getRequiredTypes() -> ExposedTypes {
        return requiredTypes
    }
}

class ShardsVar {
    private var shardsPtrs: ContiguousArray<ShardPtr> = []
    private var nativeShards = shards.Shards()
    private var composeResult = SHComposeResult()
    private var paramValue = OwnedVar()
    private var requiredVariables = ExposedTypes()
    private var exposedVariables = ExposedTypes()

    init() {
        // Initialize with nil terminator to guarantee nativeShards.elements is ALWAYS valid
        // Even if setParam is never called, activate() can safely be called (will do nothing)
        shardsPtrs.append(nil)
        // Get persistent pointer to array's buffer (valid until array is reallocated)
        nativeShards.elements = shardsPtrs.withUnsafeMutableBufferPointer { $0.baseAddress }
        nativeShards.len = 0
        nativeShards.cap = 0
    }

    private func reset() {
        // Cleanup all shards (skip NULL terminator if present)
        let shardsToCleanup = shardsPtrs.last != nil && shardsPtrs.last! == nil
            ? shardsPtrs.dropLast()
            : shardsPtrs[...]

        // Cleanup in REVERSE order (like C++ implementation)
        // NOTE: Call cleanup, NOT destroy! The shards are freed when paramValue is destroyed
        // (because they have SHARD_FLAGS_OWNED_SHARD set). Calling destroy here causes double-free!
        for shard in shardsToCleanup.reversed() {
            let error = shard!.pointee.cleanup(shard!, nil)
            if error.code != 0 {
                // Log error but continue cleanup
                if let msg = error.message.toString() {
                    print("Error during shard cleanup in reset: \(msg)")
                }
            }
        }
        shardsPtrs.removeAll()

        nativeShards.len = 0
        nativeShards.elements = nil

        withUnsafeMutablePointer(to: &composeResult) { ptr in
            G.Core.pointee.freeComposeResult(ptr)
        }

        composeResult = SHComposeResult()
        
        paramValue = .init()

        requiredVariables = ExposedTypes()
        exposedVariables = ExposedTypes()
    }

    func cleanup(context: Context) -> Result<Void, ShardError> {
        var error = SHError()
        // Skip the last element (NULL terminator) if present
        let shardsToCleanup = shardsPtrs.last != nil && shardsPtrs.last! == nil
            ? shardsPtrs.dropLast()
            : shardsPtrs[...]

        // Cleanup in REVERSE order (like C++ and Rust implementations)
        for shard in shardsToCleanup.reversed() {
            error = shard!.pointee.cleanup(shard!, context.context)
            if error.code != 0 {
                return .failure(ShardError(message: error.message.toString()!))
            }
        }
        return .success(())
    }

    func warmup(context: Context) -> Result<Void, ShardError> {
        var error = SHError()
        // Skip the last element (NULL terminator) if present
        let shardsToWarmup = shardsPtrs.last != nil && shardsPtrs.last! == nil
            ? shardsPtrs.dropLast()
            : shardsPtrs[...]

        for shard in shardsToWarmup {
            // warmup is optional - check before calling (like C++ implementation)
            if let warmupFn = shard!.pointee.warmup {
                error = warmupFn(shard!, context.context)
                if error.code != 0 {
                    return .failure(ShardError(message: error.message.toString()!))
                }
            }
        }
        return .success(())
    }

    func setParam(value: SHVar) -> Result<Void, ShardError> {
        reset()

        if value.valueType == VarType.ShardRef.asSHType() {
            // Handle single shard reference
            let shardPtr = value.payload.shardValue
            if shardPtr != nil {
                shardsPtrs.append(shardPtr)
            }
        } else if value.valueType == VarType.Seq.asSHType() {
            // Handle sequence of shards
            let seqLen = value.payload.seqValue.len
            for i in 0 ..< seqLen {
                let elemVar = value.payload.seqValue.elements[Int(i)]
                if elemVar.valueType == VarType.ShardRef.asSHType() {
                    let shardPtr = elemVar.payload.shardValue
                    if shardPtr != nil {
                        shardsPtrs.append(shardPtr)
                    }
                }
            }
        } else if value.valueType != VarType.NoValue.asSHType() {
            return .failure(ShardError(message: "Expected ShardRef or Seq<ShardRef>"))
        }
        // else: value.valueType == None is allowed, we just have an empty array

        paramValue = .init(cloning: value)

        // ALWAYS add NULL terminator for NULL-terminated array iteration
        // Even for empty arrays, this ensures nativeShards.elements is never nullptr
        shardsPtrs.append(nil)

        // Get persistent pointer to array's buffer (valid until array is reallocated)
        nativeShards.elements = shardsPtrs.withUnsafeMutableBufferPointer { $0.baseAddress }
        nativeShards.len = UInt32(shardsPtrs.count - 1) // Don't count the NULL terminator in length
        nativeShards.cap = UInt32(0)

        return .success(())
    }

    func getParam() -> SHVar {
        return paramValue.v
    }

    func compose(data: SHInstanceData) -> Result<SHComposeResult, ShardError> {
        // nativeShards is ALWAYS valid, even for empty arrays (they have [nullptr])
        composeResult = G.Core.pointee.composeShards(nativeShards, data)
        if composeResult.failed {
            return .failure(ShardError(message: composeResult.error.toString() ?? "", errorStackTrace: composeResult.errorStackTrace.toString() ?? ""))
        }

        requiredVariables = .init()
        requiredVariables.extend(types: composeResult.requiredInfo)
        exposedVariables = .init()
        exposedVariables.extend(types: composeResult.exposedInfo)

        return .success(composeResult)
    }

    func activate(context: Context, input: SHVar, output: inout SHVar) -> SHWireState {
        // nativeShards.elements is ALWAYS valid (never nullptr)
        // Even empty shards arrays have [nullptr] terminator, which VM loop handles correctly
        var inputCopy = input
        let state = withUnsafePointer(to: &inputCopy) { input in
            withUnsafeMutablePointer(to: &output) { ptr in
                G.Core.pointee.runShards(nativeShards.elements, context.context, input, ptr)
            }
        }
        return state
    }

    func activateHandlingReturn(
        context: OpaquePointer?, input: SHVar, output: UnsafeMutablePointer<SHVar>
    ) -> SHWireState {
        // nativeShards.elements is ALWAYS valid (never nullptr)
        // Even empty shards arrays have [nullptr] terminator, which VM loop handles correctly
        var inputCopy = input
        let state = withUnsafePointer(to: &inputCopy) { input in
            G.Core.pointee.runShards2(nativeShards.elements, context, input, output)
        }
        return state
    }

    func isEmpty() -> Bool {
        return shardsPtrs.isEmpty
    }

    func getExposing() -> ExposedTypes {
        return exposedVariables
    }

    func getRequiring() -> ExposedTypes {
        return requiredVariables
    }

    deinit {
        reset()
    }
}

public struct Context {
    public var context: OpaquePointer?

    public init(context: OpaquePointer?) {
        self.context = context
    }
}

public typealias ShardPtr = UnsafeMutablePointer<Shard>?

public final class ShardError: Error, LocalizedError {
    public var message: String
    public var errorStackTrace: String

    init(message: String, errorStackTrace: String = "") {
        self.message = message
        self.errorStackTrace = errorStackTrace
    }

    public var errorDescription: String? {
        return message
    }

    public var failureReason: String? {
        return errorStackTrace.isEmpty ? nil : errorStackTrace
    }
}

public class EnumInfo {
    var native = SHEnumInfo()

    private var name: ContiguousArray<CChar>
    private var help: ContiguousArray<CChar>

    private var labels: [ContiguousArray<CChar>]
    private var labelPointers: [UnsafePointer<CChar>?]

    private var descriptions: [ContiguousArray<CChar>]
    private var dscriptionsOpts: [SHOptionalString]

    private var values: [Int32]

    init(name: String, help: String, labels: [String] = [], descriptions: [String] = [], values: [Int32] = []) {
        self.name = name.utf8CString
        self.help = help.utf8CString

        self.labels = labels.map(\.utf8CString)
        labelPointers = self.labels.map { array -> UnsafePointer<CChar>? in
            array.withUnsafeBufferPointer { $0.baseAddress }
        }

        self.descriptions = descriptions.map(\.utf8CString)
        dscriptionsOpts = self.descriptions.map { array -> SHOptionalString in
            array.withUnsafeBufferPointer { buffer in
                SHOptionalString(string: buffer.baseAddress, crc: 0)
            }
        }

        self.values = values
    }

    func toSHEnumInfo() -> SHEnumInfo {
        var result = SHEnumInfo()

        name.withUnsafeBufferPointer {
            result.name = $0.baseAddress
        }
        help.withUnsafeBufferPointer {
            result.help = SHOptionalString(string: $0.baseAddress, crc: 0)
        }

        if !labels.isEmpty {
            labelPointers.withUnsafeBufferPointer { buffer in
                result.labels.elements = UnsafeMutablePointer(mutating: buffer.baseAddress)
            }
            result.labels.len = UInt32(labels.count)
            result.labels.cap = 0
        }

        if !descriptions.isEmpty {
            dscriptionsOpts.withUnsafeBufferPointer { buffer in
                result.descriptions.elements = UnsafeMutablePointer(mutating: buffer.baseAddress)
            }
            result.descriptions.len = UInt32(descriptions.count)
            result.descriptions.cap = 0
        }

        if !values.isEmpty {
            result.values.len = UInt32(values.count)
            result.values.elements = withUnsafeMutablePointer(to: &values[0]) { $0 }
            result.values.cap = 0
        }

        return result
    }
}

public class TypeInfo {
    var native = SHTypeInfo()

    init(type: VarType) {
        native.basicType = type.asSHType()
    }

    init(seqOf: TypeInfo) {
        native.basicType = VarType.Seq.asSHType()
        native.seqTypes.len = 1
        native.seqTypes.elements = withUnsafeMutablePointer(to: &seqOf.native) { $0 }
    }

    init(tableOf: TypeInfo) {
        native.basicType = VarType.Table.asSHType()
        native.table.types.len = 1
        native.table.types.elements = withUnsafeMutablePointer(to: &tableOf.native) { $0 }
    }

    init(variableOf: TypeInfo) {
        native.basicType = VarType.ContextVar.asSHType()
        native.contextVarTypes.len = 1
        native.contextVarTypes.elements = withUnsafeMutablePointer(to: &variableOf.native) { $0 }
    }
}

public class Types {
    private var types: [TypeInfo] // to keep alive
    public var native = SHTypesInfo()

    init(types: [TypeInfo]) {
        self.types = types
        for t in types {
            withUnsafeMutablePointer(to: &native) { ptr in
                withUnsafePointer(to: &t.native) { native in
                    G.Core.pointee.typesPush(ptr, native)
                }
            }
        }
    }

    deinit {
        withUnsafeMutablePointer(to: &native) { ptr in
            G.Core.pointee.typesFree(ptr)
        }
    }
}

public class ParameterInfo {
    var name: ContiguousArray<CChar>
    var help: ContiguousArray<CChar>
    var types: [TypeInfo] // to keep alive
    var typesStorage: ContiguousArray<SHTypeInfo> = []

    init(name: String, help: String, types: [TypeInfo]) {
        self.name = name.utf8CString
        self.help = help.utf8CString
        self.types = types
        for t in types {
            typesStorage.append(t.native)
        }
    }

    func toSHParameterInfo() -> SHParameterInfo {
        var result = SHParameterInfo()

        name.withUnsafeBufferPointer {
            result.name = $0.baseAddress
        }
        help.withUnsafeBufferPointer {
            result.help = SHOptionalString(string: $0.baseAddress, crc: 0)
        }
        withUnsafeMutablePointer(to: &typesStorage[0]) { ptr in
            result.valueTypes.elements = ptr
        }
        result.valueTypes.len = UInt32(types.count)
        result.valueTypes.cap = 0

        return result
    }
}

public class Parameters {
    private var infos: [ParameterInfo] = [] // to keep alive
    public var native = SHParametersInfo()

    func add(name: String, help: String, types: [TypeInfo]) {
        let info = ParameterInfo(name: name, help: help, types: types)
        infos.append(info)
    }

    func done() {
        for info in infos {
            var pInfo = info.toSHParameterInfo()
            withUnsafeMutablePointer(to: &native) { ptr in
                withUnsafePointer(to: &pInfo) { nativeInfo in
                    G.Core.pointee.paramsPush(ptr, nativeInfo)
                }
            }
        }
    }

    deinit {
        withUnsafeMutablePointer(to: &native) { ptr in
            G.Core.pointee.paramsFree(ptr)
        }
    }
}

public class ExposedTypeInfo {
    var name: ContiguousArray<CChar>
    var help: ContiguousArray<CChar>
    var exposedType: TypeInfo
    var isMutable: Bool
    var isProtected: Bool
    var global: Bool
    var declared: Bool

    init(
        name: String, help: String, exposedType: TypeInfo, isMutable: Bool = false,
        isProtected: Bool = false, global: Bool = false, declared: Bool = false
    ) {
        self.name = name.utf8CString
        self.help = help.utf8CString
        self.exposedType = exposedType
        self.isMutable = isMutable
        self.isProtected = isProtected
        self.global = global
        self.declared = declared
    }

    func toSHExposedTypeInfo() -> SHExposedTypeInfo {
        var result = SHExposedTypeInfo()

        name.withUnsafeBufferPointer {
            result.name = $0.baseAddress
        }
        help.withUnsafeBufferPointer {
            result.help = SHOptionalString(string: $0.baseAddress, crc: 0)
        }
        result.exposedType = exposedType.native

        result.isMutable = isMutable
        result.isProtected = isProtected
        result.global = global
        result.declared = declared

        return result
    }
}

public class ExposedTypes {
    private var types: [ExposedTypeInfo] // to keep alive
    public var native = SHExposedTypesInfo()

    init() {
        types = []
    }

    init(types: [ExposedTypeInfo]) {
        self.types = types
        for t in types {
            var eInfo = t.toSHExposedTypeInfo()
            withUnsafeMutablePointer(to: &native) { ptr in
                withUnsafePointer(to: &eInfo) { nativeInfo in
                    G.Core.pointee.expTypesPush(ptr, nativeInfo)
                }
            }
        }
    }

    func extend(types: SHExposedTypesInfo) {
        for i in 0 ..< types.len {
            var eInfo = types.elements[Int(i)]
            withUnsafeMutablePointer(to: &native) { ptr in
                withUnsafePointer(to: &eInfo) { nativeInfo in
                    G.Core.pointee.expTypesPush(ptr, nativeInfo)
                }
            }
        }
    }

    deinit {
        withUnsafeMutablePointer(to: &native) { ptr in
            G.Core.pointee.expTypesFree(ptr)
        }
    }
}

public protocol IShard: AnyObject {
    static var name: StaticString { get }
    static var help: StaticString { get }

    init()

    var inputTypes: Types { get }
    var outputTypes: Types { get }

    var parameters: Parameters { get }
    func setParam(idx: Int, value: SHVar) -> Result<Void, ShardError>
    func getParam(idx: Int) -> SHVar

    var exposedVariables: ExposedTypes { get }
    var requiredVariables: ExposedTypes { get }

    func compose(data: SHInstanceData) -> Result<SHTypeInfo, ShardError>

    func warmup(context: Context) -> Result<Void, ShardError>
    func cleanup(context: Context) -> Result<Void, ShardError>

    func activate(context: Context, input: SHVar) -> Result<SHVar, ShardError>

    // Need those... cos Swift generics are meh
    // I wasted a lot of time to find the optimal solution, don't think about wasting more
    // Could have used purely inherited classes but looked meh, could have done this that..
    // more here: https://chat.openai.com/share/023818da-89da-4f18-a79e-f46774e7fc8d (scoll down)
    static var inputTypesCFunc: SHInputTypesProc { get }
    static var outputTypesCFunc: SHInputTypesProc { get }
    static var destroyCFunc: SHDestroyProc { get }
    static var nameCFunc: SHNameProc { get }
    static var hashCFunc: SHHashProc { get }
    static var helpCFunc: SHHelpProc { get }
    static var parametersCFunc: SHParametersProc { get }
    static var setParamCFunc: SHSetParamProc { get }
    static var getParamCFunc: SHGetParamProc { get }
    static var exposedVariablesCFunc: SHExposedVariablesProc { get }
    static var requiredVariablesCFunc: SHRequiredVariablesProc { get }
    static var composeCFunc: SHComposeProc { get }
    static var warmupCFunc: SHWarmupProc { get }
    static var cleanupCFunc: SHCleanupProc { get }
    static var activateCFunc: SHActivateProc { get }
    var errorCache: ContiguousArray<CChar> { get set }
    var output: SHVar { get set }
}

extension IShard {}

@inlinable public func bridgeParameters<T: IShard>(_: T.Type, shard: ShardPtr) -> SHParametersInfo {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)
    return typedInstance.parameters.native
}

@inlinable public func bridgeName<T: IShard>(_: T.Type) -> UnsafePointer<Int8>? {
    return T.name.utf8Start.withMemoryRebound(to: Int8.self, capacity: 1) { $0 }
}

@inlinable public func bridgeHash<T: IShard>(_: T.Type) -> UInt32 {
    return hashShard(T.self)
}

@inlinable public func bridgeSetParam<T: IShard>(_: T.Type, shard: ShardPtr, idx: Int32, input: UnsafePointer<SHVar>?) -> SHError {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)

    var error = SHError()
    let result = typedInstance.setParam(idx: Int(idx), value: input!.pointee)
    switch result {
    case .success():
        return error
    case let .failure(err):
        error.code = 1
        typedInstance.errorCache = err.message.utf8CString
        error.message.string = typedInstance.errorCache.withUnsafeBufferPointer {
            $0.baseAddress
        }
        error.message.len = UInt64(typedInstance.errorCache.count - 1)
        return error
    }
}

@inlinable public func bridgeGetParam<T: IShard>(_: T.Type, shard: ShardPtr, idx: Int32) -> SHVar {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)
    return typedInstance.getParam(idx: Int(idx))
}

@inlinable public func bridgeHelp<T: IShard>(_: T.Type) -> SHOptionalString {
    var result = SHOptionalString()
    result.string = T.help.utf8Start.withMemoryRebound(to: Int8.self, capacity: 1) { $0 }
    return result
}

@inlinable public func bridgeDestroy<T: IShard>(_: T.Type, shard: ShardPtr) {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    _ = Unmanaged<T>.fromOpaque(instance).takeRetainedValue()
    shard!.deallocate()
}

@inlinable public func bridgeInputTypes<T: IShard>(_: T.Type, shard: ShardPtr) -> SHTypesInfo {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)
    return typedInstance.inputTypes.native
}

@inlinable public func bridgeOutputTypes<T: IShard>(_: T.Type, shard: ShardPtr) -> SHTypesInfo {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)
    return typedInstance.outputTypes.native
}

@inlinable public func bridgeCompose<T: IShard>(_: T.Type, shard: ShardPtr, data: UnsafeMutablePointer<SHInstanceData>?) -> SHShardComposeResult {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)

    var value = SHShardComposeResult()
    let result = typedInstance.compose(data: data!.pointee)
    switch result {
    case let .success(typ):
        value.result = typ
        return value
    case let .failure(err):
        var error = SHError()
        error.code = 1
        typedInstance.errorCache = err.message.utf8CString
        error.message.string = typedInstance.errorCache.withUnsafeBufferPointer {
            $0.baseAddress
        }
        error.message.len = UInt64(typedInstance.errorCache.count - 1)
        value.error = error
        return value
    }
}

@inlinable public func bridgeWarmup<T: IShard>(_: T.Type, shard: ShardPtr, ctx: OpaquePointer?) -> SHError {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)

    var error = SHError()
    let result = typedInstance.warmup(context: Context(context: ctx))
    switch result {
    case .success():
        return error
    case let .failure(err):
        error.code = 1
        typedInstance.errorCache = err.message.utf8CString
        error.message.string = typedInstance.errorCache.withUnsafeBufferPointer {
            $0.baseAddress
        }
        error.message.len = UInt64(typedInstance.errorCache.count - 1)
        return error
    }
}

@inlinable public func bridgeCleanup<T: IShard>(_: T.Type, shard: ShardPtr, ctx: OpaquePointer?) -> SHError {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)

    var error = SHError()
    let result = typedInstance.cleanup(context: Context(context: ctx))
    switch result {
    case .success():
        return error
    case let .failure(err):
        error.code = 1
        typedInstance.errorCache = err.message.utf8CString
        error.message.string = typedInstance.errorCache.withUnsafeBufferPointer {
            $0.baseAddress
        }
        error.message.len = UInt64(typedInstance.errorCache.count - 1)
        return error
    }
}

@inlinable public func bridgeActivate<T: IShard>(_: T.Type, shard: ShardPtr, ctx: OpaquePointer?, input: UnsafePointer<SHVar>?) -> UnsafePointer<SHVar>? {
    // Direct pointer cast to avoid Unmanaged overhead
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)

    // Cache output pointer location - avoids repeated property access
    let outputPtr = withUnsafeMutablePointer(to: &typedInstance.output) { $0 }

    // Process activation
    let result = typedInstance.activate(context: Context(context: ctx), input: input!.pointee)

    // Handle result - success path is hot, keep it simple
    if case let .success(res) = result {
        typedInstance.output = res
        return UnsafePointer(outputPtr)
    }

    // Error path unchanged - not performance critical
    if case let .failure(error) = result {
        error.message.withCString { cString in
            var shString = SHStringWithLen()
            shString.string = cString
            let length = error.message.lengthOfBytes(using: .utf8)
            shString.len = UInt64(length)
            G.Core.pointee.abortWire(ctx, shString)
        }
    }

    return UnsafePointer(outputPtr)
}

@inlinable public func bridgeExposedVariables<T: IShard>(_: T.Type, shard: ShardPtr) -> SHExposedTypesInfo {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)
    return typedInstance.exposedVariables.native
}

@inlinable public func bridgeRequiredVariables<T: IShard>(_: T.Type, shard: ShardPtr) -> SHExposedTypesInfo {
    let swiftShardPtr = UnsafeRawPointer(shard!).assumingMemoryBound(to: SwiftShard.self)
    let instance = unsafeBitCast(swiftShardPtr.pointee.swiftClass, to: UnsafeMutableRawPointer.self)
    let typedInstance = unsafeBitCast(instance, to: T.self)
    return typedInstance.requiredVariables.native
}

@inlinable public func hashShard<T: IShard>(_: T.Type) -> UInt32 {
    let name = T.name
    let namePtr = name.utf8Start.withMemoryRebound(to: UInt8.self, capacity: name.utf8CodeUnitCount)
        { $0 }
    let nameData = Data(bytes: namePtr, count: name.utf8CodeUnitCount)

    // Create a buffer with the shard name and SHARDS_CURRENT_ABI
    var buffer = [UInt8]()
    buffer.append(contentsOf: nameData)

    var abi = UInt32(SHARDS_CURRENT_ABI)
    withUnsafeBytes(of: &abi) { buffer.append(contentsOf: $0) }

    // Compute the hash using a simple algorithm (FNV-1a in this case)
    var hash: UInt32 = 2_166_136_261
    for byte in buffer {
        hash ^= UInt32(byte)
        hash &*= 16_777_619
    }

    return hash
}

func createSwiftShard<T: IShard>(_: T.Type) -> UnsafeMutablePointer<Shard>? {
    let shard = T()
    let cwrapper = UnsafeMutablePointer<SwiftShard>.allocate(capacity: 1)
    cwrapper.initialize(to: SwiftShard())

    cwrapper.pointee.header.name = T.nameCFunc
    cwrapper.pointee.header.hash = T.hashCFunc
    cwrapper.pointee.header.help = T.helpCFunc
    cwrapper.pointee.header.inputHelp = { _ in SHOptionalString() }
    cwrapper.pointee.header.outputHelp = { _ in SHOptionalString() }
    cwrapper.pointee.header.properties = { _ in nil }
    cwrapper.pointee.header.setup = { _ in }
    cwrapper.pointee.header.destroy = T.destroyCFunc
    cwrapper.pointee.header.activate = T.activateCFunc
    cwrapper.pointee.header.parameters = T.parametersCFunc
    cwrapper.pointee.header.setParam = T.setParamCFunc
    cwrapper.pointee.header.getParam = T.getParamCFunc
    cwrapper.pointee.header.inputTypes = T.inputTypesCFunc
    cwrapper.pointee.header.outputTypes = T.outputTypesCFunc
    cwrapper.pointee.header.warmup = T.warmupCFunc
    cwrapper.pointee.header.cleanup = T.cleanupCFunc
    cwrapper.pointee.header.compose = T.composeCFunc
    cwrapper.pointee.header.exposedVariables = T.exposedVariablesCFunc
    cwrapper.pointee.header.requiredVariables = T.requiredVariablesCFunc

    cwrapper.pointee.swiftClass = Unmanaged<T>.passRetained(shard).toOpaque()

    // Cast to Shard pointer without rebinding
    return UnsafeMutableRawPointer(cwrapper).assumingMemoryBound(to: Shard.self)
}

class WireController {
    init() {
        let cname = SHStringWithLen()
        nativeRef = G.Core.pointee.createWire(cname)
    }

    init(native: SHWireRef) {
        nativeRef = G.Core.pointee.referenceWire(native)
    }

    deinit {
        if nativeRef != nil {
            G.Core.pointee.destroyWire(nativeRef)
        }
    }

    var looped: Bool = false {
        didSet {
            G.Core.pointee.setWireLooped(nativeRef, looped)
        }
    }

    var unsafe: Bool = false {
        didSet {
            G.Core.pointee.setWireUnsafe(nativeRef, unsafe)
        }
    }

    var failed: Bool {
        let info = G.Core.pointee.getWireInfo(nativeRef)
        return info.failed
    }

    var failureMessage: String? {
        let info = G.Core.pointee.getWireInfo(nativeRef)
        if info.failed {
            return info.failureMessage.toString()
        }
        return nil
    }

    private func addExternalVar(
        name: String, varPtr: UnsafeMutablePointer<SHVar>, varType: UnsafePointer<SHTypeInfo>? = nil, trackingMask: UInt8 = 0
    ) {
        varPtr.pointee.flags |= UInt16(SHVAR_FLAGS_EXTERNAL)
        if trackingMask != 0 {
            varPtr.pointee.trackingMask |= trackingMask
        }
        var ev = SHExternalVariable()
        ev.var = varPtr
        if let varType = varType {
            ev.type = varType
        }

        name.withCString { cString in
            var cname = SHStringWithLen()
            cname.string = cString
            let length = name.lengthOfBytes(using: .utf8)
            cname.len = UInt64(length)
            G.Core.pointee.setExternalVariable(nativeRef, cname, &ev)
        }
    }

    func addExternal(name: String, owned: OwnedVar, trackingMask: UInt8 = 0) {
        addExternalVar(name: name, varPtr: owned.ptr(), trackingMask: trackingMask)
        references.append(owned)
    }

    func addExternal(name: String, owned: inout OwnedVar, trackingMask: UInt8 = 0) {
        addExternalVar(name: name, varPtr: owned.ptr(), trackingMask: trackingMask)
    }

    func addExternal(name: String, owned: inout OwnedVar, varType: inout SHTypeInfo, trackingMask: UInt8 = 0) {
        addExternalVar(name: name, varPtr: owned.ptr(), varType: &varType, trackingMask: trackingMask)
    }

    func addExternal(name: String, sequence: inout SeqVar, trackingMask: UInt8 = 0) {
        addExternalVar(name: name, varPtr: sequence.ptr(), trackingMask: trackingMask)
    }

    func addExternal(name: String, sequence: inout SeqVar, varType: inout SHTypeInfo, trackingMask: UInt8 = 0) {
        addExternalVar(name: name, varPtr: sequence.ptr(), varType: &varType, trackingMask: trackingMask)
    }

    func addExternal(name: String, table: inout TableVar, trackingMask: UInt8 = 0) {
        addExternalVar(name: name, varPtr: table.ptr(), trackingMask: trackingMask)
    }

    func addExternal(name: String, table: inout TableVar, varType: inout SHTypeInfo, trackingMask: UInt8 = 0) {
        addExternalVar(name: name, varPtr: table.ptr(), varType: &varType, trackingMask: trackingMask)
    }

    func addExternal(name: String, raw: inout SHVar, trackingMask: UInt8 = 0) {
        addExternalVar(name: name, varPtr: &raw, trackingMask: trackingMask)
    }

    func addExternal(name: String, raw: inout SHVar, varType: inout SHTypeInfo, trackingMask: UInt8 = 0) {
        addExternalVar(name: name, varPtr: &raw, varType: &varType, trackingMask: trackingMask)
    }

    func isRunning() -> Bool {
        G.Core.pointee.isWireRunning(nativeRef)
    }

    func setPriority(_ priority: Int) {
        G.Core.pointee.setWirePriority(nativeRef, Int32(priority))
    }

    func stop() {
        var result = G.Core.pointee.stopWire(nativeRef)
        withUnsafeMutablePointer(to: &result) { resultPtr in
            G.Core.pointee.destroyVar(resultPtr)
        }
    }

    public func wait() async {
        // Check copier status every 100ms
        while isRunning() {
            try? await Task.sleep(nanoseconds: 100_000_000) // 100ms
        }
    }

    var nativeRef = SHWireRef(bitPattern: 0)

    private var references: [OwnedVar] = []
}

class MeshController {
    init() {
        nativeRef = G.Core.pointee.createMesh()
    }

    init(borrowing: SHMeshRef) {
        nativeRef = borrowing
        self.borrowing = true
    }

    deinit {
        if !borrowing {
            if !errorCallbacks.isEmpty {
                for (userData, _) in errorCallbacks {
                    // Unregister with Shards
                    G.Core.pointee.unregisterErrorEvent(nativeRef, userData)
                }
            }

            G.Core.pointee.destroyMesh(nativeRef)
        }
    }

    func schedule(wire: WireController, compose: Bool = true) {
        G.Core.pointee.schedule(nativeRef, wire.nativeRef, compose)
    }

    func maybeSchedule(wire: WireController) -> Result<Void, ShardError> {
        let error = OwnedVar()
        let result = G.Core.pointee.compose(nativeRef, wire.nativeRef, &error.v)
        if result {
            schedule(wire: wire, compose: false)
            return .success(())
        }
        return .failure(ShardError(message: error.v.string))
    }

    func unschedule(wire: WireController) {
        G.Core.pointee.unschedule(nativeRef, wire.nativeRef)
    }

    func tick() -> Bool {
        G.Core.pointee.tick(nativeRef)
    }

    func isEmpty() -> Bool {
        G.Core.pointee.isEmpty(nativeRef)
    }

    func getVariable(name: String) -> UnsafeMutablePointer<SHVar> {
        return name.withCString { cString in
            var cname = SHStringWithLen()
            cname.string = cString
            let length = name.lengthOfBytes(using: .utf8)
            cname.len = UInt64(length)
            return G.Core.pointee.getMeshVariable(nativeRef, cname)!
        }
    }

    // Store callbacks to prevent deallocation while in use
    private var errorCallbacks: [UnsafeMutableRawPointer: (String, UInt32, UInt32) -> Void] = [:]

    // C function that will be called by Shards when an error occurs
    private let errorCallbackBridge:
        @convention(c) (UnsafeMutableRawPointer?, SHStringWithLen, UInt32, UInt32) -> Void = {
            userData, message, line, column in
            guard let userData = userData else { return }
            // Get the Swift closure from context
            let callbackHolder = Unmanaged<MeshController>.fromOpaque(userData)
                .takeUnretainedValue()

            if let callback = callbackHolder.errorCallbacks[userData] {
                let messageStr = message.toString() ?? "Unknown error"
                callback(messageStr, line, column)
            }
        }

    // Register a callback that will be called when an error occurs
    func registerErrorEvent(callback: @escaping (String, UInt32, UInt32) -> Void)
        -> UnsafeMutableRawPointer
    {
        // Create a context pointer to pass to the C function
        let context = Unmanaged.passUnretained(self).toOpaque()

        // Store the callback using the context pointer as the key
        errorCallbacks[context] = callback

        // Register with Shards
        G.Core.pointee.registerErrorEvent(nativeRef, context, errorCallbackBridge)

        return context
    }

    // Unregister a previously registered error callback
    func unregisterErrorEvent(userData: UnsafeMutableRawPointer) {
        // Remove from our map
        errorCallbacks.removeValue(forKey: userData)

        // Unregister with Shards
        G.Core.pointee.unregisterErrorEvent(nativeRef, userData)
    }

    // Store callbacks to prevent deallocation while in use
    private var variableChangeCallbacks: [UnsafeMutableRawPointer: (String, SHVar?, Bool, SHVar?) -> Void] = [:]

    // C function that will be called by Shards when a variable changes
    private let variableChangeCallbackBridge:
        @convention(c) (UnsafeMutableRawPointer?, SHStringWithLen, UnsafePointer<SHVar>?, Bool, UnsafePointer<SHVar>?) -> Void = {
            userData, name, key, isGlobal, value in
            guard let userData = userData else { return }
            // Get the Swift closure from context
            let callbackHolder = Unmanaged<MeshController>.fromOpaque(userData)
                .takeUnretainedValue()

            if let callback = callbackHolder.variableChangeCallbacks[userData] {
                let nameStr = name.toString() ?? "Unknown variable"
                callback(nameStr, key == nil ? nil : key?.pointee, isGlobal, value == nil ? nil : value?.pointee)
            }
        }

    // Register a callback that will be called when a variable changes
    func registerVariableChangeEvent(callback: @escaping (String, SHVar?, Bool, SHVar?) -> Void)
        -> UnsafeMutableRawPointer
    {
        // Create a context pointer to pass to the C function
        let context = Unmanaged.passUnretained(self).toOpaque()

        // Store the callback using the context pointer as the key
        variableChangeCallbacks.removeValue(forKey: context)
        variableChangeCallbacks[context] = callback

        // Register with Shards
        G.Core.pointee.registerVariableChangeEvent(nativeRef, context, variableChangeCallbackBridge)

        return context
    }

    // Unregister a previously registered variable change callback
    func unregisterVariableChangeEvent(userData: UnsafeMutableRawPointer) {
        // Remove from our map
        variableChangeCallbacks.removeValue(forKey: userData)

        // Unregister with Shards
        G.Core.pointee.unregisterVariableChangeEvent(nativeRef, userData)
    }

    var nativeRef = SHMeshRef(bitPattern: 0)
    private var borrowing: Bool = false
}

extension SHStringWithLen {
    // Create SHStringWithLen from array of CChar
    static func from(_ chars: ContiguousArray<CChar>) -> SHStringWithLen {
        var result = SHStringWithLen()
        result.string = chars.withUnsafeBufferPointer { $0.baseAddress }
        result.len = UInt64(chars.count - 1) // Subtract 1 to exclude null terminator
        return result
    }

    // Create SHStringWithLen from a static compile-time string
    static func fromStatic(_ staticString: StaticString) -> SHStringWithLen {
        var result = SHStringWithLen()
        result.string = staticString.withUTF8Buffer { buffer in
            unsafeBitCast(buffer.baseAddress, to: UnsafePointer<CChar>.self)
        }
        result.len = UInt64(staticString.utf8CodeUnitCount)
        return result
    }

    // Convert SHStringWithLen to Swift String
    func toString() -> String? {
        guard let cString = string else { return nil }
        return String(cString: cString)
    }

    // Create an empty SHStringWithLen
    static var empty: SHStringWithLen {
        var result = SHStringWithLen()
        result.string = nil
        result.len = 0
        return result
    }

    // Check if SHStringWithLen is empty
    var isEmpty: Bool {
        return len == 0 || string == nil
    }
}

class SwiftSWL {
    var chars: ContiguousArray<CChar> // store the CChar array directly

    init(_ string: String) {
        chars = string.utf8CString
    }

    func asSHStringWithLen() -> SHStringWithLen {
        SHStringWithLen.from(chars)
    }
}

@inlinable public func refCountedIncRef<T>(_: T.Type, ptr: UnsafeMutablePointer<RefCounted<T>>) {
    let rc = Unmanaged<RefCounted<T>>.fromOpaque(ptr).takeUnretainedValue()
    rc.incRef()
}

@inlinable public func refCountedDecRef<T>(_: T.Type, ptr: UnsafeMutablePointer<RefCounted<T>>) {
    let rc = Unmanaged<RefCounted<T>>.fromOpaque(ptr).takeUnretainedValue()
    rc.decRef()
}

public class RefCounted<T> {
    var value: T
    private var manualCount: Int = 1 // Start at 1 for the initial owner

    init(value: T) {
        self.value = value
    }

    public func incRef() {
        manualCount += 1
        _ = Unmanaged.passRetained(self)
    }

    public func decRef() {
        manualCount -= 1
        Unmanaged.passUnretained(self).release()
    }

    public func getSelf() -> UnsafeMutableRawPointer {
        return Unmanaged.passUnretained(self).toOpaque()
    }

    deinit {
        assert(manualCount == 1, "RefCounted object deallocated with incorrect reference count (should be 1): \(manualCount)")
    }

    public var refCount: Int {
        manualCount
    }
}

public struct ObjectInfo<T> {
    var info: SHObjectInfo = .init()
    var vendor: Int32 = 0
    var type: Int32 = 0

    public mutating func make(obj: RefCounted<T>) -> SHVar {
        let output = SHVar.object(vendorId: vendor, typeId: type, value: obj.getSelf(), objectInfo: &info)
        return output
    }
}

func registerObjectType<T>(_: T.Type, vendor: Int32, type: Int32, info: SHObjectInfo) -> ObjectInfo<T> {
    G.Core.pointee.registerObjectType(vendor, type, info)
    return ObjectInfo(info: info, vendor: vendor, type: type)
}

enum Shards {
    static func setRootPath(_ path: String) {
        path.withCString { cString in
            G.Core.pointee.setRootPath(cString)
        }
    }
    
    static func log(_ message: String) {
        message.withCString { cString in
            var shString = SHStringWithLen()
            shString.string = cString
            let length = message.lengthOfBytes(using: .utf8)
            shString.len = UInt64(length)
            G.Core.pointee.log(shString)
        }
    }

    static func logLevel(_ level: Int, _ message: String) {
        message.withCString { cString in
            var shString = SHStringWithLen()
            shString.string = cString
            let length = message.lengthOfBytes(using: .utf8)
            shString.len = UInt64(length)
            G.Core.pointee.logLevel(Int32(level), shString)
        }
    }

    static func registerObjectType(vendor: Int32, type: Int32, info: SHObjectInfo) {
        G.Core.pointee.registerObjectType(vendor, type, info)
    }

    static func registerEnumInfo(vendor: Int32, type: Int32, info: SHEnumInfo) {
        G.Core.pointee.registerEnumType(vendor, type, info)
    }

    static func maybeEvalWire(_ name: String, _ code: String, _ basePath: String) -> Result<WireController, ShardError> {
        // Create SHStringWithLen instances
        let nameStr = SwiftSWL(name)
        let codeStr = SwiftSWL(code)
        let basePathStr = SwiftSWL(basePath)

        // Create output AST struct
        var ast = SHLAst()

        // Read the AST
        let success = G.Core.pointee.read(nameStr.asSHStringWithLen(), codeStr.asSHStringWithLen(), basePathStr.asSHStringWithLen(), nil, 0, &ast)
        defer { G.Core.pointee.freeAst(&ast) }
        guard success, ast.error.message == nil else {
            let errorMessage = String(cString: ast.error.message)
            let line = ast.error.line
            let column = ast.error.column
            return .failure(ShardError(message: "Failed to read AST: \(errorMessage) at line \(line), column \(column)"))
        }
        // ast will have refcount of 0, need to bump it with a clone
        let astOwned = OwnedVar(cloning: ast.ast)

        // Create evaluation environment
        let emptyStr = SHStringWithLen.fromStatic("")
        let env = G.Core.pointee.createEvalEnv(emptyStr)

        // Create error struct for eval
        var evalError = SHLError()
        defer { G.Core.pointee.freeError(&evalError) }

        // Evaluate the AST
        let evalSuccess = G.Core.pointee.eval(env, &astOwned.v, &evalError)
        guard evalSuccess else {
            let errorMessage = String(cString: evalError.message)
            let line = evalError.line
            let column = evalError.column
            G.Core.pointee.freeEvalEnv(env)
            return .failure(ShardError(message: "Failed to evaluate AST: \(errorMessage) at line \(line), column \(column)"))
        }

        // Create output wire struct
        var outWire = SHLWire()
        defer { G.Core.pointee.freeWire(&outWire) }

        // Transform environment into a wire
        let transformSuccess = G.Core.pointee.transformEnv(
            env, nameStr.asSHStringWithLen(), &outWire
        ) // consumes env
        guard transformSuccess, outWire.error.message == nil else {
            let errorMessage = String(cString: outWire.error.message)
            let line = outWire.error.line
            let column = outWire.error.column
            return .failure(
                ShardError(
                    message:
                    "Failed to transform environment: \(errorMessage) at line \(line), column \(column)"
                ))
        }

        // Create WireController from the resulting wire
        let wireController = WireController(native: outWire.wire.pointee!)
        return .success(wireController)
    }

    static func evalWire(_ name: String, _ code: String, _ basePath: String) -> WireController? {
        let result = maybeEvalWire(name, code, basePath)
        switch result {
        case let .success(wireController):
            return wireController
        case .failure:
            return nil
        }
    }

    static func evalWire(_ name: String, _ ast: [UInt8]) -> WireController? {
        // Create SHStringWithLen instances
        let nameStr = SwiftSWL(name)

        // Read the AST
        var outAst = SHLAst()
        defer { G.Core.pointee.freeAst(&outAst) }

        let success = ast.withUnsafeBufferPointer { buffer in
            G.Core.pointee.loadAst(buffer.baseAddress!, UInt32(buffer.count), &outAst)
        }
        guard success, outAst.error.message == nil else {
            return nil
        }
        // ast will have refcount of 0, need to bump it with a clone
        let astOwned = OwnedVar(cloning: outAst.ast)

        // Create evaluation environment
        let emptyStr = SHStringWithLen.fromStatic("")
        let env = G.Core.pointee.createEvalEnv(emptyStr)

        // Create error struct for eval
        var evalError = SHLError()
        defer { G.Core.pointee.freeError(&evalError) }

        // Evaluate the AST
        let evalSuccess = G.Core.pointee.eval(env, &astOwned.v, &evalError) // consumes ast
        guard evalSuccess else {
            G.Core.pointee.freeEvalEnv(env)
            return nil
        }

        // Create output wire struct
        var outWire = SHLWire()
        defer { G.Core.pointee.freeWire(&outWire) }

        // Transform environment into a wire
        let transformSuccess = G.Core.pointee.transformEnv(env, nameStr.asSHStringWithLen(), &outWire) // consumes env
        guard transformSuccess, outWire.error.message == nil else {
            return nil
        }

        // Create WireController from the resulting wire
        let wireController = WireController(native: outWire.wire.pointee!)
        return wireController
    }

    enum SuspendResult {
        case canContinue
        case mustInterrupt
    }

    static func suspend(_ context: Context, _ duration: Double = 0.0) -> SuspendResult {
        if G.Core.pointee.suspend(context.context, duration) == SHWireState(rawValue: 0) {
            return .canContinue
        } else {
            return .mustInterrupt
        }
    }

    static func abortWire(context: Context, errorText: SHStringWithLen) {
        G.Core.pointee.abortWire(context.context, errorText)
    }

    static func addShardAlias(original: StaticString, alias: StaticString) {
        let originalName = SHStringWithLen.fromStatic(original)
        let aliasName = SHStringWithLen.fromStatic(alias)
        G.Core.pointee.addShardAlias(originalName, aliasName)
    }
}

#if canImport(SwiftUI)
    import Combine

    class VarObserver: ObservableObject {
        @Published var changeCounter: Int = 0

        private let mesh: MeshController
        private let name: String
        private let key: OwnedVar?
        private let global: Bool
        private var handle: UnsafeMutableRawPointer? = nil

        init(mesh: MeshController, name: String, key: OwnedVar? = nil, global: Bool = false) {
            self.mesh = mesh
            self.name = name
            self.key = key
            self.global = global

            // Direct callback - no Combine overhead
            if key != nil {
                handle = mesh.registerVariableChangeEvent { [weak self] varName, tableKey, isGlobal, _ in
                    if varName == name && isGlobal == global && tableKey == key!.v {
                        DispatchQueue.main.async {
                            self?.changeCounter += 1
                        }
                    }
                }
            } else {
                handle = mesh.registerVariableChangeEvent { [weak self] varName, _, isGlobal, _ in
                    if varName == name && isGlobal == global {
                        DispatchQueue.main.async {
                            self?.changeCounter += 1
                        }
                    }
                }
            }
        }

        deinit {
            if let handle = handle {
                mesh.unregisterVariableChangeEvent(userData: handle)
            }
        }
    }
#endif

#if canImport(UIKit) && !os(watchOS)
    import UIKit

    extension UIView {
        #if !WIDGETS
            var safeArea: UIEdgeInsets {
                if #available(iOS 11, *) {
                    if let window = (UIApplication.shared.connectedScenes.first as? UIWindowScene)?
                        .windows.first
                    {
                        return window.safeAreaInsets
                    }
                }
                return UIEdgeInsets(top: 0.0, left: 0.0, bottom: 0.0, right: 0.0)
            }
        #endif
    }

    #if !WIDGETS
        @_cdecl("shards_get_uiview_safe_area")
        public func getViewSafeArea(
            uiEdgeInsets: UnsafeMutablePointer<UIEdgeInsets>, viewPtr: UnsafeMutableRawPointer?
        ) {
            let view = Unmanaged<UIView>.fromOpaque(viewPtr!).takeUnretainedValue()
            uiEdgeInsets.pointee = view.safeArea
        }
    #endif

    extension OwnedVar {
        public static func from(image: UIImage) -> OwnedVar? {
            guard let cgImage = image.cgImage else {
                print("Unable to get CGImage.")
                return nil
            }

            // Base width/height from the cgImage
            let width = cgImage.width
            let height = cgImage.height
            let bytesPerPixel = 4 // RGBA
            var drawWidth = width
            var drawHeight = height

            // Adjust width/height if orientation is rotated 90 or 270 degrees
            var transform = CGAffineTransform.identity
            switch image.imageOrientation {
            case .down, .downMirrored:
                transform =
                    transform
                        .translatedBy(x: CGFloat(width), y: CGFloat(height))
                        .rotated(by: .pi)
            case .left, .leftMirrored:
                swap(&drawWidth, &drawHeight)
                transform =
                    transform
                        .translatedBy(x: CGFloat(drawWidth), y: 0)
                        .rotated(by: .pi / 2)
            case .right, .rightMirrored:
                swap(&drawWidth, &drawHeight)
                transform =
                    transform
                        .translatedBy(x: 0, y: CGFloat(drawHeight))
                        .rotated(by: -.pi / 2)
            default:
                break
            }

            let rowStride = drawWidth * bytesPerPixel
            let totalBytes = drawHeight * rowStride

            let result = OwnedVar()
            result.v.valueType = VarType.Image.asSHType()
            result.v.payload.imageValue = G.Core.pointee.imageNew(UInt32(totalBytes))

            // Update final image dimensions after orientation adjustments
            result.v.payload.imageValue.pointee.width = UInt16(drawWidth)
            result.v.payload.imageValue.pointee.height = UInt16(drawHeight)
            result.v.payload.imageValue.pointee.channels = UInt8(bytesPerPixel)
            result.v.payload.imageValue.pointee.rowStride = UInt16(rowStride)
            result.v.payload.imageValue.pointee.flags = UInt8(SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA)

            let colorSpace = CGColorSpaceCreateDeviceRGB()
            let bitmapInfo = CGImageAlphaInfo.premultipliedLast.rawValue

            guard
                let context = CGContext(
                    data: result.v.payload.imageValue.pointee.data,
                    width: drawWidth,
                    height: drawHeight,
                    bitsPerComponent: 8,
                    bytesPerRow: rowStride,
                    space: colorSpace,
                    bitmapInfo: bitmapInfo
                )
            else {
                print("Unable to create CGContext.")
                return nil
            }

            context.concatenate(transform)
            let rect = CGRect(x: 0, y: 0, width: width, height: height)
            context.draw(cgImage, in: rect)

            return result
        }
    }

    public extension SHVar {
        init(color: UIColor) {
            self.init()
            valueType = VarType.Color.asSHType()
            // this is a mess.. let's use getRed
            var r: CGFloat = 0
            var g: CGFloat = 0
            var b: CGFloat = 0
            var a: CGFloat = 0
            color.getRed(&r, green: &g, blue: &b, alpha: &a)
            payload.colorValue.r = UInt8(r * 255)
            payload.colorValue.g = UInt8(g * 255)
            payload.colorValue.b = UInt8(b * 255)
            payload.colorValue.a = UInt8(a * 255)
        }

        var nativeColor: UIColor {
            assert(valueType == VarType.Color.asSHType(), "Value is not a color")
            return UIColor(
                red: CGFloat(payload.colorValue.r) / 255.0,
                green: CGFloat(payload.colorValue.g) / 255.0,
                blue: CGFloat(payload.colorValue.b) / 255.0,
                alpha: CGFloat(payload.colorValue.a) / 255.0
            )
        }
    }
#endif

#if canImport(CoreGraphics)
    import CoreGraphics

    extension SHVar {
        func toCGImage() throws -> CGImage {
            let imageValue = payload.imageValue!
            let channels = Int(imageValue.pointee.channels)
            let width = Int(imageValue.pointee.width)
            let height = Int(imageValue.pointee.height)
            let data = imageValue.pointee.data

            // Create CGImage from raw data
            let colorSpace = CGColorSpaceCreateDeviceRGB()
            let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue)
            let bytesPerRow = width * channels

            // Create CFData from raw pointer
            let dataSize = height * bytesPerRow
            let cfData = CFDataCreate(kCFAllocatorDefault, data, dataSize)

            guard let provider = CGDataProvider(data: cfData!),
                  let cgImage = CGImage(
                      width: width,
                      height: height,
                      bitsPerComponent: 8,
                      bitsPerPixel: channels * 8,
                      bytesPerRow: bytesPerRow,
                      space: colorSpace,
                      bitmapInfo: bitmapInfo,
                      provider: provider,
                      decode: nil,
                      shouldInterpolate: false,
                      intent: .defaultIntent
                  )
            else {
                throw ShardError(message: "Failed to create CGImage from image data")
            }

            return cgImage
        }
    }
#endif

#if canImport(AppKit) && !targetEnvironment(macCatalyst)
    import AppKit

    extension OwnedVar {
        public static func from(image: NSImage) -> OwnedVar? {
            guard let cgImage = image.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
                print("Unable to get CGImage from NSImage")
                return nil
            }

            let width = cgImage.width
            let height = cgImage.height
            let bytesPerPixel = 4 // RGBA
            let rowStride = width * bytesPerPixel
            let totalBytes = height * rowStride

            let result = OwnedVar()
            result.v.valueType = VarType.Image.asSHType()
            result.v.payload.imageValue = G.Core.pointee.imageNew(UInt32(totalBytes))

            result.v.payload.imageValue.pointee.width = UInt16(width)
            result.v.payload.imageValue.pointee.height = UInt16(height)
            result.v.payload.imageValue.pointee.channels = UInt8(bytesPerPixel)
            result.v.payload.imageValue.pointee.rowStride = UInt16(rowStride)
            result.v.payload.imageValue.pointee.flags = UInt8(SHIMAGE_FLAGS_PREMULTIPLIED_ALPHA)

            let colorSpace = CGColorSpaceCreateDeviceRGB()
            let bitmapInfo = CGImageAlphaInfo.premultipliedLast.rawValue

            guard
                let context = CGContext(
                    data: result.v.payload.imageValue.pointee.data,
                    width: width,
                    height: height,
                    bitsPerComponent: 8,
                    bytesPerRow: rowStride,
                    space: colorSpace,
                    bitmapInfo: bitmapInfo
                )
            else {
                print("Unable to create CGContext.")
                return nil
            }

            // Draw the CGImage into the context
            let rect = CGRect(x: 0, y: 0, width: width, height: height)
            context.draw(cgImage, in: rect)

            return result
        }
    }

    extension SHVar {
        init(color: NSColor) {
            self.init()
            valueType = VarType.Color.asSHType()
            payload.colorValue.r = UInt8(color.redComponent * 255)
            payload.colorValue.g = UInt8(color.greenComponent * 255)
            payload.colorValue.b = UInt8(color.blueComponent * 255)
        }

        var nativeColor: NSColor {
            assert(valueType == VarType.Color.asSHType(), "Value is not a color")
            return NSColor(
                red: CGFloat(payload.colorValue.r) / 255.0,
                green: CGFloat(payload.colorValue.g) / 255.0,
                blue: CGFloat(payload.colorValue.b) / 255.0,
                alpha: CGFloat(payload.colorValue.a) / 255.0
            )
        }
    }
#endif
