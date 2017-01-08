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
      uint16_t samples;
    };

    uint32_t get_mean(const int pin, const uint16_t n);
    mean_std_t get_mean_std(const int pin, const uint16_t n);

    uint16_t checksum_fletcher16( byte const *data, uint16_t bytes );
};

#endif // tools_h