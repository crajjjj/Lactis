#include "Squirt.h"

#include "Body.h"
#include "Settings.h"

namespace Lactis::Squirt
{
	namespace
	{
		constexpr const char* kPlugin = "OninusLactis.esp";
		constexpr std::array  kAddons{ RE::FormID{ 0xD6A }, RE::FormID{ 0xD6C } };  // LactisNippleSquirtLAA, ...RAA
		constexpr std::array  kGroups{ "Lactis Group L", "Lactis Group R" };
		constexpr std::array  kAxes{ "Lactis Axis L", "Lactis Axis R" };
		constexpr float       kAxisScale = 0.25f;
		constexpr float       kEvery = 0.5f;    // seconds between placements of one actor
		// Seconds with the actor's 3D loaded but no emitter nodes on it, after which the
		// actor is forgotten (the armor came off without a Forget). Time with no 3D at
		// all (out of range) does not count: the nodes come back with it.
		constexpr float kGiveUp = 8.0f;

		struct Emitters
		{
			RE::ActorHandle      actor;
			std::array<float, 3> offset{};
			float                scale{ 1.0f };
			bool                 axis{ false };
			float                next{ 0.0f };
			float                missing{ 0.0f };
			bool                 placed{ false };
			bool                 logged{ false };
			// The engine does not carry the emitters with the nipple: it is skinned to
			// another bone than the one they hang under (a soft body: physics moves the
			// breast's own net of bones, and the chain bone stands still), or physics
			// writes that bone's world transform and leaves its children behind
			// (HDT-SMP breasts). Placed every frame then, and kept so once seen.
			bool                 follow{ false };
			std::array<bool, Body::kEyes> covered{};  // as last placed, for the log
		};

		// Place and Forget in the order they were asked: an old armor's Forget that
		// came before the new armor's Place must not erase it.
		struct Request
		{
			bool     forget{ false };
			Emitters emitters;
		};

		enum class Found
		{
			kPlaced,
			kNoNodes,  // 3D loaded, no emitter groups on it
			kNo3D,     // not loaded: wait for it
			kGone      // the actor no longer exists
		};

		std::mutex                                 lock;
		std::vector<Request>                       requests;
		std::unordered_map<std::uint32_t, Emitters> placed;  // main thread only
		float                                      clock{ 0.0f };

		void Refresh(RE::NiAVObject* a_node)
		{
			RE::NiUpdateData data;
			a_node->Update(data);
		}

		Found PlaceNow(Emitters& a_emitters, const Settings& a_settings)
		{
			const auto actor = a_emitters.actor.get();
			if (!actor) {
				return Found::kGone;
			}
			auto* root = actor->Get3D(false);
			if (!root) {
				return Found::kNo3D;
			}
			bool any = false;
			for (std::size_t side = 0; side < Body::kEyes; ++side) {
				auto* group = root->GetObjectByName(kGroups[side]);
				auto* parent = group ? group->parent : nullptr;
				if (!group || !parent) {
					continue;
				}
				any = true;
				RE::NiPoint3 point;
				RE::NiPoint3 normal;
				RE::NiAVObject* bone = nullptr;
				const bool      found = a_settings.autoNipples && Body::Nipple(actor.get(), side, point, normal, &bone);
				if (found && bone) {
					// rides the nipple's bone (directly or through the squirt mesh's
					// own nodes): the engine carries it, no need to follow
					bool rides = false;
					for (const RE::NiAVObject* up = parent; up && !rides; up = up->parent) {
						rides = up == bone;
					}
					a_emitters.follow = a_emitters.follow || !rides || Body::Driven(bone);
				}
				if (found) {
					// Against where the engine's update will put the parent, not where
					// physics left it last frame: the update is what carries the group.
					group->local.translate = Body::Animated(parent).Invert() * (point + normal * (a_settings.lift * parent->world.scale));
				} else {
					group->local.translate = { side == Body::kLeft ? a_emitters.offset[0] : -a_emitters.offset[0], a_emitters.offset[1], a_emitters.offset[2] };
				}
				group->local.scale = a_emitters.scale;
				if (auto* axis = root->GetObjectByName(kAxes[side])) {
					axis->local.scale = a_emitters.axis ? kAxisScale : 0.0f;
				}
				// No squirt through clothes: the particle systems hang under the group
				// node, so culling it hides them until the nipple is bare again.
				const bool covered = a_settings.hideCovered && Body::Covered(actor.get(), side);
				// [squirt] enabled off hides emitters already worn; the script equips no new ones
				group->SetAppCulled(covered || !a_settings.squirtEnabled);
				Refresh(group);
				if (a_settings.debugLog && (!a_emitters.logged || covered != a_emitters.covered[side])) {
					const auto& at = group->local.translate;
					logger::info("{}: {} squirt emitter under \"{}\" at ({:.2f}, {:.2f}, {:.2f}), {}{}", Body::ActorName(actor.get()),
						side == Body::kLeft ? "left" : "right", parent->name.c_str() ? parent->name.c_str() : "?", at.x, at.y, at.z,
						found ? "on the nipple found" : (a_settings.autoNipples ? "at the MCM offset: no nipple found" : "at the MCM offset"),
						covered ? ", hidden: the nipple is covered" : "");
				}
				a_emitters.covered[side] = covered;
			}
			a_emitters.logged = a_emitters.logged || any;
			return any ? Found::kPlaced : Found::kNoNodes;
		}
	}

