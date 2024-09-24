use std::ffi::{c_char, c_void, CStr, CString};

// Regex helpers
// Fixup attributes (manually fix enum flags):
//  \s*#\[derive.*\n(\s*#\[cfg_attr.*\n)+
//  \n#[repr(C)]\n
//
// Fixup vectors:
//  ([a-z_]+): Vec<(.*)>,
//  $1: *const $2,\n$1_len: u32,
//
// Fixup options:
//  (\w+): Option<(\w+)>,
//  $1: $2,\n has_$1: bool,
pub mod native {
  use std::{
    ffi::{c_char, CStr},
    marker::PhantomData,
    ptr::slice_from_raw_parts,
  };

  #[repr(C)]
  #[derive(Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub struct Handle<T> {
    pub index: u32,
    pub marker: PhantomData<T>,
  }

  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub struct Range<T> {
    start: u32,
    end: u32,
    marker: PhantomData<T>,
  }

  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub struct Block {
    body: *const Statement,
    body_len: u32,
  }

  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub struct EarlyDepthTest {
    conservative: ConservativeDepth,
    has_conservative: bool,
  }

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
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum ConservativeDepth {
    /// Shader may rewrite depth only with a value greater than calculated.
    GreaterEqual,

    /// Shader may rewrite depth smaller than one that would have been written without the modification.
    LessEqual,

    /// Shader may not rewrite depth value.
    Unchanged,
  }

  /// Stage of the programmable pipeline.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum ShaderStage {
    Vertex,
    Fragment,
    Compute,
  }

  /// Addressing space of variables.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum AddressSpace {
    /// Function locals.
    Function,
    /// Private data, per invocation, mutable.
    Private,
    /// Workgroup shared data, mutable.
    WorkGroup,
    /// Uniform buffer data.
    Uniform,
    /// Storage buffer data, potentially mutable.
    Storage { access: StorageAccess },
    /// Opaque handles, such as samplers and images.
    Handle,
    /// Push constants.
    PushConstant,
  }

  /// Built-in inputs and outputs.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum BuiltIn {
    Position { invariant: bool },
    ViewIndex,
    // vertex
    BaseInstance,
    BaseVertex,
    ClipDistance,
    CullDistance,
    InstanceIndex,
    PointSize,
    VertexIndex,
    // fragment
    FragDepth,
    PointCoord,
    FrontFacing,
    PrimitiveIndex,
    SampleIndex,
    SampleMask,
    // compute
    GlobalInvocationId,
    LocalInvocationId,
    LocalInvocationIndex,
    WorkGroupId,
    WorkGroupSize,
    NumWorkGroups,
  }

  /// Number of bytes per scalar.
  pub type Bytes = u8;

  /// Number of components in a vector.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum VectorSize {
    /// 2D vector
    Bi = 2,
    /// 3D vector
    Tri = 3,
    /// 4D vector
    Quad = 4,
  }

  /// Primitive type for a scalar.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum ScalarKind {
    /// Signed integer type.
    Sint,
    /// Unsigned integer type.
    Uint,
    /// Floating point type.
    Float,
    /// Boolean type.
    Bool,
  }

  /// Size of an array.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub enum ArraySize {
    /// The array size is constant.
    Constant(std::num::NonZeroU32),
    /// The array size can change at runtime.
    Dynamic,
  }

  /// The interpolation qualifier of a binding or struct field.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum Interpolation {
    /// The value will be interpolated in a perspective-correct fashion.
    /// Also known as "smooth" in glsl.
    Perspective,
    /// Indicates that linear, non-perspective, correct
    /// interpolation must be used.
    /// Also known as "no_perspective" in glsl.
    Linear,
    /// Indicates that no interpolation will be performed.
    Flat,
  }

  /// The sampling qualifiers of a binding or struct field.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum Sampling {
    /// Interpolate the value at the center of the pixel.
    Center,

    /// Interpolate the value at a point that lies within all samples covered by
    /// the fragment within the current primitive. In multisampling, use a
    /// single value for all samples in the primitive.
    Centroid,

    /// Interpolate the value at each sample location. In multisampling, invoke
    /// the fragment shader once per sample.
    Sample,
  }

  /// Member of a user-defined structure.
  // Clone is used only for error reporting and is not intended for end users
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub struct StructMember {
    pub name: *const c_char,
    /// Type of the field.
    pub ty: Handle<Type>,
    /// For I/O structs, defines the binding.
    pub binding: Binding,
    pub has_binding: bool,
    /// Offset from the beginning from the struct.
    pub offset: u32,
  }

  /// The number of dimensions an image has.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum ImageDimension {
    /// 1D image
    D1,
    /// 2D image
    D2,
    /// 3D image
    D3,
    /// Cube map
    Cube,
  }

  /// Flags describing an image.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum StorageAccess {
    /// Storage can be used as a source for load ops.
    LOAD = 0x1,
    /// Storage can be used as a target for store ops.
    STORE = 0x2,
  }

  /// Image storage format.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum StorageFormat {
    // 8-bit formats
    R8Unorm,
    R8Snorm,
    R8Uint,
    R8Sint,

    // 16-bit formats
    R16Uint,
    R16Sint,
    R16Float,
    Rg8Unorm,
    Rg8Snorm,
    Rg8Uint,
    Rg8Sint,

    // 32-bit formats
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

    // Packed 32-bit formats
    Rgb10a2Unorm,
    Rg11b10Float,

    // 64-bit formats
    Rg32Uint,
    Rg32Sint,
    Rg32Float,
    Rgba16Uint,
    Rgba16Sint,
    Rgba16Float,

    // 128-bit formats
    Rgba32Uint,
    Rgba32Sint,
    Rgba32Float,

    // Normalized 16-bit per channel formats
    R16Unorm,
    R16Snorm,
    Rg16Unorm,
    Rg16Snorm,
    Rgba16Unorm,
    Rgba16Snorm,
  }

  /// Sub-class of the image type.
  #[repr(C)]
  #[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum ImageClass {
    /// Regular sampled image.
    Sampled {
      /// Kind of values to sample.
      kind: ScalarKind,
      /// Multi-sampled image.
      ///
      /// A multi-sampled image holds several samples per texel. Multi-sampled
      /// images cannot have mipmaps.
      multi: bool,
    },
    /// Depth comparison image.
    Depth {
      /// Multi-sampled depth image.
      multi: bool,
    },
    /// Storage image.
    Storage {
      format: StorageFormat,
      access: StorageAccess,
    },
  }

  /// A data type declared in the module.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub struct Type {
    /// The name of the type, if any.
    pub name: *const c_char,
    /// Inner structure that depends on the kind of the type.
    pub inner: TypeInner,
  }

  /// Characteristics of a scalar type.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub struct Scalar {
    /// How the value's bits are to be interpreted.
    pub kind: ScalarKind,

    /// This size of the value in bytes.
    pub width: Bytes,
  }

  /// Enum with additional information, depending on the kind of type.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub enum TypeInner {
    /// Number of integral or floating-point kind.
    Scalar(Scalar),
    /// Vector of numbers.
    Vector { size: VectorSize, scalar: Scalar },
    /// Matrix of numbers.
    Matrix {
      columns: VectorSize,
      rows: VectorSize,
      scalar: Scalar,
    },
    /// Atomic scalar.
    Atomic(Scalar),
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
    Pointer {
      base: Handle<Type>,
      space: AddressSpace,
    },

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
    ValuePointer {
      size: VectorSize,
      has_size: bool,
      scalar: Scalar,
      space: AddressSpace,
    },

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
    Array {
      base: Handle<Type>,
      size: ArraySize,
      stride: u32,
    },

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
    Struct {
      members: *const StructMember,
      members_len: u32,
      //TODO: should this be unaligned?
      span: u32,
    },
    /// Possibly multidimensional array of texels.
    Image {
      dim: ImageDimension,
      arrayed: bool,
      //TODO: consider moving `multisampled: bool` out
      class: ImageClass,
    },
    /// Can be used to sample values from images.
    Sampler { comparison: bool },

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
    BindingArray { base: Handle<Type>, size: ArraySize },
  }

  /// Constant value.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub struct Constant {
    pub name: *const c_char,
    pub ty: Handle<Type>,
    pub init: Handle<Expression>,
  }

  /// Describes how an input/output variable is to be bound.
  #[repr(C)]
  #[derive(Clone, Copy, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum Binding {
    /// Built-in shader variable.
    BuiltIn(BuiltIn),

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
    Location {
      location: u32,
      /// Indicates the 2nd input to the blender when dual-source blending.
      second_blend_source: bool,
      interpolation: Interpolation,
      has_interpolation: bool,
      sampling: Sampling,
      has_sampling: bool,
    },
  }

  /// Pipeline binding information for global resources.
  #[repr(C)]
  #[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub struct ResourceBinding {
    /// The bind group index.
    pub group: u32,
    /// Binding number within the group.
    pub binding: u32,
  }

  /// Variable defined at module level.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub struct GlobalVariable {
    /// Name of the variable, if any.
    pub name: *const c_char,
    /// How this variable is to be stored.
    pub space: AddressSpace,
    /// For resources, defines the binding point.
    pub binding: ResourceBinding,
    has_binding: bool,
    /// The type of this variable.
    pub ty: Handle<Type>,
    /// Initial value for this variable.
    pub init: Handle<Constant>,
    has_init: bool,
  }

  /// Variable defined at function level.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub struct LocalVariable {
    /// Name of the variable, if any.
    pub name: *const c_char,
    /// The type of this variable.
    pub ty: Handle<Type>,
    /// Initial value for this variable.
    pub init: Handle<Constant>,
    has_init: bool,
  }

  /// Operation that can be applied on a single value.
  #[repr(C)]
  #[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum UnaryOperator {
    Negate,
    LogicalNot,
    BitwiseNot,
  }

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
  #[repr(C)]
  #[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum BinaryOperator {
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
  }

  /// Function on an atomic value.
  ///
  /// Note: these do not include load/store, which use the existing
  /// [`Expression::Load`] and [`Statement::Store`].
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub enum AtomicFunction {
    Add,
    Subtract,
    And,
    ExclusiveOr,
    InclusiveOr,
    Min,
    Max,
    Exchange {
      compare: Handle<Expression>,
      has_compare: bool,
    },
  }

  /// Hint at which precision to compute a derivative.
  #[repr(C)]
  #[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum DerivativeControl {
    Coarse,
    Fine,
    None,
  }

  /// Axis on which to compute a derivative.
  #[repr(C)]
  #[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum DerivativeAxis {
    X,
    Y,
    Width,
  }

  /// Built-in shader function for testing relation between values.
  #[repr(C)]
  #[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum RelationalFunction {
    All,
    Any,
    IsNan,
    IsInf,
  }

  /// Built-in shader function for math.
  #[repr(C)]
  #[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum MathFunction {
    // comparison
    Abs,
    Min,
    Max,
    Clamp,
    Saturate,
    // trigonometry
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
    // decomposition
    Ceil,
    Floor,
    Round,
    Fract,
    Trunc,
    Modf,
    Frexp,
    Ldexp,
    // exponent
    Exp,
    Exp2,
    Log,
    Log2,
    Pow,
    // geometry
    Dot,
    Outer,
    Cross,
    Distance,
    Length,
    Normalize,
    FaceForward,
    Reflect,
    Refract,
    // computational
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
    // bits
    CountTrailingZeros,
    CountLeadingZeros,
    CountOneBits,
    ReverseBits,
    ExtractBits,
    InsertBits,
    FindLsb,
    FindMsb,
    // data packing
    Pack4x8snorm,
    Pack4x8unorm,
    Pack2x16snorm,
    Pack2x16unorm,
    Pack2x16float,
    // data unpacking
    Unpack4x8snorm,
    Unpack4x8unorm,
    Unpack2x16snorm,
    Unpack2x16unorm,
    Unpack2x16float,
  }

  /// Sampling modifier to control the level of detail.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub enum SampleLevel {
    Auto,
    Zero,
    Exact(Handle<Expression>),
    Bias(Handle<Expression>),
    Gradient {
      x: Handle<Expression>,
      y: Handle<Expression>,
    },
  }

  /// Type of an image query.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub enum ImageQuery {
    /// Get the size at the specified level.
    Size {
      /// If `None`, the base level is considered.
      level: Handle<Expression>,
      has_level: bool,
    },
    /// Get the number of mipmap levels.
    NumLevels,
    /// Get the number of array layers.
    NumLayers,
    /// Get the number of samples.
    NumSamples,
  }

  /// Component selection for a vector swizzle.
  #[repr(C)]
  #[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
  pub enum SwizzleComponent {
    ///
    X = 0,
    ///
    Y = 1,
    ///
    Z = 2,
    ///
    W = 3,
  }

  bitflags::bitflags! {
      /// Memory barrier flags.
      #[repr(C)]
      #[derive(Clone, Debug, Hash, Eq, Ord, PartialEq, PartialOrd)]
      pub struct Barrier: u32 {
          /// Barrier affects all `AddressSpace::Storage` accesses.
          const STORAGE = 0x1;
          /// Barrier affects all `AddressSpace::WorkGroup` accesses.
          const WORK_GROUP = 0x2;
      }
  }

  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub enum Literal {
    /// May not be NaN or infinity.
    F64(f64),
    /// May not be NaN or infinity.
    F32(f32),
    U32(u32),
    I32(i32),
    I64(i64),
    Bool(bool),
    AbstractInt(i64),
    AbstractFloat(f64),
  }

  /// An expression that can be evaluated to obtain a value.
  ///
  /// This is a Single Static Assignment (SSA) scheme similar to SPIR-V.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub enum Expression {
    /// Literal.
    Literal(Literal),
    /// Constant value.
    Constant(Handle<Constant>),
    /// Zero value of a type.
    ZeroValue(Handle<Type>),
    /// Composite expression.
    Compose {
      ty: Handle<Type>,
      components: *const Handle<Expression>,
      components_len: u32,
    },

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
    Access {
      base: Handle<Expression>,
      index: Handle<Expression>,
    },
    /// Access the same types as [`Access`], plus [`Struct`] with a known index.
    ///
    /// [`Access`]: Expression::Access
    /// [`Struct`]: TypeInner::Struct
    AccessIndex {
      base: Handle<Expression>,
      index: u32,
    },
    /// Splat scalar into a vector.
    Splat {
      size: VectorSize,
      value: Handle<Expression>,
    },
    /// Vector swizzle.
    Swizzle {
      size: VectorSize,
      vector: Handle<Expression>,
      pattern: [SwizzleComponent; 4],
    },

    /// Reference a function parameter, by its index.
    ///
    /// A `FunctionArgument` expression evaluates to a pointer to the argument's
    /// value. You must use a [`Load`] expression to retrieve its value, or a
    /// [`Store`] statement to assign it a new value.
    ///
    /// [`Load`]: Expression::Load
    /// [`Store`]: Statement::Store
    FunctionArgument(u32),

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
    GlobalVariable(Handle<GlobalVariable>),

    /// Reference a local variable.
    ///
    /// A `LocalVariable` expression evaluates to a pointer to the variable's value.
    /// You must use a [`Load`](Expression::Load) expression to retrieve its value,
    /// or a [`Store`](Statement::Store) statement to assign it a new value.
    LocalVariable(Handle<LocalVariable>),

    /// Load a value indirectly.
    ///
    /// For [`TypeInner::Atomic`] the result is a corresponding scalar.
    /// For other types behind the `pointer<T>`, the result is `T`.
    Load { pointer: Handle<Expression> },
    /// Sample a point from a sampled or a depth image.
    ImageSample {
      image: Handle<Expression>,
      sampler: Handle<Expression>,
      /// If Some(), this operation is a gather operation
      /// on the selected component.
      gather: SwizzleComponent,
      has_gather: bool,
      coordinate: Handle<Expression>,
      array_index: Handle<Expression>,
      has_array_index: bool,
      offset: Handle<Constant>,
      has_offset: bool,
      level: SampleLevel,
      depth_ref: Handle<Expression>,
      has_depth_ref: bool,
    },

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
    ImageLoad {
      /// The image to load a texel from. This must have type [`Image`]. (This
      /// will necessarily be a [`GlobalVariable`] or [`FunctionArgument`]
      /// expression, since no other expressions are allowed to have that
      /// type.)
      ///
      /// [`Image`]: TypeInner::Image
      /// [`GlobalVariable`]: Expression::GlobalVariable
      /// [`FunctionArgument`]: Expression::FunctionArgument
      image: Handle<Expression>,

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
      coordinate: Handle<Expression>,

      /// The index into an arrayed image. If the [`arrayed`] flag in
      /// `image`'s type is `true`, then this must be `Some(expr)`, where
      /// `expr` is a [`Sint`] scalar. Otherwise, it must be `None`.
      ///
      /// [`arrayed`]: TypeInner::Image::arrayed
      /// [`Sint`]: ScalarKind::Sint
      array_index: Handle<Expression>,
      has_array_index: bool,

      /// A sample index, for multisampled [`Sampled`] and [`Depth`] images.
      ///
      /// [`Sampled`]: ImageClass::Sampled
      /// [`Depth`]: ImageClass::Depth
      sample: Handle<Expression>,
      has_sample: bool,

      /// A level of detail, for mipmapped images.
      ///
      /// This must be present when accessing non-multisampled
      /// [`Sampled`] and [`Depth`] images, even if only the
      /// full-resolution level is present (in which case the only
      /// valid level is zero).
      ///
      /// [`Sampled`]: ImageClass::Sampled
      /// [`Depth`]: ImageClass::Depth
      level: Handle<Expression>,
      has_level: bool,
    },

    /// Query information from an image.
    ImageQuery {
      image: Handle<Expression>,
      query: ImageQuery,
    },
    /// Apply an unary operator.
    Unary {
      op: UnaryOperator,
      expr: Handle<Expression>,
    },
    /// Apply a binary operator.
    Binary {
      op: BinaryOperator,
      left: Handle<Expression>,
      right: Handle<Expression>,
    },
    /// Select between two values based on a condition.
    ///
    /// Note that, because expressions have no side effects, it is unobservable
    /// whether the non-selected branch is evaluated.
    Select {
      /// Boolean expression
      condition: Handle<Expression>,
      accept: Handle<Expression>,
      reject: Handle<Expression>,
    },
    /// Compute the derivative on an axis.
    Derivative {
      axis: DerivativeAxis,
      ctrl: DerivativeControl,
      //modifier,
      expr: Handle<Expression>,
    },
    /// Call a relational function.
    Relational {
      fun: RelationalFunction,
      argument: Handle<Expression>,
    },
    /// Call a math function
    Math {
      fun: MathFunction,
      arg: Handle<Expression>,
      arg1: Handle<Expression>,
      has_arg1: bool,
      arg2: Handle<Expression>,
      has_arg2: bool,
      arg3: Handle<Expression>,
      has_arg3: bool,
    },
    /// Cast a simple type to another kind.
    As {
      /// Source expression, which can only be a scalar or a vector.
      expr: Handle<Expression>,
      /// Target scalar kind.
      kind: ScalarKind,
      /// If provided, converts to the specified byte width.
      /// Otherwise, bitcast.
      convert: Bytes,
      has_convert: bool,
    },
    /// Result of calling another function.
    CallResult(Handle<Function>),
    /// Result of an atomic operation.
    AtomicResult { ty: Handle<Type>, comparison: bool },
    /// Get the length of an array.
    /// The expression must resolve to a pointer to an array with a dynamic size.
    ///
    /// This doesn't match the semantics of spirv's `OpArrayLength`, which must be passed
    /// a pointer to a structure containing a runtime array in its' last field.
    ArrayLength(Handle<Expression>),

    /// Result of a [`Proceed`] [`RayQuery`] statement.
    ///
    /// [`Proceed`]: RayQueryFunction::Proceed
    /// [`RayQuery`]: Statement::RayQuery
    RayQueryProceedResult,

    /// Return an intersection found by `query`.
    ///
    /// If `committed` is true, return the committed result available when
    RayQueryGetIntersection {
      query: Handle<Expression>,
      committed: bool,
    },
  }

  /// The value of the switch case.
  // Clone is used only for error reporting and is not intended for end users
  #[repr(C)]
  #[derive(Clone)]
  pub enum SwitchValue {
    I32(i32),
    U32(u32),
    Default,
  }

  /// A case for a switch statement.
  // Clone is used only for error reporting and is not intended for end users
  #[repr(C)]
  #[derive(Clone)]
  pub struct SwitchCase {
    /// Value, upon which the case is considered true.
    pub value: SwitchValue,
    /// Body of the case.
    pub body: Block,
    /// If true, the control flow continues to the next case in the list,
    /// or default.
    pub fall_through: bool,
  }

  /// An operation that a [`RayQuery` statement] applies to its [`query`] operand.
  ///
  /// [`RayQuery` statement]: Statement::RayQuery
  /// [`query`]: Statement::RayQuery::query
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub enum RayQueryFunction {
    /// Initialize the `RayQuery` object.
    Initialize {
      /// The acceleration structure within which this query should search for hits.
      ///
      /// The expression must be an [`AccelerationStructure`].
      ///
      /// [`AccelerationStructure`]: TypeInner::AccelerationStructure
      acceleration_structure: Handle<Expression>,

      #[allow(rustdoc::private_intra_doc_links)]
      /// A struct of detailed parameters for the ray query.
      ///
      /// This expression should have the struct type given in
      /// [`SpecialTypes::ray_desc`]. This is available in the WGSL
      /// front end as the `RayDesc` type.
      descriptor: Handle<Expression>,
    },

    /// Start or continue the query given by the statement's [`query`] operand.
    ///
    /// After executing this statement, the `result` expression is a
    /// [`Bool`] scalar indicating whether there are more intersection
    /// candidates to consider.
    ///
    /// [`query`]: Statement::RayQuery::query
    /// [`Bool`]: ScalarKind::Bool
    Proceed {
      result: Handle<Expression>,
    },

    Terminate,
  }

  //TODO: consider removing `Clone`. It's not valid to clone `Statement::Emit` anyway.
  /// Instructions which make up an executable block.
  // Clone is used only for error reporting and is not intended for end users
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub enum Statement {
    /// Emit a range of expressions, visible to all statements that follow in this block.
    ///
    /// See the [module-level documentation][emit] for details.
    ///
    /// [emit]: index.html#expression-evaluation-time
    Emit(Range<Expression>),
    /// A block containing more statements, to be executed sequentially.
    Block(Block),
    /// Conditionally executes one of two blocks, based on the value of the condition.
    If {
      condition: Handle<Expression>, //bool
      accept: Block,
      reject: Block,
    },
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
    Switch {
      selector: Handle<Expression>, //int
      cases: *const SwitchCase,
      cases_len: u32,
    },

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
    Loop {
      body: Block,
      continuing: Block,
      break_if: Handle<Expression>,
      has_break_if: bool,
    },

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
    Return {
      value: Handle<Expression>,
      has_value: bool,
    },

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
    Barrier(Barrier),
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
    Store {
      pointer: Handle<Expression>,
      value: Handle<Expression>,
    },
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
    ImageStore {
      image: Handle<Expression>,
      coordinate: Handle<Expression>,
      array_index: Handle<Expression>,
      has_array_index: bool,
      value: Handle<Expression>,
    },
    /// Atomic function.
    Atomic {
      /// Pointer to an atomic value.
      pointer: Handle<Expression>,
      /// Function to run on the atomic.
      fun: AtomicFunction,
      /// Value to use in the function.
      value: Handle<Expression>,
      /// [`AtomicResult`] expression representing this function's result.
      ///
      /// [`AtomicResult`]: crate::Expression::AtomicResult
      result: Handle<Expression>,
      has_result: bool,
    },
    /// Load uniformly from a uniform pointer in the workgroup address space.
    ///
    /// Corresponds to the [`workgroupUniformLoad`](https://www.w3.org/TR/WGSL/#workgroupUniformLoad-builtin)
    /// built-in function of wgsl, and has the same barrier semantics
    WorkGroupUniformLoad {
      /// This must be of type [`Pointer`] in the [`WorkGroup`] address space
      ///
      /// [`Pointer`]: TypeInner::Pointer
      /// [`WorkGroup`]: AddressSpace::WorkGroup
      pointer: Handle<Expression>,
      /// The [`WorkGroupUniformLoadResult`] expression representing this load's result.
      ///
      /// [`WorkGroupUniformLoadResult`]: Expression::WorkGroupUniformLoadResult
      result: Handle<Expression>,
    },
    /// Calls a function.
    ///
    /// If the `result` is `Some`, the corresponding expression has to be
    /// `Expression::CallResult`, and this statement serves as a barrier for any
    /// operations on that expression.
    Call {
      function: Handle<Function>,
      arguments: *const Handle<Expression>,
      arguments_len: u32,
      result: Handle<Expression>,
      has_result: bool,
    },
    RayQuery {
      /// The [`RayQuery`] object this statement operates on.
      ///
      /// [`RayQuery`]: TypeInner::RayQuery
      query: Handle<Expression>,

      /// The specific operation we're performing on `query`.
      fun: RayQueryFunction,
    },
  }

  /// A function argument.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub struct FunctionArgument {
    /// Name of the argument, if any.
    pub name: *const c_char,
    /// Type of the argument.
    pub ty: Handle<Type>,
    /// For entry points, an argument has to have a binding
    /// unless it's a structure.
    pub binding: Binding,
    pub has_binding: bool,
  }

  /// A function result.
  #[repr(C)]
  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub struct FunctionResult {
    /// Type of the result.
    pub ty: Handle<Type>,
    /// For entry points, the result has to have a binding
    /// unless it's a structure.
    pub binding: Binding,
    pub has_binding: bool,
  }

  #[derive(Clone, Debug, PartialEq, PartialOrd)]
  pub enum ConversionError {
    Unknown,
  }

  #[repr(C)]
  #[derive(Debug)]
  pub struct EntryPoint {
    /// Name of this entry point, visible externally.
    ///
    /// Entry point names for a given `stage` must be distinct within a module.
    pub name: *const c_char,
    /// Shader stage.
    pub stage: ShaderStage,
    /// Early depth test for fragment stages.
    pub early_depth_test: EarlyDepthTest,
    pub has_early_depth_test: bool,
    /// Workgroup size for compute stages
    pub workgroup_size: [u32; 3],
  }

  #[repr(C)]
  #[derive(Debug, PartialEq, PartialOrd)]
  pub struct Function {
    pub name: *const c_char,
  }

  pub fn convert_string(ptr: *const c_char) -> Option<String> {
    if !ptr.is_null() {
      unsafe { Some(CStr::from_ptr(ptr).to_string_lossy().to_string()) }
    } else {
      None
    }
  }

  impl TryFrom<Block> for naga::Block {
    type Error = ConversionError;

    fn try_from(w: Block) -> Result<naga::Block, Self::Error> {
      let stmts = unsafe { &*slice_from_raw_parts(w.body, w.body_len as usize) };
      let mut blk = naga::Block::new();
      for stmt in stmts {
        blk.push(stmt.clone().try_into()?, naga::Span::default());
      }
      Ok(blk)
    }
  }

  impl<T> Clone for Handle<T> {
    fn clone(&self) -> Handle<T> {
      Self {
        index: self.index,
        marker: PhantomData,
      }
    }
  }

  impl<T, U> From<Handle<U>> for naga::Handle<T> {
    fn from(h: Handle<U>) -> naga::Handle<T> {
      naga::Handle::<T>::from_usize(h.index as usize)
    }
  }

  impl<T, U> From<naga::Handle<U>> for Handle<T> {
    fn from(h: naga::Handle<U>) -> Handle<T> {
      Handle {
        index: h.index() as u32,
        marker: PhantomData,
      }
    }
  }

  impl<T, U> From<Range<U>> for naga::Range<T> {
    fn from(h: Range<U>) -> naga::Range<T> {
      let (first, last) = unsafe {
        (
          naga::Handle::new(naga::Index::new(h.start).unwrap()),
          naga::Handle::new(naga::Index::new(h.start).unwrap()),
        )
      };
      naga::Range::<T>::new_from_bounds(first, last)
    }
  }

  impl TryFrom<EarlyDepthTest> for naga::EarlyDepthTest {
    type Error = ConversionError;

    fn try_from(e: EarlyDepthTest) -> Result<naga::EarlyDepthTest, Self::Error> {
      let conservative = if e.has_conservative {
        Some(e.conservative.try_into()?)
      } else {
        None
      };
      Ok(naga::EarlyDepthTest { conservative })
    }
  }

  impl TryFrom<ConservativeDepth> for naga::ConservativeDepth {
    type Error = ConversionError;

    fn try_from(cd: ConservativeDepth) -> Result<naga::ConservativeDepth, Self::Error> {
      let result = match cd {
        ConservativeDepth::GreaterEqual => naga::ConservativeDepth::GreaterEqual,
        ConservativeDepth::LessEqual => naga::ConservativeDepth::LessEqual,
        ConservativeDepth::Unchanged => naga::ConservativeDepth::Unchanged,
      };
      Ok(result)
    }
  }

  impl TryFrom<ShaderStage> for naga::ShaderStage {
    type Error = ConversionError;

    fn try_from(ss: ShaderStage) -> Result<naga::ShaderStage, Self::Error> {
      let result = match ss {
        ShaderStage::Vertex => naga::ShaderStage::Vertex,
        ShaderStage::Fragment => naga::ShaderStage::Fragment,
        ShaderStage::Compute => naga::ShaderStage::Compute,
      };
      Ok(result)
    }
  }

  impl TryFrom<AddressSpace> for naga::AddressSpace {
    type Error = ConversionError;

    fn try_from(aspace: AddressSpace) -> Result<naga::AddressSpace, Self::Error> {
      let result = match aspace {
        AddressSpace::Function => naga::AddressSpace::Function,
        AddressSpace::Private => naga::AddressSpace::Private,
        AddressSpace::WorkGroup => naga::AddressSpace::WorkGroup,
        AddressSpace::Uniform => naga::AddressSpace::Uniform,
        AddressSpace::Storage { access } => naga::AddressSpace::Storage {
          access: access.try_into()?,
        },
        AddressSpace::Handle => naga::AddressSpace::Handle,
        AddressSpace::PushConstant => naga::AddressSpace::PushConstant,
      };
      Ok(result)
    }
  }

  impl TryFrom<BuiltIn> for naga::BuiltIn {
    type Error = ConversionError;

    fn try_from(bi: BuiltIn) -> Result<naga::BuiltIn, Self::Error> {
      let result = match bi {
        BuiltIn::Position { invariant } => naga::BuiltIn::Position { invariant },
        BuiltIn::ViewIndex => naga::BuiltIn::ViewIndex,
        BuiltIn::BaseInstance => naga::BuiltIn::BaseInstance,
        BuiltIn::BaseVertex => naga::BuiltIn::BaseVertex,
        BuiltIn::ClipDistance => naga::BuiltIn::ClipDistance,
        BuiltIn::CullDistance => naga::BuiltIn::CullDistance,
        BuiltIn::InstanceIndex => naga::BuiltIn::InstanceIndex,
        BuiltIn::PointSize => naga::BuiltIn::PointSize,
        BuiltIn::VertexIndex => naga::BuiltIn::VertexIndex,
        BuiltIn::FragDepth => naga::BuiltIn::FragDepth,
        BuiltIn::PointCoord => naga::BuiltIn::PointCoord,
        BuiltIn::FrontFacing => naga::BuiltIn::FrontFacing,
        BuiltIn::PrimitiveIndex => naga::BuiltIn::PrimitiveIndex,
        BuiltIn::SampleIndex => naga::BuiltIn::SampleIndex,
        BuiltIn::SampleMask => naga::BuiltIn::SampleMask,
        BuiltIn::GlobalInvocationId => naga::BuiltIn::GlobalInvocationId,
        BuiltIn::LocalInvocationId => naga::BuiltIn::LocalInvocationId,
        BuiltIn::LocalInvocationIndex => naga::BuiltIn::LocalInvocationIndex,
        BuiltIn::WorkGroupId => naga::BuiltIn::WorkGroupId,
        BuiltIn::WorkGroupSize => naga::BuiltIn::WorkGroupSize,
        BuiltIn::NumWorkGroups => naga::BuiltIn::NumWorkGroups,
      };
      Ok(result)
    }
  }

  impl TryFrom<VectorSize> for naga::VectorSize {
    type Error = ConversionError;

    fn try_from(vs: VectorSize) -> Result<naga::VectorSize, Self::Error> {
      let result = match vs {
        VectorSize::Bi => naga::VectorSize::Bi,
        VectorSize::Tri => naga::VectorSize::Tri,
        VectorSize::Quad => naga::VectorSize::Quad,
      };
      Ok(result)
    }
  }

  impl TryFrom<ScalarKind> for naga::ScalarKind {
    type Error = ConversionError;

    fn try_from(sk: ScalarKind) -> Result<naga::ScalarKind, Self::Error> {
      let result = match sk {
        ScalarKind::Sint => naga::ScalarKind::Sint,
        ScalarKind::Uint => naga::ScalarKind::Uint,
        ScalarKind::Float => naga::ScalarKind::Float,
        ScalarKind::Bool => naga::ScalarKind::Bool,
      };
      Ok(result)
    }
  }

  impl TryFrom<ArraySize> for naga::ArraySize {
    type Error = ConversionError;

    fn try_from(asize: ArraySize) -> Result<naga::ArraySize, Self::Error> {
      let result = match asize {
        ArraySize::Constant(handle) => naga::ArraySize::Constant(handle),
        ArraySize::Dynamic => naga::ArraySize::Dynamic,
      };
      Ok(result)
    }
  }

  impl TryFrom<Interpolation> for naga::Interpolation {
    type Error = ConversionError;

    fn try_from(interp: Interpolation) -> Result<naga::Interpolation, Self::Error> {
      let result = match interp {
        Interpolation::Perspective => naga::Interpolation::Perspective,
        Interpolation::Linear => naga::Interpolation::Linear,
        Interpolation::Flat => naga::Interpolation::Flat,
      };
      Ok(result)
    }
  }

  impl TryFrom<Sampling> for naga::Sampling {
    type Error = ConversionError;

    fn try_from(sampling: Sampling) -> Result<naga::Sampling, Self::Error> {
      let result = match sampling {
        Sampling::Center => naga::Sampling::Center,
        Sampling::Centroid => naga::Sampling::Centroid,
        Sampling::Sample => naga::Sampling::Sample,
      };
      Ok(result)
    }
  }

  impl TryFrom<StructMember> for naga::StructMember {
    type Error = ConversionError;

    fn try_from(sm: StructMember) -> Result<naga::StructMember, Self::Error> {
      let name = if !sm.name.is_null() {
        let name_c_str = unsafe { std::ffi::CStr::from_ptr(sm.name) };
        Some(name_c_str.to_string_lossy().into_owned())
      } else {
        None
      };

      let ty = sm.ty.into();
      let binding = if sm.has_binding {
        Some(sm.binding.try_into()?)
      } else {
        None
      };

      let offset = sm.offset;

      Ok(naga::StructMember {
        name,
        ty,
        binding,
        offset,
      })
    }
  }

  impl TryFrom<ImageDimension> for naga::ImageDimension {
    type Error = ConversionError;

    fn try_from(dim: ImageDimension) -> Result<naga::ImageDimension, Self::Error> {
      let result = match dim {
        ImageDimension::D1 => naga::ImageDimension::D1,
        ImageDimension::D2 => naga::ImageDimension::D2,
        ImageDimension::D3 => naga::ImageDimension::D3,
        ImageDimension::Cube => naga::ImageDimension::Cube,
      };
      Ok(result)
    }
  }

  impl TryFrom<StorageAccess> for naga::StorageAccess {
    type Error = ConversionError;

    fn try_from(access: StorageAccess) -> Result<naga::StorageAccess, Self::Error> {
      let result = match access {
        StorageAccess::LOAD => naga::StorageAccess::LOAD,
        StorageAccess::STORE => naga::StorageAccess::STORE,
      };
      Ok(result)
    }
  }

  impl TryFrom<StorageFormat> for naga::StorageFormat {
    type Error = ConversionError;

    fn try_from(format: StorageFormat) -> Result<naga::StorageFormat, Self::Error> {
      let result = match format {
        StorageFormat::R8Unorm => naga::StorageFormat::R8Unorm,
        StorageFormat::R8Snorm => naga::StorageFormat::R8Snorm,
        StorageFormat::R8Uint => naga::StorageFormat::R8Uint,
        StorageFormat::R8Sint => naga::StorageFormat::R8Sint,
        StorageFormat::R16Uint => naga::StorageFormat::R16Uint,
        StorageFormat::R16Sint => naga::StorageFormat::R16Sint,
        StorageFormat::R16Float => naga::StorageFormat::R16Float,
        StorageFormat::Rg8Unorm => naga::StorageFormat::Rg8Unorm,
        StorageFormat::Rg8Snorm => naga::StorageFormat::Rg8Snorm,
        StorageFormat::Rg8Uint => naga::StorageFormat::Rg8Uint,
        StorageFormat::Rg8Sint => naga::StorageFormat::Rg8Sint,
        StorageFormat::R32Uint => naga::StorageFormat::R32Uint,
        StorageFormat::R32Sint => naga::StorageFormat::R32Sint,
        StorageFormat::R32Float => naga::StorageFormat::R32Float,
        StorageFormat::Rg16Uint => naga::StorageFormat::Rg16Uint,
        StorageFormat::Rg16Sint => naga::StorageFormat::Rg16Sint,
        StorageFormat::Rg16Float => naga::StorageFormat::Rg16Float,
        StorageFormat::Rgba8Unorm => naga::StorageFormat::Rgba8Unorm,
        StorageFormat::Rgba8Snorm => naga::StorageFormat::Rgba8Snorm,
        StorageFormat::Rgba8Uint => naga::StorageFormat::Rgba8Uint,
        StorageFormat::Rgba8Sint => naga::StorageFormat::Rgba8Sint,
        StorageFormat::Rgb10a2Unorm => naga::StorageFormat::Rgb10a2Unorm,
        StorageFormat::Rg11b10Float => naga::StorageFormat::Rg11b10Float,
        StorageFormat::Rg32Uint => naga::StorageFormat::Rg32Uint,
        StorageFormat::Rg32Sint => naga::StorageFormat::Rg32Sint,
        StorageFormat::Rg32Float => naga::StorageFormat::Rg32Float,
        StorageFormat::Rgba16Uint => naga::StorageFormat::Rgba16Uint,
        StorageFormat::Rgba16Sint => naga::StorageFormat::Rgba16Sint,
        StorageFormat::Rgba16Float => naga::StorageFormat::Rgba16Float,
        StorageFormat::Rgba32Uint => naga::StorageFormat::Rgba32Uint,
        StorageFormat::Rgba32Sint => naga::StorageFormat::Rgba32Sint,
        StorageFormat::Rgba32Float => naga::StorageFormat::Rgba32Float,
        StorageFormat::R16Unorm => naga::StorageFormat::R16Unorm,
        StorageFormat::R16Snorm => naga::StorageFormat::R16Snorm,
        StorageFormat::Rg16Unorm => naga::StorageFormat::Rg16Unorm,
        StorageFormat::Rg16Snorm => naga::StorageFormat::Rg16Snorm,
        StorageFormat::Rgba16Unorm => naga::StorageFormat::Rgba16Unorm,
        StorageFormat::Rgba16Snorm => naga::StorageFormat::Rgba16Snorm,
      };
      Ok(result)
    }
  }

  impl TryFrom<ImageClass> for naga::ImageClass {
    type Error = ConversionError;

    fn try_from(ic: ImageClass) -> Result<naga::ImageClass, Self::Error> {
      let result = match ic {
        ImageClass::Sampled { kind, multi } => naga::ImageClass::Sampled {
          kind: kind.try_into()?,
          multi,
        },
        ImageClass::Depth { multi } => naga::ImageClass::Depth { multi },
        ImageClass::Storage { format, access } => naga::ImageClass::Storage {
          format: format.try_into()?,
          access: access.try_into()?,
        },
      };
      Ok(result)
    }
  }

  impl TryFrom<Type> for naga::Type {
    type Error = ConversionError;

    fn try_from(t: Type) -> Result<naga::Type, Self::Error> {
      let inner = t.inner.try_into()?;
      Ok(naga::Type {
        name: convert_string(t.name),
        inner: inner,
      })
    }
  }

  impl TryFrom<Scalar> for naga::Scalar {
    type Error = ConversionError;

    fn try_from(s: Scalar) -> Result<naga::Scalar, Self::Error> {
      let kind = s.kind.try_into()?;
      Ok(naga::Scalar {
        kind,
        width: s.width,
      })
    }
  }

  impl TryFrom<TypeInner> for naga::TypeInner {
    type Error = ConversionError;

    fn try_from(ti: TypeInner) -> Result<naga::TypeInner, Self::Error> {
      let result = match ti {
        TypeInner::Scalar(scalar) => naga::TypeInner::Scalar {
          0: scalar.try_into()?,
        },
        TypeInner::Vector { size, scalar } => naga::TypeInner::Vector {
          size: size.try_into()?,
          scalar: scalar.try_into()?,
        },
        TypeInner::Matrix {
          columns,
          rows,
          scalar,
        } => naga::TypeInner::Matrix {
          columns: columns.try_into()?,
          rows: rows.try_into()?,
          scalar: scalar.try_into()?,
        },
        TypeInner::Atomic(scalar) => naga::TypeInner::Atomic {
          0: scalar.try_into()?,
        },
        TypeInner::Pointer { base, space } => naga::TypeInner::Pointer {
          base: base.into(),
          space: space.try_into()?,
        },
        TypeInner::ValuePointer {
          size,
          has_size,
          scalar,
          space,
        } => naga::TypeInner::ValuePointer {
          size: if has_size {
            Some(size.try_into()?)
          } else {
            None
          },
          scalar: scalar.try_into()?,
          space: space.try_into()?,
        },
        TypeInner::Array { base, size, stride } => naga::TypeInner::Array {
          base: base.into(),
          size: size.try_into()?,
          stride,
        },
        TypeInner::Struct {
          members,
          members_len,
          span,
        } => {
          let members = unsafe { std::slice::from_raw_parts(members, members_len as usize) };
          let mut converted_members = Vec::with_capacity(members_len as usize);
          for member in members {
            converted_members.push(member.clone().try_into()?);
          }
          naga::TypeInner::Struct {
            members: converted_members,
            span,
          }
        }
        TypeInner::Image {
          dim,
          arrayed,
          class,
        } => naga::TypeInner::Image {
          dim: dim.try_into()?,
          arrayed,
          class: class.try_into()?,
        },
        TypeInner::Sampler { comparison } => naga::TypeInner::Sampler { comparison },
        TypeInner::AccelerationStructure => naga::TypeInner::AccelerationStructure,
        TypeInner::RayQuery => naga::TypeInner::RayQuery,
        TypeInner::BindingArray { base, size } => naga::TypeInner::BindingArray {
          base: base.into(),
          size: size.try_into()?,
        },
      };
      Ok(result)
    }
  }

  impl TryFrom<Constant> for naga::Constant {
    type Error = ConversionError;

    fn try_from(c: Constant) -> Result<naga::Constant, Self::Error> {
      Ok(naga::Constant {
        name: convert_string(c.name),
        init: c.init.into(),
        ty: c.ty.into(),
      })
    }
  }

  impl TryFrom<Binding> for naga::Binding {
    type Error = ConversionError;

    fn try_from(b: Binding) -> Result<naga::Binding, Self::Error> {
      let result = match b {
        Binding::BuiltIn(bi) => naga::Binding::BuiltIn(bi.try_into()?),
        Binding::Location {
          location,
          second_blend_source,
          interpolation,
          has_interpolation,
          sampling,
          has_sampling,
        } => naga::Binding::Location {
          location,
          second_blend_source,
          interpolation: if has_interpolation {
            Some(interpolation.try_into()?)
          } else {
            None
          },
          sampling: if has_sampling {
            Some(sampling.try_into()?)
          } else {
            None
          },
        },
      };
      Ok(result)
    }
  }

  impl TryFrom<ResourceBinding> for naga::ResourceBinding {
    type Error = ConversionError;

    fn try_from(rb: ResourceBinding) -> Result<naga::ResourceBinding, Self::Error> {
      Ok(naga::ResourceBinding {
        group: rb.group,
        binding: rb.binding,
      })
    }
  }

  impl TryFrom<GlobalVariable> for naga::GlobalVariable {
    type Error = ConversionError;

    fn try_from(gv: GlobalVariable) -> Result<naga::GlobalVariable, Self::Error> {
      let space = gv.space;
      let binding = if gv.has_binding {
        Some(gv.binding.try_into()?)
      } else {
        None
      };
      let ty = gv.ty.into();
      let init = if gv.has_init {
        Some(gv.init.into())
      } else {
        None
      };
      Ok(naga::GlobalVariable {
        name: convert_string(gv.name),
        space: space.try_into()?,
        binding,
        ty,
        init,
      })
    }
  }

  impl TryFrom<LocalVariable> for naga::LocalVariable {
    type Error = ConversionError;

    fn try_from(lv: LocalVariable) -> Result<naga::LocalVariable, Self::Error> {
      let init = if lv.has_init {
        Some(lv.init.into())
      } else {
        None
      };
      Ok(naga::LocalVariable {
        name: convert_string(lv.name),
        ty: lv.ty.into(),
        init,
      })
    }
  }

  impl TryFrom<UnaryOperator> for naga::UnaryOperator {
    type Error = ConversionError;

    fn try_from(uo: UnaryOperator) -> Result<naga::UnaryOperator, Self::Error> {
      let result = match uo {
        UnaryOperator::Negate => naga::UnaryOperator::Negate,
        UnaryOperator::LogicalNot => naga::UnaryOperator::LogicalNot,
        UnaryOperator::BitwiseNot => naga::UnaryOperator::BitwiseNot,
      };
      Ok(result)
    }
  }

  impl TryFrom<BinaryOperator> for naga::BinaryOperator {
    type Error = ConversionError;

    fn try_from(bo: BinaryOperator) -> Result<naga::BinaryOperator, Self::Error> {
      let result = match bo {
        BinaryOperator::Add => naga::BinaryOperator::Add,
        BinaryOperator::Subtract => naga::BinaryOperator::Subtract,
        BinaryOperator::Multiply => naga::BinaryOperator::Multiply,
        BinaryOperator::Divide => naga::BinaryOperator::Divide,
        BinaryOperator::Modulo => naga::BinaryOperator::Modulo,
        BinaryOperator::Equal => naga::BinaryOperator::Equal,
        BinaryOperator::NotEqual => naga::BinaryOperator::NotEqual,
        BinaryOperator::Less => naga::BinaryOperator::Less,
        BinaryOperator::LessEqual => naga::BinaryOperator::LessEqual,
        BinaryOperator::Greater => naga::BinaryOperator::Greater,
        BinaryOperator::GreaterEqual => naga::BinaryOperator::GreaterEqual,
        BinaryOperator::And => naga::BinaryOperator::And,
        BinaryOperator::ExclusiveOr => naga::BinaryOperator::ExclusiveOr,
        BinaryOperator::InclusiveOr => naga::BinaryOperator::InclusiveOr,
        BinaryOperator::ShiftLeft => naga::BinaryOperator::ShiftLeft,
        BinaryOperator::ShiftRight => naga::BinaryOperator::ShiftRight,
        BinaryOperator::LogicalAnd => naga::BinaryOperator::LogicalAnd,
        BinaryOperator::LogicalOr => naga::BinaryOperator::LogicalOr,
      };
      Ok(result)
    }
  }

  impl TryFrom<FunctionResult> for naga::FunctionResult {
    type Error = ConversionError;

    fn try_from(fr: FunctionResult) -> Result<naga::FunctionResult, Self::Error> {
      let binding = if fr.has_binding {
        Some(fr.binding.try_into()?)
      } else {
        None
      };
      Ok(naga::FunctionResult {
        ty: fr.ty.into(),
        binding,
      })
    }
  }

  impl TryFrom<FunctionArgument> for naga::FunctionArgument {
    type Error = ConversionError;

    fn try_from(fa: FunctionArgument) -> Result<naga::FunctionArgument, Self::Error> {
      let binding = if fa.has_binding {
        Some(fa.binding.try_into()?)
      } else {
        None
      };
      Ok(naga::FunctionArgument {
        name: convert_string(fa.name),
        ty: fa.ty.into(),
        binding,
      })
    }
  }

  impl TryFrom<SwitchCase> for naga::SwitchCase {
    type Error = ConversionError;

    fn try_from(sc: SwitchCase) -> Result<naga::SwitchCase, Self::Error> {
      let value = match sc.value {
        SwitchValue::I32(val) => naga::SwitchValue::I32(val),
        SwitchValue::U32(val) => naga::SwitchValue::U32(val),
        SwitchValue::Default => naga::SwitchValue::Default,
      };

      let body = sc.body.try_into()?;

      Ok(naga::SwitchCase {
        value,
        body,
        fall_through: sc.fall_through,
      })
    }
  }

  impl TryFrom<Barrier> for naga::Barrier {
    type Error = ConversionError;

    fn try_from(barrier: Barrier) -> Result<naga::Barrier, Self::Error> {
      let mut flags = naga::Barrier::empty();
      if barrier.contains(Barrier::STORAGE) {
        flags |= naga::Barrier::STORAGE;
      }
      if barrier.contains(Barrier::WORK_GROUP) {
        flags |= naga::Barrier::WORK_GROUP;
      }
      Ok(flags)
    }
  }

  impl TryFrom<AtomicFunction> for naga::AtomicFunction {
    type Error = ConversionError;

    fn try_from(atomic: AtomicFunction) -> Result<naga::AtomicFunction, Self::Error> {
      let result = match atomic {
        AtomicFunction::Add => naga::AtomicFunction::Add,
        AtomicFunction::Subtract => naga::AtomicFunction::Subtract,
        AtomicFunction::And => naga::AtomicFunction::And,
        AtomicFunction::ExclusiveOr => naga::AtomicFunction::ExclusiveOr,
        AtomicFunction::InclusiveOr => naga::AtomicFunction::InclusiveOr,
        AtomicFunction::Min => naga::AtomicFunction::Min,
        AtomicFunction::Max => naga::AtomicFunction::Max,
        AtomicFunction::Exchange {
          compare,
          has_compare,
        } => naga::AtomicFunction::Exchange {
          compare: if has_compare {
            Some(compare.into())
          } else {
            None
          },
        },
      };
      Ok(result)
    }
  }

  impl TryFrom<RayQueryFunction> for naga::RayQueryFunction {
    type Error = ConversionError;

    fn try_from(ray_query: RayQueryFunction) -> Result<naga::RayQueryFunction, Self::Error> {
      let result = match ray_query {
        RayQueryFunction::Initialize {
          acceleration_structure,
          descriptor,
        } => naga::RayQueryFunction::Initialize {
          acceleration_structure: acceleration_structure.into(),
          descriptor: descriptor.into(),
        },
        RayQueryFunction::Proceed { result } => naga::RayQueryFunction::Proceed {
          result: result.into(),
        },
        RayQueryFunction::Terminate => naga::RayQueryFunction::Terminate,
      };
      Ok(result)
    }
  }

  impl TryFrom<Statement> for naga::Statement {
    type Error = ConversionError;

    fn try_from(s: Statement) -> Result<naga::Statement, Self::Error> {
      let result = match s {
        Statement::Emit(range) => naga::Statement::Emit(range.into()),
        Statement::Block(block) => naga::Statement::Block(block.try_into()?),
        Statement::If {
          condition,
          accept,
          reject,
        } => naga::Statement::If {
          condition: condition.into(),
          accept: accept.try_into()?,
          reject: reject.try_into()?,
        },
        Statement::Switch {
          selector,
          cases,
          cases_len,
        } => {
          let switch_cases = unsafe { std::slice::from_raw_parts(cases, cases_len as usize) };
          let mut cases: Vec<naga::SwitchCase> = Vec::with_capacity(cases_len as usize);
          for case in switch_cases {
            cases.push(case.clone().try_into()?);
          }

          naga::Statement::Switch {
            selector: selector.into(),
            cases: cases,
          }
        }
        Statement::Loop {
          body,
          continuing,
          break_if,
          has_break_if,
        } => naga::Statement::Loop {
          body: body.try_into()?,
          continuing: continuing.try_into()?,
          break_if: if has_break_if {
            Some(break_if.into())
          } else {
            None
          },
        },
        Statement::Break => naga::Statement::Break,
        Statement::Continue => naga::Statement::Continue,
        Statement::Return { value, has_value } => naga::Statement::Return {
          value: if has_value { Some(value.into()) } else { None },
        },
        Statement::Kill => naga::Statement::Kill,
        Statement::Barrier(barrier) => naga::Statement::Barrier(barrier.try_into()?),
        Statement::Store { pointer, value } => naga::Statement::Store {
          pointer: pointer.into(),
          value: value.into(),
        },
        Statement::ImageStore {
          image,
          coordinate,
          array_index,
          has_array_index,
          value,
        } => naga::Statement::ImageStore {
          image: image.into(),
          coordinate: coordinate.into(),
          array_index: if has_array_index {
            Some(array_index.into())
          } else {
            None
          },
          value: value.into(),
        },
        Statement::Atomic {
          pointer,
          fun,
          value,
          result,
          has_result,
        } => naga::Statement::Atomic {
          pointer: pointer.into(),
          fun: fun.try_into()?,
          value: value.into(),
          result: if has_result {
            Some(result.into())
          } else {
            None
          },
        },
        Statement::WorkGroupUniformLoad { pointer, result } => {
          naga::Statement::WorkGroupUniformLoad {
            pointer: pointer.into(),
            result: result.into(),
          }
        }
        Statement::Call {
          function,
          arguments,
          arguments_len,
          result,
          has_result,
        } => {
          let args = unsafe { std::slice::from_raw_parts(arguments, arguments_len as usize) };
          let mut out_args: Vec<naga::Handle<naga::Expression>> =
            Vec::with_capacity(arguments_len as usize);
          for arg in args {
            out_args.push(arg.clone().into());
          }

          naga::Statement::Call {
            function: function.into(),
            arguments: out_args,
            result: if has_result {
              Some(result.into())
            } else {
              None
            },
          }
        }
        Statement::RayQuery { query, fun } => naga::Statement::RayQuery {
          query: query.into(),
          fun: fun.try_into()?,
        },
      };

      Ok(result)
    }
  }

  impl TryFrom<SwizzleComponent> for naga::SwizzleComponent {
    type Error = ConversionError;

    fn try_from(sc: SwizzleComponent) -> Result<naga::SwizzleComponent, Self::Error> {
      match sc {
        SwizzleComponent::X => Ok(naga::SwizzleComponent::X),
        SwizzleComponent::Y => Ok(naga::SwizzleComponent::Y),
        SwizzleComponent::Z => Ok(naga::SwizzleComponent::Z),
        SwizzleComponent::W => Ok(naga::SwizzleComponent::W),
      }
    }
  }

  impl TryFrom<SampleLevel> for naga::SampleLevel {
    type Error = ConversionError;

    fn try_from(sample_level: SampleLevel) -> Result<naga::SampleLevel, Self::Error> {
      let result = match sample_level {
        SampleLevel::Auto => naga::SampleLevel::Auto,
        SampleLevel::Zero => naga::SampleLevel::Zero,
        SampleLevel::Exact(handle) => naga::SampleLevel::Exact(handle.into()),
        SampleLevel::Bias(handle) => naga::SampleLevel::Bias(handle.into()),
        SampleLevel::Gradient { x, y } => naga::SampleLevel::Gradient {
          x: x.into(),
          y: y.into(),
        },
      };
      Ok(result)
    }
  }

  impl TryFrom<ImageQuery> for naga::ImageQuery {
    type Error = ConversionError;

    fn try_from(image_query: ImageQuery) -> Result<naga::ImageQuery, Self::Error> {
      let result = match image_query {
        ImageQuery::Size { level, has_level } => naga::ImageQuery::Size {
          level: if has_level { Some(level.into()) } else { None },
        },
        ImageQuery::NumLevels => naga::ImageQuery::NumLevels,
        ImageQuery::NumLayers => naga::ImageQuery::NumLayers,
        ImageQuery::NumSamples => naga::ImageQuery::NumSamples,
      };
      Ok(result)
    }
  }

  impl TryFrom<DerivativeControl> for naga::DerivativeControl {
    type Error = ConversionError;

    fn try_from(
      derivative_control: DerivativeControl,
    ) -> Result<naga::DerivativeControl, Self::Error> {
      let result = match derivative_control {
        DerivativeControl::Coarse => naga::DerivativeControl::Coarse,
        DerivativeControl::Fine => naga::DerivativeControl::Fine,
        DerivativeControl::None => naga::DerivativeControl::None,
      };
      Ok(result)
    }
  }

  impl TryFrom<DerivativeAxis> for naga::DerivativeAxis {
    type Error = ConversionError;

    fn try_from(derivative_axis: DerivativeAxis) -> Result<naga::DerivativeAxis, Self::Error> {
      let result = match derivative_axis {
        DerivativeAxis::X => naga::DerivativeAxis::X,
        DerivativeAxis::Y => naga::DerivativeAxis::Y,
        DerivativeAxis::Width => naga::DerivativeAxis::Width,
      };
      Ok(result)
    }
  }

  impl TryFrom<RelationalFunction> for naga::RelationalFunction {
    type Error = ConversionError;

    fn try_from(
      relational_function: RelationalFunction,
    ) -> Result<naga::RelationalFunction, Self::Error> {
      let result = match relational_function {
        RelationalFunction::All => naga::RelationalFunction::All,
        RelationalFunction::Any => naga::RelationalFunction::Any,
        RelationalFunction::IsNan => naga::RelationalFunction::IsNan,
        RelationalFunction::IsInf => naga::RelationalFunction::IsInf,
      };
      Ok(result)
    }
  }

  impl TryFrom<MathFunction> for naga::MathFunction {
    type Error = ConversionError;

    fn try_from(math_function: MathFunction) -> Result<naga::MathFunction, Self::Error> {
      let result = match math_function {
        MathFunction::Abs => naga::MathFunction::Abs,
        MathFunction::Min => naga::MathFunction::Min,
        MathFunction::Max => naga::MathFunction::Max,
        MathFunction::Clamp => naga::MathFunction::Clamp,
        MathFunction::Saturate => naga::MathFunction::Saturate,
        MathFunction::Cos => naga::MathFunction::Cos,
        MathFunction::Cosh => naga::MathFunction::Cosh,
        MathFunction::Sin => naga::MathFunction::Sin,
        MathFunction::Sinh => naga::MathFunction::Sinh,
        MathFunction::Tan => naga::MathFunction::Tan,
        MathFunction::Tanh => naga::MathFunction::Tanh,
        MathFunction::Acos => naga::MathFunction::Acos,
        MathFunction::Asin => naga::MathFunction::Asin,
        MathFunction::Atan => naga::MathFunction::Atan,
        MathFunction::Atan2 => naga::MathFunction::Atan2,
        MathFunction::Asinh => naga::MathFunction::Asinh,
        MathFunction::Acosh => naga::MathFunction::Acosh,
        MathFunction::Atanh => naga::MathFunction::Atanh,
        MathFunction::Radians => naga::MathFunction::Radians,
        MathFunction::Degrees => naga::MathFunction::Degrees,
        MathFunction::Ceil => naga::MathFunction::Ceil,
        MathFunction::Floor => naga::MathFunction::Floor,
        MathFunction::Round => naga::MathFunction::Round,
        MathFunction::Fract => naga::MathFunction::Fract,
        MathFunction::Trunc => naga::MathFunction::Trunc,
        MathFunction::Modf => naga::MathFunction::Modf,
        MathFunction::Frexp => naga::MathFunction::Frexp,
        MathFunction::Ldexp => naga::MathFunction::Ldexp,
        MathFunction::Exp => naga::MathFunction::Exp,
        MathFunction::Exp2 => naga::MathFunction::Exp2,
        MathFunction::Log => naga::MathFunction::Log,
        MathFunction::Log2 => naga::MathFunction::Log2,
        MathFunction::Pow => naga::MathFunction::Pow,
        MathFunction::Dot => naga::MathFunction::Dot,
        MathFunction::Outer => naga::MathFunction::Outer,
        MathFunction::Cross => naga::MathFunction::Cross,
        MathFunction::Distance => naga::MathFunction::Distance,
        MathFunction::Length => naga::MathFunction::Length,
        MathFunction::Normalize => naga::MathFunction::Normalize,
        MathFunction::FaceForward => naga::MathFunction::FaceForward,
        MathFunction::Reflect => naga::MathFunction::Reflect,
        MathFunction::Refract => naga::MathFunction::Refract,
        MathFunction::Sign => naga::MathFunction::Sign,
        MathFunction::Fma => naga::MathFunction::Fma,
        MathFunction::Mix => naga::MathFunction::Mix,
        MathFunction::Step => naga::MathFunction::Step,
        MathFunction::SmoothStep => naga::MathFunction::SmoothStep,
        MathFunction::Sqrt => naga::MathFunction::Sqrt,
        MathFunction::InverseSqrt => naga::MathFunction::InverseSqrt,
        MathFunction::Inverse => naga::MathFunction::Inverse,
        MathFunction::Transpose => naga::MathFunction::Transpose,
        MathFunction::Determinant => naga::MathFunction::Determinant,
        MathFunction::CountTrailingZeros => naga::MathFunction::CountTrailingZeros,
        MathFunction::CountLeadingZeros => naga::MathFunction::CountLeadingZeros,
        MathFunction::CountOneBits => naga::MathFunction::CountOneBits,
        MathFunction::ReverseBits => naga::MathFunction::ReverseBits,
        MathFunction::ExtractBits => naga::MathFunction::ExtractBits,
        MathFunction::InsertBits => naga::MathFunction::InsertBits,
        MathFunction::FindLsb => naga::MathFunction::FindLsb,
        MathFunction::FindMsb => naga::MathFunction::FindMsb,
        MathFunction::Pack4x8snorm => naga::MathFunction::Pack4x8snorm,
        MathFunction::Pack4x8unorm => naga::MathFunction::Pack4x8unorm,
        MathFunction::Pack2x16snorm => naga::MathFunction::Pack2x16snorm,
        MathFunction::Pack2x16unorm => naga::MathFunction::Pack2x16unorm,
        MathFunction::Pack2x16float => naga::MathFunction::Pack2x16float,
        MathFunction::Unpack4x8snorm => naga::MathFunction::Unpack4x8snorm,
        MathFunction::Unpack4x8unorm => naga::MathFunction::Unpack4x8unorm,
        MathFunction::Unpack2x16snorm => naga::MathFunction::Unpack2x16snorm,
        MathFunction::Unpack2x16unorm => naga::MathFunction::Unpack2x16unorm,
        MathFunction::Unpack2x16float => naga::MathFunction::Unpack2x16float,
      };
      Ok(result)
    }
  }

  impl TryFrom<Literal> for naga::Literal {
    type Error = ConversionError;

    fn try_from(literal: Literal) -> Result<naga::Literal, Self::Error> {
      let result = match literal {
        Literal::F64(f) => naga::Literal::F64(f),
        Literal::F32(f) => naga::Literal::F32(f),
        Literal::U32(u) => naga::Literal::U32(u),
        Literal::I32(i) => naga::Literal::I32(i),
        Literal::I64(i) => naga::Literal::I64(i),
        Literal::Bool(b) => naga::Literal::Bool(b),
        Literal::AbstractInt(i) => naga::Literal::AbstractInt(i),
        Literal::AbstractFloat(f) => naga::Literal::AbstractFloat(f),
      };
      Ok(result)
    }
  }

  impl TryFrom<Expression> for naga::Expression {
    type Error = ConversionError;

    fn try_from(expr: Expression) -> Result<naga::Expression, Self::Error> {
      let result = match expr {
        Expression::Literal(literal) => naga::Expression::Literal(literal.try_into()?),
        Expression::Constant(handle) => naga::Expression::Constant(handle.into()),
        Expression::ZeroValue(ty) => naga::Expression::ZeroValue(ty.into()),
        Expression::Compose {
          ty,
          components,
          components_len,
        } => {
          let mut components1: Vec<naga::Handle<naga::Expression>> =
            Vec::with_capacity(components_len as usize);
          for handle in unsafe { std::slice::from_raw_parts(components, components_len as usize) } {
            components1.push(handle.clone().into());
          }
          naga::Expression::Compose {
            ty: ty.into(),
            components: components1,
          }
        }
        Expression::Access { base, index } => naga::Expression::Access {
          base: base.into(),
          index: index.into(),
        },
        Expression::AccessIndex { base, index } => naga::Expression::AccessIndex {
          base: base.into(),
          index,
        },
        Expression::Splat { size, value } => naga::Expression::Splat {
          size: size.try_into()?,
          value: value.into(),
        },
        Expression::Swizzle {
          size,
          vector,
          pattern,
        } => {
          let mut swizzle_components = [naga::SwizzleComponent::X; 4];
          for (src, dst) in pattern.iter().zip(swizzle_components.iter_mut()) {
            *dst = src.clone().try_into()?;
          }
          naga::Expression::Swizzle {
            size: size.try_into()?,
            vector: vector.into(),
            pattern: swizzle_components,
          }
        }
        Expression::FunctionArgument(index) => naga::Expression::FunctionArgument(index),
        Expression::GlobalVariable(handle) => naga::Expression::GlobalVariable(handle.into()),
        Expression::LocalVariable(handle) => naga::Expression::LocalVariable(handle.into()),
        Expression::Load { pointer } => naga::Expression::Load {
          pointer: pointer.into(),
        },
        Expression::ImageSample {
          image,
          sampler,
          gather,
          has_gather,
          coordinate,
          array_index,
          has_array_index,
          offset,
          has_offset,
          level,
          depth_ref,
          has_depth_ref,
        } => naga::Expression::ImageSample {
          image: image.into(),
          sampler: sampler.into(),
          gather: if has_gather {
            Some(gather.try_into()?)
          } else {
            None
          },
          coordinate: coordinate.into(),
          array_index: if has_array_index {
            Some(array_index.into())
          } else {
            None
          },
          offset: if has_offset {
            Some(offset.into())
          } else {
            None
          },
          level: level.try_into()?,
          depth_ref: if has_depth_ref {
            Some(depth_ref.into())
          } else {
            None
          },
        },
        Expression::ImageLoad {
          image,
          coordinate,
          array_index,
          has_array_index,
          sample,
          has_sample,
          level,
          has_level,
        } => naga::Expression::ImageLoad {
          image: image.into(),
          coordinate: coordinate.into(),
          array_index: if has_array_index {
            Some(array_index.into())
          } else {
            None
          },
          sample: if has_sample {
            Some(sample.into())
          } else {
            None
          },
          level: if has_level { Some(level.into()) } else { None },
        },
        Expression::ImageQuery { image, query } => naga::Expression::ImageQuery {
          image: image.into(),
          query: query.try_into()?,
        },
        Expression::Unary { op, expr } => naga::Expression::Unary {
          op: op.try_into()?,
          expr: expr.into(),
        },
        Expression::Binary { op, left, right } => naga::Expression::Binary {
          op: op.try_into()?,
          left: left.into(),
          right: right.into(),
        },
        Expression::Select {
          condition,
          accept,
          reject,
        } => naga::Expression::Select {
          condition: condition.into(),
          accept: accept.into(),
          reject: reject.into(),
        },
        Expression::Derivative { axis, ctrl, expr } => naga::Expression::Derivative {
          axis: axis.try_into()?,
          ctrl: ctrl.try_into()?,
          expr: expr.into(),
        },
        Expression::Relational { fun, argument } => naga::Expression::Relational {
          fun: fun.try_into()?,
          argument: argument.into(),
        },
        Expression::Math {
          fun,
          arg,
          arg1,
          has_arg1,
          arg2,
          has_arg2,
          arg3,
          has_arg3,
        } => naga::Expression::Math {
          fun: fun.try_into()?,
          arg: arg.into(),
          arg1: if has_arg1 { Some(arg1.into()) } else { None },
          arg2: if has_arg2 { Some(arg2.into()) } else { None },
          arg3: if has_arg3 { Some(arg3.into()) } else { None },
        },
        Expression::As {
          expr,
          kind,
          convert,
          has_convert,
        } => naga::Expression::As {
          expr: expr.into(),
          kind: kind.try_into()?,
          convert: if has_convert { Some(convert) } else { None },
        },
        Expression::CallResult(handle) => naga::Expression::CallResult(handle.into()),
        Expression::AtomicResult { ty, comparison } => naga::Expression::AtomicResult {
          ty: ty.into(),
          comparison,
        },
        Expression::ArrayLength(handle) => naga::Expression::ArrayLength(handle.into()),
        Expression::RayQueryProceedResult => naga::Expression::RayQueryProceedResult,
        Expression::RayQueryGetIntersection { query, committed } => {
          naga::Expression::RayQueryGetIntersection {
            query: query.into(),
            committed,
          }
        }
      };
      Ok(result)
    }
  }
}

