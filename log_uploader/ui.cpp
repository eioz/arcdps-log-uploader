#include "dps_report_uploader.h"
#include "elite_insights.h"
#include "imgui_ex.h"
#include "log_manager.h"
#include "logger.h"
#include "mumble_link.h"
#include "ui.h"
#include "wingman_uploader.h"

#include <mutex>
#include <shared_mutex>

namespace global { std::unique_ptr<UI> ui = std::make_unique<UI>(); }

#define LOG(message, log_level) global::logger->write(message, log_level, LogSource::UI)

#define SAVE_SETTING(Setting) \
global::settings->write([this](auto& _settings) \
{ \
_settings.Setting = this->settings.Setting; \
});

#define UI_ELEMENT(ImGuiFunction, Label, Setting) \
if (ImGuiFunction(Label, &(this->settings.Setting))) { \
SAVE_SETTING(Setting); \
}

void UI::initialize()
{
	std::lock_guard lock(this->initialization_mutex);

	this->initialized.store(true);
}

void UI::draw()
{
	if (!this->is_initialized())
		return;

	auto open = this->open.load();

	if (global::mumble_link->update())
	{
		const auto in_combat = (global::mumble_link->get_memory().getMumbleContext()->uiState & UiStateFlags_::UiStateFlags_InCombat) != 0;

		if (!in_combat)
			this->force_open.store(false);

		if (GET_SETTING(display.hide_in_combat))
			open = in_combat ? this->force_open.load() : open;
	}

	if (!open)
		return;

	this->refresh_settings_cache();
	this->refresh_encounter_logs();

	this->draw_main_window();
}

void UI::draw_arc_window_options()
{
	if (!this->is_initialized())
		return;

	auto open = this->open.load(); auto open_base = open;

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 1.f)); // aligns with arcdps default style

	if (ImGui::Checkbox("Log Uploader", &open))
		if (open != open_base)
			this->open.store(open);

	ImGui::PopStyleVar();
}

void UI::draw_arc_mod_options()
{
	if (!this->is_initialized())
		return;

	this->refresh_settings_cache();

	ImGui::ID id("Arcdps Mod Options");

	if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
	{
		this->draw_display_settings();
	}
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("Parser", ImGuiTreeNodeFlags_DefaultOpen))
	{
		this->draw_parser_settings();
	}
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("dps.report", ImGuiTreeNodeFlags_DefaultOpen))
	{
		this->draw_dps_report_settings();
	}
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("Wingman", ImGuiTreeNodeFlags_DefaultOpen))
	{
		this->draw_wingman_settings();
	}
}

void UI::on_open_hotkey()
{
	this->open.store(!this->open.load());
	this->force_open.store(!this->force_open.load());
}

void UI::refresh_settings_cache()
{
	this->settings = global::settings->get();
}

void UI::refresh_encounter_logs()
{
	this->encounter_logs.clear();

	for (auto& encounter_log : global::log_manager->get_encounter_logs())
		this->encounter_logs.push_back(std::make_tuple(encounter_log, encounter_log->get_data()));
}

