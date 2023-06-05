/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2023 Fragcolor Pte. Ltd. */

// WIP: This is a work in progress

import shards

struct Globals {
    var Core: UnsafeMutablePointer<SHCore>

    init() {
        Core = shardsInterface(UInt32(SHARDS_CURRENT_ABI))
        print("Shards Swift runtime initialized!")
        Core.pointee.registerShard(MyShard.name, MyShard.factory)
    }
}

var G = Globals()

public enum VarType : UInt8, CustomStringConvertible, CaseIterable {
    case NoValue
    case AnyValue
    case Enum
    case Bool
    case Int    // A 64bits int
    case Int2   // A vector of 2 64bits ints
    case Int3   // A vector of 3 32bits ints
    case Int4   // A vector of 4 32bits ints
    case Int8   // A vector of 8 16bits ints
    case Int16  // A vector of 16 8bits ints
    case Float  // A 64bits float
    case Float2 // A vector of 2 64bits floats
    case Float3 // A vector of 3 32bits floats
    case Float4 // A vector of 4 32bits floats
    case Color  // A vector of 4 uint8
    case Shard  // a shard, useful for future introspection shards!
    case EndOfBlittableTypes = 50 // anything below this is not blittable (ish)
    case Bytes // pointer + size
    case String
    case Path       // An OS filesystem path
    case ContextVar // A string label to find from SHContext variables
    case Image
    case Seq
    case Table
    case Wire
    case Object
    case Array // Notice: of just blittable types!

    public var description: String {
        get {
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
            case .Shard:
                return "Shard"
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
            case .Array:
                return "Array"
            default:
                fatalError("Type not found!")
            }
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
}

extension SHVar : CustomStringConvertible {
    public var description: String {
        switch type {
        case .NoValue:
            return "nil"
        case .AnyValue:
            return "Any"
        case .Enum:
            return "(Enum \(payload.enumVendorId) \(payload.enumTypeId) \(payload.enumValue))"
        case .Bool:
            return "\(payload.boolValue)"
        case .Int:
            return "\(payload.intValue)"
        case .Int2:
            return "(Int2 \(payload.int2Value.x) \(payload.int2Value.y))"
        case .Int3:
            return "(Int3 \(payload.int3Value.x) \(payload.int3Value.y) \(payload.int3Value.z))"
        case .Int4:
            return "\(payload.int4Value)"
        case .Int8:
            return "\(payload.int8Value)"
        case .Int16:
            return "\(payload.int16Value)"
        case .Float:
            return "\(payload.floatValue)"
        case .Float2:
            return "\(payload.float2Value)"
        case .Float3:
            return "\(payload.float3Value)"
        case .Float4:
            return "\(payload.float4Value)"
        case .Color:
            return "Color"
        case .Shard:
            return "Shard"
        case .Bytes:
            return "Bytes"
        case .String:
            return .init(cString: payload.stringValue)
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
        case .Array:
            return "Array"
        default:
            fatalError("Type not found!")
        }
    }

    public var typename: String {
        get {
            type.description
        }
    }

