/* Host test of fssimplewindow's RISC OS back end (fsriscoswrapper.cpp),
   against riscos-crossdev's fake RISC OS plus a scripted Wimp, keyboard and
   pointer, and stub EGL/GL calls. Run by run.sh. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <map>

extern "C" {
#include <rcdfake/fake.h>
}
#include "fakeplus/swis.h"
#include "fakeplus/kernel.h"

#include "fssimplewindow.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_riscos.h>

/* -- The fake desktop -------------------------------------------------- */

struct Event { int reason; int block[64]; };
static std::vector<Event> events;           /* Wimp_Poll script */
static int xeig=1,yeig=1,scrW=1920,scrH=1080;
static unsigned char keys[128];             /* internal keys held */
static int ptrX,ptrY,ptrButtons,ptrWindow=-1;
static int wheel;
static int nextHandle=100;
static std::map<int,std::vector<int>> windows;   /* handle -> CreateWindow block */
static std::map<int,int> visibleArea[4];
static std::map<int,int> isOpen;
static char taskName[64];
static int caretWindow,processedKey,sentMessageReason,sentMessageTo;
static int pollIdleCalls,pollCalls;
static int lastCreatedFlags;

static void Visible(int h,int v[4])
{
	for(int i=0; i<4; i++) v[i]=visibleArea[i][h];
}

static const _kernel_oserror *Swi(int swi,int r[10])
{
	int *b;
	switch(swi)
	{
	case Wimp_Initialise:
		snprintf(taskName,sizeof(taskName),"%s",(const char *)r[2]);
		r[0]=380; r[1]=0x1234;
		return NULL;
	case Wimp_CloseDown:
		return NULL;
	case Wimp_CreateWindow:
		b=(int *)r[1];
		windows[nextHandle]=std::vector<int>(b,b+23);
		lastCreatedFlags=b[7];
		r[0]=nextHandle++;
		return NULL;
	case Wimp_DeleteWindow:
		b=(int *)r[1];
		windows.erase(b[0]);
		isOpen.erase(b[0]);
		return NULL;
	case Wimp_OpenWindow:
		b=(int *)r[1];
		for(int i=0; i<4; i++) visibleArea[i][b[0]]=b[1+i];
		isOpen[b[0]]=1;
		return NULL;
	case Wimp_GetWindowState:
		b=(int *)r[1];
		for(int i=0; i<4; i++) b[1+i]=visibleArea[i][b[0]];
		b[5]=b[6]=0; b[7]=-1; b[8]=0;
		return NULL;
	case Wimp_SetCaretPosition:
		caretWindow=r[0];
		return NULL;
	case Wimp_ForceRedraw:
	case Wimp_RedrawWindow:
	case Wimp_GetRectangle:
		r[0]=0;
		return NULL;
	case Wimp_ProcessKey:
		processedKey=r[0];
		return NULL;
	case Wimp_SendMessage:
		sentMessageReason=r[0];
		sentMessageTo=r[2];
		return NULL;
	case Wimp_Poll:
	case Wimp_PollIdle:
		if(Wimp_Poll==swi) pollCalls++; else pollIdleCalls++;
		b=(int *)r[1];
		if(events.empty())
		{
			r[0]=0;
			return NULL;
		}
		r[0]=events[0].reason;
		memcpy(b,events[0].block,sizeof(events[0].block));
		events.erase(events.begin());
		return NULL;
	case Wimp_GetPointerInfo:
		b=(int *)r[1];
		b[0]=ptrX; b[1]=ptrY; b[2]=ptrButtons; b[3]=ptrWindow; b[4]=-1;
		return NULL;
	case OS_Pointer:
		if(2==r[0]) { r[0]=0; r[1]=wheel; }
		return NULL;
	case OS_ReadModeVariable:
		switch(r[1])
		{
		case 4: r[2]=xeig; break;
		case 5: r[2]=yeig; break;
		case 11: r[2]=scrW-1; break;
		case 12: r[2]=scrH-1; break;
		default: r[2]=0; break;
		}
		return NULL;
	case OS_Byte:
		if(121==r[0])
		{
			int x=r[1];
			/* As the real keyboard: Shift, Ctrl, Alt (0-2) are down when
			   either side's key is (3/6, 4/7, 5/8) */
			keys[0]=keys[3]|keys[6];
			keys[1]=keys[4]|keys[7];
			keys[2]=keys[5]|keys[8];
			if(x&0x80)
			{
				r[1]=keys[x^0x80] ? 0xFF : 0;
			}
			else
			{
				r[1]=0xFF;
				for(int k=x; k<128; k++) if(keys[k]) { r[1]=k; break; }
			}
		}
		return NULL;
	case OS_Word:
		return NULL;
	case 0x43380:                           /* TaskWindow_TaskInfo */
		r[0]=0;
		return NULL;
	}
	return NULL;
}

