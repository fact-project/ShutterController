#include "tools.h"

int tools::availableMemory() {
  int free_memory;
  if ((int)__brkval==0)
    free_memory = ((int)&free_memory) - ((int) &__bss_end);
  else
    free_memory = ((int)&free_memory) - ((int) &__brkval);

  return free_memory;
}

uint32_t tools::get_mean(const int pin, const uint16_t n)
{
    if (n == 0) return 0;
    uint32_t sum = 0;
    for (uint16_t i=0; i < n; i++)
    {
        uint16_t a = analogRead(pin);
        sum += a;
    }
    return sum / n;
}

tools::mean_std_t tools::get_mean_std(const int pin, const uint16_t n)
{
    mean_std_t tmp;
    tmp.mean = 0;
    tmp.var = 0;
    tmp.samples = 0;
    if (n == 0) return tmp;

    uint64_t sum = 0;
    uint64_t sq_sum = 0;
    for (uint16_t i=0; i < n; i++)
    {
        uint16_t a = analogRead(pin);
        sum += a;
        sq_sum += a * a;
    }

    uint64_t N = n;
    tmp.mean = (uint32_t) (sum / n);
    tmp.var = (N * sq_sum - sum * sum) / (N * N);
    tmp.samples = n;
    return tmp;
}