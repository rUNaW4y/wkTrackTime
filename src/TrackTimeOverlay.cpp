#include "TrackTimeOverlay.h"
#include "Config.h"
#include "Diagnostics.h"
#include "GameTextRenderer.h"
#include "Hooks.h"
#include "W2App.h"
#include "entities/CTask.h"
#include "entities/CTaskTeam.h"
#include "entities/CTaskTurnGame.h"
#include "entities/CTaskWorm.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <array>
#include <format>
#include <map>
#include <string>
#include <vector>

namespace {
	constexpr int FixedPointScale = 0x10000;
	constexpr int DrawBitmap2DOriginXOffsetPixels = 160;
	constexpr int DrawBitmap2DOriginYOffsetPixels = 8;
	constexpr DWORD DDMainTeamCountOffset = 0x44C;
	constexpr DWORD DDMainTeamDataOffset = 0x450;
	constexpr DWORD TeamDataStride = 0xBB8;
	constexpr DWORD TeamDataOwnerMachineOffset = 0x0;
	constexpr DWORD TeamDataNameOffset = 0x6;
	constexpr DWORD DDMainPlayerNameOffset = 0x4;
	constexpr DWORD DDMainPlayerNameStride = 0x50;
	constexpr DWORD GameGlobalTeamWormsOffset = 0x4188;
	constexpr DWORD TeamStateStride = 1308;
	constexpr DWORD WormStateStride = 156;
	constexpr DWORD GameSceneOffset = 0x524;
	constexpr DWORD ScreenWidthOffset = 0x77AC;
	constexpr DWORD ScreenHeightOffset = 0x77B4;
	constexpr int MaxTeams = 8;
	constexpr int WormsPerTeam = 8;
	constexpr char EmptyPlayerTimes[] = "/";
	constexpr int RecordedElapsedBiasDivisor = 50;

	void (__stdcall *origRenderDrawingQueue)(DWORD dddisplay, DWORD a3);
	int (__stdcall *origTurnGameRenderScene)();

	struct CurrentTurnState {
		bool valid = false;
		CTaskTurnGame *turnGame = nullptr;
		CTaskWorm *worm = nullptr;
		int ownerMachineId = -1;
		std::string playerName;
		int teamNumber = 0;
		int wormNumber = 0;
		int timerRawPrimary = -1;
		int timerRawSecondary = -1;
		int timerRaw = -1;
		bool paused = false;
		bool beforeRoundStart = false;
	};

	struct ActiveTurnCapture {
		bool active = false;
		int participantKey = -1;
		int ownerMachineId = -1;
		std::string playerName;
		int teamNumber = 0;
		int wormNumber = 0;
		int startRaw = -1;
		int lastObservedRaw = -1;
		int lowestObservedRaw = -1;
	};

	struct WormTimeRecord {
		int teamNumber = 0;
		int wormNumber = 0;
		int elapsedRaw = 0;
	};

	struct ParticipantState {
		int participantKey = -1;
		int ownerMachineId = -1;
		int teamNumber = 0;
		std::string playerName;
		bool startedTurn = false;
		std::vector<WormTimeRecord> records;
		std::vector<unsigned int> completedWormKeys;
	};

	struct OverlayLine {
		std::string text;
		GameTextRenderer::TextStyle style;
		int extraSpacingPixels = 0;
	};

	struct TeamStyle {
		bool stableObserved = false;
		bool liveObserved = false;
		int stableTextColor = 0;
		int stableBackgroundColor = 0;
		int stableFrameColor = 0;
		int liveTextColor = 0;
		int liveBackgroundColor = 0;
		int liveFrameColor = 0;
	};

	std::vector<int> participantOrder;
	std::map<int, ParticipantState> participantStates;
	ActiveTurnCapture activeTurn;
	int missingTurnFrames = 0;
	std::array<TeamStyle, MaxTeams + 1> teamStyles = {};

	std::string readCString(DWORD address) {
		if (!address) {
			return {};
		}

		const char *text = reinterpret_cast<const char *>(address);
		return text ? std::string(text) : std::string();
	}

	int getParticipantKey(int ownerMachineId, int teamNumber) {
		return ownerMachineId >= 0 ? ownerMachineId : 0x100 + teamNumber;
	}

