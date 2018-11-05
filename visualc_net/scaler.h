#define PSIZE 4
#define PTYPE Bit32u
#define WC scalerWriteCache.b32
//#define FC scalerFrameCache.b32
#define FC (*(scalerFrameCache_t*)(&scalerSourceCache.b32[400][0])).b32
#define redMask		0xff0000
#define greenMask	0x00ff00
#define blueMask	0x0000ff
#define redBits		8
#define greenBits	8
#define blueBits	8
#define redShift	16
#define greenShift	8
#define blueShift	0

#if defined (SCALERLINEAR)
static void conc4d(Normal1x, SBPP, DBPP, L)(const void *s)
{
#else
static void conc4d(Normal1x, SBPP, DBPP, R)(const void *s)
{
#endif
	/* Clear the complete line marker */
	Bitu hadChange = 0;
	const Bit8u *src = (Bit8u*)s;
	Bit8u *cache = (Bit8u*)(render.scale.cacheRead);
	render.scale.cacheRead += render.scale.cachePitch;
	Bit32u * line0 = (Bit32u *)(render.scale.outWrite);
#if (SBPP == 9)
	for(Bits x = render.src.width; x>0;)
	{
		if(*(Bit32u const*)src == *(Bit32u*)cache && !(
			render.pal.modified[src[0]] |
			render.pal.modified[src[1]] |
			render.pal.modified[src[2]] |
			render.pal.modified[src[3]]))
		{
			x -= 4;
			src += 4;
			cache += 4;
			line0 += 4 * 1;
		} 
		else 
		{
#else 
	for(Bits x = render.src.width; x>0;)
	{
		if(*(Bitu const*)src == *(Bitu*)cache)
		{
			x -= 4;
			src += 4;
			cache += 4;
			line0 += 4 * 1;
#endif
		}
		else
		{
			hadChange = 1;
			for(Bitu i = x > 32 ? 32 : x; i>0; i--, x--)
			{
				const Bit8u S = *src;
				*cache = S;
				src++; cache++;
				const Bit32u P = PMAKE(S);
				line0[0] = P;
				line0 += 1;
			}
		}
	}
#if defined(SCALERLINEAR) 
	Bitu scaleLines = 1;
#else
	Bitu scaleLines = Scaler_Aspect[render.scale.outLine++];
	if(scaleLines - 1 && hadChange)
	{
		BituMove(render.scale.outWrite + render.scale.outPitch * 1,
				 render.scale.outWrite + render.scale.outPitch * (1 - 1),
				 render.src.width * 1 * 4);
	}
#endif
	ScalerAddLines(hadChange, scaleLines);
}