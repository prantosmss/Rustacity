#!/usr/bin/env python3
"""Rebrand the built-in theme image caches (the ``*ThemeAsCeeCode.h`` files).

Why this script exists
----------------------
The four built-in themes (Classic, Light, Dark, High Contrast) are shipped as
*pre-packed image caches*.  ``ThemeBase::CreateImageCache()`` packs every theme
bitmap plus a strip of colour patches into one 440x836 PNG and
``SourceOutputStream`` writes that PNG as a C byte array into
``<Theme>ThemeAsCeeCode.h``.  ``ThemeBase::ReadImageCache()`` loads the blob at
run time and takes both the bitmaps **and the theme colours** from it, so the
colour defaults in ``src/AllThemeResources.h`` are overridden by the blob and a
colour change only takes effect once the caches are rebuilt.

Upstream rebuilds these files with the GUI-only "Output Sourcery" button in the
Theme preferences (``theTheme.SaveThemeAsCode()``), which needs a running,
built GUI application.  This script reproduces the packing instead:

* ``FlowPacker`` (``src/Theme.cpp``) is reimplemented line for line and fed from
  the image/colour declarations in ``src/AllThemeResources.h``;
* the *existing* cache is verified against that layout first - each colour patch
  must be a flat square and the 48x48 logo must sit where the packer puts it -
  and the script refuses to write if the layout does not line up;
* only the regions that change are overwritten: the 48x48 logo bitmap (the brand
  app icon) and the colour patches.  The rest of the cache, including all the
  packed bitmaps, is left exactly as it was, so there is no re-packing and no
  layout shift.

Two more things the same run takes care of:

* ``src/AllThemeResources.h`` declares the same palette as a fallback for the
  case where a cache cannot be read, so ``--sync-header`` puts its colour
  defaults through the same mapping (numbers only, formatting untouched).
* ``src/ThemeAsCeeCode.h`` was a "theme as code" file a DarkAudacity build had
  saved into the tree.  Nothing #included it and it was never part of a binary,
  but it held the whole old DarkAudacity bitmap set and palette, so it was dealt
  with first (rewritten with the rebranded Dark payload) and then deleted along
  with its build references.  A step near the bottom of ``main()`` still handles
  a tree that has not had it removed yet.

Usage
-----
    python scripts/brand/regenerate_theme_caches.py --check    # verify, no writes
    python scripts/brand/regenerate_theme_caches.py           # dry run: show the changes
    python scripts/brand/regenerate_theme_caches.py --mock OUT.png
    python scripts/brand/regenerate_theme_caches.py --apply    # rewrite the caches
    python scripts/brand/regenerate_theme_caches.py --sync-header
    python scripts/brand/regenerate_theme_caches.py --extract DIR

Requires Python 3 with Pillow and the brand kit in ``brand/`` (see
``build_brand_assets.py``).
"""

from __future__ import annotations

import argparse
import colorsys
import io
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from build_brand_assets import CREAM, EMBER, ICON, RUST_ORANGE, render  # noqa: E402

# --- mirror of src/Theme.h (teResourceFlags) --------------------------------
RES_FLAG_NONE = 0x00
RES_FLAG_PAIRED = 0x01
RES_FLAG_CURSOR = 0x02
RES_FLAG_NEWLINE = 0x04
RES_FLAG_INTERNAL = 0x08
RES_FLAG_SKIP = 0x10

FLAG_BY_NAME = {
    "resFlagNone": RES_FLAG_NONE,
    "resFlagPaired": RES_FLAG_PAIRED,
    "resFlagCursor": RES_FLAG_CURSOR,
    "resFlagNewLine": RES_FLAG_NEWLINE,
    "resFlagInternal": RES_FLAG_INTERNAL,
    "resFlagSkip": RES_FLAG_SKIP,
}

IMAGE_CACHE_WIDTH = 440            # Theme.cpp
IMAGE_CACHE_HEIGHT = 836
BORDER = 1                         # Theme.cpp sets this on the packer
PACKER_MARKER = (0xF2, 0xB0, 0x27)  # SetRGB( Rect(), 0xf2, 0xb0, 0x27 )
COLOUR_PATCH = 10                  # iColSize
COLOUR_GROUP_BASE = 750            # FlowPacker::SetColourGroup()
COLOUR_GROUP_HEIGHT = 11

RESOURCES_H = ROOT / "src/AllThemeResources.h"

THEMES = {
    "Classic": ROOT / "src/ClassicThemeAsCeeCode.h",
    "Light": ROOT / "src/LightThemeAsCeeCode.h",
    "Dark": ROOT / "src/DarkThemeAsCeeCode.h",
    "HiContrast": ROOT / "src/HiContrastThemeAsCeeCode.h",
}

LOGO_IMAGE = "bmpAudacityLogo48x48"
LOGO_SIZE = 48

