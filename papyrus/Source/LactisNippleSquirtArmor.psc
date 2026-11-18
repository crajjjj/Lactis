;
; ███████╗ ██████╗ ██╗   ██╗██╗██████╗ ████████╗     █████╗ ██████╗ ███╗   ███╗ ██████╗ ██████╗ 
; ██╔════╝██╔═══██╗██║   ██║██║██╔══██╗╚══██╔══╝    ██╔══██╗██╔══██╗████╗ ████║██╔═══██╗██╔══██╗
; ███████╗██║   ██║██║   ██║██║██████╔╝   ██║       ███████║██████╔╝██╔████╔██║██║   ██║██████╔╝
; ╚════██║██║▄▄ ██║██║   ██║██║██╔══██╗   ██║       ██╔══██║██╔══██╗██║╚██╔╝██║██║   ██║██╔══██╗
; ███████║╚██████╔╝╚██████╔╝██║██║  ██║   ██║       ██║  ██║██║  ██║██║ ╚═╝ ██║╚██████╔╝██║  ██║
; ╚══════╝ ╚══▀▀═╝  ╚═════╝ ╚═╝╚═╝  ╚═╝   ╚═╝       ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝ ╚═════╝ ╚═╝  ╚═╝
                                                                                              
Scriptname LactisNippleSquirtArmor extends ObjectReference

OninusLactis Property QuestScript Auto
 
Actor Property ActorRef Auto
Float[] Property NippleOffset Auto
Float[] Property NippleOffsetR Auto
Float Property GlobalEmitterScale Auto
Float Property EmitterScale Auto
Bool Property DebugAxisEnabled Auto

String[] LevelNifsL
String[] LevelNifsR

String LactisGroupNameL = "Lactis Group L"
String LactisGroupNameR = "Lactis Group R"

String LactisAxisNameL = "Lactis Axis L"
String LactisAxisNameR = "Lactis Axis R"

String LactisEmitter1NameL = "Lactis Emitter Left 1"
String LactisEmitter1NameR = "Lactis Emitter Right 1"

String LactisEmitter2NameL = "Lactis Emitter Left 2"
String LactisEmitter2NameR = "Lactis Emitter Right 2"

String LactisEmitter3NameL = "Lactis Emitter Left 3"
String LactisEmitter3NameR = "Lactis Emitter Right 3"

Float[] rot 
Form baseObject
ArmorAddon armorAA
ArmorAddon armorAAR
int currentLevel

; Helper function for logging that can be toggled in MCM
Function Log(string a_message)
  if QuestScript && QuestScript.DebugLoggingEnabled
    Debug.Trace("OninusLactis LactisNippleSquirtArmor: " + a_message)
  endif
EndFunction

; Helper function for critical errors that should always be logged
Function LogError(string a_message)
  Debug.Trace("OninusLactis LactisNippleSquirtArmor ERROR: " + a_message)
EndFunction

Event OnInit()    
  ; Initialize arrays
  rot = new Float[3]
  LevelNifsL = new String[3]    
  LevelNifsL[0] = "OninusLactis/nipplesquirt-left-lvl1.nif"
  LevelNifsL[1] = "OninusLactis/nipplesquirt-left-lvl2.nif"
  LevelNifsL[2] = "OninusLactis/nipplesquirt-left-lvl3.nif"
  LevelNifsR = new String[3]
  LevelNifsR[0] = "OninusLactis/nipplesquirt-right-lvl1.nif"
  LevelNifsR[1] = "OninusLactis/nipplesquirt-right-lvl2.nif"
  LevelNifsR[2] = "OninusLactis/nipplesquirt-right-lvl3.nif"
  currentLevel = 0
EndEvent

int Function GetLevel()
  return currentLevel
EndFunction

