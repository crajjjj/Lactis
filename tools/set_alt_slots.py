"""Moves the alternative plugin's squirt armor to body slots 54 and 56 (2.3.5).

    python tools/set_alt_slots.py

The armor 0xD6B takes both slots, the left addon 0xD6A slot 56, the right addon 0xD6C
slot 54. BOD2's first field is the slot bit mask (slot n = bit n - 30), so the edit is
four bytes per record and no size changes. Run once; a second run changes nothing.

Why 54 and 56: Devious Devices wears harnesses on 58 and arm cuffs on 59, the 3BA SMP
object and GT Softbody's objects sit on 60, GT Softbody's breast physics on 59. Only a
chastity bra (56) is left to clash, and that covers the nipples anyway.
"""
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parent.parent
PLUGIN = ROOT / "dist" / "Optional" / "Alternative slots 54-56 (DD and SMP compatible)" / "OninusLactis.esp"
SLOTS = {0xD6B: (54, 56), 0xD6A: (56,), 0xD6C: (54,)}


def mask(slots):
    return sum(1 << (slot - 30) for slot in slots)


def walk(data, off, end, found):
    while off < end:
        sig = data[off:off + 4]
        size = struct.unpack_from("<I", data, off + 4)[0]
        if sig == b"GRUP":
            walk(data, off + 24, off + size, found)
            off += size
            continue
        form = struct.unpack_from("<I", data, off + 12)[0] & 0xFFFFFF
        if sig in (b"ARMO", b"ARMA") and form in SLOTS:
            p, stop = off + 24, off + 24 + size
            while p < stop:
                sub = data[p:p + 4]
                length = struct.unpack_from("<H", data, p + 4)[0]
                if sub == b"BOD2":
                    found[form] = p + 6
                p += 6 + length
        off += 24 + size


def main():
    data = bytearray(PLUGIN.read_bytes())
    found = {}
    walk(data, 0, len(data), found)
    missing = set(SLOTS) - set(found)
    if missing:
        raise SystemExit("no BOD2 on " + ", ".join("%06X" % form for form in sorted(missing)))
    for form, at in sorted(found.items()):
        before = struct.unpack_from("<I", data, at)[0]
        after = mask(SLOTS[form])
        struct.pack_into("<I", data, at, after)
        print("%06X: %#010x -> %#010x" % (form, before, after))
    PLUGIN.write_bytes(data)


if __name__ == "__main__":
    main()
