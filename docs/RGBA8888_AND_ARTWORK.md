# RGBA8888 Internal Pipeline — and what it means for artwork

*Companion to [SDL3_PORT.md](SDL3_PORT.md) Phase 6b. Written after the 32bpp
flip landed and the tactical screen rendered correctly on the
`phase-6b-rgba8888` branch.*

This document explains:

1. What the 32-bit pipeline change actually is.
2. What it buys us.
3. What it does **not** automatically do.
4. The image formats involved (STCI/STI vs PNG) and what they support.
5. A practical strategy for a future PNG / true-color asset conversion.

---

## 1. What changed

The engine's internal pixel format was flipped from **RGB565** (16 bits per
pixel) to **ARGB8888** (32 bits per pixel, `0xAARRGGBB`,
`SDL_PIXELFORMAT_ARGB8888`).

This is controlled by one macro, `SGP_PIXEL_DEPTH`, in
[`sgp/pixfmt.h`](../sgp/pixfmt.h):

```c
#define SGP_PIXEL_DEPTH 32          // 16 = legacy RGB565, 32 = ARGB8888
typedef UINT32 PIXEL;               // (UINT16 when depth == 16)
```

Every framebuffer, video surface, blitter and palette now stores `PIXEL`
(currently `UINT32`). The change is deliberately contained to the pixel-format
layer; the rest of the engine stays depth-agnostic.

### Key idea: "logical colour" stays RGB565

JA2 computes colours in **hundreds** of places via `Get16BPPColor()` and stores
them in `UINT16`s (UI fills, lines, boxes, font foreground, cover tints, …).
Re-typing all of those was not viable. Instead:

> **The engine still computes colours as RGB565 "logical" values; they are
> expanded to ARGB8888 only at the moment a pixel is written**, via the
> `PixFromColor16()` inline in `pixfmt.h`.

8-bit indexed art is different — it goes through a **palette lookup table**
(`Create16BPPPalette` → `Create32BPPPalette`) that now produces ARGB8888
entries directly, so indexed sprites are full-colour-correct without per-write
conversion.

The Z-buffer stays **16-bit** throughout (it stores depth, not colour).

#### `Get16BPPColor` now returns true ARGB8888 (loose end closed in Phase 6c)

Originally `Get16BPPColor()` produced an RGB565 token (16-bit) that was
expanded to ARGB8888 only at the write site (`PixFromColor16()`), so UI
colours sourced through it were quantized to RGB565 precision. **Phase 6c
closed this:** at 32bpp `Get16BPPColor()` now returns full ARGB8888 (it forwards
to `Get32BPPColor`), so every UI colour — fills, lines, box borders, fonts,
cursors, cover/CtH tints — carries true 8-bit-per-channel fidelity.

How the migration was done (recorded here because the technique is reusable):

- `Get16BPPColor` return type → `PIXEL`. `PixFromColor16()` was made
  dual-mode: it detects an already-ARGB value (non-zero top 16 bits) and passes
  it through, while still expanding a genuine RGB565 value (≤ `0xFFFF`). That
  keeps the write path correct for both UI colours (now ARGB) **and** real
  RGB565 image data (`PngLoader`, `Copy16BPPImageTo16BPPBuffer`), which is
  unchanged.
- Every place a colour is *stored* or *passed* was widened `UINT16 → PIXEL`:
  the `line.cpp` draw API, `ColorFillVideoSurfaceArea`/`FillRect16`/`FillRect16BPP`,
  `Display2Line2Shadow*`, `DisplaySmallLine`/`OptDisplayLine`, `DrawBar`,
  `DrawCTHPixelToBuffer`, the mono-font blitter, the outline blitters /
  `BltVideoObjectOutline*`, pixelate/hatch, the `Font` fore/back/shadow globals,
  `Text Input` bevel/cursor setters, the `BaseTable` status-bar callback chain,
  `DropDown` members, `renderworld` item-cycle/outline colour globals, and the
  assorted UI locals.
