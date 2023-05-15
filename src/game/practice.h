#ifndef PRACTICE_H
#define PRACTICE_H

#include "replay.h"

#define NULL_SECTION_TIMER_RESULT -1

enum PracticeMenu {
	PRACTICE_LEVEL_SELECT = 0,
	PRACTICE_SETTINGS,
	PRACTICE_MENU_COUNT
};

enum SectionTimerType {
	SECTION_TIMER_TYPE_XCAM = 0,
	SECTION_TIMER_TYPE_LEVEL,
	SECTION_TIMER_TYPE_GAME,
	SECTION_TIMER_TYPE_COUNT
};

extern struct WarpDest gPracticeDest;
extern u8 gPracticeWarping;
extern u8 gSaveStateWarpDelay;
extern u8 gNoStarSelectWarp;
extern struct WarpDest gLastWarpDest;
extern u8 gRenderPracticeMenu;
extern u8 gPracticeMenuPage;
extern u8 gFrameAdvance;
extern u32 gLastButtons;

extern s32 gSectionTimer;
extern s32 gSectionTimerResult;

extern f32 gHeightLock;

extern u8 gWallkickFrame;
extern u16 gWallkickAngle;
extern s32 gWallkickTimer;

extern Replay gPracticeReplay;

void practice_init(void);
void practice_update(void);

void section_timer_level_init(void);
void section_timer_choose_level(void);
void section_timer_game_win(void);
void section_timer_game_reset(void);
void section_timer_star_xcam(void);

void timer_freeze(void);

void render_practice_menu(void);
void render_practice_info(void);

#endif