#ifndef NAGA_NATIVE_BINDINGS_HPP
#define NAGA_NATIVE_BINDINGS_HPP

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

namespace naga {
struct Statement;
struct Constant;
struct Type;
struct Expression;

// Manually defined first to fix cyclic dependency issues
struct Block {
  const Statement *body;
  uint32_t body_len;
};
}

namespace naga {

/// Operation that can be applied on two values.
///
/// ## Arithmetic type rules
///
/// The arithmetic operations `Add`, `Subtract`, `Multiply`, `Divide`, and
/// `Modulo` can all be applied to [`Scalar`] types other than [`Bool`], or
/// [`Vector`]s thereof. Both operands must have the same type.
///
/// `Add` and `Subtract` can also be applied to [`Matrix`] values. Both operands
/// must have the same type.
///
/// `Multiply` supports additional cases:
///
/// -   A [`Matrix`] or [`Vector`] can be multiplied by a scalar [`Float`],
///     either on the left or the right.
///
/// -   A [`Matrix`] on the left can be multiplied by a [`Vector`] on the right
///     if the matrix has as many columns as the vector has components (`matCxR
///     * VecC`).
///
/// -   A [`Vector`] on the left can be multiplied by a [`Matrix`] on the right
///     if the matrix has as many rows as the vector has components (`VecR *
///     matCxR`).
///
/// -   Two matrices can be multiplied if the left operand has as many columns
///     as the right operand has rows (`matNxR * matCxN`).
///
/// In all the above `Multiply` cases, the byte widths of the underlying scalar
/// types of both operands must be the same.
///
/// Note that `Multiply` supports mixed vector and scalar operations directly,
/// whereas the other arithmetic operations require an explicit [`Splat`] for
/// mixed-type use.
///
/// [`Scalar`]: TypeInner::Scalar
/// [`Vector`]: TypeInner::Vector
/// [`Matrix`]: TypeInner::Matrix
/// [`Float`]: ScalarKind::Float
/// [`Bool`]: ScalarKind::Bool
/// [`Splat`]: Expression::Splat
enum class BinaryOperator {
  Add,
  Subtract,
  Multiply,
  Divide,
  /// Equivalent of the WGSL's `%` operator or SPIR-V's `OpFRem`
  Modulo,
  Equal,
  NotEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  And,
  ExclusiveOr,
  InclusiveOr,
  LogicalAnd,
  LogicalOr,
  ShiftLeft,
  /// Right shift carries the sign of signed integers only.
  ShiftRight,
};

/// Enables adjusting depth without disabling early Z.
///
/// To use in a shader:
///   - GLSL: `layout (depth_<greater/less/unchanged/any>) out float gl_FragDepth;`
///     - `depth_any` option behaves as if the layout qualifier was not present.
///   - HLSL: `SV_DepthGreaterEqual`/`SV_DepthLessEqual`/`SV_Depth`
///   - SPIR-V: `ExecutionMode Depth<Greater/Less/Unchanged>`
///   - WGSL: `@early_depth_test(greater_equal/less_equal/unchanged)`
///
/// For more, see:
///   - <https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_conservative_depth.txt>
///   - <https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics#system-value-semantics>
///   - <https://www.khronos.org/registry/SPIR-V/specs/unified1/SPIRV.html#Execution_Mode>
enum class ConservativeDepth {
  /// Shader may rewrite depth only with a value greater than calculated.
  GreaterEqual,
  /// Shader may rewrite depth smaller than one that would have been written without the modification.
  LessEqual,
  /// Shader may not rewrite depth value.
  Unchanged,
};

/// Axis on which to compute a derivative.
enum class DerivativeAxis {
  X,
  Y,
  Width,
};

/// Hint at which precision to compute a derivative.
enum class DerivativeControl {
  Coarse,
  Fine,
  None,
};

/// The number of dimensions an image has.
enum class ImageDimension {
  /// 1D image
  D1,
  /// 2D image
  D2,
  /// 3D image
  D3,
  /// Cube map
  Cube,
};

/// The interpolation qualifier of a binding or struct field.
enum class Interpolation {
  /// The value will be interpolated in a perspective-correct fashion.
  /// Also known as "smooth" in glsl.
  Perspective,
  /// Indicates that linear, non-perspective, correct
  /// interpolation must be used.
  /// Also known as "no_perspective" in glsl.
  Linear,
  /// Indicates that no interpolation will be performed.
  Flat,
};

/// Built-in shader function for math.
enum class MathFunction {
  Abs,
  Min,
  Max,
  Clamp,
  Saturate,
  Cos,
  Cosh,
  Sin,
  Sinh,
  Tan,
  Tanh,
  Acos,
  Asin,
  Atan,
  Atan2,
  Asinh,
  Acosh,
  Atanh,
  Radians,
  Degrees,
  Ceil,
  Floor,
  Round,
  Fract,
  Trunc,
  Modf,
  Frexp,
  Ldexp,
  Exp,
  Exp2,
  Log,
  Log2,
  Pow,
  Dot,
  Outer,
  Cross,
  Distance,
  Length,
  Normalize,
  FaceForward,
  Reflect,
  Refract,
  Sign,
  Fma,
  Mix,
  Step,
  SmoothStep,
  Sqrt,
  InverseSqrt,
  Inverse,
  Transpose,
  Determinant,
  CountTrailingZeros,
  CountLeadingZeros,
  CountOneBits,
  ReverseBits,
  ExtractBits,
  InsertBits,
  FindLsb,
  FindMsb,
  Pack4x8snorm,
  Pack4x8unorm,
  Pack2x16snorm,
  Pack2x16unorm,
  Pack2x16float,
  Unpack4x8snorm,
  Unpack4x8unorm,
  Unpack2x16snorm,
  Unpack2x16unorm,
  Unpack2x16float,
};

/// Built-in shader function for testing relation between values.
enum class RelationalFunction {
  All,
  Any,
  IsNan,
  IsInf,
  IsFinite,
  IsNormal,
};

/// The sampling qualifiers of a binding or struct field.
enum class Sampling {
  /// Interpolate the value at the center of the pixel.
  Center,
  /// Interpolate the value at a point that lies within all samples covered by
  /// the fragment within the current primitive. In multisampling, use a
  /// single value for all samples in the primitive.
  Centroid,
  /// Interpolate the value at each sample location. In multisampling, invoke
  /// the fragment shader once per sample.
  Sample,
};

/// Primitive type for a scalar.
enum class ScalarKind {
  /// Signed integer type.
  Sint,
  /// Unsigned integer type.
  Uint,
  /// Floating point type.
  Float,
  /// Boolean type.
  Bool,
};

/// Stage of the programmable pipeline.
enum class ShaderStage {
  Vertex,
  Fragment,
  Compute,
};

/// Flags describing an image.
enum class StorageAccess {
  /// Storage can be used as a source for load ops.
  LOAD = 1,
  /// Storage can be used as a target for store ops.
  STORE = 2,
};

/// Image storage format.
enum class StorageFormat {
  R8Unorm,
  R8Snorm,
  R8Uint,
  R8Sint,
  R16Uint,
  R16Sint,
  R16Float,
  Rg8Unorm,
  Rg8Snorm,
  Rg8Uint,
  Rg8Sint,
  R32Uint,
  R32Sint,
  R32Float,
  Rg16Uint,
  Rg16Sint,
  Rg16Float,
  Rgba8Unorm,
  Rgba8Snorm,
  Rgba8Uint,
  Rgba8Sint,
  Rgb10a2Unorm,
  Rg11b10Float,
  Rg32Uint,
  Rg32Sint,
  Rg32Float,
  Rgba16Uint,
  Rgba16Sint,
  Rgba16Float,
  Rgba32Uint,
  Rgba32Sint,
  Rgba32Float,
  R16Unorm,
  R16Snorm,
  Rg16Unorm,
  Rg16Snorm,
  Rgba16Unorm,
  Rgba16Snorm,
};

/// Component selection for a vector swizzle.
enum class SwizzleComponent {
  ///
  X = 0,
  ///
  Y = 1,
  ///
  Z = 2,
  ///
  W = 3,
};

/// Operation that can be applied on a single value.
enum class UnaryOperator {
  Negate,
  Not,
};

/// Number of components in a vector.
enum class VectorSize {
  /// 2D vector
  Bi = 2,
  /// 3D vector
  Tri = 3,
  /// 4D vector
  Quad = 4,
};

struct NagaWriter;

struct Function {
  const char *name;
};

struct NagaFunctionWriter {
  NagaWriter *outer;
  Function fun;
};

template<typename T>
struct Handle {
  uint32_t index;
};

/// Number of bytes per scalar.
using Bytes = uint8_t;

/// A literal scalar value, used in constants.
struct ScalarValue {
  enum class Tag {
    Sint,
    Uint,
    Float,
    Bool,
  };

