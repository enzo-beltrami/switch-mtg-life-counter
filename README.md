# mtg-life-counter

A touch-only Commander life counter for the Nintendo Switch, built with SDL2 and
devkitPro. Made for handheld / tabletop mode — controller input is ignored.

This was heavely vibe-coded because i had this idea 1 hour before a mtg session

## Features

- Player-select screen for 2–6 players
- Each player tile shows life in large 7-segment-style digits
- Top-row tiles render upside-down so opposing players read right-side-up
- Tap the **right half** of a tile to add life, the **left half** to subtract
- Per-tile **C** button toggles commander-damage mode: a stack of horizontal
  rows, one per opponent, each colored to match that opponent. Tap the
  **right half** of a row to add commander damage, the **left half** to
  subtract. Commander damage is tracked independently of life — life only
  changes from taps on the life screen.
- **R** button in the top-left of the game screen resets all life to 40 — opens
  a Yes/No confirmation popup to avoid accidents

## Build

Requires devkitPro with `libnx`, the SDL2 and SDL2_ttf portlibs, and
`switch-tools` (for `nacptool`, `elf2nro`):

```sh
sudo (dkp-)pacman -S switch-sdl2 switch-sdl2_ttf
```

Set `DEVKITPRO` in your environment, then:

```sh
make
```

This produces `mtg-life-counter.nro`.

To clean:

```sh
make clean
```

## Install / run

Copy `mtg-life-counter.nro` to the `/switch/` directory on your Switch's SD card
and launch it from the Homebrew Menu. For iterative development, push the NRO
over the network with `nxlink`:

```sh
nxlink -a <switch-ip> mtg-life-counter.nro
```

## Notes

- Touch only works in handheld / tabletop mode. Docked mode has no touch input
  and the app is unusable there.
- The `(+)` button toggles the reset confirmation popup while in-game; on the
  player-select screen it quits the app (the on-screen `X` does the same).
- The window is created at 1280×720; tile layout is recomputed each frame from
  the current window size, so resizing or resolution changes are handled.
- All text is rendered with SDL2_ttf using the Noto Sans Bold font bundled in
  `romfs/font.ttf`. Top-row tiles use `SDL_RenderCopyEx` with a 180° rotation
  so opposing players read right-side-up.
- Life clamped to `[-99, 999]`, commander damage clamped to `[0, 99]` (display
  constraints).
