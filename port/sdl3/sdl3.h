#ifndef SDL3_PORT_H
#define SDL3_PORT_H

#include <SDL3/SDL.h>

extern SDL_Renderer *sdl3_renderer;

void sdl3_draw_open(const char *path, float px);
void sdl3_draw_scale(float px);
void sdl3_tongues(const char *dir);
void sdl3_languages(const char *dir, SDL_IOStream *log);

#endif
