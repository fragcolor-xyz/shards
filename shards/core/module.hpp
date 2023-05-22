#ifndef F838E86A_A9AF_49CA_85F4_C443DDFC02FB
#define F838E86A_A9AF_49CA_85F4_C443DDFC02FB

#include <shards/shards.h>

#ifdef SHARDS_THIS_MODULE_ID
#define SHARDS_MODULE_FN(_x) SH_CAT(_x, SH_CAT(_, SHARDS_THIS_MODULE_ID))
#define SHARDS_REGISTER_FN(_id) extern "C" void SH_CAT(shardsRegister_, SH_CAT(SHARDS_THIS_MODULE_ID, SH_CAT(_, _id)))(SHCore* core)
#else
#define SHARDS_MODULE_FN(_x) void SH_CAT(shardsRegister_, _x)(SHCore* core)
#endif

#endif /* F838E86A_A9AF_49CA_85F4_C443DDFC02FB */
