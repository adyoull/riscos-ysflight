/* fssimplewindow for host profiling: no window, the game draws into an
   OSMesa buffer with riscos-mesa's patched classic swrast, as on the Pi
   (OSMESA_BGRA, 16-bit depth: what riscos-mesa's EGL gives the RISC OS
   build's EGL_OPENGL_BIT, depth-16 request). Linux host only; never part of
   the RISC OS build. tools/profile/README.md explains the use.

   Environment:
     PROF_SIZE=WxH      buffer size (default 640x480)
     PROF_SKIP=n        frames before measuring starts (default 300)
     PROF_FRAMES=n      frames measured, then a report and exit (default 300)
     PROF_PPM=file      write the last frame as a PPM
     PROF_SEED=n        srand(n) before the first frame (default 1), so the
                        weather and clouds are the same in every run
     PROF_KEYS=list     scripted keys, "frame:key:frames,..." (key = FSKEY_
                        number); the key is held for that many frames
   Under callgrind (--instr-atstart=no) instrumentation is switched on for
   the measured frames only.

   Part of the RISC OS port of YSFlight; BSD-3 like fssimplewindow. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <GL/osmesa.h>
#include <GL/gl.h>
#include <valgrind/callgrind.h>

#include "fssimplewindow.h"

static OSMesaContext ctx;
static std::vector<unsigned char> buf;
static int W=640,H=480;
static int windowOpen=0;
static long long frame=0,skip=300,measure=300;
static long long tStart=0;

static int keyState[FSKEY_NUM_KEYCODE];
static int keyBuf[256],nKey=0;
struct ScriptedKey { long long frame; int key; long long hold; bool pushed; };
static std::vector<ScriptedKey> script;

static long long RealUs(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC,&ts);
	return (long long)ts.tv_sec*1000000+ts.tv_nsec/1000;
}

/* PROF_FIXEDDT=ms: the game's clock moves that much per frame (plus 10 us
   per reading, so waiting loops still end), so a run under valgrind sees
   the same flight as a native one. */
static long long NowUs(void)
{
	static const char *fixed=getenv("PROF_FIXEDDT");
	static long long calls=0;
	if(NULL!=fixed)
	{
		return 1000000LL+frame*atoll(fixed)*1000+(calls++)*10;
	}
	return RealUs();
}

static void ReadScript(void)
{
	const char *s=getenv("PROF_KEYS");
	while(NULL!=s && 0!=*s)
	{
		ScriptedKey k;
		k.pushed=false;
		if(3==sscanf(s,"%lld:%d:%lld",&k.frame,&k.key,&k.hold))
		{
			script.push_back(k);
		}
		s=strchr(s,',');
		if(NULL!=s) s++;
	}
}

static void WritePPM(const char *fn)
{
	FILE *fp=fopen(fn,"wb");
	if(NULL==fp) return;
	fprintf(fp,"P6\n%d %d\n255\n",W,H);
	for(int y=H-1; y>=0; y--)
	{
		const unsigned char *row=buf.data()+(size_t)y*W*4;
		for(int x=0; x<W; x++)
		{
			unsigned char rgb[3]={row[x*4+2],row[x*4+1],row[x*4]};   /* BGRA */
			fwrite(rgb,1,3,fp);
		}
	}
	fclose(fp);
}

void FsOpenWindow(const FsOpenWindowOption &opt)
{
	const char *sz=getenv("PROF_SIZE");
	if(NULL!=sz) sscanf(sz,"%dx%d",&W,&H);
	if(NULL!=getenv("PROF_SKIP")) skip=atoll(getenv("PROF_SKIP"));
	if(NULL!=getenv("PROF_FRAMES")) measure=atoll(getenv("PROF_FRAMES"));
	ReadScript();

	if(NULL!=fsOpenGLContextCreationCallBack)
	{
		(*fsOpenGLContextCreationCallBack)(fsOpenGLContextCreationCallBackParam);
	}
	const int attr[]={OSMESA_FORMAT,OSMESA_BGRA,OSMESA_DEPTH_BITS,16,OSMESA_STENCIL_BITS,0,
	                  OSMESA_ACCUM_BITS,0,OSMESA_PROFILE,OSMESA_COMPAT_PROFILE,0};
	ctx=OSMesaCreateContextAttribs(attr,NULL);
	buf.resize((size_t)W*H*4);
	if(NULL==ctx || !OSMesaMakeCurrent(ctx,buf.data(),GL_UNSIGNED_BYTE,W,H))
	{
		fprintf(stderr,"OSMesa context failed\n");
		exit(1);
	}
	windowOpen=1;

	/* Same OpenGL state as the X11 and RISC OS versions */
	glClearColor(1.0F,1.0F,1.0F,0.0F);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glShadeModel(GL_SMOOTH);
	GLfloat dif[]={0.8F,0.8F,0.8F,1.0F};
	GLfloat amb[]={0.4F,0.4F,0.4F,1.0F};
	GLfloat spc[]={0.9F,0.9F,0.9F,1.0F};
	GLfloat shininess[]={50.0,50.0,50.0,0.0};
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0,GL_DIFFUSE,dif);
	glLightfv(GL_LIGHT0,GL_SPECULAR,spc);
	glMaterialfv(GL_FRONT|GL_BACK,GL_SHININESS,shininess);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT,amb);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_NORMALIZE);
	glClearDepth(1.0F);
	glDisable(GL_DEPTH_TEST);
	glViewport(0,0,W,H);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,(float)W-1,(float)H-1,0,-1,1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glShadeModel(GL_FLAT);
	glPointSize(1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glColor3ub(0,0,0);
	if(NULL==getenv("PROF_NICEST"))
	{
		glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_FASTEST);
	}

	if(NULL!=fsOpenGLInitializationCallBack)
	{
		(*fsOpenGLInitializationCallBack)(fsOpenGLInitializationCallBackParam);
	}
	if(NULL!=fsAfterWindowCreationCallBack)
	{
		(*fsAfterWindowCreationCallBack)(fsAfterWindowCreationCallBackParam);
	}
	if(NULL!=fsOnResizeCallBack)
	{
		(*fsOnResizeCallBack)(fsOnResizeCallBackParam,W,H);
	}
}

