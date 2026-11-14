#!/usr/bin/env python3
"""
Draws the window's 16 x 16 icons into tools/art/icons/<name>.png, one per
ICON_ value in src/gfx/icons.h, and a sheet of all of them, enlarged, to look
at together before converting.

Each icon is drawn as fills only, with a letter per colour (PALETTE below):
some from shapes (discs, lines, rectangles), some placed pixel by pixel as
rows of letters. A dark outline is then added around every filled pixel, so
the icons read on the blue and brown buttons and on the dark top line alike.
Fills therefore keep a pixel free at each edge for the outline.

Run:  python3 tools/art/draw_icons.py [SHEET.png]     (needs Pillow)
Then: python3 tools/art/icons_to_c.py tools/art/icons > src/gfx/icons_data.c

The PNGs are the source the converter reads: an icon can also be redrawn in
any pixel editor, as long as it keeps to 16 x 16 and these colours.
"""
import math
import os
import sys

from PIL import Image

SIZE = 16
HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.join(HERE, "icons")

PALETTE = {
    "k": (14, 14, 22),  # outline and dark details
    "w": (240, 238, 228),  # white
    "l": (184, 188, 200),  # light grey
    "g": (108, 112, 128),  # grey
    "y": (244, 200, 74),  # yellow
    "o": (224, 138, 58),  # orange
    "r": (216, 74, 58),  # red
    "G": (102, 190, 92),  # green
    "d": (47, 122, 58),  # dark green
    "b": (74, 136, 214),  # blue
    "c": (168, 216, 248),  # light blue
    "n": (138, 90, 48),  # brown
    "t": (216, 176, 120),  # tan
    "p": (236, 136, 184),  # pink
}

# The icon names in the order of src/gfx/icons.h's enum: ICON_ + NAME.upper().
NAMES = [
    "pause", "play", "slower", "faster", "zoom_out", "zoom_in", "story",
    "notices", "camera", "jump", "trait", "weather", "skip", "branch",
    "other_branch", "compare", "new_start", "mode_watch", "mode_sandbox",
    "world", "help", "close", "let_go", "spring", "summer", "autumn",
    "winter", "animals", "drought", "hard_winter", "warm_year",
]


# --- drawing helpers: a grid is a list of 16 lists of 16 letters ---------

def blank():
    return [["."] * SIZE for _ in range(SIZE)]


def setp(g, x, y, ch):
    if 0 <= x < SIZE and 0 <= y < SIZE:
        g[y][x] = ch


def rect(g, x, y, w, h, ch):
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            setp(g, xx, yy, ch)


def disc(g, cx, cy, r, ch):
    """Every pixel whose centre is within r of (cx, cy); pixel (x, y) has its
    centre at (x + 0.5, y + 0.5)."""
    for y in range(SIZE):
        for x in range(SIZE):
            if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r:
                g[y][x] = ch


def line(g, x0, y0, x1, y1, ch, r=0.75):
    """A line as discs of radius r stamped along it, in pixel-centre units."""
    steps = int(max(abs(x1 - x0), abs(y1 - y0)) * 4) + 1
    for i in range(steps + 1):
        t = i / steps
        disc(g, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, r, ch)


def put(g, rows, x=0, y=0):
    """Hand-placed pixels: rows of letters, '.' leaves the pixel as it is."""
    for dy, row in enumerate(rows):
        for dx, ch in enumerate(row):
            if ch != ".":
                setp(g, x + dx, y + dy, ch)


def mirror(rows):
    return [row[::-1] for row in rows]


def outline(g):
    """A dark pixel beside (not across a corner of) every filled pixel."""
    out = [row[:] for row in g]
    for y in range(SIZE):
        for x in range(SIZE):
            if g[y][x] != ".":
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < SIZE and 0 <= ny < SIZE and g[ny][nx] != ".":
                    out[y][x] = "k"
                    break
    return out


# --- the icons -----------------------------------------------------------

def pause():
    g = blank()
    rect(g, 3, 2, 3, 12, "w")
    rect(g, 10, 2, 3, 12, "w")
    return g


def play():
    g = blank()
    for y in range(2, 14):
        width = 1 + min(y - 2, 13 - y)
        rect(g, 5, y, width, 1, "w")
    return g


def slower():
    g = blank()
    rect(g, 2, 6, 12, 4, "w")
    return g


def faster():
    g = blank()
    rect(g, 2, 6, 12, 4, "w")
    rect(g, 6, 2, 4, 12, "w")
    return g


