Scriptname LactisMME extends Quest
{Oninus Lactis for Milk Mod Economy (LactisMME.esp). Milking makes the nipples squirt,
and a maid whose breasts MME has filled leaks milk. It speaks to Oninus Lactis through
Lactis's mod events and reads MME through MME's own mod events and the values MME keeps
on each maid, so neither plugin is a master of LactisMME.esp. Without MME it does
nothing.}

; One squirt per MME milking stage (5 s by default), a little longer so back-to-back
; stages run without a gap; every new stage refreshes it.
int kSquirtSeconds = 7

; While MME has a maid full, she leaks: checked this often, each time for a little
; longer than the wait, so the leak runs on until she is no longer full.
float kPollSeconds = 10.0
int kLeakSeconds = 15
float kPollRadius = 4096.0

; Bump to re-run Maintenance on saves that already have the quest running.
int kVersion = 1
int version = 0

Actor[] squirting       ; actors this script made squirt, so a stop reaches only them
float[] squirtingUntil  ; real time each squirt runs out

Event OnInit()
  Maintenance()
  RegisterForSingleUpdate(kPollSeconds)
EndEvent

Function Maintenance()
  version = kVersion
  squirting = new Actor[16]
  squirtingUntil = new float[16]
  ; MME sends these for its controller vibration: (Form who, int mpas, int milkingType)
  RegisterForModEvent("MilkQuest.MilkingStage", "OnMilkingStage")
  RegisterForModEvent("MilkQuest.StopMilkingMachine", "OnMilkingStop")
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

Event OnUpdate()
  if version != kVersion
    Maintenance()
  endif
  if Game.GetModByName("MilkModNEW.esp") != 255
    Actor player = Game.GetPlayer()
    LeakIfFull(player)
    Actor[] around = MiscUtil.ScanCellNPCs(player, kPollRadius)
    int i = 0
    while i < around.Length
      if around[i] != player
        LeakIfFull(around[i])
      endif
      i += 1
    endwhile
  endif
  RegisterForSingleUpdate(kPollSeconds)
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
  int handle = ModEvent.Create("OLactis.Leak")
  if handle
    ModEvent.PushForm(handle, akActor)
    ModEvent.PushInt(handle, kLeakSeconds)
    ModEvent.PushInt(handle, 1)
    ModEvent.Send(handle)
  endif
EndFunction
