"""Add the UBE overlay leak shader to both OninusLactis.esp files.

LactisNippleLeakUBE (EFSH 000D70) is LactisNippleLeakCBBE (000D69) with one change: its
texture is textures\\lactis\\Milk_Leaking_UBE.dds (make_ube_leak.py), painted for the UBE
body's UV layout. OninusLactis.psc looks it up by that FormID: never renumber it. Both
plugins (the original slots and the Devious Devices one under Optional) get the same record
at the same FormID.

    python tools/add_ube_leak_shader.py

A second run replaces the record it added. Needs esptree.py (path below).
"""
import os
import struct
import sys

sys.path.insert(0, r"C:\Playground\Skyrim\mods\Sexlab Survival\tools\knock")
import esptree as E  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLUGINS = [
    os.path.join(ROOT, r"dist\OninusLactis.esp"),
    os.path.join(ROOT, r"dist\Optional\Alternative slots 54-56 (DD and SMP compatible)\OninusLactis.esp"),
]
SOURCE_ID = 0xD69
LOCAL_ID = 0xD70
EDID = b"LactisNippleLeakUBE\x00"
TEXTURE = b"lactis\\Milk_Leaking_UBE.dds\x00"       # relative to textures\


def local(fid):
    return fid & 0xFFFFFF


def add(path):
    tes4, top = E.parse(path)
    group = next(g for g in top if bytes(g.hdr[8:12]) == b"EFSH")
    source = next(r for r in group.kids if local(r.fid) == SOURCE_ID)
    subs = []
    for sig, data in source.load_subs():
        if sig == "EDID":
            data = EDID
        elif sig == "ICON":
            data = TEXTURE
        subs.append((sig, data))
    rec = E.Rec("EFSH", bytearray(source.hdr), b"")
    rec.fid = (source.fid & 0xFF000000) | LOCAL_ID
    rec.set_subs(subs)
    group.kids = [r for r in group.kids if local(r.fid) != LOCAL_ID] + [rec]

    out = []
    for sig, data in tes4.load_subs():
        if sig == "HEDR":
            ver, _count, next_id = struct.unpack("<fII", data)
            data = struct.pack("<fII", ver, E.count_records_and_groups(top), max(next_id, LOCAL_ID + 1))
        out.append((sig, data))
    tes4.set_subs(out)
    E.write(path, tes4, top)
    print("wrote %s: EFSH %08X %s -> textures\\%s" % (path, rec.fid, EDID[:-1].decode(), TEXTURE[:-1].decode()))


def main():
    for path in PLUGINS:
        add(path)


if __name__ == "__main__":
    main()
