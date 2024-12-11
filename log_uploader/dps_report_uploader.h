#pragma once

#include "encounter_log.h"
#include "settings.h"
#include "uploader.h"	

class DpsReportUploader : public Uploader
{
public:
	DpsReportUploader() = default;
	~DpsReportUploader() = default;

	void initialize()
	{
		std::lock_guard lock(this->initialization_mutex);

		if (this->is_initialized())
			return;

		this->initialized = true;

		this->upload_thread = std::thread(&DpsReportUploader::run, this);
	};

	void queue_upload(std::shared_ptr<EncounterLog> encounter_log) override { this->queue_upload(std::move(encounter_log), false); };
	void queue_upload(std::shared_ptr<EncounterLog> encounter_log, bool is_auto_upload);

	void process_auto_upload(std::shared_ptr<EncounterLog> encounter_log) override;

private:
	void run() override;

};

namespace global { extern std::unique_ptr<DpsReportUploader> dps_report_uploader; }
