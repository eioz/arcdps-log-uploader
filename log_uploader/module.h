#pragma once

#include <mutex>
#include <atomic>

class Module
{
public:
	virtual void release()
	{
		std::lock_guard lock(this->initialization_mutex);

		this->initialized.store(false);
	}

	auto is_initialized() -> bool
	{
		return this->initialized.load();
	}

protected:
	mutable std::mutex initialization_mutex;
	
	std::atomic<bool> initialized = false;
};