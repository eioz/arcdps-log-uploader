#include "elite_insights.h"
#include "logger.h"
#include "settings.h"
#include "wingman_uploader.h"

#include <cpr/cpr.h>
#include <miniz/miniz.h>

namespace global { std::unique_ptr<EliteInsights> elite_insights = std::make_unique<EliteInsights>(); }

#define LOG(message, log_level) global::logger->write(message, log_level, LogSource::EliteInsights)

bool EliteInsights::initialize(std::filesystem::path installation_directory, std::filesystem::path output_directory)
{
	std::lock_guard lock(this->initialization_mutex);

	if (installation_directory.empty() || output_directory.empty())
		throw std::invalid_argument("installation_directory is empty");

	if (this->is_initialized())
	{
		LOG("Already initialized", LogLevel::Debug);
		return false;
	}

	this->installation_directory = installation_directory;
	this->output_directory = output_directory;

	this->executable_file = this->installation_directory / "GuildWars2EliteInsights-CLI.exe";
	this->settings_file = this->installation_directory / "Settings" / "settings.conf";
	this->version_file = this->installation_directory / ".version";

	auto settings = GET_SETTING(elite_insights); // settings used for initialization

	if (settings.request_timeout)
		this->request_timeout = cpr::Timeout{ settings.request_timeout };

	if (!settings.auto_update)
	{
		LOG("Auto update is disabled", LogLevel::Info);

		if (this->is_installed())
		{
			this->initialized = true;
			this->parser_thread = std::thread(&EliteInsights::run_parser, this);
			return true;
		}
	}

	if (settings.auto_update || !this->is_installed())
	{
		if (!this->update(settings.update_channel))
		{
			LOG("Failed to install Elite Insights", LogLevel::Error);
			return false;
		}
	}

	this->parser_thread = std::thread(&EliteInsights::run_parser, this);

	this->initialized.store(true);

	return true;
}

void EliteInsights::release()
{
	std::lock_guard lock(this->initialization_mutex);

	this->initialized.store(false);

	std::unique_lock parser_queue_lock(this->parser_queue_mutex);

	while (!this->parser_queue.empty())
		this->parser_queue.pop();

	this->parser_cv.notify_all();

	parser_queue_lock.unlock();

	if (this->parser_thread.joinable())
		this->parser_thread.join();
}

void EliteInsights::queue_encounter_log(std::shared_ptr<EncounterLog> encounter_log)
{
	if (!this->is_initialized())
	{
		LOG("Parser not initialized", LogLevel::Warning);
		return;
	}

	std::unique_lock log_lock(encounter_log->mutex);

	if (encounter_log->parse_status != ParseStatus::UNPARSED)
	{
		LOG("Encounter log has invalid parse state", LogLevel::Warning);
		return;
	}

	encounter_log->parse_status = ParseStatus::QUEUED;

	LOG("Queued encounter log for parsing: " + encounter_log->id, LogLevel::Info);

	log_lock.unlock();

	{
		std::unique_lock parser_queue_lock(this->parser_queue_mutex);
		this->parser_queue.push(encounter_log);
		this->parser_cv.notify_one();
	}
}

void EliteInsights::process_auto_parse(std::shared_ptr<EncounterLog> encounter_log)
{
	if (GET_SETTING(elite_insights.auto_parse))
		this->queue_encounter_log(encounter_log);
}

bool EliteInsights::refresh_local_version()
{
	if (!std::filesystem::exists(this->version_file))
		return false;

	std::ifstream version_file(this->version_file);

	if (version_file)
	{
		std::string version_str;
		version_file >> version_str;
		version_file.close();

		this->local_version = EliteInsightsVersion(version_str);

		return this->local_version.is_valid();
	}
	else
		LOG("Failed to read local version file: " + this->version_file.string(), LogLevel::Warning);

	return false;
}

