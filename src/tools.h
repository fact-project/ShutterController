#ifndef tools_h
#define tools_h

#include <stdint.h>
#include "Arduino.h"

extern int  __bss_end;
extern void *__brkval;

namespace tools
{
    int availableMemory();

    struct mean_std_t {
      uint32_t mean;
      uint32_t var;
    };

    mean_std_t get_mean_std(const int pin, const uint16_t n)
    {
        mean_std_t tmp;
        tmp.mean = 0;
        tmp.var = 0;
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
        return tmp;
    }

    uint32_t get_mean(const int pin, const uint16_t n)
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

};

#endif // tools_h