Function SetLevel(int index, bool doEquip=true)
  ; Initialize if not already done
  if !baseObject || !armorAA || !armorAAR
    baseObject = self.GetBaseObject()
    armorAA = (baseObject as Armor).GetNthArmorAddon(0)
    armorAAR = (baseObject as Armor).GetNthArmorAddon(1)
  endif

  ; Check if initialization was successful
  if !armorAA || !armorAAR
    LogError("armorAA or armorAAR is None")
    return
  endif

  ; Validate and clamp index
  if index > 2
    index = 2
  elseif index < 0
    index = 0
  endif

  ; The model path sits on the armor addon, which every actor's squirt shares, so
  ; another actor's level may be on it now: always write this one's before it is
  ; equipped. (Comparing with this reference's own last level left a level 1 request
  ; playing the level 3 mesh another actor had set.)
  if true
    string leftNifPath = LevelNifsL[index]
    string rightNifPath = LevelNifsR[index]
    
    if leftNifPath && rightNifPath
      armorAA.SetModelPath(leftNifPath, false, true)
      armorAAR.SetModelPath(rightNifPath, false, true)
      currentLevel = index
      
      if doEquip
        ; Force NIF update through unequip/equip cycle
        ActorRef.UnequipItem(baseObject, true, true)
        Utility.Wait(0.05)
        ActorRef.EquipItem(baseObject, true, true)
        Utility.Wait(0.05)
        UpdateNodeProperties()
      endif
    else
      LogError("Invalid NIF paths for level " + index)
    endif
  endif
EndFunction

Event OnContainerChanged(ObjectReference akNewContainer, ObjectReference akOldContainer)        
  if akNewContainer == ActorRef
    if !baseObject
      baseObject = self.GetBaseObject()
      armorAA = (baseObject as Armor).GetNthArmorAddon(0)
      armorAAR = (baseObject as Armor).GetNthArmorAddon(1)
    endif
    
    ; Only equip if not already equipped
    if !ActorRef.IsEquipped(baseObject)
      ActorRef.EquipItem(baseObject, true, true)
      Utility.Wait(0.05)
    endif

    ; Update properties after ensuring it's equipped
    UpdateNodeProperties()
    Utility.Wait(0.05)
  endif
EndEvent

Event OnUnequipped(Actor akActor)
  ; Send event to main script for tracking
  Int OnUnEquipp = ModEvent.Create("OLactis.Unequipped")
  ModEvent.PushForm(OnUnEquipp, akActor as Form)
  ModEvent.Send(OnUnEquipp)
  if LactisNative.Loaded()
    LactisNative.ForgetEmitters(akActor)
  endif

  ; Clean up our local state
  ActorRef = None
  baseObject = None
  armorAA = None
  armorAAR = None
  currentLevel = 0
EndEvent

; Updates armor node position, scale and the debug axis. Changes it's scale 
; depending on whether it is enabled in MCM or not. If disabled, scale will be
; set to 0 to make the axis invisible.
Function UpdateNodeProperties()
  if !ActorRef || !NippleOffset
    return
  endif
  ; With OninusLactis.dll the emitters are placed natively: on the nipples found on
  ; the actor's meshes (the offset is only the fallback), once the meshes have
  ; attached, and again whenever the actor's 3D reloads. Calling it before the armor
  ; is equipped is fine: the DLL waits for the nodes.
  if LactisNative.Loaded()
    LactisNative.PlaceEmitters(ActorRef, NippleOffset[0], NippleOffset[1], NippleOffset[2], GlobalEmitterScale*EmitterScale, DebugAxisEnabled)
    return
  endif
	if (DebugAxisEnabled==true)
		NetImmerse.SetNodeScale(ActorRef, LactisAxisNameL, 0.25, false)
    NetImmerse.SetNodeScale(ActorRef, LactisAxisNameR, 0.25, false)
	else
		NetImmerse.SetNodeScale(ActorRef, LactisAxisNameL, 0, false)
    NetImmerse.SetNodeScale(ActorRef, LactisAxisNameR, 0, false)
	endif
  NetImmerse.SetNodeLocalPosition(ActorRef, LactisGroupNameL, NippleOffset, false)
  NetImmerse.SetNodeLocalPosition(ActorRef, LactisGroupNameR, NippleOffsetR, false)
  float totalScale = GlobalEmitterScale*EmitterScale
  NetImmerse.SetNodeScale(ActorRef, LactisGroupNameL, totalScale, false)    
  NetImmerse.SetNodeScale(ActorRef, LactisGroupNameR, totalScale, false)
EndFunction