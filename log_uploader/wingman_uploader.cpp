#include "logger.h"
#include "wingman_uploader.h"

#include <string>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

namespace global { std::unique_ptr<WingmanUploader> wingman_uploader = std::make_unique<WingmanUploader>(); }

#define LOG(message, log_level) global::logger->write(message, log_level, LogSource::WingmanUploader)

void WingmanUploader::queue_upload(std::shared_ptr<EncounterLog> encounter_log)
{
	if (!this->is_initialized())
	{
		LOG("Uploader not initialized", LogLevel::Warning);
		return;
	}

	std::unique_lock log_lock(encounter_log->mutex);

	if (encounter_log->wingman_upload.status != WingmanUploadStatus::AVAILABLE && encounter_log->wingman_upload.status != WingmanUploadStatus::FAILED)
	{
		LOG("Log is not available for upload", LogLevel::Warning);
		return;
	}

	encounter_log->wingman_upload.status = WingmanUploadStatus::QUEUED;

	LOG("Queued encounter log for upload: " + encounter_log->id, LogLevel::Info);

	log_lock.unlock();

	{
		std::unique_lock upload_queue_lock(this->upload_queue_mutex);
		this->upload_queue.push(encounter_log);
		this->upload_cv.notify_one();
	}
}

void WingmanUploader::process_auto_upload(std::shared_ptr<EncounterLog> encounter_log)
{
	if (!GET_SETTING(wingman.auto_upload))
		return;

	auto log_data = encounter_log->get_data();

	auto encounter_filter = GET_SETTING(wingman.auto_upload_encounters);
	bool is_in_filter = std::find(encounter_filter.begin(), encounter_filter.end(), log_data.evtc_data.trigger_id) != encounter_filter.end();

	if (is_in_filter)
	{
		if (GET_SETTING(wingman.auto_upload_filter) == AutoUploadFilter::SUCCESSFUL_ONLY && log_data.encounter_data.success != true)
		{
			LOG("Skipping wingman auto upload for encounter: " + log_data.id + " with trigger id " + std::to_string(static_cast<int>(log_data.evtc_data.trigger_id)) + " because it was not a success", LogLevel::Info);
			return;
		}

		this->queue_upload(encounter_log);
	}
	else
		LOG("Skipping wingman auto upload for encounter: " + log_data.id + " with trigger id " + std::to_string(static_cast<int>(log_data.evtc_data.trigger_id)), LogLevel::Info);
}

