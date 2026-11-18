"""The covered-nipple rule of src/Body.cpp (CoverOf), run offline on nif files.

    python tools/cover_check.py <body.nif> [<worn.nif> ...]

The shapes of every file are taken as worn together, as the game's 3D would hold
them. Skin-shaded shapes (shader type Skin Tint) are the body; per side, the body's
nipple is found with the rule of tools/nipple_check.py, and a ray goes out of it along
its normal. A non-skin shape covers it when the ray meets it between 1 unit inside the
skin and 8 units out, and at least 16 square units of it lie within 4 units of the
nipple (a ring or a pasty does not cover). Shapes compare in the bind space of a bone
both are bound to. Prints, per side, what covers the nipple or "bare". Needs numpy and
the nifkit of the Ovibody UBE project (path in nipple_check.py).
"""
import struct
import sys

import numpy as np

import nipple_check
from nipple_check import nifkit

COVER_REACH, COVER_BEHIND, COVER_RADIUS, COVER_AREA = 8.0, 1.0, 4.0, 16.0
SKIN_TINT = 5


def torso_bone(name):
    low = name.lower()
    return any(part in low for part in ("breast", "spine", "belly", "pelvis", "thigh"))


def shader_type(nif, block):
    if block < 0 or nif.kind(block) != "BSLightingShaderProperty":
        return None
    return struct.unpack_from("<I", nif.blocks[block], 0)[0]


def load(paths):
    shapes = []
    for path in paths:
        nif = nifkit.Nif(open(path, "rb").read())
        for b in nif.shapes():
            try:
                s = nifkit.SkinnedShape(nif, b)
            except Exception:
                continue
            pos = s.v["pos"].astype(np.float64)
            tris = s.tris.astype(np.int64)
            nor = nipple_check.normals(pos, tris)
            centroid = pos.mean(0)
            facing = np.linalg.norm(nor, axis=1) > 1e-6
            if ((nor[facing] * (pos[facing] - centroid)).sum(1) > 0).sum() * 2 < facing.sum():
                nor = -nor
            skin = shader_type(nif, s.info["shader"]) == SKIN_TINT
            shapes.append(dict(path=path, s=s, pos=pos, tris=tris, nor=nor, skin=skin,
                               binds={name: s.binds[i] for i, name in enumerate(s.bone_names)}))
    return shapes


def hit(origin, direction, a, b, c):
    e1, e2 = b - a, c - a
    p = np.cross(direction, e2)
    det = e1 @ p
    if abs(det) < 1e-9:
        return None
    inv = 1.0 / det
    s = origin - a
    u = (s @ p) * inv
    if u < 0 or u > 1:
        return None
    q = np.cross(s, e1)
    v = (direction @ q) * inv
    if v < 0 or u + v > 1:
        return None
    return (e2 @ q) * inv


def cover_of(shapes, side):
    bare = None
    for sh in shapes:
        if not sh["skin"]:
            continue
        t = nipple_check.find_tip(sh["s"], sh["pos"], sh["nor"], side)
        if t and (bare is None or t["coverage"] > bare[1]["coverage"]):
            bare = (sh, t)
    if bare is None:
        return "no skin weighted to this breast (covered only under a body armor)"
    body, tip = bare
    normal = sh_normal(body, tip)
    for sh in shapes:
        if sh["skin"]:
            continue
        shared = None
        if tip["bone"] in sh["binds"]:
            shared = tip["bone"]
        else:
            for name in body["s"].bone_names:
                if torso_bone(name) and name in sh["binds"]:
                    shared = name
                    break
        if shared is None:
            continue
        r_mine, t_mine = body["binds"][shared]
        r_theirs, t_theirs = sh["binds"][shared]
        origin = r_mine @ tip["tip"] + t_mine
        direction = r_mine @ normal
        direction /= np.linalg.norm(direction)
        at = sh["pos"] @ r_theirs.T + t_theirs
        tri = at[sh["tris"]]
        middle = tri.mean(1)
        near = np.nonzero(((middle - origin) ** 2).sum(1) <= COVER_RADIUS ** 2)[0]
        area = 0.5 * np.linalg.norm(np.cross(tri[near, 1] - tri[near, 0], tri[near, 2] - tri[near, 0]), axis=1).sum()
        if area < COVER_AREA:
            print("      %-28s near the nipple: %.1f sq units, too little to cover" % (sh["s"].name[:28], area))
            continue
        for k in near:
            d = hit(origin, direction, *tri[k])
            if d is not None and -COVER_BEHIND <= d <= COVER_REACH:
                return "covered by %s (%.2f units out, %.0f sq units around)" % (sh["s"].name, d, area)
        print("      %-28s near the nipple: %.1f sq units, the ray misses it" % (sh["s"].name[:28], area))
    return "bare"


def sh_normal(body, tip):
    # the mean normal of the tip's closest vertices, as Body.cpp's Tip::normal
    pos, nor = body["pos"], body["nor"]
    d = ((pos - tip["tip"]) ** 2).sum(1)
    close = np.argsort(d)[:8]
    n = (nor[close] / np.maximum(np.linalg.norm(nor[close], axis=1), 1e-9)[:, None]).sum(0)
    return n / np.linalg.norm(n)


def main():
    shapes = load(sys.argv[1:])
    for sh in shapes:
        print("  %-30s %s" % (sh["s"].name[:30], "skin" if sh["skin"] else "worn"))
    for side in "LR":
        print("   %s: %s" % (side, cover_of(shapes, side)))


if __name__ == "__main__":
    main()
