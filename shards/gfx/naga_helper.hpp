#ifndef C613BA8F_A265_4DFC_8425_034244F1EC04
#define C613BA8F_A265_4DFC_8425_034244F1EC04

#include "rust/naga-native/bindings.hpp"
#include <linalg.h>
#include <list>
#include <vector>
#include <type_traits>

namespace naga {

template <typename T> constexpr Expression::Tag getExpressionTag() {
  if constexpr (std::is_same_v<T, Expression::Literal_Body>) {
    return Expression::Tag::Literal;
  } else if constexpr (std::is_same_v<T, Expression::Constant_Body>) {
    return Expression::Tag::Constant;
  } else if constexpr (std::is_same_v<T, Expression::ZeroValue_Body>) {
    return Expression::Tag::ZeroValue;
  } else if constexpr (std::is_same_v<T, Expression::Compose_Body>) {
    return Expression::Tag::Compose;
  } else if constexpr (std::is_same_v<T, Expression::Access_Body>) {
    return Expression::Tag::Access;
  } else if constexpr (std::is_same_v<T, Expression::AccessIndex_Body>) {
    return Expression::Tag::AccessIndex;
  } else if constexpr (std::is_same_v<T, Expression::Splat_Body>) {
    return Expression::Tag::Splat;
  } else if constexpr (std::is_same_v<T, Expression::Swizzle_Body>) {
    return Expression::Tag::Swizzle;
  } else if constexpr (std::is_same_v<T, Expression::FunctionArgument_Body>) {
    return Expression::Tag::FunctionArgument;
  } else if constexpr (std::is_same_v<T, Expression::GlobalVariable_Body>) {
    return Expression::Tag::GlobalVariable;
  } else if constexpr (std::is_same_v<T, Expression::LocalVariable_Body>) {
    return Expression::Tag::LocalVariable;
  } else if constexpr (std::is_same_v<T, Expression::Load_Body>) {
    return Expression::Tag::Load;
  } else if constexpr (std::is_same_v<T, Expression::ImageSample_Body>) {
    return Expression::Tag::ImageSample;
  } else if constexpr (std::is_same_v<T, Expression::ImageLoad_Body>) {
    return Expression::Tag::ImageLoad;
  } else if constexpr (std::is_same_v<T, Expression::ImageQuery_Body>) {
    return Expression::Tag::ImageQuery;
  } else if constexpr (std::is_same_v<T, Expression::Unary_Body>) {
    return Expression::Tag::Unary;
  } else if constexpr (std::is_same_v<T, Expression::Binary_Body>) {
    return Expression::Tag::Binary;
  } else if constexpr (std::is_same_v<T, Expression::Select_Body>) {
    return Expression::Tag::Select;
  } else if constexpr (std::is_same_v<T, Expression::Derivative_Body>) {
    return Expression::Tag::Derivative;
  } else if constexpr (std::is_same_v<T, Expression::Relational_Body>) {
    return Expression::Tag::Relational;
  } else if constexpr (std::is_same_v<T, Expression::Math_Body>) {
    return Expression::Tag::Math;
  } else if constexpr (std::is_same_v<T, Expression::As_Body>) {
    return Expression::Tag::As;
  } else if constexpr (std::is_same_v<T, Expression::CallResult_Body>) {
    return Expression::Tag::CallResult;
  } else if constexpr (std::is_same_v<T, Expression::AtomicResult_Body>) {
    return Expression::Tag::AtomicResult;
  } else if constexpr (std::is_same_v<T, Expression::ArrayLength_Body>) {
    return Expression::Tag::ArrayLength;
  } else if constexpr (std::is_same_v<T, Expression::RayQueryGetIntersection_Body>) {
    return Expression::Tag::RayQueryGetIntersection;
  }
}

template <typename T> constexpr Statement::Tag getStatementTag() {
  if constexpr (std::is_same_v<T, Statement::Emit_Body>) {
    return Statement::Tag::Emit;
  } else if constexpr (std::is_same_v<T, Statement::Block_Body>) {
    return Statement::Tag::Block;
  } else if constexpr (std::is_same_v<T, Statement::If_Body>) {
    return Statement::Tag::If;
  } else if constexpr (std::is_same_v<T, Statement::Switch_Body>) {
    return Statement::Tag::Switch;
  } else if constexpr (std::is_same_v<T, Statement::Loop_Body>) {
    return Statement::Tag::Loop;
  } else if constexpr (std::is_same_v<T, Statement::Return_Body>) {
    return Statement::Tag::Return;
  } else if constexpr (std::is_same_v<T, Statement::Barrier_Body>) {
    return Statement::Tag::Barrier;
  } else if constexpr (std::is_same_v<T, Statement::Store_Body>) {
    return Statement::Tag::Store;
  } else if constexpr (std::is_same_v<T, Statement::ImageStore_Body>) {
    return Statement::Tag::ImageStore;
  } else if constexpr (std::is_same_v<T, Statement::Atomic_Body>) {
    return Statement::Tag::Atomic;
  } else if constexpr (std::is_same_v<T, Statement::WorkGroupUniformLoad_Body>) {
    return Statement::Tag::WorkGroupUniformLoad;
  } else if constexpr (std::is_same_v<T, Statement::Call_Body>) {
    return Statement::Tag::Call;
  } else if constexpr (std::is_same_v<T, Statement::RayQuery_Body>) {
    return Statement::Tag::RayQuery;
  }
}

template <typename T> constexpr ScalarKind scalarKindOf() {
  if constexpr (std::is_same_v<T, bool>) {
    return ScalarKind::Bool;
  } else if constexpr (std::is_floating_point_v<T>) {
    return ScalarKind::Float;
  } else if constexpr (std::is_integral_v<T>) {
    if constexpr (std::is_signed_v<T>) {
      return ScalarKind::Sint;
    } else {
      return ScalarKind::Uint;
    }
  } else {
    return ScalarKind(~0);
  }
}

template <typename T> constexpr Bytes typeWidth() { return sizeof(T); }

template <int M> constexpr VectorSize toVectorSize() {
  static_assert(M == 2 || M == 3 || M == 4, "Invalid vector dimension");
  if (M == 2)
    return VectorSize::Bi;
  else if (M == 3)
    return VectorSize::Tri;
  else
    return VectorSize::Quad;
}

template <typename T, int M> constexpr Type typeOfVector() {
  Type type{};
  auto &inner = type.inner;
  auto &vec = inner.vector;

  inner.tag = TypeInner::Tag::Vector;
  vec.scalar = Scalar{.kind = scalarKindOf<T>(), .width = typeWidth<T>()};
  vec.size = toVectorSize<M>();

  return type;
}

template <typename T, int M, int N> constexpr Type typeOfMatrix() {
  Type type{};
  auto &inner = type.inner;
  auto &mat = inner.matrix;

  inner.tag = TypeInner::Tag::Matrix;
  mat.rows = toVectorSize<M>();
  mat.columns = toVectorSize<N>();
  mat.scalar = Scalar{.kind = scalarKindOf<T>(), .width = typeWidth<T>()};

  return type;
}

template <typename T, template <typename...> class R> struct is_specialization : std::false_type {};
template <template <typename...> class R, typename... Args> struct is_specialization<R<Args...>, R> : std::true_type {};

template <typename T> inline Expression makeExpr(T expr) {
  Expression e{.tag = getExpressionTag<T>()};
  memcpy(&e.access, &expr, sizeof(T));
  return e;
}

struct Writer {
  NagaWriter *ctx{};
  std::list<std::vector<Handle<Constant>>> constantHandles;
  std::list<std::vector<Handle<Expression>>> exprHandles;