void UI::draw_main_window()
{

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 1.f));

	ImGui::SetNextWindowSize(this->settings.display.window_size);

	auto window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_AlwaysAutoResize;

	if (this->settings.display.hide_title_bar)
		window_flags |= ImGuiWindowFlags_NoTitleBar;

	const auto hide_background = this->settings.display.hide_background;

	if (hide_background)
	{
		window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ImVec4(0, 0, 0, 0));
	}

	auto open = this->open.load(); auto open_base = open;

	ImGui::ID id("Main Window");

	if (ImGui::Begin("Log Uploader", &open, window_flags))
	{
		if (open != open_base)
			this->open.store(open);

		if (this->settings.display.clip_to_screen)
			ImGui::ClipWindowToScreen();

		auto column_count = static_cast<int>(LogTableColumns::_COUNT);

		if (this->settings.display.hide_elite_insights)
			column_count--;
		if (this->settings.display.hide_dps_report)
			column_count--;
		if (this->settings.display.hide_wingman)
			column_count--;

		std::deque<std::pair<std::shared_ptr<EncounterLog>, EncounterLogData>> all_log_data;

		if (ImGui::BeginTable("Logs Table", column_count, ImGuiTableFlags_BordersH))
		{
			static bool select_all_toggle = false;
			bool select_all = false;
			bool all_selected = true;

			auto get_column_width = [](const char* text) -> float // please forgive me
				{
					return ImGui::CalcTextSize(text).x + 2.f;
				};

			ImGui::TableSetupColumn("##select_all", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight());
			ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, get_column_width("00:00"));
			ImGui::TableSetupColumn("Encounter", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeightWithSpacing() / 2 + get_column_width("100.000%"));
			ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, get_column_width("00m 00s 000ms"));
			if (!this->settings.display.hide_elite_insights)
				ImGui::TableSetupColumn("Report", ImGuiTableColumnFlags_WidthFixed, get_column_width("XXXXXXXXXXX"));
			if (!this->settings.display.hide_dps_report)
				ImGui::TableSetupColumn("dps.report", ImGuiTableColumnFlags_WidthFixed, get_column_width("XXXXXXXXXXX"));
			if (!this->settings.display.hide_wingman)
				ImGui::TableSetupColumn("Wingman", ImGuiTableColumnFlags_WidthFixed, get_column_width("XXXXXXXXXXX"));

			ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
			for (int column = LogTableColumns::SELECT; column < column_count; column++)
			{
				ImGui::TableSetColumnIndex(column);
				ImGui::PushID(&column);
				if (column == LogTableColumns::SELECT)
				{
					if (!this->encounter_logs.empty())
						if (ImGui::SmallCheckbox("##select_all", &select_all_toggle))
							select_all = true;
				}
				else
				{
					if (column == LogTableColumns::REPORT || column == LogTableColumns::DPS_REPORT || column == LogTableColumns::WINGMAN)
						ImGui::CenterNextItemHorizontally(ImGui::TableGetColumnName(column));

					ImGui::TextUnformatted(ImGui::TableGetColumnName(column));
				}
				ImGui::PopID();
			}

			for (auto [encounter_log, encounter_log_data] : this->encounter_logs)
			{
				ImGui::ID log_id(encounter_log_data.id);

				if (select_all)
					encounter_log_data.view.selected = select_all_toggle;

				if (!encounter_log_data.view.selected)
					all_selected = false;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				if (ImGui::SmallCheckbox("##select", &encounter_log_data.view.selected) || select_all)
				{
					std::unique_lock lock(encounter_log->mutex);
					encounter_log->view.selected = encounter_log_data.view.selected;
				}

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(encounter_log_data.view.time.c_str());
				if (ImGui::IsItemHovered())
				{
					auto timestamp_from_timepoint = [](const std::chrono::system_clock::time_point& timepoint)
						{

							auto sys_time = std::chrono::clock_cast<std::chrono::system_clock>(timepoint);
							auto sys_time_trunc = std::chrono::floor<std::chrono::seconds>(sys_time);

							std::chrono::zoned_time local_time{ std::chrono::current_zone(), sys_time_trunc };

							std::string formatted_time = std::format("{:%d %B %Y, %H:%M:%S}", local_time);

							// Format timestamp
							std::ostringstream timestamp_stream;

							timestamp_stream << formatted_time;

							// Get the current time
							auto now = std::chrono::system_clock::now();

							// Calculate difference
							auto diff = now - timepoint;

							// Components of the difference
							std::vector<std::pair<int64_t, std::string>> components = {
								{std::chrono::duration_cast<std::chrono::years>(diff).count(), "y"},
								{std::chrono::duration_cast<std::chrono::months>(diff % std::chrono::years(1)).count(), "M"},
								{std::chrono::duration_cast<std::chrono::days>(diff % std::chrono::months(1)).count(), "d"},
								{std::chrono::duration_cast<std::chrono::hours>(diff % std::chrono::days(1)).count(), "h"},
								{std::chrono::duration_cast<std::chrono::minutes>(diff % std::chrono::hours(1)).count(), "m"},
								{std::chrono::duration_cast<std::chrono::seconds>(diff % std::chrono::minutes(1)).count(), "s"}
							};

							// Build dynamic "X ago" string
							std::ostringstream ago_stream;
							bool first = true;
							for (const auto& [value, unit] : components)
							{
								if (value > 0)
								{
									ago_stream << value << unit + " ";
									first = false;
								}
							}

							if (first)
								ago_stream << "now";
							else
								ago_stream << "ago";

							// Combine and return the result
							return timestamp_stream.str() + " (" + ago_stream.str() + ")";
						};

					ImGui::BeginTooltip();
					ImGui::TextUnformatted(timestamp_from_timepoint(encounter_log_data.parse_status != ParseStatus::PARSED ? encounter_log_data.evtc_data.time : encounter_log_data.encounter_data.end_time).c_str());
					ImGui::EndTooltip();
				}
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(encounter_log_data.view.name.c_str());
				ImGui::TableNextColumn();
				if (encounter_log_data.parse_status == ParseStatus::PARSED)
					ImGui::Indicator(encounter_log_data.encounter_data.success ? Color::Green : Color::Red);
				else
					ImGui::Indicator(Color::Gray);
				ImGui::SameLine();
				ImGui::TextUnformatted(encounter_log_data.view.result.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(encounter_log_data.view.duration.c_str());

				if (!this->settings.display.hide_elite_insights)
				{
					ImGui::TableNextColumn();
					if (ImGui::ButtonParser(encounter_log_data.parse_status))
					{
						if (encounter_log_data.parse_status == ParseStatus::UNPARSED)
							global::elite_insights->queue_encounter_log(encounter_log);
						else if (encounter_log_data.parse_status == ParseStatus::PARSED)
							ShellExecute(nullptr, L"open", encounter_log_data.report_data.html_file_path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
					}
					if (encounter_log_data.report_data.error_message.has_value())
						ImGui::DelayedTooltipText(encounter_log_data.report_data.error_message.value().c_str());
				}

				if (!this->settings.display.hide_dps_report)
				{
					ImGui::TableNextColumn();
					if (ImGui::ButtonDpsReport(encounter_log_data.dps_report_upload.status))
					{
						if (encounter_log_data.dps_report_upload.status == DpsReportUploadStatus::AVAILABLE || encounter_log_data.dps_report_upload.status == DpsReportUploadStatus::FAILED)
							global::dps_report_uploader->queue_upload(encounter_log);
						else if (encounter_log_data.dps_report_upload.status == DpsReportUploadStatus::UPLOADED && !encounter_log_data.dps_report_upload.url.empty())
							ShellExecuteA(nullptr, "open", encounter_log_data.dps_report_upload.url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
					}
					if (encounter_log_data.dps_report_upload.error_message.has_value())
						ImGui::DelayedTooltipText(encounter_log_data.dps_report_upload.error_message.value().c_str());
				}

				if (!this->settings.display.hide_wingman)
				{
					ImGui::TableNextColumn();
					if (ImGui::ButtonWingman(encounter_log_data.wingman_upload.status, encounter_log_data.parse_status))
					{
						if (encounter_log_data.wingman_upload.status == WingmanUploadStatus::AVAILABLE && encounter_log_data.parse_status == ParseStatus::PARSED)
							global::wingman_uploader->queue_upload(encounter_log);
					}
					if (encounter_log_data.wingman_upload.error_message.has_value())
						ImGui::DelayedTooltipText(encounter_log_data.wingman_upload.error_message.value().c_str());
				}
			}

			select_all_toggle = all_selected;
		}
		ImGui::EndTable();

		this->draw_context_menu();
	}

	if (hide_background)
		ImGui::PopStyleColor(2);

	ImGui::PopStyleVar();
	ImGui::End();
}