bool EliteInsights::refresh_latest_version()
{
	const std::string url = "https://api.github.com/repos/baaron4/GW2-Elite-Insights-Parser/releases/latest";

	const auto response = cpr::Get(cpr::Url{ url }, this->request_timeout);

	if (response.status_code != 200)
	{
		LOG("Failed to fetch latest release information from " + url + ". (" + (response.status_code ? std::to_string(response.status_code) : "timeout") + ")", LogLevel::Warning);
		return false;
	}

	try
	{
		const auto json_response = nlohmann::json::parse(response.text);

		if (!json_response.contains("tag_name") || !json_response.contains("assets") || !json_response["assets"].is_array() || json_response["assets"].empty())
		{
			LOG("Failed to parse latest release information: invalid json response", LogLevel::Warning);
			return false;
		}

		std::string download_url;

		for (const auto& asset : json_response["assets"])
		{
			if (asset.contains("name") && asset["name"].is_string() && asset["name"] == "GW2EICLI.zip" && asset.contains("browser_download_url") && asset["browser_download_url"].is_string())
			{
				download_url = asset["browser_download_url"].get<std::string>();
				break;
			}
		}

		if (download_url.empty())
		{
			LOG("Failed to find required assets in the latest release", LogLevel::Warning);
			return false;
		}

		this->latest_version = EliteInsightsVersion(json_response.at("tag_name").get<std::string>(), download_url);

		return this->latest_version.is_valid();
	}
	catch (const nlohmann::json::exception& e)
	{
		LOG("Failed to parse latest release information: " + std::string(e.what()), LogLevel::Warning);
		return false;
	}

	return false;
}

bool EliteInsights::refresh_latest_version_wingman()
{
	const std::string version_url = "https://gw2wingman.nevermindcreations.de/api/EIversion";

	const auto version_response = cpr::Get(cpr::Url{ version_url }, this->request_timeout);

	if (version_response.status_code != 200)
	{
		LOG("Failed to fetch version tag from " + version_url + ". (" + (version_response.status_code ? std::to_string(version_response.status_code) : "timeout") + ")", LogLevel::Warning);
		return false;
	}

	const auto wingman_version = EliteInsightsVersion(version_response.text);

	if (!wingman_version.is_valid())
	{
		LOG("Failed to parse wingman version information", LogLevel::Warning);
		return false;
	}

	const std::string github_url = "https://api.github.com/repos/baaron4/GW2-Elite-Insights-Parser/releases/tags/" + wingman_version.get_tag();

	const auto response = cpr::Get(cpr::Url{ github_url }, cpr::Timeout{ 10000 });

	if (response.status_code != 200)
	{
		LOG("Failed to fetch release information for tag " + wingman_version.get_tag() + " from github. (" + (response.status_code ? std::to_string(response.status_code) : "timeout") + ")", LogLevel::Warning);
		return false;
	}

	try
	{
		const auto json_response = nlohmann::json::parse(response.text);

		if (!json_response.contains("tag_name") || !json_response.contains("assets") || !json_response["assets"].is_array() || json_response["assets"].empty())
		{
			LOG("Failed to parse release information for tag " + wingman_version.get_tag() + ": invalid json response", LogLevel::Warning);
			return false;
		}

		std::string download_url;

		for (const auto& asset : json_response["assets"])
		{
			if (asset.contains("name") && asset["name"].is_string() && asset["name"] == "GW2EICLI.zip" && asset.contains("browser_download_url") && asset["browser_download_url"].is_string())
			{
				download_url = asset["browser_download_url"].get<std::string>();
				break;
			}
		}

		if (download_url.empty())
		{
			LOG("Failed to find required assets for tag " + wingman_version.get_tag(), LogLevel::Warning);
			return false;
		}

		this->latest_version_wingman = EliteInsightsVersion(json_response.at("tag_name").get<std::string>(), download_url);
	}
	catch (const nlohmann::json::exception& e)
	{
		LOG("Failed to parse release information for tag " + wingman_version.get_tag() + ": " + std::string(e.what()), LogLevel::Warning);
		return false;
	}

	return this->latest_version_wingman.is_valid();
}

