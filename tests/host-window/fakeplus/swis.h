/* riscos-crossdev's fake swis.h plus the SWIs the window code uses. */
#ifdef __cplusplus
extern "C" {
#endif
#include <rcdfake/swis.h>
#ifdef __cplusplus
}
#endif
#ifndef OS_Word
#define OS_Word                0x07
#endif
#ifndef OS_ReadModeVariable
#define OS_ReadModeVariable    0x35
#endif
#ifndef OS_Pointer
#define OS_Pointer             0x64
#endif
#define Wimp_CreateWindow      0x400c1
#define Wimp_DeleteWindow      0x400c3
#define Wimp_OpenWindow        0x400c5
#define Wimp_RedrawWindow      0x400c8
#define Wimp_GetRectangle      0x400ca
#define Wimp_GetWindowState    0x400cb
#define Wimp_GetPointerInfo    0x400cf
#define Wimp_ForceRedraw       0x400d1
#define Wimp_SetCaretPosition  0x400d2
#define Wimp_ProcessKey        0x400dc
#define Wimp_PollIdle          0x400e1
#define Wimp_SendMessage       0x400e7