- Enumeration was **compiler-guided**: build with `-Wimplicit-int-conversion`,
  diff the narrowing warnings against a pre-change baseline to isolate the
  colour-truncation sites from the codebase's thousands of pre-existing legacy
  narrowings, widen them, rebuild, repeat until zero (244 → 0). This also
  surfaced two real hazards beyond precision: the 64K `ShadeTable`/
  `IntensityTable` build in `shading.cpp` (an ARGB value used as a table index
  → out of bounds; the build is now guarded to 16bpp, where the tables are
  actually used), and `PngLoader` (which packs a genuine 16bpp image and now
  packs RGB565 explicitly instead of borrowing `Get16BPPColor`).

Fidelity that was already correct before 6c and remains so: 8-bit indexed art
(via the palette LUT), blending (per-channel), and true-colour PNGs.

The Z-buffer stays **16-bit** throughout (it stores depth, not colour).

#### ⚠️ Gotcha: colour `0` is a "transparent / none" sentinel — and black used to alias to it

This is the single most important thing to know before touching colour code or
replacing art. Throughout the engine, a colour value of **`0` means "transparent
/ draw nothing"** — and in RGB565, **pure black `(0,0,0)` encodes as `0x0000`**.
So historically *black* and *no-colour* were the **same value**, and a lot of
code leans on that coincidence with an `if (colour != 0)` "should I draw this?"
test.

True-colour `Get16BPPColor` breaks the coincidence: black is now
`0xFF000000` (opaque, **non-zero**). Every `!= 0` / `== 0` sentinel that a
caller fed black into therefore flipped behaviour — black "none" started drawing
as an opaque black box/halo, or a black "transparent key" stopped keying. Three
instances surfaced during 6c playtesting, all fixed the same way (map pure
black back to `0` at the source, *or* compare on RGB ignoring alpha):

| Symptom | Where | Fix |
|---|---|---|
| Opaque black bars behind UI text | `SetFontBackground` / `SetRGBFontBackground` (`Font.cpp`) — `usBackground != 0` in the mono blitter | pure-black background → `0` |
| Fuzzy black drop-shadow halo around glyphs (looked like "bad AA"), esp. the laptop | `SetFontShadow` (`Font.cpp`) — `usShadow != 0` in the mono blitter | pure-black shadow → `0` (the `ubShadow!=0` bump still draws a *requested* black shadow as `1`) |
| 8bpp sprite transparency (palette index 0 → opaque `0xFF000000`, not `0`) | `Blit16_ColorKey` (`sdl_vsurface.cpp`), `BltStretchVideoSurface` | colour-key compares on **RGB only**, ignoring alpha |

**If you ever see a stray black box, halo, or a sprite that lost its
transparency, this is almost certainly the cause** — find the `colour == 0` /
`colour != 0` sentinel in that path and apply the same treatment. The same
applies when authoring true-colour PNG replacements: don't rely on black to mean
"transparent"; use a real alpha channel (the `Blt32BPPTo16BPP*` blitters read
`alpha = src>>24`).

The Z-buffer stays **16-bit** throughout (it stores depth, not colour).

---

## 2. What the 32-bit pipeline buys us

| Benefit | Why it matters |
|---|---|
| **No RGB565 banding** | 5/6/5 only gives 32 levels of red/blue, 64 of green. Gradients (skies, lighting falloff, fades, shadows) showed visible steps. ARGB8888 has full 8-bit channels — smooth. |
| **Accurate blending** | Translucency/shadow/light used lossy "halve the low bits with a 5/6/5 carry mask" tricks (`guiTranslucentMask`). Now per-channel 8-bit math (`PixBlend50`, `blendWithAlpha`). Smoke, shadows, night shading are cleaner. |
| **Display-native** | ARGB8888 is the GPU/SDL texture format. Presenting the frame is a straight `memcpy`-style upload with no conversion. Future scaling/sharpening/shaders are cleaner to add. |
| **Carries true colour end-to-end** | The framebuffer and blit path can now represent 16.7M colours, so a high-colour source image renders at full fidelity instead of being quantized to 16-bit. |