bool EliteInsights::update(EliteInsightsUpdateChannel version)
{
	auto installed = this->is_installed();

	auto& target_version = this->latest_version;

	switch (version)
	{
	case EliteInsightsUpdateChannel::LATEST_WINGMAN:
	{
		if (this->refresh_latest_version_wingman())
		{
			target_version = this->latest_version_wingman;
			break;
		}

		if (installed)
			break;

		LOG("Elite Insights version preference was set to \"latest wingman\", but the wingman required version is not available. Setting target version to \"latest\".", LogLevel::Warning);

		if (this->refresh_latest_version())
			target_version = this->latest_version;

		break;
	}
	case EliteInsightsUpdateChannel::LATEST:
	{
		if (this->refresh_latest_version())
			target_version = this->latest_version;
		break;
	}
	}

	if (!target_version.is_valid())
	{
		LOG("Invalid target version", LogLevel::Error);
		return false;
	}

	if (this->local_version < target_version)
	{
		if (installed)
			LOG("Updating Elite Insights from " + this->local_version.get_tag() + " to " + target_version.get_tag(), LogLevel::Info);
		else
			LOG("Installing Elite Insights " + target_version.get_tag(), LogLevel::Info);

		if (!this->set_version(target_version))
		{
			LOG("Failed to install Elite Insights", LogLevel::Error);
			return false;
		}

		if (!this->write_parser_settings())
		{
			LOG("Failed to write parser settings", LogLevel::Error);
			return false;
		}

		if (!this->refresh_local_version())
			LOG("Local version invalid after updating?", LogLevel::Error);


		return true;
	}

	LOG("Elite Insights is up-to-date. (" + target_version.get_tag() + ")", LogLevel::Info);

	return true;
}

bool EliteInsights::set_version(const EliteInsightsVersion version)
{
	if (!version.is_valid())
	{
		LOG("Invalid target version", LogLevel::Error);
		return false;
	}

	if (version.download_url.empty())
	{
		LOG("Download url is empty", LogLevel::Error);
		return false;
	}

	auto response = cpr::Get(cpr::Url{ version.download_url }, this->request_timeout);

	if (response.status_code != 200)
	{
		LOG("Failed to download Elite Insights from " + version.download_url, LogLevel::Warning);
		return false;
	}

	mz_zip_archive zip_archive{};
	mz_zip_zero_struct(&zip_archive);
	if (!mz_zip_reader_init_mem(&zip_archive, reinterpret_cast<const unsigned char*>(response.text.c_str()), response.text.size(), 0))
	{
		LOG("Failed to initialize zip archive", LogLevel::Error);
		return false;
	}

	if (std::filesystem::exists(this->installation_directory))
		std::filesystem::remove_all(this->installation_directory);

	auto extraction_errors = 0;

	for (auto i = 0; i < static_cast<int>(mz_zip_reader_get_num_files(&zip_archive)); ++i)
	{
		mz_zip_archive_file_stat file_stat{};
		if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
		{
			LOG("failed to get file stat for file " + std::to_string(i), LogLevel::Warning);
			extraction_errors++;
			continue;
		}

		if (file_stat.m_is_directory)
			continue;

		auto output_path = this->installation_directory / file_stat.m_filename;

		if (output_path.has_parent_path())
			std::filesystem::create_directories(output_path.parent_path());

		size_t uncompressed_size = 0;

		auto uncompressed_data = mz_zip_reader_extract_to_heap(&zip_archive, i, &uncompressed_size, 0);

		if (!uncompressed_data)
		{
			LOG("failed to extract file " + output_path.string(), LogLevel::Warning);
			extraction_errors++;
			continue;
		}

		std::ofstream out_file(output_path, std::ios::binary);

		if (out_file)
		{
			out_file.write(reinterpret_cast<const char*>(uncompressed_data), uncompressed_size);
			out_file.close();
		}
		else
		{
			LOG("Failed to write extracted file " + output_path.string(), LogLevel::Warning);
			extraction_errors++;
		}

		mz_free(uncompressed_data);
	}

	mz_zip_reader_end(&zip_archive);

	if (extraction_errors)
	{
		LOG("Failed to extract " + std::to_string(extraction_errors) + " files or directories", LogLevel::Error);
		return false;
	}

	std::ofstream version_file(this->version_file, std::ios::out);

	if (version_file)
	{
		version_file << version.tag_name;
		version_file.close();
	}
	else
	{
		LOG("Failed to write version file: " + this->version_file.string(), LogLevel::Error);
		return false;
	}

	return true;
}

