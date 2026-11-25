# Oninus Lactis NG 2.2

A nipple squirt and milk leak effect for Skyrim SE / AE, with a public API other mods
drive. 2.2 adds a native plugin (`OninusLactis.dll`) to the 2.1 scripts:

- **The nipples are found, not dialled in.** The squirt emitters are placed on the
  nipples of the meshes the actor is actually wearing, read from the game at run time:
  any body, any preset, any morph, and over clothes. The MCM offsets remain as the
  fallback for an actor on whom nothing is found.
- **A fluid milk leak.** Drops well up at the nipples, cling, run down the skin and
  drip off. It is simulated fluid lying on the actor's own surface, so it needs no
  body-specific texture. The old overlay leak (an effect shader painted for a UV
  layout) is still there as an option, now with a texture for the UBE body as well
  as CBBE / 3BA, picked per actor by race.
- **Every humanoid race can wear the effect**, custom body races included, with no
  patch plugin.
- Fixes to the 2.1 scripts (below).

## Requirements

- SKSE and Address Library
- SkyUI, PapyrusUtil (as before)
- Optional: SKSE Menu Framework, for the settings pages of the fluid leak
- Optional: ConsoleUtil Extended, for the `lactis` console commands

Without `OninusLactis.dll` the scripts behave as 2.1 did: fixed offsets, overlay leak.

## Install

Install with a mod manager, replacing 2.1. The installer asks which body slots the
squirt uses: it is worn as an invisible armor piece, and one slot holds one item, so it
must not share a slot with something else you wear. The default is 58 and 59; pick 56
and 60 when those are taken, as in a Devious Devices setup.

Installing by hand (no installer): the plugin at the archive's root uses 58 and 59;
`Optional\Alternative slots 56-60 (DD compatible)\OninusLactis.esp` is the 56 and 60
one. Copy everything else as it is.

Updating a save from 2.1 is fine. The fluid leak is on by default.

## Settings

- **MCM** (Oninus Lactis): the hotkey, the squirt's scale, the fallback offsets, the
  debug axis, and two leak switches: `Enable Milk Leak (fluid, any body)` and the old
  `Enable Nipple Leak` overlay. The line `Nipples Found Automatically` shows whether
  the search is on.
- **SKSE Menu Framework** (Oninus Lactis): everything about the fluid leak (how much,
  its look, its physics, hands) and the nipple search. Changes save to
  `OninusLactis_User.toml`; `OninusLactis.toml` holds the defaults.

Two switches people ask for, both in the SKSE menu:

- **Leak only, no spray**: turn off `Squirt > Squirt enabled` (`[squirt] enabled` in
  the toml). Nothing is sprayed any more; whatever asks for a squirt (the hotkey,
  another mod) gets the milk leak alone.
- **Leak when aroused**: set `Leak > Leak when aroused` to an arousal level (`[leak]
  arousal`, 0 is off). The player's nipples leak while arousal is at that level or
  above, heavier the further above it; `Aroused NPCs leak too` extends it to female
  NPCs nearby. Needs SexLab Aroused, any edition.

If the milk looks wrong under your lighting, start with `Look > Opacity` and
`Look > Look style`.

## How the nipples are found

On every body with breast physics bones, the weights of a breast's outermost bone peak
at the nipple, and a vertex keeps its weights whatever sliders or morphs move it. So
the plugin takes the vertices that bone carries most heavily and, among them, the one
furthest out. Clothes fitted to a body carry its weights and answer the same way; with
several layers the outermost one that covers the breast wins. A rigid cuirass with no
breast weights falls back to the surface nearest the skeleton's breast bone.

A soft body (GT Softbody) has no breast bone chain: each breast is a net of bones
closed by one on the nipple, and the search reads that one. Both of its naming
schemes are known: `Soft_L...` with `Soft_LN00` on the standard bodies, and
`L_SoftBody...` with `L_SoftBodyNip` on the Ultra body. The squirt's
emitters hang under a chain bone that physics leaves still there, so on such a body
they are moved to the nipple every frame. Shapes that are never drawn (the collision
meshes a physics body carries) are left out of the search.

Breasts on HDT-SMP physics: SMP moves the breast bones after the game has placed
everything that hangs under them, so the emitters, the milk and the test beads would
stay where the animation had the breast. The plugin sees that on a bone and puts them
back on the moving breast every frame. CBPC moves the bones before that step and
needs nothing.

`lactis status` in the console prints what was found on an actor and from which mesh
and bone; `lactis nipples 1` marks the two points with a bead.

## No squirt through clothes

When clothes or armor cover a nipple, that side's squirt is hidden, and it shows again
as soon as the nipple is bare. The milk leak is not affected: it runs over the
clothes. Each side is judged on its own, so a top that bares one breast squirts from
that one.

A nipple counts as covered when a garment lies over the body's own nipple and covers
the area around it: a top, a dress, a bikini cup, a cuirass. A piercing, a ring, a
small nipple shield or a harness strap does not cover it. Under a body armor that has
no skin of its own at the breast, the breast counts as covered.

