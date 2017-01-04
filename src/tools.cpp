#include "tools.h"

int tools::availableMemory() {
  int free_memory;
  if ((int)__brkval==0)
    free_memory = ((int)&free_memory) - ((int) &__bss_end);
  else
    free_memory = ((int)&free_memory) - ((int) &__brkval);

  return free_memory;
}