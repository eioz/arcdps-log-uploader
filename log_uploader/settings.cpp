#include "logger.h"
#include "settings.h"

namespace global { std::unique_ptr<Settings> settings = std::make_unique<Settings>(); }

#define LOG(message, log_level) global::logger->write(message, log_level, LogSource::Settings)

void Settings::initialize(std::filesystem::path settings_file_path)
{
	if (settings_file_path.empty())
		throw std::invalid_argument("settings_file_path is empty");

	std::lock_guard initialization_lock(this->initialization_mutex);

	std::unique_lock data_lock(this->mutex);

	this->settings_file_path = std::move(settings_file_path);

	if (!std::filesystem::exists(this->settings_file_path) || !this->load())
	{
		if (this->save())
			LOG("Settings file created", LogLevel::Info);
		else
			LOG("Failed to create settings file", LogLevel::Error);
	}

	data_lock.unlock();

	this->initialized.store(true);
}

bool Settings::load()
{
	if (this->settings_file_path.empty())
		return false;

	if (!std::filesystem::exists(this->settings_file_path))
	{
		LOG("Settings file does not exist", LogLevel::Error);
		return false;
	}

	std::ifstream settings_file(this->settings_file_path);

	if (settings_file.is_open())
	{
		try
		{
			nlohmann::json json;
			settings_file >> json;
			settings_file.close();

			nlohmann::json target_json = this->settings;

			target_json.merge_patch(json);

			auto settings = target_json.get<UploaderSettings>();

			settings.verify();

			this->settings = std::move(settings);

			LOG("Settings loaded from file: " + this->settings_file_path.string(), LogLevel::Info);

			return true;
		}
		catch (const std::exception& e)
		{
			LOG("Failed to load settings from file: \"" + this->settings_file_path.string() + "\" Exception: " + std::string(e.what()), LogLevel::Error);
		}
	}
	else
		LOG("Failed to open settings file for reading: " + this->settings_file_path.string(), LogLevel::Error);

	return false;
}

bool Settings::save()
{
	if (this->settings_file_path.empty() || !std::filesystem::exists(this->settings_file_path.parent_path()))
		return false;

	std::ofstream file(this->settings_file_path);

	if (file.is_open())
	{
		try
		{
			nlohmann::json json = this->settings;

			file << json.dump(2);
			file.close();

			LOG("Settings saved to file: " + this->settings_file_path.string(), LogLevel::Info);

			return true;
		}
		catch (const std::exception& e)
		{
			LOG("Failed to save settings to file: \"" + this->settings_file_path.string() + "\" Exception: " + std::string(e.what()), LogLevel::Error);
		}
	}
	else
		LOG("failed to open settings file for writing: " + this->settings_file_path.string(), LogLevel::Error);

	return false;
}

#undef LOG