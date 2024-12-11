#include "imgui_ex.h"

#include "../imgui/imgui_internal.h"

#include <unordered_map>
#include <Windows.h>

void ImGui::ClipWindowToScreen()
{
	const auto screen_size = ImGui::GetIO().DisplaySize;

	const auto window_position = ImGui::GetWindowPos();
	const auto window_size = ImGui::GetWindowSize();
	const auto target_position = ImVec2(
		std::clamp(window_position.x, 0.0f, max(screen_size.x - window_size.x, 0.f)),
		std::clamp(window_position.y, 0.0f, max(screen_size.y - window_size.y, 0.f))
	);

	if (window_position != target_position)
		ImGui::SetWindowPos(target_position);
}

bool ImGui::SmallCheckbox(const char* label, bool* v)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.f));
	const auto r = Checkbox(label, v);
	ImGui::PopStyleVar();
	return r;
}

bool ImGui::ButtonDisabled(const char* label, bool disabled)
{
	if (disabled)
	{
		PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.65f);
		PushItemFlag(ImGuiItemFlags_Disabled, true);
	}

	ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
	auto result = ImGui::Button(label, ImVec2(ImGui::GetColumnWidth(), 0.f));

	if (disabled)
	{
		PopItemFlag();
		PopStyleVar();

		return false;
	}

	return result;
}

bool ImGui::ButtonParser(ParseStatus status)
{
	ID id("Parser Button");

	const auto get_text = [](ParseStatus status) -> const char*
		{
			switch (status)
			{
			case ParseStatus::UNPARSED: return "Parse";
			case ParseStatus::QUEUED: return "Queued";
			case ParseStatus::PARSING: return "Parsing";
			case ParseStatus::PARSED: return "Open";
			case ParseStatus::FAILED: return "Failed";
			default: return "Unknown";
			}
		};

	auto available = status == ParseStatus::PARSED || status == ParseStatus::UNPARSED;

	return ButtonDisabled(get_text(status), !available);
}

void ImGui::DelayedTooltipText(const std::string& text, double delay)
{
	static std::unordered_map<ImGuiID, double> hover_times;

	auto id = ImGui::GetID((void*)(&text));

	if (ImGui::IsItemHovered())
	{
		auto [iter, inserted] = hover_times.try_emplace(id, ImGui::GetTime());

		if (ImGui::GetTime() - iter->second >= delay)
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(text.c_str());
			ImGui::EndTooltip();
		}
	}
	else
		hover_times.erase(id);
}