def magnifier(sign):
    g = blank()
    line(g, 10.5, 10.5, 13.5, 13.5, "t", 1.1)
    disc(g, 6.5, 6.5, 5.6, "w")
    disc(g, 6.5, 6.5, 4.0, "c")
    rect(g, 4, 6, 5, 1, "k")
    if sign == "+":
        rect(g, 6, 4, 1, 5, "k")
    return g


def story():
    g = blank()
    put(g, [
        ".wwwww..wwwww.",
        "wwwwwww.wwwwwww",
        "wggggww.wggggww",
        "wwwwwww.wwwwwww",
        "wgggwww.wggggww",
        "wwwwwww.wwwwwww",
        "wggggww.wgggwww",
        "wwwwwww.wwwwwww",
        "wgggggw.wggggww",
        "wwwwwww.wwwwwww",
        "nnnnnnnnnnnnnnn",
    ], 0, 2)
    # the spine between the pages
    rect(g, 7, 3, 1, 9, "g")
    return g


def notices():
    g = blank()
    for y in range(1, 15):
        half = (y - 1) // 2 + 1
        rect(g, 8 - half, y, 2 * half, 1, "y")
    rect(g, 7, 5, 2, 5, "k")
    rect(g, 7, 11, 2, 2, "k")
    return g


def camera():
    g = blank()
    put(g, [
        "....llllll....",
        "llllllllllllll",
        "lllllllllllyyl",
        "llllllllllllll",
        "llllllllllllll",
        "llllllllllllll",
        "llllllllllllll",
        "llllllllllllll",
        "llllllllllllll",
    ], 1, 3)
    disc(g, 7.5, 8.5, 3.8, "k")
    disc(g, 7.5, 8.5, 2.9, "b")
    rect(g, 6, 7, 2, 1, "c")
    rect(g, 6, 8, 1, 1, "c")
    return g


