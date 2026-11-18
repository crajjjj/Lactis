;
; ██╗      █████╗  ██████╗████████╗██╗███████╗
; ██║     ██╔══██╗██╔════╝╚══██╔══╝██║██╔════╝
; ██║     ███████║██║        ██║   ██║███████╗
; ██║     ██╔══██║██║        ██║   ██║╚════██║
; ███████╗██║  ██║╚██████╗   ██║   ██║███████║
; ╚══════╝╚═╝  ╚═╝ ╚═════╝   ╚═╝   ╚═╝╚══════╝
;
; If you are a mod developer and want to integrate the nipple squirt effect
; into your own mod, use the following public API. See the indiviudal functions
; for documentation.
;
; Public API
; ---
;
; ModEvents
; ---
; + OLactis.Lactating(Form Who, Int Duration, Int Stage)
;   Starts a nipple squirt effect on the specified actor for the given duration
;   and stage (0-2). Duration is in seconds.
;
; + OLactis.Cleanup()
;   Triggers cleanup of stuck effects and arrays. Use this when you need to
;   ensure all effects are properly cleaned up.
;
; + OLactis.SetOffset(Form Who, Float XOffset, Float ZOffset, Float YOffset, Float LactisScale)
;   Sets the nipple offset and scale for a specific actor. Use this to customize
;   the effect position and size for individual actors. With OninusLactis.dll the
;   emitters go to the nipples found on the actor's meshes and the offset is only
;   the fallback; the scale applies either way.
;
; + OLactis.RemoveOffset(Form Who)
;   Removes the stored offset and scale for a specific actor.
;
; + OLactis.Unequipped(Form Who)
;   Sent when an actor unequips the nipple squirt armor. Use this to handle
;   cleanup when the effect is removed.
;
; + OLactis.Leak(Form Who, Int Duration, Int Stage)                       (2.2)
;   The milk leak alone, without the squirt: drops well up at the nipples and
;   run down the skin for Duration seconds (0 = until OLactis.StopLeak), at
;   Stage 0-2. A squirt starts the same leak by itself.
;
; + OLactis.StopLeak(Form Who)                                            (2.2)
;   No new drops; the milk on the skin dries.
;
; + OLactis.LeakStart / OLactis.LeakStop                                  (2.2)
;   Sent by OninusLactis.dll with the leaking actor as the sender (numArg of
;   LeakStart = intensity). LactisNative.psc has the same as functions.
;
; DEPRECATED API - Use ModEvents instead
; + StartNippleSquirt(Actor actorRef, int level=0)
; + StopNippleSquirt(Actor actorRef)
; + ToggleNippleSquirt(Actor actorRef, int level=0)
; + PlayNippleSquirt(Actor actorRef, float duration, int level=0)
; + HasNippleSquirt(Actor actorRef)
;
; All other functions and properties are considered private and should not be
; used by other mods.

ScriptName OninusLactis extends SKI_QuestBase

Actor Property PlayerRef Auto
Armor Property NippleSquirtArmor Auto
EffectShader Property LactisNippleLeakCBBE Auto
LactisActorStorage Property actorStorage Auto

Bool Property NippleLeakEnabled Auto
Bool Property DebugAxisEnabled Auto
Bool Property DebugLoggingEnabled Auto
Float[] Property NippleOffsetL Auto
Float Property EmitterScale Auto
Float Property GlobalEmitterScale Auto
Int Property StartLactatingKey Auto
; The fluid milk leak of OninusLactis.dll: drops that well up at the nipples and run
; down the skin, on any body. On unless this is set (so a save from before 2.2,
; which has no value for it, gets the leak). NippleLeakEnabled is the old overlay.
Bool Property FluidLeakDisabled Auto

Actor[] Property QueuedStopActors Auto Hidden
float[] Property QueuedStopTimes Auto Hidden
float Property lastEffectTime = 0.0 Auto Hidden
float Property lastCleanupTime = 0.0 Auto Hidden
int Property activeTimerCount = 0 Auto Hidden

Bool[] Property actorBusy auto Hidden
Int[] Property queuedEffectType auto Hidden ; 0 = none, 1 = Play, 2 = Stop
Float[] Property queuedEffectDuration auto Hidden
Int[] Property queuedEffectLevel auto Hidden

string fVersion = "Oninus Lactis NG 2.2"

Actor[] armorActors
ObjectReference[] armorRefsLeft

; Leaks another mod asked for with OLactis.Leak, which a squirt neither shortens
; nor stops: the actor, and the real time the leak ends (0 = until OLactis.StopLeak).
Actor[] ownLeakActors
Float[] ownLeakUntil

; Helper function for logging that can be toggled in MCM
Function Log(string a_message)
  if DebugLoggingEnabled
    Debug.Trace("OninusLactis: " + a_message)
  endif
EndFunction

; Helper function for critical errors that should always be logged
Function LogError(string a_message)
  Debug.Trace("OninusLactis ERROR: " + a_message)
EndFunction

Event OnInit()
  fVersion = "Oninus Lactis NG " + LactisVersion.GetVersionString()
  Debug.Notification("Installing " + fVersion + "...")
  Debug.Trace("Installing " + fVersion + "...")
  Maintenance()
EndEvent