bool ImGui::KeySelector(const char* label, Hotkey* v)
{
	ID id("Key Selector");

	auto get_display_name = [](Hotkey hotkey) -> std::string
		{
			static const std::unordered_map<int, std::string> key_map =
			{
				{VK_BACK, "Backspace"},              {VK_TAB, "Tab"},                  {VK_RETURN, "Enter"},
				{VK_SHIFT, "Shift"},                 {VK_CONTROL, "Ctrl"},             {VK_MENU, "Alt"},
				{VK_PAUSE, "Pause"},                 {VK_CAPITAL, "Caps Lock"},        {VK_ESCAPE, "Esc"},
				{VK_SPACE, "Space"},                 {VK_PRIOR, "Page Up"},            {VK_NEXT, "Page Down"},
				{VK_END, "End"},                     {VK_HOME, "Home"},                {VK_LEFT, "Left Arrow"},
				{VK_UP, "Up Arrow"},                 {VK_RIGHT, "Right Arrow"},        {VK_DOWN, "Down Arrow"},
				{VK_SELECT, "Select"},               {VK_PRINT, "Print"},              {VK_EXECUTE, "Execute"},
				{VK_SNAPSHOT, "Print Screen"},       {VK_INSERT, "Insert"},            {VK_DELETE, "Delete"},
				{VK_HELP, "Help"},                   {VK_LWIN, "Left Windows"},        {VK_RWIN, "Right Windows"},
				{VK_APPS, "Applications"},           {VK_SLEEP, "Sleep"},              {VK_NUMPAD0, "Numpad 0"},
				{VK_NUMPAD1, "Numpad 1"},            {VK_NUMPAD2, "Numpad 2"},         {VK_NUMPAD3, "Numpad 3"},
				{VK_NUMPAD4, "Numpad 4"},            {VK_NUMPAD5, "Numpad 5"},         {VK_NUMPAD6, "Numpad 6"},
				{VK_NUMPAD7, "Numpad 7"},            {VK_NUMPAD8, "Numpad 8"},         {VK_NUMPAD9, "Numpad 9"},
				{VK_MULTIPLY, "Numpad *"},           {VK_ADD, "Numpad +"},             {VK_SEPARATOR, "Numpad Separator"},
				{VK_SUBTRACT, "Numpad -"},           {VK_DECIMAL, "Numpad ."},         {VK_DIVIDE, "Numpad /"},
				{VK_F1, "F1"},                       {VK_F2, "F2"},                    {VK_F3, "F3"},
				{VK_F4, "F4"},                       {VK_F5, "F5"},                    {VK_F6, "F6"},
				{VK_F7, "F7"},                       {VK_F8, "F8"},                    {VK_F9, "F9"},
				{VK_F10, "F10"},                     {VK_F11, "F11"},                  {VK_F12, "F12"},
				{VK_F13, "F13"},                     {VK_F14, "F14"},                  {VK_F15, "F15"},
				{VK_F16, "F16"},                     {VK_F17, "F17"},                  {VK_F18, "F18"},
				{VK_F19, "F19"},                     {VK_F20, "F20"},                  {VK_F21, "F21"},
				{VK_F22, "F22"},                     {VK_F23, "F23"},                  {VK_F24, "F24"},
				{VK_NUMLOCK, "Num Lock"},            {VK_SCROLL, "Scroll Lock"},
				{VK_LBUTTON, "Left Mouse Button"},   {VK_RBUTTON, "Right Mouse Button"},
				{VK_MBUTTON, "Middle Mouse Button"}, {VK_XBUTTON1, "Mouse Button 4"},  {VK_XBUTTON2, "Mouse Button 5"},
				{VK_OEM_1, ";:"},                    {VK_OEM_PLUS, "=+"},              {VK_OEM_COMMA, ",<"},
				{VK_OEM_MINUS, "-_"},                {VK_OEM_PERIOD, ".>"},            {VK_OEM_2, "/?"},
				{VK_OEM_3, "`~"},                    {VK_OEM_4, "[{"},                 {VK_OEM_5, "\\|"},
				{VK_OEM_6, "]}"},                    {VK_OEM_7, "'\""},                {VK_OEM_8, "�"},
				{VK_OEM_102, "<>"},
				{0x30, "0"}, {0x31, "1"}, {0x32, "2"}, {0x33, "3"},
				{0x34, "4"}, {0x35, "5"}, {0x36, "6"}, {0x37, "7"},
				{0x38, "8"}, {0x39, "9"}, {0x41, "A"}, {0x42, "B"},
				{0x43, "C"}, {0x44, "D"}, {0x45, "E"}, {0x46, "F"},
				{0x47, "G"}, {0x48, "H"}, {0x49, "I"}, {0x4A, "J"},
				{0x4B, "K"}, {0x4C, "L"}, {0x4D, "M"}, {0x4E, "N"},
				{0x4F, "O"}, {0x50, "P"}, {0x51, "Q"}, {0x52, "R"},
				{0x53, "S"}, {0x54, "T"}, {0x55, "U"}, {0x56, "V"},
				{0x57, "W"}, {0x58, "X"}, {0x59, "Y"}, {0x5A, "Z"}
			};

			std::string name = "";

			if (hotkey.ctrl) name += "Ctrl+";
			if (hotkey.shift) name += "Shift+";
			if (hotkey.alt) name += "Alt+";

			auto it = key_map.find(hotkey.key);

			if (it != key_map.end())
				name += it->second;
			else if (hotkey.key != 0)
				name += "?";

			return name;
		};

	static Hotkey hotkey;

	static bool is_waiting_for_input = false;

	{
		auto text = is_waiting_for_input ? (hotkey.ctrl || hotkey.shift || hotkey.alt) ? get_display_name(hotkey) : "..." : v->key ? get_display_name(*v) : "unbound";

		if (ImGui::Button(text.c_str()))
		{
			is_waiting_for_input = true;
			hotkey.reset();
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(label);
		ImGui::DelayedTooltipText("Use backspace to unbind");
	}

	if (is_waiting_for_input)
	{
		if (ImGui::IsKeyDown(VK_BACK))
		{
			v->reset();
			is_waiting_for_input = false;
			return true;
		}

		if (ImGui::IsKeyDown(VK_LBUTTON) || ImGui::IsKeyDown(VK_RBUTTON || ImGui::IsKeyDown(VK_ESCAPE)))
		{
			hotkey.reset();
			is_waiting_for_input = false;
			return false;
		}

		hotkey.ctrl = ImGui::IsKeyDown(VK_CONTROL) || ImGui::IsKeyDown(VK_LCONTROL) || ImGui::IsKeyDown(VK_RCONTROL);
		hotkey.shift = ImGui::IsKeyDown(VK_SHIFT) || ImGui::IsKeyDown(VK_LSHIFT) || ImGui::IsKeyDown(VK_RSHIFT);
		hotkey.alt = ImGui::IsKeyDown(VK_MENU) || ImGui::IsKeyDown(VK_LMENU) || ImGui::IsKeyDown(VK_RMENU);

		for (auto key = 0x08; key <= 0xFE; ++key)
		{
			if (ImGui::IsKeyDown(key) && !(key == VK_LBUTTON || key == VK_RBUTTON || key == VK_CONTROL || key == VK_LCONTROL ||
				key == VK_RCONTROL || key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT || key == VK_MENU ||
				key == VK_LMENU || key == VK_RMENU || key == VK_LWIN || key == VK_RWIN))
			{
				hotkey.key = key;
				is_waiting_for_input = false;
				break;
			}
		}

		if (hotkey.key && !is_waiting_for_input)
		{
			*v = hotkey;
			return true;
		}
	}

	return false;
}

void ImGui::Indicator(ImVec4 color)
{
	auto top_left = ImGui::GetCursorScreenPos();
	const auto height = ImGui::GetTextLineHeightWithSpacing();
	const auto radius = height / 4 - 1;
	const auto bot_right = ImVec2(top_left.x + 2 * radius, top_left.y + height);
	const auto center = ImVec2(top_left.x + radius, top_left.y + height / 2);
	ImGui::Dummy(ImVec2(2 * radius, height));
	ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(color));
}