bool EliteInsights::write_parser_settings()
{
	if (!std::filesystem::exists(this->settings_file.parent_path()))
		if (!std::filesystem::create_directories(this->settings_file.parent_path()))
		{
			LOG("Failed to create Elite Insights settings directory", LogLevel::Error);
			return false;
		}

	if (std::ofstream file_stream(this->settings_file, std::ios::out); file_stream)
	{
		std::stringstream string_stream;
		string_stream << R"(# automatically generated by arcdps log uploader extension
SaveOutJSON=true
IndentJSON=false
SaveOutHTML=true
SaveOutTrace=false
SaveAtOut=false
ParseCombatReplay=true
SingleThreaded=false
OutLocation=)" << std::regex_replace(this->output_directory.string(), std::regex(R"(\\)"), R"(\\)");

		file_stream << string_stream.str();
		file_stream.close();

		return true;
	}
	else
		LOG("Failed to write Elite Insights settings file: " + this->settings_file.string(), LogLevel::Error);

	return false;
}

void EliteInsights::run_parser()
{
	std::unique_lock parser_lock(this->parser_mutex);

	LOG("Parser thread started", LogLevel::Info);

	while (this->is_initialized())
	{
		this->parser_cv.wait(parser_lock, [this] { return !this->is_initialized() || !this->parser_queue.empty(); });

		if (!this->is_initialized())
			break;

		std::unique_lock parser_queue_lock(this->parser_queue_mutex);

		auto log = this->parser_queue.front();
		this->parser_queue.pop();

		parser_queue_lock.unlock();

		std::unique_lock log_lock(log->mutex);

		log->parse_status = ParseStatus::PARSING;

		auto evtc_file_path = log->evtc_data.evtc_file_path;

		log_lock.unlock();

		EncounterData encounter_data;
		ReportData report_data;

		const auto parse_status = this->parse(evtc_file_path, encounter_data, report_data);

		log_lock.lock();

		log->parse_status = parse_status;
		log->encounter_data = encounter_data;
		log->report_data = report_data;
		log->update_view();

		if (log->parse_status == ParseStatus::PARSED)
			LOG("Successfully parsed: " + log->id, LogLevel::Info);

		log_lock.unlock();

		if (parse_status == ParseStatus::PARSED)
			global::wingman_uploader->process_auto_upload(log);
	}

	LOG("Parser shutdown", LogLevel::Info);
}

