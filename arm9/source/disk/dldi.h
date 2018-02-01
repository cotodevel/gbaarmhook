#include <nds/arm9/dldi.h>
#include <nds/memory.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const u32  DLDI_MAGIC_NUMBER;
extern const char DLDI_MAGIC_STRING_BACKWARDS [];
extern DLDI_INTERFACE _io_dldi_stub;

#ifdef __cplusplus
}
#endif

