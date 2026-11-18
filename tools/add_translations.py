"""Append the 2.2 MCM strings to every translation file (UTF-16 LE with BOM, CRLF, tab-separated).

    python add_translations.py <dist/Interface/Translations>

English text goes into every language: a missing key would show as the raw $KEY.
Run again safely: keys already present are left alone.
"""
import glob
import os
import sys

NEW = [
    ("$OL_ENABLE_FLUID_LEAK", "Enable Milk Leak (fluid, any body)"),
    ("$OL_HELP_FLUID_LEAK", "Drops of milk well up at the nipples and run down the skin while an actor squirts or leaks. It is real fluid, so it works on any body and over clothes. Needs OninusLactis.dll; tune its look and physics in the SKSE Menu Framework (Oninus Lactis)."),
    ("$OL_NIPPLE_SEARCH", "Nipples Found Automatically"),
    ("$OL_HELP_NIPPLE_SEARCH", "With OninusLactis.dll the squirt emitters are placed on the nipples found on each actor's own meshes, whatever the body, preset or morphs. The offsets below are then only used for an actor on whom nothing is found. To place by offsets again, turn the search off in the SKSE Menu Framework (Oninus Lactis > Squirt)."),
    ("$OL_ON", "On"),
    ("$OL_OFF", "Off"),
    ("$OL_NO_DLL", "DLL not loaded"),
]


def main():
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "OninusLactis_*.txt"))):
        raw = open(path, "rb").read()
        # The game reads only UTF-16 LE with a BOM. 2.1's Portuguese file is UTF-8
        # without one, so it never loaded; it is rewritten in the right encoding.
        recoded = not raw.startswith(b"\xff\xfe")
        text = raw.decode("utf-8") if recoded else raw.decode("utf-16")
        if recoded:
            print("%s: was not UTF-16 LE, rewritten" % os.path.basename(path))
        have = {line.split("\t", 1)[0] for line in text.splitlines()}
        add = [(k, v) for k, v in NEW if k not in have]
        if not add and not recoded:
            print("%s: nothing to add" % os.path.basename(path))
            continue
        if not text.endswith("\n"):
            text += "\r\n"
        text += "".join("%s\t%s\r\n" % kv for kv in add)
        open(path, "wb").write(text.encode("utf-16"))   # utf-16 writes the BOM, little-endian here
        print("%s: +%d" % (os.path.basename(path), len(add)))


if __name__ == "__main__":
    main()
