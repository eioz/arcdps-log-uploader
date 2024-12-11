#pragma once

#include "module.h"
#include "settings.h"
#include "encounter_log.h"

#include <memory>
#include <string>
#include <tuple>
#include <atomic>
#include <deque>

class UI : public Module
{
public:
	UI() {}
	~UI() {}

	void initialize();
	void draw();
	void draw_arc_window_options();
	void draw_arc_mod_options();
	
	void on_open_hotkey();

private:
	std::atomic<bool> open = false;
	std::atomic<bool> force_open = false; // in combat override

	std::deque<std::tuple<std::shared_ptr<EncounterLog>, EncounterLogData>> encounter_logs;

	UploaderSettings settings;

	void refresh_settings_cache();
	void refresh_encounter_logs();
	
	void draw_main_window();
	void draw_context_menu();

	void draw_display_settings();
	void draw_dps_report_settings();
	void draw_wingman_settings();
	void draw_parser_settings();

	enum LogTableColumns : int
	{
		SELECT,
		TIME,
		ENCOUNTER,
		RESULT,
		DURATION,
		REPORT,
		DPS_REPORT,
		WINGMAN,
		_COUNT
	};
};

namespace global { extern std::unique_ptr<UI> ui; }