pub struct NagaWriter {
  module: naga::Module,
  validation: Option<naga::valid::ModuleInfo>,
  output_string: Option<CString>,
}

#[no_mangle]
pub unsafe extern "C" fn nagaNew() -> *mut NagaWriter {
  Box::into_raw(Box::new(NagaWriter {
    module: naga::Module {
      ..Default::default()
    },
    validation: None,
    output_string: None,
  }))
}

#[no_mangle]
pub unsafe extern "C" fn nagaFunctionSetBody(ctx_: *mut NagaFunctionWriter, stmt: native::Block) {
  (*ctx_).fun.body = stmt.try_into().expect("Invalid body");
}

#[repr(C)]
pub struct NagaFunctionWriter {
  outer: *mut NagaWriter,
  fun: naga::Function,
}

pub type FunctionWriteCallback =
  extern "C" fn(ctx: *mut NagaFunctionWriter, user_data: *mut c_void);

#[no_mangle]
pub unsafe extern "C" fn nagaFunctionStoreExpression(
  writer_: *mut NagaFunctionWriter,
  expr: native::Expression,
  name_: *const c_char,
) -> native::Handle<native::Expression> {
  let writer = writer_.as_mut().expect("Invalid writer");
  let expr = expr.try_into().expect("Invalid expression");
  let name = native::convert_string(name_);

  let handle = writer.fun.expressions.append(expr, naga::Span::default());
  if let Some(name) = name {
    writer.fun.named_expressions.insert(handle, name);
  }
  return handle.into();
}

