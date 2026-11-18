"""The overlay leak's texture for the UBE body.

The overlay leak (EFSH LactisNippleLeakCBBE) draws textures\\lactis\\Milk_Leaking_CBBE.dds,
painted for the CBBE UV layout; the UBE body has its own layout, so on a UBE actor the paint
lands under the breasts. This repaints it for UBE: each UBE texel -> its 3D point on the UBE
body -> the closest point on the CBBE 3BA body -> that point's CBBE UV -> the stock texel.

The drips hang from the nipple and the two bodies' nipples are not in the same place, so the
UBE points are first shifted nipple onto nipple (x mirrored per side). The nipples are found
by the rule of src/Body.cpp (tools/nipple_check.py), the weight peak of the breast's tip
bone, which is where the nipple is on both bodies.

    python tools/make_ube_leak.py [--src <Milk_Leaking_CBBE.dds>] [--out <dds>] [--preview out.png]

Writes dist\\textures\\lactis\\Milk_Leaking_UBE.dds. Needs numpy, Pillow, texconv and three
scripts outside this repo (paths below): the UBE overlay mapper, its face remap helper and
nifkit. The two bodies are read from the nefaram install (check_ube_overlays.MESH).
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, r"C:\Playground\Skyrim\tools\ube_overlay_check")
sys.path.insert(0, r"C:\Playground\Skyrim\mods\Apropos2 SE\tools")
import check_ube_overlays as C  # noqa: E402
import nipple_check as N  # noqa: E402
import ube_face_remap as F  # noqa: E402

SRC = os.path.join(ROOT, r"dist\textures\lactis\Milk_Leaking_CBBE.dds")
OUT = os.path.join(ROOT, r"dist\textures\lactis\Milk_Leaking_UBE.dds")


def nipple_tip(rel, name):
    """The left nipple: its position and the UV of the vertex nearest it."""
    n = N.nifkit.Nif(open(os.path.join(C.MODS, rel), "rb").read())
    for b in n.shapes():
        if n.shape(b)["name"] != name:
            continue
        s = N.nifkit.SkinnedShape(n, b)
        pos = s.v["pos"].astype(np.float64)
        nor = N.normals(pos, s.tris.astype(np.int64))
        facing = np.linalg.norm(nor, axis=1) > 1e-6
        if ((nor[facing] * (pos[facing] - pos.mean(0))).sum(1) > 0).sum() * 2 < facing.sum():
            nor = -nor
        tip = N.find_tip(s, pos, nor, "L")
        if not tip:
            raise SystemExit("no nipple found on %s" % rel)
        near = np.argmin(np.linalg.norm(pos - tip["tip"], axis=1))
        return tip["tip"], s.v["uv"][near].astype(np.float64)
    raise KeyError(name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=SRC)
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--size", type=int, default=1024)
    ap.add_argument("--preview", default="")
    args = ap.parse_args()

    (srel, sname), (drel, dname) = C.MESH["Body"]
    cbbe_tip, cbbe_uv = nipple_tip(srel, sname)
    ube_tip, ube_uv = nipple_tip(drel, dname)
    shift = cbbe_tip - ube_tip
    print("left nipple: CBBE %s uv %s | UBE %s uv %s | shift %s" % (
        np.round(cbbe_tip, 2), np.round(cbbe_uv, 4), np.round(ube_tip, 2), np.round(ube_uv, 4), np.round(shift, 2)))

    stock_load = C.load_shape

    def load_shape(rel, name):
        p, t, uv = stock_load(rel, name)
        if rel == drel:
            side = np.tanh(p[:, 0] / 1.5)           # -1 on the tip's side (x < 0), +1 on the other
            sx = shift[0] * np.sign(ube_tip[0])
            p = p + np.stack([side * sx, np.full(len(p), shift[1]), np.full(len(p), shift[2])], 1)
        return p, t, uv

    C.load_shape = load_shape
    m = C.Mapping("Body", args.size)
    src = F.dds_to_rgba(args.src)
    out = m.project(src, "Body")
    out = F.dilate(out, out[..., 3] > 0, 8)
    F.rgba_to_dds(out, args.out)
    print("wrote", args.out)

    for label, img, uv in (("stock / CBBE", src, cbbe_uv), ("output / UBE", out, ube_uv)):
        h, w = img.shape[:2]
        ys, xs = np.nonzero(img[..., 3] > 8)
        print("  %-13s nipple uv (%.4f, %.4f); paint u %.4f..%.4f v %.4f..%.4f" % (
            label, uv[0], uv[1], xs.min() / w, xs.max() / w, ys.min() / h, ys.max() / h))

    if args.preview:
        S = 1024
        wires = (C.wire(m.SUV, m.ST, S, (120, 120, 120, 110), 2), C.wire(m.DUV, m.DT, S, (120, 120, 120, 110), 2))
        stock_s = np.asarray(Image.fromarray(np.clip(src, 0, 255).astype(np.uint8), "RGBA").resize((S, S), Image.BILINEAR), np.float64)
        out_s = np.asarray(Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGBA").resize((S, S), Image.BILINEAR), np.float64)
        sheet = np.concatenate([C.panel(stock_s[..., 3] / 255.0, wires[0], S),      # stock on CBBE
                                C.panel(stock_s[..., 3] / 255.0, wires[1], S),      # stock on UBE: the defect
                                C.panel(out_s[..., 3] / 255.0, wires[1], S)], 1)    # output on UBE
        Image.fromarray(sheet.astype(np.uint8)).save(args.preview)
        print("preview", args.preview)


if __name__ == "__main__":
    main()
