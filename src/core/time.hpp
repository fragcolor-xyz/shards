#ifndef CCD685DF_1A63_4106_8892_59C211C45A20
#define CCD685DF_1A63_4106_8892_59C211C45A20

#include <chrono>

using SHClock = std::chrono::high_resolution_clock;
using SHTime = decltype(SHClock::now());
using SHDuration = std::chrono::duration<double>;
using SHTimeDiff = decltype(SHClock::now() - SHDuration(0.0));

#endif /* CCD685DF_1A63_4106_8892_59C211C45A20 */
