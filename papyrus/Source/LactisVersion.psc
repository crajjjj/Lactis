Scriptname LactisVersion Hidden
{The mod version, in one place. Keep it equal to PROJECT_VERSION in xmake.lua.}

; major * 10000 + minor * 100 + patch (2.3.0 = 20300). The MCM's GetVersion() returns
; this, so a bump here runs the MCM's OnVersionUpdate on existing saves. 2.1's MCM
; registered 2, which every packed version clears.
int Function GetVersion() global
  return 20302
EndFunction

string Function GetVersionString() global
  return "2.3.2"
EndFunction