#[no_mangle]
pub unsafe extern "C" fn nagaFunctionAddArgument(
  writer_: *mut NagaFunctionWriter,
  fn_arg_: *const native::FunctionArgument,
) {
  let writer = writer_.as_mut().expect("Invalid writer");
  let fn_arg = fn_arg_.as_ref().expect("Invalid argument");

  writer.fun.arguments.push(
    fn_arg
      .clone()
      .try_into()
      .expect("Invalid function argument"),
  );
}

#[no_mangle]
pub unsafe extern "C" fn nagaFunctionSetResult(
  writer_: *mut NagaFunctionWriter,
  res_: *const native::FunctionResult,
) {
  let writer = writer_.as_mut().expect("Invalid writer");
  let res = res_.as_ref().expect("Invalid argument");

  writer.fun.result = Some(res.clone().try_into().expect("Invalid function result"));
}

#[no_mangle]
pub unsafe extern "C" fn nagaStoreConstant(
  writer_: *mut NagaWriter,
  c: native::Constant,
) -> native::Handle<native::Constant> {
  let writer = writer_.as_mut().expect("Invalid writer");
  let c = c.try_into().expect("Invalid constant");

  writer
    .module
    .constants
    .append(c, naga::Span::default())
    .into()
}

#[no_mangle]
pub unsafe extern "C" fn nagaStoreGlobalExpression(
  writer_: *mut NagaWriter,
  expr: native::Expression,
) -> native::Handle<native::Expression> {
  let writer = writer_.as_mut().expect("Invalid writer");
  let expr = expr.try_into().expect("Invalid expression");

  let handle = writer
    .module
    .global_expressions
    .append(expr, naga::Span::default());
  return handle.into();
}

