#ifndef WKTURN_TIMER_CTASKTURNGAME_H
#define WKTURN_TIMER_CTASKTURNGAME_H

#include "CTask.h"

class CTaskTurnGame : public CTask {
public:
	static constexpr DWORD TurnTimerPrimaryOffset = 0x188;
	static constexpr DWORD TurnTimerSecondaryOffset = 0x18C;
	static constexpr DWORD TurnPausedOffset = 0x150;
	static constexpr DWORD BeforeRoundStartOffset = 0x140;

	int getIntAtOffset(DWORD offset) const {
		return *reinterpret_cast<const int *>(reinterpret_cast<const unsigned char *>(this) + offset);
	}

	int getTurnTimerPrimaryRaw() const {
		return getIntAtOffset(TurnTimerPrimaryOffset);
	}

	int getTurnTimerSecondaryRaw() const {
		return getIntAtOffset(TurnTimerSecondaryOffset);
	}

	bool isTurnPaused() const {
		return getIntAtOffset(TurnPausedOffset) != 0;
	}

	bool isBeforeRoundStart() const {
		return getIntAtOffset(BeforeRoundStartOffset) != 0;
	}
};

#endif // WKTURN_TIMER_CTASKTURNGAME_H
