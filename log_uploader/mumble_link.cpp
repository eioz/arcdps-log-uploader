#include "logger.h"
#include "mumble_link.h"

namespace global { std::unique_ptr<MumbleLink> mumble_link = std::make_unique<MumbleLink>(); }

#define LOG(message, log_level) global::logger->write(message, log_level, LogSource::MumbleLink)

bool MumbleLink::initialize(std::string linked_mem_name)
{
	std::lock_guard initialization_lock(this->initialization_mutex);

	std::unique_lock memory_lock(this->mutex);

	if (linked_mem_name.empty())
		return false;

	this->linked_mem_name = std::move(linked_mem_name);

	this->file_handle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(LinkedMem), this->linked_mem_name.c_str());

	if (this->file_handle == nullptr)
	{
		LOG("Failed to create file mapping", LogLevel::Error);
		return false;
	}

	this->linked_mem = static_cast<LinkedMem*>(MapViewOfFile(this->file_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem)));

	if (this->linked_mem == nullptr)
	{
		LOG("Linked memory invalid", LogLevel::Warning);
		CloseHandle(this->file_handle);
		return false;
	}

	auto link_name = std::wstring(this->linked_mem->name);

	if (link_name == L"Guild Wars 2")
	{
		LOG("Initialized (" + this->linked_mem_name + ")", LogLevel::Info);
		this->initialized.store(true);
		return true;
	}

	return false;
}

void MumbleLink::release()
{
	std::lock_guard initialization_lock(this->initialization_mutex);

	this->initialized.store(false);

	std::unique_lock memory_lock(this->mutex);

	if (this->file_handle != nullptr)
	{
		CloseHandle(this->file_handle);
		this->file_handle = nullptr;
	}

	if (this->linked_mem != nullptr)
	{
		UnmapViewOfFile(this->linked_mem);
		this->linked_mem = nullptr;
	}

	this->linked_mem_name.clear();
}

#undef LOG