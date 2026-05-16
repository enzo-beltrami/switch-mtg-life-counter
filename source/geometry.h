#pragma once

#include <SDL.h>
#include "state.h"

bool      point_in(const SDL_Rect &r, int x, int y);
SDL_Rect  tile_to_screen(const Tile &t, SDL_Rect local);
bool      screen_to_tile_local(const Tile &t, int sx, int sy, int &lx, int &ly);

SDL_Rect  tile_cmd_btn_local(const Tile &t);

SDL_Rect  reset_rect(int W, int H);
SDL_Rect  quit_rect(int W, int H);
SDL_Rect  select_rect(int idx, int W, int H);

SDL_Rect  confirm_box_rect(int W, int H);
SDL_Rect  confirm_yes_rect(int W, int H);
SDL_Rect  confirm_no_rect(int W, int H);