#[no_mangle]
pub unsafe extern "C" fn nagaStoreGlobal(
  writer_: *mut NagaWriter,
  gv: native::GlobalVariable,
) -> native::Handle<native::GlobalVariable> {
  let writer = writer_.as_mut().expect("Invalid writer");
  let gv = gv.try_into().expect("Invalid global variable");

  writer
    .module
    .global_variables
    .append(gv, naga::Span::default())
    .into()
}

#[no_mangle]
pub unsafe extern "C" fn nagaStoreType(
  writer_: *mut NagaWriter,
  t: native::Type,
) -> native::Handle<native::Type> {
  let writer = writer_.as_mut().expect("Invalid writer");
  let t = t.try_into().expect("Invalid type");

  writer.module.types.insert(t, naga::Span::default()).into()
}

#[no_mangle]
pub unsafe extern "C" fn nagaFunctionGetWriter(
  writer_: *mut NagaFunctionWriter,
) -> *mut NagaWriter {
  writer_.as_mut().expect("Invalid writer").outer
}

#[no_mangle]
pub unsafe extern "C" fn nagaAddFunction(
  writer_: *mut NagaWriter,
  desc_: *const native::Function,
  cb: FunctionWriteCallback,
  user_data: *mut c_void,
) -> native::Handle<native::Function> {
  let writer = writer_.as_mut().expect("Invalid writer");
  let desc = desc_.as_ref().expect("Invalid descriptor");

  let mut ctx = NagaFunctionWriter {
    outer: writer_,
    fun: naga::Function {
      name: native::convert_string(desc.name),
      ..Default::default()
    },
  };

  cb(&mut ctx, user_data);

  writer
    .module
    .functions
    .append(ctx.fun, naga::Span::default())
    .into()
}

