#ifndef tools_h
#define tools_h

extern int  __bss_end;
extern void *__brkval;

namespace tools
{
    int availableMemory();

    struct mean_std_t {
      double mean;
      double std;
    };
};

#endif // tools_h