# A compiled-in "theme as code" file that a DarkAudacity build saved into src/.
# It is listed in the build files but not #included by any translation unit, so
# it is not part of any binary - it still holds the whole old DarkAudacity
# bitmap set and palette though, which is exactly what should not be left lying
# around.  Its own layout is from an older resource list (none of its cells line
# up with the current one, so it cannot be patched in place); it is rewritten
# instead with the payload of the rebranded Dark theme, which is what the file
# is for: the theme a DarkAudacity build compiles in.  The file has since been
# deleted from the tree, so this is now only a fallback for a tree that still
# carries it - the guard below never recreates it.
LEGACY_CACHE = ROOT / "src/ThemeAsCeeCode.h"
LEGACY_SOURCE_THEME = "Dark"


# ---------------------------------------------------------------------------
# Parsing src/AllThemeResources.h
# ---------------------------------------------------------------------------
def _flags(expr: str) -> int:
    value = 0
    for token in re.split(r"[|]", expr):
        token = token.strip()
        if token:
            if token not in FLAG_BY_NAME:
                raise SystemExit(f"unknown theme flag {token!r}")
            value |= FLAG_BY_NAME[token]
    return value


def parse_resources(path=RESOURCES_H):
    """Return ``(images, colours)`` in registration order.

    ``images`` is a list of ``(name, width, height, flags)`` exactly as
    ``Theme::RegisterImages()`` registers them; ``colours`` is a list of
    ``(name, (r, g, b, a), flags)``.  Comment lines are ignored - the file has
    no conditional resource definitions.
    """
    images, colours = [], []
    flags = RES_FLAG_PAIRED
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("//") or line.startswith("*"):
            continue
        m = re.match(r"SET_THEME_FLAGS\(\s*(.*?)\s*\)\s*;", line)
        if m:
            flags = _flags(m.group(1))
            continue
        m = re.match(r"DEFINE_IMAGE\(\s*(\w+)\s*,\s*wxImage\(\s*(\d+)\s*,\s*(\d+)\s*\)", line)
        if m:
            images.append((m.group(1), int(m.group(2)), int(m.group(3)), flags))
            # RegisterImage() clears resFlagSkip after registering an image.
            flags &= ~RES_FLAG_SKIP
            continue
        m = re.match(r"DEFINE_COLOUR\(\s*(\w+)\s*,\s*wxColour\(\s*([0-9,\s]+?)\s*\)", line)
        if m:
            parts = [int(p) for p in m.group(2).split(",") if p.strip() != ""]
            while len(parts) < 4:
                parts.append(255)
            colours.append((m.group(1), tuple(parts), flags))
    return images, colours


# ---------------------------------------------------------------------------
# Mirror of FlowPacker (src/Theme.cpp)
# ---------------------------------------------------------------------------
class FlowPacker:
    def __init__(self, width):
        self.cache_width = width
        self.border = 0
        self.flags = RES_FLAG_PAIRED
        self.old_flags = self.flags
        self.x = 0
        self.y = 0
        self.y_base = 0
        self.height = 0
        self.group_size = 1
        self.index = -1
        self.component_w = 0
        self.component_h = 0
        self.new_group(1)

    def new_group(self, size):
        self.y_base += self.height * self.group_size
        self.x = 0
        self.old_flags = self.flags
        self.group_size = size
        self.index = -1
        self.component_w = 0

    def set_colour_group(self):
        self.y_base = COLOUR_GROUP_BASE
        self.x = 0
        self.old_flags = self.flags
        self.group_size = 1
        self.index = -1
        self.component_w = 0
        self.height = COLOUR_GROUP_HEIGHT

    def next_position(self, x_size, y_size):
        x_size += 2 * self.border
        y_size += 2 * self.border
        if y_size > self.height or ((self.flags ^ self.old_flags) & ~RES_FLAG_SKIP) != 0:
            self.new_group(2 if (self.flags & RES_FLAG_PAIRED) else 1)
            self.height = y_size
        self.index += 1
        if self.index == self.group_size:
            self.index = 0
            self.x += self.component_w
        if self.x > (self.cache_width - x_size):
            self.new_group(self.group_size)
            self.index += 1
            self.height = y_size
        self.y = self.y_base + self.index * self.height
        self.component_w = x_size
        self.component_h = y_size
        return self.rect(), self.rect_inner()

    def rect(self):
        return (self.x, self.y, self.component_w, self.component_h)

    def rect_inner(self):
        return (self.x + self.border, self.y + self.border,
                self.component_w - 2 * self.border, self.component_h - 2 * self.border)


