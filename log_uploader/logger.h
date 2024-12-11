#pragma once

#include "module.h"
#include "arcdps.h"

#include <filesystem>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <fstream>
#include <thread>

enum class LogLevel
{
	Info,
	Warning,
	Error,
	Debug
};

enum class LogSource
{
	Core,
	DirectoryMonitor,
	EVTCParser,
	LogManager,
	Settings,
	EliteInsights,
	DpsReportUploader,
	WingmanUploader,
	UI,
	MumbleLink
};

struct LogMessage
{
	LogLevel level = LogLevel::Info;
	LogSource source = LogSource::Core;
	std::string message;
};

class Logger : public Module
{
public:
	Logger() {}
	~Logger() {}

	void initialize(std::filesystem::path log_file_path, HANDLE arcdps_handle);
	void release() override;

	void write(const std::string& message, LogLevel level = LogLevel::Info, LogSource source = LogSource::Core);

private:
	std::mutex message_queue_mutex;
	std::condition_variable message_queue_cv;
	std::queue<LogMessage> message_queue;

	std::thread message_handler_thread;
	std::ofstream log_file;

	ArcdpsLogFunctionPtr arcdps_log_function = nullptr;
	ArcdpsLogFunctionPtr arcdps_file_log_function = nullptr;

	void write_thread();

	auto get_timestamp() -> std::string
	{
		auto now = std::chrono::system_clock::now();
		auto now_time_t = std::chrono::system_clock::to_time_t(now);
		std::tm now_tm = {};
		localtime_s(&now_tm, &now_time_t);
		std::stringstream ss;
		ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");

		return ss.str();
	};

	auto get_log_level_string(LogLevel level) -> std::string
	{
		switch (level)
		{
		case LogLevel::Info:
			return "Info";
		case LogLevel::Warning:
			return "Warning";
		case LogLevel::Error:
			return "Error";
		case LogLevel::Debug:
			return "Debug";
		default:
			return "Unknown";
		}
	};

	auto get_log_source_string(LogSource source) -> std::string
	{
		switch (source)
		{
		case LogSource::Core:
			return "Core";
		case LogSource::DirectoryMonitor:
			return "Directory Monitor";
		case LogSource::EVTCParser:
			return "EVTC Parser";
		case LogSource::LogManager:
			return "Log Manager";
		case LogSource::Settings:
			return "Settings";
		case LogSource::EliteInsights:
			return "Elite Insights";
		case LogSource::DpsReportUploader:
			return "dps.report Uploader";
		case LogSource::WingmanUploader:
			return "Wingman Uploader";
		case LogSource::UI:
			return "UI";
		case LogSource::MumbleLink:
			return "Mumble Link";
		default:
			return "Unknown";
		}
	};

	auto format_log_message(const LogMessage& log_message) -> std::string
	{
		return "[" + this->get_timestamp() + "] " +
			this->get_log_source_string(log_message.source) + " | " +
			this->get_log_level_string(log_message.level) + ": " +
			log_message.message;
	};

	auto get_log_level_color_code_arc(const LogLevel level) -> std::string
	{
		switch (level)
		{
		case LogLevel::Info:
			return "7F61FF";
		case LogLevel::Warning:
			return "FF7F61";
		case LogLevel::Error:
			return "FF6161";
		case LogLevel::Debug:
		default:
			return "FFFFFF";
		}
	};

	auto format_log_message_arc(const LogMessage& log_message) -> std::string
	{
		auto format_colored = [](const std::string& text, const std::string& color_code) -> std::string
			{
				return "<c=#" + color_code + ">" + text + "</c>";
			};

		return 
			format_colored("[log uploader]", "7F7F7F") + " " +
			format_colored(this->get_log_source_string(log_message.source), "7F7F7F") +
			format_colored(" | ", "7F7F7F") +
			format_colored(this->get_log_level_string(log_message.level), this->get_log_level_color_code_arc(log_message.level)) + ": " + log_message.message;
	};

	auto format_log_message_arc_file(const LogMessage& log_message) -> std::string
	{
		return "[log uploader] " +
			this->get_log_source_string(log_message.source) + " | " +
			this->get_log_level_string(log_message.level) + ": " +
			log_message.message;
	};
};

namespace global { extern std::unique_ptr<Logger> logger; }