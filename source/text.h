#pragma once

#include <SDL.h>

// Draws `text` centered at (cx, cy). When flip=true the texture is rotated 180°
// so a player on the opposite side of the screen reads it right-side-up.
void draw_text_centered(SDL_Renderer *r, int cx, int cy, int size,
                        const char *text, SDL_Color color, bool flip);

// Frees all cached text textures and closes every opened TTF_Font slot.
// Call before TTF_Quit() during shutdown.
void text_shutdown();