def jump():
    g = blank()
    for y in range(7, 15):
        half = max(0, (14 - y) * 5 // 7)
        rect(g, 8 - half - 1, y, 2 * half + 1, 1, "r")
    disc(g, 7.5, 6.0, 5.0, "r")
    disc(g, 7.5, 6.0, 2.0, "w")
    return g


def trait():
    """A present: the trait is given."""
    g = blank()
    put(g, [
        "..yy....yy..",
        ".y..y..y..y.",
        ".y...yy...y.",
        "..yyyyyyyy..",
    ], 2, 1)
    rect(g, 1, 5, 14, 3, "r")
    rect(g, 2, 8, 12, 6, "r")
    rect(g, 7, 5, 2, 9, "y")
    rect(g, 1, 7, 14, 1, "n")
    return g


def weather():
    g = blank()
    disc(g, 5.0, 6.5, 3.2, "w")
    disc(g, 9.0, 5.0, 4.0, "w")
    disc(g, 12.0, 7.5, 2.6, "w")
    rect(g, 2, 7, 12, 3, "w")
    rect(g, 2, 9, 12, 1, "l")
    for x, y in ((3, 11), (7, 11), (11, 11), (5, 13), (9, 13), (13, 13)):
        setp(g, x, y, "c")
        setp(g, x - 1, y + 1, "c")
    return g


def skip():
    g = blank()
    for y in range(3, 13):
        width = 1 + min(y - 3, 12 - y)
        rect(g, 1, y, width, 1, "w")
        rect(g, 7, y, width, 1, "w")
    rect(g, 13, 3, 2, 10, "w")
    return g


def branch():
    g = blank()
    line(g, 4.5, 3.0, 4.5, 13.0, "w", 0.9)
    line(g, 11.5, 4.0, 11.5, 6.5, "w", 0.9)
    line(g, 11.5, 6.5, 4.8, 10.5, "w", 0.9)
    disc(g, 4.5, 3.0, 2.0, "y")
    disc(g, 4.5, 13.0, 2.0, "y")
    disc(g, 11.5, 3.5, 2.0, "G")
    return g


def other_branch():
    g = blank()
    arrow = [
        "........w....",
        "........ww...",
        "wwwwwwwwwww..",
        "wwwwwwwwwwww.",
        "wwwwwwwwwww..",
        "........ww...",
        "........w....",
    ]
    put(g, arrow, 2, 1)
    put(g, [row.replace("w", "y") for row in mirror(arrow)], 1, 8)
    return g


def compare():
    g = blank()
    rect(g, 2, 6, 4, 7, "c")
    rect(g, 9, 2, 4, 11, "y")
    rect(g, 1, 13, 14, 1, "w")
    return g


def new_start():
    """A seedling: a world begun again."""
    g = blank()
    put(g, [
        "........GGG..",
        ".......GGGGG.",
        ".GGG..GGGGdd.",
        "GGGGG.GGGdd..",
        "GGGGGdGddd...",
        ".ddddGd......",
        ".....Gd......",
        ".....Gd......",
    ], 1, 1)
    rect(g, 1, 10, 14, 3, "n")
    rect(g, 3, 10, 2, 1, "t")
    rect(g, 10, 11, 2, 1, "t")
    return g


def mode_watch():
    """An eye: watching."""
    g = blank()
    put(g, [
        "....wwwwww....",
        "..wwwwwwwwww..",
        ".wwwwwwwwwwww.",
        "wwwwwwwwwwwwww",
        ".wwwwwwwwwwww.",
        "..wwwwwwwwww..",
        "....wwwwww....",
    ], 1, 4)
    disc(g, 8.0, 7.5, 3.0, "b")
    rect(g, 7, 6, 2, 3, "k")
    setp(g, 6, 6, "c")
    return g


def mode_sandbox():
    """A spade: changing the world."""
    g = blank()
    rect(g, 5, 1, 6, 2, "t")
    rect(g, 7, 3, 2, 5, "t")
    put(g, [
        "llllll",
        "lwllll",
        "lwllll",
        "lwllll",
        ".llll.",
        "..ll..",
    ], 5, 8)
    return g


def world():
    g = blank()
    disc(g, 8.0, 8.0, 6.6, "b")
    land = [
        "......GG......",
        "....GG.GGG....",
        "..GGGG..GG....",
        "..GGG....G....",
        "...GG.........",
        "....G.....GG..",
        ".........GGGG.",
        "........GGGG..",
        "...GG...GGG...",
        "..GGGG...G....",
        "...GG.........",
    ]
    for dy, row in enumerate(land):
        for dx, ch in enumerate(row):
            x, y = 1 + dx, 2 + dy
            if ch != "." and g[y][x] == "b":
                g[y][x] = ch
    return g


def help_icon():
    g = blank()
    put(g, [
        "..wwwww..",
        ".wwwwwww.",
        "www...www",
        "ww....www",
        ".....www.",
        "....www..",
        "...www...",
        "...ww....",
        "...ww....",
        ".........",
        "...ww....",
        "...ww....",
    ], 3, 1)
    return g


def close():
    g = blank()
    line(g, 3.5, 3.5, 12.5, 12.5, "w", 1.2)
    line(g, 12.5, 3.5, 3.5, 12.5, "w", 1.2)
    return g


def let_go():
    """An arrow leaving a box: let go of who is followed."""
    g = blank()
    put(g, [
        "lllllll",
        "l......",
        "l......",
        "l......",
        "l......",
        "l......",
        "l......",
        "l......",
        "l......",
        "lllllll",
    ], 1, 3)
    put(g, [
        ".......w..",
        ".......ww.",
        "wwwwwwwwww",
        "wwwwwwwwww",
        ".......ww.",
        ".......w..",
    ], 5, 5)
    rect(g, 14, 7, 1, 2, "w")
    return g


def spring():
    g = blank()
    for cx, cy in ((8, 4.5), (8, 11.5), (4.5, 8), (11.5, 8)):
        disc(g, cx, cy, 2.9, "p")
    disc(g, 8, 8, 2.2, "y")
    return g


def summer():
    g = blank()
    for i in range(8):
        a = i * math.pi / 4
        line(g, 8 + 5.2 * math.cos(a), 8 + 5.2 * math.sin(a),
             8 + 6.6 * math.cos(a), 8 + 6.6 * math.sin(a), "o", 0.7)
    disc(g, 8, 8, 4.2, "y")
    rect(g, 6, 5, 2, 1, "w")
    setp(g, 5, 6, "w")
    return g


def autumn():
    """A leaf along the diagonal, its vein and stem darker."""
    g = blank()
    for y in range(SIZE):
        for x in range(SIZE):
            px, py = x + 0.5 - 8.5, y + 0.5 - 7.5
            u = (px - py) / math.sqrt(2)  # along the leaf, toward the top right
            v = (px + py) / math.sqrt(2)  # across it
            if (u / 6.2) ** 2 + (v / 3.6) ** 2 <= 1:
                g[y][x] = "o"
    line(g, 4.0, 12.0, 12.5, 3.5, "r", 0.5)
    line(g, 1.8, 14.2, 4.0, 12.0, "n", 0.6)
    return g


def winter():
    g = blank()
    for i in range(4):
        a = i * math.pi / 4
        dx, dy = 6 * math.cos(a), 6 * math.sin(a)
        line(g, 8 - dx, 8 - dy, 8 + dx, 8 + dy, "c", 0.55)
    disc(g, 8, 8, 1.3, "w")
    for x, y in ((8, 2), (8, 13), (2, 8), (13, 8)):
        setp(g, x, y, "w")
    return g


def animals():
    """A paw print."""
    g = blank()
    put(g, [
        ".....tt..tt.....",
        "....ttt..ttt....",
        "....ttt..ttt....",
        ".....t....t.....",
        ".tt..........tt.",
        ".ttt........ttt.",
        ".ttt..tttt..ttt.",
        "..t..tttttt..t..",
        "....tttttttt....",
        "...tttttttttt...",
        "...tttttttttt...",
        "...tttttttttt...",
        "....tttttttt....",
    ], 0, 1)
    return g


def drought():
    """A low sun over cracked ground."""
    g = blank()
    disc(g, 8, 6.5, 4.6, "o")
    disc(g, 8, 6.5, 3.4, "y")
    rect(g, 1, 9, 14, 5, "t")
    put(g, [
        "..n.....n....",
        "...n...n..n..",
        "..n.n...n..n.",
        ".n...n..n.n..",
    ], 1, 10)
    return g


def hard_winter():
    """Icicles hanging from a snowy ledge."""
    g = blank()
    rect(g, 1, 2, 14, 3, "w")
    put(g, [
        "cccccccccccccc",
        ".ccc.cccc.ccc.",
        ".ccc..cc..cc..",
        "..c...cc..cc..",
        "..c...c....c..",
        "......c....c..",
        "......c.......",
    ], 1, 5)
    rect(g, 1, 4, 14, 1, "l")
    return g


def warm_year():
    """A thermometer, red to the top."""
    g = blank()
    rect(g, 6, 1, 4, 10, "w")
    rect(g, 7, 2, 2, 9, "r")
    disc(g, 8, 12, 3.2, "r")
    for y in (3, 5, 7):
        setp(g, 10, y, "w")
        setp(g, 11, y, "w")
    return g


DRAW = {
    "pause": pause, "play": play, "slower": slower, "faster": faster,
    "zoom_out": lambda: magnifier("-"), "zoom_in": lambda: magnifier("+"),
    "story": story, "notices": notices, "camera": camera, "jump": jump,
    "trait": trait, "weather": weather, "skip": skip, "branch": branch,
    "other_branch": other_branch, "compare": compare, "new_start": new_start,
    "mode_watch": mode_watch, "mode_sandbox": mode_sandbox, "world": world,
    "help": help_icon, "close": close, "let_go": let_go, "spring": spring,
    "summer": summer, "autumn": autumn, "winter": winter, "animals": animals,
    "drought": drought, "hard_winter": hard_winter, "warm_year": warm_year,
}


def to_image(g):
    im = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    for y in range(SIZE):
        for x in range(SIZE):
            ch = g[y][x]
            if ch != ".":
                im.putpixel((x, y), PALETTE[ch] + (255,))
    return im


def sheet(images, path):
    """Every icon at 1x and 4x on the three backgrounds it is drawn on."""
    backgrounds = [(0x1c, 0x2a, 0x44), (0x3a, 0x2a, 0x18), (0x12, 0x14, 0x1c)]
    cell = SIZE * 4 + 12
    cols = 8
    rows = (len(images) + cols - 1) // cols
    out = Image.new("RGB", (cols * cell * len(backgrounds), rows * (cell + 24)),
                    (60, 60, 60))
    for b, bg in enumerate(backgrounds):
        for i, im in enumerate(images):
            x0 = (b * cols + i % cols) * cell
            y0 = (i // cols) * (cell + 24)
            tile = Image.new("RGB", (cell, cell + 24), bg)
            big = im.resize((SIZE * 4, SIZE * 4), Image.NEAREST)
            tile.paste(big, (6, 6), big)
            tile.paste(im, (6, cell + 2), im)
            out.paste(tile, (x0, y0))
    out.save(path)


def main():
    assert NAMES == list(DRAW), "NAMES and DRAW must list the same icons"
    os.makedirs(OUT_DIR, exist_ok=True)
    images = []
    for name in NAMES:
        g = outline(DRAW[name]())
        im = to_image(g)
        im.save(os.path.join(OUT_DIR, name + ".png"))
        images.append(im)
    if len(sys.argv) > 1:
        sheet(images, sys.argv[1])


if __name__ == "__main__":
    main()
