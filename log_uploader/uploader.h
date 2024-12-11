#pragma once

#include "module.h"
#include "encounter_log.h"

#include <queue>
#include <memory>
#include <thread>
#include <mutex>

class Uploader : public Module
{
public:
	Uploader() = default;

	virtual ~Uploader() = default;

	virtual void queue_upload(std::shared_ptr<EncounterLog> encounter_log) = 0;

	auto clear_upload_queue()
	{
		std::lock_guard lock(this->upload_queue_mutex);
		std::queue<std::shared_ptr<EncounterLog>> empty;
		std::swap(this->upload_queue, empty);
	}

	virtual void initialize() = 0;

	auto release() -> void override
	{
		std::lock_guard lock(this->initialization_mutex);

		if (!this->initialized)
			return;

		this->initialized = false;

		this->clear_upload_queue();

		this->upload_cv.notify_all();

		if (this->upload_thread.joinable())
			this->upload_thread.join();
	};

	virtual void process_auto_upload(std::shared_ptr<EncounterLog> encounter_log) = 0;

protected:
	std::mutex upload_mutex;
	std::condition_variable upload_cv;

	std::mutex upload_queue_mutex;
	std::queue<std::shared_ptr<EncounterLog>> upload_queue;

	std::thread upload_thread;

	virtual void run() = 0;
};