	unsigned int buildCompletedWormKey(int teamNumber, int wormNumber) {
		return (static_cast<unsigned int>(teamNumber & 0xFFFF) << 16) | static_cast<unsigned int>(wormNumber & 0xFFFF);
	}

	bool containsCompletedWormKey(const ParticipantState &participant, unsigned int wormKey) {
		return std::find(participant.completedWormKeys.begin(), participant.completedWormKeys.end(), wormKey) != participant.completedWormKeys.end();
	}

	int chooseTimerRaw(int primaryRaw, int secondaryRaw) {
		int chosen = -1;
		bool sawZero = false;
		for (int candidate : {primaryRaw, secondaryRaw}) {
			if (candidate > 0 && (chosen < 0 || candidate < chosen)) {
				chosen = candidate;
			} else if (candidate == 0) {
				sawZero = true;
			}
		}
		if (chosen < 0 && sawZero) {
			chosen = 0;
		}
		return chosen;
	}

	int getWormHp(DWORD gameGlobal, int teamIndex, int wormIndex) {
		if (!gameGlobal) {
			return 0;
		}

		DWORD teamNumber = static_cast<DWORD>(teamIndex + 1);
		DWORD hpAddress = gameGlobal + GameGlobalTeamWormsOffset + TeamStateStride * teamNumber + WormStateStride * wormIndex;
		return static_cast<int>(*reinterpret_cast<WORD *>(hpAddress));
	}

	bool isTeamPresentInRound(DWORD gameGlobal, int teamIndex) {
		for (int wormIndex = 0; wormIndex < WormsPerTeam; ++wormIndex) {
			if (getWormHp(gameGlobal, teamIndex, wormIndex) > 0) {
				return true;
			}
		}
		return false;
	}

	std::string resolvePlayerName(int ownerMachineId, int teamNumber) {
		DWORD ddMain = W2App::getAddrDDMain();
		if (ddMain && ownerMachineId >= 0) {
			std::string ownerName = readCString(ddMain + DDMainPlayerNameOffset + ownerMachineId * DDMainPlayerNameStride);
			if (!ownerName.empty()) {
				return ownerName;
			}
		}

		if (ddMain && teamNumber > 0 && teamNumber <= MaxTeams) {
			DWORD teamData = ddMain + DDMainTeamDataOffset + static_cast<DWORD>(teamNumber - 1) * TeamDataStride;
			std::string teamName = readCString(teamData + TeamDataNameOffset);
			if (!teamName.empty()) {
				return teamName;
			}
		}

		return std::format("Player {}", teamNumber > 0 ? teamNumber : ownerMachineId + 1);
	}

	void registerParticipant(int ownerMachineId, int teamNumber, const std::string &playerName) {
		int participantKey = getParticipantKey(ownerMachineId, teamNumber);
		auto [it, inserted] = participantStates.emplace(participantKey, ParticipantState{});
		ParticipantState &participant = it->second;
		if (inserted) {
			participant.participantKey = participantKey;
			participant.ownerMachineId = ownerMachineId;
			participant.teamNumber = teamNumber;
		} else if (participant.teamNumber == 0 && teamNumber > 0) {
			participant.teamNumber = teamNumber;
		}

		if (participant.playerName.empty()) {
			participant.playerName = !playerName.empty() ? playerName : resolvePlayerName(ownerMachineId, teamNumber);
		} else if (!playerName.empty()) {
			participant.playerName = playerName;
		}
	}

	void registerParticipantTurnStart(int participantKey) {
		auto it = participantStates.find(participantKey);
		if (it == participantStates.end()) {
			return;
		}

		ParticipantState &participant = it->second;
		if (participant.startedTurn) {
			return;
		}

		participant.startedTurn = true;
		participantOrder.push_back(participantKey);
	}

