#include "Arousal.h"
#include "Hooks.h"
#include "Menu.h"
#include "Papyrus.h"
#include "Settings.h"
#include "Squirt.h"

namespace
{
	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		switch (a_message->type) {
		case SKSE::MessagingInterface::kInputLoaded:
			Lactis::Hooks::Install();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Lactis::Menu::Register();
			Lactis::Squirt::PatchRaces();
			Lactis::Arousal::Connect();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Lactis::Hooks::ResetSession();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	// 14 bytes: the one call hook of src/Hooks.cpp
	SKSE::Init(a_skse, { .trampoline = true, .trampolineSize = 14 });
	// CommonLib loads the Address Library at the first relocation; load it here so a
	// missing one fails at game start, not mid-session
	(void)REL::IDDB::get();

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	logger::info("{} {} on Skyrim {}", plugin->GetName(), plugin->GetVersion().string("."sv), REL::Module::get().version().string("."sv));

	Lactis::Settings::Load();

	auto* papyrus = SKSE::GetPapyrusInterface();
	if (!papyrus || !papyrus->Register(Lactis::Papyrus::Register)) {
		logger::error("could not register the Papyrus functions");
	}
	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(OnMessage)) {
		logger::error("could not register the SKSE message listener; the milk leak and the nipple search stay off");
		return false;
	}
	return true;
}
