#include "Menu.h"

#include "Milk.h"
#include "Settings.h"

// The ONLY include site of the vendored SKSE Menu Framework header (see
// vendor\SKSEMenuFramework\README.txt). Its ImGui wrappers resolve the framework's
// exports with GetProcAddress and do not check for null, so every one used below is
// on kFrameworkExports, probed at registration: a framework that lacks one leaves
// the menu off instead of crashing. Text from settings or the game goes through
// TextUnformatted, never a printf-style wrapper.
#pragma warning(push, 0)
#include "SKSEMenuFramework/SKSEMenuFramework.h"
#pragma warning(pop)

namespace Lactis::Menu
{
	namespace
	{
		namespace Ui = ImGuiMCP;

		constexpr std::array kFrameworkExports{
			"AddSectionItem",
			"RegisterEventPriority",
			"igBeginTooltip",
			"igButton",
			"igCheckbox",
			"igEndTooltip",
			"igGetFontSize",
			"igIsItemHovered",
			"igPopID",
			"igPopTextWrapPos",
			"igPushID_Int",
			"igPushTextWrapPos",
			"igSameLine",
			"igSeparator",
			"igSliderFloat",
			"igSliderInt",
			"igSpacing",
			"igTextUnformatted",
		};

		std::atomic<bool> g_dirty{ false };
		float             g_testIntensity{ 0.7f };
		float             g_testSeconds{ 30.0f };
		float             g_testRadius{ 1500.0f };

		void Text(const std::string& a_text)
		{
			Ui::TextUnformatted(a_text.c_str());
		}

		void Tip(const char* a_text)
		{
			if (a_text && Ui::IsItemHovered()) {
				Ui::BeginTooltip();
				Ui::PushTextWrapPos(Ui::GetFontSize() * 30.0f);
				Ui::TextUnformatted(a_text);
				Ui::PopTextWrapPos();
				Ui::EndTooltip();
			}
		}

		void RenderField(const Field& a_field, int a_id)
		{
			Ui::PushID(a_id);
			std::visit(
				[&](auto a_member) {
					auto      guard = Settings::Lock();
					auto&     value = Settings::Get().*a_member;
					auto      edited = value;
					using T = std::remove_cvref_t<decltype(value)>;
					bool changed = false;
					if constexpr (std::is_same_v<T, bool>) {
						changed = Ui::Checkbox(a_field.label, &edited);
					} else if constexpr (std::is_same_v<T, int>) {
						changed = Ui::SliderInt(a_field.label, &edited, static_cast<int>(a_field.min), static_cast<int>(a_field.max),
							a_field.format ? a_field.format : "%d");
					} else {
						changed = Ui::SliderFloat(a_field.label, &edited, a_field.min, a_field.max, a_field.format ? a_field.format : "%.2f");
					}
					if (changed && edited != value) {
						value = edited;
						Settings::Persisted().*a_member = edited;
						g_dirty = true;
					}
				},
				a_field.member);
			Tip(a_field.tip);
			Ui::PopID();
		}

		void RenderPage(std::string_view a_page)
		{
			int id = 0;
			for (const auto& field : Fields()) {
				++id;
				if (a_page == field.page) {
					RenderField(field, id);
				}
			}
		}

		void Save()
		{
			if (g_dirty.exchange(false) && !Settings::Save()) {
				logger::error("menu: could not save OninusLactis_User.toml");
			}
		}

		RE::ActorHandle Player()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			return player ? player->GetHandle() : RE::ActorHandle{};
		}

		void __stdcall RenderLeak()
		{
			Text("The milk leak: drops that well up at the nipples and run down the skin. Changes apply at once and save when the menu closes.");
			Ui::Separator();
			RenderPage("Leak");
		}

		void __stdcall RenderLook()
		{
			Text("How the milk looks on the skin.");
			Ui::Separator();
			RenderPage("Look");
		}

