/*
*  Copyright (C) 2002-2017  The DOSBox Team
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include <algorithm>
#include <assert.h>
#ifdef WIN32
#include <signal.h>
#include <process.h>
#endif

#include "cross.h"
#include "SDL.h"

#include "dosbox.h"
#include "video.h"
#include "mouse.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "support.h"
#include "debug.h"
#include "mapper.h"
#include "vga.h"
#include "keyboard.h"
#include "cpu.h"
#include "cross.h"
#include "control.h"


#define MAPPERFILE "mapper-" VERSION ".map"
//#define DISABLE_JOYSTICK

#if !(ENVIRON_INCLUDED)
extern char** environ;
#endif

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define STDOUT_FILE	TEXT("stdout.txt")
#define STDERR_FILE	TEXT("stderr.txt")
#define DEFAULT_CONFIG_FILE "/dosbox.conf"
#elif defined(MACOSX)
#define DEFAULT_CONFIG_FILE "/Library/Preferences/DOSBox Preferences"
#else /*linux freebsd*/
#define DEFAULT_CONFIG_FILE "./dosbox.conf"
#endif

#if C_SET_PRIORITY
#include <sys/resource.h>
#define PRIO_TOTAL (PRIO_MAX-PRIO_MIN)
#endif

#ifdef OS2
#define INCL_DOS
#define INCL_WIN
#include <os2.h>
#endif

//#if defined(__ANDROID__)|| defined(__IPHONEOS__)
#define USE_OVERLAY
//#endif

enum PRIORITY_LEVELS
{
	PRIORITY_LEVEL_PAUSE,
	PRIORITY_LEVEL_LOWEST,
	PRIORITY_LEVEL_LOWER,
	PRIORITY_LEVEL_NORMAL,
	PRIORITY_LEVEL_HIGHER,
	PRIORITY_LEVEL_HIGHEST
};

struct SDL_Block
{
	bool inited = false;
	bool active = false;							//If this isn't set don't draw
	bool updating = false;
	struct
	{
		int width = 0;
		int height = 0;
		Bit32u bpp = 0;
		Bitu flags = 0;
		double scalex = 0, scaley = 0;
		GFX_CallBack_t callback = nullptr;
	} draw;
	bool wait_on_error = false;
	struct
	{
		struct
		{
			int width = 0, height = 0;
			bool fixed = false;
		} full;
		struct
		{
			int width = 0, height = 0;
		} window;
		bool fullscreen = false;
	} desktop;
	struct
	{
		SDL_Renderer* renderer = nullptr;
		SDL_Texture* texture = nullptr;
		SDL_Texture* screen_tex = nullptr;
		double scaleFactor = 0;
	} render;
	struct
	{
		PRIORITY_LEVELS focus = PRIORITY_LEVEL_HIGHEST;
		PRIORITY_LEVELS nofocus = PRIORITY_LEVEL_HIGHEST;
	} priority;
	SDL_Rect clip{ 0, 0, 0, 0 };
	SDL_Window * window = nullptr;
	struct
	{
		bool autolock = false;
		bool autoenable = false;
		bool requestlock = false;
		bool locked = false;
		Bitu sensitivity = 0;
	} mouse;
	Bitu num_joysticks = 0;
#if defined (WIN32)
	//bool using_windib;
	// Time when sdl regains focus (alt-tab) in windowed mode
	Bit32u focus_ticks = 0;
#endif
	// state of alt-keys for certain special handlings
	Bit32u laltstate = 0;
	Bit32u raltstate = 0;
	int curr_w = 0, curr_h = 0;
};

static SDL_Block sdl;

void getFullscreenResolution(int displayindex, int* width, int* height)
{
	SDL_DisplayMode vidinfo;
	SDL_GetCurrentDisplayMode(displayindex, &vidinfo);
	*width = vidinfo.w;
	*height = vidinfo.h;
}

void GFX_UpdateSDLCaptureState(void);
void GFX_RestoreMode();

#ifdef USE_OVERLAY
	void resizeOverlay();
#endif

SDL_Window* SDL_SetVideoMode_Wrap(int width, int height, int bpp, Bit32u flags)
{
	static SDL_Window* s = NULL;
	static bool fullscreen = false;

	if(s == NULL)
	{
		s = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);

		if(s == NULL)
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "", SDL_GetError(), NULL);
		}

		sdl.window = s;
	}

	if(flags & SDL_WINDOW_FULLSCREEN)
	{
		SDL_DisplayMode mode;

		SDL_GetWindowDisplayMode(sdl.window, &mode);

		if(mode.h != height || mode.w != width || mode.refresh_rate != 60)
		{
			mode.h = height;
			mode.w = width;
			mode.refresh_rate = 60;
			mode.driverdata = NULL;

			SDL_SetWindowDisplayMode(sdl.window, &mode);
		}

		if(!fullscreen)
		{
			if(SDL_SetWindowFullscreen(sdl.window, SDL_WINDOW_FULLSCREEN))
			{
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "", SDL_GetError(), NULL);
			}

			fullscreen = true;
		}
	}
	else
	{
		if(SDL_SetWindowFullscreen(sdl.window, 0))
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "", SDL_GetError(), NULL);
		}

		if(sdl.curr_w != width || sdl.curr_h != height)
		{
			SDL_SetWindowSize(sdl.window, width, height);
			SDL_SetWindowPosition(sdl.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		}

		fullscreen = false;
	}

	if (!sdl.render.renderer)
	{
		sdl.render.renderer = SDL_CreateRenderer(s, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
	}

	sdl.curr_w = width;
	sdl.curr_h = height;

	if(sdl.mouse.locked)
	{
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}

	return s;
}

extern const char* RunningProgram;
extern bool CPU_CycleAutoAdjust;
//Globals for keyboard initialisation
bool startup_state_numlock = false;
bool startup_state_capslock = false;

void GFX_SetTitle(int cycles, int frameskip, bool paused)
{
	char title[200] = {0};
	static Bit32s internal_cycles = 0;
	static Bit32s internal_frameskip = 0;
	if(cycles != -1) internal_cycles = cycles;
	if(frameskip != -1) internal_frameskip = frameskip;
	if(CPU_CycleAutoAdjust)
	{
		sprintf(title, "DOSBox %s, CPU speed: max %3d%% cycles, Frameskip %2d, Program: %8s", VERSION, internal_cycles, internal_frameskip, RunningProgram);
	}
	else
	{
		sprintf(title, "DOSBox %s, CPU speed: %8d cycles, Frameskip %2d, Program: %8s", VERSION, internal_cycles, internal_frameskip, RunningProgram);
	}

	if(paused) strcat(title, " PAUSED");
	//SDL_WM_SetCaption(title,VERSION);

	SDL_SetWindowTitle(sdl.window, "Chuck Jones: Space Cop of the Future");
}

static unsigned char logo[32 * 32 * 4] = {
#include "dosbox_logo.h"
};
static void GFX_SetIcon()
{
#if !defined(MACOSX)
	/* Set Icon (must be done before any sdl_setvideomode call) */
	/* But don't set it on OS X, as we use a nicer external icon there. */
	/* Made into a separate call, so it can be called again when we restart the graphics output on win32 */
#if WORDS_BIGENDIAN
	SDL_Surface* logos = SDL_CreateRGBSurfaceFrom((void*)logo, 32, 32, 32, 128, 0xff000000, 0x00ff0000, 0x0000ff00, 0);
#else
	SDL_Surface* logos = SDL_CreateRGBSurfaceFrom((void*)logo, 32, 32, 32, 128, 0x000000ff, 0x0000ff00, 0x00ff0000, 0);
#endif
	SDL_SetWindowIcon(sdl.window, logos);
#endif
}


static void KillSwitch(bool pressed)
{
	if(!pressed)
		return;
	throw 1;
}

static void PauseDOSBox(bool pressed)
{
	if(!pressed)
		return;
	GFX_SetTitle(-1, -1, true);
	bool paused = true;
	KEYBOARD_ClrBuffer();
	SDL_Delay(500);
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		// flush event queue.
	}

	while(paused)
	{
		SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.
		switch(event.type)
		{

		case SDL_QUIT:
			KillSwitch(true);
			break;
		case SDL_KEYDOWN:   // Must use Pause/Break Key to resume.
		case SDL_KEYUP:
			if(event.key.keysym.sym == SDL_SCANCODE_PAUSE)
			{

				paused = false;
				GFX_SetTitle(-1, -1, false);
				break;
			}
#if defined (MACOSX) && !defined(__IPHONEOS__)
			if(event.key.keysym.sym == SDL_SCANCODE_q && (event.key.keysym.mod == KMOD_RALT || event.key.keysym.mod == KMOD_LALT))
			{
				/* On macs, all aps exit when pressing cmd-q */
				KillSwitch(true);
				break;
			}
#endif
		}
	}
}

