#include <u.h>
#include <9pm/libc.h>
#include <9pm/libg.h>
#include <9pm/windows.h>

/* From WinG Programmer's Reference */

static int	entryalloc(PALETTEENTRY *, int, int, int);
static HPALETTE	simplepal(uchar cmap[256][3]);
static HPALETTE	identitypal(uchar cmap[256][3]);

static void
ClearSystemPalette(void)
{
	//*** A dummy palette setup
	struct
	{
		WORD Version;
		WORD NumberOfEntries;
		PALETTEENTRY aEntries[256];
	} Palette =
	{
		0x300,
		256
	};

	HPALETTE ScreenPalette = 0;
	HDC ScreenDC;
	int Counter;

	//*** Reset everything in the system palette to black
	for(Counter = 0; Counter < 256; Counter++)
	{
		Palette.aEntries[Counter].peRed = 0;
		Palette.aEntries[Counter].peGreen = 0;
		Palette.aEntries[Counter].peBlue = 0;

		Palette.aEntries[Counter].peFlags = PC_NOCOLLAPSE;
	}

	//*** Create, select, realize, deselect, and delete the palette
	ScreenDC = GetDC (NULL);
	ScreenPalette = CreatePalette ((LOGPALETTE *)&Palette);
	if (ScreenPalette)
	{
		ScreenPalette = SelectPalette (ScreenDC,ScreenPalette,FALSE);
		RealizePalette (ScreenDC);
		ScreenPalette = SelectPalette (ScreenDC,ScreenPalette,FALSE);
		DeleteObject (ScreenPalette);
	}
	ReleaseDC (NULL, ScreenDC);
}

HPALETTE
paletteinit(uchar cmap[256][3])
{
/*	return identitypal(cmap); */	
	return simplepal(cmap);	
}

static HPALETTE
simplepal(uchar cmap[256][3])
{
	PALETTEENTRY *pal;
	int i;
	LOGPALETTE *logpal;
	HPALETTE palette;
	HDC hdc;

	hdc = GetDC(0);

	logpal = mallocz(sizeof(LOGPALETTE) + 256*sizeof(PALETTEENTRY));
	logpal->palVersion = 0x300;
	logpal->palNumEntries = 4;
	pal = logpal->palPalEntry;

	pal[0].peRed = pal[0].peGreen = pal[0].peBlue = 0;
	pal[1].peRed = pal[1].peGreen = pal[1].peBlue = 85;
	pal[2].peRed = pal[2].peGreen = pal[2].peBlue = 170;
	pal[3].peRed = pal[3].peGreen = pal[3].peBlue = 255;

	for(i=0; i<256; i++) {
		cmap[i][0] = pal[i].peRed;
		cmap[i][1] = pal[i].peGreen;
		cmap[i][2] = pal[i].peBlue;
	}
	
	palette = CreatePalette(logpal);
	free(logpal);

	ReleaseDC (NULL, hdc);

	return palette;
}

/*
 * Create an identity palette and return in cmap.  The application can
 * then use the palette as it sees fit.
 * The palette should contain the
 *	(6x6x6) color cube (216 entries)
 *	10 levels of gray that fit nicely in between the 6 level in the color cube
 *		giving a total of 16 levels of gray
 *	33% and 66% gray for libg ldepth 1 compatiblity
 *
 * To create an identity palette, we create a logical palette that has the 20 static system
 * colors in their default postion (10 bottom and 10 top entries).  We then place the
 * desired colors in the remaining 236 entrie with the proviso that no two entries in the
 * logical palette should match.  When the application is raised to the top, RealisePalette
 * should generate an identity palette.  Also, because there are no duplicates in the logical
 * palette, the system palette will not change when another application with the same paletee
 * is brought to the top.  This is desirable, since it reduces the number of refreshes caused
 * by changes in the realisation of the palette.  Also, an application in this situtaion will
 * still have an identity palette
 */

