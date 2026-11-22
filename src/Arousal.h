#pragma once

namespace Lactis::Arousal
{
	// The leak that arousal starts ([leak] arousal): while an actor's arousal is at
	// the threshold or above, the nipples leak, heavier the further above it. The
	// player always, female NPCs nearby with [leak] arousal_npcs.
	//
	// Arousal comes from SLO Aroused NG's C API when that DLL is loaded, else from
	// the rank in the sla_Arousal faction of SexLabAroused.esm, which every edition
	// of SexLab Aroused keeps. With neither, the setting does nothing.

	// Looks for the arousal source. Call at kDataLoaded.
	void Connect();

	// Main thread, once per frame.
	void Update(float a_delta);

	// Forgets which leaks are its own (load, new game, main menu).
	void Reset();
}
