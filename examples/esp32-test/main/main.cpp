#include <stdio.h>
#include "shards_esp32.h"

// Exercise the component so the linker actually pulls in the glue (and thus the
// core runtime symbols it references) — this surfaces exactly what core sources
// still need to be compiled in.
extern "C" void app_main(void) {
  printf("Shards ESP32 boot\n");
  shards_esp32_init();
  printf("has running wires: %d\n", (int)shards_esp32_has_running_wires());
  shards_esp32_tick();
  shards_esp32_shutdown();
  printf("done\n");
}
