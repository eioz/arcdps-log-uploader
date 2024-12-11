#include <atomic>
#include <deque>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>
#include <Windows.h>

#include "directory_monitor.h"
#include "elite_insights.h"
#include "encounter_log.h"
#include "evtc_parser.h"
#include "log_manager.h"
#include "logger.h"

namespace global { std::unique_ptr<DirectoryMonitor> directory_monitor = std::make_unique<DirectoryMonitor>(); }

#define LOG(message, log_level) global::logger->write(message, log_level, LogSource::DirectoryMonitor)

void DirectoryMonitor::initialize(std::filesystem::path monitor_directory)
{
	std::lock_guard lock(this->initialization_mutex);

	if (monitor_directory.empty())
		throw std::invalid_argument("monitor_directory is empty");

	if (this->is_initialized())
	{
		LOG("Already initialized", LogLevel::Debug);
		return;
	}

	this->monitor_directory = monitor_directory;

	this->initialized.store(true);

	this->monitor_thread = std::thread(&DirectoryMonitor::run, this);
}

void DirectoryMonitor::release()
{
	std::lock_guard lock(this->initialization_mutex);

	this->initialized.store(false);

	if (this->monitor_overlapped.hEvent != NULL)
		SetEvent(this->monitor_overlapped.hEvent);

	if (this->monitor_thread.joinable())
		this->monitor_thread.join();

	this->monitor_overlapped = { 0 };
	this->monitor_directory.clear();
}

void DirectoryMonitor::run()
{
	if (!this->is_initialized())
		return;

	if (!std::filesystem::exists(monitor_directory))
	{
		LOG("Monitor directory does not exist: " + monitor_directory.string(), LogLevel::Error);
		return;
	}

	auto logs_directory_handle = CreateFileW(monitor_directory.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

	if (logs_directory_handle == INVALID_HANDLE_VALUE)
	{
		LOG("Failed to open logs directory: " + monitor_directory.string(), LogLevel::Error);
		return;
	}

	std::vector<BYTE> buffer(4096);
	DWORD bytes_returned;

	this->monitor_overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (this->monitor_overlapped.hEvent == NULL)
	{
		LOG("Failed to create event for directory monitoring", LogLevel::Error);
		CloseHandle(logs_directory_handle);
		return;
	}

	LOG("Started monitoring " + monitor_directory.string(), LogLevel::Info);

	while (this->is_initialized())
	{
		auto result = ReadDirectoryChangesW(logs_directory_handle, buffer.data(), static_cast<DWORD>(buffer.size()), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME, &bytes_returned, &this->monitor_overlapped, nullptr);

		if (!result && GetLastError() != ERROR_IO_PENDING)
		{
			LOG("Failed to read logs directory changes", LogLevel::Error);
			break;
		}

		DWORD wait_result = WaitForSingleObject(this->monitor_overlapped.hEvent, INFINITE);

		if (!this->is_initialized() || this->monitor_overlapped.hEvent == NULL)
			break;

		if (wait_result == WAIT_OBJECT_0)
		{
			DWORD bytes_transferred;

			if (GetOverlappedResult(logs_directory_handle, &this->monitor_overlapped, &bytes_transferred, TRUE))
			{
				ResetEvent(this->monitor_overlapped.hEvent);

				auto* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());

				std::deque<std::shared_ptr<EncounterLog>> new_logs;

				do
				{
					if (fni->Action == FILE_ACTION_RENAMED_NEW_NAME)
					{
						auto file_name = std::filesystem::path(std::wstring(fni->FileName, fni->FileNameLength / sizeof(wchar_t)));

						auto extension = file_name.extension().string();

						if (extension == ".evtc" || extension == ".zevtc")
						{
							{
								static const auto is_file_openable = [](const std::filesystem::path& log_path)
									{
										std::ifstream file_stream(log_path);
										return file_stream.is_open();
									};

								auto file_path = monitor_directory / file_name;

								auto log_available = is_file_openable(file_path);

								int cooldown = 0;

								while (!log_available && cooldown++ < 100)
								{
									std::this_thread::sleep_for(std::chrono::milliseconds(100));
									log_available = is_file_openable(file_path);
								}

								if (log_available)
								{
									LOG("New evtc file detected: " + file_path.string(), LogLevel::Debug);

									EVTCData evtc_data;

									try
									{
										evtc_data = global::evtc_parser->parse(file_path);

										if (evtc_data.trigger_id != TriggerID::Invalid)
											global::log_manager->add_encounter_log(evtc_data);
									}
									catch (const std::exception& e)
									{
										LOG("Evtc parsing failed. File: \"" + file_name.string() + "\" Exception: " + e.what(), LogLevel::Warning);
									}
								}
								else
									LOG("Evtc file unavailable: " + file_path.string(), LogLevel::Warning);
							}
						}
					}

					fni = (fni->NextEntryOffset != 0) ? reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(fni) + fni->NextEntryOffset) : nullptr;

				} while (fni != nullptr && this->is_initialized());
			}
		}
		else if (wait_result == WAIT_FAILED)
		{
			LOG("Logs directory monitor WaitForSingleObject failed", LogLevel::Error);
			break;
		}
	}

	if (monitor_overlapped.hEvent != NULL)
	{
		CloseHandle(monitor_overlapped.hEvent);
		this->monitor_overlapped.hEvent = NULL;
	}

	if (logs_directory_handle != INVALID_HANDLE_VALUE)
		CloseHandle(logs_directory_handle);

	LOG("Directory monitor stopped", LogLevel::Info);
}

#undef LOG