#if defined (WIN32)
bool GFX_SDLUsingWinDIB(void)
{
	//return sdl.using_windib;
	return false;
}
#endif

/* Reset the screen with current values in the sdl structure */
Bitu GFX_GetBestMode(Bitu flags)
{
	flags |= GFX_SCALING;
	flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);

	flags &= ~GFX_LOVE_8;		//Disable love for 8bpp modes
								/* Check if we can satisfy the depth it loves */
	switch(sdl.draw.bpp)
	{
	case 8:
		if(flags & GFX_CAN_8) flags &= ~(GFX_CAN_15 | GFX_CAN_16 | GFX_CAN_32);
		break;
	case 15:
		if(flags & GFX_CAN_15) flags &= ~(GFX_CAN_8 | GFX_CAN_16 | GFX_CAN_32);
		break;
	case 16:
		if(flags & GFX_CAN_16) flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_32);
		break;
	case 24:
	case 32:
		if(flags & GFX_CAN_32) flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
		break;
	}
	flags |= GFX_SCALING;

	return flags;
}


void GFX_ResetScreen(void)
{
	GFX_Stop();
	if(sdl.draw.callback)
	{
		(sdl.draw.callback)(GFX_CallBackReset);
	}
	GFX_Start();
	CPU_Reset_AutoAdjust();
}

void GFX_ForceFullscreenExit(void)
{
	sdl.desktop.fullscreen = false;
	GFX_ResetScreen();
}

static int int_log2(int val)
{
	int log = 0;
	while((val >>= 1) != 0)
		log++;
	return log;
}


#include "SDL_system.h"

#ifdef USE_OVERLAY

#define NUM_ACTIONS 6

class overlayButton
{
	SDL_Surface* imagedata = nullptr;
	SDL_Texture* texture = nullptr;
	SDL_Surface* scaledSurface = nullptr;
	SDL_Rect rect;

public:

	SDL_Scancode key;

	void reallocateTexture()
	{
		assert(scaledSurface);

		if (texture) 
		{
			SDL_DestroyTexture(texture);
			texture = nullptr;
		};

		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
		assert(sdl.render.renderer);
		texture = SDL_CreateTextureFromSurface(sdl.render.renderer, scaledSurface);
		assert(texture);

		Uint32 f;
		if (SDL_QueryTexture(texture, &f, NULL, NULL, NULL) != 0)
		{
			puts(SDL_GetError());
			assert(false);
		}
	}

	overlayButton(std::string path, SDL_Scancode k)
	{
#if defined (__ANDROID__)
		path = SDL_AndroidGetInternalStoragePath() + std::string("/") + path;
#endif
		imagedata = SDL_LoadBMP(path.c_str());

		rect.x = 0;
		rect.y = 0;
		rect.w = 0;
		rect.h = 0;

		key = k;
	}

	void changeRect(int x, int y, int w, int h)
	{
		if (w != rect.w || h != rect.h)
		{
			if (scaledSurface) { SDL_FreeSurface(scaledSurface); };

			int scaledW = (int)ceil(w / (double)imagedata->w) * imagedata->w;
			int scaledH = (int)ceil(h / (double)imagedata->h) * imagedata->h;

			scaledSurface = SDL_CreateRGBSurface(0, scaledW, scaledH, 32, 0, 0, 0, 0);
			assert(scaledSurface);

			if (SDL_BlitScaled(imagedata, NULL, scaledSurface, NULL))
			{
				puts(SDL_GetError());
			}

			reallocateTexture();
		}

		rect.x = x;
		rect.y = y;
		rect.w = w;
		rect.h = h;
	}

	void draw(bool color)
	{
		assert(texture);
		if(color)
		{
			SDL_SetTextureColorMod(texture, 0, 255, 0);
		}

		if (SDL_RenderCopy(sdl.render.renderer, texture, NULL, &rect))
		{
			puts(SDL_GetError());
		}

		if(color)
		{
			SDL_SetTextureColorMod(texture, 255, 255, 255);
		}
	}

	bool checkBounds(int x_Check, int y_Check)
	{
		return (x_Check < rect.x + rect.w) && (y_Check < rect.y + rect.h) && (x_Check > rect.x) && (y_Check > rect.y);
	}
};

int cursorSelected = 0;

overlayButton* actions[NUM_ACTIONS];

bool overlayIsSetup = false;

void resizeOverlay()
{
	if (!overlayIsSetup) { return; };

	float ddpi, hdpi, vdpi;

	SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(sdl.window), &ddpi, &hdpi, &vdpi);

	float scaleRatio = (vdpi / 426.0f);

	int buttonSize = (sdl.curr_h * scaleRatio) / 5;

	float left = std::max(sdl.clip.x - buttonSize, 0) / 2;

	float buttonSpacing = (buttonSize / 4);

	float blockHeight = buttonSize * 4 + buttonSpacing * 3;
	float top = (sdl.curr_h - blockHeight) / 2;
	float farLeft = sdl.curr_w - buttonSize - left;

	actions[0]->changeRect(left, top + 0 * buttonSize + 0 * buttonSpacing, buttonSize, buttonSize);
	actions[1]->changeRect(left, top + 1 * buttonSize + 1 * buttonSpacing, buttonSize, buttonSize);
	actions[2]->changeRect(left, top + 2 * buttonSize + 2 * buttonSpacing, buttonSize, buttonSize);
	actions[3]->changeRect(left, top + 3 * buttonSize + 3 * buttonSpacing, buttonSize, buttonSize);

	actions[4]->changeRect(farLeft, top + 0 * buttonSize + 0 * buttonSpacing, buttonSize, buttonSize);
	actions[5]->changeRect(farLeft, top + 3 * buttonSize + 3 * buttonSpacing, buttonSize, buttonSize);
}

void setupOverlay()
{
	actions[0] = new overlayButton("icons/default.bmp", SDL_SCANCODE_1);
	actions[1] = new overlayButton("icons/hand.bmp", SDL_SCANCODE_2);
	actions[2] = new overlayButton("icons/look.bmp", SDL_SCANCODE_3);
	actions[3] = new overlayButton("icons/talk.bmp", SDL_SCANCODE_4);

	actions[4] = new overlayButton("icons/menu.bmp", SDL_SCANCODE_ESCAPE);
	actions[5] = new overlayButton("icons/bag.bmp", SDL_SCANCODE_I);

	overlayIsSetup = true;

	resizeOverlay();
}

void drawOverlay()
{
	if (!overlayIsSetup) { return; };

 	for(int i = 0; i < NUM_ACTIONS; i++)
	{
		actions[i]->draw(cursorSelected == i);
	}
}
#endif

static void GFX_SetupSurfaceScaled()
{
	Bit32u sdl_flags = 0;

	if (sdl.desktop.fullscreen)
	{
		sdl_flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MOUSE_CAPTURE;
	}

	if(sdl.desktop.full.fixed || !sdl.desktop.fullscreen)
	{
		int fixedWidth;
		int fixedHeight;

		if (sdl.desktop.fullscreen)
		{
			getFullscreenResolution(SDL_GetWindowDisplayIndex(sdl.window),
									&sdl.desktop.full.width,
									&sdl.desktop.full.height);

			fixedWidth = sdl.desktop.full.width;
			fixedHeight = sdl.desktop.full.height;
		}
		else
		{
			fixedWidth = sdl.desktop.window.width;
			fixedHeight = sdl.desktop.window.height;
		}

		double ratio_w = (double)fixedWidth / (sdl.draw.width * sdl.draw.scalex);
		double ratio_h = (double)fixedHeight / (sdl.draw.height * sdl.draw.scaley);

		if(ratio_w < ratio_h)
		{
			sdl.clip.w = fixedWidth;
			sdl.clip.h = (int)(sdl.draw.height * sdl.draw.scaley * ratio_w + 0.1); //possible rounding issues
		}
		else
		{
			/*
			* The 0.4 is there to correct for rounding issues.
			* (partly caused by the rounding issues fix in RENDER_SetSize)
			*/
			sdl.clip.w = (int)(sdl.draw.width*sdl.draw.scalex*ratio_h + 0.4);
			sdl.clip.h = (int)fixedHeight;
		}

		if(sdl.desktop.fullscreen)
		{
			sdl.window = SDL_SetVideoMode_Wrap(fixedWidth, fixedHeight, 32, sdl_flags);
		}
		else
		{
			sdl.window = SDL_SetVideoMode_Wrap(sdl.clip.w, sdl.clip.h, 32, sdl_flags);
		}

		sdl.clip.x = (int)((sdl.curr_w - sdl.clip.w) / 2);
		sdl.clip.y = (int)((sdl.curr_h - sdl.clip.h) / 2);
	}
	else
	{
		//just set the mode directly and let the hardware deal with it
		sdl.clip.x = 0;
		sdl.clip.y = 0;
		sdl.clip.w = (int)(sdl.draw.width * sdl.draw.scalex);
		sdl.clip.h = (int)(sdl.draw.height * sdl.draw.scaley);
		sdl.window = SDL_SetVideoMode_Wrap(sdl.clip.w, sdl.clip.h, 32, sdl_flags);
	}
}

