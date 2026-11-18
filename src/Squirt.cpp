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
		constexpr float       kGiveUp = 8.0f;   // seconds without the meshes after which the actor is forgotten

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
		};

		std::mutex                                 lock;
		std::vector<Emitters>                      pending;
		std::vector<std::uint32_t>                 dropped;
		std::unordered_map<std::uint32_t, Emitters> placed;  // main thread only
		float                                      clock{ 0.0f };

		void Refresh(RE::NiAVObject* a_node)
		{
			RE::NiUpdateData data;
			a_node->Update(data);
		}

		// True when at least one emitter group was there to place.
		bool PlaceNow(Emitters& a_emitters, const Settings& a_settings)
		{
			const auto actor = a_emitters.actor.get();
			auto*      root = actor ? actor->Get3D(false) : nullptr;
			if (!root) {
				return false;
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
				const bool   found = a_settings.autoNipples && Body::Nipple(actor.get(), side, point, normal);
				if (found) {
					group->local.translate = parent->world.Invert() * (point + normal * (a_settings.lift * parent->world.scale));
				} else {
					group->local.translate = { side == Body::kLeft ? a_emitters.offset[0] : -a_emitters.offset[0], a_emitters.offset[1], a_emitters.offset[2] };
				}
				group->local.scale = a_emitters.scale;
				if (auto* axis = root->GetObjectByName(kAxes[side])) {
					axis->local.scale = a_emitters.axis ? kAxisScale : 0.0f;
				}
				Refresh(group);
				if (!a_emitters.logged && a_settings.debugLog) {
					const auto& at = group->local.translate;
					logger::info("{}: {} squirt emitter under \"{}\" at ({:.2f}, {:.2f}, {:.2f}), {}", actor->GetDisplayFullName(),
						side == Body::kLeft ? "left" : "right", parent->name.c_str() ? parent->name.c_str() : "?", at.x, at.y, at.z,
						found ? "on the nipple found" : (a_settings.autoNipples ? "at the MCM offset: no nipple found" : "at the MCM offset"));
				}
			}
			a_emitters.logged = a_emitters.logged || any;
			return any;
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
		Emitters emitters;
		emitters.actor = a_actor;
		emitters.offset = a_offset;
		emitters.scale = std::isfinite(a_scale) && a_scale > 0.0f ? a_scale : 1.0f;
		emitters.axis = a_axis;
		std::scoped_lock guard(lock);
		pending.push_back(emitters);
	}

	void Forget(RE::ActorHandle a_actor)
	{
		if (!a_actor) {
			return;
		}
		std::scoped_lock guard(lock);
		dropped.push_back(a_actor.native_handle());
	}

	void Update(float a_delta)
	{
		clock += a_delta;
		{
			std::scoped_lock guard(lock);
			for (auto& emitters : pending) {
				placed[emitters.actor.native_handle()] = emitters;  // replaces an older request: place at once
			}
			pending.clear();
			for (const auto key : dropped) {
				placed.erase(key);
			}
			dropped.clear();
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
			if (emitters.placed && clock < emitters.next) {
				++it;
				continue;
			}
			if (PlaceNow(emitters, settings)) {
				emitters.placed = true;
				emitters.missing = 0.0f;
				emitters.next = clock + kEvery;
			} else {
				emitters.placed = false;
				emitters.missing += a_delta;
				if (emitters.missing > kGiveUp) {
					it = placed.erase(it);
					continue;
				}
			}
			++it;
		}
	}

	void Reset()
	{
		std::scoped_lock guard(lock);
		pending.clear();
		dropped.clear();
		placed.clear();
	}
}
