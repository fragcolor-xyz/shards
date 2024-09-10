#include "math.hpp"
#include "shards/inlined.hpp"
#include <shards/core/module.hpp>

SHARDS_REGISTER_FN(math) {
  using namespace shards::Math;
  REGISTER_ENUM(Mean::MeanEnumInfo);

  REGISTER_SHARD("Math.Add", Add);
  REGISTER_SHARD_ALIAS("Add", Add);
  REGISTER_SHARD("Math.Subtract", Subtract);
  REGISTER_SHARD_ALIAS("Sub", Subtract);
  REGISTER_SHARD("Math.Multiply", Multiply);
  REGISTER_SHARD_ALIAS("Mul", Multiply);
  REGISTER_SHARD("Math.Divide", Divide);
  REGISTER_SHARD_ALIAS("Div", Divide);

  REGISTER_SHARD("Math.Xor", Xor);
  REGISTER_SHARD("Math.And", And);
  REGISTER_SHARD("Math.Or", Or);
  REGISTER_SHARD("Math.Mod", Mod);
  REGISTER_SHARD("Math.LShift", LShift);
  REGISTER_SHARD("Math.RShift", RShift);

  REGISTER_SHARD("Math.Abs", Abs);
  REGISTER_SHARD_ALIAS("Abs", Abs);
  REGISTER_SHARD("Math.Exp", Exp);
  REGISTER_SHARD("Math.Exp2", Exp2);
  REGISTER_SHARD("Math.Expm1", Expm1);
  REGISTER_SHARD("Math.Log", Log);
  REGISTER_SHARD("Math.Log10", Log10);
  REGISTER_SHARD("Math.Log2", Log2);
  REGISTER_SHARD("Math.Log1p", Log1p);
  REGISTER_SHARD("Math.Sqrt", Sqrt);
  REGISTER_SHARD_ALIAS("Sqrt", Sqrt);
  REGISTER_SHARD("Math.FastSqrt", FastSqrt);
  REGISTER_SHARD_ALIAS("FastSqrt", FastSqrt);
  REGISTER_SHARD("Math.FastInvSqrt", FastInvSqrt);
  REGISTER_SHARD("Math.Cbrt", Cbrt);
  REGISTER_SHARD("Math.Sin", Sin);
  REGISTER_SHARD_ALIAS("Sin", Sin);
  REGISTER_SHARD("Math.Cos", Cos);
  REGISTER_SHARD_ALIAS("Cos", Cos);
  REGISTER_SHARD("Math.Tan", Tan);
  REGISTER_SHARD_ALIAS("Tan", Tan);
  REGISTER_SHARD("Math.Asin", Asin);
  REGISTER_SHARD_ALIAS("Asin", Asin);
  REGISTER_SHARD("Math.Acos", Acos);
  REGISTER_SHARD_ALIAS("Acos", Acos);
  REGISTER_SHARD("Math.Atan", Atan);
  REGISTER_SHARD_ALIAS("Atan", Atan);
  REGISTER_SHARD("Math.Sinh", Sinh);
  REGISTER_SHARD("Math.Cosh", Cosh);
  REGISTER_SHARD("Math.Tanh", Tanh);
  REGISTER_SHARD("Math.Asinh", Asinh);
  REGISTER_SHARD("Math.Acosh", Acosh);
  REGISTER_SHARD("Math.Atanh", Atanh);
  REGISTER_SHARD("Math.Erf", Erf);
  REGISTER_SHARD("Math.Erfc", Erfc);
  REGISTER_SHARD("Math.TGamma", TGamma);
  REGISTER_SHARD("Math.LGamma", LGamma);
  REGISTER_SHARD("Math.Ceil", Ceil);
  REGISTER_SHARD_ALIAS("Ceil", Ceil);
  REGISTER_SHARD("Math.Floor", Floor);
  REGISTER_SHARD_ALIAS("Floor", Floor);
  REGISTER_SHARD("Math.Trunc", Trunc);
  REGISTER_SHARD("Math.Round", Round);

  REGISTER_SHARD("Math.Mean", Mean);
  REGISTER_SHARD("Math.Inc", Inc);
  REGISTER_SHARD_ALIAS("Inc", Inc);
  REGISTER_SHARD("Math.Dec", Dec);
  REGISTER_SHARD_ALIAS("Dec", Dec);
  REGISTER_SHARD("Math.Negate", Negate);

  REGISTER_SHARD("Max", Max);
  REGISTER_SHARD("Min", Min);
  REGISTER_SHARD("Clamp", Clamp);

  REGISTER_SHARD("Math.Pow", Pow);
  REGISTER_SHARD_ALIAS("Pow", Pow);

  REGISTER_SHARD("Math.Lerp", Lerp);
  REGISTER_SHARD_ALIAS("Lerp", Lerp);

  REGISTER_SHARD("Math.Percentile", Percentile);
  REGISTER_SHARD_ALIAS("Percentile", Percentile);
}