	void syncParticipantsFromRoundState() {
		DWORD ddMain = W2App::getAddrDDMain();
		DWORD gameGlobal = W2App::getAddrGameGlobal();
		if (!ddMain || !gameGlobal) {
			return;
		}

		int teamCount = std::clamp(static_cast<int>(*reinterpret_cast<unsigned char *>(ddMain + DDMainTeamCountOffset)), 0, MaxTeams);
		for (int teamIndex = 0; teamIndex < teamCount; ++teamIndex) {
			if (!isTeamPresentInRound(gameGlobal, teamIndex)) {
				continue;
			}

			DWORD teamData = ddMain + DDMainTeamDataOffset + static_cast<DWORD>(teamIndex) * TeamDataStride;
			int ownerMachineId = static_cast<int>(*reinterpret_cast<unsigned char *>(teamData + TeamDataOwnerMachineOffset));
			int teamNumber = teamIndex + 1;
			registerParticipant(ownerMachineId, teamNumber, resolvePlayerName(ownerMachineId, teamNumber));
		}
	}

	std::vector<CTaskWorm *> collectWorms(CTask *root) {
		std::vector<CTaskWorm *> worms;
		if (!root) {
			return worms;
		}

		root->traverse([&](CTask *task, const int) {
			if (task && task->classtype == Constants::ClassType_Task_Worm) {
				worms.push_back(reinterpret_cast<CTaskWorm *>(task));
			}
		});
		return worms;
	}

	CTaskWorm *findActiveWorm(const std::vector<CTaskWorm *> &worms) {
		for (auto *worm : worms) {
			if (!worm) {
				continue;
			}
			if (!worm->isActive()) {
				continue;
			}
			if (worm->getTeamNumber() <= 0 || worm->getWormNumber() <= 0) {
				continue;
			}
			return worm;
		}
		return nullptr;
	}

	CurrentTurnState resolveCurrentTurnState() {
		CurrentTurnState state;
		DWORD turnGameAddr = W2App::getAddrTurnGameObject();
		if (!turnGameAddr) {
			return state;
		}

		auto *turnGame = reinterpret_cast<CTaskTurnGame *>(turnGameAddr);
		auto worms = collectWorms(turnGame);
		CTaskWorm *worm = findActiveWorm(worms);
		if (!worm || !worm->parent || worm->parent->classtype != Constants::ClassType_Task_Team) {
			return state;
		}

		auto *team = reinterpret_cast<CTaskTeam *>(worm->parent);
		int teamNumber = worm->getTeamNumber();
		int ownerMachineId = team->getOwnerMachineId();
		std::string playerName = resolvePlayerName(ownerMachineId, teamNumber);
		int primaryRaw = turnGame->getIntAtOffset(static_cast<DWORD>(Config::getPrimaryTimerOffset()));
		int secondaryRaw = turnGame->getIntAtOffset(static_cast<DWORD>(Config::getSecondaryTimerOffset()));

		state.valid = true;
		state.turnGame = turnGame;
		state.worm = worm;
		state.ownerMachineId = ownerMachineId;
		state.playerName = playerName;
		state.teamNumber = teamNumber;
		state.wormNumber = worm->getWormNumber();
		state.timerRawPrimary = primaryRaw;
		state.timerRawSecondary = secondaryRaw;
		state.timerRaw = chooseTimerRaw(primaryRaw, secondaryRaw);
		state.paused = turnGame->isTurnPaused();
		state.beforeRoundStart = turnGame->isBeforeRoundStart();
		return state;
	}

	bool isSameTurn(const ActiveTurnCapture &active, const CurrentTurnState &current) {
		return active.active &&
			active.teamNumber == current.teamNumber &&
			active.wormNumber == current.wormNumber;
	}

	std::string formatElapsedSeconds(int elapsedRaw) {
		int ticksPerSecond = Config::getTimerTicksPerSecond();
		if (ticksPerSecond <= 0) {
			return std::format("{}t", elapsedRaw);
		}

		double elapsedSeconds = static_cast<double>(elapsedRaw) / static_cast<double>(ticksPerSecond);
		return std::format("{:.2f}s", elapsedSeconds);
	}

	int getRecordedElapsedBiasRaw() {
		int ticksPerSecond = Config::getTimerTicksPerSecond();
		if (ticksPerSecond <= 0) {
			return 20;
		}

		return std::max(1, ticksPerSecond / RecordedElapsedBiasDivisor);
	}

