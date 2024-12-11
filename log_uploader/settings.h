#pragma once

#include "evtc.h"
#include "module.h"

#include "../imgui/imgui.h"

#include <string>
#include <shared_mutex>
#include <nlohmann/json.hpp>

struct Hotkey
{
	int key = 0;

	bool ctrl = false;
	bool alt = false;
	bool shift = false;

	auto reset() { this->key = 0; this->ctrl = this->shift = this->alt = false; }

	friend void to_json(nlohmann::json& j, const Hotkey& h)
	{
		j = nlohmann::json{ {"key", h.key}, {"ctrl", h.ctrl}, {"alt", h.alt}, {"shift", h.shift} };
	}

	friend void from_json(const nlohmann::json& j, Hotkey& h)
	{
		j.at("key").get_to(h.key);
		j.at("ctrl").get_to(h.ctrl);
		j.at("alt").get_to(h.alt);
		j.at("shift").get_to(h.shift);
	}
};

inline void to_json(nlohmann::json& j, const ImVec2& v)
{
	j = nlohmann::json{ {"x", v.x}, {"y", v.y} };
};

inline void from_json(const nlohmann::json& j, ImVec2& v)
{
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
};

enum class EliteInsightsUpdateChannel
{
	LATEST,
	LATEST_WINGMAN
};

enum class AutoUploadFilter
{
	ALL,
	SUCCESSFUL_ONLY
};

using EncounterSelection = std::vector<TriggerID>;

struct UploaderSettings
{
	struct DpsReport
	{
		bool auto_upload = false;
		bool copy_to_clipboard = false;
		std::string user_token = "";
		bool anonymize = false;
		bool detailed_wvw = false;

		AutoUploadFilter auto_upload_filter = AutoUploadFilter::ALL;
		EncounterSelection auto_upload_encounters;

		// internal
		int request_timeout = 180000;

		auto set_user_token(std::string token) { if (token.length() == 0 || token.length() == 32) this->user_token = token; }

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(DpsReport, auto_upload, copy_to_clipboard, user_token, anonymize, detailed_wvw, auto_upload_encounters, request_timeout)
	} dps_report;

	struct Wingman
	{
		bool auto_upload = false;
		AutoUploadFilter auto_upload_filter = AutoUploadFilter::SUCCESSFUL_ONLY;
		EncounterSelection auto_upload_encounters;

		// internal
		int request_timeout = 180000;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Wingman, auto_upload, auto_upload_filter, auto_upload_encounters, request_timeout)
	} wingman;

	struct EliteInsights
	{
		bool auto_update = true;
		EliteInsightsUpdateChannel update_channel = EliteInsightsUpdateChannel::LATEST_WINGMAN;

		bool auto_parse = true;

		int request_timeout = 180000; // temporary ...

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(EliteInsights, auto_update, update_channel, auto_parse)
	} elite_insights;

	struct Display
	{
		Hotkey hotkey = Hotkey();

		ImVec2 window_size = ImVec2(800, 350);
		auto set_window_size(ImVec2 size)
		{
			size.x = std::clamp(size.x, 200.0f, 3840.0f);
			size.y = std::clamp(size.y, 200.0f, 2160.0f);

			this->window_size = size;
		}

		bool hide_title_bar = false;
		bool hide_background = false;

		bool hide_elite_insights = false;
		bool hide_dps_report = false;
		bool hide_wingman = false;
		bool hide_in_combat = false;

		bool clip_to_screen = false;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Display, hotkey, window_size, hide_title_bar, hide_background, hide_elite_insights, hide_dps_report, hide_wingman, hide_in_combat, clip_to_screen)
	} display;

	void verify()
	{
#define VERIFY_SETTING(path, setting) this->path.set_##setting(this->path.setting)

		VERIFY_SETTING(dps_report, user_token);
		VERIFY_SETTING(display, window_size);

#undef VERIFY_SETTING
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(UploaderSettings, dps_report, wingman, elite_insights, display)
};

class Settings : public Module
{
public:
	void initialize(std::filesystem::path settings_file_path);
	
	auto release() -> void override
	{
		std::lock_guard initialization_lock(this->initialization_mutex);

		this->initialized.store(false);

		std::unique_lock data_lock(this->mutex);

		this->save();
	}

	auto verify() -> void
	{
		std::unique_lock lock(this->mutex);
		this->settings.verify();
	}

	template<typename Func>
	auto read(Func&& func) const -> decltype(func(std::declval<const UploaderSettings&>()))
	{
		std::shared_lock lock(this->mutex);
		return func(settings);
	}

	template<typename Func>
	auto write(Func&& func) -> decltype(func(std::declval<UploaderSettings&>()))
	{
		std::unique_lock lock(this->mutex);
		return func(settings);
	}

	auto get() const -> UploaderSettings
	{
		std::shared_lock lock(this->mutex);
		return settings;
	}

private:	
	std::filesystem::path settings_file_path;

	mutable std::shared_mutex mutex;
	UploaderSettings settings;

	bool load();
	bool save();
};

namespace global { extern std::unique_ptr<Settings> settings; }

#define GET_SETTING(path) \
    ([]() -> decltype(auto) { \
        auto _s = global::settings->read([](const auto& s) -> decltype(auto) { return s.path; }); \
        return _s; \
    }())

#define SET_SETTING(path, value) \
    do { \
        global::settings->write([&](auto& s) { s.path = value; }); \
    } while (0)
