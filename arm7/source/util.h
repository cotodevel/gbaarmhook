#include "typedefsTGDS.h"
#include "dsregs.h"
#include "dsregs_asm.h"

//LUT table for fast-seeking 32bit depth unsigned values
extern const  u8 minilut[0x10];

//divswi output
extern u32 divswiarr[0x3];

extern u32 bios_irqhandlerstub_C;

#ifdef __cplusplus
extern "C" {
#endif

//lookup calls
extern u8 lutu16bitcnt(u16 x);
extern u8 lutu32bitcnt(u32 x);

#ifdef __cplusplus
}
#endif