void WingmanUploader::run()
{
	std::unique_lock upload_lock(this->upload_mutex);

	LOG("Uploader thread started", LogLevel::Info);

	while (this->is_initialized())
	{
		this->upload_cv.wait(upload_lock, [this] { return !this->is_initialized() || !this->upload_queue.empty(); });

		if (!this->is_initialized())
			break;

		if (!this->check_server_availability())
		{
			LOG("Wingman servers unavailable. Uploading paused for 60 seconds ...", LogLevel::Warning);

			auto delay = std::chrono::seconds(60);
			auto step = std::chrono::seconds(1);

			while (delay > std::chrono::seconds(0))
			{
				if (!this->is_initialized())
					break;

				std::this_thread::sleep_for(step);
				delay -= step;

				LOG("Delaying uploads for " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(delay).count()) + " seconds ...", LogLevel::Debug);
			}

			this->upload_cv.notify_one();

			continue;
		}

		std::unique_lock upload_queue_lock(this->upload_queue_mutex);

		if (this->upload_queue.empty())
			continue;

		auto log = this->upload_queue.front();
		this->upload_queue.pop();

		upload_queue_lock.unlock();

		std::unique_lock log_lock(log->mutex);

		if (log->wingman_upload.status != WingmanUploadStatus::QUEUED || log->parse_status != ParseStatus::PARSED)
		{
			LOG("Log has invalid status: " + log->id, LogLevel::Debug);
			continue;
		}

		log->wingman_upload.status = WingmanUploadStatus::UPLOADING;

		LOG("Uploading encounter log: " + log->id, LogLevel::Info);

		auto log_data = log->get_data_locked();

		log_lock.unlock();

		auto& upload = log_data.wingman_upload;

		const auto& evtc_file = log_data.evtc_data.evtc_file_path;
		const auto& html_file = log_data.report_data.html_file_path;
		const auto& json_file = log_data.report_data.json_file_path;

		if (!std::filesystem::exists(evtc_file) || !std::filesystem::exists(html_file) || !std::filesystem::exists(json_file))
		{
			upload.error_message = "Evtc, html or json file does not exist";
			upload.status = WingmanUploadStatus::FAILED;
		}
		else
		{
			auto filesize = std::filesystem::file_size(evtc_file);
			auto ftime = std::filesystem::last_write_time(evtc_file);
			auto cftime = std::chrono::system_clock::to_time_t(std::chrono::clock_cast<std::chrono::system_clock>(ftime));

			/*
			/checkUpload
			Checks if there is a conflict in the database regarding a specified log. Returns "Error" if no upload is possible right now, "False" if 'file' or 'filesize' is not specified or the log already exists in the database, "True" if upload is possible.
			It is highly recommended to run this before /uploadProcessed to quickly reject duplicates without the need of uploading or processing.
			POST 'file': The name of the .zevtc file to be checked.
			POST 'timestamp': The timestamp of the creation of this .zevtc file (Unix epoch timestamps in seconds).
			POST 'filesize': The size in bytes of this .zevtc file.
			POST 'account': The account name of the uploader account. Recommended for quicker detection of duplicates.
			*/
			cpr::Multipart multipart_check_upload =
			{
				{ "file", evtc_file.filename().string() },
				{ "timestamp", std::to_string(cftime) },
				{ "filesize", std::to_string(filesize) },
				{ "account", log_data.encounter_data.account_name }
			};

			auto response_check_upload = cpr::Post(cpr::Url("https://gw2wingman.nevermindcreations.de/checkUpload"), multipart_check_upload, cpr::Timeout{ GET_SETTING(wingman.request_timeout) });

			if (response_check_upload.status_code == 200)
			{
				if (response_check_upload.text == "True")
				{
					cpr::Multipart multipart_upload_processed{
						{ "file", cpr::File(evtc_file.string(), evtc_file.filename().string())},
						{ "jsonfile", cpr::File(json_file.string(), json_file.filename().string()) },
						{ "htmlfile", cpr::File(html_file.string(), html_file.filename().string()) },
						{ "account", log_data.encounter_data.account_name }
					};

					/*
					/uploadProcessed
					Uploads an EliteInsights-processed log (composed of .zevtc, .json, .html), checks for duplicates and adds it to the gw2wingman database if appropriate. Returns the processed json if successful, "False" otherwise.
					POST 'file': The original .zevtc log file.
					POST 'jsonfile': The EliteInsights .json output file.
					POST 'htmlfile': The EliteInsights .html output file.
					POST 'account': The account name of the uploader account. Optional, but recommended for desk-rejecting duplicate logs.
					*/

					auto response_upload_processed = cpr::Post(cpr::Url("https://gw2wingman.nevermindcreations.de/uploadProcessed"), multipart_upload_processed, cpr::Timeout{ GET_SETTING(wingman.request_timeout) });

					if (response_upload_processed.status_code == 200)
					{
						if (response_upload_processed.text == "True")
							upload.status = WingmanUploadStatus::UPLOADED;
						else
						{
							if (response_upload_processed.text.empty())
								upload.error_message = "Wingman returned an error on /uploadProcessed: " + response_upload_processed.text;
							else
								upload.error_message = "Wingman returned an error on /uploadProcessed";
						}
					}
					else
					{
						upload.error_message = "Wingman returned an http error on /uploadProcessed (" + std::to_string(response_upload_processed.status_code) + ")";
					}
				}
				else if (response_check_upload.text == "Error")
				{
					upload.error_message = "Wingman returned an error on /checkUpload";
				}
				else if (response_check_upload.text == "False")
				{
					upload.status = WingmanUploadStatus::SKIPPED;
					upload.error_message = "Log already exists in the wingman database";

					log_lock.lock();
					log->wingman_upload = upload;
					continue;
				}
				else
				{
					if (response_check_upload.text.empty())
						upload.error_message = "Wingman returned no data on /checkUpload";
					else
						upload.error_message = "Wingman returned an error on /checkUpload: " + response_check_upload.text;
				}
			}
			else
			{
				upload.error_message = "Wingman returned an http error on /checkUpload (" + std::to_string(response_check_upload.status_code) + ")";
			}
		}

		log_lock.lock();

		if (upload.error_message.has_value())
			upload.status = WingmanUploadStatus::FAILED;

		log->wingman_upload = upload;

		if (upload.status == WingmanUploadStatus::SKIPPED)
		{
			LOG("Encounter log skipped: " + log->id, LogLevel::Info);
			continue;
		}
		else if (upload.status == WingmanUploadStatus::FAILED)
		{
			LOG("Encounter log upload failed: " + log->id + " - " + upload.error_message.value(), LogLevel::Error);
			continue;
		}
		else if (upload.status == WingmanUploadStatus::UPLOADED)
		{
			LOG("Encounter log uploaded: " + log->id, LogLevel::Info);
		}
	}

	LOG("Uploader shutdown", LogLevel::Info);
}

bool WingmanUploader::check_server_availability()
{
	static std::chrono::system_clock::time_point last_check_time = std::chrono::system_clock::now();
	static const std::chrono::seconds check_interval = std::chrono::seconds(180);
	auto current_time = std::chrono::system_clock::now();

	static auto servers_available = false;

	if (!servers_available || current_time - last_check_time > check_interval)
	{
		last_check_time = current_time;

		/*
		/testConnection
		Returns "True" if a connection to the wingman database can be established, "False" otherwise.
		*/

		auto response = cpr::Get(cpr::Url("https://gw2wingman.nevermindcreations.de/testConnection"), cpr::Timeout{ GET_SETTING(wingman.request_timeout) });

		return servers_available = (response.status_code == 200 && response.text == "True");
	}

	return servers_available;
}

#undef LOG