  Writer(NagaWriter *ctx) : ctx(ctx) {}

  template <typename T> Handle<Type> makeDerivedType(const T &dummy) {
    if constexpr (std::is_scalar_v<T>) {
      Type t{};
      auto &ti = t.inner;
      ti.scalar._0.width = sizeof(T);
      ti.scalar._0.kind = scalarKindOf<T>();
      return nagaStoreType(ctx, t);
    }
  }

  template <typename T, int M> Handle<Type> makeDerivedType(const linalg::vec<T, M> &dummy) {
    return nagaStoreType(ctx, typeOfVector<T, M>());
  }

  template <typename T, int M, int N> Handle<Type> makeDerivedType(const linalg::mat<T, M, N> &dummy) {
    return nagaStoreType(ctx, typeOfMatrix<T, M, N>());
  }

  template <typename T> Handle<Type> makeType() {
    static T dummy{};
    return makeDerivedType(dummy);
  }

  template <typename T> inline Handle<Expression> makeConstExpr(T value) {
    Expression e{
        .tag = Expression::Tag::Literal,
    };
    if constexpr (std::is_same_v<T, bool>) {
      e.literal._0.tag = Literal::Tag::Bool;
      e.literal._0.bool_._0 = value;
    } else if constexpr (std::is_same_v<T, float>) {
      e.literal._0.tag = Literal::Tag::F32;
      e.literal._0.f32._0 = value;
    } else if constexpr (std::is_same_v<T, double>) {
      e.literal._0.tag = Literal::Tag::F64;
      e.literal._0.f64._0 = value;
    } else if constexpr (std::is_same_v<T, uint32_t>) {
      e.literal._0.tag = Literal::Tag::U32;
      e.literal._0.u32._0 = value;
    } else if constexpr (std::is_same_v<T, int32_t>) {
      e.literal._0.tag = Literal::Tag::I32;
      e.literal._0.i32._0 = value;
    } else if constexpr (std::is_integral_v<T>) {
      e.literal._0.tag = Literal::Tag::I64;
      e.literal._0.i64._0 = int64_t(value);
    } else {
      throw std::runtime_error("Unsupported constant type");
    }
    return nagaStoreGlobalExpression(ctx, e);
  }