Event OnPlayerLoadGame()
  RegisterModEvents()
  FixStuckEffects()
endEvent

int Function GetVersion()
  return 2
EndFunction

string Function GetVerboseVersion()
  return fVersion
EndFunction

Function Maintenance()
  ; a script variable keeps the value a save holds, so the version is set on every
  ; load, not by its initializer
  fVersion = "Oninus Lactis NG " + LactisVersion.GetVersionString()
  ; a load ends every fluid leak, so the record of other mods' leaks starts empty
  ownLeakActors = new Actor[20]
  ownLeakUntil = new Float[20]
  CleanupAllArrays()
  if !QueuedStopActors
    QueuedStopActors = new Actor[20]
    QueuedStopTimes = new float[20]
  endif
  if (armorActors.Length != 20)
    Actor[] tempActors = armorActors
    ObjectReference[] tempRefsLeft = armorRefsLeft
    armorActors = new Actor[20]
    armorRefsLeft = new ObjectReference[20]
    if tempActors && tempActors.Length > 0
      int copyLimit = tempActors.Length
      if copyLimit > 20
        copyLimit = 20
      endif
      int i = 0
      while i < copyLimit
        armorActors[i] = tempActors[i]
        if i < tempRefsLeft.Length
          armorRefsLeft[i] = tempRefsLeft[i]
        endif
        i += 1
      endwhile
    endif
  endif
  if !actorBusy || actorBusy.Length != 20
    actorBusy = new Bool[20]
  endif
  if !queuedEffectType || queuedEffectType.Length != 20
    queuedEffectType = new Int[20]
    queuedEffectDuration = new Float[20]
    queuedEffectLevel = new Int[20]
  endif
  RegisterForSingleUpdate(20.0)
  ; on every load, not only at install: a save from 2.1 has never registered the
  ; events added since (the quest's own OnPlayerLoadGame never fires)
  RegisterModEvents()
  ; the storage script sits on this same quest: take it from there, so an unfilled
  ; property heals instead of ending the maintenance before the key is registered
  actorStorage = (self as Form) as LactisActorStorage
  if !actorStorage
    LogError("Could not initialize actorStorage!")
    Return
  endif
  Log("actorStorage initialized: " + actorStorage)
  Log("loaded version is " + fVersion)
  RegisterForKey(StartLactatingKey)
  ApplyArmoredActorProperties()
EndFunction

Function RegisterModEvents()
  Utility.Wait(1)
  UnregisterForModEvent("OLactis.Lactating")
  RegisterForModEvent("OLactis.Lactating","OnModEvent_Lactating")
  UnregisterForModEvent("OLactis.Cleanup")
  RegisterForModEvent("OLactis.Cleanup","OnModEvent_Cleanup")
  UnregisterForModEvent("OLactis.SetOffset")
  RegisterForModEvent("OLactis.SetOffset","OnModEvent_SetOffset")
  UnregisterForModEvent("OLactis.RemoveOffset")
  RegisterForModEvent("OLactis.RemoveOffset","OnModEvent_RemoveOffset")
  UnregisterForModEvent("OLactis.Leak")
  RegisterForModEvent("OLactis.Leak","OnModEvent_Leak")
  UnregisterForModEvent("OLactis.StopLeak")
  RegisterForModEvent("OLactis.StopLeak","OnModEvent_StopLeak")
EndFunction

;
; ██████╗ ██╗   ██╗██████╗ ██╗     ██╗ ██████╗     █████╗ ██████╗ ██╗
; ██╔══██╗██║   ██║██╔══██╗██║     ██║██╔════╝    ██╔══██╗██╔══██╗██║
; ██████╔╝██║   ██║██████╔╝██║     ██║██║         ███████║██████╔╝██║
; ██╔═══╝ ██║   ██║██╔══██╗██║     ██║██║         ██╔══██║██╔═══╝ ██║
; ██║     ╚██████╔╝██████╔╝███████╗██║╚██████╗    ██║  ██║██║     ██║
; ╚═╝      ╚═════╝ ╚═════╝ ╚══════╝╚═╝ ╚═════╝    ╚═╝  ╚═╝╚═╝     ╚═╝
;
; Use ModEvents to start and stop the nipple squirt effect. 
; ---
; ModEvents
; ---
; + OLactis.Lactating(Form Who, Int Duration, Int Stage)
;   Starts a nipple squirt effect on the specified actor for the given duration
;   and stage (0-2). Duration is in seconds.
;
; + OLactis.Cleanup()
;   Triggers cleanup of stuck effects and arrays. Use this when you need to
;   ensure all effects are properly cleaned up.
;
; + OLactis.SetOffset(Form Who, Float XOffset, Float ZOffset, Float YOffset, Float LactisScale)
;   Sets the nipple offset and scale for a specific actor. Use this to customize
;   the effect position and size for individual actors.
;
; + OLactis.RemoveOffset(Form Who)
;   Removes the stored offset and scale for a specific actor.
;
; + OLactis.Unequipped(Form Who)
;   Sent when an actor unequips the nipple squirt armor. Use this to handle
;   cleanup when the effect is removed.
; ---
; DEPRECATED API - Use ModEvents instead
; + StartNippleSquirt(Actor actorRef, int level=0)
; + StopNippleSquirt(Actor actorRef)
; + ToggleNippleSquirt(Actor actorRef, int level=0)
; + PlayNippleSquirt(Actor actorRef, float duration, int level=0)
; + HasNippleSquirt(Actor actorRef)
; ---
; All other functions and properties are considered private and should not be
; used by other mods.

Function OnModEvent_Lactating(Form Who, Int Duration, Int Stage)
  ; Force minimum separation between effect starts
  float currentTime = Utility.GetCurrentRealTime()
  float timeSinceLast = currentTime - lastEffectTime
  
  ; If we're getting calls too quickly, add a forced delay
  if timeSinceLast < 0.2  ; 200ms minimum separation
    Utility.Wait(0.3)  ; Force minimum separation
  endif
  
  ; Record this effect start time for next call
  lastEffectTime = Utility.GetCurrentRealTime()
  
  ; Now proceed with normal processing
  PlayNippleSquirt(Who as Actor, Duration, Stage)
EndFunction

Function OnModEvent_Cleanup(string a_eventName = "", string a_strArg = "", float a_numArg = 0.0, Form a_sender = None)
  PerformCleanup()
EndFunction

Function PerformCleanup()
  ; Debounce cleanup operations - don't allow too frequent cleanups
  float currentTime = Utility.GetCurrentRealTime()
  float timeSinceLast = currentTime - lastCleanupTime
  
  ; If we're getting cleanup calls too quickly, ignore this one
  if timeSinceLast < 1.0  ; 1 second minimum between cleanups
    Log("Ignoring cleanup request - too soon after previous cleanup")
    return
  endif
  
  ; Record this cleanup time for next call
  lastCleanupTime = Utility.GetCurrentRealTime()
  
  Log("Performing cleanup")
  Utility.Wait(0.2)
  FixStuckEffects()
  Utility.Wait(0.2)
  CleanupAllArrays()
EndFunction

; OLactis.Leak(Form Who, Int Duration, Int Stage): the milk leak alone, without the
; squirt, for Duration seconds (0 = until OLactis.StopLeak) at Stage 0 to 2.
Function OnModEvent_Leak(Form Who, Int Duration, Int Stage)
  Actor actorRef = Who as Actor
  if !actorRef
    return
  endif
  NoteOwnLeak(actorRef, Duration)
  StartMilkLeak(actorRef, Duration as float, Stage)
EndFunction

; OLactis.StopLeak(Form Who): no new drops; the milk on the skin dries.
Function OnModEvent_StopLeak(Form Who)
  Actor actorRef = Who as Actor
  ForgetOwnLeak(actorRef)
  StopMilkLeak(actorRef)
EndFunction

Function OnModEvent_SetOffset(Form Who, Float XOffset, Float ZOffset, Float YOffset, Float LactisScale)
  actorStorage.SetNpcOffsetIndex(Who as Actor, 0, XOffset)
  actorStorage.SetNpcOffsetIndex(Who as Actor, 1, ZOffset)
  actorStorage.SetNpcOffsetIndex(Who as Actor, 2, YOffset)
  actorStorage.SetNpcScale(Who as Actor,LactisScale)
EndFunction

Function OnModEvent_RemoveOffset(Form Who)
  actorStorage.DeleteNpcStorage(Who as Actor)
EndFunction

Function StartNippleSquirt(Actor actorRef, int level=0)
  if !actorRef
    return
  endif
  if actorRef.GetLeveledActorBase().GetSex() == 1
    if GetArmoredActorsCount() >= 20
      return
    endif
    if HasArmorRefs(actorRef)
      return
    endif
    LactisNippleSquirtArmor armorRef = StartNippleSquirtLeft(actorRef, level)
    StoreArmorRefs(actorRef, armorRef)
    SquirtLeak(actorRef, level)
  else
    Log("Cannot apply effect to male actor")
    return
  endif
  Utility.Wait(0.05)
EndFunction

; Stops the nipple squirt effect on the given 'actorRef'.
; If the actor does not have an nipple squirt effect running the call will be ignored.
; If the "Nipple Leak" feature is enabled in the MCM this function will also stop
; the nipple leak overlay.
Function StopNippleSquirt(Actor actorRef)
  if !actorRef || !HasArmorRefs(actorRef)
    return
  endif
  int busyIndex = armorActors.Find(actorRef)
  if busyIndex >= 0 && actorBusy[busyIndex]
    Log("Actor " + actorRef.GetDisplayName() + " is busy, queuing StopNippleSquirt")
    queuedEffectType[busyIndex] = 2
    return
  endif
  if busyIndex >= 0
    actorBusy[busyIndex] = true
  endif
  if !HasOwnLeak(actorRef)
    StopMilkLeak(actorRef)
  endif
  LactisNippleSquirtArmor actorArmor = GetArmorRefs(actorRef)
  
  ; Only call StopNippleSquirtInternal if we got a valid armor reference
  if actorArmor
    StopNippleSquirtInternal(actorRef, actorArmor)
  else
    Log("Warning - Could not get armor reference for " + actorRef.GetDisplayName())
  endif
  
  Utility.Wait(0.1)
  RemoveArmorRefs(actorRef)
  if busyIndex >= 0
    actorBusy[busyIndex] = false
    if queuedEffectType[busyIndex] == 1
      Log("Processing queued PlayNippleSquirt for " + actorRef.GetDisplayName())
      float qDuration = queuedEffectDuration[busyIndex]
      int qLevel = queuedEffectLevel[busyIndex]
      queuedEffectType[busyIndex] = 0
      PlayNippleSquirt(actorRef, qDuration, qLevel)
    elseif queuedEffectType[busyIndex] == 2
      Log("Processing queued StopNippleSquirt for " + actorRef.GetDisplayName())
      queuedEffectType[busyIndex] = 0
      StopNippleSquirt(actorRef)
    endif
  endif
EndFunction

; Toggles the nipple squirt effect for the given 'actorRef' on or off using
; the given squirt 'level' in the range [0..2].
Function ToggleNippleSquirt(Actor actorRef, int level=0)
  if !actorRef
    return
  endif
  bool hasNippleSquirt = HasArmorRefs(actorRef)
  ; How long does our operation take?
  ; float ftimeStart = Utility.GetCurrentRealTime()
  if !hasNippleSquirt
    StartNippleSquirt(actorRef, level)
  else
    StopNippleSquirt(actorRef)
  endif
  ; float ftimeEnd = Utility.GetCurrentRealTime()
  ; Console("Starting/stopping took " + (ftimeEnd - ftimeStart) + " seconds to run")
EndFunction

; Plays the nipple squirt effect on the given 'actorRef' for the given 'duration'
; specified in seconds using the given squirt 'level' in the range [0..2].
; The effect will automatically stop and removed from the actor after the given
; duration.
Function PlayNippleSquirt(Actor actorRef, float duration, int level=0)
  if !actorRef
    return
  endif
  int busyIndex = armorActors.Find(actorRef)
  if busyIndex >= 0 && actorBusy[busyIndex]
    Log("Actor " + actorRef.GetDisplayName() + " is busy, queuing PlayNippleSquirt")
    queuedEffectType[busyIndex] = 1
    queuedEffectDuration[busyIndex] = duration
    queuedEffectLevel[busyIndex] = level
    return
  endif
  if busyIndex >= 0
    actorBusy[busyIndex] = true
  endif
  string actorName = actorRef.GetDisplayName()

  ; Cap duration at 18 seconds
  if duration > 18.0
    duration = 18.0
  endif
  
  int inclvl = (level + 1)
  Log(actorRef.GetDisplayName() + " playing level " + inclvl + " nipple squirt for " + duration as int + " seconds")
  bool hasEquippedArmor = actorRef.IsEquipped(NippleSquirtArmor)
  
  bool needsLevelChange = false
  int actorIndex = armorActors.Find(actorRef)
  int timerIndex = QueuedStopActors.Find(actorRef)

  ; We can't reliably get the current level, so check equipped state only
  if hasEquippedArmor && actorIndex >= 0 && timerIndex >= 0
    ; Perfect state - update timer and leak effect
    QueuedStopTimes[timerIndex] = Utility.GetCurrentRealTime() + duration
    Log("Updating existing timer for " + actorName + " to stop in " + duration + " seconds")
    
    ; Refresh the leak
    SquirtLeak(actorRef, level)
    if busyIndex >= 0
      actorBusy[busyIndex] = false
    endif
    return
  endif

  ; A new effect: a short random wait, so several actors started in one frame do
  ; not all equip in the same one. (This used to be up to 2 s, and ran before the
  ; refresh above too.)
  Utility.Wait(GetRandomDelay())

  ; Has armor but level might differ - do level change
  Log("Level change needed for " + actorName + " to " + (level + 1))
  needsLevelChange = true
  
  if needsLevelChange && hasEquippedArmor
    ; Remove existing armor but maintain actor tracking
    Log("Removing old armor for level change on " + actorName)
    actorRef.UnequipItem(NippleSquirtArmor, false, true)
    actorRef.RemoveItem(NippleSquirtArmor, 1, true)
    Utility.Wait(0.05)  ; Small wait to ensure unequip completes
    hasEquippedArmor = false
  endif

  if !hasEquippedArmor
    
    ; Start a new effect
    Log("Starting new effect for " + actorName)
    LactisNippleSquirtArmor armorRef = StartNippleSquirtLeft(actorRef, level)
    StoreArmorRefs(actorRef, armorRef)

    SquirtLeak(actorRef, level)
  endif

  ; Update timer if we didn't already do it in the perfect case
  int existingSlot = -1
  int emptySlot = -1
  int i = 0
  while i < QueuedStopActors.Length
    if QueuedStopActors[i] == actorRef
      existingSlot = i
    elseif QueuedStopActors[i] == None && emptySlot == -1
      emptySlot = i
    endif
    i += 1
  endwhile
  
  if existingSlot >= 0
    ; Update the existing timer
    QueuedStopTimes[existingSlot] = Utility.GetCurrentRealTime() + duration
    Log("Updating existing timer for " + actorName + " to stop in " + duration as int + " seconds")
  elseif emptySlot >= 0
    ; Create a new timer
    QueuedStopActors[emptySlot] = actorRef
    QueuedStopTimes[emptySlot] = Utility.GetCurrentRealTime() + duration
    activeTimerCount += 1
    Log("Creating new timer for " + actorName + " to stop in " + duration as int + " seconds")
  else
    Log("Warning - No free slots to queue effect stop")
  endif
  
  if activeTimerCount > 0
    RegisterForSingleUpdate(0.3)  ; Fine-grained timing for staggered effects
  endif
  if busyIndex >= 0
    actorBusy[busyIndex] = false
  endif
  if busyIndex >= 0
    actorBusy[busyIndex] = false
    if queuedEffectType[busyIndex] == 1
      Log("Processing queued PlayNippleSquirt for " + actorRef.GetDisplayName())
      float qDuration = queuedEffectDuration[busyIndex]
      int qLevel = queuedEffectLevel[busyIndex]
      queuedEffectType[busyIndex] = 0
      PlayNippleSquirt(actorRef, qDuration, qLevel)
    elseif queuedEffectType[busyIndex] == 2
      Log("Processing queued StopNippleSquirt for " + actorRef.GetDisplayName())
      queuedEffectType[busyIndex] = 0
      StopNippleSquirt(actorRef)
    endif
  endif
EndFunction

Event OnUpdate()
  if !QueuedStopActors || !QueuedStopTimes
    ; Safeguard against arrays not being initialized
    Log("Warning - Arrays not initialized in OnUpdate")
    RegisterForSingleUpdate(20.0)
    return
  endif

  CleanupAllArrays()
  float currentTime = Utility.GetCurrentRealTime()
  bool needsQuickUpdate = false
  int i = 0
  
  ; Make sure we don't exceed the array bounds
  int arrayLength = QueuedStopActors.Length
  if QueuedStopTimes.Length < arrayLength
    arrayLength = QueuedStopTimes.Length
  endif
  
  while i < arrayLength
    Actor actorRef = QueuedStopActors[i]
    if actorRef
      if currentTime >= QueuedStopTimes[i]
        if HasNippleSquirt(actorRef)
          Log("Timer expired for " + actorRef.GetDisplayName())
          StopNippleSquirt(actorRef)
        endif
        QueuedStopActors[i] = None
        QueuedStopTimes[i] = 0.0
        activeTimerCount -= 1
        
        ; If this was the last timer, schedule a cleanup
        if activeTimerCount == 0
          Log("Last timer ended, performing final cleanup")
          PerformCleanup()
        endif
      else
        needsQuickUpdate = true
      endif
    endif
    i += 1
  endwhile
  
  if needsQuickUpdate
    RegisterForSingleUpdate(0.3)  ; Fine-grained timing for staggered effects
  else
    RegisterForSingleUpdate(20.0)  ; Default cleanup interval
  endif
EndEvent

bool Function HasNippleSquirt(Actor actorRef)
  if !actorRef || !NippleSquirtArmor
    return false
  endif
  return actorRef.IsEquipped(NippleSquirtArmor)
EndFunction

;
; ██████╗ ██████╗ ██╗██╗   ██╗ █████╗ ████████╗███████╗     █████╗ ██████╗ ██╗
; ██╔══██╗██╔══██╗██║██║   ██║██╔══██╗╚══██╔══╝██╔════╝    ██╔══██╗██╔══██╗██║
; ██████╔╝██████╔╝██║██║   ██║███████║   ██║   █████╗      ███████║██████╔╝██║
; ██╔═══╝ ██╔══██╗██║╚██╗ ██╔╝██╔══██║   ██║   ██╔══╝      ██╔══██║██╔═══╝ ██║
; ██║     ██║  ██║██║ ╚████╔╝ ██║  ██║   ██║   ███████╗    ██║  ██║██║     ██║
; ╚═╝     ╚═╝  ╚═╝╚═╝  ╚═══╝  ╚═╝  ╚═╝   ╚═╝   ╚══════╝    ╚═╝  ╚═╝╚═╝     ╚═╝
;

Function StopNippleSquirtInternal(Actor actorRef, LactisNippleSquirtArmor armorRef)
  if !actorRef
    return
  endif
  
  string actorName = actorRef.GetDisplayName()
  Log("Stopping nipple squirt for " + actorName)
  
  ; First try to remove any equipped armor
  if actorRef.IsEquipped(NippleSquirtArmor)
    actorRef.UnequipItem(NippleSquirtArmor, false, true)
    Utility.Wait(0.05)
  endif

  ; Then remove the item, trying specific reference first
  if armorRef && actorRef.GetItemCount(armorRef) > 0
    Log("Removing specific armor reference")
    actorRef.RemoveItem(armorRef, 1, true)
  else
    ; Fallback to removing any NippleSquirtArmor
    int count = actorRef.GetItemCount(NippleSquirtArmor)
    if count > 0
      Log("No valid armor reference, removing " + count + " generic armors")
      actorRef.RemoveItem(NippleSquirtArmor, count, true)
    endif
  endif
  
  Int OnUnEquipp = ModEvent.Create("OLactis.Unequipped")
  ModEvent.PushForm(OnUnEquipp, actorRef as Form)
  ModEvent.Send(OnUnEquipp)
  Utility.Wait(0.2)
  
  ; Safe animation refresh
  if actorRef
    Debug.SendAnimationEvent(actorRef, "RefreshObject")
  endif
EndFunction

Function ForceStopNippleSquirt(Actor actorRef)
  if !actorRef
    return
  endif
  int i = 0
  bool found = false
  while i < QueuedStopActors.Length && !found
    if QueuedStopActors[i] == actorRef
      QueuedStopActors[i] = None
      QueuedStopTimes[i] = 0.0
      activeTimerCount -= 1
      found = true
    endif
    i += 1
  endwhile
  actorRef.RemoveItem(NippleSquirtArmor, 99, true)
  if HasArmorRefs(actorRef)
    RemoveArmorRefs(actorRef)
  endif
  actorRef.QueueNiNodeUpdate()
EndFunction


Function StopAllNippleSquirts()
  int i = 0
  int len = armorActors.Length
  Actor actorRef = None
  Log("Stopping all nipple squirts")
  while i < len
    actorRef = armorActors[i]
    if actorRef
      LactisNippleSquirtArmor actorArmor = GetArmorRefs(actorRef)
      StopNippleSquirtInternal(actorRef, actorArmor)
      RemoveArmorRefs(actorRef)
      Utility.Wait(0.1)
      actorRef.QueueNiNodeUpdate()
    endif
    i += 1
  endwhile
  FixStuckEffects()
  CleanupAllArrays()
EndFunction

; Updates the properties of the given armor object reference.
; Note that all parameters apply to the left and right armor and
; cannot be controlled individually.
Function UpdateArmorProperties(LactisNippleSquirtArmor armorRef, Float[] nippleOffset, float actorEmitterScale)
  if !armorRef || !nippleOffset
    return
  endif
  armorRef.NippleOffset = nippleOffset
  ; invert x-axis for the right side
  float[] nippleOffsetR = new float[3]
  nippleOffsetR[0] = -nippleOffset[0]
  nippleOffsetR[1] = nippleOffset[1]
  nippleOffsetR[2] = nippleOffset[2]
  armorRef.NippleOffsetR = nippleOffsetR
  armorRef.DebugAxisEnabled = DebugAxisEnabled
  armorRef.GlobalEmitterScale = GlobalEmitterScale
  armorRef.EmitterScale = actorEmitterScale
  armorRef.UpdateNodeProperties()
EndFunction

Function ApplyArmoredActorProperties()
  int i = 0
  Actor actorRef = None
  while i < armorActors.Length
    actorRef = armorActors[i]
    if actorRef != None
      LactisNippleSquirtArmor actorArmor = GetArmorRefs(actorRef)
      if actorArmor
        actorArmor.UpdateNodeProperties()
      endif
    endif
    i += 1
  endwhile
EndFunction

; The leak that goes with a squirt: 18 s, as the overlay always ran, cut short when
; the squirt stops. A leak another mod asked for is left as it is.
Function SquirtLeak(Actor actorRef, int level)
  if !HasOwnLeak(actorRef)
    StartMilkLeak(actorRef, 18.0, level)
  endif
EndFunction

; The milk leak: the fluid leak of OninusLactis.dll unless FluidLeakDisabled, and
; the old overlay when NippleLeakEnabled. seconds 0 = until stopped, level 0 to 2.
Function StartMilkLeak(Actor actorRef, float seconds, int level=0)
  if !actorRef || !actorRef.Is3DLoaded()
    return
  endif
  if !FluidLeakDisabled && LactisNative.Loaded()
    float intensity = 0.4 + 0.3 * level
    if intensity > 1.0
      intensity = 1.0
    elseif intensity < 0.4
      intensity = 0.4
    endif
    LactisNative.Leak(actorRef, intensity, seconds)
  endif
  if NippleLeakEnabled
    if seconds > 0.0
      StartNippleLeak(actorRef, Math.Ceiling(seconds))
    else
      StartNippleLeak(actorRef, -1) ; the effect shader's "until stopped"
    endif
  endif
EndFunction

Function NoteOwnLeak(Actor actorRef, int duration)
  if !ownLeakActors || !actorRef
    return
  endif
  int i = ownLeakActors.Find(actorRef)
  if i < 0
    i = ownLeakActors.Find(None)
  endif
  if i < 0
    return
  endif
  ownLeakActors[i] = actorRef
  if duration > 0
    ownLeakUntil[i] = Utility.GetCurrentRealTime() + duration
  else
    ownLeakUntil[i] = 0.0
  endif
EndFunction

Function ForgetOwnLeak(Actor actorRef)
  if !ownLeakActors || !actorRef
    return
  endif
  int i = ownLeakActors.Find(actorRef)
  if i >= 0
    ownLeakActors[i] = None
    ownLeakUntil[i] = 0.0
  endif
EndFunction

; True while a leak another mod asked for (OLactis.Leak) runs on the actor.
bool Function HasOwnLeak(Actor actorRef)
  if !ownLeakActors || !actorRef
    return false
  endif
  int i = ownLeakActors.Find(actorRef)
  if i < 0
    return false
  endif
  if ownLeakUntil[i] > 0.0 && Utility.GetCurrentRealTime() >= ownLeakUntil[i]
    ownLeakActors[i] = None
    ownLeakUntil[i] = 0.0
    return false
  endif
  return true
EndFunction

; The overlay leak (an effect shader drawn through the body's UVs; its texture is
; painted for the CBBE layout).
Function StartNippleLeak(Actor actorRef, int duration)
  if !actorRef || !LactisNippleLeakCBBE
    return
  endif
  LactisNippleLeakCBBE.play(actorRef, duration)
EndFunction

; Stops the overlay leak only, as it always did: other mods call this to put their
; own overlay in its place, and must not end the fluid leak by it.
Function StopNippleLeak(Actor actorRef)
  if !actorRef || !LactisNippleLeakCBBE
    return
  endif
  LactisNippleLeakCBBE.Stop(actorRef)
EndFunction

; Stops both kinds of leak. The fluid already on the skin runs its course and dries.
Function StopMilkLeak(Actor actorRef)
  if !actorRef
    return
  endif
  if LactisNative.Loaded()
    LactisNative.StopLeak(actorRef, false)
  endif
  if NippleLeakEnabled && actorRef.Is3DLoaded()
    StopNippleLeak(actorRef)
  endif
EndFunction

bool Function HasArmorRefs(Actor actorRef)
  if !actorRef
    return false
  endif
  int actorIndex = armorActors.Find(actorRef)
  if actorIndex >= 0
    return true
  Else
    return false
  endif
EndFunction

int Function StoreArmorRefs(Actor actorRef, LactisNippleSquirtArmor armorRefLeft)
  if !actorRef || !armorRefLeft
    Log("Error - Tried to store null actor or armor reference")
    return -1
  endif
  int firstFreeIndex = armorActors.Find(None)
  if firstFreeIndex>=0
    armorActors[firstFreeIndex] = actorRef
    armorRefsLeft[firstFreeIndex] = armorRefLeft
  else
    Log("Nipple squirt ArmorRef storage full!")
  endif
  return firstFreeIndex
EndFunction

LactisNippleSquirtArmor Function GetArmorRefs(Actor actorRef)
  if !actorRef
    return None
  endif
  int actorIndex = armorActors.Find(actorRef)
  if actorIndex >= 0
    return armorRefsLeft[actorIndex] as LactisNippleSquirtArmor
  endif
  ; we cannot return None explicitly here as this will result in a runtime cast error
  ; luckily returning nothing seems to actually return None :)
  return None
