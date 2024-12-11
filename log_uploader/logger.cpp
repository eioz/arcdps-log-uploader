#include "logger.h"

#include <iostream>

namespace global { std::unique_ptr<Logger> logger = std::make_unique<Logger>(); }

void Logger::initialize(std::filesystem::path log_file_path, HANDLE arcdps_handle)
{
	std::lock_guard lock(this->initialization_mutex);

	if (this->is_initialized())
		return;

	if (log_file_path.empty())
		throw std::runtime_error("Log file path is empty");

	if (!std::filesystem::exists(log_file_path.parent_path()))
		std::filesystem::create_directories(log_file_path.parent_path());

	this->log_file = std::ofstream(log_file_path, std::ofstream::out | std::ofstream::trunc);

	if (!this->log_file.is_open())
		throw std::runtime_error("Failed to open log file: " + log_file_path.string());

	this->arcdps_log_function = reinterpret_cast<ArcdpsLogFunctionPtr>(GetProcAddress(reinterpret_cast<HMODULE>(arcdps_handle), "e8"));
	this->arcdps_file_log_function = reinterpret_cast<ArcdpsLogFunctionPtr>(GetProcAddress(reinterpret_cast<HMODULE>(arcdps_handle), "e3"));

	if (!this->arcdps_log_function || !this->arcdps_file_log_function)
		throw std::runtime_error("Failed to get arcdps log functions");

	this->message_handler_thread = std::thread(&Logger::write_thread, this);

	this->initialized.store(true);
}

void Logger::release()
{
	std::lock_guard lock(this->initialization_mutex);

	this->initialized.store(false);

	this->message_queue_cv.notify_all();

	if (this->message_handler_thread.joinable())
		this->message_handler_thread.join();
}

void Logger::write(const std::string& message, LogLevel level, LogSource source)
{
#ifndef _DEBUG
	if (level == LogLevel::Debug)
		return;
#endif // !_DEBUG

	if (!this->is_initialized())
		return;

	{
		std::lock_guard<std::mutex> lock(this->message_queue_mutex);
		this->message_queue.push({ level, source, message });
	}

	this->message_queue_cv.notify_one();
}

void Logger::write_thread()
{
	while (true)
	{
		std::unique_lock<std::mutex> lock(this->message_queue_mutex);

		this->message_queue_cv.wait(lock, [this]
			{
				return !this->message_queue.empty() || !this->is_initialized();
			}
		);

		if (!this->is_initialized() && this->message_queue.empty())
			break;

		auto log_message = this->message_queue.front();
		this->message_queue.pop();

		lock.unlock();

#ifdef _DEBUG
		std::cout << this->format_log_message(log_message) << std::endl;
#endif // _DEBUG

		if (this->arcdps_log_function)
			this->arcdps_log_function(const_cast<char*>(this->format_log_message_arc(log_message).c_str()));

		if (log_message.level == LogLevel::Error && this->arcdps_file_log_function)
			this->arcdps_file_log_function(const_cast<char*>(this->format_log_message_arc_file(log_message).c_str()));

		if (this->log_file.is_open())
			this->log_file << this->format_log_message(log_message) << std::endl;

		this->log_file.flush();
	}
}
