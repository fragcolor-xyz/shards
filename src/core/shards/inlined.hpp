#ifndef AA4E918E_F09A_4717_B158_76C05D2A7388
#define AA4E918E_F09A_4717_B158_76C05D2A7388

#include "shards.h"
#include <string_view>

namespace shards {
// These shards run fully inline in the wire threaded execution engine
struct InlineShard {
  enum Type : uint32_t {
    // regular shards
    NotInline,
    // special flag that will optimize and skip activate calls
    NoopShard,
    // internal "quick" inlined shards
    CoreConst,
    CoreSleep,
    CoreInput,
    CoreForRange,
    CoreRepeat,
    CoreOnce,
    CoreGet,
    CoreRefRegular,
    CoreRefTable,
    CoreSetUpdateRegular,
    CoreSetUpdateTable,
    CoreSwap,
    CorePush,
    CoreIs,
    CoreIsNot,
    CoreAnd,
    CoreOr,
    CoreNot,
    CoreIsMore,
    CoreIsLess,
    CoreIsMoreEqual,
    CoreIsLessEqual,

    MathAdd,
    MathSubtract,
    MathMultiply,
    MathDivide,
    MathXor,
    MathAnd,
    MathOr,
    MathMod,
    MathLShift,
    MathRShift,

    MathAbs,
    MathExp,
    MathExp2,
    MathExpm1,
    MathLog,
    MathLog10,
    MathLog2,
    MathLog1p,
    MathSqrt,
    MathFastSqrt,
    MathFastInvSqrt,
    MathCbrt,
    MathSin,
    MathCos,
    MathTan,
    MathAsin,
    MathAcos,
    MathAtan,
    MathSinh,
    MathCosh,
    MathTanh,
    MathAsinh,
    MathAcosh,
    MathAtanh,
    MathErf,
    MathErfc,
    MathTGamma,
    MathLGamma,
    MathCeil,
    MathFloor,
    MathTrunc,
    MathRound,
  };
};

#ifdef SHARDS_INLINE_EVERYTHING
#define SHARDS_CONDITIONAL_INLINE __attribute__((always_inline))
#else
#define SHARDS_CONDITIONAL_INLINE
#endif

SHARDS_CONDITIONAL_INLINE void setInlineShardId(Shard *shard, std::string_view name);
SHARDS_CONDITIONAL_INLINE bool activateShardInline(Shard *blk, SHContext *context, const SHVar &input, SHVar &output);
} // namespace shards

#endif /* AA4E918E_F09A_4717_B158_76C05D2A7388 */