`lactis status` says, per side, `bare` or `covered by` and the mesh. To squirt through
clothes as before, turn off `Squirt > Not through clothes` in the SKSE Menu Framework
(`hide_covered` in the toml).

## Milk Mod Economy

A separate add-on, `LactisMME-<version>.zip` on the release page (source in
`addons\MME`), brings Oninus Lactis to Milk Mod Economy: milking makes the maid squirt,
and a maid whose breasts are full leaks a little now and then. See
`addons\MME\README.md`.

## Console (ConsoleUtil Extended)

Commands act on the console-selected actor, or the player.

| Command | Does |
|---|---|
| `lactis leak 0.7 30` | leak at intensity 0.7 for 30 s (0 s = until stopped) |
| `lactis stop` / `lactis stop 1` | stop; `1` also wipes |
| `lactis drip` | one drop from each nipple |
| `lactis squirt 10 1` | the particle squirt for 10 s at level 1 |
| `lactis status` | what was found on the body, and the state of the milk |
| `lactis nipples 1` | mark the nipples found |
| `lactis auto 0` | squirt emitters at the MCM offsets instead of the nipples found |
| `lactis mount 1` | draw the milk in world space (see Troubleshooting) |

## For mod authors

The 2.1 API is unchanged: the `OLactis.Lactating`, `OLactis.Cleanup`,
`OLactis.SetOffset`, `OLactis.RemoveOffset` mod events and the functions of
`OninusLactis.psc` work as before. A squirt now also starts the fluid leak, which
stops with the squirt; a leak you started with `OLactis.Leak` is left alone by
squirts.

New in 2.2:

```papyrus
; the leak alone, without the squirt: 20 s at stage 1 (0 to 2)
int evt = ModEvent.Create("OLactis.Leak")
ModEvent.PushForm(evt, akActor)
ModEvent.PushInt(evt, 20)      ; seconds, 0 = until OLactis.StopLeak
ModEvent.PushInt(evt, 1)
ModEvent.Send(evt)

int stop = ModEvent.Create("OLactis.StopLeak")
ModEvent.PushForm(stop, akActor)
ModEvent.Send(stop)
```

Or directly, when you compile against `LactisNative.psc`:

```papyrus
if LactisNative.Loaded()
    LactisNative.Leak(akActor, 0.7, 20.0)   ; intensity 0 to 1, seconds
    LactisNative.Drip(akActor, 2)           ; one drop from both
    LactisNative.StopLeak(akActor)
endif
```

The plugin sends `OLactis.LeakStart` (numArg = intensity) and `OLactis.LeakStop` with
the actor as the sender. `StopNippleLeak` still stops only the overlay, so a mod that
swaps in its own overlay does not end the fluid leak.

## Fixed from 2.1

- A squirt level was shared between actors: a level 1 request could play the level 3
  mesh another actor had set last.
- The emitter offset was applied on a fixed 0.05 s wait after the equip; when the mesh
  attached later, the stream started inside the breast. The plugin now places the
  emitters when the mesh is there, and again after the actor's 3D reloads.
- Every squirt request waited a random 0 to 2 seconds, refreshes included. A refresh
  is now immediate; a new effect staggers by at most 0.3 s.
- A missing storage property ended the maintenance before the hotkey was registered.
- The Portuguese translation was saved in an encoding the game does not read.

## Troubleshooting

- **Drops float beside a bouncing breast.** Some physics setups move the breast mesh
  without moving what hangs under the breast bone. Turn on `Test > Draw in world
  space` (or `lactis mount 1`).
- **The squirt starts in the wrong place on one actor.** `lactis status` on that actor
  says how each nipple was found. `estimated` means no mesh on the actor is weighted
  to a breast bone; use the MCM actor offsets for that outfit, or `lactis auto 0`.
- **Nothing leaks.** `lactis status`; then `OninusLactis.log` in the SKSE log folder
  with `Detailed log` on.

## Building

- `git submodule update --init --recursive`, then `xmake f -m release` and `xmake`:
  the DLL into `dist\SKSE\Plugins`.
- `xmake build papyrus`: Pyro compiles `papyrus\Source` into `dist\Scripts` and writes
  `Release\OninusLactisNG-<version>.zip`. `xmake build release` rebuilds the DLL first.
- `python tools/make_milk.py` regenerates the milk meshes and textures.
- `python tools/nipple_check.py <mesh.nif>` runs the nipple rule on a nif file;
  `python tools/cover_check.py <body.nif> <outfit.nif>` the covered-nipple rule.
- `python tools/make_ube_leak.py` repaints the overlay leak texture for the UBE body;
  `python tools/add_ube_leak_shader.py` adds its effect shader to both plugins.

## Credits and license

- Oninus Lactis by Orbus Ninus; Oninus Lactis NG 2.1 by its maintainers.
- The fluid simulation and its rendering are adapted from OStrands by LeonVanWinkle
  (Saint), by way of Lacrima. See `NOTICE.txt`.

The scripts, meshes and textures of 2.1 keep their terms: free to use and build upon
with attribution, no paid distribution. `OninusLactis.dll` and its source are
GPL-3.0 (`LICENSE`).