  struct Sint_Body {
    int64_t _0;
  };

  struct Uint_Body {
    uint64_t _0;
  };

  struct Float_Body {
    double _0;
  };

  struct Bool_Body {
    bool _0;
  };

  Tag tag;
  union {
    Sint_Body sint;
    Uint_Body uint;
    Float_Body float_;
    Bool_Body bool_;
  };
};

/// Addressing space of variables.
struct AddressSpace {
  enum class Tag {
    /// Function locals.
    Function,
    /// Private data, per invocation, mutable.
    Private,
    /// Workgroup shared data, mutable.
    WorkGroup,
    /// Uniform buffer data.
    Uniform,
    /// Storage buffer data, potentially mutable.
    Storage,
    /// Opaque handles, such as samplers and images.
    Handle,
    /// Push constants.
    PushConstant,
  };

  struct Storage_Body {
    StorageAccess access;
  };

  Tag tag;
  union {
    Storage_Body storage;
  };
};

/// Size of an array.
struct ArraySize {
  enum class Tag {
    /// The array size is constant.
    Constant,
    /// The array size can change at runtime.
    Dynamic,
  };

  struct Constant_Body {
    Handle<Constant> _0;
  };

  Tag tag;
  union {
    Constant_Body constant;
  };
};

/// Built-in inputs and outputs.
struct BuiltIn {
  enum class Tag {
    Position,
    ViewIndex,
    BaseInstance,
    BaseVertex,
    ClipDistance,
    CullDistance,
    InstanceIndex,
    PointSize,
    VertexIndex,
    FragDepth,
    PointCoord,
    FrontFacing,
    PrimitiveIndex,
    SampleIndex,
    SampleMask,
    GlobalInvocationId,
    LocalInvocationId,
    LocalInvocationIndex,
    WorkGroupId,
    WorkGroupSize,
    NumWorkGroups,
  };

  struct Position_Body {
    bool invariant;
  };

  Tag tag;
  union {
    Position_Body position;
  };
};

/// Describes how an input/output variable is to be bound.
struct Binding {
  enum class Tag {
    /// Built-in shader variable.
    BuiltIn,
    /// Indexed location.
    ///
    /// Values passed from the [`Vertex`] stage to the [`Fragment`] stage must
    /// have their `interpolation` defaulted (i.e. not `None`) by the front end
    /// as appropriate for that language.
    ///
    /// For other stages, we permit interpolations even though they're ignored.
    /// When a front end is parsing a struct type, it usually doesn't know what
    /// stages will be using it for IO, so it's easiest if it can apply the
    /// defaults to anything with a `Location` binding, just in case.
    ///
    /// For anything other than floating-point scalars and vectors, the
    /// interpolation must be `Flat`.
    ///
    /// [`Vertex`]: crate::ShaderStage::Vertex
    /// [`Fragment`]: crate::ShaderStage::Fragment
    Location,
  };

  struct BuiltIn_Body {
    BuiltIn _0;
  };

  struct Location_Body {
    uint32_t location;
    Interpolation interpolation;
    bool has_interpolation;
    Sampling sampling;
    bool has_sampling;
  };

  Tag tag;
  union {
    BuiltIn_Body built_in;
    Location_Body location;
  };
};

/// Member of a user-defined structure.
struct StructMember {
  const char *name;
  /// Type of the field.
  Handle<Type> ty;
  /// For I/O structs, defines the binding.
  Binding binding;
  bool has_binding;
  /// Offset from the beginning from the struct.
  uint32_t offset;
};

/// Sub-class of the image type.
struct ImageClass {
  enum class Tag {
    /// Regular sampled image.
    Sampled,
    /// Depth comparison image.
    Depth,
    /// Storage image.
    Storage,
  };

