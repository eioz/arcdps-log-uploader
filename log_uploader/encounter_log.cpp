#include "encounter_log.h"
#include "evtc.h"

#include <algorithm>
#include <map>

namespace global
{
	const std::map<TriggerID, std::string> trigger_id_encounter_name_map =
	{
		// World vs World
		{TriggerID::WorldVsWorld, "World vs. World"},

		// Raids
		// Spirit Vale
		{TriggerID::ValeGuardian, "Vale Guardian"},
		{TriggerID::Gorseval, "Gorseval"},
		{TriggerID::SpiritRace, "Spirit Race"},
		{TriggerID::SabethaTheSaboteur, "Sabetha the Saboteur"},
		// Salvation Pass
		{TriggerID::Slothasor, "Slothasor"},
		{TriggerID::BanditTrio, "Bandit Trio"},
		{TriggerID::MatthiasGabrel, "Matthias Gabrel"},
		// Stronghold of the Faithful
		{TriggerID::SiegeTheStronghold, "Siege the Stronghold"},
		{TriggerID::KeepConstruct, "Keep Construct"},
		{TriggerID::TwistedCastle, "Twisted Castle"},
		{TriggerID::Xera, "Xera"},
		// Bastion of the Penitent
		{TriggerID::CairnTheIndomitable, "Cairn the Indomitable"},
		{TriggerID::MursaatOverseer, "Mursaat Overseer"},
		{TriggerID::Samarog, "Samarog"},
		{TriggerID::Deimos, "Deimos"},
		// Hall of Chains
		{TriggerID::SoullessHorror, "Soulless Horror"},
		{TriggerID::RiverOfSouls, "River of Souls"},
		{TriggerID::StatueOfIce, "Statue of Ice"},
		{TriggerID::StatueOfDarkness, "Statue of Darkness"},
		{TriggerID::StatueOfDeath, "Statue of Death"},
		{TriggerID::Dhuum, "Dhuum"},
		// Mythwright Gambit
		{TriggerID::ConjuredAmalgamate, "Conjured Amalgamate"},
		{TriggerID::TwinLargos, "Twin Largos"},
		{TriggerID::Qadim, "Qadim"},
		// The Key of Ahdashim
		{TriggerID::CardinalAdina, "Cardinal Adina"},
		{TriggerID::CardinalSabir, "Cardinal Sabir"},
		{TriggerID::QadimThePeerless, "Qadim the Peerless"},
		// Mount Balrior
		{TriggerID::DecimaTheStormsinger, "Decima, the Stormsinger"},
		{TriggerID::GreerTheBlightbringer, "Greer, the Blightbringer"},
		{TriggerID::UraTheSteamshrieker, "Ura, the Steamshrieker"},

		// Fractals
		// Nightmare
		{TriggerID::MAMA, "MAMA"},
		{TriggerID::SiaxTheCorrupted, "Siax the Corrupted"},
		{TriggerID::EnsolyssOfTheEndlessTorment, "Ensolyss of the Endless Torment"},
		// Shattered Observatory
		{TriggerID::SkorvaldTheShattered, "Skorvald the Shattered"},
		{TriggerID::Artsariiv, "Artsariiv"},
		{TriggerID::Arkk, "Arkk"},
		// Sunqua Peak
		{TriggerID::AiKeeperOfThePeak, "Ai, Keeper of the Peak"},
		// Silent Surf
		{TriggerID::Kanaxai, "Kanaxai"},
		{TriggerID::KanaxaiChallengeMode, "Kanaxai CM"},
		// Lonely Tower
		{TriggerID::Eparch, "Eparch"},

		// Strike Missions
		// Core
		{TriggerID::OldLionsCourt, "Old Lion's Court"},
		{TriggerID::OldLionsCourtChallengeMode, "Old Lion's Court CM"},
		// The Icebrood Saga
		{TriggerID::IcebroodConstruct, "Icebrood Construct"},
		{TriggerID::SuperKodanBrothers, "Super Kodan Brothers"},
		{TriggerID::FraenirOfJormag, "Fraenir of Jormag"},
		{TriggerID::Boneskinner, "Boneskinner"},
		{TriggerID::WhisperOfJormag, "Whisper of Jormag"},
		// End of Dragons
		{TriggerID::AetherbladeHideout, "Aetherblade Hideout"},
		{TriggerID::XunlaiJadeJunkyard, "Xunlai Jade Junkyard"},
		{TriggerID::KainengOverlook, "Kaineng Overlook"},
		{TriggerID::KainengOverlookChallengeMode, "Kaineng Overlook CM"},
		{TriggerID::HarvestTemple, "Harvest Temple"},
		// Secrets of the Obscure
		{TriggerID::CosmicObservatory, "Cosmic Observatory"},
		{TriggerID::TempleOfFebe, "Temple of Febe"},

		//Other
		// Convergences
		{TriggerID::DemonKnight, "Demon Knight"},
		{TriggerID::Sorrow, "Sorrow"},
		{TriggerID::Dreadwing, "Dreadwing"},
		{TriggerID::HellSister, "Hell Sister"},
		{TriggerID::Umbriel, "Umbriel, Halberd of House Aurkus"},

		// Special Forces Training Area
		{TriggerID::StandardKittyGolem, "Standard Kitty Golem"},
		{TriggerID::MediumKittyGolem, "Medium Kitty Golem"},
		{TriggerID::LargeKittyGolem, "Large Kitty Golem"},

		// Open World
		{TriggerID::SooWon, "Soo-Won"},

		// Uncategorized
		{TriggerID::Freezie, "Freezie"},
		{TriggerID::DregShark, "Dreg Shark"},
		{TriggerID::HeartsAndMinds, "Hearts and Minds"},
	};
}