    public var type: VarType {
        get {
            VarType(rawValue: self.valueType.rawValue)!
        }
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
            self = .init(value: newValue)
        }
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
        } set {
            self = .init(value: newValue)
        }
    }

    init(x: Int64, y: Int64) {
        var v = SHVar()
        v.valueType = Int2
        v.payload.int2Value = SHInt2(x: x, y: y)
        self = v
    }

    init(value: SIMD2<Int64>) {
        var v = SHVar()
        v.valueType = Int2
        v.payload.int2Value = value
        self = v
    }

    public init(value: Float) {
        var v = SHVar()
        v.valueType = Float
        v.payload.floatValue = SHFloat(value)
        self = v
    }

    public var float: Float {
        get {
            assert(type == .Float, "Float variable expected!")
            return Float(payload.floatValue)
        } set {
            self = .init(value: newValue)
        }
    }

    init(value: inout ContiguousArray<CChar>) {
        var v = SHVar()
        v.valueType = String
        value.withUnsafeBufferPointer {
            v.payload.stringValue = $0.baseAddress
            v.payload.stringLen = UInt32(value.count - 1) // assumes \0 terminator
            v.payload.stringCapacity = UInt32(value.capacity)
        }
        self = v
    }

    public var string: String {
        get {
            assert(type == .String, "String variable expected!")
            return .init(cString: payload.stringValue)
        }
    }

    init(value: ShardPtr) {
        var v = SHVar()
        v.valueType = SHType(rawValue: VarType.Shard.rawValue)
        v.payload.shardValue = value
        self = v
    }

    public var shard: ShardPtr {
        get {
            assert(type == .Shard, "Shard variable expected!")
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
        } set {
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
}

public struct SHContext {
    var context: OpaquePointer?
}

public typealias ShardPtr = Optional<UnsafeMutablePointer<Shard>>

public protocol IShard : AnyObject {
    static var name: String { get }
    static var help: String { get }

    init()

    func destroy()
    func inputTypes() -> [SHTypeInfo]
    func outputTypes() -> [SHTypeInfo]
    func activate(context: SHContext, input: SHVar) -> SHVar

    static var factory: SHShardConstructor { get }
    static var nameProc: SHNameProc { get }
    static var activateProc: SHActivateProc { get }
}

public protocol IShardWithDestroy : IShard {
    var destroyProc: SHDestroyProc { get }
}

func cshardCreate<T: IShard>(_: T.Type) -> UnsafeMutablePointer<Shard>? {
    #if DEBUG
    print("Creating swift shard: \(T.name)")
    #endif
    let shard = T()
    let cwrapper = UnsafeMutablePointer<SwiftShard>.allocate(capacity: 1)
    cwrapper.pointee.header.name = T.nameProc
    if let blk = shard as? IShardWithDestroy {
        cwrapper.pointee.header.destroy = blk.destroyProc
    } else {
        // TODO
    }
    cwrapper.pointee.header.activate = T.activateProc
    cwrapper.pointee.swiftClass = Unmanaged<T>.passRetained(shard).toOpaque()
    let ptr = cwrapper.withMemoryRebound(to: UnsafeMutablePointer<Shard>.self, capacity: 1) {
        $0.pointee
    }
    return ptr;
}

func cshardName<T: IShard>(_: T.Type) -> UnsafePointer<Int8>? {
    T.name.utf8CString.withUnsafeBufferPointer {
        $0.baseAddress
    }
}

func cshardDestroy<T: IShard>(_: T.Type, shard: ShardPtr) {
    #if DEBUG
    print("Destroying swift shard: \(T.name)")
    #endif
    _ = shard!.withMemoryRebound(to: UnsafeMutablePointer<SwiftShard>.self, capacity: 1) {
        Unmanaged<T>.fromOpaque($0.pointee.pointee.swiftClass).takeRetainedValue()
    }
    // let it release
}

func cshardActivate<T: IShard>(_: T.Type, shard: ShardPtr, ctx: OpaquePointer?, input: UnsafePointer<SHVar>?) -> SHVar {
    let b = shard!.withMemoryRebound(to: UnsafeMutablePointer<SwiftShard>.self, capacity: 1) {
        Unmanaged<T>.fromOpaque($0.pointee.pointee.swiftClass).takeUnretainedValue()
    }
    return b.activate(context: SHContext(context: ctx), input: input!.pointee)
}

final class MyShard : IShard, IShardWithDestroy {
    static var factory: SHShardConstructor = { cshardCreate(MyShard.self) }
    static var nameProc: SHNameProc = { _ in cshardName(MyShard.self) }
    var destroyProc: SHDestroyProc = { cshardDestroy(MyShard.self, shard: $0) }
    static var activateProc: SHActivateProc = { cshardActivate(MyShard.self, shard: $0, ctx: $1, input: $2) }

    func destroy() {
    }

    static var name: String = "MyShard"

    static var help: String = ""

    func inputTypes() -> [SHTypeInfo] {
        []
    }

    func outputTypes() -> [SHTypeInfo] {
        []
    }

    func activate(context: SHContext, input: SHVar) -> SHVar {
        SHVar()
    }
}

struct Parameter {
    private weak var owner: ShardController?
    var info: ParameterInfo

    public var value: SHVar {
        get {
            owner!.getParam(index: info.index)
        }
        set {
            _ = owner!.setParam(index: info.index, value: newValue)
        }
    }

    public init(shard: ShardController, info: ParameterInfo) {
        owner = shard
        self.info = info
    }
}

class ParameterInfo {
    init(name: String, types: SHTypesInfo, index: Int) {
        self.name = name
        self.types = types
        self.index = index
    }

    // init(name: String, help: String, types: SHTypesInfo, index: Int) {
    //     self.name = name
    //     self.help = help
    //     self.types = types
    //     self.index = index
    // }

    var name: String
    // var help: String
    var types: SHTypesInfo
    var index: Int
}

class ShardController : Equatable, Identifiable {
    var id: Int {
        get {
            nativeShard.hashValue
        }
    }

    convenience init(name: String) {
        self.init(native: G.Core.pointee.createShard(name)!)
    }

    init(native: ShardPtr) {
        nativeShard = native

        let blkname = nativeShard?.pointee.name(nativeShard!)
        let nparams = nativeShard?.pointee.parameters(nativeShard)
        if (nparams?.len ?? 0) > 0 {
            for i in 0..<nparams!.len {
                let nparam = nparams!.elements[Int(i)]
                let nparamId = "\(blkname!)-\(nparam.name!)"
                if let info = ShardController.infos[nparamId] {
                    params.append(Parameter(shard: self, info: info))
                } else {
                    let info = ParameterInfo(
                        name: .init(cString: nparam.name!),
                        // help: .init(cString: nparam.help!),
                        types: nparam.valueTypes,
                        index: Int(i))
                    ShardController.infos[nparamId] = info
                    params.append(Parameter(shard: self, info: info))
                }
            }
        }
    }

    deinit {
        if nativeShard != nil {
            if !nativeShard!.pointee.owned {
                nativeShard!.pointee.destroy(nativeShard)
            }
        }
    }

    static func ==(lhs: ShardController, rhs: ShardController) -> Bool {
        return lhs.nativeShard == rhs.nativeShard
    }

    var inputTypes: UnsafeMutableBufferPointer<SHTypeInfo> {
        get {
            let infos = nativeShard!.pointee.inputTypes(nativeShard!)
            return .init(start: infos.elements, count: Int(infos.len))
        }
    }

    var noInput: Bool {
        get {
            inputTypes.allSatisfy {info in
                info.basicType == None
            }
        }
    }

    var outputTypes: UnsafeMutableBufferPointer<SHTypeInfo> {
        get {
            let infos = nativeShard!.pointee.outputTypes(nativeShard!)
            return .init(start: infos.elements, count: Int(infos.len))
        }
    }

    var name: String {
        get {
            .init(cString: nativeShard!.pointee.name(nativeShard)!)
        }
    }

    var parameters: [Parameter] {
        get {
            params
        }
    }

    func getParam(index: Int) -> SHVar {
        nativeShard!.pointee.getParam(nativeShard, Int32(index))
    }

    func setParam(index: Int, value: SHVar) -> ShardController {
        #if DEBUG
        print("setParam(\(index), \(value))")
        #endif
        var mutableValue = value
        let error = nativeShard!.pointee.setParam(nativeShard, Int32(index), &mutableValue)
        if error.code != SH_ERROR_NONE {
            print("Error setting parameter: \(error)")
        }
        return self
    }

    func setParam(name: String, value: SHVar) -> ShardController {
        for idx in params.indices {
            if params[idx].info.name == name {
                params[idx].value = value
                return self;
            }
        }
        fatalError("Parameter not found! \(name)")
    }

    func setParam(name: String, string: String) -> ShardController {
        for idx in params.indices {
            if params[idx].info.name == name {
                var ustr = string.utf8CString
                params[idx].value = SHVar(value: &ustr)
                return self;
            }
        }
        fatalError("Parameter not found! \(name)")
    }

    func setParam(index: Int, string: String) -> ShardController {
        var ustr = string.utf8CString
        params[index].value = SHVar(value: &ustr)
        return self
    }

    func setParam(name: String, @ShardsBuilder _ contents: () -> [ShardController]) -> ShardController {
        let shards = contents()
        for idx in params.indices {
            if params[idx].info.name == name {
                var vshards = shards.map {
                    SHVar(value: $0.nativeShard!)
                }
                vshards.withUnsafeMutableBufferPointer {
                    params[idx].value.seq = $0
                }
                return self;
            }
        }
        fatalError("Parameter not found! \(name)")
    }

    var nativeShard: ShardPtr
    var params = [Parameter]()
    static var infos: [String: ParameterInfo] = [:]
}

@resultBuilder struct ShardsBuilder {
    static func buildBlock(_ components: ShardController...) -> [ShardController] {
        components
    }

    static func buildBlock(_ component: ShardController) -> [ShardController] {
        [component]
    }

    static func buildOptional(_ component: [ShardController]?) -> [ShardController] {
        component ?? []
    }

    static func buildEither(first: [ShardController]) -> [ShardController] {
        first
    }

    static func buildEither(second: [ShardController]) -> [ShardController] {
        second
    }

    static func buildArray(_ components: [[ShardController]]) -> [ShardController] {
        components.flatMap { $0 }
    }

    static func buildExpression(_ expression: ShardController) -> [ShardController] {
        [expression]
    }

    static func buildExpression(_ expression: [ShardController]) -> [ShardController] {
        expression
    }

    static func buildLimitedAvailability(_ component: [ShardController]) -> [ShardController] {
        component
    }

    static func buildFinalResult(_ component: [ShardController]) -> [ShardController] {
        component
    }
}

class WireController {
    init() {
        nativeRef = G.Core.pointee.createWire()
    }

    convenience init(shards: [ShardController]) {
        self.init()
        shards.forEach {
            add(shard: $0)
        }
    }

    convenience init(@ShardsBuilder _ contents: () -> [ShardController]) {
        self.init(shards: contents())
    }

    deinit {
        if nativeRef != nil {
            // release any ref we created
            // before destroy otherwise dangling check fails!
            for ref in refs {
                G.Core.pointee.releaseVariable(ref.value)
            }

            G.Core.pointee.destroyWire(nativeRef)

            // wire will delete shards
            for shard in shards {
                shard.nativeShard = nil
            }
        }
    }

    func add(shard: ShardController) {
        if !shards.contains(shard) {
            shards.append(shard)
            G.Core.pointee.addShard(nativeRef, shard.nativeShard)
        } else {
            fatalError("Wire already had the same shard!")
        }
    }

    func remove(shard: ShardController) {
        if let idx = shards.firstIndex(of: shard) {
            G.Core.pointee.removeShard(nativeRef, shard.nativeShard)
            shards.remove(at: idx)
        } else {
            fatalError("Wire did not have that shard!")
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

    var name: String = "" {
        didSet {
            G.Core.pointee.setWireName(nativeRef, name)
        }
    }

    func variable(name: String) -> UnsafeMutablePointer<SHVar> {
        if let current = refs[name] {
            return current
        } else {
            let r = G.Core.pointee.referenceWireVariable(nativeRef, name)!
            refs[name] = r
            return r
        }
    }

    var nativeRef = SHWireRef(bitPattern: 0)
    var shards = [ShardController]()
    var refs: [String: UnsafeMutablePointer<SHVar>] = [:]
}

class MeshController {
    init() {
        nativeRef = G.Core.pointee.createMesh()
    }

    deinit {
        G.Core.pointee.destroyMesh(nativeRef)
    }

    func schedule(wire: WireController) {
        wires.append(wire)
        G.Core.pointee.schedule(nativeRef, wire.nativeRef)
    }

    func unschedule(wire: WireController) {
        G.Core.pointee.unschedule(nativeRef, wire.nativeRef)
        let idx = wires.firstIndex {
            $0.nativeRef == wire.nativeRef
        }
        wires.remove(at: idx!)
    }

    func tick() -> Bool {
        G.Core.pointee.tick(nativeRef)
    }

    var nativeRef = SHMeshRef(bitPattern: 0)
    var wires = [WireController]()
}