	int getStableTeamTextColor(int teamNumber) {
		if (teamNumber > 0 && teamNumber <= MaxTeams) {
			const TeamStyle &style = teamStyles[teamNumber];
			if (style.stableObserved) {
				return style.stableTextColor;
			}
			if (style.liveObserved) {
				return style.liveTextColor;
			}
		}
		return Config::getTextColor();
	}

	int getStableTeamBackgroundColor(int teamNumber) {
		if (teamNumber > 0 && teamNumber <= MaxTeams) {
			const TeamStyle &style = teamStyles[teamNumber];
			if (style.stableObserved) {
				return style.stableBackgroundColor;
			}
			if (style.liveObserved) {
				return style.liveBackgroundColor;
			}
		}
		return Config::getTextBackground();
	}

	int getStableTeamFrameColor(int teamNumber) {
		if (teamNumber > 0 && teamNumber <= MaxTeams) {
			const TeamStyle &style = teamStyles[teamNumber];
			if (style.stableObserved) {
				return style.stableFrameColor;
			}
			if (style.liveObserved) {
				return style.liveFrameColor;
			}
		}
		return Config::getTextFrame();
	}

	int getLiveTeamTextColor(int teamNumber) {
		if (teamNumber > 0 && teamNumber <= MaxTeams) {
			const TeamStyle &style = teamStyles[teamNumber];
			if (style.liveObserved) {
				return style.liveTextColor;
			}
			if (style.stableObserved) {
				return style.stableTextColor;
			}
		}
		return Config::getTextColor();
	}

	int getLiveTeamBackgroundColor(int teamNumber) {
		if (teamNumber > 0 && teamNumber <= MaxTeams) {
			const TeamStyle &style = teamStyles[teamNumber];
			if (style.liveObserved) {
				return style.liveBackgroundColor;
			}
			if (style.stableObserved) {
				return style.stableBackgroundColor;
			}
		}
		return Config::getTextBackground();
	}

	int getLiveTeamFrameColor(int teamNumber) {
		if (teamNumber > 0 && teamNumber <= MaxTeams) {
			const TeamStyle &style = teamStyles[teamNumber];
			if (style.liveObserved) {
				return style.liveFrameColor;
			}
			if (style.stableObserved) {
				return style.stableFrameColor;
			}
		}
		return Config::getTextFrame();
	}

	GameTextRenderer::TextStyle buildPlayerNameStyle(int teamNumber, bool activeTurnName) {
		GameTextRenderer::TextStyle style = GameTextRenderer::getDefaultStyle();
		style.textColor = activeTurnName ? getLiveTeamTextColor(teamNumber) : getStableTeamTextColor(teamNumber);
		style.backgroundColor = activeTurnName ? getLiveTeamBackgroundColor(teamNumber) : getStableTeamBackgroundColor(teamNumber);
		style.frameColor = activeTurnName ? getLiveTeamFrameColor(teamNumber) : getStableTeamFrameColor(teamNumber);
		return style;
	}

	GameTextRenderer::TextStyle buildPlayerTimesStyle(int teamNumber) {
		GameTextRenderer::TextStyle style = GameTextRenderer::getDefaultStyle();
		style.backgroundColor = getStableTeamBackgroundColor(teamNumber);
		style.frameColor = getStableTeamFrameColor(teamNumber);
		return style;
	}

	void finalizeActiveTurn(const char *reason) {
		if (!activeTurn.active) {
			return;
		}

		registerParticipant(activeTurn.ownerMachineId, activeTurn.teamNumber, activeTurn.playerName);
		int participantKey = activeTurn.participantKey;
		ParticipantState &participant = participantStates[participantKey];
		if (participant.playerName.empty()) {
			participant.playerName = resolvePlayerName(activeTurn.ownerMachineId, activeTurn.teamNumber);
		}

		int endRaw = activeTurn.lowestObservedRaw >= 0 ? activeTurn.lowestObservedRaw : activeTurn.lastObservedRaw;
		int elapsedRaw = (activeTurn.startRaw >= 0 && endRaw >= 0) ? std::max(0, activeTurn.startRaw - endRaw) : -1;
		if (elapsedRaw >= 0) {
			// The captured stop value consistently lands one 0.02s step before the frozen on-screen timer.
			elapsedRaw += getRecordedElapsedBiasRaw();
		}
		unsigned int wormKey = buildCompletedWormKey(activeTurn.teamNumber, activeTurn.wormNumber);
		bool duplicate = containsCompletedWormKey(participant, wormKey);
		if (elapsedRaw >= 0 && !duplicate) {
			participant.records.push_back({activeTurn.teamNumber, activeTurn.wormNumber, elapsedRaw});
			participant.completedWormKeys.push_back(wormKey);
		}

		Diagnostics::log(
			"turn end: reason=%s player=%s team=%d worm=%d start=%d end=%d elapsed=%d duplicate=%d",
			reason,
			participant.playerName.c_str(),
			activeTurn.teamNumber,
			activeTurn.wormNumber,
			activeTurn.startRaw,
			endRaw,
			elapsedRaw,
			duplicate ? 1 : 0);

		activeTurn = ActiveTurnCapture{};
		missingTurnFrames = 0;
	}