bool ImGui::ButtonDpsReport(DpsReportUploadStatus status)
{
	ID id("Dps Report Button");

	const auto get_text = [](DpsReportUploadStatus status) -> const char*
		{
			switch (status)
			{
			case DpsReportUploadStatus::AVAILABLE: return "Upload";
			case DpsReportUploadStatus::QUEUED:	return "Queued";
			case DpsReportUploadStatus::UPLOADING: return "Uploading";
			case DpsReportUploadStatus::UPLOADED: return "Open";
			case DpsReportUploadStatus::FAILED:	return "Reupload";
			default: return "Unknown";
			}
		};

	auto available = status == DpsReportUploadStatus::AVAILABLE || status == DpsReportUploadStatus::UPLOADED || status == DpsReportUploadStatus::FAILED;

	return ButtonDisabled(get_text(status), !available);
}

bool ImGui::ButtonWingman(WingmanUploadStatus status, ParseStatus parse_status)
{
	ID id("Wingman Button");

	const auto get_text = [](WingmanUploadStatus status) -> const char*
		{
			switch (status)
			{
			case WingmanUploadStatus::AVAILABLE: return "Upload";
			case WingmanUploadStatus::QUEUED: return "Queued";
			case WingmanUploadStatus::UPLOADING: return "Uploading";
			case WingmanUploadStatus::SKIPPED: return "Skipped";
			case WingmanUploadStatus::UPLOADED: return "Uploaded";
			case WingmanUploadStatus::UNAVAILABLE: return "Unavailable";
			case WingmanUploadStatus::FAILED: return "Retry";
			default: return "Unknown";
			}
		};

	auto available = parse_status == ParseStatus::PARSED && (status == WingmanUploadStatus::AVAILABLE || status == WingmanUploadStatus::FAILED);

	return ButtonDisabled(get_text(status), !available);

}

void ImGui::CenterNextItemHorizontally(const char* text)
{
	auto offset = (ImGui::GetColumnWidth() - ImGui::CalcTextSize(text).x) * .5f;
	offset = offset > 0 ? offset : 0;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
}