	void PatchRaces()
	{
		if (!Settings::Snapshot().allRaces) {
			return;
		}
		auto* data = RE::TESDataHandler::GetSingleton();
		if (!data) {
			return;
		}
		std::size_t added = 0;
		std::size_t addons = 0;
		for (const auto id : kAddons) {
			auto* addon = data->LookupForm<RE::TESObjectARMA>(id, kPlugin);
			if (!addon) {
				continue;
			}
			++addons;
			for (auto* race : data->GetFormArray<RE::TESRace>()) {
				if (!race || race == addon->race || !race->HasKeywordString("ActorTypeNPC") ||
					std::ranges::find(addon->additionalRaces, race) != addon->additionalRaces.end()) {
					continue;
				}
				addon->additionalRaces.push_back(race);
				++added;
			}
		}
		if (addons == 0) {
			logger::warn("squirt: the armor addons were not found in {}; races left as they are", kPlugin);
		} else {
			logger::info("squirt: {} race entries added to {} armor addon(s)", added, addons);
		}
	}

	void Place(RE::ActorHandle a_actor, const std::array<float, 3>& a_offset, float a_scale, bool a_axis)
	{
		if (!a_actor) {
			return;
		}
		Request request;
		request.emitters.actor = a_actor;
		request.emitters.offset = a_offset;
		// 0 hides the emitters, as the script's SetNodeScale did
		request.emitters.scale = std::isfinite(a_scale) ? std::max(a_scale, 0.0f) : 1.0f;
		request.emitters.axis = a_axis;
		std::scoped_lock guard(lock);
		requests.push_back(request);
	}

	void Forget(RE::ActorHandle a_actor)
	{
		if (!a_actor) {
			return;
		}
		Request request;
		request.forget = true;
		request.emitters.actor = a_actor;
		std::scoped_lock guard(lock);
		requests.push_back(request);
	}

	void Update(float a_delta)
	{
		clock += a_delta;
		{
			std::scoped_lock guard(lock);
			for (const auto& request : requests) {
				const auto key = request.emitters.actor.native_handle();
				if (request.forget) {
					placed.erase(key);
				} else {
					placed[key] = request.emitters;  // replaces an older request: place at once
				}
			}
			requests.clear();
		}
		if (placed.empty()) {
			return;
		}
		const Settings settings = Settings::Snapshot();
		for (auto it = placed.begin(); it != placed.end();) {
			auto& emitters = it->second;
			// Every frame until the meshes attach (the equip is not instant), then at an
			// easy pace: the nodes can come back under a new 3D, and a morph can move
			// the nipple.
			if (clock < emitters.next) {
				++it;
				continue;
			}
			switch (PlaceNow(emitters, settings)) {
			case Found::kPlaced:
				emitters.placed = true;
				emitters.missing = 0.0f;
				emitters.next = emitters.follow ? 0.0f : clock + kEvery;
				break;
			case Found::kNo3D:
				emitters.placed = false;
				emitters.next = clock + kEvery;
				break;
			case Found::kNoNodes:
				emitters.placed = false;
				emitters.missing += a_delta;  // looked for every frame from here
				emitters.next = 0.0f;
				if (emitters.missing > kGiveUp) {
					it = placed.erase(it);
					continue;
				}
				break;
			case Found::kGone:
				it = placed.erase(it);
				continue;
			}
			++it;
		}
	}

	void Rewind()
	{
		for (auto& [key, emitters] : placed) {
			emitters.placed = false;
			emitters.missing = 0.0f;
			emitters.next = 0.0f;
		}
	}

	void Reset()
	{
		std::scoped_lock guard(lock);
		requests.clear();
		placed.clear();
	}
}