void GFX_TearDown(void)
{
	if(sdl.updating)
	{
		GFX_EndUpdate(0);
	}
}

#define DEFAULT_PIXEL_FORMAT SDL_PIXELFORMAT_RGBA32

Bitu GFX_SetSize(Bitu width, Bitu height, Bitu flags, double scalex, double scaley, GFX_CallBack_t callback)
{
	if(sdl.updating)
		GFX_EndUpdate(0);

	sdl.draw.width = width;
	sdl.draw.height = height;
	sdl.draw.callback = callback;
	sdl.draw.scalex = scalex;
	sdl.draw.scaley = scaley;

	int bpp = 0;
	Bitu retFlags = 0;

	if(sdl.render.texture)
	{
		SDL_DestroyTexture(sdl.render.texture);
		SDL_DestroyTexture(sdl.render.screen_tex);
		sdl.render.texture = sdl.render.screen_tex = nullptr;
	}
	
	GFX_SetupSurfaceScaled();
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	
	sdl.render.texture = SDL_CreateTexture(sdl.render.renderer, DEFAULT_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, sdl.draw.width, sdl.draw.height);
	assert(sdl.render.texture);
	
	if((double)sdl.curr_w / (double)sdl.curr_h < ((double)sdl.draw.width / (double)sdl.draw.height))
	{
		//contrary popular belief 5:4 monitors do exist
		//I like them quite a bit
		sdl.render.scaleFactor = sdl.curr_w / (double)sdl.draw.width;
	}
	else
	{
		sdl.render.scaleFactor = sdl.curr_h / (double)sdl.draw.height;
	}
	
	int w = sdl.draw.width * int(sdl.render.scaleFactor);
	int h = sdl.draw.height * int(sdl.render.scaleFactor);
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	
	sdl.render.screen_tex = SDL_CreateTexture(sdl.render.renderer, SDL_GetWindowPixelFormat(sdl.window), SDL_TEXTUREACCESS_TARGET, w, h);
	assert(sdl.render.screen_tex);

#ifdef USE_OVERLAY
	resizeOverlay();
#endif

	Uint32 f;
	if (SDL_QueryTexture(sdl.render.texture, &f, NULL, NULL, NULL) != 0)
	{
		puts(SDL_GetError());
		assert(false);
	}
	
	switch(SDL_BITSPERPIXEL(f))
	{
	case 15:
		retFlags = GFX_CAN_15 | GFX_SCALING | GFX_HARDWARE;
		break;
	case 16:
		retFlags = GFX_CAN_16 | GFX_SCALING | GFX_HARDWARE;
		break;
	case 24:
	case 32:
		retFlags = GFX_CAN_32 | GFX_SCALING | GFX_HARDWARE;
		break;
	}

	if(retFlags)
	{
		GFX_Start();
	}

	if(!sdl.mouse.autoenable)
	{
		SDL_ShowCursor(sdl.mouse.autolock ? SDL_DISABLE : SDL_ENABLE);
	}

	return retFlags;
}