EndFunction

Function RemoveArmorRefs(Actor actorRef)
  if !actorRef
    return
  endif
  int i = 0
  while i < armorActors.Length
    if armorActors[i] == actorRef
      armorActors[i] = None
      armorRefsLeft[i] = None
      actorBusy[i] = false
      queuedEffectType[i] = 0
      queuedEffectDuration[i] = 0.0
      queuedEffectLevel[i] = 0
    endif
    i += 1
  endwhile
EndFunction

Function FixStuckEffects()
  int fixedCount = 0
  Cell playerCell = Game.GetPlayer().GetParentCell()
  if playerCell
    int actorCount = playerCell.GetNumRefs(43) ; 43 = Actor
    int i = 0
    while i < actorCount
      Actor cellActor = playerCell.GetNthRef(i, 43) as Actor
      if cellActor
        int idx = armorActors.Find(cellActor)
        if idx >= 0
          actorBusy[idx] = true
        endif
        int itemCount = cellActor.GetItemCount(NippleSquirtArmor)
        bool isEquipped = cellActor.IsEquipped(NippleSquirtArmor)
        bool isTracked = IsActorInArray(cellActor, armorActors)
        bool hasTimer = IsActorInArray(cellActor, QueuedStopActors)
        Log("Checking " + cellActor.GetDisplayName() + ": equipped=" + isEquipped + ", inv=" + itemCount + ", tracked=" + isTracked + ", timer=" + hasTimer)
        if (isEquipped || itemCount > 0)
          cellActor.UnequipItem(NippleSquirtArmor, false, true)
          cellActor.RemoveItem(NippleSquirtArmor, itemCount, true)
          RemoveArmorRefs(cellActor)
          ForceStopNippleSquirt(cellActor)
          cellActor.QueueNiNodeUpdate()
          Debug.SendAnimationEvent(cellActor, "RefreshObject")
          fixedCount += 1
          Log("Fixed stuck armor on " + cellActor.GetDisplayName())
        endif
        if idx >= 0
          actorBusy[idx] = false
          if queuedEffectType[idx] == 1
            Log("Processing queued PlayNippleSquirt for " + cellActor.GetDisplayName())
            float qDuration = queuedEffectDuration[idx]
            int qLevel = queuedEffectLevel[idx]
            queuedEffectType[idx] = 0
            PlayNippleSquirt(cellActor, qDuration, qLevel)
          elseif queuedEffectType[idx] == 2
            Log("Processing queued StopNippleSquirt for " + cellActor.GetDisplayName())
            queuedEffectType[idx] = 0
            StopNippleSquirt(cellActor)
          endif
        endif
      endif
      i += 1
    endwhile
  endif    
  if fixedCount > 0
    if fixedCount > 1
      Debug.Notification("Fixed " + fixedCount + " stuck nipple squirts")
    else
      Debug.Notification("Fixed " + fixedCount + " stuck nipple squirt")
    endif
  endif