ParseStatus EliteInsights::parse(std::filesystem::path evtc_file_path, EncounterData& encounter_data, ReportData& report_data)
{
	auto _LOG = [&report_data, this](std::string message, LogLevel log_level = LogLevel::Info) -> void
		{
			report_data.error_message = message;
			LOG(message, log_level);
		};

	if (evtc_file_path.empty())
	{
		_LOG("EVTC file path is empty", LogLevel::Error);
		return ParseStatus::FAILED;
	}

	if (!this->is_initialized())
	{
		_LOG("Parser not initialized", LogLevel::Warning);
		return ParseStatus::FAILED;
	}

	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
	HANDLE read_pipe, write_pipe;

	if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0))
	{
		_LOG("Failed to create read/write pipe", LogLevel::Error);
		return ParseStatus::FAILED;
	}

	STARTUPINFOA si = {};
	PROCESS_INFORMATION pi = {};
	si.cb = sizeof(si);
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdOutput = write_pipe;
	si.hStdError = write_pipe;

	if (!std::filesystem::exists(this->output_directory))
		if (!std::filesystem::create_directories(this->output_directory))
		{
			CloseHandle(read_pipe);
			CloseHandle(write_pipe);
			_LOG("Failed to create Elite Insights output directory: " + this->output_directory.string(), LogLevel::Warning);
			return ParseStatus::FAILED;
		}

	auto command = "\"" + this->executable_file.string() + "\" -c \"" + this->settings_file.string() + "\" \"" + evtc_file_path.string() + "\"";

	if (!CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
	{
		CloseHandle(read_pipe);
		CloseHandle(write_pipe);
		_LOG("Failed to start Elite Insights", LogLevel::Warning);
		return ParseStatus::FAILED;
	}

	CloseHandle(write_pipe);

	if (WaitForSingleObject(pi.hProcess, 180000) == WAIT_TIMEOUT)
	{
		TerminateProcess(pi.hProcess, EXIT_FAILURE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(read_pipe);
		_LOG("Elite Insights parser timeout. PID: " + std::to_string(pi.dwProcessId), LogLevel::Warning);
		return ParseStatus::FAILED;
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	CHAR buffer[4096];
	DWORD read;
	std::string output;

	while (ReadFile(read_pipe, buffer, sizeof(buffer) - 1, &read, NULL))
	{
		buffer[read] = '\0';
		output += buffer;
	}

	CloseHandle(read_pipe);

	std::smatch matches;

	std::regex json_regex(R"(Generated:\s*(.+\.json)\s*)");

	if (std::regex_search(output, matches, json_regex) && matches.size() > 1)
		report_data.json_file_path = std::filesystem::path(matches[1].str());

	std::regex html_regex(R"(Generated:\s*(.+\.html)\s*)");

	if (std::regex_search(output, matches, html_regex) && matches.size() > 1)
		report_data.html_file_path = std::filesystem::path(matches[1].str());

	std::regex success_regex(R"(Parsing Successful)");
	std::regex failure_regex(R"(Parsing Failure)");

	auto valid_output = std::regex_search(output, success_regex) && !std::regex_search(output, failure_regex);

	if (valid_output && std::filesystem::exists(report_data.json_file_path) && std::filesystem::exists(report_data.html_file_path))
	{
		std::ifstream json_file(report_data.json_file_path);

		if (json_file.is_open())
		{
			try
			{
				nlohmann::json json = nlohmann::json::parse(json_file);

				auto trigger_id = 0;

				if (json.contains("triggerID"))
					trigger_id = json.at("triggerID").get<int>();

				if (json.contains("fightName"))
					encounter_data.encounter_name = json.at("fightName").get<std::string>();

				if (json.contains("recordedAccountBy"))
					encounter_data.account_name = json.at("recordedAccountBy").get<std::string>();

				if (json.contains("durationMS"))
					encounter_data.duration_ms = json.at("durationMS").get<int>();

				if (json.contains("success"))
					encounter_data.success = json.at("success").get<bool>();

				bool cm = false;

				if (json.contains("isCM"))
					cm = json.at("isCM").get<bool>();

				bool lcm = false;

				if (json.contains("isLegendaryCM"))
					lcm = json.at("isLegendaryCM").get<bool>();

				encounter_data.difficulty = cm ? EncounterDifficulty::CHALLENGE_MODE : lcm ? EncounterDifficulty::LEGENDARY_CHALLENGE_MODE : EncounterDifficulty::NORMAL_MODE;

				static const auto parse_time = [](const std::string& utc_time_str) -> std::chrono::system_clock::time_point
					{
						std::istringstream ss(utc_time_str);
						std::chrono::system_clock::time_point tp;
						ss >> std::chrono::parse("%F %T %z", tp);
						return tp;
					};

				if (json.contains("timeStartStd"))
					encounter_data.start_time = parse_time(json.at("timeStartStd").get<std::string>());

				if (json.contains("timeEndStd"))
					encounter_data.end_time = parse_time(json.at("timeEndStd").get<std::string>());


				if (trigger_id && json.contains("targets") && !json.at("targets").empty())
				{
					for (auto i = 0; i < json.at("targets").size(); i++)
					{
						auto& target = json.at("targets").at(i);

						if (target.contains("id"))
						{
							auto id = target.at("id").get<int>();

							if (id == trigger_id) // main boss
							{
								if (target.contains("healthPercentBurned"))
									encounter_data.health_percent_burned = target.at("healthPercentBurned").get<float>();

								encounter_data.valid_boss = true;

								break;
							}
						}
					}
				}
			}
			catch (const nlohmann::json::exception& e)
			{
				_LOG("Failed to parse json file: " + report_data.json_file_path.string() + " (" + std::string(e.what()) + ")", LogLevel::Warning);
				return ParseStatus::FAILED;
			}

			json_file.close();

			return ParseStatus::PARSED;
		}
		else
		{
			_LOG("Failed to open json file: " + report_data.json_file_path.string(), LogLevel::Warning);
			return ParseStatus::FAILED;
		}
	}
	else
	{
		std::regex failure_regex_message(R"(Parsing Failure - .*?: .*?: (.+))");

		if (std::regex_search(output, matches, failure_regex_message) && matches.size() > 1)
			_LOG("Parsing failed: " + matches[1].str(), LogLevel::Warning);

		return ParseStatus::FAILED;
	}

	return ParseStatus::FAILED;
}