  struct Sampled_Body {
    /// Kind of values to sample.
    ScalarKind kind;
    /// Multi-sampled image.
    ///
    /// A multi-sampled image holds several samples per texel. Multi-sampled
    /// images cannot have mipmaps.
    bool multi;
  };

  struct Depth_Body {
    /// Multi-sampled depth image.
    bool multi;
  };

  struct Storage_Body {
    StorageFormat format;
    StorageAccess access;
  };

  Tag tag;
  union {
    Sampled_Body sampled;
    Depth_Body depth;
    Storage_Body storage;
  };
};

/// Enum with additional information, depending on the kind of type.
struct TypeInner {
  enum class Tag {
    /// Number of integral or floating-point kind.
    Scalar,
    /// Vector of numbers.
    Vector,
    /// Matrix of floats.
    Matrix,
    /// Atomic scalar.
    Atomic,
    /// Pointer to another type.
    ///
    /// Pointers to scalars and vectors should be treated as equivalent to
    /// [`ValuePointer`] types. Use the [`TypeInner::equivalent`] method to
    /// compare types in a way that treats pointers correctly.
    ///
    /// ## Pointers to non-`SIZED` types
    ///
    /// The `base` type of a pointer may be a non-[`SIZED`] type like a
    /// dynamically-sized [`Array`], or a [`Struct`] whose last member is a
    /// dynamically sized array. Such pointers occur as the types of
    /// [`GlobalVariable`] or [`AccessIndex`] expressions referring to
    /// dynamically-sized arrays.
    ///
    /// However, among pointers to non-`SIZED` types, only pointers to `Struct`s
    /// are [`DATA`]. Pointers to dynamically sized `Array`s cannot be passed as
    /// arguments, stored in variables, or held in arrays or structures. Their
    /// only use is as the types of `AccessIndex` expressions.
    ///
    /// [`SIZED`]: valid::TypeFlags::SIZED
    /// [`DATA`]: valid::TypeFlags::DATA
    /// [`Array`]: TypeInner::Array
    /// [`Struct`]: TypeInner::Struct
    /// [`ValuePointer`]: TypeInner::ValuePointer
    /// [`GlobalVariable`]: Expression::GlobalVariable
    /// [`AccessIndex`]: Expression::AccessIndex
    Pointer,
    /// Pointer to a scalar or vector.
    ///
    /// A `ValuePointer` type is equivalent to a `Pointer` whose `base` is a
    /// `Scalar` or `Vector` type. This is for use in [`TypeResolution::Value`]
    /// variants; see the documentation for [`TypeResolution`] for details.
    ///
    /// Use the [`TypeInner::equivalent`] method to compare types that could be
    /// pointers, to ensure that `Pointer` and `ValuePointer` types are
    /// recognized as equivalent.
    ///
    /// [`TypeResolution`]: proc::TypeResolution
    /// [`TypeResolution::Value`]: proc::TypeResolution::Value
    ValuePointer,
    /// Homogenous list of elements.
    ///
    /// The `base` type must be a [`SIZED`], [`DATA`] type.
    ///
    /// ## Dynamically sized arrays
    ///
    /// An `Array` is [`SIZED`] unless its `size` is [`Dynamic`].
    /// Dynamically-sized arrays may only appear in a few situations:
    ///
    /// -   They may appear as the type of a [`GlobalVariable`], or as the last
    ///     member of a [`Struct`].
    ///
    /// -   They may appear as the base type of a [`Pointer`]. An
    ///     [`AccessIndex`] expression referring to a struct's final
    ///     unsized array member would have such a pointer type. However, such
    ///     pointer types may only appear as the types of such intermediate
    ///     expressions. They are not [`DATA`], and cannot be stored in
    ///     variables, held in arrays or structs, or passed as parameters.
    ///
    /// [`SIZED`]: crate::valid::TypeFlags::SIZED
    /// [`DATA`]: crate::valid::TypeFlags::DATA
    /// [`Dynamic`]: ArraySize::Dynamic
    /// [`Struct`]: TypeInner::Struct
    /// [`Pointer`]: TypeInner::Pointer
    /// [`AccessIndex`]: Expression::AccessIndex
    Array,
    /// User-defined structure.
    ///
    /// There must always be at least one member.
    ///
    /// A `Struct` type is [`DATA`], and the types of its members must be
    /// `DATA` as well.
    ///
    /// Member types must be [`SIZED`], except for the final member of a
    /// struct, which may be a dynamically sized [`Array`]. The
    /// `Struct` type itself is `SIZED` when all its members are `SIZED`.
    ///
    /// [`DATA`]: crate::valid::TypeFlags::DATA
    /// [`SIZED`]: crate::valid::TypeFlags::SIZED
    /// [`Array`]: TypeInner::Array
    Struct,
    /// Possibly multidimensional array of texels.
    Image,
    /// Can be used to sample values from images.
    Sampler,
    /// Opaque object representing an acceleration structure of geometry.
    AccelerationStructure,
    /// Locally used handle for ray queries.
    RayQuery,
    /// Array of bindings.
    ///
    /// A `BindingArray` represents an array where each element draws its value
    /// from a separate bound resource. The array's element type `base` may be
    /// [`Image`], [`Sampler`], or any type that would be permitted for a global
    /// in the [`Uniform`] or [`Storage`] address spaces. Only global variables
    /// may be binding arrays; on the host side, their values are provided by
    /// [`TextureViewArray`], [`SamplerArray`], or [`BufferArray`]
    /// bindings.
    ///
    /// Since each element comes from a distinct resource, a binding array of
    /// images could have images of varying sizes (but not varying dimensions;
    /// they must all have the same `Image` type). Or, a binding array of
    /// buffers could have elements that are dynamically sized arrays, each with
    /// a different length.
    ///
    /// Binding arrays are in the same address spaces as their underlying type.
    /// As such, referring to an array of images produces an [`Image`] value
    /// directly (as opposed to a pointer). The only operation permitted on
    /// `BindingArray` values is indexing, which works transparently: indexing
    /// a binding array of samplers yields a [`Sampler`], indexing a pointer to the
    /// binding array of storage buffers produces a pointer to the storage struct.
    ///
    /// Unlike textures and samplers, binding arrays are not [`ARGUMENT`], so
    /// they cannot be passed as arguments to functions.
    ///
    /// Naga's WGSL front end supports binding arrays with the type syntax
    /// `binding_array<T, N>`.
    ///
    /// [`Image`]: TypeInner::Image
    /// [`Sampler`]: TypeInner::Sampler
    /// [`Uniform`]: AddressSpace::Uniform
    /// [`Storage`]: AddressSpace::Storage
    /// [`TextureViewArray`]: https://docs.rs/wgpu/latest/wgpu/enum.BindingResource.html#variant.TextureViewArray
    /// [`SamplerArray`]: https://docs.rs/wgpu/latest/wgpu/enum.BindingResource.html#variant.SamplerArray
    /// [`BufferArray`]: https://docs.rs/wgpu/latest/wgpu/enum.BindingResource.html#variant.BufferArray
    /// [`DATA`]: crate::valid::TypeFlags::DATA
    /// [`ARGUMENT`]: crate::valid::TypeFlags::ARGUMENT
    /// [naga#1864]: https://github.com/gfx-rs/naga/issues/1864
    BindingArray,
  };