EndFunction

bool Function IsActorInArray(Actor akActor, Actor[] akArray)
  int i = 0
  while i < akArray.Length
    if akArray[i] == akActor
      return true
    endif
    i += 1
  endwhile
  return false
EndFunction

Int Function GetArmoredActorsCount()
  int i = 0
  int len = armorActors.Length
  int count = 0
  Actor actorRef = None
  while i < len
    actorRef = armorActors[i]
    if actorRef && HasNippleSquirt(actorRef)
      count += 1
    endif
    i += 1
  endwhile
  return count
EndFunction

Function CleanupAllArrays()
  if !armorActors
    return
  endif
  int i = 0
  while i < armorActors.Length
    Actor actorRef = armorActors[i]
    if actorRef
      if !actorRef.IsEquipped(NippleSquirtArmor)
        armorActors[i] = None
        armorRefsLeft[i] = None
        actorBusy[i] = false
        queuedEffectType[i] = 0
        queuedEffectDuration[i] = 0.0
        queuedEffectLevel[i] = 0
        Log("Clearing tracking for " + actorRef.GetDisplayName() + " - no armor equipped")
      endif
    endif
    i += 1
  endwhile
  
  int activeCount = 0
  i = 0
  while i < QueuedStopActors.Length
    if QueuedStopActors[i] != None
      activeCount += 1
    endif
    i += 1
  endwhile
  activeTimerCount = activeCount