extern "C" int _kernel_osbyte(int op,int x,int y)
{
	_kernel_swi_regs r;
	r.r[0]=op; r.r[1]=x; r.r[2]=y;
	_kernel_swi(OS_Byte,&r,&r);
	return (r.r[1]&0xFF)|((r.r[2]&0xFF)<<8);
}

extern "C" int _kernel_osword(int op,int *data)
{
	_kernel_swi_regs r;
	r.r[0]=op; r.r[1]=(int)data;
	_kernel_swi(OS_Word,&r,&r);
	return 0;
}

static void Setup(void)
{
	static const int list[]={Wimp_Initialise,Wimp_CloseDown,Wimp_CreateWindow,Wimp_DeleteWindow,
		Wimp_OpenWindow,Wimp_GetWindowState,Wimp_SetCaretPosition,Wimp_ForceRedraw,
		Wimp_RedrawWindow,Wimp_GetRectangle,Wimp_ProcessKey,Wimp_SendMessage,Wimp_Poll,
		Wimp_PollIdle,Wimp_GetPointerInfo,OS_Pointer,OS_ReadModeVariable,OS_Byte,OS_Word,0x43380};
	fake_reset();
	for(int s : list) fake_swi(s,NULL,(fake_swi_fn *)Swi);
}

/* -- Stub EGL and GL ----------------------------------------------------- */

static int surfaceHandle,surfacesCreated,surfaceRenderW,surfaceRenderH,swaps,configAskedOpenGL;
static int overlayChecks;

