/* Decodes PNG files with YsRawPngDecoder and prints a hash of each image
   (size and RGBA after Flip). Built twice by run.sh: with YsPngUncompressor
   and with YS_PNG_ZLIB; the two outputs must be identical. */
#include <stdio.h>
#include <stdint.h>
#include "yspng.h"

int main(int ac,char *av[])
{
	for(int i=1; i<ac; ++i)
	{
		YsRawPngDecoder dec;
		dec.Decode(av[i]);
		dec.Flip();
		uint64_t h=1469598103934665603ULL;
		for(long long k=0; k<(long long)dec.wid*dec.hei*4; ++k)
		{
			h=(h^dec.rgba[k])*1099511628211ULL;
		}
		printf("%s %dx%d %016llx\n",av[i],dec.wid,dec.hei,(unsigned long long)h);
	}
	return 0;
}
