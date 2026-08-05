#ifndef WKTURN_TIMER_CTASKWORM_H
#define WKTURN_TIMER_CTASKWORM_H

#include "CGameTask.h"

class CTaskWorm : public CGameTask {
public:
	static constexpr DWORD TeamNumberOffset = 0xFC;
	static constexpr DWORD WormNumberOffset = 0x100;
	static constexpr DWORD ActiveOffset = 0x104;
	static constexpr DWORD RopeModeOffset = 0x250;

	int getIntAtOffset(DWORD offset) const {
		return *reinterpret_cast<const int *>(reinterpret_cast<const unsigned char *>(this) + offset);
	}

	int getTeamNumber() const {
		return getIntAtOffset(TeamNumberOffset);
	}

	int getWormNumber() const {
		return getIntAtOffset(WormNumberOffset);
	}

	bool isActive() const {
		return getIntAtOffset(ActiveOffset) != 0;
	}

	bool isRopeMode() const {
		return getIntAtOffset(RopeModeOffset) != 0;
	}
};

#endif // WKTURN_TIMER_CTASKWORM_H
