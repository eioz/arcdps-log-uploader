#pragma once

#include "module.h"

#include <atomic>
#include <thread>
#include <Windows.h>
#include <filesystem>

class DirectoryMonitor : public Module
{
public:
	DirectoryMonitor() {}
	~DirectoryMonitor() {}

	void initialize(std::filesystem::path monitor_directory);
	void release() override;

private:
	std::thread monitor_thread;
	OVERLAPPED monitor_overlapped = { 0 };
	std::filesystem::path monitor_directory;

	void run();
};

namespace global { extern std::unique_ptr<DirectoryMonitor> directory_monitor; }