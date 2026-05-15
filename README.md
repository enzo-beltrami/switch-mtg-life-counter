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
  rows, one per opponent, each colored to match that opponent. Left/right taps
  on a row adjust commander damage and life together (so dealing 1 commander
  damage also subtracts 1 life).
- **R** button in the top-left of the game screen resets all life to 40 — requires
  a confirm tap within 1.5 seconds to avoid accidents

## Build

Requires devkitPro with `libnx`, SDL2 portlib, and `switch-tools` (for `nacptool`,
`elf2nro`). Set `DEVKITPRO` in your environment, then:

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
  and the app is unusable there (aside from the `(+)` quit).
- The window is created at 1280×720; tile layout is recomputed each frame from
  the current window size, so resizing or resolution changes are handled.
- No font dependency — digits use a 7-segment renderer; the few letters
  ("C", "R", "X", and the title text on the select screen) are drawn either with
  custom rect strokes or a small built-in 5×7 bitmap font.
- Life clamped to `[-99, 999]`, commander damage clamped to `[0, 99]` (display
  constraints).