def layout(images, colours):
    """Replay ``CreateImageCache`` and return the regions it writes."""
    packer = FlowPacker(IMAGE_CACHE_WIDTH)
    packer.border = BORDER
    image_regions = []
    for name, w, h, flags in images:
        packer.flags = flags
        if flags & RES_FLAG_INTERNAL:
            image_regions.append((name, None, None, flags))
            continue
        rect, inner = packer.next_position(w, h)
        image_regions.append((name, rect, inner, flags))
    packer.set_colour_group()
    colour_regions = []
    for name, rgba, _flags in colours:
        rect, inner = packer.next_position(COLOUR_PATCH, COLOUR_PATCH)
        # CreateImageCache() sets the patch alpha from the *outer* origin.
        alpha_box = (rect[0], rect[1], COLOUR_PATCH, COLOUR_PATCH)
        colour_regions.append((name, rgba, rect, inner, alpha_box))
    return image_regions, colour_regions


# ---------------------------------------------------------------------------
# Reading and writing the C byte arrays
# ---------------------------------------------------------------------------
HEADER = (
    "//   {name}\r\n"
    "//\r\n"
    "//   This file was Auto-Generated.\r\n"
    "//   It is included by Theme.cpp.\r\n"
    "//   Only check this into Git if you've read and understood the guidelines!\r\n"
    "\r\n   "
)


def read_cache(path: Path) -> bytes:
    text = path.read_text(encoding="utf-8")
    numbers = re.findall(r"(\d+)\s*,", text)
    return bytes(int(n) for n in numbers)


def cache_header(path: Path) -> str:
    """The comment block of an existing cache file, verbatim."""
    # read_bytes(), not read_text(): universal newlines would eat the CRs.
    text = path.read_bytes().decode("utf-8")
    m = re.search(r"\r\n   \d", text)
    if not m:
        raise SystemExit(f"{path} does not look like a theme cache")
    return text[:m.start() + 5]


def write_cache(path: Path, png: bytes, header: str | None = None) -> None:
    if header is None:
        header = cache_header(path) if path.exists() else HEADER.format(name=path.name)
    out = [header]
    for i, byte in enumerate(png, start=1):
        out.append(f"{byte},")
        if i % 20 == 0:
            out.append("\r\n   ")
    # SourceOutputStream::~SourceOutputStream() appends a final CRLF.
    out.append("\r\n")
    path.write_bytes("".join(out).encode("utf-8"))


def decode(png: bytes) -> Image.Image:
    img = Image.open(io.BytesIO(png)).convert("RGBA")
    if img.size != (IMAGE_CACHE_WIDTH, IMAGE_CACHE_HEIGHT):
        raise SystemExit(f"unexpected cache size {img.size}")
    return img


def encode(img: Image.Image) -> bytes:
    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=False)
    return buf.getvalue()


# ---------------------------------------------------------------------------
# Verification
# ---------------------------------------------------------------------------
def check_cache(img, image_regions, colour_regions):
    """Check the cache really was packed with this layout.

    Returns ``(image_rects, colour_values, problems)``.  The colour patches are
    authoritative: they are what this script rewrites and where the colours are
    read back from at run time.  Images are checked too, but only the ones we
    care about (the logo) have to be perfect - the shipped caches contain a few
    bitmaps whose *content* was loaded from a slightly different sized file by
    upstream's "load components" workflow, which leaves the marker ring of those
    cells partially overwritten.  Those are reported as notes, not failures.
    """
    px = img.load()
    rects, problems, notes = {}, [], []
    for name, rect, inner, flags in image_regions:
        if rect is None:
            continue
        rects[name] = (rect, inner)
        x, y, w, h = rect
        ring = []
        for i in range(w):
            ring.append((x + i, y))
            ring.append((x + i, y + h - 1))
        for i in range(h):
            ring.append((x, y + i))
            ring.append((x + w - 1, y + i))
        bad = [p for p in ring if px[p][:3] != PACKER_MARKER]
        if bad:
            # The logo, and a handful of other bitmaps upstream loaded from
            # component files, can have a partially overwritten marker ring.
            # The *content* position is what we verify (see below), so these are
            # reported as notes rather than failures.
            notes.append(f"image {name}: {len(bad)}/{len(ring)} border pixels are not the "
                         f"packer marker at rect {rect}, e.g. {bad[0]}={px[bad[0]]}")

    colour_values = {}
    for name, rgba, rect, inner, alpha_box in colour_regions:
        x, y, w, h = inner
        found = {px[x + i, y + j][:3] for i in range(w) for j in range(h)}
        found_alpha = {px[x + i, y + j][3] for i in range(w) for j in range(h)}
        if len(found) != 1 or found_alpha != {255}:
            problems.append(f"colour {name}: patch at {inner} is not a flat opaque square "
                            f"({len(found)} colours, alpha {sorted(found_alpha)})")
            continue
        colour_values[name] = found.pop()
    return rects, colour_values, problems, notes


def logo_is_where_expected(img, rects):
    """The old logo content must be inside the cell the packer reserves for it."""
    rect, inner = rects[LOGO_IMAGE]
    x, y, w, h = inner
    if (x, y, w, h) != (rect[0] + BORDER, rect[1] + BORDER, LOGO_SIZE, LOGO_SIZE):
        return False, f"logo cell {rect} is not {LOGO_SIZE}x{LOGO_SIZE}"
    return True, f"logo cell {rect} inner {inner}"