std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::vector<TriggerID>>>>> categorized_triggers = {
	{"Raids", {
		{"Spirit Vale", {TriggerID::ValeGuardian, TriggerID::SpiritRace, TriggerID::Gorseval, TriggerID::SabethaTheSaboteur}},
		{"Salvation Pass", {TriggerID::Slothasor, TriggerID::BanditTrio, TriggerID::MatthiasGabrel}},
		{"Stronghold of the Faithful", {TriggerID::SiegeTheStronghold, TriggerID::KeepConstruct, TriggerID::TwistedCastle, TriggerID::Xera}},
		{"Bastion of the Penitent", {TriggerID::CairnTheIndomitable, TriggerID::MursaatOverseer, TriggerID::Samarog, TriggerID::Deimos}},
		{"Hall of Chains", {TriggerID::SoullessHorror, TriggerID::RiverOfSouls, TriggerID::StatueOfIce, TriggerID::StatueOfDarkness, TriggerID::StatueOfDeath, TriggerID::Dhuum}},
		{"Mythwright Gambit", {TriggerID::ConjuredAmalgamate, TriggerID::TwinLargos, TriggerID::Qadim}},
		{"The Key of Ahdashim", {TriggerID::CardinalAdina, TriggerID::CardinalSabir, TriggerID::QadimThePeerless}},
		{"Mount Balrior", {TriggerID::DecimaTheStormsinger, TriggerID::GreerTheBlightbringer, TriggerID::UraTheSteamshrieker}},
	}},
	{"Fractals", {
		{"Nightmare", {TriggerID::MAMA, TriggerID::SiaxTheCorrupted, TriggerID::EnsolyssOfTheEndlessTorment}},
		{"Shattered Observatory", {TriggerID::SkorvaldTheShattered, TriggerID::Artsariiv, TriggerID::Arkk}},
		{"Sunqua Peak", {TriggerID::AiKeeperOfThePeak}},
		{"Silent Surf", {TriggerID::Kanaxai, TriggerID::KanaxaiChallengeMode}},
		{"Lonely Tower", {TriggerID::Eparch}},
	}},
	{"Strike Missions", {
		{"Core", {TriggerID::OldLionsCourt, TriggerID::OldLionsCourtChallengeMode}},
		{"The Icebrood Saga", {TriggerID::IcebroodConstruct, TriggerID::SuperKodanBrothers, TriggerID::FraenirOfJormag, TriggerID::Boneskinner, TriggerID::WhisperOfJormag}},
		{"End of Dragons", {TriggerID::AetherbladeHideout, TriggerID::XunlaiJadeJunkyard, TriggerID::KainengOverlook, TriggerID::KainengOverlookChallengeMode, TriggerID::HarvestTemple}},
		{"Secrets of the Obscure", {TriggerID::CosmicObservatory, TriggerID::TempleOfFebe}},
	}},
	{"Other", {
		{"Convergences", {TriggerID::DemonKnight, TriggerID::Sorrow, TriggerID::Dreadwing, TriggerID::HellSister, TriggerID::Umbriel}},
		{"Special Forces Training Area", {TriggerID::StandardKittyGolem, TriggerID::MediumKittyGolem, TriggerID::LargeKittyGolem}},
		{"World vs World", {TriggerID::WorldVsWorld}},
		{"Uncategorized", {TriggerID::Freezie, TriggerID::DregShark, TriggerID::HeartsAndMinds}},
	}},
};

