#pragma once

#include <variant>

namespace Lactis
{
	// Every tunable lives here. Load reads the shipped OninusLactis.toml and then
	// OninusLactis_User.toml on top; the SKSE menu edits the live values and Save
	// writes only the ones that differ from the shipped file into the user file, so
	// an update never overwrites what a player tuned.
	//
	// The values below are fallbacks for a key the toml lacks. The defaults players
	// see are the ones in dist\SKSE\Plugins\OninusLactis.toml.
	struct Settings
	{
		bool enabled{ true };
		bool debugLog{ false };

		// [leak]
		float rate{ 20.0f };
		int   drop{ 8 };
		float well{ 0.8f };
		float hold{ 0.6f };
		float variety{ 0.4f };
		bool  bothEyes{ true };  // both breasts
		int   arousal{ 0 };          // leak at this arousal and above; 0 = never
		bool  arousalNpcs{ false };  // female NPCs nearby too, not only the player
		int   maxActors{ 6 };
		int   maxParticles{ 200 };
		float maxDistance{ 3000.0f };

		// [look]
		int   style{ 1 };  // 0 reflective, 1 plain (no glow, no reflection), 2 refraction
		float refraction{ 0.3f };
		float size{ 0.9f };
		float trail{ 0.9f };
		float opacity{ 0.75f };
		float shine{ 0.6f };
		float gloss{ 80.0f };
		float reflection{ 0.1f };
		float glow{ 0.0f };
		bool  envLightFade{ true };
		float lifetime{ 45.0f };
		float fade{ 6.0f };

		// [physics]
		float stickiness{ 1.5f };
		float friction{ 40.0f };
		float viscosity{ 35.0f };
		float cohesion{ 3.0f };
		float stretch{ 3.0f };
		float trailStep{ 2.5f };
		float cling{ 1.5f };
		float fall{ 0.6f };

		// [hands]
		bool  hands{ true };
		float absorb{ 0.3f };

		// [squirt]: the nipple squirt emitters (the equipped particle meshes)
		bool  squirtEnabled{ true };  // off: no particle streams, a squirt request gives its leak only
		bool  autoNipples{ true };  // place the emitters on the nipples found on the meshes
		float lift{ 0.1f };         // game units off the nipple, along its normal
		bool  allRaces{ true };     // let every humanoid race wear the emitters
		bool  hideCovered{ true };  // no squirt from a nipple clothes or armor cover

		// [debug]
		bool showDucts{ false };
		bool mountWorld{ false };

		// Never read from a file: the fluid code inherited from Lacrima still asks for
		// them (streams along an eyelid, running mascara). One start point, no marks.
		int   streams{ 1 };
		float cornerTears{ 0.0f };
		float mascara{ 0.0f };
		float mascaraOpacity{ 0.5f };
		float mascaraWidth{ 1.0f };
		float mascaraSmudge{ 0.0f };
		float mascaraSpread{ 0.0f };
		float mascaraCorners{ 0.0f };
		int   mascaraLines{ 3 };
		float mascaraLength{ 0.0f };
		float mascaraLifetime{ 1.0f };

		static std::unique_lock<std::recursive_mutex> Lock();
		static Settings&                              Get();
		// What the menu saves. The menu edits both this and Get(); a script or the
		// console edits only Get(), so it lasts one session.
		static Settings&                              Persisted();
		static Settings                               Snapshot();
		static const Settings&                        Defaults();

		static void Load();
		static bool Save();
		static void ResetToDefaults();
	};

	struct Field
	{
		using Member = std::variant<bool Settings::*, int Settings::*, float Settings::*>;

		const char* section;
		const char* key;
		Member      member;
		float       min;
		float       max;
		const char* page;
		const char* label;
		const char* format;
		const char* tip;
	};

	std::span<const Field> Fields();
}