  struct Scalar_Body {
    ScalarKind kind;
    Bytes width;
  };

  struct Vector_Body {
    VectorSize size;
    ScalarKind kind;
    Bytes width;
  };

  struct Matrix_Body {
    VectorSize columns;
    VectorSize rows;
    Bytes width;
  };

  struct Atomic_Body {
    ScalarKind kind;
    Bytes width;
  };

  struct Pointer_Body {
    Handle<Type> base;
    AddressSpace space;
  };

  struct ValuePointer_Body {
    VectorSize size;
    bool has_size;
    ScalarKind kind;
    Bytes width;
    AddressSpace space;
  };

  struct Array_Body {
    Handle<Type> base;
    ArraySize size;
    uint32_t stride;
  };

  struct Struct_Body {
    const StructMember *members;
    uint32_t members_len;
    uint32_t span;
  };

  struct Image_Body {
    ImageDimension dim;
    bool arrayed;
    ImageClass class_;
  };

  struct Sampler_Body {
    bool comparison;
  };

  struct BindingArray_Body {
    Handle<Type> base;
    ArraySize size;
  };

  Tag tag;
  union {
    Scalar_Body scalar;
    Vector_Body vector;
    Matrix_Body matrix;
    Atomic_Body atomic;
    Pointer_Body pointer;
    ValuePointer_Body value_pointer;
    Array_Body array;
    Struct_Body struct_;
    Image_Body image;
    Sampler_Body sampler;
    BindingArray_Body binding_array;
  };
};

/// A data type declared in the module.
struct Type {
  /// The name of the type, if any.
  const char *name;
  /// Inner structure that depends on the kind of the type.
  TypeInner inner;
};

/// Additional information, dependent on the kind of constant.
struct ConstantInner {
  enum class Tag {
    Scalar,
    Composite,
  };

  struct Scalar_Body {
    Bytes width;
    ScalarValue value;
  };

  struct Composite_Body {
    Handle<Type> ty;
    const Handle<Constant> *components;
    uint32_t components_len;
  };

  Tag tag;
  union {
    Scalar_Body scalar;
    Composite_Body composite;
  };
};

/// Constant value.
struct Constant {
  const char *name;
  uint32_t specialization;
  bool has_specialization;
  ConstantInner inner;
};

/// Pipeline binding information for global resources.
struct ResourceBinding {
  /// The bind group index.
  uint32_t group;
  /// Binding number within the group.
  uint32_t binding;
};

/// Variable defined at module level.
struct GlobalVariable {
  /// Name of the variable, if any.
  const char *name;
  /// How this variable is to be stored.
  AddressSpace space;
  /// For resources, defines the binding point.
  ResourceBinding binding;
  bool has_binding;
  /// The type of this variable.
  Handle<Type> ty;
  /// Initial value for this variable.
  Handle<Constant> init;
  bool has_init;
};

/// Variable defined at function level.
struct LocalVariable {
  /// Name of the variable, if any.
  const char *name;
  /// The type of this variable.
  Handle<Type> ty;
  /// Initial value for this variable.
  Handle<Constant> init;
  bool has_init;
};

/// Sampling modifier to control the level of detail.
struct SampleLevel {
  enum class Tag {
    Auto,
    Zero,
    Exact,
    Bias,
    Gradient,
  };

  struct Exact_Body {
    Handle<Expression> _0;
  };

  struct Bias_Body {
    Handle<Expression> _0;
  };

  struct Gradient_Body {
    Handle<Expression> x;
    Handle<Expression> y;
  };

  Tag tag;
  union {
    Exact_Body exact;
    Bias_Body bias;
    Gradient_Body gradient;
  };
};

/// Type of an image query.
struct ImageQuery {
  enum class Tag {
    /// Get the size at the specified level.
    Size,
    /// Get the number of mipmap levels.
    NumLevels,
    /// Get the number of array layers.
    NumLayers,
    /// Get the number of samples.
    NumSamples,
  };

  struct Size_Body {
    /// If `None`, the base level is considered.
    Handle<Expression> level;
    bool has_level;
  };