bool ImGui::EncounterSelector(const char* label, EncounterSelection* value)
{
	ID id("Encounter Selector");

	auto r = false;

	static char search_buffer[64] = "";
	static char previous_search_buffer[64] = "";
	static bool expand_on_search_update = false;
	static bool expand_all = false;
	static bool collapse_all = false;

	ImGui::TextDisabled(label);
	ImGui::Spacing();

	if (ImGui::InputText("Search", search_buffer, IM_ARRAYSIZE(search_buffer)))
	{
		if (strcmp(search_buffer, previous_search_buffer) != 0)
		{
			expand_on_search_update = true;
			strncpy_s(previous_search_buffer, search_buffer, sizeof(previous_search_buffer));
		}
	}

	std::string search_text_lower = search_buffer;
	std::transform(search_text_lower.begin(), search_text_lower.end(), search_text_lower.begin(), [](unsigned char c)
		{
			return std::tolower(c);
		});

	ImGui::Spacing();
	if (ImGui::Button("Select All"))
	{
		for (const auto& [main_category, sub_categories] : categorized_triggers)
		{
			for (const auto& [sub_category, triggers] : sub_categories)
			{
				for (const auto& trigger_id : triggers)
				{
					auto it = global::trigger_id_encounter_name_map.find(trigger_id);
					if (it != global::trigger_id_encounter_name_map.end())
					{
						std::string trigger_name_lower = it->second;
						std::string trigger_id_str = std::to_string(static_cast<int>(trigger_id));
						std::string searchable_text = trigger_name_lower + " " + trigger_id_str + " " + main_category + " " + sub_category;

						std::transform(searchable_text.begin(), searchable_text.end(), searchable_text.begin(), [](unsigned char c)
							{
								return std::tolower(c);
							});

						if (search_text_lower.empty() || searchable_text.find(search_text_lower) != std::string::npos)
							if (std::find(value->begin(), value->end(), trigger_id) == value->end())
								value->push_back(trigger_id);
					}
				}
			}
		}
		r = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Deselect All"))
	{
		for (const auto& [main_category, sub_categories] : categorized_triggers)
		{
			for (const auto& [sub_category, triggers] : sub_categories)
			{
				for (const auto& trigger_id : triggers)
				{
					auto it = global::trigger_id_encounter_name_map.find(trigger_id);
					if (it != global::trigger_id_encounter_name_map.end())
					{
						std::string trigger_name_lower = it->second;
						std::string trigger_id_str = std::to_string(static_cast<int>(trigger_id));
						std::string searchable_text = trigger_name_lower + " " + trigger_id_str + " " + main_category + " " + sub_category;

						std::transform(searchable_text.begin(), searchable_text.end(), searchable_text.begin(), [](unsigned char c)
							{
								return std::tolower(c);
							});

						if (search_text_lower.empty() || searchable_text.find(search_text_lower) != std::string::npos)
							value->erase(std::remove(value->begin(), value->end(), trigger_id), value->end());
					}
				}
			}
		}
		r = true;
	}
	ImGui::Spacing();
	if (ImGui::Button("Expand All"))
	{
		expand_all = true;
		collapse_all = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Collapse All"))
	{
		collapse_all = true;
		expand_all = false;
	}

	ImGui::Spacing();

	for (const auto& [main_category, sub_categories] : categorized_triggers)
	{
		bool main_category_matches = false;

		std::vector<std::pair<std::string, std::vector<TriggerID>>> filtered_subcategories;

		for (const auto& [sub_category, triggers] : sub_categories)
		{
			std::vector<TriggerID> filtered_triggers;

			for (const auto& trigger_id : triggers)
			{
				auto it = global::trigger_id_encounter_name_map.find(trigger_id);
				if (it != global::trigger_id_encounter_name_map.end())
				{
					std::string trigger_name_lower = it->second;
					std::string trigger_id_str = std::to_string(static_cast<int>(trigger_id));
					std::string searchable_text = trigger_name_lower + " " + trigger_id_str + " " + main_category + " " + sub_category;

					std::transform(searchable_text.begin(), searchable_text.end(), searchable_text.begin(), [](unsigned char c)
						{
							return std::tolower(c);
						});

					if (search_text_lower.empty() || searchable_text.find(search_text_lower) != std::string::npos)
						filtered_triggers.push_back(trigger_id);
				}
			}

			if (!filtered_triggers.empty())
			{
				filtered_subcategories.emplace_back(sub_category, filtered_triggers);
				main_category_matches = true;
			}
		}

		if (!main_category_matches)
			continue;

		bool main_category_open = expand_all || (expand_on_search_update && main_category_matches);

		if (main_category_open)
			ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		else if (collapse_all)
			ImGui::SetNextItemOpen(false, ImGuiCond_Always);

		if (ImGui::CollapsingHeader(main_category.c_str(), ImGuiTreeNodeFlags_AllowItemOverlap))
		{
			ImGui::Indent();

			for (const auto& [sub_category, filtered_triggers] : filtered_subcategories)
			{
				bool sub_category_open = expand_all || (expand_on_search_update && main_category_matches);

				if (sub_category_open)
					ImGui::SetNextItemOpen(true, ImGuiCond_Always);
				else if (collapse_all)
					ImGui::SetNextItemOpen(false, ImGuiCond_Always);

				if (ImGui::TreeNode(sub_category.c_str()))
				{
					for (const auto& trigger_id : filtered_triggers)
					{
						auto it = global::trigger_id_encounter_name_map.find(trigger_id);
						if (it == global::trigger_id_encounter_name_map.end())
							continue;

						const std::string& name = it->second;
						std::string checkbox_label = name + " (" + std::to_string(static_cast<int>(trigger_id)) + ")";

						bool selected = std::find(value->begin(), value->end(), trigger_id) != value->end();
						if (ImGui::Checkbox(checkbox_label.c_str(), &selected))
						{
							if (selected)
							{
								if (std::find(value->begin(), value->end(), trigger_id) == value->end())
									value->push_back(trigger_id);
							}
							else
								value->erase(std::remove(value->begin(), value->end(), trigger_id), value->end());

							r = true;
						}
					}
					ImGui::TreePop();
				}
			}

			ImGui::Unindent();
		}
	}

	// reset flags
	expand_all = false;
	collapse_all = false;
	expand_on_search_update = false;

	return r;
}
