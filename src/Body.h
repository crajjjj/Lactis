#pragma once

namespace Lactis::Body
{
	// A point on the skin (or on what is worn over it), posed this frame, in world units.
	struct Patch
	{
		RE::NiPoint3 position;
		RE::NiPoint3 normal;
	};

	// Where one breast leaks: the nipple tip, the surface's outward normal there, and
	// "down the skin" (gravity along the surface). The fluid code inherited from
	// Lacrima calls this a duct.
	struct Duct
	{
		RE::NiPoint3 point;
		RE::NiPoint3 normal;
		RE::NiPoint3 down;
		bool         valid{ false };
	};

	// The two sides. The names are the fluid code's (it was written for eyes); kLeft
	// is the actor's left breast.
	enum Eye : std::size_t
	{
		kLeft,
		kRight,
		kEyes
	};

	// One start point per breast. The corner slots and the lid rows exist only so the
	// fluid code compiles unchanged; they are never valid here.
	constexpr std::size_t kMaxStreams = 1;
	constexpr std::size_t kInnerCorner = kMaxStreams;
	constexpr std::size_t kOuterCorner = kMaxStreams + 1;
	constexpr std::size_t kMaxDucts = kMaxStreams + 2;
	using Ducts = std::array<std::array<Duct, kMaxDucts>, kEyes>;

	constexpr std::size_t kLidPoints = 8;
	constexpr std::size_t kLidRows = 3;
	using Lids = std::array<std::array<std::array<Duct, kLidPoints>, kLidRows>, kEyes>;

	// A box in world units: where the fluid is this frame.
	struct Region
	{
		RE::NiPoint3 low;
		RE::NiPoint3 high;
	};

	struct View
	{
		std::span<const Patch> patches;
		Ducts                  ducts;  // [side][0] is the nipple
		float                  scale{ 1.0f };
		bool                   morphing{ false };  // either side's skin moved
		Lids                   lids;               // never valid
		// The bone each side's fluid lives in: the breast bone the nipple is skinned to,
		// so a drop at rest on the breast is carried by the engine, physics included.
		std::array<RE::NiAVObject*, kEyes> bones{};
		// The skin under a side's fluid moved against that side's bone since last frame
		// (a bending torso, the other bones of a bouncing breast): fluid resting there
		// must be stirred.
		std::array<bool, kEyes> moving{};
	};

	// Call once per frame before Of().
	void BeginFrame(float a_delta);

	// Kept for the fluid code; nothing in it places anything here.
	struct Placement
	{
		int streams{ 1 };

		bool operator==(const Placement&) const = default;
	};

	// The actor's torso, posed for this frame; nullopt when the actor has no loaded 3D
	// or no mesh Lactis can read. Only the skin that can fall inside a_region, plus the
	// skin around the nipples, is posed; null poses the nipples' surroundings only.
	// a_watch: per side, the box around that side's fluid, tested for skin moving
	// against the side's bone (View::moving). Main thread only.
	std::optional<View> Of(RE::Actor* a_actor, const Placement& a_placement, const Region* a_region,
		const std::array<const Region*, kEyes>& a_watch = {});

	// One nipple, posed now: its world position and normal, and the bone it rides on.
	// False when that side was not found. Main thread only.
	bool Nipple(RE::Actor* a_actor, std::size_t a_side, RE::NiPoint3& a_point, RE::NiPoint3& a_normal, RE::NiAVObject** a_bone = nullptr);

	// The bone each side's fluid lives in (null: not found), without posing anything.
	std::array<RE::NiAVObject*, kEyes> Bones(RE::Actor* a_actor);

	// One line about what was found on the actor, for the status command.
	std::string Describe(RE::Actor* a_actor, const Placement& a_placement);

	// The bone the fluid falls back to when a side has no breast bone.
	constexpr const char* kHeadBone = "NPC Spine2 [Spn2]";

	// The name of the node every milk mesh hangs under, so the body reader can leave
	// Lactis's own meshes out of the surfaces it reads.
	constexpr const char* kMilkRoot = "LactisMilk";

	void Reset();
}