EncounterLog::EncounterLog(EVTCData evtc_data)
{
	auto generate_id = [](const std::filesystem::path& path) -> std::string
		{
			auto marker = std::find(path.begin(), path.end(), "arcdps.cbtlogs");

			if (marker != path.end() && ++marker != path.end())
			{
				std::filesystem::path result;

				for (auto it = marker; it != path.end(); ++it)
					result /= *it;

				return result.string();
			}

			return path.string(); // fallback
		};

	this->id = generate_id(evtc_data.evtc_file_path);
	this->evtc_data = evtc_data;
	this->update_view();
}

EncounterLog::~EncounterLog()
{
	if (std::filesystem::exists(this->report_data.html_file_path))
		std::filesystem::remove(this->report_data.html_file_path);

	if (std::filesystem::exists(this->report_data.json_file_path))
		std::filesystem::remove(this->report_data.json_file_path);
}

void EncounterLogData::update_view()
{
	auto& view = this->view;

	// after evtc parsing
	if (this->parse_status == ParseStatus::UNPARSED)
	{
		auto sys_time = std::chrono::clock_cast<std::chrono::system_clock>(this->evtc_data.time);
		std::chrono::zoned_time local_time{ std::chrono::current_zone(), sys_time };

		view.time = std::format("{:%H:%M}", local_time);

		{
			auto it = global::trigger_id_encounter_name_map.find(this->evtc_data.trigger_id);

			if (it != global::trigger_id_encounter_name_map.end())
				view.name = it->second;
			else
				view.name = "Undefined";
		}
	}
	// after elite insights parsing
	else if (this->parse_status == ParseStatus::PARSED)
	{
		auto sys_time = std::chrono::clock_cast<std::chrono::system_clock>(this->encounter_data.end_time);
		std::chrono::zoned_time local_time{ std::chrono::current_zone(), sys_time };

		view.time = std::format("{:%H:%M}", local_time);
		view.name = encounter_data.encounter_name;
		view.result = encounter_data.success ? "Success" : encounter_data.valid_boss ? std::format("{:.2f}%", 100.f - encounter_data.health_percent_burned) : "Failure";

		auto update_duration = [&]()
			{
				auto minutes = encounter_data.duration_ms / (60 * 1000);
				auto seconds = (encounter_data.duration_ms / 1000) % 60;
				auto milliseconds = encounter_data.duration_ms % 1000;

				std::ostringstream oss;
				if (minutes > 0)
					oss << std::setfill('0') << std::setw(1) << minutes << "m ";
				oss << std::setfill('0') << std::setw(1) << seconds << "s ";
				oss << std::setfill('0') << std::setw(1) << milliseconds << "ms";

				view.duration = oss.str();
			};

		update_duration();
	}
}