void GFX_CaptureMouse(void)
{
	sdl.mouse.locked = !sdl.mouse.locked;
	if(sdl.mouse.locked)
	{
		//SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
	else
	{
		SDL_SetRelativeMouseMode(SDL_FALSE);
		if(sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
	mouselocked = sdl.mouse.locked;
}

void GFX_UpdateSDLCaptureState(void)
{
	if(sdl.mouse.locked)
	{
		//SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
	else
	{
		SDL_SetRelativeMouseMode(SDL_FALSE);
		//SDL_WM_GrabInput(SDL_GRAB_OFF);
		if(sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
	CPU_Reset_AutoAdjust();
	GFX_SetTitle(-1, -1, false);
}

bool mouselocked; //Global variable for mapper
static void CaptureMouse(bool pressed)
{
	if(!pressed)
		return;
	GFX_CaptureMouse();
}

#if defined (WIN32)
STICKYKEYS stick_keys = {sizeof(STICKYKEYS), 0};
void sticky_keys(bool restore)
{
	static bool inited = false;
	if(!inited)
	{
		inited = true;
		SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
	}
	if(restore)
	{
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
		return;
	}
	//Get current sticky keys layout:
	STICKYKEYS s = {sizeof(STICKYKEYS), 0};
	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	if(!(s.dwFlags & SKF_STICKYKEYSON))
	{ //Not on already
		s.dwFlags &= ~SKF_HOTKEYACTIVE;
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	}
}
#endif

void GFX_SwitchFullScreen(void)
{
	sdl.desktop.fullscreen = !sdl.desktop.fullscreen;
	if(sdl.desktop.fullscreen)
	{
		if(!sdl.mouse.locked) GFX_CaptureMouse();
#if defined (WIN32)
		sticky_keys(false); //disable sticky keys in fullscreen mode
#endif
	}
	else
	{
		if(sdl.mouse.locked) GFX_CaptureMouse();
#if defined (WIN32)
		sticky_keys(true); //restore sticky keys to default state in windowed mode.
#endif
	}
	GFX_ResetScreen();
}

static void SwitchFullScreen(bool pressed)
{
	if(!pressed)
		return;

	GFX_SwitchFullScreen();
}

void GFX_SwitchFullscreenNoReset(void)
{
	sdl.desktop.fullscreen = !sdl.desktop.fullscreen;
}

void GFX_RestoreMode(void)
{
	GFX_SetSize(sdl.draw.width, sdl.draw.height, sdl.draw.flags, sdl.draw.scalex, sdl.draw.scaley, sdl.draw.callback);
	GFX_UpdateSDLCaptureState();
}


bool GFX_StartUpdate(Bit8u * & pixels, Bitu & pitch)
{
	if(!sdl.active || sdl.updating)
	{
		return false;
	}

	int i_pitch;
	void* i_pixels;

	if(SDL_LockTexture(sdl.render.texture, NULL, &i_pixels, &i_pitch) < 0)
	{
		return false;
	}

	pixels = (Bit8u *)i_pixels;
	pitch = i_pitch;
	sdl.updating = true;

	return true;
}

void GFX_EndUpdate(const Bit16u *changedLines)
{
	if(!sdl.updating)
	{
		return;
	}

	sdl.updating = false;

	SDL_UnlockTexture(sdl.render.texture);

	SDL_SetRenderTarget(sdl.render.renderer, sdl.render.screen_tex);

	if (SDL_RenderCopy(sdl.render.renderer, sdl.render.texture, 0, 0))
	{
		puts(SDL_GetError());
	}

	SDL_SetRenderTarget(sdl.render.renderer, NULL);
//#ifdef USE_OVERLAY
	//TODO
	SDL_RenderClear(sdl.render.renderer);
//#endif
	if(SDL_RenderCopy(sdl.render.renderer, sdl.render.screen_tex, 0, &sdl.clip))
	{
		puts(SDL_GetError());
	}
#ifdef USE_OVERLAY
	drawOverlay();
#endif
	SDL_RenderPresent(sdl.render.renderer);
}

void GFX_SetPalette(Bitu start, Bitu count, GFX_PalEntry * entries)
{
	/* I should probably not change the GFX_PalEntry :) */
	//if (sdl.surface->flags & SDL_HWPALETTE) {
	//	if (!SDL_SetPalette(sdl.surface,SDL_PHYSPAL,(SDL_Color *)entries,start,count)) {
	//		E_Exit("SDL:Can't set palette");
	//	}
	//} else {
	//	if (!SDL_SetPalette(sdl.surface,SDL_LOGPAL,(SDL_Color *)entries,start,count)) {
	//		E_Exit("SDL:Can't set palette");
	//	}
	//}
}

Bitu GFX_GetRGB(Bit8u red, Bit8u green, Bit8u blue)
{
	Uint32 f;

	SDL_QueryTexture(sdl.render.texture, &f, NULL, NULL, NULL);

	SDL_PixelFormat* fmt = SDL_AllocFormat(f);

	Bitu c = SDL_MapRGB(fmt, red, green, blue);

	SDL_FreeFormat(fmt);

	return c;
}

void GFX_Stop()
{
	if(sdl.updating)
	{
		GFX_EndUpdate(0);
	}
	sdl.active = false;
}

void GFX_Start()
{
	sdl.active = true;
}

static void GUI_ShutDown(Section * /*sec*/)
{
	GFX_Stop();
	if(sdl.draw.callback) { (sdl.draw.callback)(GFX_CallBackStop); }
	if(sdl.mouse.locked) { GFX_CaptureMouse(); }
	//if(sdl.desktop.fullscreen) { GFX_SwitchFullScreen(); }
}


static void SetPriority(PRIORITY_LEVELS level)
{

#if C_SET_PRIORITY
	// Do nothing if priorties are not the same and not root, else the highest
	// priority can not be set as users can only lower priority (not restore it)

	if((sdl.priority.focus != sdl.priority.nofocus) &&
		(getuid() != 0)) return;

#endif
	switch(level)
	{
#ifdef WIN32
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_LOWER:
		SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_NORMAL:
		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHER:
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHEST:
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		break;
#elif C_SET_PRIORITY
		/* Linux use group as dosbox has mulitple threads under linux */
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		setpriority(PRIO_PGRP, 0, PRIO_MAX);
		break;
	case PRIORITY_LEVEL_LOWER:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - (PRIO_TOTAL / 3));
		break;
	case PRIORITY_LEVEL_NORMAL:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - (PRIO_TOTAL / 2));
		break;
	case PRIORITY_LEVEL_HIGHER:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - ((3 * PRIO_TOTAL) / 5));
		break;
	case PRIORITY_LEVEL_HIGHEST:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - ((3 * PRIO_TOTAL) / 4));
		break;
#endif
	default:
		break;
}
}

extern Bit8u int10_font_14[256 * 14];
static void OutputString(Bitu x, Bitu y, const char * text, Bit32u color, Bit32u color2, SDL_Surface * output_surface)
{
	Bit32u * draw = (Bit32u*)(((Bit8u *)output_surface->pixels) + ((y)*output_surface->pitch)) + x;
	while(*text)
	{
		Bit8u * font = &int10_font_14[(*text) * 14];
		Bitu i, j;
		Bit32u * draw_line = draw;
		for(i = 0; i < 14; i++)
		{
			Bit8u map = *font++;
			for(j = 0; j < 8; j++)
			{
				if(map & 0x80) *((Bit32u*)(draw_line + j)) = color; else *((Bit32u*)(draw_line + j)) = color2;
				map <<= 1;
			}
			draw_line += output_surface->pitch / 4;
		}
		text++;
		draw += 8;
	}
}

#include "dosbox_splash.h"

//extern void UI_Run(bool);
void Restart(bool pressed);

static void GUI_StartUp(Section * sec)
{
	sec->AddDestroyFunction(&GUI_ShutDown);
	Section_prop * section = static_cast<Section_prop *>(sec);
	sdl.active = false;
	sdl.updating = false;

	GFX_SetIcon();

	sdl.desktop.fullscreen = section->Get_bool("fullscreen");
	sdl.wait_on_error = section->Get_bool("waitonerror");

	Prop_multival* p = section->Get_multival("priority");
	std::string focus = p->GetSection()->Get_string("active");
	std::string notfocus = p->GetSection()->Get_string("inactive");

	uint32_t defaultFlags = sdl.desktop.fullscreen ? SDL_WINDOW_FULLSCREEN : 0;

	SDL_GL_SetSwapInterval(1);

	if(focus == "lowest") { sdl.priority.focus = PRIORITY_LEVEL_LOWEST; }
	else if(focus == "lower") { sdl.priority.focus = PRIORITY_LEVEL_LOWER; }
	else if(focus == "normal") { sdl.priority.focus = PRIORITY_LEVEL_NORMAL; }
	else if(focus == "higher") { sdl.priority.focus = PRIORITY_LEVEL_HIGHER; }
	else if(focus == "highest") { sdl.priority.focus = PRIORITY_LEVEL_HIGHEST; }

	if(notfocus == "lowest") { sdl.priority.nofocus = PRIORITY_LEVEL_LOWEST; }
	else if(notfocus == "lower") { sdl.priority.nofocus = PRIORITY_LEVEL_LOWER; }
	else if(notfocus == "normal") { sdl.priority.nofocus = PRIORITY_LEVEL_NORMAL; }
	else if(notfocus == "higher") { sdl.priority.nofocus = PRIORITY_LEVEL_HIGHER; }
	else if(notfocus == "highest") { sdl.priority.nofocus = PRIORITY_LEVEL_HIGHEST; }
	else if(notfocus == "pause")
	{
		/* we only check for pause here, because it makes no sense
		* for DOSBox to be paused while it has focus
		*/
		sdl.priority.nofocus = PRIORITY_LEVEL_PAUSE;
	}

	SetPriority(sdl.priority.focus); //Assume focus on startup
	sdl.mouse.locked = false;
	mouselocked = false; //Global for mapper
	sdl.mouse.requestlock = false;
	sdl.desktop.full.fixed = false;
	const char* fullresolution = section->Get_string("fullresolution");
	sdl.desktop.full.width = 0;
	sdl.desktop.full.height = 0;
	if(fullresolution && *fullresolution)
	{
		char res[100];
		safe_strncpy(res, fullresolution, sizeof(res));
		fullresolution = lowcase(res);//so x and X are allowed
		if(strcmp(fullresolution, "original"))
		{
			sdl.desktop.full.fixed = true;
			if(strcmp(fullresolution, "desktop"))
			{ //desktop = 0x0
				char* height = const_cast<char*>(strchr(fullresolution, 'x'));
				if(height && * height)
				{
					*height = 0;
					sdl.desktop.full.height = (Bit16u)atoi(height + 1);
					sdl.desktop.full.width = (Bit16u)atoi(res);
				}
			}
		}
	}

	sdl.desktop.window.width = 0;
	sdl.desktop.window.height = 0;
	const char* windowresolution = section->Get_string("windowresolution");
	if(windowresolution && *windowresolution)
	{
		char res[100];
		safe_strncpy(res, windowresolution, sizeof(res));
		windowresolution = lowcase(res);//so x and X are allowed
		if(strcmp(windowresolution, "original"))
		{
			char* height = const_cast<char*>(strchr(windowresolution, 'x'));
			if(height && *height)
			{
				*height = 0;
				sdl.desktop.window.height = (Bit16u)atoi(height + 1);
				sdl.desktop.window.width = (Bit16u)atoi(res);
			}
		}
	}
	//sdl.desktop.doublebuf = section->Get_bool("fulldouble");

	if(!sdl.desktop.full.width || !sdl.desktop.full.height)
	{
		//Can only be done on the very first call! Not restartable.
		getFullscreenResolution(0,
								&sdl.desktop.full.width,
								&sdl.desktop.full.height);
	}

	if(!sdl.desktop.full.width)
	{
#ifdef WIN32
		sdl.desktop.full.width = (Bit16u)GetSystemMetrics(SM_CXSCREEN);
#else
		LOG_MSG("Your fullscreen resolution can NOT be determined, it's assumed to be 1024x768.\nPlease edit the configuration file if this value is wrong.");
		sdl.desktop.full.width = 1024;
#endif
	}
	if(!sdl.desktop.full.height)
	{
#ifdef WIN32
		sdl.desktop.full.height = (Bit16u)GetSystemMetrics(SM_CYSCREEN);
#else
		sdl.desktop.full.height = 768;
#endif
	}
	sdl.mouse.autoenable = section->Get_bool("autolock");
	if(!sdl.mouse.autoenable) SDL_ShowCursor(SDL_DISABLE);
	sdl.mouse.autolock = false;
	sdl.mouse.sensitivity = section->Get_int("sensitivity");

	std::string output = section->Get_string("output");

	SDL_SetHint(SDL_HINT_RENDER_DRIVER, output.c_str());

	int windowHeight = sdl.desktop.fullscreen ? sdl.desktop.full.height : sdl.desktop.window.height;
	int windowWidth = sdl.desktop.fullscreen ? sdl.desktop.full.width : sdl.desktop.window.width;

	if(windowWidth < 640 || windowHeight < 400)
	{
		windowHeight = DEFAULT_HEIGHT;
		windowWidth = DEFAULT_WIDTH;
	}

	/* Initialize screen for first time */
	sdl.window = SDL_SetVideoMode_Wrap(windowWidth, windowHeight, 0, defaultFlags);
	if(sdl.window == NULL)
	{
		E_Exit("Could not initialize video: %s", SDL_GetError());
	}

	{
		sdl.draw.bpp = SDL_BITSPERPIXEL(DEFAULT_PIXEL_FORMAT);

		if(sdl.draw.bpp == 24)
		{
			LOG_MSG("SDL: You are running in 24 bpp mode, this will slow down things!");
		}
	}

	GFX_Stop();
	SDL_SetWindowTitle(sdl.window, "DOSBox");

	/* The endian part is intentionally disabled as somehow it produces correct results without according to rhoenie*/
	//#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	//    Bit32u rmask = 0xff000000;
	//    Bit32u gmask = 0x00ff0000;
	//    Bit32u bmask = 0x0000ff00;
	//#else
	Bit32u rmask = 0x000000ff;
	Bit32u gmask = 0x0000ff00;
	Bit32u bmask = 0x00ff0000;
	//#endif

	SDL_RenderClear(sdl.render.renderer);
	SDL_RenderPresent(sdl.render.renderer);

	SDL_RaiseWindow(sdl.window);

	/* Setup Mouse correctly if fullscreen */
	if(sdl.desktop.fullscreen)
	{
		if(!sdl.mouse.locked)
		{
			GFX_CaptureMouse();
		}
	}

	/* Please leave the Splash screen stuff in working order in DOSBox. We spend a lot of time making DOSBox. */
	SDL_Surface* splash_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, windowWidth, windowHeight, 32, rmask, gmask, bmask, 0);

	SDL_SetSurfaceBlendMode(splash_surf, SDL_BLENDMODE_BLEND);

	if(splash_surf)
	{
		SDL_FillRect(splash_surf, NULL, SDL_MapRGB(splash_surf->format, 0xba, 0x3d, 0x00));

		int yoffset = (windowHeight - 400) / 2;

		Bit8u* tmpbufp = new Bit8u[640 * 400 * 3];
		GIMP_IMAGE_RUN_LENGTH_DECODE(tmpbufp, gimp_image.rle_pixel_data, 640 * 400, 3);
		for(Bitu y = 0; y < 400; y++)
		{

			int offset = (windowWidth - 640) / 2;

			Bit8u* tmpbuf = tmpbufp + y * 640 * 3;
			Bit32u * draw = (Bit32u*)(((Bit8u *)splash_surf->pixels) + ((y + yoffset)*splash_surf->pitch)) + offset;

			for(Bitu x = 0; x < 640; x++)
			{
				//#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				//				*draw++ = tmpbuf[x*3+2]+tmpbuf[x*3+1]*0x100+tmpbuf[x*3+0]*0x10000+0x00000000;
				//#else
				*draw++ = tmpbuf[x * 3 + 0] + tmpbuf[x * 3 + 1] * 0x100 + tmpbuf[x * 3 + 2] * 0x10000 + 0x00000000;
				//#endif
			}
		}

		bool exit_splash = false;

		static Bitu max_splash_loop = 2400;
		static Bitu splash_fade = 400;
		static bool use_fadeout = true;

		SDL_Texture* splash_tex = SDL_CreateTextureFromSurface(sdl.render.renderer, splash_surf);
		SDL_SetRenderDrawColor(sdl.render.renderer, 0, 0, 0, 255);

		for(Bit32u ct = 0, startticks = GetTicks(); ct < max_splash_loop; ct = GetTicks() - startticks)
		{
			SDL_Event evt;
			while(SDL_PollEvent(&evt))
			{
				if(evt.type == SDL_QUIT)
				{
					exit_splash = true;
					break;
				}
			}
			if(exit_splash) break;

			if(ct < 1)
			{
				SDL_RenderCopy(sdl.render.renderer, splash_tex, NULL, NULL);
				SDL_RenderPresent(sdl.render.renderer);
			}
			else if(ct >= max_splash_loop - splash_fade)
			{
				if(use_fadeout)
				{
					SDL_RenderClear(sdl.render.renderer);
					SDL_SetTextureAlphaMod(splash_tex, (Bit8u)((max_splash_loop - 1 - ct) * 255 / (splash_fade - 1)));
					SDL_RenderCopy(sdl.render.renderer, splash_tex, NULL, NULL);
					SDL_RenderPresent(sdl.render.renderer);
				}
			}
		}

		if(use_fadeout)
		{
			SDL_RenderClear(sdl.render.renderer);
			SDL_RenderPresent(sdl.render.renderer);
			//this second one clears in case of double buffer
			SDL_RenderClear(sdl.render.renderer);
		}
		SDL_FreeSurface(splash_surf);
		delete[] tmpbufp;
	}

#ifdef USE_OVERLAY
	setupOverlay();
#endif

	/* Get some Event handlers */
	//MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","ShutDown");
	MAPPER_AddHandler(CaptureMouse, MK_f10, MMOD1, "capmouse", "Cap Mouse");
	//MAPPER_AddHandler(SwitchFullScreen, MK_return, MMOD2, "fullscr", "Fullscreen");
	MAPPER_AddHandler(SwitchFullScreen, MK_f11, 0, "fullscr", "Fullscreen");
	//MAPPER_AddHandler(SwitchFullScreen, MK_f11, MMOD2| MMOD1, "fullscr", "Fullscreen");
	//MAPPER_AddHandler(Restart,MK_home,MMOD1|MMOD2,"restart","Restart");
#if C_DEBUG
	/* Pause binds with activate-debugger */
#else
	//MAPPER_AddHandler(&PauseDOSBox, MK_pause, MMOD2, "pause", "Pause DBox");
#endif
	/* Get Keyboard state of numlock and capslock */
	SDL_Keymod keystate = SDL_GetModState();
	if(keystate&KMOD_NUM) startup_state_numlock = true;
	if(keystate&KMOD_CAPS) startup_state_capslock = true;
	}

void Mouse_AutoLock(bool enable)
{
	sdl.mouse.autolock = enable;
	if(sdl.mouse.autoenable) sdl.mouse.requestlock = enable;
	else
	{
		SDL_ShowCursor(enable ? SDL_DISABLE : SDL_ENABLE);
		sdl.mouse.requestlock = false;
	}
}

struct
{
	float lastX = 0.5, lastY = 0.5;
	int lastPixX = 160, lastPixY = 120;
} mouseInfo;

static void HandleMouseMotion(SDL_MouseMotionEvent * motion)
{
	if (motion->which == SDL_TOUCH_MOUSEID) { return; }

	if(sdl.mouse.locked || !sdl.mouse.autoenable)
	{
		float deltaX = (float)motion->xrel * sdl.mouse.sensitivity / 100.0f;
		float deltaY = (float)motion->yrel * sdl.mouse.sensitivity / 100.0f;

		Mouse_CursorMoved(	deltaX, deltaY,
							(float)(motion->x - sdl.clip.x) / (sdl.clip.w - 1)*sdl.mouse.sensitivity / 100.0f,
							(float)(motion->y - sdl.clip.y) / (sdl.clip.h - 1)*sdl.mouse.sensitivity / 100.0f,
							sdl.mouse.locked);

		//if (sdl.mouse.locked)
		{
			mouseInfo.lastPixX = std::clamp((int)(mouseInfo.lastPixX + deltaX), 0, sdl.draw.width);
			mouseInfo.lastPixY = std::clamp((int)(mouseInfo.lastPixY + deltaY), 0, sdl.draw.height);
		}
	}
}

static void HandleMouseButton(SDL_MouseButtonEvent * button)
{
	if (button->which == SDL_TOUCH_MOUSEID) { return; }

	switch(button->state)
	{
	case SDL_PRESSED:
		if(sdl.mouse.requestlock && !sdl.mouse.locked)
		{
			GFX_CaptureMouse();
			// Dont pass klick to mouse handler
			break;
		}
		if(!sdl.mouse.autoenable && sdl.mouse.autolock && button->button == SDL_BUTTON_MIDDLE)
		{
			GFX_CaptureMouse();
			break;
		}
		switch(button->button)
		{
		case SDL_BUTTON_LEFT:
			Mouse_ButtonPressed(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonPressed(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonPressed(2);
			break;
		}
		break;
	case SDL_RELEASED:
		switch(button->button)
		{
		case SDL_BUTTON_LEFT:
			Mouse_ButtonReleased(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonReleased(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonReleased(2);
			break;
		}
		break;
	}
}

#ifdef USE_OVERLAY
bool checkOverlayTap(SDL_TouchFingerEvent * finger)
{
	int xcoord = finger->x * sdl.curr_w;
	int ycoord = finger->y * sdl.curr_h;

	static SDL_Scancode key = SDL_NUM_SCANCODES;

	if(finger->type == SDL_FINGERDOWN)
	{
		for(int i = 0; i < NUM_ACTIONS; i++)
		{
			if(actions[i]->checkBounds(xcoord, ycoord))
			{
				if(i < 4)
				{
					GFX_ResetScreen();
					cursorSelected = i;
				}

				SDL_Event sdlevent;
				sdlevent.type = SDL_KEYDOWN;
				sdlevent.key.keysym.scancode = actions[i]->key;

				SDL_PushEvent(&sdlevent);

				key = actions[i]->key;

				return true;
			}
		}
	}
	else if(finger->type == SDL_FINGERUP)
	{
		if(key != SDL_NUM_SCANCODES)
		{
			SDL_Event sdlevent;
			sdlevent.type = SDL_KEYUP;
			sdlevent.key.keysym.scancode = key;

			SDL_PushEvent(&sdlevent);

			key = SDL_NUM_SCANCODES;

			return true;
		}
	}

	return false;
}
#endif

void moveFinger(SDL_TouchFingerEvent* finger)
{
	double xscale = (sdl.curr_w / sdl.render.scaleFactor);
	double yscale = (sdl.curr_h / sdl.render.scaleFactor);

	int pixX = std::clamp((int)((finger->x * xscale) - (xscale - sdl.draw.width) / 2.0), 0, sdl.draw.width);
	int pixY = std::clamp((int)((finger->y * yscale) - (yscale - sdl.draw.height) / 2.0), 0, sdl.draw.height);

	int x = pixX - mouseInfo.lastPixX;
	int y = pixY - mouseInfo.lastPixY;

	Mouse_CursorMoved(x, y, 0, 0, true);

	mouseInfo.lastPixX = pixX;
	mouseInfo.lastPixY = pixY;
}

static void HandleTouch(SDL_TouchFingerEvent * finger)
{
#ifdef USE_OVERLAY
	if(checkOverlayTap(finger))
	{
		return;
	}
#endif
	switch(finger->type)
	{

	case SDL_FINGERDOWN:
	{
		moveFinger(finger);
		Mouse_ButtonPressed(0);
	}
	break;
	case SDL_FINGERMOTION:
		moveFinger(finger);
		break;
	case SDL_FINGERUP:
		Mouse_ButtonReleased(0);
		break;
	}
}

void GFX_LosingFocus(void)
{
	sdl.laltstate = SDL_KEYUP;
	sdl.raltstate = SDL_KEYUP;
	MAPPER_LosingFocus();
}

bool GFX_IsFullscreen(void)
{
	return sdl.desktop.fullscreen;
}

void MAPPER_CheckEvent(SDL_Event * event);

void GFX_Events()
{
	SDL_Event event;
#if defined (REDUCE_JOYSTICK_POLLING)
	static int poll_delay = 0;
	int time = GetTicks();
	if(time - poll_delay > 20)
	{
		poll_delay = time;
		if(sdl.num_joysticks > 0) SDL_JoystickUpdate();
		MAPPER_UpdateJoysticks();
	}
#endif
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
		case SDL_WINDOWEVENT:

			switch(event.window.type)
			{
			case SDL_WINDOWEVENT_FOCUS_GAINED:
#ifdef WIN32
				if(!sdl.desktop.fullscreen)
				{
					sdl.focus_ticks = GetTicks();
				}
#endif
				if(sdl.desktop.fullscreen && !sdl.mouse.locked)
				{
					GFX_CaptureMouse();
				}

				SetPriority(sdl.priority.focus);
				CPU_Disable_SkipAutoAdjust();
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				if(sdl.mouse.locked)
				{
#ifdef WIN32
					if(sdl.desktop.fullscreen)
					{
						VGA_KillDrawing();
						GFX_ForceFullscreenExit();
					}
#endif
					GFX_CaptureMouse();
				}
				SetPriority(sdl.priority.nofocus);
				GFX_LosingFocus();
				CPU_Enable_SkipAutoAdjust();
				break;
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				SDL_RenderClear(sdl.render.renderer);
				break;
			}
#ifndef __ANDROID__
			/* Non-focus priority is set to pause; check to see if we've lost window or input focus
			*/
			if(sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE)
			{
				/* Window has lost focus, pause the emulator.
				* This is similar to what PauseDOSBox() does, but the exit criteria is different.
				* Instead of waiting for the user to hit Alt-Break, we wait for the window to
				* regain window or input focus.
				*/
				bool paused = true;
				SDL_Event ev;

				GFX_SetTitle(-1, -1, true);
				KEYBOARD_ClrBuffer();
				//					SDL_Delay(500);
				//					while (SDL_PollEvent(&ev)) {
				// flush event queue.
				//					}

				while(paused)
				{
					// WaitEvent waits for an event rather than polling, so CPU usage drops to zero
					SDL_WaitEvent(&ev);

					switch(ev.type)
					{
					case SDL_QUIT: throw(0); break; // a bit redundant at linux at least as the active events gets before the quit event.
					case SDL_WINDOWEVENT:     // wait until we get window focus back
						if(ev.window.type == SDL_WINDOWEVENT_FOCUS_GAINED)
						{
							// We've got focus back, so unpause and break out of the loop
							paused = false;
							GFX_SetTitle(-1, -1, false);

							/* Now poke a "release ALT" command into the keyboard buffer
							* we have to do this, otherwise ALT will 'stick' and cause
							* problems with the app running in the DOSBox.
							*/
							KEYBOARD_AddKey(KBD_leftalt, false);
							KEYBOARD_AddKey(KBD_rightalt, false);
						}
						break;
					}
				}
			}
#endif
			break;
		case SDL_APP_DIDENTERFOREGROUND:
			GFX_ResetScreen();
			break;
		case SDL_MOUSEMOTION:
#ifndef __IPHONEOS__
			HandleMouseMotion(&event.motion);
#endif
			break;
		case SDL_FINGERDOWN:
		case SDL_FINGERUP:
		case SDL_FINGERMOTION:
			HandleTouch(&event.tfinger);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
#ifndef __IPHONEOS__
			HandleMouseButton(&event.button);
#endif
			break;
			//		case SDL_VIDEORESIZE:
			////			HandleVideoResize(&event.resize);
			//			break;
		case SDL_QUIT:
			throw(0);
			break;
			//		case SDL_VIDEOEXPOSE:
			//			if (sdl.draw.callback) sdl.draw.callback( GFX_CallBackRedraw );
			//			break;
		case SDL_TEXTINPUT:
		{
			int len = strlen(event.text.text);

			for(int i = 0; i < len; i++)
			{
				char character[] = {event.text.text[i], 0};

				if(character[0] >= 65 && character[0] <= 90)
				{
					SDL_Event sdlevent;
					sdlevent.type = SDL_KEYDOWN;
					sdlevent.key.keysym.scancode = SDL_SCANCODE_LSHIFT;

					MAPPER_CheckEvent(&sdlevent);
				}

				if(character[0] >= 97 && character[0] <= 122)
				{
					character[0] -= 32;
				}

				SDL_Event sdlevent;
				sdlevent.type = SDL_KEYDOWN;
				sdlevent.key.keysym.scancode = SDL_GetScancodeFromName(character);

				MAPPER_CheckEvent(&sdlevent);

				SDL_Event sdlevent1;
				sdlevent1.type = SDL_KEYUP;
				sdlevent1.key.keysym.scancode = sdlevent.key.keysym.scancode;

				MAPPER_CheckEvent(&sdlevent1);

				if(character[0] >= 65 && character[0] <= 90)
				{
					SDL_Event sdlevent;
					sdlevent.type = SDL_KEYUP;
					sdlevent.key.keysym.scancode = SDL_SCANCODE_LSHIFT;

					MAPPER_CheckEvent(&sdlevent);
				}
			}
		}
		break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
#ifdef WIN32
			// ignore event alt+tab
			if(event.key.keysym.sym == SDL_SCANCODE_LALT) sdl.laltstate = event.key.type;
			if(event.key.keysym.sym == SDL_SCANCODE_RALT) sdl.raltstate = event.key.type;
			if(((event.key.keysym.sym == SDL_SCANCODE_TAB)) && ((sdl.laltstate == SDL_KEYDOWN) || (sdl.raltstate == SDL_KEYDOWN))) break;
			// This can happen as well.
			if(((event.key.keysym.sym == SDL_SCANCODE_TAB)) && (event.key.keysym.mod & KMOD_ALT)) break;
			// ignore tab events that arrive just after regaining focus. (likely the result of alt-tab)
			if((event.key.keysym.sym == SDL_SCANCODE_TAB) && (GetTicks() - sdl.focus_ticks < 2)) break;
#endif
#if defined (__ANDROID__) && defined(__IPHONEOS__)
			if(SDL_IsTextInputActive()) 
			{
				if(event.key.keysym.scancode != SDL_SCANCODE_BACKSPACE && event.key.keysym.scancode != SDL_SCANCODE_RETURN)
				{
					break;
				}
			}
#endif
#if defined (MACOSX) && !defined(__IPHONEOS__)
			/* On macs CMD-Q is the default key to close an application */
			if(event.key.keysym.sym == SDL_SCANCODE_q && (event.key.keysym.mod == KMOD_RALT || event.key.keysym.mod == KMOD_LALT))
			{
				KillSwitch(true);
				break;
			}
#endif
		default:
			MAPPER_CheckEvent(&event);
		}
	}
}

#if defined (WIN32)
static BOOL WINAPI ConsoleEventHandler(DWORD event)
{
	switch(event)
	{
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		raise(SIGTERM);
		return TRUE;
	case CTRL_C_EVENT:
	default: //pass to the next handler
		return FALSE;
	}
}
#endif

/* static variable to show wether there is not a valid stdout.
* Fixes some bugs when -noconsole is used in a read only directory */
static bool no_stdout = false;
void GFX_ShowMsg(char const* format, ...)
{
	char buf[512];
	va_list msg;
	va_start(msg, format);
	vsprintf(buf, format, msg);
	strcat(buf, "\n");
	va_end(msg);
	if(!no_stdout) printf("%s", buf); //Else buf is parsed again.

#ifdef __ANDROID__
	__android_log_print(ANDROID_LOG_INFO, "DOSBOX", "%s", buf);
#endif
}

void Config_Add_SDL()
{
	Section_prop * sdl_sec = control->AddSection_prop("sdl", &GUI_StartUp);
	sdl_sec->AddInitFunction(&MAPPER_StartUp);
	Prop_bool* Pbool;
	Prop_string* Pstring;
	Prop_int* Pint;
	Prop_multival* Pmulti;

	Pbool = sdl_sec->Add_bool("fullscreen", Property::Changeable::Always, false);
	Pbool->Set_help("Start dosbox directly in fullscreen. (Press ALT-Enter to go back)");

	Pbool = sdl_sec->Add_bool("fulldouble", Property::Changeable::Always, false);
	Pbool->Set_help("Use double buffering in fullscreen. It can reduce screen flickering, but it can also result in a slow DOSBox.");

	Pstring = sdl_sec->Add_string("fullresolution", Property::Changeable::Always, "original");
	Pstring->Set_help("What resolution to use for fullscreen: original, desktop or a fixed size (e.g. 1024x768).\n"
					  "  Using your monitor's native resolution with aspect=true might give the best results.\n"
					  "  If you end up with small window on a large screen, try an output different from surface.");

	Pstring = sdl_sec->Add_string("windowresolution", Property::Changeable::Always, "original");
	Pstring->Set_help("Scale the window to this size IF the output device supports hardware scaling.\n"
					  "  (output=software does not!)");

	const char* outputs[] = 
	{
		"software",
		"opengl",
		"opengles",
		"opengles2",
#if defined (MACOSX) || defined(__IPHONEOS__)
		"metal",
#endif
#if _WIN32
		"direct3d",
#endif
		0};

	Pstring = sdl_sec->Add_string("output", Property::Changeable::Always, "direct3d");
	Pstring->Set_help("What video system to use for output.");
	Pstring->Set_values(outputs);

	Pbool = sdl_sec->Add_bool("autolock", Property::Changeable::Always, true);
	Pbool->Set_help("Mouse will automatically lock, if you click on the screen. (Press CTRL-F10 to unlock)");

	Pint = sdl_sec->Add_int("sensitivity", Property::Changeable::Always, 100);
	Pint->SetMinMax(1, 1000);
	Pint->Set_help("Mouse sensitivity.");

	Pbool = sdl_sec->Add_bool("waitonerror", Property::Changeable::Always, true);
	Pbool->Set_help("Wait before closing the console if dosbox has an error.");

	Pmulti = sdl_sec->Add_multi("priority", Property::Changeable::Always, ",");
	Pmulti->SetValue("higher,normal");
	Pmulti->Set_help("Priority levels for dosbox. Second entry behind the comma is for when dosbox is not focused/minimized.\n"
					 "  pause is only valid for the second entry.");

	const char* actt[] = {"lowest", "lower", "normal", "higher", "highest", "pause", 0};
	Pstring = Pmulti->GetSection()->Add_string("active", Property::Changeable::Always, "higher");
	Pstring->Set_values(actt);

	const char* inactt[] = {"lowest", "lower", "normal", "higher", "highest", "pause", 0};
	Pstring = Pmulti->GetSection()->Add_string("inactive", Property::Changeable::Always, "normal");
	Pstring->Set_values(inactt);

	Pstring = sdl_sec->Add_path("mapperfile", Property::Changeable::Always, MAPPERFILE);
	Pstring->Set_help("File used to load/save the key/event mappings from. Resetmapper only works with the default value.");

	Pbool = sdl_sec->Add_bool("usescancodes", Property::Changeable::Always, true);
	Pbool->Set_help("Avoid usage of symkeys, might not work on all operating systems.");
}

static void show_warning(char const * const message)
{
	bool textonly = true;
#ifdef WIN32
	textonly = false;
	if(!sdl.inited && SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0) textonly = true;
	sdl.inited = true;
#endif
	printf("%s", message);
	if(textonly) return;
	if(!sdl.window) sdl.window = SDL_SetVideoMode_Wrap(DEFAULT_WIDTH, DEFAULT_HEIGHT, 0, 0);
	if(!sdl.window) return;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	Bit32u rmask = 0xff000000;
	Bit32u gmask = 0x00ff0000;
	Bit32u bmask = 0x0000ff00;
#else
	Bit32u rmask = 0x000000ff;
	Bit32u gmask = 0x0000ff00;
	Bit32u bmask = 0x00ff0000;
#endif
	SDL_Surface* splash_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, DEFAULT_WIDTH, DEFAULT_HEIGHT, 32, rmask, gmask, bmask, 0);
	if(!splash_surf) return;

	int x = 120, y = 20;
	std::string m(message), m2;
	std::string::size_type a, b, c, d;

	while(m.size())
	{ //Max 50 characters. break on space before or on a newline
		c = m.find('\n');
		d = m.rfind(' ', 50);
		if(c > d) a = b = d; else a = b = c;
		if(a != std::string::npos) b++;
		m2 = m.substr(0, a); m.erase(0, b);
		OutputString(x, y, m2.c_str(), 0xffffffff, 0, splash_surf);
		y += 20;
	}

	//SDL_Surface* s = SDL_GetWindowSurface(sdl.surface);
	//
	//SDL_BlitSurface(splash_surf, NULL, s, NULL);
	//SDL_UpdateWindowSurface(sdl.surface);
	//SDL_Flip(s);
	SDL_Delay(12000);
}

static void launcheditor()
{
	std::string path, file;
	Cross::CreatePlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;
	FILE* f = fopen(path.c_str(), "r");
	if(!f && !control->PrintConfig(path.c_str()))
	{
		printf("tried creating %s. but failed.\n", path.c_str());
		exit(1);
	}
	if(f) fclose(f);
	/*	if(edit.empty()) {
	printf("no editor specified.\n");
	exit(1);
	}*/
	std::string edit;
	while(control->cmdline->FindString("-editconf", edit, true)) //Loop until one succeeds
		execlp(edit.c_str(), edit.c_str(), path.c_str(), (char*)0);
	//if you get here the launching failed!
	printf("can't find editor(s) specified at the command line.\n");
	exit(1);
}
#if C_DEBUG
extern void DEBUG_ShutDown(Section * /*sec*/);
#endif

void restart_program(std::vector<std::string> & parameters)
{
	char** newargs = new char*[parameters.size() + 1];
	// parameter 0 is the executable path
	// contents of the vector follow
	// last one is NULL
	for(Bitu i = 0; i < parameters.size(); i++) newargs[i] = (char*)parameters[i].c_str();
	newargs[parameters.size()] = NULL;
	SDL_CloseAudio();
	SDL_Delay(50);
	SDL_Quit();
#if C_DEBUG
	// shutdown curses
	DEBUG_ShutDown(NULL);
#endif
	assert(parameters.size() > 0);
	if(execvp(newargs[0], newargs) == -1)
	{
#ifdef WIN32
		if(newargs[0][0] == '\"')
		{
			//everything specifies quotes around it if it contains a space, however my system disagrees
			std::string edit = parameters[0];
			edit.erase(0, 1); edit.erase(edit.length() - 1, 1);
			//However keep the first argument of the passed argv (newargs) with quotes, as else repeated restarts go wrong.
			if(execvp(edit.c_str(), newargs) == -1) E_Exit("Restarting failed");
		}
#endif
		E_Exit("Restarting failed");
	}
	delete [] newargs;
}
void Restart(bool pressed)
{ // mapper handler
	restart_program(control->startup_params);
}

static void launchcaptures(std::string const& edit)
{
	std::string path, file;
	Section* t = control->GetSection("dosbox");
	if(t) file = t->GetPropValue("captures");
	if(!t || file == NO_SUCH_PROPERTY)
	{
		printf("Config system messed up.\n");
		exit(1);
	}
	Cross::CreatePlatformConfigDir(path);
	path += file;
	Cross::CreateDir(path);
	struct stat cstat;
	if(stat(path.c_str(), &cstat) || (cstat.st_mode & S_IFDIR) == 0)
	{
		printf("%s doesn't exists or isn't a directory.\n", path.c_str());
		exit(1);
	}
	/*	if(edit.empty()) {
	printf("no editor specified.\n");
	exit(1);
	}*/

	execlp(edit.c_str(), edit.c_str(), path.c_str(), (char*)0);
	//if you get here the launching failed!
	printf("can't find filemanager %s\n", edit.c_str());
	exit(1);
}

static void printconfiglocation()
{
	std::string path, file;
	Cross::CreatePlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;

	FILE* f = fopen(path.c_str(), "r");
	if(!f && !control->PrintConfig(path.c_str()))
	{
		printf("tried creating %s. but failed", path.c_str());
		exit(1);
	}
	if(f) fclose(f);
	printf("%s\n", path.c_str());
	exit(0);
}

static void eraseconfigfile()
{
	FILE* f = fopen("dosbox.conf", "r");
	if(f)
	{
		fclose(f);
		show_warning("Warning: dosbox.conf exists in current working directory.\nThis will override the configuration file at runtime.\n");
	}
	std::string path, file;
	Cross::GetPlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;
	f = fopen(path.c_str(), "r");
	if(!f) exit(0);
	fclose(f);
	unlink(path.c_str());
	exit(0);
}

static void erasemapperfile()
{
	FILE* g = fopen("dosbox.conf", "r");
	if(g)
	{
		fclose(g);
		show_warning("Warning: dosbox.conf exists in current working directory.\nKeymapping might not be properly reset.\n"
					 "Please reset configuration as well and delete the dosbox.conf.\n");
	}

	std::string path, file = MAPPERFILE;
	Cross::GetPlatformConfigDir(path);
	path += file;
	FILE* f = fopen(path.c_str(), "r");
	if(!f) exit(0);
	fclose(f);
	unlink(path.c_str());
	exit(0);
}

//extern void UI_Init(void);
int main(int argc, char* argv[])
{
	try
	{
		CommandLine com_line(argc, argv);
		Config myconf(&com_line);
		control = &myconf;
		/* Init the configuration system and add default values */
		Config_Add_SDL();
		DOSBOX_Init();

		std::string editor;
		if(control->cmdline->FindString("-editconf", editor, false)) launcheditor();
		if(control->cmdline->FindString("-opencaptures", editor, true)) launchcaptures(editor);
		if(control->cmdline->FindExist("-eraseconf")) eraseconfigfile();
		if(control->cmdline->FindExist("-resetconf")) eraseconfigfile();
		if(control->cmdline->FindExist("-erasemapper")) erasemapperfile();
		if(control->cmdline->FindExist("-resetmapper")) erasemapperfile();

		/* Can't disable the console with debugger enabled */
#if defined(WIN32) && !(C_DEBUG)


		//if (control->cmdline->FindExist("-noconsole")) {
		//	FreeConsole();
		//	/* Redirect standard input and standard output */
		//	if(freopen(STDOUT_FILE, "w", stdout) == NULL)
		//		no_stdout = true; // No stdout so don't write messages
		//	freopen(STDERR_FILE, "w", stderr);
		//	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);	/* Line buffered */
		//	setbuf(stderr, NULL);					/* No buffering */
		//} else {
#ifdef _DEBUG
		if(AllocConsole())
		{
			fclose(stdin);
			fclose(stdout);
			fclose(stderr);
			freopen("CONIN$", "r", stdin);
			freopen("CONOUT$", "w", stdout);
			freopen("CONOUT$", "w", stderr);
		}
		SetConsoleTitle("DOSBox Status Window");
#else
		no_stdout = true;
#endif
		//}
#endif  //defined(WIN32) && !(C_DEBUG)
		if(control->cmdline->FindExist("-version") ||
		   control->cmdline->FindExist("--version"))
		{
			printf("\nDOSBox version %s, copyright 2002-2017 DOSBox Team.\n\n", VERSION);
			printf("DOSBox is written by the DOSBox Team (See AUTHORS file))\n");
			printf("DOSBox comes with ABSOLUTELY NO WARRANTY.  This is free software,\n");
			printf("and you are welcome to redistribute it under certain conditions;\n");
			printf("please read the COPYING file thoroughly before doing so.\n\n");
			return 0;
		}
		if(control->cmdline->FindExist("-printconf")) printconfiglocation();

#if C_DEBUG
		DEBUG_SetupConsole();
#endif

#if defined(WIN32)
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleEventHandler, TRUE);
#endif

#ifdef OS2
		PPIB pib;
		PTIB tib;
		DosGetInfoBlocks(&tib, &pib);
		if(pib->pib_ultype == 2) pib->pib_ultype = 3;
		setbuf(stdout, NULL);
		setbuf(stderr, NULL);
#endif

		/* Display Welcometext in the console */
		LOG_MSG("DOSBox version %s", VERSION);
		LOG_MSG("Copyright 2002-2017 DOSBox Team, published under GNU GPL.");
		LOG_MSG("---");

		/* Init SDL */
		putenv(const_cast<char*>("SDL_DISABLE_LOCK_KEYS=1"));

#ifdef _WIN32
		putenv("SDL_AUDIODRIVER=directsound");
#endif

		if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE) != 0)
		{
			E_Exit("Can't init SDL %s", SDL_GetError());
		}
		sdl.inited = true;

#if defined(__ANDROID__)|| defined(__IPHONEOS__)
		sdl.window = SDL_SetVideoMode_Wrap(DEFAULT_WIDTH, DEFAULT_HEIGHT, 0, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
#endif

		SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");

#ifndef DISABLE_JOYSTICK
		//Initialise Joystick separately. This way we can warn when it fails instead
		//of exiting the application
		if(SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) LOG_MSG("Failed to init joystick support");
#endif

		sdl.laltstate = SDL_KEYUP;
		sdl.raltstate = SDL_KEYUP;

		sdl.num_joysticks = SDL_NumJoysticks();

		/* Parse configuration files */
		std::string config_file, config_path;
		Cross::GetPlatformConfigDir(config_path);

		//First parse -userconf
		if(control->cmdline->FindExist("-userconf", true))
		{
			config_file.clear();
			Cross::GetPlatformConfigDir(config_path);
			Cross::GetPlatformConfigName(config_file);
			config_path += config_file;
			control->ParseConfigFile(config_path.c_str());
			if(!control->configfiles.size())
			{
				//Try to create the userlevel configfile.
				config_file.clear();
				Cross::CreatePlatformConfigDir(config_path);
				Cross::GetPlatformConfigName(config_file);
				config_path += config_file;
				if(control->PrintConfig(config_path.c_str()))
				{
					LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s", config_path.c_str());
					//Load them as well. Makes relative paths much easier
					control->ParseConfigFile(config_path.c_str());
				}
			}
		}

		//Second parse -conf switches
		while(control->cmdline->FindString("-conf", config_file, true))
		{
			if(!control->ParseConfigFile(config_file.c_str()))
			{
				// try to load it from the user directory
				control->ParseConfigFile((config_path + config_file).c_str());
			}
		}
		// if none found => parse localdir conf
		if(!control->configfiles.size())
		{
			control->ParseConfigFile("dosbox.conf");
		}

		// if none found => parse userlevel conf
		if(!control->configfiles.size())
		{
			config_file.clear();
			Cross::GetPlatformConfigName(config_file);
			control->ParseConfigFile((config_path + config_file).c_str());
		}

		if(!control->configfiles.size())
		{
			//Try to create the userlevel configfile.
			config_file.clear();
			Cross::CreatePlatformConfigDir(config_path);
			Cross::GetPlatformConfigName(config_file);
			config_path += config_file;
			if(control->PrintConfig(config_path.c_str()))
			{
				LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s", config_path.c_str());
				//Load them as well. Makes relative paths much easier
				control->ParseConfigFile(config_path.c_str());
			}
			else
			{
				LOG_MSG("CONFIG: Using default settings. Create a configfile to change them");
			}
		}


#if (ENVIRON_LINKED)
		control->ParseEnv(environ);
#endif
		//		UI_Init();
		//		if (control->cmdline->FindExist("-startui")) UI_Run(false);
		/* Init all the sections */
		control->Init();
		/* Some extra SDL Functions */
		Section_prop * sdl_sec = static_cast<Section_prop *>(control->GetSection("sdl"));

		if(control->cmdline->FindExist("-fullscreen") || sdl_sec->Get_bool("fullscreen"))
		{
			if(!sdl.desktop.fullscreen)
			{ //only switch if not already in fullscreen
				GFX_SwitchFullScreen();
			}
		}

		/* Init the keyMapper */
		MAPPER_Init();
		if(control->cmdline->FindExist("-startmapper")) MAPPER_RunInternal();
		/* Start up main machine */
		control->StartUp();
		/* Shutdown everything */
		}
	catch(char * error)
	{
#if defined (WIN32)
		sticky_keys(true);
#endif
		GFX_ShowMsg("Exit to error: %s", error);
		fflush(NULL);
		if(sdl.wait_on_error)
		{
			//TODO Maybe look for some way to show message in linux?
#if (C_DEBUG)
			GFX_ShowMsg("Press enter to continue");
			fflush(NULL);
			fgetc(stdin);
#elif defined(WIN32)
			Sleep(5000);
#endif
		}

	}
	catch(int)
	{
		; //nothing, pressed killswitch
	}
	catch(...)
	{
		; // Unknown error, let's just exit.
	}
#if defined (WIN32)
	sticky_keys(true); //Might not be needed if the shutdown function switches to windowed mode, but it doesn't hurt
#endif
	//Force visible mouse to end user. Somehow this sometimes doesn't happen
	SDL_SetRelativeMouseMode(SDL_FALSE);
	
	SDL_Quit();//Let's hope sdl will quit as well when it catches an exception
	return 0;
}