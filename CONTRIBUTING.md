# Contributing

## Building locally

You need devkitPro with `libnx`, `switch-tools`, and the SDL2 + SDL2_ttf
portlibs:

```sh
sudo (dkp-)pacman -S switch-sdl2 switch-sdl2_ttf
```

Then with `DEVKITPRO` exported in your environment:

```sh
make          # produces mtg-life-counter.nro
make clean    # clears the build/ tree and artifacts
```

## Code layout

`source/` is split by concern:

- `state.{h,cpp}` — types, constants, palette, `reset_state`, `layout_tiles`
- `geometry.{h,cpp}` — coordinate transforms and rect helpers
- `text.{h,cpp}` — TTF font + texture caches, `draw_text_centered`
- `render.{h,cpp}` — all `draw_*` scene functions
- `input.{h,cpp}` — `handle_tap` and friends
- `main.cpp` — SDL/TTF/romfs lifecycle and the event loop

The Makefile auto-globs `source/*.cpp`, so adding a new module just means
dropping in a new `.h`/`.cpp` pair.

## Cutting a release

Releases are built by `.github/workflows/release.yml`. A push of any tag
matching `v*` triggers a fresh build inside the official `devkitpro/devkita64`
container and publishes the resulting `.nro` to a GitHub release with
auto-generated notes.

The version string baked into the NRO comes from `APP_VERSION` in the
`Makefile`, which is **not** derived from git. You must bump it manually
before tagging so the on-Switch metadata matches the tag.

Steps:

1. Update `APP_VERSION` in `Makefile` (e.g. `1.0.0` → `1.0.1`).
2. Commit: `git commit -am "bump version to 1.0.1"`.
3. Tag and push:
   ```sh
   git tag -a v1.0.1 -m "v1.0.1"
   git push origin main v1.0.1
   ```
4. Watch the Release workflow on the **Actions** tab. When it finishes, the
   release appears under **Releases** with `mtg-life-counter.nro` attached.

Use semver:

- **patch** (`1.0.0` → `1.0.1`) — bug fixes, no behavior changes.
- **minor** (`1.0.0` → `1.1.0`) — new features, backwards-compatible.
- **major** (`1.0.0` → `2.0.0`) — breaking UX changes (controls, save format,
  etc.).

If the CI build fails, fix forward — don't delete and re-push the same tag.
Bump to the next patch and tag again.
