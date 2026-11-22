"""The nipple rule of src/Body.cpp (FindTip), run offline on nif files.

    python tools/nipple_check.py <mesh.nif> [...]

Per shape and side: the tip bone, how many vertices carry it (coverage), how many are
within 85 percent of its heaviest weight, and the tip found. With a UV in the mesh the
tip's UV is printed too, so a naked body can be checked against its texture. Needs
numpy and a nifkit with SkinnedShape (path below).
"""
import re
import sys

import numpy as np

sys.path.insert(0, r"C:\Playground\Skyrim\mods\Ovibody UBE")
import nifkit  # noqa: E402

TIP_WEIGHT, TIP_LOOSE, TIP_FLOOR, TIP_VERTICES, TIP_DEPTH = 0.85, 0.6, 0.2, 6, 0.15


def breast_bone(name):
    low = name.lower()
    m = re.fullmatch(r"soft_([lr])(n00|\d{3})", low)  # GT Softbody's breast net
    if m:
        return m.group(1).upper(), 1000 if m.group(2) == "n00" else 100 + int(m.group(2)[1:])
    if "breast" not in low or "pre" in low:
        return None
    padded = " " + low
    side = "L" if " l " in padded else "R" if " r " in padded else None
    if side is None:
        return None
    m = re.search(r"(\d+)$", low)
    return side, int(m.group(1)) if m else 0


def normals(pos, tris):
    n = np.zeros_like(pos)
    f = np.cross(pos[tris[:, 1]] - pos[tris[:, 0]], pos[tris[:, 2]] - pos[tris[:, 0]])
    for k in range(3):
        np.add.at(n, tris[:, k], f)
    return n


def find_tip(s, pos, nor, side):
    best, tip_bone = -1, None
    for i, name in enumerate(s.bone_names):
        b = breast_bone(name)
        if b and b[0] == side and b[1] > best:
            best, tip_bone = b[1], i
    if tip_bone is None:
        return None
    w = np.zeros(s.n)
    for slot in range(4):
        w += np.where(s.vbones[:, slot] == tip_bone, s.v["weights"][:, slot], 0)
    w[np.linalg.norm(nor, axis=1) < 1e-6] = 0
    heaviest = w.max()
    if heaviest < TIP_FLOOR:
        return None
    top = np.zeros(0, int)
    for share in (TIP_WEIGHT, TIP_LOOSE):
        top = np.nonzero(w >= share * heaviest)[0]
        if len(top) >= TIP_VERTICES:
            break
    if len(top) < TIP_VERTICES:
        return None
    unit = nor[top] / np.maximum(np.linalg.norm(nor[top], axis=1), 1e-9)[:, None]
    centre = pos[top].mean(0)
    axis = unit.sum(0)
    if np.linalg.norm(axis) < 1e-4:
        return None
    axis /= np.linalg.norm(axis)
    proj = (pos[top] - centre) @ axis
    near = top[proj >= proj.max() - TIP_DEPTH]
    return dict(bone=s.bone_names[tip_bone], coverage=int((w >= TIP_FLOOR).sum()), top=len(top), tip=pos[near].mean(0),
                vertex=near[0], reach=proj.max())


def main():
    for path in sys.argv[1:]:
        n = nifkit.Nif(open(path, "rb").read())
        print(path)
        for b in n.shapes():
            try:
                s = nifkit.SkinnedShape(n, b)
            except Exception as e:
                print("   %-24s skipped (%s)" % (n.shape(b)["name"][:24], type(e).__name__))
                continue
            pos = s.v["pos"].astype(np.float64)
            nor = normals(pos, s.tris.astype(np.int64))
            centroid = pos.mean(0)
            facing = np.linalg.norm(nor, axis=1) > 1e-6
            if ((nor[facing] * (pos[facing] - centroid)).sum(1) > 0).sum() * 2 < facing.sum():
                nor = -nor
            for side in "LR":
                t = find_tip(s, pos, nor, side)
                if not t:
                    continue
                uv = ""
                if "uv" in s.v:
                    uv = " uv (%.3f, %.3f)" % tuple(s.v["uv"][t["vertex"]])
                print("   %-24s %s %-12s coverage %5d, top %4d, tip %s%s" % (
                    s.name[:24], side, t["bone"], t["coverage"], t["top"], np.round(t["tip"], 2), uv))


if __name__ == "__main__":
    main()
