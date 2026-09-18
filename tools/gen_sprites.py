#!/usr/bin/env python3
"""Generate placeholder pixel-art sprites for Conflict Converge.

AI-generated art was chosen, but no image model is available in this
environment, so this script procedurally synthesizes the equivalent:
32x32 unit sprites (idle + attack frames, blue/red team variants),
building sprites, resource node icons, and 16x16 HUD icons — all with a
shared pixel-art palette (dark outlines, team accents, muzzle flashes).
Re-run to regenerate: `python tools/gen_sprites.py`. Output PNGs under
`data/sprites/`. When real AI-generated sheets arrive, they drop into the
same layout and `Art` needs no code changes (filenames are the contract).
"""

from PIL import Image, ImageDraw

OUT = "data/sprites"
TEAMS = {"blue": (44, 123, 229), "red": (229, 72, 77)}
DARK = (26, 28, 44)
SKIN = (232, 190, 150)
OLIVE = (110, 124, 78)
VEST = (210, 120, 40)
HULL = (128, 134, 140)
HULL_DARK = (88, 92, 100)
GUNMETAL = (40, 42, 54)
FLASH_Y = (250, 210, 90)
FLASH_W = (255, 250, 230)

CANVAS = 32


def canvas(size=CANVAS):
    return Image.new("RGBA", (size, size), (0, 0, 0, 0))


def flash(draw, x, y):
    draw.polygon([(x, y - 5), (x + 2, y - 1), (x + 6, y), (x + 2, y + 1),
                  (x, y + 5), (x - 2, y + 1), (x - 6, y), (x - 2, y - 1)], fill=FLASH_Y)
    draw.rectangle([x - 2, y - 2, x + 2, y + 2], fill=FLASH_W)


def trooper(draw, team, vest=False, tube=False):
    body, accent = (VEST if vest else OLIVE), TEAMS[team]
    draw.rectangle([12, 24, 15, 29], fill=DARK)          # legs
    draw.rectangle([16, 24, 19, 29], fill=DARK)
    draw.rectangle([11, 16, 20, 23], fill=body)          # torso
    draw.rectangle([11, 16, 20, 18], fill=accent)        # team stripe
    draw.rectangle([12, 9, 19, 14], fill=HULL_DARK)      # helmet
    draw.rectangle([12, 13, 19, 14], fill=SKIN)          # visor slit
    if tube:  # anti-armor launcher
        draw.rectangle([14, 10, 29, 13], fill=GUNMETAL)
        draw.rectangle([14, 10, 17, 13], fill=accent)
    else:     # rifle
        draw.rectangle([20, 18, 28, 20], fill=GUNMETAL)
    if vest:  # engineer pack + wrench
        draw.rectangle([8, 17, 10, 22], fill=HULL)
        draw.rectangle([22, 22, 24, 26], fill=HULL)
        draw.rectangle([23, 20, 23, 21], fill=FLASH_W)


def vehicle(draw, team, w, h, barrel_len, barrel_y, turret=True):
    accent = TEAMS[team]
    x0, y0 = (CANVAS - w) // 2, CANVAS - 4 - h
    draw.rectangle([x0, y0 + h - 5, x0 + w - 1, y0 + h - 1], fill=DARK)  # tracks
    for wx in range(x0 + 2, x0 + w - 1, 5):                              # wheels
        draw.rectangle([wx, y0 + h - 4, wx + 2, y0 + h - 2], fill=HULL_DARK)
    draw.rectangle([x0, y0 + 2, x0 + w - 1, y0 + h - 5], fill=HULL)       # hull
    draw.rectangle([x0, y0 + 2, x0 + w - 1, y0 + 4], fill=accent)         # team stripe
    if turret:
        draw.rectangle([x0 + 4, y0 - 3, x0 + 12, y0 + 2], fill=HULL_DARK)
    draw.rectangle([x0 + 8, barrel_y, x0 + 8 + barrel_len, barrel_y + 2], fill=GUNMETAL)
    return x0 + 8 + barrel_len, barrel_y + 1  # muzzle anchor


UNIT_DEFS = ["infantry", "antiarmor", "engineer", "ifv",
             "artillery", "lighttank", "heavytank"]