	void startActiveTurn(const CurrentTurnState &current) {
		registerParticipant(current.ownerMachineId, current.teamNumber, current.playerName);
		if (current.timerRaw < 0) {
			return;
		}

		registerParticipantTurnStart(getParticipantKey(current.ownerMachineId, current.teamNumber));
		activeTurn.active = true;
		activeTurn.participantKey = getParticipantKey(current.ownerMachineId, current.teamNumber);
		activeTurn.ownerMachineId = current.ownerMachineId;
		activeTurn.playerName = current.playerName;
		activeTurn.teamNumber = current.teamNumber;
		activeTurn.wormNumber = current.wormNumber;
		activeTurn.startRaw = current.timerRaw;
		activeTurn.lastObservedRaw = current.timerRaw;
		activeTurn.lowestObservedRaw = current.timerRaw;
		missingTurnFrames = 0;

		Diagnostics::log(
			"turn start: player=%s team=%d worm=%d timer=%d primary=%d secondary=%d",
			current.playerName.c_str(),
			current.teamNumber,
			current.wormNumber,
			current.timerRaw,
			current.timerRawPrimary,
			current.timerRawSecondary);
	}

	void updateActiveTurn(const CurrentTurnState &current) {
		if (!activeTurn.active) {
			return;
		}

		if (!current.playerName.empty()) {
			activeTurn.playerName = current.playerName;
		}
		if (current.timerRaw >= 0) {
			activeTurn.lastObservedRaw = current.timerRaw;
			if (activeTurn.lowestObservedRaw < 0 || current.timerRaw < activeTurn.lowestObservedRaw) {
				activeTurn.lowestObservedRaw = current.timerRaw;
			}
		}
	}

	void updateTracking(const CurrentTurnState &current) {
		syncParticipantsFromRoundState();

		if (current.valid && !current.beforeRoundStart) {
			registerParticipant(current.ownerMachineId, current.teamNumber, current.playerName);
			missingTurnFrames = 0;

			if (!activeTurn.active) {
				startActiveTurn(current);
				return;
			}

			if (isSameTurn(activeTurn, current)) {
				updateActiveTurn(current);
				return;
			}

			finalizeActiveTurn("turn changed");
			startActiveTurn(current);
			return;
		}

		if (!activeTurn.active) {
			return;
		}

		++missingTurnFrames;
		if (missingTurnFrames >= std::max(1, Config::getTurnEndGraceFrames())) {
			finalizeActiveTurn("turn missing");
		}
	}

	std::vector<std::string> buildTimesLines(const std::vector<WormTimeRecord> &records) {
		if (records.empty()) {
			return {EmptyPlayerTimes};
		}

		std::vector<std::string> lines;
		for (size_t i = 0; i < records.size(); i += 2) {
			std::string line = formatElapsedSeconds(records[i].elapsedRaw);
			if (i + 1 < records.size()) {
				line += " / ";
				line += formatElapsedSeconds(records[i + 1].elapsedRaw);
			}
			lines.push_back(line);
		}
		return lines;
	}

