/* Minimal kernel.h for the joystick host test: the test supplies _kernel_swi. */
#ifndef FAKE_KERNEL_H
#define FAKE_KERNEL_H
typedef struct { int r[10]; } _kernel_swi_regs;
typedef struct { int errnum; char errmess[252]; } _kernel_oserror;
#ifdef __cplusplus
extern "C" {
#endif
_kernel_oserror *_kernel_swi(int no,_kernel_swi_regs *in,_kernel_swi_regs *out);
#ifdef __cplusplus
}
#endif
#endif
