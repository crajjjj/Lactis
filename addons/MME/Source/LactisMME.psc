Scriptname LactisMME extends Quest
{Oninus Lactis for Milk Mod Economy (LactisMME.esp). Milking makes the nipples squirt,
and a maid whose breasts MME has filled leaks a little now and then. It speaks to
Oninus Lactis through Lactis's mod events and reads MME through MME's own mod events
and the values MME keeps on each maid, so neither plugin is a master of LactisMME.esp.
Without MME it does nothing.}

; One squirt per MME milking stage (5 s by default), a little longer so back-to-back
; stages run without a gap; every new stage refreshes it.
int kSquirtSeconds = 7

; A full maid leaks in bursts, as MME's own leak does: one short, light burst each time
; MME's milk cycle runs (every MilkPoll game hours, 1 by default, about 3 minutes of
; play) and finds her full, never two within kLeakGap seconds of each other (MME can be
; set to cycle much faster, and waiting or sleeping runs many cycles at once).
int kLeakSeconds = 20
int kLeakStage = 0
float kLeakGap = 90.0
float kLeakRadius = 4096.0

; Bump to re-run Maintenance on saves that already have the quest running.
; 1: 1.0.0 (a leak kept up every 10 s while full). 2: bursts on MME's milk cycle.
int kVersion = 2
int version = 0

Actor[] squirting       ; actors this script made squirt, so a stop reaches only them
float[] squirtingUntil  ; real time each squirt runs out
Actor[] leaked          ; actors given a burst lately, and the real time of it
float[] leakedAt
int nextLeaked = 0      ; the slot a new actor takes, round the ring

Event OnInit()
  Maintenance()
EndEvent

; 1.0.0 polled on a 10 s update loop. That loop's next tick lands here once on an old
; save: it upgrades and is not renewed.
Event OnUpdate()
  if version != kVersion
    Maintenance()
  endif
EndEvent

Function Maintenance()
  version = kVersion
  squirting = new Actor[16]
  squirtingUntil = new float[16]
  leaked = new Actor[32]
  leakedAt = new float[32]
  ; MME sends these for its controller vibration: (Form who, int mpas, int milkingType)
  RegisterForModEvent("MilkQuest.MilkingStage", "OnMilkingStage")
  RegisterForModEvent("MilkQuest.StopMilkingMachine", "OnMilkingStop")
  ; and this once its milk cycle has run over every maid
  RegisterForModEvent("MME_MilkCycleComplete", "OnMilkCycle")
  Debug.Trace("LactisMME: version " + version + " running")
EndFunction

; ---------------------------------------------------------------- milking: squirt

; A milking stage began: MME's mpas is 2 when the maid is aroused past 75, else 1.
Event OnMilkingStage(Form akWho, int aiMpas, int aiMilkingType)
  Actor akActor = akWho as Actor
  if !akActor
    return
  endif
  int level = 1
  if aiMpas >= 2
    level = 2
  endif
  Squirt(akActor, kSquirtSeconds, level)
  int i = squirting.Find(akActor)
  if i < 0
    i = squirting.Find(None)
  endif
  if i >= 0
    squirting[i] = akActor
    squirtingUntil[i] = Utility.GetCurrentRealTime() + kSquirtSeconds
  endif
EndEvent

; The machine let go: end a squirt this script started, if it still runs. (A refresh
; of one second is how a squirt is cut short through Lactis's events.)
Event OnMilkingStop(Form akWho, int aiMpas, int aiMilkingType)
  Actor akActor = akWho as Actor
  int i = squirting.Find(akActor)
  if !akActor || i < 0
    return
  endif
  bool running = Utility.GetCurrentRealTime() < squirtingUntil[i]
  squirting[i] = None
  squirtingUntil[i] = 0.0
  if running
    Squirt(akActor, 1, 1)
  endif
EndEvent

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
  if version != kVersion
    Maintenance()
  endif
  Actor player = Game.GetPlayer()
  LeakIfFull(player)
  Actor[] around = MiscUtil.ScanCellNPCs(player, kLeakRadius)
  int i = 0
  while i < around.Length
    if around[i] != player
      LeakIfFull(around[i])
    endif
    i += 1
  endwhile
EndEvent

; MME leaks a maid when her milk reaches her maximum. Read straight from the values
; MME stores on her: MME's own getters would write a default onto any actor that is
; not a maid, and log a line for each.
Function LeakIfFull(Actor akActor)
  if !akActor || !akActor.Is3DLoaded() || !StorageUtil.HasFloatValue(akActor, "MME.MilkMaid.MilkMaximum")
    return
  endif
  float most = StorageUtil.GetFloatValue(akActor, "MME.MilkMaid.MilkMaximum")
  if most <= 0.0 || StorageUtil.GetFloatValue(akActor, "MME.MilkMaid.MilkCount") < most
    return
  endif
  ; one burst per kLeakGap (a time ahead of now is from before a restart: stale)
  float now = Utility.GetCurrentRealTime()
  int i = leaked.Find(akActor)
  if i >= 0 && leakedAt[i] <= now && now - leakedAt[i] < kLeakGap
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
    ModEvent.PushInt(handle, kLeakSeconds)
    ModEvent.PushInt(handle, kLeakStage)
    ModEvent.Send(handle)
  endif
EndFunction
