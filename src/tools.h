#ifndef tools_h
#define tools_h

extern int  __bss_end;
extern void *__brkval;

namespace tools
{
    int availableMemory();
};

#endif tools_h