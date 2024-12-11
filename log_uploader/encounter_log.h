#pragma once

#include "evtc_parser.h"

#include <filesystem>
#include <shared_mutex>
#include <string>
#include <chrono>
#include <mutex>
#include <optional>
#include <map>

enum EncounterType
{
	UNKNOWN = 0,
	RAID = 1,
	RAID_EVENT = 2,
	FRACTAL = 3,
	STRIKE_MISSION = 4,
	WORLD_VS_WORLD = 5,
	TRAINING_AREA = 6
};

enum class ParseStatus
{
	UNPARSED = 0,
	QUEUED = 1,
	PARSING = 2,
	PARSED = 3,
	FAILED = 4
};

enum class EncounterDifficulty
{
	NORMAL_MODE = 0,
	CHALLENGE_MODE = 1,
	LEGENDARY_CHALLENGE_MODE = 2,
	EMBOLDENED_MODE = 3,
	EMBOLDENED_MODE_2 = 4,
	EMBOLDENED_MODE_3 = 5,
	EMBOLDENED_MODE_4 = 6,
	EMBOLDENED_MODE_5 = 7
};

class Upload
{
public:
	std::string url = "";
	std::optional<std::string> error_message;
};

enum class DpsReportUploadStatus
{
	AVAILABLE,
	QUEUED,
	UPLOADING,
	UPLOADED,
	FAILED
};

class DpsReportUpload : public Upload
{
public:
	DpsReportUploadStatus status = DpsReportUploadStatus::AVAILABLE;

	std::string id;
	std::string user_token;
	bool is_auto_upload = false;
};

enum class WingmanUploadStatus
{
	UNAVAILABLE,
	AVAILABLE,
	QUEUED,
	UPLOADING,
	UPLOADED,
	SKIPPED,
	FAILED
};

class WingmanUpload : public Upload
{
public:
	WingmanUploadStatus status = WingmanUploadStatus::AVAILABLE;
};

class EncounterData
{
public:
	std::string encounter_name = "";

	std::string account_name = "";

	int duration_ms = 0;

	bool success = false;

	bool valid_boss = false;

	float health_percent_burned = 0.f;

	EncounterDifficulty difficulty = EncounterDifficulty::NORMAL_MODE;

	std::chrono::system_clock::time_point start_time{};
	std::chrono::system_clock::time_point end_time{};
};

class ReportData
{
public:
	std::filesystem::path html_file_path;
	std::filesystem::path json_file_path;

	std::optional<std::string> error_message;
};

class EncounterLogView
{
public:
	bool selected = false;

	std::string name = "";
	std::string time = "";
	std::string result = "";
	std::string duration = "";
};

using EncounterLogID = std::string;

class EncounterLogData
{
public:
	EncounterLogID id = "";

	EVTCData evtc_data = EVTCData();
	ParseStatus parse_status = ParseStatus::UNPARSED;
	EncounterData encounter_data = EncounterData();
	ReportData report_data = ReportData();
	EncounterLogView view = EncounterLogView();
	DpsReportUpload dps_report_upload = DpsReportUpload();
	WingmanUpload wingman_upload = WingmanUpload();

	void update_view();
};

class EncounterLog : public EncounterLogData
{
public:
	EncounterLog(EVTCData evtc_data);
	~EncounterLog();

	auto get_data() -> EncounterLogData
	{
		std::lock_guard lock(this->mutex);
		return *this;
	}

	auto get_data_locked() -> EncounterLogData
	{
		return *this;
	}

	mutable std::shared_mutex mutex;
};

namespace global
{
	extern const std::map<TriggerID, std::string> trigger_id_encounter_name_map;
}