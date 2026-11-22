#include "Arousal.h"

#include "Milk.h"
#include "Settings.h"

namespace Lactis::Arousal
{
	namespace
	{
		constexpr float kPoll = 5.0f;   // seconds between looks
		constexpr float kHold = 12.0f;  // seconds one look's leak lasts; the next look renews it

		using GetArousalInt = std::int32_t (*)(RE::Actor*);

		GetArousalInt   g_native{ nullptr };
		RE::TESFaction* g_faction{ nullptr };
		float           g_wait{ 0.0f };
		// Actors whose running leak this module started. A leak anyone else started is
		// never renewed, shortened or stopped from here.
		std::unordered_set<std::uint32_t> g_own;

		// -1 when there is no reading for the actor.
		int Read(RE::Actor* a_actor)
		{
			if (g_native) {
				return g_native(a_actor);
			}
			if (g_faction) {
				return a_actor->GetFactionRank(g_faction, a_actor->IsPlayerRef());
			}
			return -1;
		}

		void Look(RE::Actor* a_actor, int a_threshold, std::size_t a_most)
		{
			if (!a_actor || a_actor->IsDead() || !a_actor->Get3D(false) || a_actor->IsChild()) {
				return;
			}
			const auto* base = a_actor->GetActorBase();
			const auto* race = a_actor->GetRace();
			// humanoids only: the high-process list holds creatures too, and SexLab
			// Aroused rates those as well
			if (!base || base->GetSex() != RE::SEX::kFemale || !race || !race->HasKeywordString("ActorTypeNPC")) {
				return;
			}
			const auto handle = a_actor->GetHandle();
			const auto key = handle.native_handle();
			const int  arousal = Read(a_actor);
			if (arousal < a_threshold) {
				g_own.erase(key);  // the leak it has runs out by itself
				return;
			}
			const auto state = Milk::Get().Query(handle);
			if (state.crying) {
				// someone else's leak, or one of ours a script has since replaced with
				// a longer one: leave it alone
				if (!g_own.contains(key) || state.timeLeft < 0.0f || state.timeLeft > kHold + 0.5f) {
					g_own.erase(key);
					return;
				}
			} else if (Milk::Get().CryingCount() >= a_most) {
				// A new leak past [leak] max_actors would end the oldest one, which
				// may be a squirt's or another mod's, and with more aroused actors
				// than the limit they would push each other out at every look.
				g_own.erase(key);
				return;
			}
			const float above = a_threshold >= 100 ? 1.0f : static_cast<float>(arousal - a_threshold) / static_cast<float>(100 - a_threshold);
			Milk::Get().Cry(handle, 0.3f + 0.7f * std::clamp(above, 0.0f, 1.0f), kHold);
			g_own.insert(key);
		}
	}

	void Connect()
	{
		if (const HMODULE module = ::GetModuleHandleW(L"SexlabArousedNG.dll")) {
			g_native = reinterpret_cast<GetArousalInt>(::GetProcAddress(module, "SLA_GetArousalInt"));
		}
		if (auto* data = RE::TESDataHandler::GetSingleton()) {
			g_faction = data->LookupForm<RE::TESFaction>(0x3FC36, "SexLabAroused.esm"sv);
		}
		logger::info("arousal: {}", g_native ? "read from SLO Aroused NG" :
									g_faction ? "read from the sla_Arousal faction" :
												"no SexLab Aroused found; [leak] arousal does nothing");
	}

	void Update(float a_delta)
	{
		g_wait -= a_delta;
		if (g_wait > 0.0f) {
			return;
		}
		g_wait = kPoll;
		if (!g_native && !g_faction) {
			return;
		}
		int         threshold = 0;
		bool        npcs = false;
		std::size_t most = 1;
		{
			auto guard = Settings::Lock();
			const auto& settings = Settings::Get();
			threshold = settings.enabled ? settings.arousal : 0;
			npcs = settings.arousalNpcs;
			most = static_cast<std::size_t>(std::max(settings.maxActors, 1));
		}
		if (threshold <= 0) {
			g_own.clear();
			return;
		}
		Look(RE::PlayerCharacter::GetSingleton(), threshold, most);
		if (auto* lists = npcs ? RE::ProcessLists::GetSingleton() : nullptr) {
			for (const auto& handle : lists->highActorHandles) {
				Look(handle.get().get(), threshold, most);
			}
		}
	}

	void Reset()
	{
		g_own.clear();
		g_wait = 0.0f;
	}
}