static HPALETTE
identitypal(uchar cmap[256][3])
{
	PALETTEENTRY *pal;
	int i, j, k;
	LOGPALETTE *logpal;
	HPALETTE palette;
	HDC hdc;

	ClearSystemPalette();

	hdc = GetDC(0);

	logpal = mallocz(sizeof(LOGPALETTE) + 256*sizeof(PALETTEENTRY));
	logpal->palVersion = 0x300;
	logpal->palNumEntries = 256;
	pal = logpal->palPalEntry;

	//*** Get the static colors from the system palette
	GetSystemPaletteEntries (hdc, 0, 256, pal);

	/* use peFlags to indicate if the entry is allocated */
	for(i=0; i<10; i++)
		pal[i].peFlags = 0;
	for(i=10; i<246; i++)
		pal[i].peFlags = 1;
	for(i=246; i<256; i++)
		pal[i].peFlags = 0;


	for(i=0; i<6; i++) {
		for(j=0; j<6; j++) {
			for(k=0; k<6; k++) {
				entryalloc(pal, i*51, j*51, k*51);
			}
		}
	}

	/* sixteen shades of gray */
	for(i=0; i<16; i++)
		entryalloc(pal, 17*i, 17*i, 17*i);

	/*
	 * fill in the remainder of the palette with reasonable values.
	 * these value are not typically used
	 */

	/* sixteen shades of red green blue */
	for(i=0; i<16; i++) {
		j = (i+5)%16;
		if(entryalloc(pal, 17*j, 0, 0) < 0)
			break;
		if(entryalloc(pal, 0, 17*j, 0) < 0)
			break;
		if(entryalloc(pal, 0, 0, 17*j) < 0)
			break;
	}

	/* at this point the table shoud definitely be full */
	for(i=0; i<256; i++)
		assert(pal[i].peFlags == 0);

	for(i=10; i<246; i++)
		pal[i].peFlags = PC_NOCOLLAPSE;

	for(i=0; i<256; i++) {
		cmap[i][0] = pal[i].peRed;
		cmap[i][1] = pal[i].peGreen;
		cmap[i][2] = pal[i].peBlue;
	}
	
	palette = CreatePalette(logpal);
	free(logpal);

	ReleaseDC (NULL, hdc);

	return palette;
}

static int
entryalloc(PALETTEENTRY *pal, int r, int g, int b)
{
	int i, j;

	/* look for match */
	j = -1;
	for(i=0; i<256; i++) {
		if(pal[i].peFlags != 0) {
			if(j < 0)
				j = i;
			continue;
		}
		if(r != pal[i].peRed || g != pal[i].peGreen || b != pal[i].peBlue)
			continue;
		return i;
	}

	if(j < 0)
		return -1;

	pal[j].peRed = r;
	pal[j].peGreen = g;
	pal[j].peBlue = b;
	pal[j].peFlags = 0;
	
	return j;
}


void
palettecheck(HWND hw, HPALETTE pal)
{
	PALETTEENTRY lpe[256], spe[256];
	HDC hdc;
	int i;
	
	hdc = GetDC(hw);
	SelectPalette(hdc, pal, 0);
	RealizePalette(hdc);
	GetSystemPaletteEntries (hdc, 0, 256, spe);
	GetPaletteEntries(pal, 0, 256, lpe);
	ReleaseDC (hw, hdc);

	
	for(i=0; i<256; i++) {
		if(lpe[i].peRed == spe[i].peRed &&
		   lpe[i].peGreen == spe[i].peGreen &&
		   lpe[i].peBlue == spe[i].peBlue)
			continue;
		fprint(2, "%d: sys = [%d %d %d] log = [%d %d %d]\n", i,
			spe[i].peRed, spe[i].peGreen, spe[i].peBlue,
			lpe[i].peRed, lpe[i].peGreen, lpe[i].peBlue);
	}
}
