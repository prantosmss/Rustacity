#!/usr/bin/env python3
"""Regenerate Rustacity's image assets from the SVG sources in ``images/brand/``.

This script is the single source of truth for every raster/icon asset that
carries the Rustacity identity: application icons (Windows ``.ico``, macOS
``.icns``, Linux hicolor PNG/XPM), the window-icon XPMs, the splash/About
lockups, the installer/DMG artwork, the macOS document icons and the GNOME
mime icon.

Nothing here draws the logo: every mark, lockup and icon is rasterised straight
from the SVG files in ``images/brand/``, which are tracked with the sources so
the assets can be rebuilt from a clean clone.

Tooling
-------
* Python 3 with `Pillow <https://python-pillow.org/>`_ for all image handling.
* One SVG rasteriser, tried in this order:

  1. ``resvg_py``   (``pip install resvg-py``) - used for the assets in the tree
  2. ``node`` with ``@resvg/resvg-js`` installed next to a temporary script
  3. ``rsvg-convert`` (librsvg)
  4. ``cairosvg`` (``pip install cairosvg``)

Usage
-----
    python scripts/brand/build_brand_assets.py [--only NAME ...]

Run it from anywhere; paths are resolved relative to the repository root.
"""

from __future__ import annotations

import argparse
import io
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
BRAND = ROOT / "images" / "brand"

# --- The Rustacity palette (see images/brand/rustacity-brand-sheet.svg) ------
RUST_ORANGE = (0xD9, 0x48, 0x1F)
EMBER = (0xF4, 0x73, 0x3A)
CREAM = (0xF5, 0xE9, 0xDD)
CHARCOAL = (0x1E, 0x23, 0x27)

# The characters an XPM key may use: printable ASCII minus the two that cannot
# appear literally inside an XPM string (" and \).
XPM_KEY_CHARS = "".join(c for c in map(chr, range(33, 127)) if c not in '"\\')

# --- Brand sources ----------------------------------------------------------
ICON = BRAND / "rustacity-icon.svg"            # rounded-square app icon
MARK = BRAND / "rustacity-mark.svg"            # mark for dark backgrounds
MARK_LIGHT = BRAND / "rustacity-mark-light-bg.svg"   # mark for light backgrounds
LOGO_DARK = BRAND / "rustacity-logo-dark.svg"        # lockup for dark backgrounds
LOGO_LIGHT = BRAND / "rustacity-logo-light.svg"      # lockup for light backgrounds


# ---------------------------------------------------------------------------
# SVG rasterisation
# ---------------------------------------------------------------------------
def render(svg: Path, width: int | None = None, height: int | None = None) -> Image.Image:
    """Rasterise ``svg`` to an RGBA image, preserving the aspect ratio."""
    data = svg.read_bytes().decode("utf-8")

    try:
        import resvg_py  # type: ignore

        png = resvg_py.svg_to_bytes(svg_string=data, width=width, height=height)
        return Image.open(io.BytesIO(bytes(png))).convert("RGBA")
    except ImportError:
        pass

    node = shutil.which("node")
    if node:
        with tempfile.TemporaryDirectory() as tmp:
            script = Path(tmp) / "render.js"
            script.write_text(
                "const {Resvg}=require('@resvg/resvg-js');const fs=require('fs');\n"
                "const a=process.argv.slice(2);\n"
                "const opts={background:'rgba(0,0,0,0)'};\n"
                "if(a[1]!=='0'){opts.fitTo={mode:'width',value:parseInt(a[1],10)}}\n"
                "fs.writeFileSync(a[2],new Resvg(fs.readFileSync(a[0],'utf8'),opts).render().asPng());\n"
            )
            out = Path(tmp) / "out.png"
            env = dict(os.environ, NODE_PATH=str(ROOT.parent))
            subprocess.run(
                [node, str(script), str(svg), str(width or 0), str(out)],
                check=True, env=env, capture_output=True,
            )
            return Image.open(out).convert("RGBA")

    for cli, args in (
        ("rsvg-convert", ["-w", str(width)] if width else []),
        ("cairosvg", ["-W", str(width)] if width else []),
    ):
        exe = shutil.which(cli)
        if exe:
            with tempfile.TemporaryDirectory() as tmp:
                out = Path(tmp) / "out.png"
                mode = "--output" if cli == "rsvg-convert" else "-o"
                subprocess.run([exe, *args, mode, str(out), str(svg)], check=True)
                return Image.open(out).convert("RGBA")

    sys.exit(
        "No SVG rasteriser found. Install one of: resvg-py, @resvg/resvg-js, "
        "rsvg-convert or cairosvg."
    )