int FsCheckWindowOpen(void) { return windowOpen; }
void FsCloseWindow(void) { windowOpen=0; }
void FsMaximizeWindow(void) {}
void FsUnmaximizeWindow(void) {}
void FsMakeFullScreen(void) {}
void FsGetWindowSize(int &wid,int &hei) { wid=W; hei=H; }
void FsGetWindowPosition(int &x0,int &y0) { x0=0; y0=0; }
void FsSetWindowTitle(const char []) {}
void FsResizeWindow(int,int) {}
void FsSetMousePosition(int,int) {}

void FsPollDevice(void)
{
	for(auto &k : script)
	{
		if(0<k.key && k.key<FSKEY_NUM_KEYCODE)
		{
			if(frame>=k.frame && !k.pushed && nKey<256)
			{
				k.pushed=true;
				keyBuf[nKey++]=k.key;
			}
			keyState[k.key]=(k.frame<=frame && frame<k.frame+k.hold);
		}
	}
	if(NULL!=fsPollDeviceHook)
	{
		(*fsPollDeviceHook)(fsPollDeviceHookParam);
	}
}

void FsPushOnPaintEvent(void) {}
void FsSleep(int ms) { if(0<ms) usleep(ms*1000); }
long long int FsPassedTime(void)
{
	static long long last=0;
	long long now=NowUs()/1000,d=(0==last ? 0 : now-last);
	last=now;
	return d;
}
long long int FsSubSecondTimer(void) { return NowUs()/1000; }

int FsInkey(void)
{
	if(0<nKey)
	{
		int k=keyBuf[0];
		memmove(keyBuf,keyBuf+1,(--nKey)*sizeof(int));
		return k;
	}
	return 0;
}
int FsInkeyChar(void) { return 0; }
void FsPushKey(int fskey) { if(nKey<256) keyBuf[nKey++]=fskey; }
void FsPushChar(int) {}
int FsGetKeyState(int k) { return (0<=k && k<FSKEY_NUM_KEYCODE) ? keyState[k] : 0; }
int FsCheckWindowExposure(void) { return 0; }
void FsGetMouseState(int &lb,int &mb,int &rb,int &mx,int &my) { lb=mb=rb=0; mx=W/2; my=H/2; }
int FsGetMouseEvent(int &lb,int &mb,int &rb,int &mx,int &my)
{
	FsGetMouseState(lb,mb,rb,mx,my);
	return FSMOUSEEVENT_NONE;
}

void FsSwapBuffers(void)
{
	if(NULL!=fsSwapBuffersHook && true==(*fsSwapBuffersHook)(fsSwapBuffersHookParam))
	{
		return;
	}
	glFinish();
	if(0==frame)
	{
		srand(NULL!=getenv("PROF_SEED") ? atoi(getenv("PROF_SEED")) : 1);
	}
	frame++;
	if(NULL!=getenv("PROF_VERBOSE"))
	{
		static long long tLast=0;
		const long long t=RealUs();
		fprintf(stderr,"PROF: frame %lld %lld ms\n",frame,0==tLast ? 0 : (t-tLast)/1000);
		tLast=t;
	}
	if(frame==skip)
	{
		tStart=RealUs();
		CALLGRIND_START_INSTRUMENTATION;
		fprintf(stderr,"PROF: measuring from frame %lld\n",frame);
	}
	else if(frame==skip+measure)
	{
		CALLGRIND_STOP_INSTRUMENTATION;
		const long long us=RealUs()-tStart;
		fprintf(stderr,"PROF: %lld frames in %lld ms = %.2f ms/frame (%.1f fps) at %dx%d\n",
		        measure,us/1000,(double)us/1000.0/measure,measure*1e6/us,W,H);
		if(NULL!=getenv("PROF_PPM"))
		{
			WritePPM(getenv("PROF_PPM"));
		}
		_exit(0);
	}
}

void FsChangeToProgramDir(void) {}
int FsGetNumCurrentTouch(void) { return 0; }
const FsVec2i *FsGetCurrentTouch(void) { return nullptr; }
void FsX11GetClipBoardString(long long int &len,char *&str) { len=0; str=nullptr; }
void FsX11SetClipBoardString(long long int,const char []) {}
int FsEnableIME(void) { return 0; }
void FsDisableIME(void) {}
int FsIsNativeTextInputAvailable(void) { return 0; }
int FsOpenNativeTextInput(int,int,int,int) { return 0; }
void FsCloseNativeTextInput(void) {}
void FsSetNativeTextInputText(const wchar_t []) {}
int FsGetNativeTextInputTextLength(void) { return 0; }
void FsGetNativeTextInputText(wchar_t str[],int bufLen) { if(0<bufLen) str[0]=0; }
int FsGetNativeTextInputEvent(void) { return FSNATIVETEXTEVENT_NONE; }
