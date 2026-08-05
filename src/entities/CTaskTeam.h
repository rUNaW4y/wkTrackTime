#ifndef WKDEANOREMINDER_CTASKTEAM_H
#define WKDEANOREMINDER_CTASKTEAM_H

#include "CTask.h"
#include "../W2App.h"

struct TeamBarStruct {
	DWORD vtable_dword0;
	DWORD team_number_dword4;
	DWORD team_alliance_dword8;
	DWORD gameglobal_dwordC;
	DWORD textbox_dword10;
	DWORD energy_bar_bitmap_dword14;
	DWORD width_dword18;
	DWORD highlight_dword1C;
	DWORD dword20;
};

class CTaskTeam : public CTask {
public:
	static constexpr DWORD TeamNumberOffset = 0x38;
	static constexpr DWORD OwnerMachineOffset = 0x40;
	static constexpr DWORD MyMachineOffset = 0xD9DC;

	int getIntAtOffset(DWORD offset) const {
		return *reinterpret_cast<const int *>(reinterpret_cast<const unsigned char *>(this) + offset);
	}

	unsigned char getByteAtOffset(DWORD offset) const {
		return *reinterpret_cast<const unsigned char *>(reinterpret_cast<const unsigned char *>(this) + offset);
	}

	int getTeamNumber() const {
		return getIntAtOffset(TeamNumberOffset);
	}

	int getOwnerMachineId() const {
		return static_cast<int>(getByteAtOffset(OwnerMachineOffset));
	}

	bool isOwnedByMe() const {
		DWORD ddMain = W2App::getAddrDDMain();
		if (!ddMain) {
			return false;
		}

		unsigned char myMachineId = *reinterpret_cast<const unsigned char *>(ddMain + MyMachineOffset);
		return getByteAtOffset(OwnerMachineOffset) == myMachineId;
	}

	static void install();
};

#endif // WKDEANOREMINDER_CTASKTEAM_H
