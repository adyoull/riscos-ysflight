/* riscos-crossdev's fake kernel.h plus _kernel_osbyte/_kernel_osword. */
#ifdef __cplusplus
extern "C" {
#endif
#include <rcdfake/kernel.h>
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
int _kernel_osbyte (int op, int x, int y);
int _kernel_osword (int op, int *data);
#ifdef __cplusplus
}
#endif
