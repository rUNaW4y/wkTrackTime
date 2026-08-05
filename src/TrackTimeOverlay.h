#ifndef WKTRACKTIME_OVERLAY_H
#define WKTRACKTIME_OVERLAY_H

typedef unsigned long DWORD;

class TrackTimeOverlay {
public:
	static void install();
	static void reset();
	static void draw(DWORD gameScene);
	static void observeTeamStyle(unsigned long teamNumber, unsigned long highlight, int textColor, int backgroundColor, int frameColor);
};

#endif // WKTRACKTIME_OVERLAY_H
