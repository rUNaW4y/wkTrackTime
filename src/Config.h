#ifndef WKTRACKTIME_CONFIG_H
#define WKTRACKTIME_CONFIG_H

#include <filesystem>
#include <string>

class Config {
public:
	static inline const std::string iniFile = "wkTrackTime.ini";
	static inline const std::string cacheFile = "wkTrackTime.cache";
	static inline const std::string moduleName = "wkTrackTime";

private:
	static inline bool moduleEnabled = true;
	static inline bool runtimeModuleEnabled = true;
	static inline bool ignoreVersionCheck = false;
	static inline bool useOffsetCache = true;
	static inline bool overlayEnabled = true;
	static inline int primaryTimerOffset = 0x188;
	static inline int secondaryTimerOffset = 0x18C;
	static inline int timerTicksPerSecond = 1000;
	static inline int overlayLeftPixels = -150;
	static inline int overlayBottomPixels = 72;
	static inline int overlayLineSpacingPixels = 2;
	static inline int overlayPlayerSpacingPixels = 4;
	static inline int turnEndGraceFrames = 3;
	static inline int textColor = 6;
	static inline int textBackground = 21;
	static inline int textFrame = 54;
	static inline int textFont = 1;
	static inline int textOpacity = 0x10000;
	static inline std::filesystem::path waDir;

public:
	static void readConfig();
	static bool isModuleEnabled();
	static bool isRuntimeModuleEnabled();
	static void setRuntimeModuleEnabled(bool enabled);
	static int waVersionCheck();
	static const std::filesystem::path &getWaDir();

	static std::string getVersionStr();
	static std::string getBuildStr();
	static std::string getModuleStr();
	static std::string getFullStr();

	static bool isUseOffsetCache();
	static std::string getWaVersionAsString();
	static const std::string &getCacheFile();

	static bool isOverlayEnabled();
	static int getPrimaryTimerOffset();
	static int getSecondaryTimerOffset();
	static int getTimerTicksPerSecond();
	static int getOverlayLeftPixels();
	static int getOverlayBottomPixels();
	static int getOverlayLineSpacingPixels();
	static int getOverlayPlayerSpacingPixels();
	static int getTurnEndGraceFrames();
	static int getTextColor();
	static int getTextBackground();
	static int getTextFrame();
	static int getTextFont();
	static int getTextOpacity();
};

#endif // WKTRACKTIME_CONFIG_H
