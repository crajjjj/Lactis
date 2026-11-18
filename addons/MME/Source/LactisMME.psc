Scriptname LactisMME extends Quest
{Oninus Lactis for Milk Mod Economy (LactisMME.esp). Milking makes the nipples squirt,
and a maid whose breasts MME has filled leaks a little now and then. It speaks to
Oninus Lactis through Lactis's mod events and reads MME through MME's own mod events
and the values MME keeps on each maid, so neither plugin is a master of LactisMME.esp.
Without MME it does nothing.}

; The settings are read-only properties, not variables: a save keeps every script
; variable's value over its initializer, so a setting kept in a variable could never be
; changed for a save that has run the quest.

; This script's revision: bump it when Maintenance must run again. A save whose
; `version` differs runs it at the next hook (a loading screen, a milking event, MME's
; milk cycle). 1: 1.0.0.
int Property CURRENT_VERSION = 1 AutoReadOnly

; Squirt for this long per MME milking stage: Lactis's longest. A stage lasts as long as
; MME's Milking_Duration says (5 s by default, more with its per-second animation work
; and a feeding stage between), so the next stage refreshes the squirt before it ends,
; and the machine's stop cuts it short.
int Property SQUIRT_SECONDS = 18 AutoReadOnly

; A full maid leaks in bursts, as MME's own leak does: one short, light burst each time
; MME's milk cycle runs and finds her full (every MilkPoll game hours, 1 by default, or
; every MilkPoll real seconds when it is set above 24), at most one per LEAK_GAP seconds.
int Property LEAK_SECONDS = 20 AutoReadOnly
int Property LEAK_STAGE = 0 AutoReadOnly
float Property LEAK_GAP = 90.0 AutoReadOnly
; The maids looked at: those in the player's cell within this distance (the whole
; interior, or the player's exterior cell; maids in a neighbouring exterior cell are
; not seen until the player crosses over).
float Property LEAK_RADIUS = 4096.0 AutoReadOnly

int version = 0

Actor[] squirting       ; actors this script made squirt, so a stop reaches only them
float[] squirtingUntil  ; real time each squirt runs out
Actor[] leaked          ; actors given a burst lately, and the real time of it
float[] leakedAt
int nextLeaked = 0      ; the slot a new actor takes, round the ring

Event OnInit()
  Maintenance()
EndEvent

Function Upgrade()
  if version != CURRENT_VERSION
    Maintenance()
  endif
EndFunction

Function Maintenance()
  version = CURRENT_VERSION
  squirting = new Actor[16]
  squirtingUntil = new float[16]
  leaked = new Actor[32]
  leakedAt = new float[32]
  nextLeaked = 0
  ; MME sends these for its controller vibration: (Form who, int mpas, int milkingType)
  RegisterForModEvent("MilkQuest.MilkingStage", "OnMilkingStage")
  RegisterForModEvent("MilkQuest.StopMilkingMachine", "OnMilkingStop")
  ; and this once its milk cycle has run over every maid
  RegisterForModEvent("MME_MilkCycleComplete", "OnMilkCycle")
  ; every loading screen, so a later revision of this script reaches the save at once
  RegisterForMenu("Loading Menu")
  Debug.Trace("LactisMME: script revision " + version + " running")
EndFunction

Event OnMenuClose(string asMenuName)
  Upgrade()
EndEvent

; ---------------------------------------------------------------- milking: squirt

; A milking stage began: MME's mpas is 2 when the maid is aroused past 75, else 1.
Event OnMilkingStage(Form akWho, int aiMpas, int aiMilkingType)
  Upgrade()
  Actor akActor = akWho as Actor
  if !akActor
    return
  endif
  int level = 1
  if aiMpas >= 2
    level = 2
  endif
  Squirt(akActor, SQUIRT_SECONDS, level)
  int i = squirting.Find(akActor)
  if i < 0
    i = FreeSquirtSlot()
  endif
  if i >= 0
    squirting[i] = akActor
    squirtingUntil[i] = Utility.GetCurrentRealTime() + SQUIRT_SECONDS
  endif
EndEvent

