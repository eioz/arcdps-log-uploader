#pragma once

#include "module.h"
#include "encounter_log.h"
#include "settings.h"

#include <string>
#include <regex>
#include <sstream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>

#include <cpr/cpr.h>

class EliteInsightsVersion
{
private:
	int v1 = 0, v2 = 0, v3 = 0, v4 = 0;
	bool valid = false;
public:
	std::string download_url;
	std::string tag_name;

	EliteInsightsVersion() : v1(0), v2(0), v3(0), v4(0), valid(false) {}
	EliteInsightsVersion(std::string tag_name, const std::string& download_url = "")
	{
		this->tag_name = tag_name;
		this->download_url = download_url;

		tag_name = tag_name.starts_with("v.") ? "v" + tag_name.substr(2) : tag_name; // fix for inconsistent tag names

		auto pos = tag_name.find("v");

		if (pos != std::string::npos)
		{
			std::string version_str = tag_name.substr(pos + 1);
			std::replace(version_str.begin(), version_str.end(), '.', ' ');
			std::istringstream iss(version_str);
			iss >> v1 >> v2 >> v3 >> v4;

			this->valid = !iss.fail() && std::regex_match(tag_name, std::regex("^v(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)$"));
		}
	}

	bool is_valid() const
	{
		return this->valid;
	}

	const std::string get_tag() const { return this->tag_name; /*return "v" + std::to_string(v1) + "." + std::to_string(v2) + "." + std::to_string(v3) + "." + std::to_string(v4);*/ }

	bool operator==(const EliteInsightsVersion& other) const
	{
		return v1 == other.v1 && v2 == other.v2 && v3 == other.v3 && v4 == other.v4;
	}

	bool operator!=(const EliteInsightsVersion& other) const
	{
		return !(*this == other);
	}

	bool operator<(const EliteInsightsVersion& other) const
	{
		return std::tie(v1, v2, v3, v4) < std::tie(other.v1, other.v2, other.v3, other.v4);
	}

	bool operator>(const EliteInsightsVersion& other) const
	{
		return other < *this;
	}

	bool operator<=(const EliteInsightsVersion& other) const
	{
		return !(other < *this);
	}

	bool operator>=(const EliteInsightsVersion& other) const
	{
		return !(*this < other);
	}
};

class EliteInsights : public Module
{
public:
	bool initialize(std::filesystem::path installation_directory, std::filesystem::path output_directory);
	void release() override;

	void queue_encounter_log(std::shared_ptr<EncounterLog> encounter_log);
	void process_auto_parse(std::shared_ptr<EncounterLog> encounter_log);
private:

	std::filesystem::path installation_directory;
	std::filesystem::path output_directory;
	
	std::filesystem::path executable_file;
	std::filesystem::path settings_file;
	std::filesystem::path version_file;

	cpr::Timeout request_timeout = cpr::Timeout{ std::chrono::seconds(30) };

	auto is_installed() -> bool
	{
		return std::filesystem::exists(this->executable_file) && std::filesystem::exists(this->settings_file) && (this->local_version.is_valid() || this->refresh_local_version());
	}

	std::mutex parser_mutex;
	std::condition_variable_any parser_cv;
	std::mutex parser_queue_mutex;
	std::queue<std::shared_ptr<EncounterLog>> parser_queue;
	
	std::thread parser_thread;

	EliteInsightsVersion local_version = {};
	EliteInsightsVersion latest_version = {};
	EliteInsightsVersion latest_version_wingman = {};

	bool refresh_local_version();
	bool refresh_latest_version();
	bool refresh_latest_version_wingman();

	bool update(EliteInsightsUpdateChannel version);

	bool set_version(const EliteInsightsVersion version);
	bool write_parser_settings();

	void run_parser();

	ParseStatus parse(std::filesystem::path evtc_file_path, EncounterData& encounter_data, ReportData& report_data);
};

namespace global { extern std::unique_ptr<EliteInsights> elite_insights; }
