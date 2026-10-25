#pragma once

#include <string>
#include <string_view>

// Element ids of data/ui/*.rml. The refresh path and the click/change
// handlers share these, so an id typo fails to compile instead of
// no-opping at runtime.
namespace UiIds
{

// hud.rml
inline constexpr std::string_view ResLine = "res-line";
inline constexpr std::string_view SelLine = "sel-line";
inline constexpr std::string_view QueueLine = "queue-line";
inline constexpr std::string_view FpsLine = "fps-line";
inline constexpr std::string_view NeedProduction = "need-production";
inline constexpr std::string_view ProdQueueRow = "prod-queue-row";
inline constexpr std::string_view HudProducing = "hud-producing";
inline constexpr std::string_view ProducingFill = "producing-fill";
inline constexpr std::string_view HudHints = "hud-hints";
inline constexpr std::string_view BtnCancel = "btn-cancel";
inline constexpr std::string_view TabInfantry = "tab-infantry";
inline constexpr std::string_view TabVehicles = "tab-vehicles";
inline constexpr std::string_view TabBuildings = "tab-buildings";
inline constexpr std::string_view RowsInfantry = "rows-infantry";
inline constexpr std::string_view RowsVehicles = "rows-vehicles";
inline constexpr std::string_view RowsBuildings = "rows-buildings";
inline constexpr std::string_view AbHold = "ab-hold";
inline constexpr std::string_view AbGuard = "ab-guard";
inline constexpr std::string_view AbPatrol = "ab-patrol";
inline constexpr std::string_view AbAttackMove = "ab-attackmove";
inline constexpr std::string_view AbHalt = "ab-halt";
inline constexpr std::string_view AbRepair = "ab-repair";
inline constexpr std::string_view AbHeal = "ab-heal";
inline constexpr std::string_view AbRally = "ab-rally";
inline constexpr std::string_view AbDemolish = "ab-demolish";

// pause.rml
inline constexpr std::string_view BtnResume = "btn-resume";
inline constexpr std::string_view BtnPRemap = "btn-p-remap";
inline constexpr std::string_view BtnPToMenu = "btn-p-tomenu";
inline constexpr std::string_view BtnPToDesktop = "btn-p-todesktop";
inline constexpr std::string_view OptPCamSpeed = "opt-p-camspeed";
inline constexpr std::string_view OptPMinimap = "opt-p-minimap";
inline constexpr std::string_view OptPHints = "opt-p-hints";
inline constexpr std::string_view OptPMaster = "opt-p-master";
inline constexpr std::string_view OptPMusic = "opt-p-music";
inline constexpr std::string_view OptPSfx = "opt-p-sfx";
inline constexpr std::string_view OptPMute = "opt-p-mute";
inline constexpr std::string_view OptPRightDrag = "opt-p-rightdrag";
inline constexpr std::string_view OptPColorBlind = "opt-p-colorblind";
inline constexpr std::string_view ValPCamSpeed = "val-p-camspeed";
inline constexpr std::string_view ValPMaster = "val-p-master";
inline constexpr std::string_view ValPMusic = "val-p-music";
inline constexpr std::string_view ValPSfx = "val-p-sfx";

// outcome.rml
inline constexpr std::string_view OutcomeTitle = "outcome-title";
inline constexpr std::string_view OutcomeBody = "outcome-body";
inline constexpr std::string_view BtnOToMenu = "btn-o-tomenu";
inline constexpr std::string_view BtnOToDesktop = "btn-o-todesktop";

// confirm.rml
inline constexpr std::string_view ConfirmTitle = "confirm-title";
inline constexpr std::string_view ConfirmSub = "confirm-sub";
inline constexpr std::string_view BtnYes = "btn-yes";
inline constexpr std::string_view BtnNo = "btn-no";

// main_menu.rml
inline constexpr std::string_view BtnSetup = "btn-setup";
inline constexpr std::string_view BtnLoad = "btn-load";
inline constexpr std::string_view BtnSettings = "btn-settings";
inline constexpr std::string_view BtnEditor = "btn-editor";
inline constexpr std::string_view BtnQuit = "btn-quit";

// skirmish_setup.rml
inline constexpr std::string_view MapList = "maplist";
inline constexpr std::string_view MapDetail = "mapdetail";
inline constexpr std::string_view BtnStart = "btn-start";
inline constexpr std::string_view BtnBackRight = "btn-back-right";

// settings.rml
inline constexpr std::string_view BtnSettingsBack = "btn-settings-back";
inline constexpr std::string_view BtnRemap = "btn-remap";
inline constexpr std::string_view OptCamSpeed = "opt-camspeed";
inline constexpr std::string_view OptMinimap = "opt-minimap";
inline constexpr std::string_view OptHints = "opt-hints";
inline constexpr std::string_view OptMaster = "opt-master";
inline constexpr std::string_view OptMusic = "opt-music";
inline constexpr std::string_view OptSfx = "opt-sfx";
inline constexpr std::string_view OptMute = "opt-mute";
inline constexpr std::string_view OptRightDrag = "opt-rightdrag";
inline constexpr std::string_view OptColorBlind = "opt-colorblind";
inline constexpr std::string_view OptUiScale = "opt-uiscale";
inline constexpr std::string_view ValCamSpeed = "val-camspeed";
inline constexpr std::string_view ValMaster = "val-master";
inline constexpr std::string_view ValMusic = "val-music";
inline constexpr std::string_view ValSfx = "val-sfx";
inline constexpr std::string_view ValUiScale = "val-uiscale";

// load_game.rml
inline constexpr std::string_view BtnLoadBack = "btn-load-back";
inline constexpr std::string_view BtnReplay = "btn-replay";

// hotkey_remap.rml
inline constexpr std::string_view RemapStatus = "remap-status";
inline constexpr std::string_view HkColA = "hkcol-a";
inline constexpr std::string_view HkColB = "hkcol-b";
inline constexpr std::string_view BtnRemapBack = "btn-remap-back";

// Row families: the element id is the prefix followed by the row index.
inline constexpr std::string_view CgRow = "cg-";
inline constexpr std::string_view ProdInfantryRow = "prod-i-";
inline constexpr std::string_view ProdVehicleRow = "prod-v-";
inline constexpr std::string_view RepeatInfantryRow = "repr-i-";
inline constexpr std::string_view RepeatVehicleRow = "repr-v-";
inline constexpr std::string_view PlaceRow = "place-";
inline constexpr std::string_view DifficultyRow = "diff-";
inline constexpr std::string_view SlotRow = "slot-";
inline constexpr std::string_view MapRow = "mapitem-";
inline constexpr std::string_view RemapKeyRow = "hkkey-";
inline constexpr std::string_view RemapRow = "hkrow-";

inline std::string WithIndex(std::string_view prefix, int index)
{
	return std::string(prefix) + std::to_string(index);
}

inline bool StartsWith(std::string_view id, std::string_view prefix)
{
	return id.starts_with(prefix);
}

} // namespace UiIds
