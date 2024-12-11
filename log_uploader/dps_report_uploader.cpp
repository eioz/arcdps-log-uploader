#include "dps_report_uploader.h"
#include "logger.h"

#include <string>


#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

namespace global { std::unique_ptr<DpsReportUploader> dps_report_uploader = std::make_unique<DpsReportUploader>(); }

#define LOG(message, log_level) global::logger->write(message, log_level, LogSource::DpsReportUploader)

void DpsReportUploader::queue_upload(std::shared_ptr<EncounterLog> encounter_log, bool is_auto_upload)
{
	if (!this->is_initialized())
	{
		LOG("Uploader not initialized", LogLevel::Warning);
		return;
	}

	std::unique_lock log_lock(encounter_log->mutex);

	if (encounter_log->dps_report_upload.status != DpsReportUploadStatus::AVAILABLE && encounter_log->dps_report_upload.status != DpsReportUploadStatus::FAILED)
	{
		LOG("Log is not available for upload", LogLevel::Warning);
		return;
	}

	encounter_log->dps_report_upload.is_auto_upload = is_auto_upload;
	encounter_log->dps_report_upload.status = DpsReportUploadStatus::QUEUED;

	LOG("Queued encounter log for upload: " + encounter_log->id, LogLevel::Info);

	log_lock.unlock();

	{
		std::unique_lock upload_queue_lock(this->upload_queue_mutex);
		this->upload_queue.push(encounter_log);
		this->upload_cv.notify_one();
	}
}

void DpsReportUploader::process_auto_upload(std::shared_ptr<EncounterLog> encounter_log)
{
	if (!GET_SETTING(dps_report.auto_upload))
		return;

	auto log_data = encounter_log->get_data();

	auto encounter_filter = GET_SETTING(dps_report.auto_upload_encounters);
	bool is_in_filter = std::find(encounter_filter.begin(), encounter_filter.end(), log_data.evtc_data.trigger_id) != encounter_filter.end();

	if (is_in_filter)
		this->queue_upload(encounter_log, true);
	else
		LOG("Skipping dps.report auto upload for encounter: " + log_data.id + " with trigger id " + std::to_string(static_cast<int>(log_data.evtc_data.trigger_id)), LogLevel::Info);
}

void DpsReportUploader::run()
{
	std::unique_lock upload_lock(this->upload_mutex);

	LOG("Uploader thread started", LogLevel::Info);

	while (this->is_initialized())
	{
		this->upload_cv.wait(upload_lock, [this] { return !this->is_initialized() || !this->upload_queue.empty(); });

		if (!this->is_initialized())
			break;

		std::unique_lock upload_queue_lock(this->upload_queue_mutex);

		if (this->upload_queue.empty())
			continue;

		auto log = this->upload_queue.front();
		this->upload_queue.pop();

		upload_queue_lock.unlock();

		std::unique_lock log_lock(log->mutex);

		if (log->dps_report_upload.status != DpsReportUploadStatus::QUEUED)
		{
			LOG("Log has invalid upload status state", LogLevel::Debug);
			continue;
		}

		log->dps_report_upload.status = DpsReportUploadStatus::UPLOADING;

		LOG("Uploading encounter log: " + log->id, LogLevel::Info);

		DpsReportUpload upload = log->dps_report_upload;

		log_lock.unlock();

		cpr::Url url("https://dps.report/uploadContent");
		cpr::Parameters parameters{};
		cpr::Multipart multipart{ { "file", cpr::File(log->evtc_data.evtc_file_path.string(), log->evtc_data.evtc_file_path.filename().string())}, { "json", "1" } };

		auto settings = GET_SETTING(dps_report);

		if (!settings.user_token.empty() && settings.user_token.length() == 32)
			parameters.Add({ "userToken", settings.user_token });

		if (settings.anonymize)
			parameters.Add({ "anonymous", "true" });

		if (settings.detailed_wvw)
			parameters.Add({ "detailedwvw", "true" });

		auto response = cpr::Post(url, parameters, multipart, cpr::Timeout{ settings.request_timeout });

		upload.status = DpsReportUploadStatus::FAILED;

		if (response.status_code == 200)
		{
			try
			{
				nlohmann::json json = nlohmann::json::parse(response.text);

				upload.id = json.value("id", std::string());
				upload.url = json.value("permalink", std::string());
				upload.user_token = json.value("userToken", std::string());

				if (json.contains("error") && !json.at("error").is_null())
				{
					upload.error_message = "Json contains errors";

					if (json.at("error").is_string())
						upload.error_message = json.at("error").get<std::string>();
				}
				else
					upload.status = DpsReportUploadStatus::UPLOADED;
			}
			catch (const nlohmann::json::exception& e)
			{
				upload.error_message = "Failed to parse response: " + std::string(e.what());
			}
		}
		else if (response.status_code >= 400 && response.status_code < 500)
		{
			upload.error_message = "Client error: " + std::to_string(response.status_code);

			if (!response.text.empty())
			{
				try
				{
					auto json = nlohmann::json::parse(response.text);

					if (json.contains("error") && json.at("error").is_string())
						upload.error_message = "Error: " + json.at("error").get<std::string>();
				}
				catch (const nlohmann::json::exception& e)
				{
					upload.error_message = "Failed to parse error message: " + std::string(e.what());
				}
			}
		}
		else
			upload.error_message = "Server error: " + std::to_string(response.status_code);

		log_lock.lock();

		log->dps_report_upload = upload;

		if (log->dps_report_upload.status == DpsReportUploadStatus::UPLOADED)
			LOG("Encounter log uploaded: " + log->id, LogLevel::Info);
		else
		{
			if (upload.error_message.has_value())
				LOG("Failed to upload encounter log: " + log->id + " - " + upload.error_message.value(), LogLevel::Error);
			else
				LOG("Failed to upload encounter log: " + log->id, LogLevel::Error);
		}

		log_lock.unlock();

		if (GET_SETTING(dps_report.copy_to_clipboard) && upload.status == DpsReportUploadStatus::UPLOADED)
		{
			std::string clipboard_text = upload.url;

			if (OpenClipboard(NULL))
			{
				EmptyClipboard();

				HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, clipboard_text.size() + 1);
				if (hg)
				{
					memcpy(GlobalLock(hg), clipboard_text.c_str(), clipboard_text.size() + 1);
					GlobalUnlock(hg);
					SetClipboardData(CF_TEXT, hg);
				}

				CloseClipboard();
			}
		}
	}

	LOG("Uploader shutdown", LogLevel::Info);
}

#undef LOG