# ---------------------------------------------------------------------------
# Small image helpers
# ---------------------------------------------------------------------------
def content_bbox(img: Image.Image, bg=(0, 0, 0, 0), tol: int = 6):
    """Bounding box of the pixels of ``img`` that differ from ``bg``."""
    diff = ImageChops.difference(img.convert("RGBA"), Image.new("RGBA", img.size, bg))
    return diff.convert("L").point(lambda v: 255 if v > tol else 0).getbbox()


def on_canvas(content: Image.Image, size, bg=(0, 0, 0, 0), margin=0) -> Image.Image:
    """Scale ``content`` (aspect preserved) to fit inside ``size`` and centre it."""
    canvas = Image.new("RGBA", size, bg)
    box = (max(size[0] - 2 * margin, 1), max(size[1] - 2 * margin, 1))
    scaled = content.copy()
    scaled.thumbnail(box, Image.LANCZOS)
    canvas.paste(scaled, ((size[0] - scaled.size[0]) // 2,
                          (size[1] - scaled.size[1]) // 2), scaled)
    return canvas


def lockup_plate(svg: Path, plate, size, margin_ratio=0.02) -> Image.Image:
    """A lockup lockup on its own plate, cropped to the design's clear space.

    The brand lockups carry a lot of empty plate around the artwork; cropping to
    the artwork (plus a small optical margin) and re-pasting it on a plate of the
    requested size keeps the logo as large as possible while filling the canvas
    with the plate colour, so the result is seamless on a surface of that colour.
    """
    native = render(svg)
    artwork = native.crop(content_bbox(native, bg=(*plate, 255), tol=30))
    margin = max(1, round(min(size) * margin_ratio))
    return on_canvas(artwork, size, bg=(*plate, 255), margin=margin)


def saturated_bbox(img: Image.Image, alpha_min=180, sat_min=60):
    """Bounding box of the strongly coloured (logo) pixels of an image."""
    px = img.convert("RGBA").load()
    w, h = img.size
    xs, ys = [], []
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a >= alpha_min and max(r, g, b) - min(r, g, b) > sat_min:
                xs.append(x)
                ys.append(y)
    if not xs:
        raise SystemExit("no logo pixels found")
    return (min(xs), min(ys), max(xs) + 1, max(ys) + 1)


def swap_logo(img: Image.Image, mark: Image.Image, box, scale=1.0, pad=4, dy=0):
    """Replace the logo of ``img`` with ``mark``.

    ``box`` is the region the old logo occupies (measured once from the original
    artwork; hard-coded so that re-running this script is idempotent).  The
    pixels in that box are filled with the "paper" colour around it - sampled
    with the mode of a thin ring - so that the transparent parts of the new mark
    do not punch holes in e.g. a document sheet.
    """
    src = img.convert("RGBA")
    w, h = src.size
    clear = (max(box[0] - pad, 0), max(box[1] - pad, 0),
             min(box[2] + pad, w), min(box[3] + pad, h))

    px = src.load()
    ring = {}
    for x in range(clear[0], clear[2]):
        for y in (max(clear[1] - 1, 0), min(clear[3], h - 1)):
            ring[px[x, y]] = ring.get(px[x, y], 0) + 1
    for y in range(clear[1], clear[3]):
        for x in (max(clear[0] - 1, 0), min(clear[2], w - 1)):
            ring[px[x, y]] = ring.get(px[x, y], 0) + 1
    opaque = {c: n for c, n in ring.items() if c[3] >= 250}
    fill = max(opaque, key=opaque.get) if opaque else (255, 255, 255, 255)

    content = mark.copy()
    content.thumbnail((round((box[2] - box[0]) * scale), round((box[3] - box[1]) * scale)),
                      Image.LANCZOS)
    cx = (box[0] + box[2]) // 2
    cy = (box[1] + box[3]) // 2 + dy

    out = src.copy()
    out.paste(fill, clear)
    out.paste(content, (cx - content.size[0] // 2, cy - content.size[1] // 2), content)
    return out


# ---------------------------------------------------------------------------
# Writers
# ---------------------------------------------------------------------------
def png_bytes(img: Image.Image) -> bytes:
    buf = io.BytesIO()
    img.convert("RGBA").save(buf, format="PNG")
    return buf.getvalue()


def existing_symbol(path: Path, fallback: str) -> str:
    """Keep the C symbol of an XPM file we are about to replace."""
    if path.exists():
        m = re.search(r"const\s+char\s*\*?\s*const\s+(\w+)\s*\[\]", path.read_text(errors="ignore"))
        if m:
            return m.group(1)
        m = re.search(r"char\s*\*\s*const\s+(\w+)\s*\[\]", path.read_text(errors="ignore"))
        if m:
            return m.group(1)
    return fallback


def write_xpm(img: Image.Image, path: Path, fallback_symbol: str) -> None:
    """Write an XPM3 file, keeping the symbol the C++ source expects.

    XPM has no partial transparency; pixels with alpha < 128 become ``None``
    (a 1-bit mask), exactly like the hand-made XPMs that are being replaced.
    """
    img = img.convert("RGBA")
    w, h = img.size
    px = img.load()
    palette: dict[str, int] = {}
    order: list[str] = []
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            r, g, b, a = px[x, y]
            key = "None" if a < 128 else "#%02X%02X%02X" % (r, g, b)
            if key not in palette:
                palette[key] = len(order)
                order.append(key)
            row.append(palette[key])
        rows.append(row)

    cpp = 1
    while len(order) > len(XPM_KEY_CHARS) ** cpp:
        cpp += 1
    keys = [
        "".join(XPM_KEY_CHARS[(i // (len(XPM_KEY_CHARS) ** k)) % len(XPM_KEY_CHARS)]
                for k in range(cpp))
        for i in range(len(order))
    ]
    keyof = {k: keys[i] for i, k in enumerate(order)}

    out = ["/* XPM */",
           "static const char * const %s[] = {" % existing_symbol(path, fallback_symbol),
           '   "%d %d %d %d",' % (w, h, len(order), cpp)]
    out += ['   "%s\tc %s",' % (keys[i], k) for i, k in enumerate(order)]
    out += ['   "%s",' % "".join(keyof[order[i]] for i in rows[y]) for y in range(h)]
    out.append("};")
    # No newline= override: these XPMs are tracked text under "* text=auto", so
    # let Python write the platform's line ending (CRLF on Windows) - the same
    # form git checks the rest of the tree out in.  Forcing "\n" made every
    # regenerated XPM look modified after a run even though nothing had changed.
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


def dib_bytes(img: Image.Image) -> bytes:
    """A 32bpp BITMAPINFOHEADER DIB (with AND mask) for an ICO entry."""
    w, h = img.size
    header = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    px = img.convert("RGBA").load()
    body = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b, a = px[x, y]
            body += bytes((b, g, r, a))
    mask_row = ((w + 31) // 32) * 4
    mask = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray(mask_row)
        for x in range(w):
            if px[x, y][3] == 0:
                row[x // 8] |= 0x80 >> (x % 8)
        mask += row
    return header + bytes(body) + bytes(mask)


def write_ico(path: Path, sizes, renderer) -> None:
    entries = []
    for size in sizes:
        img = renderer(size)
        payload = png_bytes(img) if size >= 96 else dib_bytes(img)
        entries.append((size, payload))
    directory = bytearray()
    offset = 6 + 16 * len(entries)
    for size, payload in entries:
        directory += struct.pack(
            "<BBBBHHII", size if size < 256 else 0, size if size < 256 else 0,
            0, 0, 1, 32, len(payload), offset,
        )
        offset += len(payload)
    blob = struct.pack("<HHH", 0, 1, len(entries)) + bytes(directory)
    for _, payload in entries:
        blob += payload
    path.write_bytes(blob)


def write_icns(path: Path, chunks, info: bytes | None = None) -> None:
    """Write an ICNS container.

    ``chunks`` maps an ICNS type (``ic07``, ``ic11``, ...) to an RGBA image; the
    PNG representation is stored, which is what macOS 10.7 and later read.  The
    legacy raw/is32 style chunks of the original files are not reproduced: they
    are the <= 10.6 path, and they are the only place the old logo would survive.
    """
    body = bytearray()
    if info:
        body += b"info" + struct.pack(">I", 8 + len(info)) + info
    for typ, img in chunks.items():
        payload = png_bytes(img)
        body += typ.encode("ascii") + struct.pack(">I", 8 + len(payload)) + payload
    path.write_bytes(b"icns" + struct.pack(">I", 8 + len(body)) + bytes(body))


def read_icns(path: Path):
    """Yield ``(type, payload)`` chunks of an existing ICNS file."""
    data = path.read_bytes()
    if data[:4] != b"icns":
        raise SystemExit(f"{path} is not an ICNS file")
    pos = 8
    while pos + 8 <= len(data):
        typ = data[pos:pos + 4]
        size = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        yield typ.decode("latin1"), data[pos + 8:pos + size]
        pos += size


def icns_master(path: Path, size=512) -> Image.Image:
    """The largest PNG inside an ICNS file."""
    best = None
    for typ, payload in read_icns(path):
        if payload[:8] != b"\x89PNG\r\n\x1a\n":
            continue
        img = Image.open(io.BytesIO(payload))
        if best is None or img.size[0] > best.size[0]:
            best = img
    if best is None:
        raise SystemExit(f"no PNG chunk found in {path}")
    if best.size[0] != size:
        best = best.resize((size, size), Image.LANCZOS)
    return best.convert("RGBA")


APP_ICNS_CHUNKS = ("ic11", "ic12", "ic07", "ic13", "ic08", "ic14", "ic09", "ic10")
ICNS_SIZE = {
    "icp4": 16, "icp5": 32, "icp6": 64,
    "ic11": 32, "ic12": 64, "ic07": 128, "ic13": 256,
    "ic08": 256, "ic14": 512, "ic09": 512, "ic10": 1024,
}


# ---------------------------------------------------------------------------
# The asset list
# ---------------------------------------------------------------------------
ASSETS: dict[str, callable] = {}


def asset(fn):
    ASSETS[fn.__name__] = fn
    return fn


# --- application icons -----------------------------------------------------
HICOLOR_PNGS = {
    "images/icons/16x16/rustacity.png": 16,
    "images/icons/22x22/rustacity.png": 22,
    "images/icons/24x24/rustacity.png": 24,
    "images/icons/32x32/rustacity.png": 32,
    "images/icons/48x48/rustacity.png": 48,
}

HICOLOR_XPMS = {
    "images/icons/16x16/rustacity16.xpm": (16, "audacity16_xpm"),
    "images/icons/32x32/rustacity32.xpm": (32, "audacity32_xpm"),
    "images/icons/48x48/rustacity.xpm": (48, "AudacityLogo48x48_xpm"),
}

ICO_SIZES = (8, 10, 14, 16, 24, 32, 48, 64, 96, 128, 256)


@asset
def app_icons():
    for rel, size in HICOLOR_PNGS.items():
        render(ICON, width=size).save(ROOT / rel)
    for rel, (size, symbol) in HICOLOR_XPMS.items():
        write_xpm(render(ICON, width=size), ROOT / rel, symbol)
    # The scalable Linux icon is the brand SVG itself.
    (ROOT / "images/rustacity.svg").write_bytes(ICON.read_bytes())
    for rel in ("win/rustacity.ico", "win/darkaudacity.ico"):
        write_ico(ROOT / rel, ICO_SIZES, lambda s: render(ICON, width=s))
    info = next((p for t, p in read_icns(ROOT / "mac/Resources/Rustacity.icns") if t == "info"), None)
    write_icns(
        ROOT / "mac/Resources/Rustacity.icns",
        {t: render(ICON, width=ICNS_SIZE[t]) for t in APP_ICNS_CHUNKS},
        info=info,
    )


@asset
def window_icons():
    """The XPM logos the frames use for their window icons."""
    write_xpm(on_canvas(render(ICON, width=190), (215, 190)), ROOT / "images/RustacityLogo.xpm",
              "RustacityLogo_xpm")
    write_xpm(render(ICON, width=48), ROOT / "images/RustacityLogo48x48.xpm",
              "RustacityLogo48x48_xpm")
    write_xpm(render(ICON, width=48), ROOT / "images/RustacityLogoAlpha.xpm",
              "RustacityLogoAlpha_xpm")


# --- splash / About / documentation logos ----------------------------------
@asset
def splash_and_about():
    """The lockup images used by the splash dialog, the About dialog, the
    startup splash screen and (in a DarkAudacity build) their dark twins.

    Both dialogs take their background colour from pixel (1,1) of this bitmap,
    so the cream/charcoal plate doubles as the dialog background.
    """
    lockup = lockup_plate(LOGO_LIGHT, CREAM, (506, 200))
    write_xpm(lockup, ROOT / "images/RustacityLogoWithName.xpm", "RustacityLogoWithName_xpm")

    dark = lockup_plate(LOGO_DARK, CHARCOAL, (629, 200))
    write_xpm(dark, ROOT / "images/DarkAudacityLogoWithName.xpm", "RustacityLogoWithName_xpm")


@asset
def doc_logos():
    """The manual/documentation logos and the leftover 'large logo' SVG.

    Both PNGs are header logos for the generated HTML manual: the 155x147 one is
    the copy ``scripts/mw2html_rustacity/mw2html.py`` deploys to the manual site,
    the 80x67 one is the older, unused copy of the same asset.
    """
    write_png(on_canvas(render(ICON, width=294), (155, 147)), ROOT / "scripts/mw2html_rustacity/RustacityLogo.png")
    write_png(on_canvas(render(ICON, width=134), (80, 67)), ROOT / "images/RustacityLogo.png")
    (ROOT / "images/Rustacity_Logo_large.svg").write_bytes(ICON.read_bytes())


def write_png(img: Image.Image, path: Path) -> None:
    img.convert("RGBA").save(path)


# --- macOS document icons and the GNOME mime icon --------------------------
DOC_ICNS = ("RustacityAIFF.icns", "RustacityAU.icns", "RustacityMP3.icns",
            "RustacityOGG.icns", "RustacityProject.icns", "RustacityWAV.icns")

DOC_ICNS_CHUNKS = ("ic11", "ic12", "ic07", "ic13", "ic08", "ic14", "ic09")

# Where the old Audacity logo sits in the 512x512 sheet of every document icon
# (identical in all six), and in the 48x48 GNOME mime icon.
DOC_LOGO_BOX = (112, 118, 402, 362)
MIME_LOGO_BOX = (13, 13, 40, 36)


@asset
def document_icons():
    """Keep each document icon's sheet and label, replace the logo inside it."""
    mark = render(MARK_LIGHT, width=512)
    for name in DOC_ICNS:
        path = ROOT / "mac/Resources" / name
        master = swap_logo(icns_master(path, 512), mark, DOC_LOGO_BOX, scale=0.92)
        write_icns(path, {t: master.resize((ICNS_SIZE[t],) * 2, Image.LANCZOS)
                          for t in DOC_ICNS_CHUNKS})

    mime = ROOT / "images/gnome-mime-application-x-audacity-project.xpm"
    new = swap_logo(read_xpm(mime), render(MARK_LIGHT, width=128), MIME_LOGO_BOX,
                    scale=0.98, pad=2)
    write_xpm(new, mime, "gnome_mime_application_x_audacity_project_xpm")


def read_xpm(path: Path) -> Image.Image:
    """Minimal XPM reader (only used for the two files we re-compose)."""
    text = path.read_text(encoding="utf-8", errors="replace").replace("/* XPM */", "")
    strings = re.findall(r'"((?:[^"\\]|\\.)*)"', text)
    w, h, ncol, cpp = (int(v) for v in strings[0].split()[:4])
    colours = {}
    for i in range(1, ncol + 1):
        key = strings[i][:cpp]
        m = re.search(r"c\s+([^\s\"\t]+)", strings[i][cpp:])
        colours[key] = (m.group(1) if m else "None")
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = img.load()
    for y in range(h):
        row = strings[1 + ncol + y]
        for x in range(w):
            val = colours[row[x * cpp:(x + 1) * cpp]]
            if val.lower() == "none":
                continue
            val = val.lstrip("#")
            if len(val) == 12:
                px[x, y] = tuple([int(val[i:i + 4], 16) >> 8 for i in (0, 4, 8)] + [255])
            elif len(val) == 6:
                px[x, y] = tuple([int(val[i:i + 2], 16) for i in (0, 2, 4)] + [255])
            else:
                px[x, y] = (0, 0, 0, 255)
    return img


# --- installer / DMG artwork ----------------------------------------------
# The old Audacity lockup in the DMG background; cleared before the new one is
# pasted.  The right-hand edge also covers the old trademark (R) glyph.
DMG_LOGO_BOX = (52, 22, 556, 156)


@asset
def installer_art():
    # Inno Setup wizard side panel (164x314): the original is a white panel with
    # the lockup; give the panel the brand's light surface colour instead.
    panel = Image.new("RGBA", (164, 314), (*CREAM, 255))
    lockup = render(LOGO_LIGHT)
    lockup = lockup.crop(content_bbox(lockup, bg=(*CREAM, 255), tol=30))
    lockup.thumbnail((148, 117), Image.LANCZOS)
    panel.paste(lockup, ((164 - lockup.size[0]) // 2, 26 + (117 - lockup.size[1]) // 2), lockup)
    panel.convert("RGB").save(ROOT / "win/Inno_Setup_Wizard/rustacity_InnoWizardImage.bmp")

    small = on_canvas(render(ICON, width=32), (32, 32), bg=(255, 255, 255, 255))
    small.convert("RGB").save(ROOT / "win/Inno_Setup_Wizard/rustacity_InnoWizardSmallImage.bmp")

    # macOS DMG background (600x450): keep the original's layout (install bar,
    # instructions and drag arrow - all of which carry text and are out of
    # scope) and replace only the Audacity lockup in the top left corner.
    dmg = ROOT / "mac/Resources/Rustacity-DMG-background.png"
    base = Image.open(dmg).convert("RGBA")
    base.paste((255, 255, 255, 255), DMG_LOGO_BOX)   # clear box, incl. the old (R)
    logo = render(LOGO_LIGHT)
    logo = logo.crop(content_bbox(logo, bg=(*CREAM, 255), tol=30))
    logo.thumbnail((306, 104), Image.LANCZOS)
    base.paste(logo, (64, 34 + (104 - logo.size[1]) // 2), logo)
    base.convert("RGB").save(dmg)


# ---------------------------------------------------------------------------
def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--only", nargs="*", choices=sorted(ASSETS),
                        help="regenerate only these asset groups")
    args = parser.parse_args(argv)

    groups = args.only or sorted(ASSETS)
    for name in groups:
        ASSETS[name]()
        print(f"  {name}: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