		void __stdcall RenderPhysics()
		{
			Text("How the milk moves over the skin.");
			Ui::Separator();
			RenderPage("Physics");
		}

		void __stdcall RenderHands()
		{
			Text("The actor's own hands smear the milk they touch and take it away.");
			Ui::Separator();
			RenderPage("Hands");
		}

		void __stdcall RenderSquirt()
		{
			Text("The nipple squirt (the particle streams). The rest of its settings are in the mod's MCM.");
			Ui::Separator();
			RenderPage("Squirt");
		}

		void __stdcall RenderTest()
		{
			RenderPage("Test");
			Ui::Separator();
			Ui::SliderFloat("Test intensity", &g_testIntensity, 0.05f, 1.0f, "%.2f");
			Ui::SliderFloat("Test seconds", &g_testSeconds, 0.0f, 300.0f, "%.0f s");
			Tip("0 leaks until stopped.");
			if (Ui::Button("Player leaks")) {
				Milk::Get().Cry(Player(), g_testIntensity, g_testSeconds);
			}
			Ui::SameLine();
			if (Ui::Button("One drop")) {
				Milk::Get().Shed(Player(), 2);
			}
			Ui::SameLine();
			if (Ui::Button("Player stops")) {
				Milk::Get().Stop(Player(), false);
			}
			Ui::SameLine();
			if (Ui::Button("Wipe the player")) {
				Milk::Get().Wipe(Player());
			}
			Ui::SliderFloat("Nearby radius", &g_testRadius, 100.0f, 5000.0f, "%.0f units");
			if (Ui::Button("Nearby actors leak")) {
				Milk::Get().CryNearby(g_testRadius, g_testIntensity, g_testSeconds);
			}
			Ui::SameLine();
			if (Ui::Button("Wipe everyone")) {
				Milk::Get().WipeAll();
			}
			Ui::Separator();
			Text(Milk::Get().Status(Player()));
			Text(std::format("{} actor(s) leaking.", Milk::Get().CryingCount()));
			Ui::Separator();
			if (Ui::Button("Save now")) {
				g_dirty = true;
				Save();
			}
			Ui::SameLine();
			if (Ui::Button("Reset everything to the shipped defaults")) {
				Settings::ResetToDefaults();
				g_dirty = true;
				Save();
			}
			Tip("Puts every setting back to OninusLactis.toml and deletes your saved changes.");
		}

		void __stdcall OnEvent(SKSEMenuFramework::Model::EventType a_event) noexcept
		{
			try {
				if (a_event == SKSEMenuFramework::Model::EventType::kCloseMenu) {
					Save();
				}
			} catch (...) {
			}
		}
	}

	void Register()
	{
		const HMODULE module = ::GetModuleHandleW(L"SKSEMenuFramework");
		if (!module) {
			logger::info("menu: off, SKSE Menu Framework is not installed; edit OninusLactis.toml by hand");
			return;
		}
		std::string missing;
		for (const char* name : kFrameworkExports) {
			if (!::GetProcAddress(module, name)) {
				missing += missing.empty() ? "" : ", ";
				missing += name;
			}
		}
		if (!missing.empty()) {
			logger::warn("menu: off, this SKSE Menu Framework lacks {}", missing);
			return;
		}
		SKSEMenuFramework::SetSection("Oninus Lactis");
		SKSEMenuFramework::AddSectionItem("Leak", &RenderLeak);
		SKSEMenuFramework::AddSectionItem("Look", &RenderLook);
		SKSEMenuFramework::AddSectionItem("Physics", &RenderPhysics);
		SKSEMenuFramework::AddSectionItem("Hands", &RenderHands);
		SKSEMenuFramework::AddSectionItem("Squirt", &RenderSquirt);
		SKSEMenuFramework::AddSectionItem("Test", &RenderTest);
		SKSEMenuFramework::AddEvent(&OnEvent, 0.0f);  // lives as long as the game
		logger::info("menu: registered");
	}
}