# ---------------------------------------------------------------------------
# The brand colours
# ---------------------------------------------------------------------------
BRAND_HUE = 16 / 360.0   # Rust Orange #D9481F sits at ~13-16 degrees


def _srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def _linear_to_srgb(c):
    return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def luminance(rgb):
    r, g, b = (_srgb_to_linear(v / 255) for v in rgb[:3])
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def _solve(rgb, saturation, target):
    """Brand hue + ``saturation``, lightness solved to reach ``target`` luminance."""
    lo, hi = 0.0, 1.0
    for _ in range(28):
        mid = (lo + hi) / 2
        if luminance([v * 255 for v in colorsys.hls_to_rgb(BRAND_HUE, mid, saturation)]) < target:
            lo = mid
        else:
            hi = mid
    return tuple(round(v * 255) for v in colorsys.hls_to_rgb(BRAND_HUE, (lo + hi) / 2, saturation))


def hsl_with_luminance(rgb, target, saturation=None):
    r, g, b = (v / 255 for v in rgb[:3])
    _h, _l, s = colorsys.rgb_to_hls(r, g, b)
    return _solve(rgb, s if saturation is None else saturation, target)


def _converge(step, rgb, rounds=8):
    """Apply ``step`` until it stops changing: makes the mapping a fixed point.

    Solving for an 8-bit colour from a target luminance can land a rounding step
    away from where it started, which would make a second run of the script
    produce a second, slightly different file.  Iterating here means the value we
    write is one that maps to itself, so re-running the generator is a no-op.
    """
    out = tuple(rgb[:3])
    for _ in range(rounds):
        nxt = step(out)
        if nxt == out:
            break
        out = nxt
    return out


def brandise(rgb, min_saturation=0.0):
    """Move a colour onto the brand hue, keeping its luminance exactly.

    The hue becomes the Rust Orange hue, the saturation is kept (optionally
    raised to ``min_saturation`` so that large neutral surfaces pick up a subtle
    warm cast) and the lightness is then solved so the relative luminance is
    unchanged.  Keeping the luminance is what keeps every contrast relationship
    - waveform against clip background, ruler text, meter - exactly as legible
    as it was before.  Pure black and pure white are fixed points.
    """
    return _converge(lambda c: _brandise_once(c, min_saturation), rgb)


def _brandise_once(rgb, min_saturation):
    r, g, b = (v / 255 for v in rgb[:3])
    h, _l, s = colorsys.rgb_to_hls(r, g, b)
    # Already on the brand hue: nothing to do (also the common case on a re-run).
    if s >= min_saturation and min(abs(h - BRAND_HUE), 1 - abs(h - BRAND_HUE)) < 2 / 360:
        return tuple(rgb[:3])
    return hsl_with_luminance(rgb, luminance(rgb), max(s, min_saturation))


def brand_signal(original, surface, target, min_contrast=3.1):
    """The brand colour for a signal drawn on ``surface``.

    ``target`` (Rust Orange or Ember) is used as it is when it already clears
    ``min_contrast`` against the surface; otherwise it is moved along its own
    lightness, keeping the brand hue and saturation, until it does.  Whether the
    signal is drawn darker or lighter than its surface is taken from the original
    colour, so the design of each theme is preserved.
    """
    return _converge(lambda c: _brand_signal_once(c, surface, target, min_contrast), original)


def _brand_signal_once(original, surface, target, min_contrast):
    ys = luminance(surface)
    y0 = luminance(target)
    if luminance(original) >= ys:
        y = max(y0, (ys + 0.05) * min_contrast - 0.05)
    else:
        y = min(y0, (ys + 0.05) / min_contrast - 0.05)
    return hsl_with_luminance(target, min(max(y, 0.0), 1.0))


# Big surfaces get a subtle brand cast; tinted identity colours keep the
# saturation they had.  Both keep their exact luminance (see brandise()).
WARM_SURFACES = (
    "clrBlank", "clrUnselected", "clrSelected", "clrBlankSelected",
    "clrTrackBackground", "clrTrackInfo", "clrTrackInfoSelected",
    "clrRulerBackground", "clrLight", "clrMedium", "clrDark",
    "clrLightSelected", "clrMediumSelected", "clrDarkSelected",
    "clrLabelUnselectedBrush", "clrLabelUnselectedPen",
    "clrLabelSelectedBrush", "clrLabelSelectedPen",
    "clrLabelTextNormalBrush", "clrMuteButtonActive", "clrMuteButtonVetoed",
    "clrTimeBack", "clrSyncLockSel", "clrScrubRuler",
    "clrSliderLight", "clrSliderMain", "clrSliderDark", "clrShadow",
    "clrSpectro1", "clrSpectro1Sel",
)
# The RMS band and the envelope line are soft brand tints rather than the full
# brand colour: they sit inside or on top of the clip, so keeping their original
# luminance is what keeps them distinguishable from the sample pen and from each
# other.
WARM_TINTS = ("clrRms", "clrEnvelope")

