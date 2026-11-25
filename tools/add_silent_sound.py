"""Gives the squirt armor (ARMO 0xD6B) a silent pick-up and put-down sound in BOTH
plugins, so equipping the emitters makes no noise. YNAM / ZNAM point at Skyrim.esm's
XXXPlaceHolderSilenceDesc (0x000339C7) and go right after BOD2. Run again safely: a
plugin that already has them is left alone.

    python tools/add_silent_sound.py
"""
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLUGINS = [
    ROOT / "dist" / "OninusLactis.esp",
    ROOT / "dist" / "Optional" / "Alternative slots 54-56 (DD and SMP compatible)" / "OninusLactis.esp",
]
ARMOR = 0x02000D6B
SILENCE = 0x000339C7


def patch(path: Path) -> str:
    data = bytearray(path.read_bytes())
    offset = 0
    tes4_size = struct.unpack_from("<I", data, 4)[0]
    offset = 24 + tes4_size
    while offset < len(data):
        assert data[offset:offset + 4] == b"GRUP", "top level must be groups"
        group_size = struct.unpack_from("<I", data, offset + 4)[0]
        if data[offset + 8:offset + 12] == b"ARMO":
            record = offset + 24
            while record < offset + group_size:
                size, _, form = struct.unpack_from("<III", data, record + 4)
                if form == ARMOR:
                    sub = record + 24
                    end = sub + size
                    after_bod2 = None
                    while sub < end:
                        kind = bytes(data[sub:sub + 4])
                        length = struct.unpack_from("<H", data, sub + 4)[0]
                        if kind in (b"YNAM", b"ZNAM"):
                            return "already has them"
                        sub += 6 + length
                        if kind == b"BOD2":
                            after_bod2 = sub
                    assert after_bod2, "no BOD2 on the armor"
                    extra = b"".join(tag + struct.pack("<HI", 4, SILENCE) for tag in (b"YNAM", b"ZNAM"))
                    data[after_bod2:after_bod2] = extra
                    struct.pack_into("<I", data, record + 4, size + len(extra))
                    struct.pack_into("<I", data, offset + 4, group_size + len(extra))
                    path.write_bytes(data)
                    return "added"
                record += 24 + size
        offset += group_size
    raise SystemExit(f"{path}: armor {ARMOR:08X} not found")


for plugin in PLUGINS:
    print(plugin.relative_to(ROOT), "-", patch(plugin))