  Tag tag;
  union {
    Size_Body size;
  };
};

/// An expression that can be evaluated to obtain a value.
///
/// This is a Single Static Assignment (SSA) scheme similar to SPIR-V.
struct Expression {
  enum class Tag {
    /// Array access with a computed index.
    ///
    /// ## Typing rules
    ///
    /// The `base` operand must be some composite type: [`Vector`], [`Matrix`],
    /// [`Array`], a [`Pointer`] to one of those, or a [`ValuePointer`] with a
    /// `size`.
    ///
    /// The `index` operand must be an integer, signed or unsigned.
    ///
    /// Indexing a [`Vector`] or [`Array`] produces a value of its element type.
    /// Indexing a [`Matrix`] produces a [`Vector`].
    ///
    /// Indexing a [`Pointer`] to any of the above produces a pointer to the
    /// element/component type, in the same [`space`]. In the case of [`Array`],
    /// the result is an actual [`Pointer`], but for vectors and matrices, there
    /// may not be any type in the arena representing the component's type, so
    /// those produce [`ValuePointer`] types equivalent to the appropriate
    /// [`Pointer`].
    ///
    /// ## Dynamic indexing restrictions
    ///
    /// To accommodate restrictions in some of the shader languages that Naga
    /// targets, it is not permitted to subscript a matrix or array with a
    /// dynamically computed index unless that matrix or array appears behind a
    /// pointer. In other words, if the inner type of `base` is [`Array`] or
    /// [`Matrix`], then `index` must be a constant. But if the type of `base`
    /// is a [`Pointer`] to an array or matrix or a [`ValuePointer`] with a
    /// `size`, then the index may be any expression of integer type.
    ///
    /// You can use the [`Expression::is_dynamic_index`] method to determine
    /// whether a given index expression requires matrix or array base operands
    /// to be behind a pointer.
    ///
    /// (It would be simpler to always require the use of `AccessIndex` when
    /// subscripting arrays and matrices that are not behind pointers, but to
    /// accommodate existing front ends, Naga also permits `Access`, with a
    /// restricted `index`.)
    ///
    /// [`Vector`]: TypeInner::Vector
    /// [`Matrix`]: TypeInner::Matrix
    /// [`Array`]: TypeInner::Array
    /// [`Pointer`]: TypeInner::Pointer
    /// [`space`]: TypeInner::Pointer::space
    /// [`ValuePointer`]: TypeInner::ValuePointer
    /// [`Float`]: ScalarKind::Float
    Access,
    /// Access the same types as [`Access`], plus [`Struct`] with a known index.
    ///
    /// [`Access`]: Expression::Access
    /// [`Struct`]: TypeInner::Struct
    AccessIndex,
    /// Constant value.
    Constant,
    /// Splat scalar into a vector.
    Splat,
    /// Vector swizzle.
    Swizzle,
    /// Composite expression.
    Compose,
    /// Reference a function parameter, by its index.
    ///
    /// A `FunctionArgument` expression evaluates to a pointer to the argument's
    /// value. You must use a [`Load`] expression to retrieve its value, or a
    /// [`Store`] statement to assign it a new value.
    ///
    /// [`Load`]: Expression::Load
    /// [`Store`]: Statement::Store
    FunctionArgument,
    /// Reference a global variable.
    ///
    /// If the given `GlobalVariable`'s [`space`] is [`AddressSpace::Handle`],
    /// then the variable stores some opaque type like a sampler or an image,
    /// and a `GlobalVariable` expression referring to it produces the
    /// variable's value directly.
    ///
    /// For any other address space, a `GlobalVariable` expression produces a
    /// pointer to the variable's value. You must use a [`Load`] expression to
    /// retrieve its value, or a [`Store`] statement to assign it a new value.
    ///
    /// [`space`]: GlobalVariable::space
    /// [`Load`]: Expression::Load
    /// [`Store`]: Statement::Store
    GlobalVariable,
    /// Reference a local variable.
    ///
    /// A `LocalVariable` expression evaluates to a pointer to the variable's value.
    /// You must use a [`Load`](Expression::Load) expression to retrieve its value,
    /// or a [`Store`](Statement::Store) statement to assign it a new value.
    LocalVariable,
    /// Load a value indirectly.
    ///
    /// For [`TypeInner::Atomic`] the result is a corresponding scalar.
    /// For other types behind the `pointer<T>`, the result is `T`.
    Load,
    /// Sample a point from a sampled or a depth image.
    ImageSample,
    /// Load a texel from an image.
    ///
    /// For most images, this returns a four-element vector of the same
    /// [`ScalarKind`] as the image. If the format of the image does not have
    /// four components, default values are provided: the first three components
    /// (typically R, G, and B) default to zero, and the final component
    /// (typically alpha) defaults to one.
    ///
    /// However, if the image's [`class`] is [`Depth`], then this returns a
    /// [`Float`] scalar value.
    ///
    /// [`ScalarKind`]: ScalarKind
    /// [`class`]: TypeInner::Image::class
    /// [`Depth`]: ImageClass::Depth
    /// [`Float`]: ScalarKind::Float
    ImageLoad,
    /// Query information from an image.
    ImageQuery,
    /// Apply an unary operator.
    Unary,
    /// Apply a binary operator.
    Binary,
    /// Select between two values based on a condition.
    ///
    /// Note that, because expressions have no side effects, it is unobservable
    /// whether the non-selected branch is evaluated.
    Select,
    /// Compute the derivative on an axis.
    Derivative,
    /// Call a relational function.
    Relational,
    /// Call a math function
    Math,
    /// Cast a simple type to another kind.
    As,
    /// Result of calling another function.
    CallResult,
    /// Result of an atomic operation.
    AtomicResult,
    /// Get the length of an array.
    /// The expression must resolve to a pointer to an array with a dynamic size.
    ///
    /// This doesn't match the semantics of spirv's `OpArrayLength`, which must be passed
    /// a pointer to a structure containing a runtime array in its' last field.
    ArrayLength,
    /// Result of a [`Proceed`] [`RayQuery`] statement.
    ///
    /// [`Proceed`]: RayQueryFunction::Proceed
    /// [`RayQuery`]: Statement::RayQuery
    RayQueryProceedResult,
    /// Return an intersection found by `query`.
    ///
    /// If `committed` is true, return the committed result available when
    RayQueryGetIntersection,
  };

  struct Access_Body {
    Handle<Expression> base;
    Handle<Expression> index;
  };

  struct AccessIndex_Body {
    Handle<Expression> base;
    uint32_t index;
  };

  struct Constant_Body {
    Handle<Constant> _0;
  };

  struct Splat_Body {
    VectorSize size;
    Handle<Expression> value;
  };

  struct Swizzle_Body {
    VectorSize size;
    Handle<Expression> vector;
    SwizzleComponent pattern[4];
  };

  struct Compose_Body {
    Handle<Type> ty;
    const Handle<Expression> *components;
    uint32_t components_len;
  };

  struct FunctionArgument_Body {
    uint32_t _0;
  };

  struct GlobalVariable_Body {
    Handle<GlobalVariable> _0;
  };

  struct LocalVariable_Body {
    Handle<LocalVariable> _0;
  };

  struct Load_Body {
    Handle<Expression> pointer;
  };