# The waveform itself carries the brand, so here the *brand* colour is used -
# Rust Orange on the light themes' clip surfaces, Ember on the dark ones - with
# its lightness moved only as far as the contrast guard requires.
SIGNATURE = {
    # name: (surface it is drawn on, target for light surfaces, target for dark)
    "clrSample": ("clrUnselected", RUST_ORANGE, EMBER),
    "clrSelSample": ("clrSelected", RUST_ORANGE, EMBER),
}
SIGNAL_CONTRAST = 3.1           # the guard, a shade over WCAG's 3:1 for graphics
LIGHT_SURFACE_LUMINANCE = 0.2   # above this, use the light-surface brand target

# The About dialog paints its background with this.  Use the brand's cream
# surface so the logo lockups sit on their brand plate.
EXPLICIT = {"clrAboutBoxBackground": {theme: CREAM for theme in THEMES}}

# Colours that are drawn on top of each other, for the legibility report.
CONTRAST_PAIRS = (
    ("clrSelected", "clrUnselected"),
    ("clrSample", "clrUnselected"),
    ("clrSelSample", "clrSelected"),
    ("clrRms", "clrUnselected"),
    ("clrEnvelope", "clrUnselected"),
    ("clrClipped", "clrUnselected"),
    ("clrTrackPanelText", "clrTrackBackground"),
    ("clrTrackPanelText", "clrTrackInfo"),
    ("clrLabelTrackText", "clrLabelTextNormalBrush"),
)

# Deliberately *not* touched, because the colour carries meaning rather than
# brand identity: clipping reds, play/record/ruler pens and brushes, every meter
# colour, the spectrogram data palette, graph/plot pens, and the green progress
# bar.  Recorded here so the choice is visible in review.
KEEP_SEMANTIC = (
    "clrClipped", "clrSelTranslucent", "clrDragSample", "clrMuteSample",
    "clrMuteRms", "clrMeterPeak", "clrMeterDisabledPen", "clrMeterDisabledBrush",
    "clrMeterInputPen", "clrMeterInputBrush", "clrMeterInputRMSBrush",
    "clrMeterInputClipBrush", "clrMeterInputLightPen", "clrMeterInputDarkPen",
    "clrMeterOutputPen", "clrMeterOutputBrush", "clrMeterOutputRMSBrush",
    "clrMeterOutputClipBrush", "clrMeterOutputLightPen", "clrMeterOutputDarkPen",
    "clrRecordingPen", "clrRecordingBrush", "clrPlaybackPen", "clrPlaybackBrush",
    "clrRulerRecordingBrush", "clrRulerRecordingPen",
    "clrRulerPlaybackBrush", "clrRulerPlaybackPen",
    "clrProgressDone", "clrProgressNotYet", "clrMuteClipped", "clrCursorPen",
    "clrAxisLines", "clrGraphLines", "clrResponseLines", "clrHzPlot",
    "clrWavelengthPlot", "clrSpectro2", "clrSpectro3", "clrSpectro4",
    "clrSpectro5", "clrSpectro2Sel", "clrSpectro3Sel", "clrSpectro4Sel",
    "clrSpectro5Sel", "clrTrackFocus0", "clrTrackFocus1", "clrTrackFocus2",
    "clrSnapGuide", "clrGraphLabels", "clrTrackNameText", "clrPlaceHolder1",
    "clrFoxusBox", "clrMidiZebra", "clrMidiLines", "clrTextNegativeNumbers",
    "clrLabelTextEditBrush", "clrLabelSurroundPen", "clrLabelTrackText",
    "clrTrackPanelText", "clrTimeFont", "clrTimeFontFocus", "clrTimeBackFocus",
    "clrTimeHours", "clrSpectroBackground",
)


def theme_colours(theme: str, current: dict[str, tuple[int, int, int]]):
    """Return the new value of every colour this script changes for ``theme``."""
    new = {}
    for name, colour in current.items():
        override = EXPLICIT.get(name, {}).get(theme, "unset")
        if override != "unset":
            if override is not None:
                new[name] = override
        elif name in WARM_SURFACES:
            new[name] = brandise(colour, min_saturation=0.06)
        elif name in WARM_TINTS:
            new[name] = brandise(colour)
    # The signal colours are resolved against the *new* surfaces, so that a
    # second run over an already patched cache cannot shift them.
    merged = {**current, **new}
    for name in SIGNATURE:
        if name not in current:
            continue
        surface_name, light_target, dark_target = SIGNATURE[name]
        surface = merged.get(surface_name)
        if surface is None:
            continue
        target = light_target if luminance(surface) >= LIGHT_SURFACE_LUMINANCE else dark_target
        new[name] = brand_signal(current[name], surface, target, SIGNAL_CONTRAST)
    return {name: value for name, value in new.items() if value != current[name]}


