/* Host test of ysjoystick/src/riscos/ysjoyreader.cpp against a fake
   Joystick module: USBJoystick's slot API (records laid out as in its
   h.joyswis), the Acorn Joystick_Read only, and no module at all. -m32,
   since RISC OS keeps pointers in int registers. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <kernel.h>
#include "ysjoyreader.h"

static int fails=0,checks=0;
#define CHECK(c) do{ ++checks; if(!(c)){ ++fails; printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c); } }while(0)

/* USBJoystick records (h.joyswis) */
struct joystick_device_info { uint32_t size; char *manufacturer,*product,*serial; uint32_t vendor_id,product_id,num_axes,num_buttons,num_hats,capabilities; uint8_t guid[16]; };
struct joystick_axis_info { uint32_t hid_usage_page,hid_usage,type; char *name; int32_t min,mid,max; int is_digital; };

static int mode=0;   // 0 none, 1 classic, 2 native, 3 native with no devices
static _kernel_oserror err={0x1E6,"SWI not known"};
static joystick_device_info dev;
static joystick_axis_info axes[8];
static int32_t values[8];
static uint32_t buttons;
static int classicPos,classicBtn;
static int unplugged=0;
static char product[]="Test Stick";

// types: RZ, X, HATX, HATY, Y, SLIDER, Z ; order deliberately mixed
static const uint32_t types[7]={10,5,1,2,6,11,7};

extern "C" _kernel_oserror *_kernel_swi(int no,_kernel_swi_regs *in,_kernel_swi_regs *out)
{
	if(0==mode) return &err;
	if(0x43F40==no)
	{
		out->r[0]=classicPos; out->r[1]=classicBtn; return NULL;
	}
	if(1==mode) return &err;   // classic module: no native SWIs
	switch(no)
	{
	case 0x43F44: out->r[0]=(3==mode ? 0 : 0x4); out->r[1]=7; return NULL;   // slot 2
	case 0x43F45:
		if(2!=in->r[0] || unplugged) return &err;
		out->r[0]=0; out->r[1]=sizeof(dev); out->r[2]=(int)(intptr_t)&dev; return NULL;
	case 0x43F46:
		if(2!=in->r[0] || unplugged) return &err;
		out->r[0]=0; out->r[1]=sizeof(joystick_axis_info); out->r[2]=7; out->r[3]=(int)(intptr_t)axes; return NULL;
	case 0x43F47:
		if(2!=in->r[0] || unplugged) return &err;
		out->r[0]=0; out->r[1]=4; out->r[2]=7; out->r[3]=(int)(intptr_t)values; out->r[4]=buttons; return NULL;
	}
	return &err;
}

int main(void)
{
	YsJoyReader joy[8];
	int n;

	// No Joystick module
	mode=0;
	YsJoyReaderSetUpJoystick(n,joy,8);
	CHECK(0==n);

	// Native API, nothing attached
	mode=3;
	YsJoyReaderSetUpJoystick(n,joy,8);
	CHECK(0==n);

	// Native API, one device in slot 2
	mode=2;
	memset(&dev,0,sizeof(dev));
	dev.size=sizeof(dev); dev.product=product; dev.num_axes=7; dev.num_buttons=12; dev.num_hats=1;
	for(int i=0; i<7; ++i)
	{
		axes[i].type=types[i];
		axes[i].is_digital=(1==types[i] || 2==types[i]);
		axes[i].min=(axes[i].is_digital ? -1 : 0);
		axes[i].mid=(axes[i].is_digital ? 0 : 512);
		axes[i].max=(axes[i].is_digital ? 1 : 1023);
		values[i]=100+i;
	}
	values[2]=1; values[3]=-1;   // hat up-right
	buttons=0x805;               // buttons 0, 2, 11
	YsJoyReaderSetUpJoystick(n,joy,8);
	CHECK(1==n);
	CHECK(0==strcmp(joy[0].name,"Test Stick"));
	// axis order X, Y, Z, RZ, (RX, RY absent), slider
	CHECK(joy[0].axis[0].exist && 101==joy[0].axis[0].value);   // X  = module axis 1
	CHECK(joy[0].axis[1].exist && 104==joy[0].axis[1].value);   // Y  = module axis 4
	CHECK(joy[0].axis[2].exist && 106==joy[0].axis[2].value);   // Z  = module axis 6
	CHECK(joy[0].axis[3].exist && 100==joy[0].axis[3].value);   // RZ = module axis 0
	CHECK(joy[0].axis[4].exist && 105==joy[0].axis[4].value);   // slider = module axis 5
	CHECK(0==joy[0].axis[5].exist);
	CHECK(0==joy[0].axis[0].calibMin && 1023==joy[0].axis[0].calibMax && 512==joy[0].axis[0].calibCenter);
	CHECK(joy[0].button[0].exist && joy[0].button[11].exist && 0==joy[0].button[12].exist);
	CHECK(1==joy[0].button[0].value && 0==joy[0].button[1].value && 1==joy[0].button[2].value && 1==joy[0].button[11].value);
	CHECK(joy[0].hatSwitch[0].exist && 2==joy[0].hatSwitch[0].value && 2==joy[0].hatSwitch[0].GetDiscreteValue());
	// hat directions: up 1, right 3, down 5, left 7, centre 0
	int hx[]={0,1,0,-1,0}, hy[]={-1,0,1,0,0}, hv[]={1,3,5,7,0};
	for(int i=0; i<5; ++i)
	{
		values[2]=hx[i]; values[3]=hy[i];
		joy[0].Read();
		CHECK(hv[i]==joy[0].hatSwitch[0].value);
	}
	// calibrated value: max -> +1, min -> -1, mid -> 0
	values[1]=1023; joy[0].Read(); CHECK(1.0==joy[0].axis[0].GetCalibratedValue());
	values[1]=0;    joy[0].Read(); CHECK(-1.0==joy[0].axis[0].GetCalibratedValue());
	values[1]=512;  joy[0].Read(); CHECK(0.0==joy[0].axis[0].GetCalibratedValue());
	// unplugged: centred, released
	unplugged=1; buttons=0xFFF; joy[0].Read(); unplugged=0;
	CHECK(512==joy[0].axis[0].value && 0==joy[0].button[0].value && 0==joy[0].hatSwitch[0].value);

	// Classic Joystick_Read only
	mode=1;
	YsJoyReader joy2[8];
	classicPos=(0x1234<<16)|0xFFFF;   // X 0x1234, Y top
	classicBtn=0x3;
	YsJoyReaderSetUpJoystick(n,joy2,8);
	CHECK(1==n);
	CHECK(joy2[0].axis[0].exist && joy2[0].axis[1].exist && 0==joy2[0].axis[2].exist);
	CHECK(0x1234==joy2[0].axis[0].value);
	CHECK(0==joy2[0].axis[1].value);                 // top -> smallest
	CHECK(-1.0==joy2[0].axis[1].GetCalibratedValue());
	CHECK(1==joy2[0].button[0].value && 1==joy2[0].button[1].value && 0==joy2[0].button[2].value);
	CHECK(0==joy2[0].hatSwitch[0].exist);

	printf("ysjoyreader RISC OS: %d checks, %d failed\n",checks,fails);
	return 0!=fails;
}