  struct ImageSample_Body {
    Handle<Expression> image;
    Handle<Expression> sampler;
    /// If Some(), this operation is a gather operation
    /// on the selected component.
    SwizzleComponent gather;
    bool has_gather;
    Handle<Expression> coordinate;
    Handle<Expression> array_index;
    bool has_array_index;
    Handle<Constant> offset;
    bool has_offset;
    SampleLevel level;
    Handle<Expression> depth_ref;
    bool has_depth_ref;
  };

  struct ImageLoad_Body {
    /// The image to load a texel from. This must have type [`Image`]. (This
    /// will necessarily be a [`GlobalVariable`] or [`FunctionArgument`]
    /// expression, since no other expressions are allowed to have that
    /// type.)
    ///
    /// [`Image`]: TypeInner::Image
    /// [`GlobalVariable`]: Expression::GlobalVariable
    /// [`FunctionArgument`]: Expression::FunctionArgument
    Handle<Expression> image;
    /// The coordinate of the texel we wish to load. This must be a scalar
    /// for [`D1`] images, a [`Bi`] vector for [`D2`] images, and a [`Tri`]
    /// vector for [`D3`] images. (Array indices, sample indices, and
    /// explicit level-of-detail values are supplied separately.) Its
    /// component type must be [`Sint`].
    ///
    /// [`D1`]: ImageDimension::D1
    /// [`D2`]: ImageDimension::D2
    /// [`D3`]: ImageDimension::D3
    /// [`Bi`]: VectorSize::Bi
    /// [`Tri`]: VectorSize::Tri
    /// [`Sint`]: ScalarKind::Sint
    Handle<Expression> coordinate;
    /// The index into an arrayed image. If the [`arrayed`] flag in
    /// `image`'s type is `true`, then this must be `Some(expr)`, where
    /// `expr` is a [`Sint`] scalar. Otherwise, it must be `None`.
    ///
    /// [`arrayed`]: TypeInner::Image::arrayed
    /// [`Sint`]: ScalarKind::Sint
    Handle<Expression> array_index;
    bool has_array_index;
    /// A sample index, for multisampled [`Sampled`] and [`Depth`] images.
    ///
    /// [`Sampled`]: ImageClass::Sampled
    /// [`Depth`]: ImageClass::Depth
    Handle<Expression> sample;
    bool has_sample;
    /// A level of detail, for mipmapped images.
    ///
    /// This must be present when accessing non-multisampled
    /// [`Sampled`] and [`Depth`] images, even if only the
    /// full-resolution level is present (in which case the only
    /// valid level is zero).
    ///
    /// [`Sampled`]: ImageClass::Sampled
    /// [`Depth`]: ImageClass::Depth
    Handle<Expression> level;
    bool has_level;
  };

  struct ImageQuery_Body {
    Handle<Expression> image;
    ImageQuery query;
  };

  struct Unary_Body {
    UnaryOperator op;
    Handle<Expression> expr;
  };

  struct Binary_Body {
    BinaryOperator op;
    Handle<Expression> left;
    Handle<Expression> right;
  };

  struct Select_Body {
    /// Boolean expression
    Handle<Expression> condition;
    Handle<Expression> accept;
    Handle<Expression> reject;
  };

  struct Derivative_Body {
    DerivativeAxis axis;
    DerivativeControl ctrl;
    Handle<Expression> expr;
  };

  struct Relational_Body {
    RelationalFunction fun;
    Handle<Expression> argument;
  };

  struct Math_Body {
    MathFunction fun;
    Handle<Expression> arg;
    Handle<Expression> arg1;
    bool has_arg1;
    Handle<Expression> arg2;
    bool has_arg2;
    Handle<Expression> arg3;
    bool has_arg3;
  };

  struct As_Body {
    /// Source expression, which can only be a scalar or a vector.
    Handle<Expression> expr;
    /// Target scalar kind.
    ScalarKind kind;
    /// If provided, converts to the specified byte width.
    /// Otherwise, bitcast.
    Bytes convert;
    bool has_convert;
  };

  struct CallResult_Body {
    Handle<Function> _0;
  };

  struct AtomicResult_Body {
    Handle<Type> ty;
    bool comparison;
  };

  struct ArrayLength_Body {
    Handle<Expression> _0;
  };

  struct RayQueryGetIntersection_Body {
    Handle<Expression> query;
    bool committed;
  };

  Tag tag;
  union {
    Access_Body access;
    AccessIndex_Body access_index;
    Constant_Body constant;
    Splat_Body splat;
    Swizzle_Body swizzle;
    Compose_Body compose;
    FunctionArgument_Body function_argument;
    GlobalVariable_Body global_variable;
    LocalVariable_Body local_variable;
    Load_Body load;
    ImageSample_Body image_sample;
    ImageLoad_Body image_load;
    ImageQuery_Body image_query;
    Unary_Body unary;
    Binary_Body binary;
    Select_Body select;
    Derivative_Body derivative;
    Relational_Body relational;
    Math_Body math;
    As_Body as;
    CallResult_Body call_result;
    AtomicResult_Body atomic_result;
    ArrayLength_Body array_length;
    RayQueryGetIntersection_Body ray_query_get_intersection;
  };
};

/// A function argument.
struct FunctionArgument {
  /// Name of the argument, if any.
  const char *name;
  /// Type of the argument.
  Handle<Type> ty;
  /// For entry points, an argument has to have a binding
  /// unless it's a structure.
  Binding binding;
  bool has_binding;
};

/// A function result.
struct FunctionResult {
  /// Type of the result.
  Handle<Type> ty;
  /// For entry points, the result has to have a binding
  /// unless it's a structure.
  Binding binding;
  bool has_binding;
};

using FunctionWriteCallback = void(*)(NagaFunctionWriter *ctx, void *user_data);

struct EarlyDepthTest {
  ConservativeDepth conservative;
  bool has_conservative;
};

struct EntryPoint {
  /// Name of this entry point, visible externally.
  ///
  /// Entry point names for a given `stage` must be distinct within a module.
  const char *name;
  /// Shader stage.
  ShaderStage stage;
  /// Early depth test for fragment stages.
  EarlyDepthTest early_depth_test;
  bool has_early_depth_test;
  /// Workgroup size for compute stages
  uint32_t workgroup_size[3];
};

template<typename T>
struct Range {
  uint32_t start;
  uint32_t end;
};

/// The value of the switch case.
struct SwitchValue {
  enum class Tag {
    I32,
    U32,
    Default,
  };

