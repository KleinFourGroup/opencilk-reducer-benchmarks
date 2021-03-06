#ifndef _RABIN_H_
#define _RABIN_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Define USED macro */
#define USED(x) { ulong y __attribute__ ((unused)); y = (ulong)x; }

enum {
    NWINDOW   = 32,
    MinSegment  = 1024,
    RabinMask = 0xfff,  // must be less than <= 0x7fff 
};

void rabininit(int, u32int*, u32int*);
int rabinseg(uchar*, int, int, u32int*, u32int*);

#ifdef __cplusplus
}
#endif

#endif //_RABIN_H_

