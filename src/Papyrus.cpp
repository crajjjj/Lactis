#include "Papyrus.h"

#include "Milk.h"
#include "Settings.h"
#include "Squirt.h"

namespace Lactis::Papyrus
{
	namespace
	{
		constexpr auto kScript = "LactisNative"sv;
		constexpr auto kTestScript = "LactisTest"sv;

		RE::ActorHandle HandleOf(RE::Actor* a_actor)
		{
			return a_actor ? a_actor->GetHandle() : RE::ActorHandle{};
		}

		std::int32_t GetAPIVersion(RE::StaticFunctionTag*)
		{
			return kAPIVersion;
		}

		// ------------------------------------------------------------- the leak

		void Leak(RE::StaticFunctionTag*, RE::Actor* a_actor, float a_intensity, float a_seconds)
		{
			Milk::Get().Cry(HandleOf(a_actor), a_intensity, a_seconds);
		}

		void StopLeak(RE::StaticFunctionTag*, RE::Actor* a_actor, bool a_wipe)
		{
			Milk::Get().Stop(HandleOf(a_actor), a_wipe);
		}

		void Drip(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t a_side)
		{
			Milk::Get().Shed(HandleOf(a_actor), a_side);
		}

		void WipeMilk(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			Milk::Get().Wipe(HandleOf(a_actor));
		}

		bool IsLeaking(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return a_actor && Milk::Get().Query(a_actor->GetHandle()).crying;
		}

		float GetLeakIntensity(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return a_actor ? Milk::Get().Query(a_actor->GetHandle()).intensity : 0.0f;
		}

		float GetLeakTimeLeft(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return a_actor ? Milk::Get().Query(a_actor->GetHandle()).timeLeft : 0.0f;
		}

		bool IsLeakEnabled(RE::StaticFunctionTag*)
		{
			auto guard = Settings::Lock();
			return Settings::Get().enabled;
		}

		void SetLeakEnabled(RE::StaticFunctionTag*, bool a_enabled)
		{
			auto guard = Settings::Lock();
			Settings::Get().enabled = a_enabled;
		}

		void ReloadConfig(RE::StaticFunctionTag*)
		{
			Settings::Load();
		}

		RE::BSFixedString GetStatus(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			return Milk::Get().Status(HandleOf(a_actor), true);
		}

		// ----------------------------------------------------------- the squirt

		void PlaceEmitters(RE::StaticFunctionTag*, RE::Actor* a_actor, float a_x, float a_y, float a_z, float a_scale, bool a_axis)
		{
			Squirt::Place(HandleOf(a_actor), { a_x, a_y, a_z }, a_scale, a_axis);
		}

		void ForgetEmitters(RE::StaticFunctionTag*, RE::Actor* a_actor)
		{
			Squirt::Forget(HandleOf(a_actor));
		}

		bool FindsNipples(RE::StaticFunctionTag*)
		{
			auto guard = Settings::Lock();
			return Settings::Get().autoNipples;
		}

		bool IsSquirtEnabled(RE::StaticFunctionTag*)
		{
			auto guard = Settings::Lock();
			return Settings::Get().squirtEnabled;
		}

		// LactisTest debug natives

		void NearbyNative(RE::StaticFunctionTag*, float a_radius, float a_intensity, float a_seconds)
		{
			Milk::Get().CryNearby(a_radius, a_intensity, a_seconds);
		}

		void ShowNipplesNative(RE::StaticFunctionTag*, bool a_show)
		{
			auto guard = Settings::Lock();
			Settings::Get().showDucts = a_show;
		}

		void MountWorldNative(RE::StaticFunctionTag*, bool a_world)
		{
			auto guard = Settings::Lock();
			Settings::Get().mountWorld = a_world;
		}

		void AutoNipplesNative(RE::StaticFunctionTag*, bool a_auto)
		{
			auto guard = Settings::Lock();
			Settings::Get().autoNipples = a_auto;
		}

		void WipeAllNative(RE::StaticFunctionTag*)
		{
			Milk::Get().WipeAll();
		}

		std::int32_t LeakingCountNative(RE::StaticFunctionTag*)
		{
			return static_cast<std::int32_t>(Milk::Get().CryingCount());
		}
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("GetAPIVersion"sv, kScript, GetAPIVersion, true);
		a_vm->RegisterFunction("Leak"sv, kScript, Leak);
		a_vm->RegisterFunction("StopLeak"sv, kScript, StopLeak);
		a_vm->RegisterFunction("Drip"sv, kScript, Drip);
		a_vm->RegisterFunction("WipeMilk"sv, kScript, WipeMilk);
		a_vm->RegisterFunction("IsLeaking"sv, kScript, IsLeaking, true);
		a_vm->RegisterFunction("GetLeakIntensity"sv, kScript, GetLeakIntensity, true);
		a_vm->RegisterFunction("GetLeakTimeLeft"sv, kScript, GetLeakTimeLeft, true);
		a_vm->RegisterFunction("IsLeakEnabled"sv, kScript, IsLeakEnabled, true);
		a_vm->RegisterFunction("SetLeakEnabled"sv, kScript, SetLeakEnabled);
		a_vm->RegisterFunction("ReloadConfig"sv, kScript, ReloadConfig);
		a_vm->RegisterFunction("GetStatus"sv, kScript, GetStatus);
		a_vm->RegisterFunction("PlaceEmitters"sv, kScript, PlaceEmitters);
		a_vm->RegisterFunction("ForgetEmitters"sv, kScript, ForgetEmitters);
		a_vm->RegisterFunction("FindsNipples"sv, kScript, FindsNipples, true);
		a_vm->RegisterFunction("IsSquirtEnabled"sv, kScript, IsSquirtEnabled, true);

		a_vm->RegisterFunction("NearbyNative"sv, kTestScript, NearbyNative);
		a_vm->RegisterFunction("ShowNipplesNative"sv, kTestScript, ShowNipplesNative);
		a_vm->RegisterFunction("MountWorldNative"sv, kTestScript, MountWorldNative);
		a_vm->RegisterFunction("AutoNipplesNative"sv, kTestScript, AutoNipplesNative);
		a_vm->RegisterFunction("WipeAllNative"sv, kTestScript, WipeAllNative);
		a_vm->RegisterFunction("LeakingCountNative"sv, kTestScript, LeakingCountNative, true);
		logger::info("Papyrus API version {} registered", kAPIVersion);
		return true;
	}
}
