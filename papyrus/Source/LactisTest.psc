Scriptname LactisTest Hidden
{Console harness for Oninus Lactis NG, exposed through ConsoleUtil Extended by
SKSE\CustomConsole\OninusLactis.yaml: "lactis leak", "lactis status" and so on.
Commands act on the console-selected actor, or on the player when none is selected.
Returned strings print to the console; they must never contain a percent sign.}

Function NearbyNative(Float afRadius, Float afIntensity, Float afSeconds) global native
Function ShowNipplesNative(Bool abShow) global native
Function MountWorldNative(Bool abWorld) global native
Function AutoNipplesNative(Bool abAuto) global native
Function WipeAllNative() global native
Int Function LeakingCountNative() global native

Actor Function Target() global
	Actor target = Game.GetCurrentConsoleRef() as Actor
	If !target
		target = Game.GetPlayer()
	EndIf
	Return target
EndFunction

String Function Who() global
	If Game.GetCurrentConsoleRef() as Actor
		Return "The selected actor"
	EndIf
	Return "The player"
EndFunction

OninusLactis Function Main() global
	Return Game.GetFormFromFile(0xD61, "OninusLactis.esp") as OninusLactis
EndFunction

; Params in YAML order: intensity, seconds
String Function Leak(Float intensity, Float seconds) global
	If !LactisNative.Loaded()
		Return "OninusLactis.dll is not loaded."
	EndIf
	If !LactisNative.IsLeakEnabled()
		Return "The milk leak is off; turn it on with: lactis enable 1"
	EndIf
	LactisNative.Leak(Target(), intensity, seconds)
	If seconds > 0.0
		Return Who() + " leaks at intensity " + intensity + " for " + seconds + " s."
	EndIf
	Return Who() + " leaks at intensity " + intensity + " until stopped."
EndFunction

; Params in YAML order: wipe
String Function Stop(Int wipe) global
	LactisNative.StopLeak(Target(), wipe != 0)
	If wipe != 0
		Return Who() + " stops leaking; milk wiped."
	EndIf
	Return Who() + " stops leaking; the milk on the skin dries."
EndFunction

; Params in YAML order: side
String Function Drip(Int side) global
	LactisNative.Drip(Target(), side)
	Return "A drop."
EndFunction

String Function Wipe() global
	LactisNative.WipeMilk(Target())
	Return Who() + ": milk wiped."
EndFunction

String Function WipeAll() global
	WipeAllNative()
	Return "Milk wiped off everyone."
EndFunction

; Params in YAML order: radius, intensity, seconds
String Function Nearby(Float radius, Float intensity, Float seconds) global
	NearbyNative(radius, intensity, seconds)
	Return "Actors within " + radius + " units leak; see OninusLactis.log for the count."
EndFunction

String Function Status() global
	If !LactisNative.Loaded()
		Return "OninusLactis.dll is not loaded."
	EndIf
	Return LactisNative.GetStatus(Target()) + " " + LeakingCountNative() + " actor(s) leaking."
EndFunction

; Params in YAML order: on
String Function Enable(Int on) global
	LactisNative.SetLeakEnabled(on != 0)
	If on != 0
		Return "Milk leak on (this session; the SKSE menu saves it)."
	EndIf
	Return "Milk leak off (this session; the SKSE menu saves it)."
EndFunction

; Params in YAML order: on
String Function Nipples(Int on) global
	ShowNipplesNative(on != 0)
	If on != 0
		Return "A bead marks each nipple found on " + Who() + "."
	EndIf
	Return "Nipple markers off."
EndFunction

; Params in YAML order: on
String Function AutoNipples(Int on) global
	AutoNipplesNative(on != 0)
	If on != 0
		Return "Squirt emitters go to the nipples found (this session)."
	EndIf
	Return "Squirt emitters use the MCM offsets (this session)."
EndFunction

; Params in YAML order: world
String Function Mount(Int world) global
	MountWorldNative(world != 0)
	If world != 0
		Return "Milk is drawn in world space under the cell node."
	EndIf
	Return "Milk hangs under each breast's bone."
EndFunction

String Function Reload() global
	LactisNative.ReloadConfig()
	Return "OninusLactis.toml read again."
EndFunction

; Params in YAML order: seconds, level
String Function Squirt(Float seconds, Int level) global
	OninusLactis lactis = Main()
	If !lactis
		Return "OninusLactis.esp is not loaded."
	EndIf
	lactis.PlayNippleSquirt(Target(), seconds, level)
	Return Who() + " squirts for " + seconds + " s at level " + level + "."
EndFunction

String Function StopSquirt() global
	OninusLactis lactis = Main()
	If !lactis
		Return "OninusLactis.esp is not loaded."
	EndIf
	lactis.StopNippleSquirt(Target())
	Return Who() + ": squirt stopped."
EndFunction