#[no_mangle]
pub unsafe extern "C" fn nagaAddEntryPoint(
  writer_: *mut NagaWriter,
  desc_: *const native::EntryPoint,
  cb: FunctionWriteCallback,
  user_data: *mut c_void,
) {
  let writer = writer_.as_mut().expect("Invalid writer");
  let desc = desc_.as_ref().expect("Invalid descriptor");

  let mut ctx = NagaFunctionWriter {
    outer: writer_,
    fun: naga::Function {
      name: Some(CStr::from_ptr(desc.name).to_string_lossy().to_string()),
      ..Default::default()
    },
  };

  cb(&mut ctx, user_data);

  writer.module.entry_points.push(naga::EntryPoint {
    early_depth_test: if desc.has_early_depth_test {
      Some(
        desc
          .early_depth_test
          .try_into()
          .expect("Invalid early depth test"),
      )
    } else {
      None
    },
    name: CStr::from_ptr(desc.name).to_string_lossy().to_string(),
    function: ctx.fun,
    stage: desc.stage.try_into().expect("Invalid stage"),
    workgroup_size: desc.workgroup_size,
  });
}

fn into_wgsl(writer: &mut NagaWriter) -> Result<*const c_char, ()> {
  use naga::back::wgsl;

  let valid_module = writer.validation.as_ref().ok_or(())?;

  let explicit_types = false;

  let mut flags = wgsl::WriterFlags::empty();
  flags.set(wgsl::WriterFlags::EXPLICIT_TYPES, explicit_types);

  let string = wgsl::write_string(&writer.module, valid_module, flags).map_err(|x| {
    std::println!("Failed to write WGSL: {:?}", x);
    ()
  })?;

  writer.output_string = Some(CString::new(string).map_err(|_e| ())?);
  Ok(writer.output_string.as_ref().unwrap().as_ptr())
}

#[no_mangle]
pub unsafe extern "C" fn nagaValidate(writer: *mut NagaWriter) -> bool {
  let writer = writer.as_mut().expect("Invalid writer");

  match naga::valid::Validator::new(
    naga::valid::ValidationFlags::all(),
    naga::valid::Capabilities::all(),
  )
  .validate(&writer.module)
  {
    Ok(info) => {
      writer.validation = Some(info);
      true
    }
    Err(e) => {
      std::println!("Validation failed: {:?}", e);
      false
    }
  }
}

#[no_mangle]
pub unsafe extern "C" fn nagaIntoWgsl(writer: *mut NagaWriter) -> *const c_char {
  match into_wgsl(writer.as_mut().expect("Invalid writer")) {
    Ok(string) => string,
    Err(_) => std::ptr::null(),
  }
}