  struct I32_Body {
    int32_t _0;
  };

  struct U32_Body {
    uint32_t _0;
  };

  Tag tag;
  union {
    I32_Body i32;
    U32_Body u32;
  };
};

/// A case for a switch statement.
struct SwitchCase {
  /// Value, upon which the case is considered true.
  SwitchValue value;
  /// Body of the case.
  Block body;
  /// If true, the control flow continues to the next case in the list,
  /// or default.
  bool fall_through;
};

/// Memory barrier flags.
struct Barrier {
  uint32_t bits;

  constexpr explicit operator bool() const {
    return !!bits;
  }
  constexpr Barrier operator~() const {
    return Barrier { static_cast<decltype(bits)>(~bits) };
  }
  constexpr Barrier operator|(const Barrier& other) const {
    return Barrier { static_cast<decltype(bits)>(this->bits | other.bits) };
  }
  Barrier& operator|=(const Barrier& other) {
    *this = (*this | other);
    return *this;
  }
  constexpr Barrier operator&(const Barrier& other) const {
    return Barrier { static_cast<decltype(bits)>(this->bits & other.bits) };
  }
  Barrier& operator&=(const Barrier& other) {
    *this = (*this & other);
    return *this;
  }
  constexpr Barrier operator^(const Barrier& other) const {
    return Barrier { static_cast<decltype(bits)>(this->bits ^ other.bits) };
  }
  Barrier& operator^=(const Barrier& other) {
    *this = (*this ^ other);
    return *this;
  }
};
/// Barrier affects all `AddressSpace::Storage` accesses.
constexpr static const Barrier Barrier_STORAGE = Barrier{ /* .bits = */ (uint32_t)1 };
/// Barrier affects all `AddressSpace::WorkGroup` accesses.
constexpr static const Barrier Barrier_WORK_GROUP = Barrier{ /* .bits = */ (uint32_t)2 };

/// Function on an atomic value.
///
/// Note: these do not include load/store, which use the existing
/// [`Expression::Load`] and [`Statement::Store`].
struct AtomicFunction {
  enum class Tag {
    Add,
    Subtract,
    And,
    ExclusiveOr,
    InclusiveOr,
    Min,
    Max,
    Exchange,
  };

  struct Exchange_Body {
    Handle<Expression> compare;
    bool has_compare;
  };

  Tag tag;
  union {
    Exchange_Body exchange;
  };
};

/// An operation that a [`RayQuery` statement] applies to its [`query`] operand.
///
/// [`RayQuery` statement]: Statement::RayQuery
/// [`query`]: Statement::RayQuery::query
struct RayQueryFunction {
  enum class Tag {
    /// Initialize the `RayQuery` object.
    Initialize,
    /// Start or continue the query given by the statement's [`query`] operand.
    ///
    /// After executing this statement, the `result` expression is a
    /// [`Bool`] scalar indicating whether there are more intersection
    /// candidates to consider.
    ///
    /// [`query`]: Statement::RayQuery::query
    /// [`Bool`]: ScalarKind::Bool
    Proceed,
    Terminate,
  };

  struct Initialize_Body {
    /// The acceleration structure within which this query should search for hits.
    ///
    /// The expression must be an [`AccelerationStructure`].
    ///
    /// [`AccelerationStructure`]: TypeInner::AccelerationStructure
    Handle<Expression> acceleration_structure;
    /// A struct of detailed parameters for the ray query.
    ///
    /// This expression should have the struct type given in
    /// [`SpecialTypes::ray_desc`]. This is available in the WGSL
    /// front end as the `RayDesc` type.
    Handle<Expression> descriptor;
  };

  struct Proceed_Body {
    Handle<Expression> result;
  };