	std::vector<OverlayLine> buildOverlayLines(const CurrentTurnState &current) {
		std::vector<OverlayLine> lines;
		int playerSpacing = std::max(0, Config::getOverlayPlayerSpacingPixels());
		int activeParticipantKey = (current.valid && !current.beforeRoundStart)
			? getParticipantKey(current.ownerMachineId, current.teamNumber)
			: -1;
		for (int participantKey : participantOrder) {
			auto it = participantStates.find(participantKey);
			if (it == participantStates.end()) {
				continue;
			}

			const ParticipantState &participant = it->second;
			std::string playerLine = participant.playerName.empty() ? std::format("Player {}", participantKey + 1) : participant.playerName;
			playerLine += ":";
			bool activeTurnName = participantKey == activeParticipantKey;
			lines.push_back({playerLine, buildPlayerNameStyle(participant.teamNumber, activeTurnName), 0});

			std::vector<std::string> timesLines = buildTimesLines(participant.records);
			for (size_t i = 0; i < timesLines.size(); ++i) {
				int extraSpacing = (i + 1 == timesLines.size()) ? playerSpacing : 0;
				lines.push_back({timesLines[i], buildPlayerTimesStyle(participant.teamNumber), extraSpacing});
			}
		}
		if (!lines.empty()) {
			lines.back().extraSpacingPixels = 0;
		}
		return lines;
	}

	bool drawOverlayLines(DWORD gameScene, DWORD screenWidth, DWORD screenHeight, const std::vector<OverlayLine> &lines) {
		if (lines.empty()) {
			return false;
		}

		std::vector<int> widths(lines.size(), 0);
		std::vector<int> heights(lines.size(), 0);
		int totalHeight = 0;
		for (size_t i = 0; i < lines.size(); ++i) {
			int width = 0;
			int height = 0;
			if (!GameTextRenderer::measureText(i, lines[i].text, &width, &height, lines[i].style)) {
				return false;
			}
			widths[i] = width;
			heights[i] = height;
			totalHeight += height + lines[i].extraSpacingPixels;
		}

		int spacing = std::max(0, Config::getOverlayLineSpacingPixels());
		if (lines.size() > 1) {
			totalHeight += spacing * static_cast<int>(lines.size() - 1);
		}

		int drawX = Config::getOverlayLeftPixels();
		int drawY = std::max(0, static_cast<int>(screenHeight) - Config::getOverlayBottomPixels() - totalHeight);
		int currentY = drawY;
		bool drewAnyLine = false;
		for (size_t i = 0; i < lines.size(); ++i) {
			// DrawBitmap2D positions the textbox around the supplied X, so add half the measured width
			// to keep every line anchored to the same left edge.
			int fixedX = (drawX + widths[i] / 2 - static_cast<int>(screenWidth / 2) + DrawBitmap2DOriginXOffsetPixels) * FixedPointScale;
			int fixedY = (currentY - static_cast<int>(screenHeight / 2) + DrawBitmap2DOriginYOffsetPixels) * FixedPointScale;
			if (GameTextRenderer::drawText2D(gameScene, i, fixedX, fixedY, lines[i].text, lines[i].style)) {
				drewAnyLine = true;
			}
			currentY += heights[i] + spacing + lines[i].extraSpacingPixels;
		}
		return drewAnyLine;
	}

	void logOverlayState(bool drawn, const std::vector<OverlayLine> &lines, const CurrentTurnState &current) {
		static DWORD lastLogTick = 0;
		DWORD now = GetTickCount();
		if (now - lastLogTick <= 1000) {
			return;
		}
		lastLogTick = now;

		Diagnostics::log(
			"draw: active=%d player=%s team=%d worm=%d timer=%d lines=%u drawn=%d participants=%u",
			current.valid ? 1 : 0,
			current.playerName.c_str(),
			current.teamNumber,
			current.wormNumber,
			current.timerRaw,
			static_cast<unsigned int>(lines.size()),
			drawn ? 1 : 0,
			static_cast<unsigned int>(participantOrder.size()));
	}

	void __stdcall hookRenderDrawingQueue(DWORD dddisplay, DWORD a3) {
		DWORD gameScene;
		_asm mov gameScene, eax

		_asm mov eax, gameScene
		_asm push a3
		_asm push dddisplay
		_asm call origRenderDrawingQueue
	}

