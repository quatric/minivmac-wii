/*
	launcher.h

	What a model inside the all-in-one Mini vMac Wii U app can ask of the
	launcher (wiiu/launcher.c), and what the launcher lends it.
*/

#ifndef MV_LAUNCHER_H
#define MV_LAUNCHER_H

#include <SDL2/SDL.h>

/* The window and renderer are the launcher's: they live as long as the app. */
SDL_Window *MVL_Window(void);
SDL_Renderer *MVL_Renderer(void);

/* Show the model picker once this model has quit. */
void MVL_RequestSwitch(void);

/* The app is being closed (power button, the system asking it to quit):
   do not show the picker. */
void MVL_RequestQuit(void);

#endif