; The machine let go: end a squirt this script started, if it still runs. (A refresh
; of one second is how a squirt is cut short through Lactis's events.)
Event OnMilkingStop(Form akWho, int aiMpas, int aiMilkingType)
  Upgrade()
  Actor akActor = akWho as Actor
  if !akActor
    return
  endif
  int i = squirting.Find(akActor)
  if i < 0
    return
  endif
  bool running = SquirtRunning(i)
  squirting[i] = None
  squirtingUntil[i] = 0.0
  if running
    Squirt(akActor, 1, 1)
  endif
EndEvent

; Whether slot i's squirt is still on. A time further ahead than one squirt is from
; before a restart (real time starts again at 0 with the game): not running.
bool Function SquirtRunning(int i)
  float left = squirtingUntil[i] - Utility.GetCurrentRealTime()
  return left > 0.0 && left <= SQUIRT_SECONDS + 1
EndFunction

; An empty slot, or one whose squirt has run out (a stop MME never sent).
int Function FreeSquirtSlot()
  int i = squirting.Find(None)
  if i >= 0
    return i
  endif
  i = 0
  while i < squirting.Length
    if !SquirtRunning(i)
      return i
    endif
    i += 1
  endwhile
  return -1
EndFunction

Function Squirt(Actor akActor, int aiSeconds, int aiLevel)
  int handle = ModEvent.Create("OLactis.Lactating")
  if handle
    ModEvent.PushForm(handle, akActor)
    ModEvent.PushInt(handle, aiSeconds)
    ModEvent.PushInt(handle, aiLevel)
    ModEvent.Send(handle)
  endif
EndFunction

; ---------------------------------------------------------------- full: leak

Event OnMilkCycle(string asEventName, string asStrArg, float afNumArg, Form akSender)
  Upgrade()
  Actor player = Game.GetPlayer()
  LeakIfFull(player)
  Actor[] around = MiscUtil.ScanCellNPCs(player, LEAK_RADIUS)
  int i = 0
  while i < around.Length
    if around[i] != player
      LeakIfFull(around[i])
    endif
    i += 1
  endwhile
EndEvent

; A burst when MME would leak her: her milk at her maximum while she still makes milk,
; and no chastity bra or nipple plug holding it in. Read straight from the values MME
; stores on her: MME's own getters would write a default onto any actor that is not a
; maid, and log a line for each.
Function LeakIfFull(Actor akActor)
  if !akActor || !StorageUtil.HasFloatValue(akActor, "MME.MilkMaid.MilkMaximum")
    return
  endif
  float most = StorageUtil.GetFloatValue(akActor, "MME.MilkMaid.MilkMaximum")
  if most <= 0.0 || StorageUtil.GetFloatValue(akActor, "MME.MilkMaid.MilkCount") < most
    return
  endif
  if StorageUtil.GetFloatValue(akActor, "MME.MilkMaid.MilkGen") <= 0.0 || !akActor.Is3DLoaded()
    return
  endif
  ; being milked: the squirt's own leak is on, stronger
  int milking = squirting.Find(akActor)
  if milking >= 0 && SquirtRunning(milking)
    return
  endif
  if HeldIn(akActor)
    return
  endif
  ; one burst per LEAK_GAP (a time ahead of now is from before a restart: stale)
  float now = Utility.GetCurrentRealTime()
  int i = leaked.Find(akActor)
  if i >= 0 && leakedAt[i] <= now && now - leakedAt[i] < LEAK_GAP
    return
  endif
  if i < 0
    i = nextLeaked  ; the oldest entry makes room
    nextLeaked = (nextLeaked + 1) % leaked.Length
  endif
  leaked[i] = akActor
  leakedAt[i] = now
  int handle = ModEvent.Create("OLactis.Leak")
  if handle
    ModEvent.PushForm(handle, akActor)
    ModEvent.PushInt(handle, LEAK_SECONDS)
    ModEvent.PushInt(handle, LEAK_STAGE)
    ModEvent.Send(handle)
  endif
EndFunction

; MME's own leak skips a maid in a Devious Devices chastity bra, and one with a nipple
; plug (a slot 56 item named "Plug", MME's PiercingCheck).
bool Function HeldIn(Actor akActor)
  Keyword bra = Keyword.GetKeyword("zad_DeviousBra")
  if bra && akActor.WornHasKeyword(bra)
    return true
  endif
  Form nipples = akActor.GetWornForm(Armor.GetMaskForSlot(56))
  return nipples && StringUtil.Find(nipples.GetName(), "Plug") >= 0
EndFunction
