/* Host test of the RISC OS sound back end (platform/riscos/fsairsoundriscos.cpp)
   against a fake /dev/dsp that plays RATE samples a second of a virtual
   clock. Run from the runtime directory (it loads the game's WAV files). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <vector>
#include <sys/soundcard.h>

#include <fsairsound.h>
#include <yswavfile.h>

static int fails=0,checks=0;
#define CHECK(c) do{ ++checks; if(!(c)){ ++fails; printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c); } }while(0)

static const int RATE=22050,TARGET=RATE*2*160/1000;

// ---- fake /dev/dsp ----
static bool failOpen=false;
static int opens=0,openFlags=0,fmt=0,chans=0,speed=0,frag=0,resets=0,closes=0;
static bool isOpen=false;
static double clockMs=0.0;              // virtual time
static long long played=0;              // bytes played (by the clock) since open
static std::vector<short> out;          // every sample written, in order
static long long writtenSinceOpen=0;
static int underruns=0,maxQueued=0;
static int spaceLimit=-1;               // a nearly full device: room it reports and takes
static int overOffered=0;               // writes of more than there was room for
static double openedAt=0.0;
static bool started=false;

static void Advance(double ms)
{
	clockMs+=ms;
	if(!isOpen || !started) return;
	long long canPlay=(long long)((clockMs-openedAt)*RATE*2/1000.0)&~1LL;
	if(canPlay>writtenSinceOpen)
	{
		++underruns;
		openedAt=clockMs-writtenSinceOpen*1000.0/(RATE*2);   // the stream waits for more
		canPlay=writtenSinceOpen;
	}
	played=canPlay;
}

extern "C" int fake_dsp_open(const char *path,int flags)
{
	++opens;
	if(failOpen) { errno=ENODEV; return -1; }
	CHECK(0==strcmp(path,"/dev/dsp"));
	openFlags=flags; isOpen=true; played=0; writtenSinceOpen=0; started=false;
	return 7;
}
extern "C" int fake_dsp_ioctl(int fd,unsigned long req,void *arg)
{
	CHECK(7==fd && isOpen);
	int *ip=(int *)arg;
	switch(req)
	{
	case SNDCTL_DSP_SETFRAGMENT: frag=*ip; return 0;
	case SNDCTL_DSP_SETFMT: fmt=*ip; return 0;
	case SNDCTL_DSP_CHANNELS: chans=*ip; return 0;
	case SNDCTL_DSP_SPEED: speed=*ip; return 0;
	case SNDCTL_DSP_RESET: ++resets; played=writtenSinceOpen=0; started=false; return 0;
	case SNDCTL_DSP_GETODELAY: *ip=(int)(writtenSinceOpen-played); return 0;
	case SNDCTL_DSP_GETOSPACE:
		{
			audio_buf_info *b=(audio_buf_info *)arg;
			b->fragstotal=16; b->fragsize=2048;
			b->bytes=16*2048-(int)(writtenSinceOpen-played);
			if(0<=spaceLimit && spaceLimit<b->bytes) b->bytes=spaceLimit;
			b->fragments=b->bytes/2048;
			return 0;
		}
	}
	CHECK(!"unexpected ioctl");
	return -1;
}
extern "C" int fake_dsp_write(int fd,const void *buf,int n)
{
	CHECK(7==fd && isOpen);
	CHECK(0==(n&1));
	int room=16*2048-(int)(writtenSinceOpen-played);
	if(0<=spaceLimit && spaceLimit<room) room=spaceLimit;
	if(n>room) ++overOffered;
	if(room<=0) { errno=EAGAIN; return -1; }
	if(n>room) n=room&~1;
	const short *s=(const short *)buf;
	out.insert(out.end(),s,s+n/2);
	writtenSinceOpen+=n;
	if(!started) { started=true; openedAt=clockMs; }
	int q=(int)(writtenSinceOpen-played);
	if(q>maxQueued) maxQueued=q;
	return n;
}
extern "C" int fake_dsp_close(int fd)
{
	CHECK(7==fd && isOpen);
	isOpen=false; ++closes;
	return 0;
}

// ---- reference sounds, loaded the same way ----
static void Ref(YsWavFile &w,const char *fn)
{
	CHECK(YSOK==w.LoadWav(fn));
	w.ConvertToMono(); w.ConvertTo16Bit(); w.ConvertToSigned(); w.Resample(RATE);
}
static short At(const YsWavFile &w,unsigned int i)
{
	return ((const short *)w.DataPointer())[i%w.NTimeStep()];
}

static void Frames(int n,double ms)
{
	for(int i=0; i<n; ++i) { Advance(ms); FsSoundKeepPlaying(); }
}

int main(void)
{
	YsWavFile jet5,jet9,bang,gun;
	Ref(jet5,"sound/engine5.wav"); Ref(jet9,"sound/engine9.wav");
	Ref(bang,"sound/bang.wav"); Ref(gun,"sound/gun.wav");

	FsSoundInitialize();

	// Nothing to play: the device isn't opened.
	Frames(10,45);
	CHECK(0==opens);

	// Engine at power 0.55 (level 5): opened non-blocking, 16-bit mono 22050.
	FsSoundSetEngine(FSSND_ENGINE_JETNORMAL,1,0.55);
	Frames(1,45);
	CHECK(1==opens);
	CHECK(0!=(openFlags&O_NONBLOCK) && O_WRONLY==(openFlags&O_ACCMODE));
	CHECK(AFMT_S16_LE==fmt && 1==chans && RATE==speed);
	CHECK(((16<<16)|11)==frag);
	CHECK((int)out.size()*2==TARGET-(TARGET&1));

	// 200 frames of 45 ms and some slow ones of 120 ms: never runs dry,
	// never more than TARGET queued; the output is the loop, unbroken.
	Frames(200,45);
	Frames(20,120);
	Frames(50,45);
	CHECK(0==underruns);
	CHECK(maxQueued<=TARGET);
	bool same=true;
	for(size_t i=0; i<out.size(); ++i) if(out[i]!=At(jet5,i)) { same=false; break; }
	CHECK(same);

	// A 300 ms gap (longer than what's queued) runs dry once, then recovers.
	Frames(1,300);
	CHECK(1==underruns);
	Frames(20,45);
	CHECK(1==underruns);

	// A one-shot is mixed in from the next write, once, and ends.
	size_t before=out.size();
	unsigned int envPos=(unsigned int)before;   // engine position = samples so far
	FsSoundSetOneTime(FSSND_ONETIME_BANG);
	Frames(40,45);
	bool mixed=true;
	for(size_t i=before; i<out.size(); ++i)
	{
		int v=At(jet5,envPos+(unsigned int)(i-before));
		if(i-before<bang.NTimeStep()) v+=((const short *)bang.DataPointer())[i-before];
		v=(v>32767 ? 32767 : (v<-32768 ? -32768 : v));
		if(out[i]!=v) { mixed=false; printf("  mix differs at %d\n",(int)(i-before)); break; }
	}
	CHECK(mixed);

	// Full power (level 9): same place in the loop, new sound.
	before=out.size();
	FsSoundSetEngine(FSSND_ENGINE_JETNORMAL,1,0.95);
	Frames(1,45);
	CHECK(out.size()>before);
	CHECK(out[before]==At(jet9,(unsigned int)(before%jet5.NTimeStep())));

	// The gun overrides the engine while firing.
	FsSoundSetMachineGun(FSSND_MACHINEGUN_MACHINEGUN);
	Frames(2,45);
	before=out.size();
	Frames(1,45);
	CHECK(out.size()>before);

	// A device with little room: no more than GETOSPACE says is ever
	// offered, so nothing is cut, and the stream keeps going.
	FsSoundSetMachineGun(FSSND_MACHINEGUN_SILENT);
	spaceLimit=600;
	Frames(30,45);
	spaceLimit=-1;
	Frames(30,45);
	CHECK(0==overOffered);
	CHECK(maxQueued<=TARGET);

	// StopAll drops the queue and closes; nothing reopens it while silent.
	FsSoundStopAll();
	CHECK(1<=resets && 1==closes && !isOpen);
	Frames(10,45);
	CHECK(1==opens);

	// One-shots alone open it again.
	FsSoundSetOneTime(FSSND_ONETIME_TOUCHDWN);
	Frames(1,45);
	CHECK(2==opens && isOpen);

	// Master switch off: closed, and quiet.
	FsSoundSetMasterSwitch(YSFALSE);
	CHECK(!isOpen);
	FsSoundSetEngine(FSSND_ENGINE_JETNORMAL,1,0.5);
	size_t n=out.size();
	Frames(10,45);
	CHECK(out.size()==n && 2==opens);
	FsSoundSetMasterSwitch(YSTRUE);

	// No /dev/dsp: reported once, tried again only after 200 frames.
	FsSoundStopAll();
	failOpen=true;
	FsSoundSetEngine(FSSND_ENGINE_JETNORMAL,1,0.5);
	Frames(1,45);
	int o=opens;
	Frames(100,45);
	CHECK(o==opens);
	failOpen=false;
	Frames(150,45);
	CHECK(o+1==opens && isOpen);

	// Flap sounds have no WAV: ignored.
	FsSoundSetOneTime(FSSND_ONETIME_FLAPUP);
	FsSoundSetOneTime((FSSND_ONETIMETYPE)999);
	Frames(5,45);

	FsSoundTerminate();
	CHECK(!isOpen);

	printf("%d checks, %d failed\n",checks,fails);
	return fails ? 1 : 0;
}
