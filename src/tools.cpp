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

uint16_t tools::checksum_fletcher16( byte const *data, uint16_t bytes ) {
    uint16_t sum1 = 0xff, sum2 = 0xff;

    while (bytes) {
            uint16_t tlen = bytes > 20 ? 20 : bytes;
            bytes -= tlen;
            do {
                    sum2 += sum1 += *data++;
            } while (--tlen);
            sum1 = (sum1 & 0xff) + (sum1 >> 8);
            sum2 = (sum2 & 0xff) + (sum2 >> 8);
    }
    /* Second reduction step to reduce sums to 8 bits */
    sum1 = (sum1 & 0xff) + (sum1 >> 8);
    sum2 = (sum2 & 0xff) + (sum2 >> 8);
    return sum2 << 8 | sum1;
}