extern "C" {
EGLDisplay eglGetDisplay(EGLNativeDisplayType) { return (EGLDisplay)1; }
EGLBoolean eglInitialize(EGLDisplay,EGLint *a,EGLint *b) { *a=1; *b=4; return EGL_TRUE; }
EGLBoolean eglBindAPI(EGLenum) { return EGL_TRUE; }
EGLBoolean eglChooseConfig(EGLDisplay,const EGLint *a,EGLConfig *c,EGLint,EGLint *n)
{
	for(int i=0; a[i]!=EGL_NONE; i+=2)
		if(EGL_RENDERABLE_TYPE==a[i] && EGL_OPENGL_BIT==a[i+1]) configAskedOpenGL=1;
	*c=(EGLConfig)1; *n=1; return EGL_TRUE;
}
EGLContext eglCreateContext(EGLDisplay,EGLConfig,EGLContext,const EGLint *) { return (EGLContext)2; }
EGLSurface eglCreateWindowSurface(EGLDisplay,EGLConfig,EGLNativeWindowType w,const EGLint *a)
{
	surfaceHandle=(int)(long)w; surfacesCreated++; surfaceRenderW=surfaceRenderH=0;
	for(int i=0; a && a[i]!=EGL_NONE; i+=2)
	{
		if(EGL_RENDER_WIDTH_RISCOS==a[i]) surfaceRenderW=a[i+1];
		if(EGL_RENDER_HEIGHT_RISCOS==a[i]) surfaceRenderH=a[i+1];
	}
	return (EGLSurface)3;
}
EGLBoolean eglMakeCurrent(EGLDisplay,EGLSurface,EGLSurface,EGLContext) { return EGL_TRUE; }
EGLBoolean eglDestroySurface(EGLDisplay,EGLSurface) { return EGL_TRUE; }
EGLBoolean eglDestroyContext(EGLDisplay,EGLContext) { return EGL_TRUE; }
EGLBoolean eglTerminate(EGLDisplay) { return EGL_TRUE; }
EGLint eglGetError(void) { return EGL_SUCCESS; }
EGLBoolean eglSwapBuffers(EGLDisplay,EGLSurface) { swaps++; return EGL_TRUE; }
EGLBoolean eglQuerySurface(EGLDisplay,EGLSurface,EGLint a,EGLint *v) { *v=0; (void)a; return EGL_TRUE; }
EGLBoolean eglRedrawWindowRISCOS(EGLDisplay,int *) { return EGL_TRUE; }
EGLBoolean eglCheckOverlaysRISCOS(EGLDisplay) { overlayChecks++; return EGL_TRUE; }

void glClearColor(GLfloat,GLfloat,GLfloat,GLfloat) {}
void glClear(GLbitfield) {}
void glFlush(void) {}
void glEnable(GLenum) {}
void glDisable(GLenum) {}
void glDepthFunc(GLenum) {}
void glShadeModel(GLenum) {}
void glLightfv(GLenum,GLenum,const GLfloat *) {}
void glMaterialfv(GLenum,GLenum,const GLfloat *) {}
void glLightModelfv(GLenum,const GLfloat *) {}
void glClearDepth(GLclampd) {}
static int vpW,vpH,hintFastest;
void glViewport(GLint,GLint,GLsizei w,GLsizei h) { vpW=w; vpH=h; }
void glMatrixMode(GLenum) {}
void glLoadIdentity(void) {}
void glOrtho(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble) {}
void glPointSize(GLfloat) {}
void glColor3ub(GLubyte,GLubyte,GLubyte) {}
void glHint(GLenum t,GLenum m) { if(GL_PERSPECTIVE_CORRECTION_HINT==t && GL_FASTEST==m) hintFastest=1; }
}

/* -- Helpers ------------------------------------------------------------ */

static void Push(int reason,std::initializer_list<int> words)
{
	Event e;
	memset(&e,0,sizeof(e));
	e.reason=reason;
	int i=0;
	for(int w : words) e.block[i++]=w;
	events.push_back(e);
}

static int win;                             /* our window handle */

/* Pointer at program pixel (px,py) of our window, buttons */
static void PointerAt(int px,int py,int buttons)
{
	int v[4];
	Visible(win,v);
	ptrX=v[0]+(px<<xeig);
	ptrY=v[3]-1-(py<<yeig);
	ptrButtons=buttons;
	ptrWindow=win;
}

static int resizeW,resizeH,resizes,closeAnswer=1,closeAsked;
static void OnResize(void *,int w,int h) { resizeW=w; resizeH=h; resizes++; }
static bool OnClose(void *) { closeAsked++; return closeAnswer; }

static int MouseEvent(int &x,int &y,int &lb,int &mb,int &rb)
{
	return FsGetMouseEvent(lb,mb,rb,x,y);
}