EndFunction

; ---------------------------- Utility functions

Event OnKeyDown(Int keyCode)
  ; https://www.creationkit.com/index.php?title=Input_Script#DXScanCodes
  if (Utility.IsInMenuMode() || UI.IsMenuOpen("console"))
    Return
  endif
  Log("Key pressed: " + keyCode)
  if (keyCode == StartLactatingKey)
    Log("Start lactating key pressed")
    ObjectReference crosshairObjRef = Game.GetCurrentCrosshairRef()
    Actor crosshairActor = crosshairObjRef as Actor
    Actor affectedActor = PlayerRef
    if crosshairActor != None
      affectedActor = crosshairActor
    endif
    ToggleNippleSquirt(affectedActor)
    Return ; Added return to prevent checking other keys
  endif
  ; Your other key handlers can stay below this...
  ; keycode 34 = G
  ; keycode 35 = H
  ; keycode 42 = left shift
  ; keycode 54 = right shift
EndEvent

Function RemapStartLactatingKey(Int zKey)
  Log("Remapping ToggleNippleSquirt from " + StartLactatingKey + " to "+ zKey)
  UnregisterForKey(StartLactatingKey)
  StartLactatingKey = zKey
  RegisterForKey(StartLactatingKey)
EndFunction

; Maps the specified value val from the interval defined by srcMin and
; srcMax to the interval defined by dstMin and dstMax.
; returns: The value mapped to the destination interval.
; param 'srcMin': The minimum value of the source interval.
; param 'srcMax': The maximum value of the source interval.
; param 'dstMin': The minimum value of the destination interval.
; param 'dstMax': The maximum value of the destination interval.
; param 'clamp': clamp values outside [dstMin..dstMax] or not.
float Function MapValue(float val, float srcMin, float srcMax, float dstMin, float dstMax, bool clamp) global
  if clamp
    if (val>=srcMax)
      return dstMax
    endif
    if (val<=srcMin)
      return dstMin
    endif
  endif
  return dstMin + (val-srcMin) / (srcMax-srcMin) * (dstMax-dstMin)
