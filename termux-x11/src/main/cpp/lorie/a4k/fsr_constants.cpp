#include <cstdint>
#define A_CPU 1
#define A_GCC 1
#include "../../../../../../a4k/upstream/ffx_fsr_a.h"
#include "../../../../../../a4k/upstream/ffx_fsr1.h"
namespace a4k {
void easuConstants(uint32_t* c,int iw,int ih,int ow,int oh){FsrEasuCon(c,c+4,c+8,c+12,float(iw),float(ih),float(iw),float(ih),float(ow),float(oh));}
}
