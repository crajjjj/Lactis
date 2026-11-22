Scriptname LactisNative Hidden
{The native half of Oninus Lactis NG (OninusLactis.dll): the milk leak and the
nipple search.

The milk leak is fluid: drops well up at the nipples, cling, run down the skin (or
the clothes over it) and drip off, on any body the actor wears. Every function is
global: LactisNative.Leak(akActor, 0.7, 20.0).

A mod that wants no compile-time dependency sends the mod events of OninusLactis.psc
instead (OLactis.Leak and the rest); those end up here.

The DLL sends two mod events with the leaking actor as the sender:
  OLactis.LeakStart   numArg = intensity
  OLactis.LeakStop
A leak does not survive loading a save, and no OLactis.LeakStop is sent then.}

; Bumped whenever a function is added. 0 means OninusLactis.dll did not load.
Int Function GetAPIVersion() global native

; Makes akActor's nipples leak. afIntensity is 0.0 to 1.0: how often and how heavily
; they drip; 0.0 stops the leak. afSeconds <= 0 leaks until StopLeak. Calling Leak
; on an actor who already leaks replaces the intensity and the time left. The actor
; needs loaded 3D; one who has none is ignored.
Function Leak(Actor akActor, Float afIntensity = 0.6, Float afSeconds = 20.0) global native

; Stops new drops. The milk already on the skin runs its course and dries, unless
; abWipe is true, which removes it at once.
Function StopLeak(Actor akActor, Bool abWipe = false) global native

; Lets go of a single drop, also from an actor who is not leaking.
; aiSide: 0 left, 1 right, 2 both, -1 one at random.
Function Drip(Actor akActor, Int aiSide = -1) global native

; Removes the milk on the actor at once, without changing whether the actor leaks.
Function WipeMilk(Actor akActor) global native

Bool Function IsLeaking(Actor akActor) global native

; The intensity Leak set; 0.0 when not leaking.
Float Function GetLeakIntensity(Actor akActor) global native

; Seconds of leaking left: -1.0 = until StopLeak, 0.0 = not leaking.
Float Function GetLeakTimeLeft(Actor akActor) global native

Bool Function IsLeakEnabled() global native

; Turns the fluid leak on or off. Off removes every drop at once and ignores new
; requests. Lasts for this session only; the checkbox in the SKSE menu is the saved
; setting.
Function SetLeakEnabled(Bool abEnabled) global native

; Reads OninusLactis.toml and OninusLactis_User.toml again.
Function ReloadConfig() global native

; One line about the actor's milk and what was found on their body (which mesh and
; bone gave each nipple), for debugging. The first call for an actor may answer that
; it is still reading the actor; ask again a moment later.
String Function GetStatus(Actor akActor) global native

; The squirt emitters (the particle meshes of the equipped Nipple Squirt armor).
; Puts akActor's emitters on the nipples found on the actor's meshes and keeps them
; there while the armor is on: the meshes are waited for if they have not attached
; yet, and placed again after the actor's 3D reloads. afX, afY, afZ: where the left
; emitter goes in its bone's space when the nipples are not to be found (the search
; is off, or nothing on the actor answers); the right one mirrors afX. afScale: the
; emitters' size. abAxis: show the debug axis.
Function PlaceEmitters(Actor akActor, Float afX, Float afY, Float afZ, Float afScale, Bool abAxis) global native

; Stops keeping akActor's emitters placed. Call when the armor comes off.
Function ForgetEmitters(Actor akActor) global native

; Whether the nipple search is on ([squirt] auto_nipples): the emitters go to the
; nipples found, and the offsets are only the fallback.
Bool Function FindsNipples() global native

; API 2. Whether the particle squirt is on ([squirt] enabled, the SKSE menu's Squirt
; page). Off, the scripts equip no emitters and a squirt request gives its leak only.
Bool Function IsSquirtEnabled() global native

; True when OninusLactis.dll is loaded. The scripts fall back to the old behaviour
; (fixed offsets, the overlay leak only) without it.
Bool Function Loaded() global
	Return SKSE.GetPluginVersion("OninusLactis") != -1
EndFunction
