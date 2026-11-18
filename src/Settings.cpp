#include "Settings.h"

#include <fstream>

#include <toml++/toml.hpp>

namespace Lactis
{
	namespace
	{
		constexpr auto kBaseFile = "Data/SKSE/Plugins/OninusLactis.toml";
		constexpr auto kUserFile = "Data/SKSE/Plugins/OninusLactis_User.toml";

		std::recursive_mutex g_lock;
		Settings             g_settings;
		Settings             g_persisted;
		Settings             g_defaults;

		// clang-format off
		const std::array kFields{
			Field{ "general", "enabled", &Settings::enabled, 0, 1, "Test", "Milk leak enabled", nullptr,
				"Off removes every drop at once and ignores new requests. The squirt emitters are not affected." },
			Field{ "general", "debug_log", &Settings::debugLog, 0, 1, "Test", "Detailed log", nullptr,
				"Writes mesh, nipple and fluid details to OninusLactis.log, and the status line (what lactis status prints) when a leak starts and every 10 s while it lasts." },

			Field{ "leak", "rate", &Settings::rate, 0.5f, 60.0f, "Leak", "Drops per minute", "%.1f",
				"How many drops each nipple lets go per minute at full intensity. Lower intensity leaks fewer." },
			Field{ "leak", "drop", &Settings::drop, 2, 20, "Leak", "Drop size", "%d",
				"Fluid particles in one drop at full intensity. More makes a heavier drop that runs further." },
			Field{ "leak", "well", &Settings::well, 0.1f, 4.0f, "Leak", "Welling time", "%.2f s",
				"Seconds a drop takes to gather at the nipple." },
			Field{ "leak", "hold", &Settings::hold, 0.0f, 4.0f, "Leak", "Cling time", "%.2f s",
				"Seconds a full drop clings to the nipple before it runs." },
			Field{ "leak", "variety", &Settings::variety, 0.0f, 1.0f, "Leak", "Variety", "%.2f",
				"How much one drop differs from the next: where around the nipple it starts, how big it is, and whether another follows right after. 0 makes every drop alike." },
			Field{ "leak", "both_breasts", &Settings::bothEyes, 0, 1, "Leak", "Both breasts", nullptr,
				"Off leaks from one breast only, picked at random each time a leak starts." },
			Field{ "leak", "max_actors", &Settings::maxActors, 1, 20, "Leak", "Actors at once", "%d",
				"How many actors can leak at the same time. The oldest stops when a new one starts past the limit." },
			Field{ "leak", "max_particles", &Settings::maxParticles, 20, 600, "Leak", "Particles per breast", "%d",
				"Cap on the fluid on one breast. Older trails stop growing past it." },
			Field{ "leak", "max_distance", &Settings::maxDistance, 500.0f, 20000.0f, "Leak", "Simulate within", "%.0f units",
				"Milk on an actor farther than this from the camera pauses where it is until the actor comes closer; the trails still dry up." },

			Field{ "look", "style", &Settings::style, 0, 2, "Look", "Look style", "%d",
				"1 plain: lit like any surface, no glow and no reflection. 0 reflective: wet, with a reflection of the surroundings. "
				"2 refraction: plain, and the scene behind bends through the fluid (experimental)." },
			Field{ "look", "refraction", &Settings::refraction, 0.0f, 1.0f, "Look", "Refraction", "%.2f",
				"In the refraction style, how strongly the scene behind bends through a drop." },
			Field{ "look", "size", &Settings::size, 0.5f, 3.0f, "Look", "Drop size", "%.2f",
				"Scales every drop. Smaller is subtler, bigger reads better from a distance." },
			Field{ "look", "trail", &Settings::trail, 0.2f, 2.0f, "Look", "Trail width", "%.2f",
				"Thickness of the trail a drop leaves on the skin." },
			Field{ "look", "opacity", &Settings::opacity, 0.01f, 1.0f, "Look", "Opacity", "%.2f",
				"How much the milk hides the skin under it. Milk is nearly opaque; lower it for a thinner, watery look." },
			Field{ "look", "shine", &Settings::shine, 0.0f, 10.0f, "Look", "Shine", "%.2f",
				"Strength of the wet highlight." },
			Field{ "look", "gloss", &Settings::gloss, 1.0f, 1000.0f, "Look", "Gloss", "%.0f",
				"Sharpness of the highlight. High is a small bright glint, low a broad sheen." },
			Field{ "look", "reflection", &Settings::reflection, 0.0f, 3.0f, "Look", "Reflection", "%.2f",
				"In the reflective style, how much of the surroundings the fluid reflects. 0 is matte." },
			Field{ "look", "env_light_fade", &Settings::envLightFade, 0, 1, "Look", "Reflection follows the light", nullptr,
				"The reflection is dimmed with the light falling on the body, so milk does not shine in a dark room." },
			Field{ "look", "glow", &Settings::glow, 0.0f, 1.0f, "Look", "Glow", "%.2f",
				"A light of the fluid's own (reflective style). 0 is none; milk needs none." },
			Field{ "look", "lifetime", &Settings::lifetime, 3.0f, 300.0f, "Look", "Trail lifetime", "%.0f s",
				"Seconds a trail stays on the skin." },
			Field{ "look", "fade", &Settings::fade, 0.2f, 30.0f, "Look", "Fade", "%.1f s",
				"Seconds at the end of its life a trail spends thinning out." },

			Field{ "physics", "stickiness", &Settings::stickiness, 0.0f, 3.0f, "Physics", "Stickiness", "%.2f",
				"How firmly a slow drop sticks to the skin and stops." },
			Field{ "physics", "friction", &Settings::friction, 1.0f, 200.0f, "Physics", "Skin friction", "%.0f",
				"Drag on a drop running over skin. Higher runs slower." },
			Field{ "physics", "viscosity", &Settings::viscosity, 0.0f, 100.0f, "Physics", "Viscosity", "%.0f",
				"How much a drop moves as one. Milk is thicker than water." },
			Field{ "physics", "cohesion", &Settings::cohesion, 0.0f, 20.0f, "Physics", "Cohesion", "%.2f",
				"How strongly a drop holds together as a bead." },
			Field{ "physics", "stretch", &Settings::stretch, 0.0f, 30.0f, "Physics", "Trail stretch", "%.1f",
				"How easily a running drop stretches into a trail before it breaks." },
			Field{ "physics", "trail_step", &Settings::trailStep, 0.3f, 5.0f, "Physics", "Trail step", "%.1f cm",
				"A running drop leaves a bit of its trail behind every this many centimetres, so a bigger drop runs further before it stops." },
			Field{ "physics", "cling", &Settings::cling, 0.0f, 20.0f, "Physics", "Cling under the breast", "%.1f s",
				"How long a drop hangs from skin that faces down, under the breast, before it falls. A lone drop hangs twice this, a heavy bead less." },
			Field{ "physics", "fall", &Settings::fall, 0.1f, 3.0f, "Physics", "Falling drop life", "%.2f s",
				"Seconds a drop that falls off the body stays visible." },

			Field{ "hands", "hands", &Settings::hands, 0, 1, "Hands", "Hands take milk", nullptr,
				"The actor's own hands smear and take away the milk they touch, in any animation that brings them to the breasts." },
			Field{ "hands", "absorb", &Settings::absorb, 0.02f, 2.0f, "Hands", "Wipe speed", "%.2f s",
				"Seconds a drop touched by a hand lasts before it is gone." },

			Field{ "squirt", "auto_nipples", &Settings::autoNipples, 0, 1, "Squirt", "Find the nipples", nullptr,
				"Places the squirt emitters on the nipples found on the actor's own meshes, whatever the body, preset or morphs. Off uses the offsets from the MCM, as before." },
			Field{ "squirt", "lift", &Settings::lift, -1.0f, 3.0f, "Squirt", "Emitter lift", "%.2f units",
				"How far off the nipple the squirt starts, along the nipple's direction. A little clear of the skin keeps the particles from starting inside it." },
			Field{ "squirt", "all_races", &Settings::allRaces, 0, 1, "Squirt", "Every humanoid race", nullptr,
				"Lets every humanoid race wear the squirt emitters, custom body races included, with no patch plugin. Takes effect at the next game start." },

			Field{ "debug", "show_nipples", &Settings::showDucts, 0, 1, "Test", "Show the nipples found", nullptr,
				"Marks where the leak starts on the console-selected actor, or on the player (a bead on each nipple). While it is on, that actor's milk draws at least 0.4 opaque." },
			Field{ "debug", "mount_world", &Settings::mountWorld, 0, 1, "Test", "Draw in world space", nullptr,
				"Off, the milk hangs under each breast's bone and rides it in the same frame. On, it is drawn in world space under the cell node, a frame behind a moving body: try it if drops float off a breast whose physics moves the mesh but not the bone's children." },
		};
		// clang-format on

