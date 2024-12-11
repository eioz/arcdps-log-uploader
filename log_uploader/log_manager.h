#pragma once

#include "encounter_log.h"
#include "evtc_parser.h"

#include <deque>
#include <memory>

class LogManager
{
public:
	LogManager() {}
	~LogManager() {}

	auto get_encounter_logs() -> std::deque<std::shared_ptr<EncounterLog>> 
	{
		std::shared_lock lock(this->encounter_logs_mutex);

		return this->encounter_logs; 
	}

	auto clear_encounter_logs() -> void
	{
		std::unique_lock lock(this->encounter_logs_mutex);
		this->encounter_logs.clear();
	}

	void add_encounter_log(EVTCData evtc_data);

private:
	std::shared_mutex encounter_logs_mutex;
	std::deque<std::shared_ptr<EncounterLog>> encounter_logs;
};

namespace global { extern std::unique_ptr<LogManager> log_manager; }