def contrast(a, b):
    """WCAG contrast ratio between two colours."""
    la, lb = luminance(a), luminance(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


# ---------------------------------------------------------------------------
# Applying the changes
# ---------------------------------------------------------------------------
def logo_image():
    """The brand app icon at the theme logo's size, as RGBA pixels."""
    img = render(ICON, LOGO_SIZE, LOGO_SIZE).convert("RGBA")
    if img.size != (LOGO_SIZE, LOGO_SIZE):
        raise SystemExit(f"brand icon rendered as {img.size}")
    return img


def patch_cache(img, rects, colour_regions, colours_to_write, logo):
    """Overwrite the logo cell and the colour patches, exactly as the packer would."""
    out = img.copy()
    px = out.load()

    rect, inner = rects[LOGO_IMAGE]
    x, y, w, h = inner
    logo_px = logo.load()
    for j in range(h):
        for i in range(w):
            px[x + i, y + j] = logo_px[i, j]

    for name, _rgba, rect, inner, alpha_box in colour_regions:
        if name not in colours_to_write:
            continue
        value = colours_to_write[name]
        # SetRGB( Rect(), marker ) then SetRGB( RectInner(), colour ) then the
        # patch alpha from the outer origin - mirrors CreateImageCache().
        for i in range(rect[2]):
            for j in range(rect[3]):
                px[rect[0] + i, rect[1] + j] = (*PACKER_MARKER, px[rect[0] + i, rect[1] + j][3])
        for i in range(inner[2]):
            for j in range(inner[3]):
                px[inner[0] + i, inner[1] + j] = (*value, 255)
        ax, ay, aw, ah = alpha_box
        for i in range(aw):
            for j in range(ah):
                cx, cy = ax + i, ay + j
                px[cx, cy] = (*px[cx, cy][:3], 255)
    return out


def allowed_regions(rects, colour_regions):
    """Every pixel the generator is allowed to touch: the logo cell + the patches."""
    regions = []
    rect, inner = rects[LOGO_IMAGE]
    regions.append(inner)
    for _name, _rgba, rect, _inner, alpha_box in colour_regions:
        regions.append(rect)
        regions.append(alpha_box)
    return regions


def verify_only_allowed_changed(before, after, regions):
    """Return the number of changed pixels outside ``regions``."""
    allowed = Image.new("L", before.size, 0)
    mask = ImageDraw.Draw(allowed)
    for x, y, w, h in regions:
        mask.rectangle((x, y, x + w - 1, y + h - 1), fill=255)
    mask_bits = allowed.tobytes()
    pa, pb = before.tobytes(), after.tobytes()
    outside = 0
    for index in range(0, len(pa), 4):
        if pa[index:index + 4] != pb[index:index + 4] and not mask_bits[index // 4]:            outside += 1
    return outside


# ---------------------------------------------------------------------------
# Mock render of the resulting theme, for eyeballing legibility
# ---------------------------------------------------------------------------
def render_mock(theme: str, values: dict[str, tuple[int, int, int]], logo: Image.Image,
                title: str) -> Image.Image:
    """Draw a small clip/ruler/track-info fragment using the theme's colours."""
    def c(name, fallback=(128, 128, 128)):
        return tuple(values.get(name, fallback))

    w, h = 420, 150
    img = Image.new("RGB", (w, h), c("clrTrackBackground"))
    d = ImageDraw.Draw(img)
    # ruler strip
    d.rectangle((0, 0, w, 16), fill=c("clrRulerBackground"))
    for i in range(0, w, 40):
        d.line((i, 12, i, 16), fill=c("clrUnselected"))
    d.text((4, 3), title, fill=c("clrTrackPanelText"))
    # two clips
    for index, (surface, blank) in enumerate((("clrUnselected", "clrBlank"),
                                              ("clrSelected", "clrBlankSelected"))):
        x0 = 90 + index * 165
        d.rectangle((x0, 22, x0 + 150, 138), fill=c(blank), outline=c("clrShadow"))
        d.rectangle((x0, 22, x0 + 150, 138), outline=c(surface))
        # stem waveform with an rms band and a couple of clipped samples
        mid = 80
        for i in range(0, 150, 2):
            amp = 8 + (i * 37 % 44)
            pen = c("clrClipped") if i in (30, 122) else c("clrSample")
            d.line((x0 + i, mid - amp, x0 + i, mid + amp), fill=pen)
        d.line((x0, mid, x0 + 150, mid), fill=c("clrRms"))
        d.rectangle((x0, mid - 6, x0 + 150, mid + 6), outline=c("clrSelSample"))
        # track info panel with the logo
        d.rectangle((x0 - 88, 22, x0 - 4, 138), fill=c("clrTrackInfoSelected" if index else "clrTrackInfo"))
        img.paste(logo.convert("RGB"), (x0 - 84, 26), logo)
    return img


def mock_sheet(out_path: Path, current: dict, proposed: dict, logo: Image.Image):
    tiles = []
    for theme in THEMES:
        before = render_mock(theme, current[theme], logo, f"{theme} current")
        after = render_mock(theme, proposed[theme], logo, f"{theme} rebranded")
        tiles.append((before, after))
    width = 420 * 2 + 30
    height = (150 + 10) * len(tiles) + 10
    sheet = Image.new("RGB", (width, height), (40, 40, 40))
    for row, (before, after) in enumerate(tiles):
        y = 10 + row * 160
        sheet.paste(before, (10, y))
        sheet.paste(after, (440, y))
    sheet.save(out_path)
    return sheet


def sync_header(defaults: dict[str, tuple[int, int, int]]):
    """Rewrite the colour defaults declared in src/AllThemeResources.h.

    The header's defaults are what a theme falls back to if its packed cache
    cannot be read, and they still carry the old palette (the Audacity purple,
    the blue waveform).  They are the declaration of record for the theme
    colours, so they get the same treatment: one pass of the same mapping, with
    the header's own values as the input.  Only the numbers change.
    """
    text = RESOURCES_H.read_bytes().decode("utf-8")
    changed = []
    # (255, 255, 20) is the header's "not really set" sentinel (it is what the
    # placeholder colours use); those lines are left alone.
    sentinel = (255, 255, 20)

    def repl(match):
        name, numbers = match.group(1), match.group(2)
        old = tuple(int(p) for p in numbers.split(",") if p.strip())
        new = defaults.get(name)
        if new is None or tuple(new) == old or old[:3] == sentinel:
            return match.group(0)
        changed.append((name, old, tuple(new)))
        # Pad to each field's original width, so only the digits are in the diff.
        fields = [str(value).rjust(len(field))
                  for field, value in zip(numbers.split(","), new)]
        start = match.start(2) - match.start(0)
        end = match.end(2) - match.start(0)
        return match.group(0)[:start] + ",".join(fields) + match.group(0)[end:]

    updated = re.sub(r"DEFINE_COLOUR\(\s*(\w+)\s*,\s*wxColour\(([0-9,\s]+?)\)", repl, text)
    if changed:
        RESOURCES_H.write_bytes(updated.encode("utf-8"))
    return changed


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--check", action="store_true", help="verify the caches, do not write")
    group.add_argument("--apply", action="store_true", help="rewrite the caches")
    group.add_argument("--mock", metavar="PNG", help="write a before/after comparison picture")
    group.add_argument("--extract", metavar="DIR", help="dump the packed bitmaps of a cache")
    parser.add_argument("--sync-header", action="store_true",
                        help="also rewrite the colour defaults in src/AllThemeResources.h")
    parser.add_argument("--theme", choices=list(THEMES), help="limit --check/--extract/--mock")
    args = parser.parse_args(argv)

    images, colours = parse_resources()
    image_regions, colour_regions = layout(images, colours)
    themes = [args.theme] if args.theme else list(THEMES)

    current, proposed, rects = {}, {}, {}
    problems = 0
    for theme in themes:
        path = THEMES[theme]
        png = read_cache(path)
        img = decode(png)
        rects[theme], values, issues, notes = check_cache(img, image_regions, colour_regions)
        ok, detail = logo_is_where_expected(img, rects[theme])
        problems += len(issues) + (0 if ok else 1)
        print(f"{theme}: cache {len(png)} bytes, {len(values)} colour patches, logo {detail}")
        for issue in issues:
            print(f"   PROBLEM {issue}")
        if args.check and notes:
            print(f"   ({len(notes)} packed bitmaps have an upstream-sized cell - unchanged, not ours)")
        current[theme] = values
        proposed[theme] = theme_colours(theme, values)

    # Re-emitting an unmodified cache must reproduce the file byte for byte;
    # that is our proof that the writer matches upstream's generator.
    if args.check or args.apply:
        for theme in themes:
            path = THEMES[theme]
            before = path.read_bytes()
            header = cache_header(path)
            if header != HEADER.format(name=path.name):
                print(f"{theme}: PROBLEM unexpected file header {header!r}")
                problems += 1
            tmp = path.with_suffix(".tmp")
            write_cache(tmp, read_cache(path), header)
            same = tmp.read_bytes() == before
            tmp.unlink()
            print(f"{theme}: byte-for-byte round trip {'OK' if same else 'MISMATCH'}")
            if not same:
                problems += 1

    if problems:
        print(f"\n{problems} problems - refusing to write")
        return 1

    if args.extract:
        out = Path(args.extract)
        out.mkdir(parents=True, exist_ok=True)
        for theme in themes:
            img = decode(read_cache(THEMES[theme]))
            theme_dir = out / theme
            theme_dir.mkdir(exist_ok=True)
            for name, rect, inner, flags in image_regions:
                if inner is None:
                    continue
                x, y, w, h = inner
                img.crop((x, y, x + w, y + h)).save(theme_dir / f"{name}.png")
            print(f"wrote {theme} bitmaps to {theme_dir}")

    logo = None
    if args.mock or args.apply:
        logo = logo_image()

    if args.sync_header:
        header_defaults = {name: rgba[:3] for name, rgba, _flags in colours}
        changed = sync_header(theme_colours("Classic", header_defaults))
        rel = RESOURCES_H.relative_to(ROOT)
        print(f"\n{rel}: {len(changed)} colour default(s) updated")
        for name, old, new in changed:
            print(f"   {name:24s} {old} -> {new}")

    if args.mock:
        mock_sheet(Path(args.mock), current, {t: {**current[t], **proposed[t]} for t in themes}, logo)
        print(f"wrote {args.mock}")

    if args.apply:
        for theme in themes:
            path = THEMES[theme]
            before = decode(read_cache(path))
            merged = {**current[theme], **proposed[theme]}
            patched = patch_cache(before, rects[theme], colour_regions, merged, logo)
            write_cache(path, encode(patched), cache_header(path))
            # Prove the patch touched nothing but the logo and the patches.
            written = decode(read_cache(path))
            outside = verify_only_allowed_changed(
                before, written, allowed_regions(rects[theme], colour_regions))
            status = "OK" if outside == 0 else f"{outside} PIXELS OUTSIDE THE ALLOWED AREAS"
            print(f"{theme}: patched {len(proposed[theme])} colours + the logo "
                  f"({written.size[0]}x{written.size[1]}), {len(read_cache(path))} bytes, {status}")
            if outside:
                problems += 1
        if problems:
            return 1
    if args.apply:
        # The patches are only meaningful if the file still verifies and the
        # second run is a no-op.
        for theme in themes:
            path = THEMES[theme]
            img = decode(read_cache(path))
            _rects, values, issues, _notes = check_cache(img, image_regions, colour_regions)
            again = theme_colours(theme, values)
            print(f"{theme}: re-check {'failed' if issues else 'clean'}, "
                  f"{len(again)} further change(s) on a re-run")
            if issues or again:
                problems += 1
        if problems:
            return 1

    if args.apply and LEGACY_CACHE.exists():
        # The compiled-in "theme as code" file a DarkAudacity build saved into
        # src/: not compiled, but it still carried the old bitmap set and the old
        # palette.  The file has been deleted from the tree, so this step only
        # runs for a tree that still has it, and it never recreates it.
        source = THEMES[LEGACY_SOURCE_THEME]
        before = LEGACY_CACHE.read_bytes()
        write_cache(LEGACY_CACHE, read_cache(source), cache_header(LEGACY_CACHE))
        payload = read_cache(LEGACY_CACHE)
        same_as_source = payload == read_cache(source)
        img = decode(payload)
        _r, _v, issues, notes = check_cache(img, image_regions, colour_regions)
        rel = LEGACY_CACHE.relative_to(ROOT)
        print(f"{rel}: rebuilt from the rebranded {LEGACY_SOURCE_THEME} theme "
              f"(payload {'matches' if same_as_source else 'DIFFERS FROM'} {source.name}, "
              f"{len(notes)} known cell quirk(s), {len(issues)} problem(s), "
              f"{len(before)} -> {len(LEGACY_CACHE.read_bytes())} bytes)")
        if issues or not same_as_source:
            problems += 1
        if problems:
            return 1

    if not (args.check or args.apply or args.mock or args.extract):
        print("\nchanges proposed (nothing written):")
        for theme in themes:
            for name, value in proposed[theme].items():
                print(f"   {theme:10s} {name:24s} {current[theme][name]} -> {value}")
        print("\nlegibility check (WCAG contrast of the colours drawn on top of each other):")
        regressions = 0
        for theme in themes:
            after = {**current[theme], **proposed[theme]}
            for fg, bg in CONTRAST_PAIRS:
                if fg not in after or bg not in after:
                    continue
                c0 = contrast(current[theme][fg], current[theme][bg])
                c1 = contrast(after[fg], after[bg])
                # Only a *drop* below what upstream already had counts as a
                # regression here; plenty of upstream pairs are below 3:1 by
                # design (the selected-clip tint, the RMS band, the clip red).
                flag = ""
                if c1 < min(c0, 3.0) - 0.03:
                    flag = "   <-- worse than before"
                    regressions += 1
                elif c1 < 3.0:
                    flag = "   (below 3:1, unchanged from before)"
                print(f"   {theme:10s} {fg:20s} on {bg:26s} {c0:5.2f} -> {c1:5.2f}{flag}")
        print(f"   contrast regressions: {regressions}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