		void Clamp(Settings& a_settings)
		{
			for (const auto& field : kFields) {
				std::visit(
					[&](auto a_member) {
						using T = std::remove_reference_t<decltype(a_settings.*a_member)>;
						if constexpr (std::is_same_v<T, int>) {
							a_settings.*a_member = std::clamp(a_settings.*a_member, static_cast<int>(field.min), static_cast<int>(field.max));
						} else if constexpr (std::is_same_v<T, float>) {
							const float value = a_settings.*a_member;
							a_settings.*a_member = std::isfinite(value) ? std::clamp(value, field.min, field.max) : field.min;
						}
					},
					field.member);
			}
		}

		bool ReadFile(const char* a_path, Settings& a_settings, bool a_required)
		{
			std::error_code error;
			if (!std::filesystem::exists(a_path, error)) {
				if (a_required) {
					logger::warn("{} is missing; using built-in defaults", a_path);
				}
				return false;
			}
			toml::table table;
			try {
				table = toml::parse_file(a_path);
			} catch (const toml::parse_error& e) {
				logger::error("{}: {} (line {}); file ignored", a_path, e.description(), e.source().begin.line);
				return false;
			}
			for (const auto& field : kFields) {
				const auto node = table[field.section][field.key];
				if (!node) {
					continue;
				}
				std::visit(
					[&](auto a_member) {
						using T = std::remove_reference_t<decltype(a_settings.*a_member)>;
						if constexpr (std::is_same_v<T, bool>) {
							if (const auto value = node.template value<bool>()) {
								a_settings.*a_member = *value;
							}
						} else if constexpr (std::is_same_v<T, int>) {
							if (const auto value = node.template value<std::int64_t>()) {
								a_settings.*a_member = static_cast<int>(*value);
							}
						} else {
							if (const auto value = node.template value<double>()) {
								a_settings.*a_member = static_cast<float>(*value);
							}
						}
					},
					field.member);
			}
			Clamp(a_settings);
			return true;
		}
	}

