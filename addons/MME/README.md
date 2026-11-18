# Oninus Lactis for Milk Mod Economy (LactisMME)

A small add-on that brings Oninus Lactis NG to Milk Mod Economy:

- **Milking squirts.** Every milking stage makes the maid squirt: level 2 when she is
  aroused past 75, level 1 otherwise. The squirt stops when the machine lets go.
- **Full breasts leak, now and then.** Each time Milk Mod Economy's milk cycle runs
  (every MilkPoll game hours, 1 by default, about 3 minutes of play) and finds a maid
  full, her nipples give a short, light leak of 20 seconds, as Milk Mod Economy's own
  leak does. Never more than one burst per maid in 90 seconds. Like Milk Mod Economy's
  own leak, there is none while she no longer makes milk, wears a Devious Devices
  chastity bra or a nipple plug, or is being milked. The player is included; other maids
  are seen in the player's cell (the whole interior, or the player's exterior cell).

## Requirements

- Oninus Lactis NG 2.2 or later
- Milk Mod Economy

## Install

Install `LactisMME-<version>.zip` with a mod manager, next to Oninus Lactis NG, and
enable `LactisMME.esp`. It is a light plugin, so it takes no load order slot. Neither
Milk Mod Economy nor Oninus Lactis is a master of it, so its place in the load order
does not matter, and without Milk Mod Economy it does nothing. Adding it to a running
save is fine, and so is updating it.

Milk Mod Economy's own leak effects stay on beside these. Turn them off on the Debug page
of Milk Mod Economy's menu if you want only Lactis's.

## How it works

`LactisMME.esp` holds one quest that starts with the game and carries the script
`LactisMME.psc`. The script listens for Milk Mod Economy's `MilkQuest.MilkingStage` and
`MilkQuest.StopMilkingMachine` events and sends Oninus Lactis's `OLactis.Lactating`. On
Milk Mod Economy's `MME_MilkCycleComplete` it reads the milk Milk Mod Economy keeps on
each maid nearby (the StorageUtil values `MME.MilkMaid.MilkCount`,
`MME.MilkMaid.MilkMaximum` and `MME.MilkMaid.MilkGen`) and sends `OLactis.Leak` for the
full ones.

## Building

`xmake build mme` in the repository root compiles the script and writes
`Release\LactisMME-<version>.zip`. The add-on's version is `ADDON_VERSION` in
`scripts\addon_mme.lua`. The script's own `CURRENT_VERSION` re-runs its setup on saves
that ran an earlier revision: bump it whenever the setup changes. Settings in the script
are `AutoReadOnly` properties on purpose: a save keeps a script variable's value over
its initializer, so a setting kept in a variable never changes for an existing save.

## License

GPL-3.0, as the rest of the Oninus Lactis NG native code; the archive carries it as
`LactisMME-LICENSE.txt`.