	int __stdcall hookTurnGameRenderScene() {
		int turnGame = 0;
		int retv = 0;
		_asm mov turnGame, edi

		DWORD gameGlobal = W2App::getAddrGameGlobal();
		DWORD gameScene = gameGlobal ? *reinterpret_cast<DWORD *>(gameGlobal + GameSceneOffset) : 0;
		TrackTimeOverlay::draw(gameScene);

		_asm mov edi, turnGame
		_asm call origTurnGameRenderScene
		_asm mov retv, eax

		return retv;
	}
}

void TrackTimeOverlay::install() {
	GameTextRenderer::install();
	DWORD addrRenderDrawingQueue = _ScanPattern("RenderDrawingQueue", "\x83\xEC\x1C\x53\x55\x8B\x6C\x24\x28\x56\x57\x8B\xF0\x8B\x86\x00\x00\x00\x00\x68\x00\x00\x00\x00\x6A\x04\x50\x8D\x8E\x00\x00\x00\x00\x51\xE8\x00\x00\x00\x00", "??????xxxxxxxxx????x????xxxxx????xx????");
	_Hook("RenderDrawingQueue", addrRenderDrawingQueue, (DWORD *)&hookRenderDrawingQueue, (DWORD *)&origRenderDrawingQueue);
	DWORD addrTurnGameRenderScene = _ScanPattern("TurnGameRenderScene", "\x83\xEC\x28\x8B\x47\x2C\x8B\x48\x24\x8B\x91\x00\x00\x00\x00\x85\xD2\x53\x8B\x98\x00\x00\x00\x00\x55\x8B\xA8\x00\x00\x00\x00\x56\x89\x5C\x24\x1C\x89\x6C\x24\x20\x7E\x4E", "??????xxxxx????xxxxx????xxx????xxxxxxxxxxx");
	_Hook("TurnGameRenderScene", addrTurnGameRenderScene, (DWORD *)&hookTurnGameRenderScene, (DWORD *)&origTurnGameRenderScene);
	Diagnostics::log("track time overlay installed: renderDrawingQueue=0x%X turnGameRenderScene=0x%X", addrRenderDrawingQueue, addrTurnGameRenderScene);
}

void TrackTimeOverlay::reset() {
	participantOrder.clear();
	participantStates.clear();
	activeTurn = ActiveTurnCapture{};
	missingTurnFrames = 0;
	teamStyles.fill({});
	Diagnostics::log("track time overlay reset");
}

void TrackTimeOverlay::draw(DWORD gameScene) {
	CurrentTurnState current = resolveCurrentTurnState();
	if (!Config::isRuntimeModuleEnabled() || !gameScene || !W2App::getAddrGameGlobal()) {
		logOverlayState(false, {}, current);
		return;
	}

	updateTracking(current);
	if (!Config::isOverlayEnabled()) {
		logOverlayState(false, {}, current);
		return;
	}

	std::vector<OverlayLine> lines = buildOverlayLines(current);
	if (lines.empty()) {
		logOverlayState(false, lines, current);
		return;
	}

	DWORD gameGlobal = W2App::getAddrGameGlobal();
	DWORD screenWidth = gameGlobal ? *reinterpret_cast<DWORD *>(gameGlobal + ScreenWidthOffset) : 640;
	DWORD screenHeight = gameGlobal ? *reinterpret_cast<DWORD *>(gameGlobal + ScreenHeightOffset) : 480;
	bool drawn = drawOverlayLines(gameScene, screenWidth, screenHeight, lines);

	logOverlayState(drawn, lines, current);
}

void TrackTimeOverlay::observeTeamStyle(unsigned long teamNumber, unsigned long highlight, int textColor, int backgroundColor, int frameColor) {
	if (teamNumber == 0 || teamNumber > MaxTeams) {
		return;
	}

	TeamStyle &style = teamStyles[teamNumber];
	style.liveObserved = true;
	style.liveTextColor = textColor;
	style.liveBackgroundColor = backgroundColor;
	style.liveFrameColor = frameColor;
	if (!style.stableObserved || highlight == 0) {
		style.stableObserved = true;
		style.stableTextColor = textColor;
		style.stableBackgroundColor = backgroundColor;
		style.stableFrameColor = frameColor;
	}
}
