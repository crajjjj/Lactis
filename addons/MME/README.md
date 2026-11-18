# Oninus Lactis for Milk Mod Economy (LactisMME)

A small add-on that brings Oninus Lactis NG to Milk Mod Economy:

- **Milking squirts.** Every milking stage makes the maid squirt: level 2 when she is
  aroused past 75, level 1 otherwise. The squirt stops when the machine lets go.
- **Full breasts leak.** While a maid near the player has her milk at her maximum, her
  nipples leak (checked every 10 seconds). The leak runs out once she is no longer
  full.

## Requirements

- Oninus Lactis NG 2.2 or later
- Milk Mod Economy

## Install

Install `LactisMME-<version>.zip` with a mod manager, next to Oninus Lactis NG, and
enable `LactisMME.esp`. It is a light plugin, so it takes no load order slot. Neither
Milk Mod Economy nor Oninus Lactis is a master of it, so its place in the load order
does not matter, and without Milk Mod Economy it does nothing. Adding it to a running
save is fine.

Milk Mod Economy's own leak effects stay on beside these. Turn them off on the Debug page
of Milk Mod Economy's menu if you want only Lactis's.

## How it works

`LactisMME.esp` holds one quest that starts with the game and carries the script
`LactisMME.psc`. The script listens for Milk Mod Economy's `MilkQuest.MilkingStage` and
`MilkQuest.StopMilkingMachine` events and sends Oninus Lactis's `OLactis.Lactating`. For
the leak it reads the milk level Milk Mod Economy keeps on each maid (the StorageUtil
values `MME.MilkMaid.MilkCount` and `MME.MilkMaid.MilkMaximum`) and sends `OLactis.Leak`.

## Building

`xmake build mme` in the repository root compiles the script and writes
`Release\LactisMME-<version>.zip`. The add-on's version is `ADDON_VERSION` in
`scripts\addon_mme.lua`; `kVersion` in the script re-runs its setup on saves that
already have it.

## License

GPL-3.0, as the rest of the Oninus Lactis NG native code (see `LICENSE` in the
repository root).