  template <typename T, int M> inline Handle<Expression> makeConstExpr(linalg::vec<T, M> value) {
    Expression e{.tag = Expression::Tag::Compose};
    e.compose.ty = makeDerivedType(value);

    auto &storage = exprHandles.emplace_back();
    storage.resize(M);
    for (size_t i = 0; i < M; i++) {
      storage[i] = makeConstExpr(value[i]);
    }
    e.compose.components = storage.data();
    e.compose.components_len = storage.size();
    return nagaStoreGlobalExpression(ctx, e);
  }

  template <typename T, int M, int N>
  inline Handle<Expression> makeConstExpr(linalg::mat<T, M, N> value, const char *name = nullptr) {
    Expression e{.tag = Expression::Tag::Compose};
    e.compose.ty = makeDerivedType(value);

    // Store matrix constant as an array of N column vectors (of size M)
    auto &storage = exprHandles.emplace_back();
    storage.resize(M);
    for (size_t i = 0; i < N; i++) {
      auto &col = value[i];
      storage[i] = makeConstExpr(col);
    }
    e.compose.components = storage.data();
    e.compose.components_len = storage.size();
    return nagaStoreConstExpression(ctx, e);
  }

  template <typename T> inline Handle<Constant> makeConstant(T value, const char *name = nullptr) {
    Constant c{};
    c.name = name;
    c.ty = makeType<T>();
    c.init = makeConstExpr(value);

    return nagaStoreConstant(ctx, c);
  }

  template <typename T, int M> inline Handle<Constant> makeConstant(linalg::vec<T, M> value, const char *name = nullptr) {
    Constant c{};
    c.name = name;
    c.ty = makeDerivedType(value);
    c.init = makeConstExpr(value);

    return nagaStoreConstant(ctx, c);
  }

  template <typename T, int M, int N>
  inline Handle<Constant> makeConstant(linalg::mat<T, M, N> value, const char *name = nullptr) {
    Constant c{};
    c.name = name;
    c.ty = makeDerivedType(value);
    c.init = makeConstExpr(value);

    return nagaStoreConstant(ctx, c);
  }

  template <typename T> inline Handle<Function> addFunction(T callback, const char *name = nullptr);
  template <typename T>
  inline void addEntryPoint(T callback, const char *name, ShaderStage stage, linalg::aliases::uint3 workgroupSize = {0, 0, 0});
};

struct FunctionWriter {
  Writer ctx;
  NagaFunctionWriter *functionCtx{};

  FunctionWriter(NagaFunctionWriter *fnCtx) : ctx(nagaFunctionGetWriter(fnCtx)), functionCtx(fnCtx) {}

  template <typename T> inline Handle<Expression> makeExpr(T expr) { return makeExpr<T>(nullptr, expr); }

  template <typename T> inline Handle<Expression> makeExpr(const char *name, T expr) {
    Expression e{.tag = getExpressionTag<T>()};
    memcpy(&e.access, &expr, sizeof(T));
    return nagaFunctionStoreExpression(functionCtx, e, name);
  }

  void setBody(std::vector<Statement> &&statements) {
    nagaFunctionSetBody(functionCtx, Block{
                                         .body = statements.data(),
                                         .body_len = uint32_t(statements.size()),
                                     });
  }

  void setBody(Block &&block) { nagaFunctionSetBody(functionCtx, block); }
};

template <typename T> Handle<Function> Writer::addFunction(T callback, const char *name) {
  struct Data {
    T callback;
  } data{callback};
  Function desc{
      .name = name,
  };
  return nagaAddFunction(
      ctx, &desc,
      [](NagaFunctionWriter *fnCtx, void *ud) {
        FunctionWriter writer(fnCtx);
        auto body = ((Data *)ud)->callback(writer);
        writer.setBody(std::move(body));
      },
      &data);
}

template <typename T>
void Writer::addEntryPoint(T callback, const char *name, ShaderStage stage, linalg::aliases::uint3 workgroupSize) {
  struct Data {
    T callback;
  } data{callback};
  EntryPoint desc{
      .name = name,
      .stage = stage,
      .workgroup_size = {workgroupSize.x, workgroupSize.y, workgroupSize.z},
  };
  nagaAddEntryPoint(
      ctx, &desc,
      [](NagaFunctionWriter *fnCtx, void *ud) {
        FunctionWriter writer(fnCtx);
        auto body = ((Data *)ud)->callback(writer);
        writer.setBody(std::move(body));
      },
      &data);
}

template <typename T> inline Statement makeStmt(T stmt) {
  Statement s{.tag = getStatementTag<T>()};
  memcpy(&s.atomic, &stmt, sizeof(T));
  return s;
}

} // namespace naga

#endif /* C613BA8F_A265_4DFC_8425_034244F1EC04 */