def draw_unit(unit, team):
    img = canvas()
    d = ImageDraw.Draw(img)
    muzzle = None
    if unit == "infantry":
        trooper(d, team)
        muzzle = (29, 19)
    elif unit == "antiarmor":
        trooper(d, team, tube=True)
        muzzle = (30, 11)
    elif unit == "engineer":
        trooper(d, team, vest=True)
        muzzle = (29, 19)
    elif unit == "ifv":
        muzzle = vehicle(d, team, 24, 10, 12, 13)
    elif unit == "artillery":
        muzzle = vehicle(d, team, 24, 9, 18, 14, turret=False)
    elif unit == "lighttank":
        muzzle = vehicle(d, team, 22, 12, 14, 13)
    elif unit == "heavytank":
        muzzle = vehicle(d, team, 28, 14, 16, 12)
    return img, muzzle


def buildings(team):
    accent = TEAMS[team]
    out = {}
    base = canvas(64)
    d = ImageDraw.Draw(base)
    d.rectangle([8, 20, 55, 57], fill=HULL)
    d.rectangle([8, 20, 55, 27], fill=accent)                       # team roof
    d.rectangle([8, 52, 55, 57], fill=HULL_DARK)
    for wx in (16, 28, 40):                                         # windows
        d.rectangle([wx, 34, wx + 5, 39], fill=FLASH_W)
    d.rectangle([48, 4, 50, 20], fill=GUNMETAL)                     # antenna
    d.rectangle([47, 2, 51, 5], fill=accent)
    out["base"] = base
    factory = canvas(64)
    d = ImageDraw.Draw(factory)
    d.rectangle([6, 24, 57, 57], fill=HULL_DARK)
    d.rectangle([6, 24, 57, 30], fill=HULL)
    d.rectangle([44, 8, 50, 24], fill=GUNMETAL)                     # chimney
    d.rectangle([26, 40, 37, 57], fill=accent)                      # team door
    for wx in (12, 44):
        d.rectangle([wx, 36, wx + 5, 41], fill=FLASH_W)
    out["factory"] = factory
    depot = canvas()
    d = ImageDraw.Draw(depot)
    d.rectangle([4, 16, 15, 27], fill=(150, 104, 60))               # crates
    d.rectangle([4, 16, 15, 18], fill=(110, 74, 42))
    d.rectangle([16, 10, 27, 27], fill=(150, 104, 60))
    d.rectangle([16, 10, 27, 12], fill=accent)                      # team stripe
    d.rectangle([20, 16, 22, 24], fill=(110, 74, 42))
    out["depot"] = depot
    return out


def nodes_and_icons():
    iron = canvas()
    d = ImageDraw.Draw(iron)
    d.polygon([(16, 4), (22, 14), (16, 26), (10, 14)], fill=HULL)   # crystal
    d.polygon([(16, 4), (22, 14), (16, 14)], fill=FLASH_W)
    d.polygon([(8, 16), (12, 22), (8, 28), (4, 22)], fill=HULL_DARK)
    d.polygon([(22, 18), (27, 23), (22, 28), (18, 23)], fill=HULL_DARK)
    oil = canvas()
    d = ImageDraw.Draw(oil)
    d.ellipse([6, 20, 26, 28], fill=DARK)                           # pool
    d.polygon([(16, 4), (23, 18), (16, 24), (9, 18)], fill=(20, 20, 28))  # drop
    d.polygon([(13, 10), (16, 8), (16, 18), (12, 16)], fill=(80, 200, 120))  # sheen
    icon_iron = iron.resize((16, 16), Image.NEAREST)
    icon_oil = oil.resize((16, 16), Image.NEAREST)
    return {"iron": iron, "oil": oil}, {"iron": icon_iron, "oil": icon_oil}


def main():
    import os
    for sub in ("units", "buildings", "nodes", "icons"):
        os.makedirs(f"{OUT}/{sub}", exist_ok=True)
    for unit in UNIT_DEFS:
        for team in TEAMS:
            idle, muzzle = draw_unit(unit, team)
            idle.save(f"{OUT}/units/{unit}_{team}_idle.png")
            atk = idle.copy()
            flash(ImageDraw.Draw(atk), *muzzle)
            atk.save(f"{OUT}/units/{unit}_{team}_attack.png")
    for team in TEAMS:
        for name, img in buildings(team).items():
            img.save(f"{OUT}/buildings/{name}_{team}.png")
    node_imgs, icon_imgs = nodes_and_icons()
    for name, img in node_imgs.items():
        img.save(f"{OUT}/nodes/{name}.png")
    for name, img in icon_imgs.items():
        img.save(f"{OUT}/icons/{name}.png")
    print("sprites written to", OUT)


if __name__ == "__main__":
    main()
