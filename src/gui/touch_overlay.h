#ifndef TOUCH_OVERLAY_H
#define TOUCH_OVERLAY_H

#include "SDL.h"

namespace touchOverlay
{
	void resize(int width, int height, const SDL_Rect& screenArea);

	void setup(SDL_Window* window, int width, int height, const SDL_Rect& screenArea);

	void draw();

	bool checkTap(SDL_TouchFingerEvent* finger);
};

#endif