EndFunction

Function Uninstall()
  Debug.Notification("Uninstalling Oninus Lactis")
  UnregisterForUpdate()
  StopAllNippleSquirts()
  actorStorage.Clear()
  UnregisterForAllModEvents()
  UnregisterForAllKeys()
  Reset()
  Stop()
EndFunction

Event OnUnload()
  CleanupAllArrays()
EndEvent

LactisNippleSquirtArmor Function StartNippleSquirtLeft(Actor actorRef, int level=0)
  if !actorRef
    Log("Cannot start nipple squirt - actor is None")
    return None
  endif
  string actorName = actorRef.GetDisplayName()
  
  Log("Placing armor for " + actorName + " at level " + (level + 1))
  LactisNippleSquirtArmor armorRef = actorRef.PlaceAtMe(NippleSquirtArmor, 1, true) as LactisNippleSquirtArmor
  
  if !armorRef
    Log("Failed to create armor reference!")
    return None
  endif
  
  Log("Successfully created armor reference " + armorRef)
  armorRef.ActorRef = actorRef
  armorRef.SetLevel(level, false)
  
  if actorStorage.HasNpcStorage(actorRef)
    ; update npc armors
    Log("Using custom offset for " + actorName)
    UpdateArmorProperties(armorRef, actorStorage.GetNpcOffset(actorRef), actorStorage.GetNpcScale(actorRef))
  else
    ; update the player's armor
    Log("Using default offset for " + actorName)
    UpdateArmorProperties(armorRef, NippleOffsetL, EmitterScale)
  endif
  
  Log("Adding armor to " + actorName)
  actorRef.AddItem(armorRef, 1, true)
  
  return armorRef
EndFunction

float Function GetRandomDelay()
  int randomVariation = Utility.RandomInt(0, 100)
  float randomDelay = randomVariation * 0.003
  return randomDelay
EndFunction