  Tag tag;
  union {
    Initialize_Body initialize;
    Proceed_Body proceed;
  };
};

/// Instructions which make up an executable block.
struct Statement {
  enum class Tag {
    /// Emit a range of expressions, visible to all statements that follow in this block.
    ///
    /// See the [module-level documentation][emit] for details.
    ///
    /// [emit]: index.html#expression-evaluation-time
    Emit,
    /// A block containing more statements, to be executed sequentially.
    Block,
    /// Conditionally executes one of two blocks, based on the value of the condition.
    If,
    /// Conditionally executes one of multiple blocks, based on the value of the selector.
    ///
    /// Each case must have a distinct [`value`], exactly one of which must be
    /// [`Default`]. The `Default` may appear at any position, and covers all
    /// values not explicitly appearing in other cases. A `Default` appearing in
    /// the midst of the list of cases does not shadow the cases that follow.
    ///
    /// Some backend languages don't support fallthrough (HLSL due to FXC,
    /// WGSL), and may translate fallthrough cases in the IR by duplicating
    /// code. However, all backend languages do support cases selected by
    /// multiple values, like `case 1: case 2: case 3: { ... }`. This is
    /// represented in the IR as a series of fallthrough cases with empty
    /// bodies, except for the last.
    ///
    /// [`value`]: SwitchCase::value
    /// [`body`]: SwitchCase::body
    /// [`Default`]: SwitchValue::Default
    Switch,
    /// Executes a block repeatedly.
    ///
    /// Each iteration of the loop executes the `body` block, followed by the
    /// `continuing` block.
    ///
    /// Executing a [`Break`], [`Return`] or [`Kill`] statement exits the loop.
    ///
    /// A [`Continue`] statement in `body` jumps to the `continuing` block. The
    /// `continuing` block is meant to be used to represent structures like the
    /// third expression of a C-style `for` loop head, to which `continue`
    /// statements in the loop's body jump.
    ///
    /// The `continuing` block and its substatements must not contain `Return`
    /// or `Kill` statements, or any `Break` or `Continue` statements targeting
    /// this loop. (It may have `Break` and `Continue` statements targeting
    /// loops or switches nested within the `continuing` block.)
    ///
    /// If present, `break_if` is an expression which is evaluated after the
    /// continuing block. If its value is true, control continues after the
    /// `Loop` statement, rather than branching back to the top of body as
    /// usual. The `break_if` expression corresponds to a "break if" statement
    /// in WGSL, or a loop whose back edge is an `OpBranchConditional`
    /// instruction in SPIR-V.
    ///
    /// [`Break`]: Statement::Break
    /// [`Continue`]: Statement::Continue
    /// [`Kill`]: Statement::Kill
    /// [`Return`]: Statement::Return
    /// [`break if`]: Self::Loop::break_if
    Loop,
    /// Exits the innermost enclosing [`Loop`] or [`Switch`].
    ///
    /// A `Break` statement may only appear within a [`Loop`] or [`Switch`]
    /// statement. It may not break out of a [`Loop`] from within the loop's
    /// `continuing` block.
    ///
    /// [`Loop`]: Statement::Loop
    /// [`Switch`]: Statement::Switch
    Break,
    /// Skips to the `continuing` block of the innermost enclosing [`Loop`].
    ///
    /// A `Continue` statement may only appear within the `body` block of the
    /// innermost enclosing [`Loop`] statement. It must not appear within that
    /// loop's `continuing` block.
    ///
    /// [`Loop`]: Statement::Loop
    Continue,
    /// Returns from the function (possibly with a value).
    ///
    /// `Return` statements are forbidden within the `continuing` block of a
    /// [`Loop`] statement.
    ///
    /// [`Loop`]: Statement::Loop
    Return,
    /// Aborts the current shader execution.
    ///
    /// `Kill` statements are forbidden within the `continuing` block of a
    /// [`Loop`] statement.
    ///
    /// [`Loop`]: Statement::Loop
    Kill,
    /// Synchronize invocations within the work group.
    /// The `Barrier` flags control which memory accesses should be synchronized.
    /// If empty, this becomes purely an execution barrier.
    Barrier,
    /// Stores a value at an address.
    ///
    /// For [`TypeInner::Atomic`] type behind the pointer, the value
    /// has to be a corresponding scalar.
    /// For other types behind the `pointer<T>`, the value is `T`.
    ///
    /// This statement is a barrier for any operations on the
    /// `Expression::LocalVariable` or `Expression::GlobalVariable`
    /// that is the destination of an access chain, started
    /// from the `pointer`.
    Store,
    /// Stores a texel value to an image.
    ///
    /// The `image`, `coordinate`, and `array_index` fields have the same
    /// meanings as the corresponding operands of an [`ImageLoad`] expression;
    /// see that documentation for details. Storing into multisampled images or
    /// images with mipmaps is not supported, so there are no `level` or
    /// `sample` operands.
    ///
    /// This statement is a barrier for any operations on the corresponding
    /// [`Expression::GlobalVariable`] for this image.
    ///
    /// [`ImageLoad`]: Expression::ImageLoad
    ImageStore,
    /// Atomic function.
    Atomic,
    /// Calls a function.
    ///
    /// If the `result` is `Some`, the corresponding expression has to be
    /// `Expression::CallResult`, and this statement serves as a barrier for any
    /// operations on that expression.
    Call,
    RayQuery,
  };

  struct Emit_Body {
    Range<Expression> _0;
  };

  struct Block_Body {
    Block _0;
  };

  struct If_Body {
    Handle<Expression> condition;
    Block accept;
    Block reject;
  };

  struct Switch_Body {
    Handle<Expression> selector;
    const SwitchCase *cases;
    uint32_t cases_len;
  };

  struct Loop_Body {
    Block body;
    Block continuing;
    Handle<Expression> break_if;
    bool has_break_if;
  };

  struct Return_Body {
    Handle<Expression> value;
    bool has_value;
  };

  struct Barrier_Body {
    Barrier _0;
  };

  struct Store_Body {
    Handle<Expression> pointer;
    Handle<Expression> value;
  };

  struct ImageStore_Body {
    Handle<Expression> image;
    Handle<Expression> coordinate;
    Handle<Expression> array_index;
    bool has_array_index;
    Handle<Expression> value;
  };

  struct Atomic_Body {
    /// Pointer to an atomic value.
    Handle<Expression> pointer;
    /// Function to run on the atomic.
    AtomicFunction fun;
    /// Value to use in the function.
    Handle<Expression> value;
    /// [`AtomicResult`] expression representing this function's result.
    ///
    /// [`AtomicResult`]: crate::Expression::AtomicResult
    Handle<Expression> result;
  };

  struct Call_Body {
    Handle<Function> function;
    const Handle<Expression> *arguments;
    uint32_t arguments_len;
    Handle<Expression> result;
    bool has_result;
  };

  struct RayQuery_Body {
    /// The [`RayQuery`] object this statement operates on.
    ///
    /// [`RayQuery`]: TypeInner::RayQuery
    Handle<Expression> query;
    /// The specific operation we're performing on `query`.
    RayQueryFunction fun;
  };

  Tag tag;
  union {
    Emit_Body emit;
    Block_Body block;
    If_Body if_;
    Switch_Body switch_;
    Loop_Body loop;
    Return_Body return_;
    Barrier_Body barrier;
    Store_Body store;
    ImageStore_Body image_store;
    Atomic_Body atomic;
    Call_Body call;
    RayQuery_Body ray_query;
  };
};

extern "C" {

NagaWriter *nagaNew();

void nagaFunctionSetBody(NagaFunctionWriter *ctx_, Block stmt);

Handle<Expression> nagaFunctionStoreExpression(NagaFunctionWriter *writer_,
                                               Expression expr,
                                               const char *name_);

void nagaFunctionAddArgument(NagaFunctionWriter *writer_, const FunctionArgument *fn_arg_);

void nagaFunctionSetResult(NagaFunctionWriter *writer_, const FunctionResult *res_);

Handle<Constant> nagaStoreConstant(NagaWriter *writer_, Constant c);

Handle<GlobalVariable> nagaStoreGlobal(NagaWriter *writer_, GlobalVariable gv);

Handle<Type> nagaStoreType(NagaWriter *writer_, Type t);

NagaWriter *nagaFunctionGetWriter(NagaFunctionWriter *writer_);

Handle<Function> nagaAddFunction(NagaWriter *writer_,
                                 const Function *desc_,
                                 FunctionWriteCallback cb,
                                 void *user_data);

void nagaAddEntryPoint(NagaWriter *writer_,
                       const EntryPoint *desc_,
                       FunctionWriteCallback cb,
                       void *user_data);

bool nagaValidate(NagaWriter *writer);

const char *nagaIntoWgsl(NagaWriter *writer);

} // extern "C"

} // namespace naga

#endif // NAGA_NATIVE_BINDINGS_HPP