int main(void)
{
	Setup();
	setenv("YSFlight$Dir","SDFS::ePic.$.Apps.!YSFlight",1);
	unsetenv("YSFlight$Size");
	unsetenv("YSFlight$Render");
	FsRegisterWindowResizeCallBack(OnResize,NULL);
	FsRegisterCloseWindowCallBack(OnClose,NULL);

	/* Open: 640x480 at 32,32 in a 1920x1080 mode, eig 1 */
	FsOpenWindowOption opt;
	opt.x0=32; opt.y0=64; opt.wid=640; opt.hei=480; opt.useDoubleBuffer=true;
	opt.windowTitle="YS FLIGHT";
	FsOpenWindow(opt);
	win=surfaceHandle;
	int v[4];
	Visible(win,v);
	CHECK(0==strcmp(taskName,"YSFlight"),"task name from <YSFlight$Dir>");
	CHECK((unsigned)windows[win][7]==0xAF000002u,"normal window flags");
	CHECK((windows[win][15]>>12&15)==3,"button type click");
	CHECK(v[0]==64 && v[2]==64+1280,"x in OS units (eig 1)");
	CHECK(v[3]==2160-128 && v[1]==2160-128-960,"y from the screen top, OS units");
	CHECK(1==configAskedOpenGL,"EGL config asks for OpenGL (not the ES default)");
	CHECK(caretWindow==win,"input focus taken");
	CHECK(1==hintFastest,"GL_FASTEST perspective hint");
	CHECK(vpW==640 && vpH==480,"viewport = window size");
	int w,h;
	FsGetWindowSize(w,h);
	CHECK(w==640 && h==480,"FsGetWindowSize");
	int x0,y0;
	FsGetWindowPosition(x0,y0);
	CHECK(x0==32 && y0==64,"FsGetWindowPosition");
	CHECK(1==FsCheckWindowOpen(),"window open");

	/* Gain the caret, press Q (internal key 16) */
	Push(12,{win,-1,0,0,0,0});
	FsPollDevice();
	keys[16]=1;
	Push(8,{win,-1,0,0,0,0,'q'});
	FsPollDevice();
	CHECK(FSKEY_Q==FsInkey(),"FsInkey Q");
	CHECK('q'==FsInkeyChar(),"FsInkeyChar q");
	CHECK(1==FsGetKeyState(FSKEY_Q),"Q held");
	/* Auto-repeat while held */
	Push(8,{win,-1,0,0,0,0,'q'});
	FsPollDevice();
	CHECK(FSKEY_Q==FsInkey(),"auto-repeat gives Q again");
	CHECK(1==FsGetKeyState(FSKEY_Q),"Q still held");
	keys[16]=0;
	FsPollDevice();
	CHECK(0==FsGetKeyState(FSKEY_Q),"Q released");
	CHECK(0==FsInkey(),"no more keys");

	/* A tap between two polls: Key_Pressed, but the key is already up */
	Push(8,{win,-1,0,0,0,0,'w'});
	FsPollDevice();
	CHECK(FSKEY_W==FsInkey(),"tap: FsInkey W (from the Wimp code)");
	CHECK(1==FsGetKeyState(FSKEY_W),"tap: W counts as held for one poll");
	FsPollDevice();
	CHECK(0==FsGetKeyState(FSKEY_W),"tap: then released");

	/* Held key with no Key_Pressed (e.g. Shift, arrow) */
	keys[3]=1;                              /* left Shift */
	keys[57]=1;                             /* Up */
	FsPollDevice();
	CHECK(1==FsGetKeyState(FSKEY_SHIFT) && 1==FsGetKeyState(FSKEY_LEFT_SHIFT),"Shift held (both codes)");
	CHECK(1==FsGetKeyState(FSKEY_UP),"Up held");
	keys[3]=keys[57]=0;
	/* Losing the focus: nothing held */
	keys[33]=1;                             /* W down, but the window lost the caret */
	Push(11,{win,-1,0,0,0,0});
	FsPollDevice();
	CHECK(0==FsGetKeyState(FSKEY_W),"no keys without the focus");
	keys[33]=0;
	Push(12,{win,-1,0,0,0,0});
	FsPollDevice();

	while(0!=FsInkeyChar()) {}
	while(0!=FsInkey()) {}
	/* Ctrl-A: no character, but the key */
	keys[4]=keys[65]=1;
	Push(8,{win,-1,0,0,0,0,1});
	FsPollDevice();
	CHECK(FSKEY_A==FsInkey(),"Ctrl-A gives FSKEY_A");
	CHECK(0==FsInkeyChar(),"Ctrl-A gives no character");
	keys[4]=keys[65]=0;
	FsPollDevice();

	/* A key we don't use goes back to the Wimp */
	processedKey=0;
	Push(8,{win,-1,0,0,0,0,0x1FF});
	FsPollDevice();
	CHECK(0x1FF==processedKey,"unknown key passed on (Wimp_ProcessKey)");

	/* Mouse: click (Select) at pixel 100,50; released by the next poll */
	int x,y,lb,mb,rb,e;
	PointerAt(100,50,0);
	FsPollDevice();
	while(FSMOUSEEVENT_NONE!=MouseEvent(x,y,lb,mb,rb)) {}
	Visible(win,v);
	Push(6,{v[0]+(100<<xeig),v[3]-1-(50<<yeig),4,win,-1});
	PointerAt(150,60,0);                    /* already up and moved at poll time */
	FsPollDevice();
	e=MouseEvent(x,y,lb,mb,rb);
	CHECK(FSMOUSEEVENT_LBUTTONDOWN==e && 100==x && 50==y && 1==lb,"press at the click's position");
	e=MouseEvent(x,y,lb,mb,rb);
	CHECK(FSMOUSEEVENT_LBUTTONUP==e && 0==lb,"release seen on the same poll");
	e=MouseEvent(x,y,lb,mb,rb);
	CHECK(FSMOUSEEVENT_MOVE==e && 150==x && 60==y,"then the movement");
	CHECK(FSMOUSEEVENT_NONE==MouseEvent(x,y,lb,mb,rb),"no more mouse events");
	/* Adjust held, drag, release */
	Push(6,{v[0]+(10<<xeig),v[3]-1-(10<<yeig),1,win,-1});
	PointerAt(10,10,1);
	FsPollDevice();
	e=MouseEvent(x,y,lb,mb,rb);
	CHECK(FSMOUSEEVENT_RBUTTONDOWN==e && 1==rb,"Adjust = right button");
	PointerAt(20,30,1);
	FsPollDevice();
	e=MouseEvent(x,y,lb,mb,rb);
	CHECK(FSMOUSEEVENT_MOVE==e && 20==x && 30==y && 1==rb,"drag with Adjust held");
	FsGetMouseState(lb,mb,rb,x,y);
	CHECK(1==rb && 20==x && 30==y,"FsGetMouseState while dragging");
	PointerAt(20,30,0);
	FsPollDevice();
	e=MouseEvent(x,y,lb,mb,rb);
	CHECK(FSMOUSEEVENT_RBUTTONUP==e,"Adjust released");
	/* Menu = middle */
	Push(6,{v[0]+20,v[3]-21,2,win,-1});
	FsPollDevice();
	e=MouseEvent(x,y,lb,mb,rb);
	CHECK(FSMOUSEEVENT_MBUTTONDOWN==e,"Menu = middle button");
	FsPollDevice();
	while(FSMOUSEEVENT_NONE!=MouseEvent(x,y,lb,mb,rb)) {}

	/* Wheel */
	wheel+=2;
	FsPollDevice();
	CHECK(FSKEY_WHEELUP==FsInkey() && FSKEY_WHEELUP==FsInkey(),"wheel up twice");
	CHECK(1==FsGetKeyState(FSKEY_WHEELUP),"wheel counts as a key for one poll");
	wheel-=1;
	FsPollDevice();
	CHECK(FSKEY_WHEELDOWN==FsInkey(),"wheel down");
	CHECK(0==FsGetKeyState(FSKEY_WHEELUP),"wheel up released");

	/* The user resizes the window */
	resizes=0;
	Push(2,{win,v[0],v[3]-800,v[0]+1600,v[3],0,0,-1});
	FsPollDevice();
	CHECK(1==resizes && 800==resizeW && 400==resizeH,"resize callback with the new pixel size");
	CHECK(1==FsCheckWindowExposure() && 0==FsCheckWindowExposure(),"exposure after a resize, once");

	/* Full screen and back */
	int before[4];
	Visible(win,before);
	FsMakeFullScreen();
	int fsw=surfaceHandle;
	CHECK(fsw!=win && windows.count(win)==0,"full screen: new window, old one deleted");
	CHECK((unsigned)lastCreatedFlags==0x80000000u,"full screen: borderless");
	Visible(fsw,v);
	CHECK(v[0]==0 && v[1]==0 && v[2]==3840 && v[3]==2160,"full screen covers the screen");
	FsGetWindowSize(w,h);
	CHECK(1920==w && 1080==h,"full screen size in pixels");
	CHECK(caretWindow==fsw,"full screen window has the focus");
	win=fsw;
	FsUnmaximizeWindow();
	win=surfaceHandle;
	Visible(win,v);
	CHECK(0==memcmp(v,before,sizeof(v)),"back to the same window position and size");
	CHECK((unsigned)lastCreatedFlags==0xAF000002u,"back to a normal window");

	/* Close icon: the program says no, then yes */
	closeAnswer=0;
	Push(3,{win});
	FsPollDevice();
	CHECK(1==closeAsked && 1==FsCheckWindowOpen(),"close refused by the program");
	closeAnswer=1;
	Push(3,{win});
	FsPollDevice();
	CHECK(2==closeAsked && 0==FsCheckWindowOpen(),"close accepted");

	/* FsSleep waits in Wimp_PollIdle, not busy */
	pollIdleCalls=0;
	FsSleep(40);
	CHECK(0<pollIdleCalls,"FsSleep uses Wimp_PollIdle");

	/* Desktop shutdown: PreQuit objected to, then restarted at close */
	Push(18,{24,0x777,55,0,8,0});           /* size, sender, my_ref, your_ref, PreQuit, flags 0 */
	FsPollDevice();
	CHECK(19==sentMessageReason && 0x777==sentMessageTo,"PreQuit acknowledged (objected)");
	processedKey=0;
	FsCloseWindow();
	CHECK(0x1FC==processedKey,"shutdown restarted with Ctrl-Shift-F12");

	/* EX0 EY0, render size */
	Setup();
	xeig=yeig=0; scrW=3840; scrH=2160;
	setenv("YSFlight$Render","320x240",1);
	setenv("YSFlight$Size","800x600",1);
	opt.x0=100; opt.y0=100;
	FsOpenWindow(opt);
	win=surfaceHandle;
	Visible(win,v);
	CHECK(v[2]-v[0]==800 && v[3]-v[1]==600,"EX0 EY0: OS units = pixels; YSFlight$Size");
	CHECK(320==surfaceRenderW && 240==surfaceRenderH,"YSFlight$Render passed to EGL");
	FsGetWindowSize(w,h);
	CHECK(320==w && 240==h,"program sees the render size");
	CHECK(320==vpW && 240==vpH,"viewport = render size");
	Push(12,{win,-1,0,0,0,0});
	Push(6,{v[0]+400,v[3]-1-300,4,win,-1});
	PointerAt(400,300,4);
	FsPollDevice();
	e=MouseEvent(x,y,lb,mb,rb);
	CHECK(FSMOUSEEVENT_LBUTTONDOWN==e && 160==x && 120==y,"mouse scaled to the render size");
	/* Mode change message: resize */
	scrW=1280; scrH=720;
	Push(17,{20,0,0,0,0x400C1});
	FsPollDevice();
	CHECK(1==FsCheckWindowExposure(),"mode change: exposure");
	FsCloseWindow();

	return fake_done("fssimplewindow RISC OS back end");
}
