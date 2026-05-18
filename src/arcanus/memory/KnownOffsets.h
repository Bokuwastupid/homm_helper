#pragma once

#include <cstddef>
#include <cstdint>

namespace arcanus::offsets {

constexpr std::uintptr_t Il2CppPreferredImageBase = 0x180000000ull;

namespace rva {
constexpr std::uintptr_t DataSave = 0x20F1A00ull;
constexpr std::uintptr_t DataCtor = 0x20F1B70ull;
constexpr std::uintptr_t DataInit = 0x20F1B90ull;
constexpr std::uintptr_t DataLoadMap = 0x20F2E20ull;
constexpr std::uintptr_t SideInit = 0x2115060ull;
constexpr std::uintptr_t TransferBattleGetSide = 0x2020FF0ull;
constexpr std::uintptr_t TransferBattleIsValid = 0x2021350ull;
constexpr std::uintptr_t TransferSideUnitCount = 0x20228B0ull;
constexpr std::uintptr_t WorldCameraInit = 0x29E7FC0ull;
constexpr std::uintptr_t WorldCameraUpdate = 0x29E8200ull;
constexpr std::uintptr_t MapNodeToWorld = 0x2939270ull;
constexpr std::uintptr_t MapCellToWorld = 0x2939550ull;
constexpr std::uintptr_t CameraWorldToScreenPoint = 0x3F2C2F0ull;
constexpr std::uintptr_t CameraWorldToScreenPointInjected = 0x3F2C280ull;
}

namespace il2cpp {
constexpr std::size_t ObjectHeader = 0x10;
constexpr std::size_t ArrayLength = 0x18;
constexpr std::size_t ArrayItems = 0x20;
constexpr std::size_t ListItems = 0x10;
constexpr std::size_t ListSize = 0x18;
constexpr std::size_t StringLength = 0x10;
constexpr std::size_t StringChars = 0x14;
}

namespace data {
constexpr std::size_t Objects = 0x10;
constexpr std::size_t Sides = 0x20;
constexpr std::size_t Heroes = 0x28;
constexpr std::size_t Squads = 0x30;
constexpr std::size_t TurnMode = 0x60;
constexpr std::size_t DaysInGameCount = 0x98;
constexpr std::size_t Day = 0x9C;
constexpr std::size_t Week = 0xA0;
constexpr std::size_t Month = 0xA4;
constexpr std::size_t EnableFactionLaws = 0xC8;
}

namespace data_init_context {
constexpr std::size_t StartInfo = 0x10;
constexpr std::size_t Random = 0x18;
constexpr std::size_t MapData = 0x20;
}

namespace data_turn_mode {
constexpr std::size_t TurnPhase = 0x10;
constexpr std::size_t TurnMode = 0x14;
constexpr std::size_t SwitchTurnModeIsStartDay = 0x18;
constexpr std::size_t IsWasAreaInteruption = 0x19;
constexpr std::size_t CurrentSideIndex = 0x38;
constexpr std::size_t NextMode = 0x48;
}

namespace map_data {
constexpr std::size_t FileMapName = 0x10;
constexpr std::size_t MapName = 0x18;
constexpr std::size_t SizeX = 0x34;
constexpr std::size_t SizeZ = 0x38;
constexpr std::size_t Objects = 0x78;
}

namespace map_data_objects {
constexpr std::size_t Sid = 0x10;
constexpr std::size_t Ids = 0x18;
constexpr std::size_t Nodes = 0x20;
}

namespace world_camera {
constexpr std::size_t TargetCamera = 0x30;
constexpr std::size_t LookAt = 0x38;
constexpr std::size_t CameraZoom = 0x60;
constexpr std::size_t Map = 0x80;
}

namespace data_sides {
constexpr std::size_t MyIndex = 0x10;
constexpr std::size_t SideArray = 0x20;
}

namespace data_heroes {
constexpr std::size_t FreeId = 0x10;
constexpr std::size_t List = 0x18;
constexpr std::size_t Pool = 0x20;
constexpr std::size_t ById = 0x28;
constexpr std::size_t BySid = 0x30;
}

namespace data_objects {
constexpr std::size_t ResObjs = 0x10;
constexpr std::size_t TodoObjs = 0x18;
constexpr std::size_t HireObjs = 0x20;
constexpr std::size_t ItemObjs = 0x28;
constexpr std::size_t EventBankObjs = 0x30;
constexpr std::size_t ResMines = 0x38;
constexpr std::size_t CityObjs = 0x40;
constexpr std::size_t MarketObjs = 0x48;
constexpr std::size_t TavernObjs = 0x50;
constexpr std::size_t PortalObjs = 0x58;
constexpr std::size_t BlockObjs = 0x60;
constexpr std::size_t ChestObjs = 0x68;
constexpr std::size_t PrisonObjs = 0x70;
constexpr std::size_t ResTradeLabs = 0x78;
constexpr std::size_t Outposts = 0x80;
constexpr std::size_t Garrisons = 0x88;
constexpr std::size_t ItemMarkets = 0x90;
constexpr std::size_t RandomHires = 0x98;
constexpr std::size_t UnitUpgrades = 0xA0;
constexpr std::size_t EternalDragons = 0xA8;
constexpr std::size_t InsarasEyes = 0xB0;
constexpr std::size_t Chimerologists = 0xB8;
constexpr std::size_t SacrificialShrines = 0xC0;
constexpr std::size_t GladiatorArenas = 0xC8;
constexpr std::size_t Mirages = 0xD0;
constexpr std::size_t FickleShrines = 0xD8;
constexpr std::size_t MagicMines = 0xE0;
constexpr std::size_t TownGates = 0xE8;
constexpr std::size_t UnitResTradeLabs = 0xF0;
constexpr std::size_t PocketDimensions = 0xF8;
constexpr std::size_t AllObjects = 0x100;
}

namespace data_object {
constexpr std::size_t IdMapObject = 0x10;
constexpr std::size_t SidConfig = 0x18;
constexpr std::size_t Released = 0x20;
constexpr std::size_t OwnerSide = 0x24;
constexpr std::size_t IsNeutralObj = 0x28;
constexpr std::size_t Properties = 0x30;
constexpr std::size_t AiValue = 0x38;
constexpr std::size_t GarnisonParty = 0x40;
constexpr std::size_t RewardSet = 0x48;
constexpr std::size_t SideScoutings = 0x50;
}

namespace side {
constexpr std::size_t Name = 0x18;
constexpr std::size_t Index = 0x20;
constexpr std::size_t Res = 0x30;
constexpr std::size_t RewardSets = 0x40;
constexpr std::size_t FractionLaws = 0x58;
constexpr std::size_t SideHeroes = 0x68;
constexpr std::size_t CurrentState = 0x88;
constexpr std::size_t Fraction = 0x98;
constexpr std::size_t CurrentLevel = 0xB8;
constexpr std::size_t CurrentExp = 0xBC;
constexpr std::size_t LastSelectedHero = 0x100;
}

namespace side_heroes {
constexpr std::size_t Heroes = 0x10;
}

namespace res_heap {
constexpr std::size_t Gold = 0x10;
constexpr std::size_t Wood = 0x18;
constexpr std::size_t Ore = 0x20;
constexpr std::size_t Gemstones = 0x28;
constexpr std::size_t Crystals = 0x30;
constexpr std::size_t Mercury = 0x38;
constexpr std::size_t Dust = 0x40;
}

namespace resource {
constexpr std::size_t Name = 0x10;
constexpr std::size_t Value = 0x18;
}

namespace data_reward_sets {
constexpr std::size_t List = 0x10;
constexpr std::size_t FreeId = 0x18;
}

namespace data_reward_set {
constexpr std::size_t Rewards = 0x30;
constexpr std::size_t Id = 0x38;
constexpr std::size_t HeroId = 0x3C;
constexpr std::size_t ObjectId = 0x40;
constexpr std::size_t Label = 0x48;
constexpr std::size_t SelectionWindowType = 0x50;
}

namespace data_reward {
constexpr std::size_t StringRewardType = 0x10;
constexpr std::size_t RewardType = 0x18;
constexpr std::size_t RewardShowType = 0x1C;
constexpr std::size_t RewardName = 0x30;
constexpr std::size_t RewardDescription = 0x38;
constexpr std::size_t Parameters = 0x48;
}

namespace hero {
constexpr std::size_t Sid = 0x10;
constexpr std::size_t Level = 0x20;
constexpr std::size_t Experience = 0x24;
constexpr std::size_t StatsByLevel = 0x48;
constexpr std::size_t AdditionalStats = 0x50;
constexpr std::size_t Magics = 0x58;
constexpr std::size_t Skills = 0x60;
}

namespace session_hero {
constexpr std::size_t Id = 0x10;
constexpr std::size_t SideId = 0x14;
constexpr std::size_t Node = 0x18;
constexpr std::size_t RotationAngle = 0x1C;
constexpr std::size_t InBattle = 0x20;
constexpr std::size_t Status = 0x21;
constexpr std::size_t ConfigSid = 0x28;
constexpr std::size_t MountSid = 0x30;
constexpr std::size_t WorldMovePoints = 0x48;
constexpr std::size_t CurrentLevel = 0x4C;
constexpr std::size_t CurrentExp = 0x50;
constexpr std::size_t Mana = 0x54;
constexpr std::size_t Party = 0x68;
constexpr std::size_t StatsByLevel = 0x80;
constexpr std::size_t AdditionalStats = 0x88;
constexpr std::size_t Skills = 0x90;
constexpr std::size_t Magics = 0xA0;
}

namespace data_party {
constexpr std::size_t Units = 0x10;
}

namespace data_party_unit {
constexpr std::size_t Sid = 0x10;
constexpr std::size_t Amount = 0x18;
}

namespace hero_stat {
constexpr std::size_t Offence = 0x10;
constexpr std::size_t Defence = 0x14;
constexpr std::size_t SpellPower = 0x18;
constexpr std::size_t Intelligence = 0x1C;
constexpr std::size_t Luck = 0x20;
constexpr std::size_t Moral = 0x24;
constexpr std::size_t MovementBonus = 0x44;
}

namespace transfer_battle {
constexpr std::size_t BattleType = 0x10;
constexpr std::size_t ArenaLogicSid = 0x18;
constexpr std::size_t Sides = 0x40;
}

namespace transfer_side {
constexpr std::size_t Name = 0x10;
constexpr std::size_t EntityIdInWorld = 0x1C;
constexpr std::size_t SideIdInWorld = 0x20;
constexpr std::size_t TypeInWorld = 0x24;
constexpr std::size_t ControlType = 0x28;
constexpr std::size_t Units = 0x30;
constexpr std::size_t Hero = 0x48;
}

namespace transfer_unit {
constexpr std::size_t ConfigSid = 0x10;
constexpr std::size_t Stacks = 0x18;
constexpr std::size_t SlotPos = 0x1C;
constexpr std::size_t UnitLogicConfig = 0x28;
}

namespace unit_logic_config {
constexpr std::size_t BaseSid = 0x18;
constexpr std::size_t UpgradeSid = 0x20;
constexpr std::size_t SquadValue = 0x4C;
constexpr std::size_t Tier = 0x50;
constexpr std::size_t Fraction = 0x58;
constexpr std::size_t Stats = 0x70;
constexpr std::size_t Abilities = 0x78;
constexpr std::size_t Passives = 0x80;
constexpr std::size_t DefaultAttacks = 0x98;
constexpr std::size_t UnitCost = 0xB8;
}

namespace unit_stat {
constexpr std::size_t ActionPoints = 0x28;
constexpr std::size_t DamageMin = 0x2C;
constexpr std::size_t DamageMax = 0x30;
constexpr std::size_t Defence = 0x34;
constexpr std::size_t EnergyPerCast = 0x38;
constexpr std::size_t Offence = 0x48;
constexpr std::size_t Hp = 0x50;
constexpr std::size_t Initiative = 0x54;
constexpr std::size_t Luck = 0x88;
}

}