	std::span<const Field> Fields()
	{
		return kFields;
	}

	std::unique_lock<std::recursive_mutex> Settings::Lock()
	{
		return std::unique_lock{ g_lock };
	}

	Settings& Settings::Get()
	{
		return g_settings;
	}

	Settings& Settings::Persisted()
	{
		return g_persisted;
	}

	Settings Settings::Snapshot()
	{
		auto guard = Lock();
		return g_settings;
	}

	const Settings& Settings::Defaults()
	{
		return g_defaults;
	}

	void Settings::Load()
	{
		auto     guard = Lock();
		Settings loaded;
		ReadFile(kBaseFile, loaded, true);
		g_defaults = loaded;
		const bool user = ReadFile(kUserFile, loaded, false);
		g_settings = loaded;
		g_persisted = loaded;
		logger::info("settings loaded from {}{}", kBaseFile, user ? std::format(" and {}", kUserFile) : std::string{});
	}

	bool Settings::Save()
	{
		auto        guard = Lock();
		toml::table table;
		for (const auto& field : kFields) {
			std::visit(
				[&](auto a_member) {
					const auto value = g_persisted.*a_member;
					if (value == g_defaults.*a_member) {
						return;
					}
					if (!table.contains(field.section)) {
						table.insert(field.section, toml::table{});
					}
					auto* section = table[field.section].as_table();
					using T = std::remove_cvref_t<decltype(value)>;
					if constexpr (std::is_same_v<T, bool>) {
						section->insert_or_assign(field.key, value);
					} else if constexpr (std::is_same_v<T, int>) {
						section->insert_or_assign(field.key, static_cast<std::int64_t>(value));
					} else {
						section->insert_or_assign(field.key, std::round(static_cast<double>(value) * 1000.0) / 1000.0);
					}
				},
				field.member);
		}
		const std::filesystem::path target{ kUserFile };
		std::error_code             error;
		if (table.empty()) {
			std::filesystem::remove(target, error);
			return true;
		}
		auto temporary = target;
		temporary += ".tmp";
		{
			std::ofstream out{ temporary, std::ios::trunc };
			if (!out) {
				logger::error("cannot write {}", temporary.string());
				return false;
			}
			out << "# Written by the Oninus Lactis settings menu: only the values that differ from OninusLactis.toml.\n"
				   "# Delete this file to go back to the shipped defaults.\n\n";
			out << table << '\n';
			if (!out) {
				logger::error("cannot write {}", temporary.string());
				return false;
			}
		}
		std::filesystem::rename(temporary, target, error);
		if (error) {
			logger::error("cannot replace {}: {}", target.string(), error.message());
			std::filesystem::remove(temporary, error);
			return false;
		}
		return true;
	}

	void Settings::ResetToDefaults()
	{
		auto guard = Lock();
		g_settings = g_defaults;
		g_persisted = g_defaults;
	}
}
