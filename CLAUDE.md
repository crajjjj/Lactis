# CLAUDE.md

Guidance for Claude Code in this repository.

## What this is

Oninus Lactis NG 2.2: the 2.1 mod (Papyrus scripts, `OninusLactis.esp`, particle
squirt meshes) plus a native plugin, `OninusLactis.dll` (C++23 on CommonLibSSE-NG,
alandtse fork v15.2.0, submodule at `lib/commonlibsse-ng`). The DLL does three things:

1. **Finds the nipples** on the meshes an actor wears (`Body.cpp`).
2. **Places the squirt emitters** on them and keeps them there, and lets every
   humanoid race wear the squirt armor (`Squirt.cpp`).
3. **The fluid milk leak** (`Milk.cpp`): drops that well up at the nipples and run
   down the skin.

The DLL is a fork of the user's Lacrima (`C:\Playground\Skyrim\mods\Lacrima`,
procedural tears), copied at Lacrima 0.5.0 and not shared with it. GPL-3.0 because the
fluid core comes from OStrands; `NOTICE.txt` lists what. The 2.1 scripts and assets
keep their own terms.

Upstream 2.1 is `Downloads\Oninus Lactis NG v2.1.7z`; an unpacked copy of its scripts
is in `C:\Playground\Skyrim\mods\build\Oninus Lactis NG` (a dependency folder other
projects import from: do not turn it into this project).

## Build

- `xmake f -m release` then `xmake`: DLL into `dist\SKSE\Plugins`. Never run a bare
  `xmake f --<option>=...` (it resets the rest of the config).
- The CommonLib plugin rule would install the DLL after every build into
  `%XSE_TES5_MODS_PATH%\OninusLactis`, or with that unset into the game's own Data
  folder. `xmake.lua` overrides `on_install` with nothing; keep that.
- `xmake build papyrus`: Pyro compiles `papyrus\Source` into `dist\Scripts` and writes
  `Release\OninusLactisNG-<version>.zip` (the `dist` tree). Imports come from the
  game's `Data\Source\Scripts` (SkyUI SDK, PapyrusUtil, SKSE are there).
- `python tools/make_milk.py`: `dist\meshes\OninusLactis\milk_*.nif` and
  `dist\textures\lactis\milk*.dds`. Bone names `BlobNN` / `RopeNN` and the rope bone
  count are shared with `src/Milk.cpp`.
- `python tools/add_translations.py dist/Interface/Translations`: appends new `$OL_`
  keys to all eleven files (UTF-16 LE with BOM; English text in every language).
- `python tools/nipple_check.py <mesh.nif>`: the nipple rule of `Body.cpp`, offline.
- Version: `xmake.lua` `PROJECT_VERSION` and `fVersion` in `OninusLactis.psc`. The
  MCM's `GetVersion()` is a plain counter (3 since 2.2).
- `dist\OninusLactis.esp` is 2.1's original-slot plugin, unchanged;
  `dist\Optional\...\OninusLactis.esp` the 56/60 one. Form IDs the code uses: quest
  `0xD61`, armor `0xD6B`, armor addons `0xD6A` / `0xD6C`, overlay shader `0xD69`.

## Vocabulary

`Milk.cpp` is Lacrima's `Tears.cpp` with as few edits as would do, so its names are
the tears': a **Crier** is a leaking actor, a **Cheek** one breast's fluid domain, a
**duct** / **Spot** a nipple, `kEyes` the two sides, `Cry` / `Shed` the leak and the
single drop. The running-mascara code (`Stain`, `Smudge`, lids) is still there and
never runs: `Settings` holds its values at zero and no mark mesh ships. Leave it
unless a cleanup is asked for; removing it means re-reading 3000 lines.

## Architecture

- **Threading.** `Hooks.cpp`: an input-event sink queues one SKSE task per frame; on
  the main thread it runs `Body::BeginFrame`, `Milk::Update`, `Squirt::Update`.
  Papyrus natives and the menu never touch the scene graph: they queue requests.
