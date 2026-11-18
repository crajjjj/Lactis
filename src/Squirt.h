#pragma once

namespace Lactis::Squirt
{
	// The nipple squirt is a pair of particle meshes the script equips as an armor
	// (OninusLactis.esp); each hangs under a breast bone with a group node the emitters
	// sit in. This module puts those group nodes on the nipples Body finds, keeps them
	// there (a reloaded 3D, a morph that moved the nipple), and lets every humanoid
	// race wear the armor.

	// Adds every humanoid race to the squirt's armor addons, so a custom body race
	// (UBE and the like) needs no patch plugin. Call at kDataLoaded.
	void PatchRaces();

	// Asks for a_actor's emitters to be placed, and kept placed while they exist
	// (again after the actor's 3D unloads and comes back).
	// a_offset is the left emitter's place in its bone's space (x mirrored for the
	// right), used when the nipples are not to be found ([squirt] auto_nipples off, or
	// nothing on the actor answers); a_scale the emitters' size; a_axis shows the debug
	// axis. Safe from any thread: the nodes are touched on the main thread, and a mesh
	// that has not attached yet is waited for.
	void Place(RE::ActorHandle a_actor, const std::array<float, 3>& a_offset, float a_scale, bool a_axis);

	// Stops keeping a_actor's emitters placed (the armor came off). Applied in order
	// with Place: a Place asked after it stands.
	void Forget(RE::ActorHandle a_actor);

	// Main thread, once per frame, after Body::BeginFrame.
	void Update(float a_delta);

	// Main thread: places every kept actor again from scratch (the bodies were
	// dropped, as after RaceMenu), keeping the requests.
	void Rewind();

	// Drops every request (load, new game, main menu).
	void Reset();
}
