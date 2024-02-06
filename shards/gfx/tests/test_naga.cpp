// Tests for the naga API

#include <gfx/naga_helper.hpp>
#include <catch2/catch_all.hpp>
#include <gfx/gfx_wgpu.hpp>
#include <spdlog/spdlog.h>

using namespace gfx;
using namespace naga;
TEST_CASE("Naga Basic", "[naga]") {
  Writer ctx(nagaNew());

  std::vector<StructMember> outStructMembers;
  outStructMembers.push_back(StructMember{
      .name = "position",
      .ty = ctx.makeType<float4>(),
      .binding =
          Binding{
              .tag = Binding::Tag::BuiltIn,
              .built_in =
                  Binding::BuiltIn_Body{
                      ._0 = BuiltIn{.tag = BuiltIn::Tag::Position},
                  },
          },
      .has_binding = true,
  });
  outStructMembers.push_back(StructMember{
      .name = "color",
      .ty = ctx.makeType<float4>(),
      .binding =
          Binding{
              .tag = Binding::Tag::Location,
              .location =
                  Binding::Location_Body{
                      .location = 0,
                  },
          },
      .has_binding = true,
      .offset = sizeof(float) * 4,
  });

  auto t = Type{
      .name = "Output",
      .inner =
          TypeInner{
              .tag = TypeInner::Tag::Struct,
              .struct_ =
                  TypeInner::Struct_Body{
                      .members = outStructMembers.data(),
                      .members_len = uint32_t(outStructMembers.size()),
                      .span = sizeof(float) * 4 * 2,
                  },
          },
  };
  auto structType = nagaStoreType(ctx.ctx, t);
  auto globalOut =
      nagaStoreGlobal(ctx.ctx, GlobalVariable{.name = "test", .space = {.tag = AddressSpace::Tag::Private}, .ty = structType});

  ctx.addEntryPoint(
      [&](FunctionWriter &fn) {
        FunctionResult fr{
            .ty = ctx.makeType<float4>(),
            .binding =
                Binding{
                    .tag = Binding::Tag::BuiltIn,
                    .built_in =
                        Binding::BuiltIn_Body{
                            ._0 = BuiltIn{.tag = BuiltIn::Tag::Position},
                        },
                },
            .has_binding = true,
        };
        nagaFunctionSetResult(fn.functionCtx, &fr);

        FunctionArgument arg0{
            .name = "position",
            .ty = ctx.makeType<float3>(),
            .binding =
                Binding{
                    .tag = Binding::Tag::Location,
                    .location =
                        Binding::Location_Body{
                            .location = 0,
                        },
                },
            .has_binding = true,
        };
        nagaFunctionAddArgument(fn.functionCtx, &arg0);

        auto e0 = fn.makeExpr(Expression::Constant_Body{._0 = ctx.makeConstant(float4(0.0f, 0.0f, 0.0f, 1.0f))});

        auto &outStructValues = ctx.exprHandles.emplace_back();
        outStructValues.push_back(fn.makeExpr(Expression::Constant_Body{._0 = ctx.makeConstant(float4(0.0f, 0.0f, 0.0f, 1.0f))}));
        outStructValues.push_back(fn.makeExpr(Expression::Constant_Body{._0 = ctx.makeConstant(float4(1.0f, 1.0f, 1.0f, 1.0f))}));

        Expression::Compose_Body compose{};
        compose.components = outStructValues.data();
        compose.components_len = uint32_t(outStructValues.size());
        compose.ty = structType;
        auto makeResult = fn.makeExpr("result", compose);

        auto gv = fn.makeExpr(Expression::GlobalVariable_Body{._0 = globalOut});

        return std::vector<Statement>{
            makeStmt(Statement::Emit_Body{._0 = Range<Expression>{.start = gv.index, .end = gv.index}}),
            makeStmt(Statement::Store_Body{.pointer = gv, .value = makeResult}),
            makeStmt(Statement::Return_Body{.value = e0, .has_value = true}),
        };
      },
      "main", ShaderStage::Vertex);

  CHECK(nagaValidate(ctx.ctx));

  const char *tmp = nagaIntoWgsl(ctx.ctx);
  CHECK(tmp);
  SPDLOG_DEBUG("Result:\n{}", tmp);

  // struct ctx

  // nagaAddEntryPoint(naga, &ep, [](NagaFunctionWriteContext *ctx_, void *ud) {
  //       Context ctx(ctx_);
  //       std::vector<Statement> statements{};

  //       auto c0 = ctx.makeExpr(Expression::Constant_Body{._0 = ctx.makeConstant(float4(0.0f))});
  //       statements.push_back(makeStmt(Statement::Return_Body{.value = c0, .has_value = true}));
  //     },
  //     nullptr);

  // gfxNag
  // CHECK(counter == 201);
}
