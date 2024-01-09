#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <PR/ultratypes.h>
#include <stdbool.h>

#define CONFIGFILE_DEFAULT "sm64config.txt"

#define MAX_BINDS  3
#define MAX_VOLUME 127

typedef struct {
    unsigned int x, y, w, h;
    bool vsync;
    bool reset;
    bool fullscreen;
    bool exiting_fullscreen;
    bool settings_changed;
} ConfigWindow;

extern ConfigWindow configWindow;
extern unsigned int configFiltering;
extern unsigned int configMasterVolume;
extern unsigned int configMusicVolume;
extern unsigned int configSfxVolume;
extern unsigned int configEnvVolume;
extern unsigned int configKeyA[];
extern unsigned int configKeyB[];
extern unsigned int configKeyStart[];
extern unsigned int configKeyL[];
extern unsigned int configKeyR[];
extern unsigned int configKeyZ[];
extern unsigned int configKeyCUp[];
extern unsigned int configKeyCDown[];
extern unsigned int configKeyCLeft[];
extern unsigned int configKeyCRight[];
extern unsigned int configKeyStickUp[];
extern unsigned int configKeyStickDown[];
extern unsigned int configKeyStickLeft[];
extern unsigned int configKeyStickRight[];

extern unsigned int configKeyDUp[];
extern unsigned int configKeyDDown[];
extern unsigned int configKeyDLeft[];
extern unsigned int configKeyDRight[];

extern unsigned int configStickDeadzone;
#ifdef EXTERNAL_DATA
extern bool         configPrecacheRes;
#endif

extern bool         configHUD;
extern bool         configSkipIntro;
extern bool         configFileSelectStart;
extern bool         configAnglerOverride;
extern bool         configNonstop;
#ifdef DISCORDRPC
extern bool         configDiscordRPC;
#endif

extern bool configShowTimer;
extern bool configShowControls;
extern bool configShowPos;
extern bool configShowHOLP;
extern bool configShowAngle;
extern bool configShowVel;
extern bool configShowSlidingVel;
extern bool configShowTwirlYaw;
extern bool configShowMaxHeight;
extern bool configShowWallkickFrame;
extern bool configShowWallkickAngle;
extern bool configShowRNGInfo;
extern bool configShowBowserInfo;
extern bool configShowEfficiency;
extern bool configShowSwimStrength;
extern bool configShowSwimTrainer;
extern bool configShowAPressCount;
extern bool configShowGlobalTimer;
extern bool configUseGhost;
extern bool configGhostDistanceFade;
extern bool configResetMusic;
extern bool configDisableMusic;
extern bool configIntroSkipTiming;
extern bool configNoInvisibleWalls;
extern bool configDisableA;
extern bool configYellowStars;

extern u32 configAngleDisplayType;
extern u32 configPracticeType;
extern u32 configStageText;
extern u32 configGhostOpacity;

void configfile_load(const char *filename);
void configfile_save(const char *filename);
const char *configfile_name(void);

#endif