void UI::draw_context_menu()
{
	ImGui::ID id("Context Menu");

	auto bg_color = ImGui::GetStyle().Colors[ImGuiCol_PopupBg];
	bg_color.w = max(bg_color.w, 0.85f);

	ImGui::PushStyleColor(ImGuiCol_PopupBg, bg_color);

	if (ImGui::BeginPopupContextWindow("Context Menu"))
	{
		ImGui::TextDisabled("Log actions");
		ImGui::Spacing();

		struct LogSelection
		{
			enum Type
			{
				LAST,
				SELECTED,
				ALL,
				_COUNT
			};
		};

		for (int ctx = 0; ctx < LogSelection::_COUNT; ctx++)
		{
			std::deque<std::tuple<std::shared_ptr<EncounterLog>, EncounterLogData>>* logs = nullptr;
			std::string menu_label;

			switch (ctx)
			{
			case LogSelection::LAST:
				if (!this->encounter_logs.empty())
				{
					static std::deque<std::tuple<std::shared_ptr<EncounterLog>, EncounterLogData>> last_log_deq;
					last_log_deq.clear();
					last_log_deq.push_back(this->encounter_logs.front());
					logs = &last_log_deq;
				}
				menu_label = "Last log";
				break;

			case LogSelection::SELECTED:
			{
				static std::deque<std::tuple<std::shared_ptr<EncounterLog>, EncounterLogData>> selected_logs;
				selected_logs.clear();
				for (auto& [encounter_log, encounter_log_data] : this->encounter_logs)
				{
					if (encounter_log_data.view.selected)
					{
						selected_logs.push_back(std::make_tuple(encounter_log, encounter_log_data));
					}
				}
				logs = &selected_logs;
				menu_label = "Selected logs";
				break;
			}

			case LogSelection::ALL:
				logs = &this->encounter_logs;
				menu_label = "All logs";
				break;

			default:
				break;
			}

			if (logs && !logs->empty() && !(ctx == LogSelection::LAST))
			{
				menu_label += " (" + std::to_string(logs->size()) + ")";
			}

			if (ImGui::BeginMenu(menu_label.c_str()))
			{
				if (logs && !logs->empty())
				{
					struct LogAction
					{
						enum Type
						{
							PARSE,
							OPEN_REPORTS,
							UPLOAD_TO_DPS_REPORT,
							COPY_DPS_REPORT_URLS,
							UPLOAD_TO_WINGMAN,
							_COUNT
						};
					};

					std::array<std::deque<std::tuple<std::shared_ptr<EncounterLog>, EncounterLogData>>, LogAction::_COUNT> action_logs;

					for (auto& [encounter_log, encounter_log_data] : *logs)
					{
						if (encounter_log_data.parse_status == ParseStatus::PARSED)
						{
							// Open Reports
							action_logs[LogAction::OPEN_REPORTS].emplace_back(encounter_log, encounter_log_data);

							// Upload to Wingman
							if (encounter_log_data.wingman_upload.status == WingmanUploadStatus::AVAILABLE ||
								encounter_log_data.wingman_upload.status == WingmanUploadStatus::FAILED)
							{
								action_logs[LogAction::UPLOAD_TO_WINGMAN].emplace_back(encounter_log, encounter_log_data);
							}
						}
						else if (encounter_log_data.parse_status == ParseStatus::UNPARSED)
						{
							// Parse
							action_logs[LogAction::PARSE].emplace_back(encounter_log, encounter_log_data);
						}

						// Upload to dps.report
						if (encounter_log_data.dps_report_upload.status == DpsReportUploadStatus::AVAILABLE ||
							encounter_log_data.dps_report_upload.status == DpsReportUploadStatus::FAILED)
						{
							action_logs[LogAction::UPLOAD_TO_DPS_REPORT].emplace_back(encounter_log, encounter_log_data);
						}

						// Copy dps.report URLs
						if (encounter_log_data.dps_report_upload.status == DpsReportUploadStatus::UPLOADED &&
							!encounter_log_data.dps_report_upload.url.empty())
						{
							action_logs[LogAction::COPY_DPS_REPORT_URLS].emplace_back(encounter_log, encounter_log_data);
						}
					}

					bool options_available = false;

					// Parse
					if (!action_logs[LogAction::PARSE].empty())
					{
						options_available = true;
						if (ImGui::MenuItem(("Parse (" + std::to_string(action_logs[LogAction::PARSE].size()) + ")").c_str()))
						{
							for (auto& [encounter_log, _] : action_logs[LogAction::PARSE])
							{
								global::elite_insights->queue_encounter_log(encounter_log);
							}
						}
					}

					// Open Reports
					if (!action_logs[LogAction::OPEN_REPORTS].empty())
					{
						options_available = true;
						if (ImGui::MenuItem(("Open reports (" + std::to_string(action_logs[LogAction::OPEN_REPORTS].size()) + ")").c_str()))
						{
							for (auto& [_, encounter_log_data] : action_logs[LogAction::OPEN_REPORTS])
							{
								ShellExecuteW(nullptr, L"open", encounter_log_data.report_data.html_file_path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
							}
						}
					}

					// Upload to dps.report
					if (!action_logs[LogAction::UPLOAD_TO_DPS_REPORT].empty())
					{
						options_available = true;
						if (ImGui::MenuItem(("Upload to dps.report (" + std::to_string(action_logs[LogAction::UPLOAD_TO_DPS_REPORT].size()) + ")").c_str()))
						{
							for (auto& [encounter_log, _] : action_logs[LogAction::UPLOAD_TO_DPS_REPORT])
							{
								global::dps_report_uploader->queue_upload(encounter_log);
							}
						}
					}

					// Copy dps.report URLs
					if (!action_logs[LogAction::COPY_DPS_REPORT_URLS].empty())
					{
						options_available = true;
						if (ImGui::BeginMenu(("Copy dps.report URLs (" + std::to_string(action_logs[LogAction::COPY_DPS_REPORT_URLS].size()) + ")").c_str()))
						{
							if (ImGui::MenuItem("As raw list"))
							{
								std::string urls;
								for (auto& [_, encounter_log_data] : action_logs[LogAction::COPY_DPS_REPORT_URLS])
								{
									urls += encounter_log_data.dps_report_upload.url + "\n";
								}
								if (!urls.empty())
								{
									ImGui::SetClipboardText(urls.c_str());
								}
							}

							if (ImGui::MenuItem("As markdown links"))
							{
								std::stringstream ss;
								for (auto& [_, encounter_log_data] : action_logs[LogAction::COPY_DPS_REPORT_URLS])
								{
									if (encounter_log_data.parse_status == ParseStatus::PARSED)
										ss << "[" << encounter_log_data.view.name << " (" << encounter_log_data.view.duration
										<< (!encounter_log_data.encounter_data.success ? " | " + (encounter_log_data.encounter_data.valid_boss ? std::format("{:.2f}%", 100.f - encounter_log_data.encounter_data.health_percent_burned) + " left" : "failure") : "")
										<< ")](" << encounter_log_data.dps_report_upload.url << ")"
										<< "\n";
									else
										ss << "[" << encounter_log_data.view.name << "](" << encounter_log_data.dps_report_upload.url << ")\n";
								}
								ImGui::SetClipboardText(ss.str().c_str());
							}
							ImGui::EndMenu();
						}
					}

					// Upload to Wingman
					if (!action_logs[LogAction::UPLOAD_TO_WINGMAN].empty())
					{
						options_available = true;
						if (ImGui::MenuItem(("Upload to Wingman (" + std::to_string(action_logs[LogAction::UPLOAD_TO_WINGMAN].size()) + ")").c_str()))
						{
							for (auto& [encounter_log, _] : action_logs[LogAction::UPLOAD_TO_WINGMAN])
							{
								global::wingman_uploader->queue_upload(encounter_log);
							}
						}
					}

					if (!options_available)
					{
						ImGui::TextDisabled("No options available");
					}
				}
				else
				{
					const char* text = "No logs available";
					if (ctx == LogSelection::SELECTED)
					{
						text = "No logs selected";
					}
					ImGui::TextDisabled(text);
				}
				ImGui::EndMenu();
			}
		}
		ImGui::Separator();

		ImGui::TextDisabled("Log uploader options");
		ImGui::Spacing();

		if (ImGui::BeginMenu("Display"))
		{
			this->draw_display_settings();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Parser"))
		{
			this->draw_parser_settings();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("dps.report"))
		{
			this->draw_dps_report_settings();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Wingman"))
		{
			this->draw_wingman_settings();
			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor();
}

void UI::draw_display_settings()
{
	ImGui::ID id("Display Settings");

	ImGui::TextDisabled("Display options");
	ImGui::Spacing();
	UI_ELEMENT(ImGui::KeySelector, "Hotkey", display.hotkey);
	// Window Width
	auto window_size_x = static_cast<int>(this->settings.display.window_size.x);
	if (ImGui::SliderInt("Window width", &window_size_x, 200, 1000, "%d px", ImGuiSliderFlags_AlwaysClamp))
	{
		this->settings.display.window_size.x = static_cast<float>(window_size_x);
		SAVE_SETTING(display.window_size.x);
	}
	// Window Height
	auto window_size_y = static_cast<int>(this->settings.display.window_size.y);
	if (ImGui::SliderInt("Window height", &window_size_y, 100, 1000, "%d px", ImGuiSliderFlags_AlwaysClamp))
	{
		this->settings.display.window_size.y = static_cast<float>(window_size_y);
		SAVE_SETTING(display.window_size.y);
	}
	UI_ELEMENT(ImGui::Checkbox, "Hide title bar", display.hide_title_bar);
	UI_ELEMENT(ImGui::Checkbox, "Hide background", display.hide_background);
	UI_ELEMENT(ImGui::Checkbox, "Hide in combat", display.hide_in_combat);
	UI_ELEMENT(ImGui::Checkbox, "Clip to screen", display.clip_to_screen);

	ImGui::SmallSpacing();

	ImGui::TextDisabled("Log table options");
	UI_ELEMENT(ImGui::Checkbox, "Hide report", display.hide_elite_insights);
	UI_ELEMENT(ImGui::Checkbox, "Hide dps.report", display.hide_dps_report);
	UI_ELEMENT(ImGui::Checkbox, "Hide Wingman", display.hide_wingman);
}

void UI::draw_dps_report_settings()
{
	ImGui::ID id("Dps Report Settings");

	ImGui::TextDisabled("dps.report settings");
	ImGui::Spacing();
	UI_ELEMENT(ImGui::Checkbox, "Auto upload", dps_report.auto_upload);
	ImGui::DelayedTooltipText("Automatically uploads new .evtc files to dps report.");
	if (ImGui::BeginMenu("Auto upload encounters"))
	{
		UI_ELEMENT(ImGui::EncounterSelector, "dps.report auto upload encounter filter", dps_report.auto_upload_encounters)
			ImGui::EndMenu();
	}
	ImGui::DelayedTooltipText("Select encounters that should be uploaded automatically.");
	const char* items[] = { "All", "Successful only" };
	if (ImGui::Combo("Auto upload filter", reinterpret_cast<int*>(&this->settings.dps_report.auto_upload_filter), items, IM_ARRAYSIZE(items)))
	{
		SAVE_SETTING(dps_report.auto_upload_filter);
	}
	ImGui::DelayedTooltipText("Additional filters applied before auto uploading.");
	UI_ELEMENT(ImGui::Checkbox, "Auto copy url to clipboard", dps_report.copy_to_clipboard);
	ImGui::DelayedTooltipText("Automatically copy the link of the last uploaded log to the clipboard.");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	char dps_report_user_token[32 + 1] = {};
	strncpy_s(dps_report_user_token, this->settings.dps_report.user_token.c_str(), sizeof(dps_report_user_token) - 1);
	static bool toggle_password = false;
	if (ImGui::InputText("User token", dps_report_user_token, sizeof(dps_report_user_token), !toggle_password ? ImGuiInputTextFlags_Password : 0))
	{
		std::string user_token = dps_report_user_token;

		if (user_token.length() == 0 || user_token.length() == 32)
		{
			this->settings.dps_report.user_token = user_token;
			SAVE_SETTING(dps_report.user_token);
		}
		ImGui::DelayedTooltipText("Your personal dps.report user token. Will be automatically aquired on first upload if not specified.");
	}
	ImGui::SameLine();
	if (ImGui::Checkbox("##show_user_token", &toggle_password))
	{
		LOG("toggle", LogLevel::Info);
	}
	ImGui::DelayedTooltipText("Reveal user token");

	UI_ELEMENT(ImGui::Checkbox, "Anonymize", dps_report.anonymize);
	ImGui::DelayedTooltipText("Player names will be anonymized.");
	UI_ELEMENT(ImGui::Checkbox, "Detailed WvW", dps_report.detailed_wvw);
	ImGui::DelayedTooltipText("Enable detailed WvW reports. This may break and return a 500 error with particularly long logs.");
}

void UI::draw_wingman_settings()
{
	ImGui::ID id("Wingman Settings");

	ImGui::TextDisabled("Wingman settings");
	ImGui::Spacing();
	UI_ELEMENT(ImGui::Checkbox, "Auto upload", wingman.auto_upload);
	ImGui::DelayedTooltipText("Automatically uploads parsed logs to Wingman.");
	if (ImGui::BeginMenu("Auto upload encounters"))
	{
		UI_ELEMENT(ImGui::EncounterSelector, "Wingman auto upload encounter filter", wingman.auto_upload_encounters)
			ImGui::EndMenu();
	}
	ImGui::DelayedTooltipText("Select encounters that should be uploaded automatically.");
	const char* items[] = { "All", "Successful only" };
	if (ImGui::Combo("Auto upload filter", reinterpret_cast<int*>(&this->settings.wingman.auto_upload_filter), items, IM_ARRAYSIZE(items)))
	{
		SAVE_SETTING(wingman.auto_upload_filter);
	}
	ImGui::DelayedTooltipText("Additional filters applied before auto uploading.");
}

void UI::draw_parser_settings()
{
	ImGui::ID id("Parser Settings");

	ImGui::TextDisabled("Parser settings");
	ImGui::Spacing();
	UI_ELEMENT(ImGui::Checkbox, "Auto parse", elite_insights.auto_parse);
	ImGui::DelayedTooltipText("Automatically parse new logs with Elite Insights.");

	UI_ELEMENT(ImGui::Checkbox, "Auto update parser", elite_insights.auto_update);
	ImGui::DelayedTooltipText("Automatically update Elite Insights on launch if a new version is available.");

	const char* items[] = { "Latest", "Latest Wingman supported" };
	if (ImGui::Combo("Parser update channel", reinterpret_cast<int*>(&this->settings.elite_insights.update_channel), items, IM_ARRAYSIZE(items)))
	{
		SAVE_SETTING(elite_insights.update_channel);
	}
	ImGui::DelayedTooltipText("Specifies the target Elite Insights version. Sometimes Wingman does not support the latest version right away.");
}