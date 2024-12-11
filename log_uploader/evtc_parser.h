#pragma once

#include "evtc.h"

#include <filesystem>
#include <chrono>
#include <memory>

class EVTCData
{
public:
	std::filesystem::path evtc_file_path;
	std::chrono::system_clock::time_point time;
	TriggerID trigger_id = TriggerID::Invalid;
};

class EVTCParser
{
public:
	EVTCData parse(const std::filesystem::path& evtc_file_path);
};

namespace global { extern std::unique_ptr<EVTCParser> evtc_parser; }
