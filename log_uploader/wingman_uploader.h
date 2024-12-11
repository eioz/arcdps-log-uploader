#pragma once

#include "encounter_log.h"
#include "uploader.h"	
#include "settings.h"

#include <memory>

class WingmanUploader : public Uploader
{
public:
	WingmanUploader() = default;
	~WingmanUploader() = default;

	void initialize() override
	{
		std::lock_guard lock(this->initialization_mutex);

		if (this->is_initialized())
			return;

		this->initialized.store(true);

		this->upload_thread = std::thread(&WingmanUploader::run, this);
	};

	void queue_upload(std::shared_ptr<EncounterLog> encounter_log) override;
	void process_auto_upload(std::shared_ptr<EncounterLog> encounter_log);

protected:
	void run() override;
private:
	bool check_server_availability();
};

namespace global{ extern std::unique_ptr<WingmanUploader> wingman_uploader; }
