#include "evtc_parser.h"

#include <miniz/miniz.h>
#include <fstream>

namespace global { std::unique_ptr<EVTCParser> evtc_parser = std::make_unique<EVTCParser>(); }

//#define LOG(message, log_level) global::logger->write(message, log_level, LogSource::EVTCParser)

EVTCData EVTCParser::parse(const std::filesystem::path& evtc_file_path)
{
	EVTCData evtc_data;

	if (evtc_file_path.empty())
		throw std::invalid_argument("evtc_file_path is empty");

	evtc_data.evtc_file_path = evtc_file_path;

	evtc_data.time = std::chrono::clock_cast<std::chrono::system_clock>(std::filesystem::last_write_time(evtc_file_path));

	std::vector<uint8_t> file_data;

	if (evtc_file_path.extension() == ".zevtc")
	{
		mz_zip_archive zip_archive{};

		mz_zip_zero_struct(&zip_archive);

		if (!mz_zip_reader_init_file(&zip_archive, evtc_file_path.string().c_str(), 0))
			throw std::runtime_error("Failed to open zip archive");

		mz_zip_archive_file_stat file_stat;

		if (!mz_zip_reader_file_stat(&zip_archive, 0, &file_stat))
		{
			mz_zip_reader_end(&zip_archive);
			throw std::runtime_error("Failed to get file stat from zip archive");
		}

		file_data.resize(file_stat.m_uncomp_size);

		if (!mz_zip_reader_extract_to_mem(&zip_archive, 0, file_data.data(), file_data.size(), 0))
		{
			mz_zip_reader_end(&zip_archive);
			throw std::runtime_error("Failed to extract file from zip archive");
		}

		mz_zip_reader_end(&zip_archive);
	}
	else
	{
		std::ifstream file_stream(evtc_file_path, std::ios::binary | std::ios::ate);

		if (!file_stream.is_open())
			throw std::runtime_error("Failed to open file: " + evtc_file_path.string());

		std::streamsize size = file_stream.tellg();
		file_data.resize(size);

		file_stream.seekg(0, std::ios::beg);

		if (!file_stream.read(reinterpret_cast<char*>(file_data.data()), size))
		{
			file_stream.close();
			throw std::runtime_error("Failed to read file: " + evtc_file_path.string());
		}
	}

	if (file_data.size() < 16)
		throw std::runtime_error("Invalid evtc file size");

	uint64_t index = 0;

	const auto evtc_identifier = std::string(reinterpret_cast<const char*>(file_data.data()), 4);

	if (evtc_identifier != "EVTC")
		throw std::runtime_error("Invalid evtc file header");

	index += 4; // evtc identifier
	index += 4; // version
	index += 1; // revision
	index += 4; // ?

	evtc_data.trigger_id = *reinterpret_cast<TriggerID*>(file_data.data() + index);
	index += sizeof(TriggerID);

	return evtc_data;
}

#undef LOG