- **Body** (`Body.cpp`): per actor, every visible skinned shape under the 3D root
  (not the facegen head, not Lactis's own `LactisMilk` nodes) is read from its skin
  partition's vertex buffer; the vertices whose heaviest bone is a torso bone
  (breast, spine, belly, pelvis, thigh) become wrap points on a 0.25-unit voxel.
  **The nipple** (`FindTip`): the tip bone of a side is the breast bone with the
  highest trailing number (`L Breast03` over `NPC L Breast`); of the vertices that
  bone carries, those within 85% of its heaviest weight, and of those the ones
  within 0.15 units of the furthest out along their mean normal. Weights peak at the
  nipple on 3BA, UBE and BHUNP and survive every slider and morph, which is why this
  is a weight rule and not a geometry or UV rule (the rule "furthest forward vertex"
  lands 2.7 units inside the cleavage on a zero-slider body). With several shapes
  the outermost one wins among those carrying at least 25% of the widest shape's
  breast vertices (a pendant or nipple jewellery is weighted to the bone too). No
  shape with a breast bone: the surface nearest the skeleton's own breast bone,
  reported as `estimated`. The rebuild trigger is the list of (geometry, skin
  partition) pointers, checked every 2 s: skee swaps the partition when it
  re-applies morphs.
  Per frame `Of` poses the points around the nipples and those whose bind position
  can fall in the fluid's region, and reports per side whether the skin under that
  side's fluid moved against the side's bone (`moving`: each joint's two probe
  points, taken into the bone's space, compared with last frame).
- **Milk** (`Milk.cpp`): one simulation per breast, **in the frame of the breast bone
  the nipple is skinned to** (`Crier::frames`, `anchors`; Spine2 when there is no
  such bone). The milk nodes hang under that bone, so a drop at rest on a bouncing
  breast is carried by the engine and sleeps; skin moving against the bone (the
  belly below, the breast's other bones) wakes the fluid. A bone scaled by node
  inflation scales the simulation metre, so `Prepare` divides the particle spacing
  by the scale (in steps of 0.05) to keep drops the same size. A different bone
  name under the same side (an outfit weighted differently) clears that side's
  fluid; a reloaded 3D keeps it. The actor's own hands are colliders per side.
- **Squirt** (`Squirt.cpp`): `Place` queues an actor; each frame until the group
  nodes `Lactis Group L/R` exist under the actor's 3D, then every 0.5 s, the node's
  local translation is set to the nipple (plus `[squirt] lift` along the normal) in
  its parent's space, or to the MCM offset when the search is off or finds nothing.
  Scale and the debug axis are set there too. `PatchRaces` at `kDataLoaded` adds
  every `ActorTypeNPC` race to the two armor addons.
- **Scripts.** `LactisNative.psc` (natives; `Loaded()` = `SKSE.GetPluginVersion`),
  `LactisTest.psc` + `dist\SKSE\CustomConsole\OninusLactis.yaml` (CUE, alias
  `lactis`; args by position, no `%` in returned strings). `OninusLactis.psc`:
  `StartMilkLeak` / `StopMilkLeak` drive the fluid leak (unless `FluidLeakDisabled`,
  inverted so an old save gets it on) and the overlay (`NippleLeakEnabled`);
  `StopNippleLeak` still stops only the overlay, because other mods call it to swap
  in their own. `LactisNippleSquirtArmor.UpdateNodeProperties` hands placement to
  the DLL when it is loaded.
- **Settings** (`Settings.cpp`): one field table drives the toml reader, the user
  file writer and the menu pages. Defaults belong in `dist\SKSE\Plugins\OninusLactis.toml`;
  the `Settings.h` initialisers mirror it, and `make_milk.py` authors the material
  at the same `[look]` values.

## Rules

- Never change the arguments of a shipped native or mod event; add a new one.
- The 2.1 API (mod events, `OninusLactis.psc` functions and properties, form IDs)
  is used by other mods: Estrus, Beeing Female patches, SGO, SLO VE. Keep it.
- CK-filled properties stay in the `.psc` even if unused.
- A local named `near` or `far` breaks the build (Windows.h macros).
