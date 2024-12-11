#include "dps_report_uploader.h"
#include "elite_insights.h"
#include "log_manager.h"
#include "logger.h"

namespace global { std::unique_ptr<LogManager> log_manager = std::make_unique<LogManager>(); }

#define LOG(message, log_level) global::logger->write(message, log_level, LogSource::LogManager)

void LogManager::add_encounter_log(EVTCData evtc_data)
{
	auto encounter_log = std::make_shared<EncounterLog>(evtc_data);

	auto id = encounter_log->id;

	global::dps_report_uploader->process_auto_upload(encounter_log);
	global::elite_insights->process_auto_parse(encounter_log);

	{
		std::unique_lock lock(this->encounter_logs_mutex);
		this->encounter_logs.push_front(encounter_log);
	}

	LOG("Added encounter log: " + id, LogLevel::Info);
}

#undef LOG