The first three are realised **today**, automatically, with the existing art.
The fourth is the *enabler* for the artwork story below.

---

## 3. What it does NOT do

**Flipping to 32bpp does not upgrade existing art.** The overwhelming majority
of JA2 assets are **8-bit indexed** STCI sprites (tiles, mercs, items, faces,
cursors, most UI). After the flip they still:

- carry a **256-colour palette** per image (their *source* fidelity is
  unchanged — only the framebuffer they're drawn into is now 32-bit), and
- render through the palette LUT (correct, but still 256 colours).

So the screen is now a true-colour *canvas*, but most of what's painted onto it
is still 256-colour *paint*. Getting genuinely true-colour art requires
**replacing the source assets**, which is a content project (see §5).

---

## 4. Image formats

### STCI / STI — the native JA2 format

STCI ("ST(C)I") has exactly two pixel modes:

| Mode | Bits | Notes |
|---|---|---|
| **Indexed** | 8 | Palette (256 `SGPPaletteEntry` RGB triples) + **ETRLE** run-length compression. Supports transparency (index 0) and the multi-image / animation / Z-strip metadata JA2 tiles need. **This is ~all of JA2's art.** |
| **RGB** | 16 | RGB555/565, uncompressed. Rare; used for a few full-screen-ish images. |

**STCI has no 32-bit / alpha mode.** There is no header bit for it, no loader
for it, and the ETRLE scheme is built around an 8-bit index stream.

Indexed mode also powers gameplay-critical **palette tricks**: merc skin/hair
recolouring, team colour, the enemy "glow" highlight, day/night shade tables —
all implemented as *palette substitutions* on the 256-entry table
(`CreateEnemyGlow16BPPPalette`, `Create16BPPPaletteShaded`, the per-tile
`pShades[]` shade ramps). True-colour art has no palette to substitute, so
these effects would need reimplementing (see §5).

### PNG — the true-colour path (already wired up)

1.13 added **PNG loaders with fallback**. You can see it in the surface/object
descriptors:

```c
vs_desc.fCreateFlags = VSURFACE_CREATE_FROMFILE
                     | VSURFACE_SYSTEM_MEM_USAGE
                     | VSURFACE_CREATE_FROMPNG_FALLBACK;   // <-- PNG override
```

`ImageFileType::PNG_FALLBACK` means: *try `<name>.png` first, otherwise fall
back to the original STI*. The HIMAGE path already supports 24/32-bit images
(`ubBitDepth == 32`, `p32BPPData`) and the engine fills surfaces from them via
`Blt32BPPTo16BPPTrans` (which now has a real ARGB destination branch).

**So the mechanism to drop in a true-colour replacement for any given sprite
already exists.** That is the foundation a conversion strategy builds on.

---

## 5. A future PNG / true-colour conversion strategy

Goal: progressively replace 8-bit indexed art with true-colour PNGs **where it
pays off**, without breaking the gameplay that depends on indexed palettes.

### 5.1 Pick targets by payoff, not by volume

Best candidates (large, smooth-gradient, rarely palette-swapped):

- Full-screen / menu / laptop backgrounds, the loading and intro art.
- Cutscene-ish or photographic UI (the map parchment, big panels).
- Skyboxes / large lighting gradients if any.

Worst candidates (small, palette-swapped, or animation-heavy) — leave as STCI:

- **Mercs / civilians / enemies** — recoloured per-merc via palette
  substitution and glow-highlighted. Converting these means reimplementing
  recolour + glow as shader/tint passes (see 5.4).
- **Tiles** — thousands of them, day/night shaded via `pShades[]` palette
  ramps; ETRLE + Z-strip metadata matters for the isometric renderer.
- **Items / small icons** — tiny; 256 colours is plenty; high churn.

### 5.2 Use the PNG-fallback mechanism (non-destructive)

Author the replacement as `<originalname>.png` next to the STI and load with
`VSURFACE_CREATE_FROMPNG_FALLBACK` / `ImageFileType::PNG_FALLBACK`. The STI stays
as the fallback, so:

- conversion is **incremental and reversible** (delete the PNG → original art),
- no need to touch the STCI files or the indexed-art pipeline,
- a mod/asset-pack can ship PNGs without replacing base data.

### 5.3 Transparency / colour-key

Indexed art keys transparency on **palette index 0** (which the LUT now maps to
opaque `0xFF000000`, colour-keyed on RGB==0 — see `Blit16_ColorKey`). PNGs bring
a **real alpha channel**, which is strictly better. Decide per-pipeline whether a
converted sprite should:

- use **true alpha** (preferred — needs the consuming blit to honour the source
  alpha; the `Blt32BPPTo16BPP*` ARGB blitters already read `alpha = src>>24`), or
- keep **colour-key** semantics for drop-in parity (key on a chosen RGB).

This is the main place where "just convert the PNG" needs a conscious decision
per art category.

### 5.4 The palette-effects problem (the real work)

Anything that today does a **palette substitution** must be reimplemented as a
**per-pixel colour operation** when the source is true-colour, because there's
no 256-entry table to swap:

| Effect today (indexed) | True-colour equivalent |
|---|---|
| Day/night shade ramps (`pShades[]`) | Multiply RGB by a light/shade factor at blit time (we already do this per-channel in `PixShade`) |
| Merc skin/hair/team recolour | Hue/tint remap or palette-region multiply applied per pixel |
| Enemy glow highlight (`CreateEnemyGlow…`) | Additive/again per-pixel tint pass |

For sprites that need these, **either keep them indexed** or budget for a
tint/shader pass. This is why mercs/tiles are poor early targets.

### 5.5 Performance / memory

- 32-bit art is **2× the memory** of 16-bit and **4× of 8-bit indexed** per
  pixel. Tiles are loaded in bulk; converting them wholesale would balloon RAM
  and texture-upload cost. Backgrounds (loaded a few at a time) are cheap.
- ETRLE compression is lost when going to PNG (PNG has its own compression on
  disk, but the in-memory surface is uncompressed). Another reason to target
  big static images, not thousands of small tiles.

### 5.6 Suggested phased order

1. **Backgrounds & menus** — biggest visual win, no palette effects, low count,
   loaded infrequently. Pure upside.
2. **Large UI panels / laptop** — same rationale.
3. **Effects with smooth gradients** (if authored as true-colour) — leverage the
   clean blending.
4. **Stop there for a long time.** Mercs/tiles/items only if/when someone is
   willing to reimplement recolour/shade as per-pixel passes and accept the
   memory cost.

---

## 6. Reference: where the pixel format lives in code

| Concern | File / symbol |
|---|---|
| Depth selector, `PIXEL`, `PixShade`, `PixIntensity`, `PixFromColor16`, `PixBlend50` | [`sgp/pixfmt.h`](../sgp/pixfmt.h) |
| 8bpp→screen palette LUT | `Create16BPPPalette` / `Create32BPPPalette` (`sgp/himage.cpp`) |
| RGB565 "logical colour" generator | `Get16BPPColor` (`sgp/himage.cpp`) |
| 32-bit source-image blit | `Blt32BPPTo16BPPTrans` / `…TransShadow` (`sgp/vobject_blitters.cpp`) |
| Translucency / alpha blends | `blendWithAlpha`, `PixBlend50`, the `…Translucent` blitters |
| Framebuffer → SDL texture | `sgp/sdl_video.cpp` (`SDL_PIXELFORMAT_ARGB8888`) |
| PNG fallback loader flags | `VSURFACE_CREATE_FROMPNG_FALLBACK`, `ImageFileType::PNG_FALLBACK` |
| Day/night tile shade ramps | `pShades[]`, `Create16BPPPaletteShaded`, `TileEngine/Shade Table Util.cpp` |

**Bottom line:** the 32-bit pipeline makes the screen a true-colour canvas and
gives us clean blending/lighting today; the PNG-fallback path lets us replace
individual sprites with true-colour art incrementally. A full art upgrade is an
asset-authoring project — best targeted at large, static, non-palette-swapped
images first.
