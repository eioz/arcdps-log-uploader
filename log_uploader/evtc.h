#pragma once

#include <cstdint>

enum class TriggerID : uint16_t
{
	Invalid = 0,

	// World vs World
	WorldVsWorld = 1,

	// Raids
	// Spirit Vale
	ValeGuardian = 15438,
	Gorseval = 15429,
	SpiritRace = 15415,
	SabethaTheSaboteur = 15375,
	// Salvation Pass
	Slothasor = 16123,
	BanditTrio = 16088,
	MatthiasGabrel = 16115,
	// Stronghold of the Faithful
	SiegeTheStronghold = 16253,
	KeepConstruct = 16235,
	TwistedCastle = 16247,
	Xera = 16246,
	// Bastion of the Penitent
	CairnTheIndomitable = 17194,
	MursaatOverseer = 17172,
	Samarog = 17188,
	Deimos = 17154,
	// Hall of Chains
	SoullessHorror = 19767,
	RiverOfSouls = 19828,
	StatueOfIce = 19691, // broken king
	StatueOfDarkness = 19844, // eyes
	StatueOfDeath = 19536, // eater of souls
	Dhuum = 19450,
	// Mythwright Gambit
	ConjuredAmalgamate = 43974,
	TwinLargos = 21105,
	Qadim = 20934,
	// The Key of Ahdashim
	CardinalAdina = 22006,
	CardinalSabir = 21964,
	QadimThePeerless = 22000,
	// Mount Balrior
	DecimaTheStormsinger = 26774,
	GreerTheBlightbringer = 26725,
	UraTheSteamshrieker = 26712,

	// Fractals
	// Nightmare
	MAMA = 17021,
	SiaxTheCorrupted = 17028,
	EnsolyssOfTheEndlessTorment = 16948,
	// Shattered Observatory
	SkorvaldTheShattered = 17632,
	Artsariiv = 17949,
	Arkk = 17759,
	// Sunqua Peak
	AiKeeperOfThePeak = 23254,
	//Silent Surf
	Kanaxai = 25572,
	KanaxaiChallengeMode = 25577,
	// Lonely Tower
	Eparch = 26231,

	// Strike Missions
	// Core
	OldLionsCourt = 25413,
	OldLionsCourtChallengeMode = 25414,
	// The Icebrood Saga
	IcebroodConstruct = 22154,
	SuperKodanBrothers = 22343,
	FraenirOfJormag = 22492,
	Boneskinner = 22521,
	WhisperOfJormag = 22711,
	// End of Dragons
	AetherbladeHideout = 24033,
	XunlaiJadeJunkyard = 23957,
	KainengOverlook = 24485,
	KainengOverlookChallengeMode = 24266,
	HarvestTemple = 43488,
	// Secrets of the Obscure
	CosmicObservatory = 25705,
	TempleOfFebe = 25989,

	// Other
	// Convergences
	DemonKnight = 26142,
	Sorrow = 26143,
	Dreadwing = 26161,
	HellSister = 26146,
	Umbriel = 26196,

	// Special Forces Training Area
	StandardKittyGolem = 16199,
	MediumKittyGolem = 19645,
	LargeKittyGolem = 19676,

	//Open World
	SooWon = 35552,

	// Uncategorized
	Freezie = 21333,
	DregShark = 21181,
	HeartsAndMinds = 15884,
};