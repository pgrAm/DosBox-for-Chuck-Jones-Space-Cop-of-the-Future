#include "config.h"
#include <assert.h>
#include <algorithm>
#include <string>

#include "SDL.h"
#include "SDL_system.h"

#include "touch_overlay.h"
#include "video.h"

namespace touchOverlay
{
	constexpr auto NUM_ACTIONS = 6;

	class overlayButton
	{
		SDL_Surface* imagedata = nullptr;
		SDL_Texture* texture = nullptr;
		SDL_Surface* scaledSurface = nullptr;
		SDL_Rect rect;

	public:
		static SDL_Window* window;
		static int width, height;

		SDL_Scancode key;

		void reallocateTexture()
		{
			assert(scaledSurface);

			if (texture)
			{
				SDL_DestroyTexture(texture);
				texture = nullptr;
			};

			auto renderer = SDL_GetRenderer(window);

			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
			assert(renderer);
			texture = SDL_CreateTextureFromSurface(renderer, scaledSurface);
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
			if (color)
			{
				SDL_SetTextureColorMod(texture, 0, 255, 0);
			}

			if (SDL_RenderCopy(SDL_GetRenderer(window), texture, NULL, &rect))
			{
				puts(SDL_GetError());
			}

			if (color)
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

	void resize(int width, int height, const SDL_Rect& screenArea)
	{
		if (!overlayIsSetup) { return; };

		float ddpi, hdpi, vdpi;

		SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(overlayButton::window), &ddpi, &hdpi, &vdpi);

		float scaleRatio = (vdpi / 426.0f);

		int buttonSize = (height * scaleRatio) / 5;

		float left = std::max(screenArea.x - buttonSize, 0) / 2;

		float buttonSpacing = (buttonSize / 4);

		float blockHeight = buttonSize * 4 + buttonSpacing * 3;
		float top = (height - blockHeight) / 2;
		float farLeft = width - buttonSize - left;

		actions[0]->changeRect(left, top + 0 * buttonSize + 0 * buttonSpacing, buttonSize, buttonSize);
		actions[1]->changeRect(left, top + 1 * buttonSize + 1 * buttonSpacing, buttonSize, buttonSize);
		actions[2]->changeRect(left, top + 2 * buttonSize + 2 * buttonSpacing, buttonSize, buttonSize);
		actions[3]->changeRect(left, top + 3 * buttonSize + 3 * buttonSpacing, buttonSize, buttonSize);

		actions[4]->changeRect(farLeft, top + 0 * buttonSize + 0 * buttonSpacing, buttonSize, buttonSize);
		actions[5]->changeRect(farLeft, top + 3 * buttonSize + 3 * buttonSpacing, buttonSize, buttonSize);

		overlayButton::width = width;
		overlayButton::height = height;
	}

	void setup(SDL_Window* window, int width, int height, const SDL_Rect& screenArea)
	{
		actions[0] = new overlayButton("icons/default.bmp", SDL_SCANCODE_1);
		actions[1] = new overlayButton("icons/hand.bmp", SDL_SCANCODE_2);
		actions[2] = new overlayButton("icons/look.bmp", SDL_SCANCODE_3);
		actions[3] = new overlayButton("icons/talk.bmp", SDL_SCANCODE_4);

		actions[4] = new overlayButton("icons/menu.bmp", SDL_SCANCODE_ESCAPE);
		actions[5] = new overlayButton("icons/bag.bmp", SDL_SCANCODE_I);

		overlayIsSetup = true;

		overlayButton::window = window;

		resize(width, height, screenArea);
	}

	void draw()
	{
		if (!overlayIsSetup) { return; };

		for (int i = 0; i < NUM_ACTIONS; i++)
		{
			actions[i]->draw(cursorSelected == i);
		}
	}

	bool checkTap(SDL_TouchFingerEvent* finger)
	{
		int xcoord = finger->x * overlayButton::width;
		int ycoord = finger->y * overlayButton::height;

		static SDL_Scancode key = SDL_NUM_SCANCODES;

		if (finger->type == SDL_FINGERDOWN || finger->type == SDL_FINGERMOTION)
		{
			for (int i = 0; i < NUM_ACTIONS; i++)
			{
				if (actions[i]->checkBounds(xcoord, ycoord))
				{
					if (finger->type == SDL_FINGERMOTION)
					{
						return true;
					}

					if (i < 4)
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
		else if (finger->type == SDL_FINGERUP)
		{
			if (key != SDL_NUM_SCANCODES)
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

	SDL_Window* overlayButton::window = nullptr;
	int overlayButton::width = 0;
	int overlayButton::height = 0;
}