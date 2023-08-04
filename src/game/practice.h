#ifndef PRACTICE_H
#define PRACTICE_H

#include "sm64.h"
#include "replay.h"

#define NULL_SECTION_TIMER_RESULT -1

#define PRACTICE_REPLAY_EXT "pre"

enum PracticeMenu {
	PRACTICE_LEVEL_SELECT = 0,
	PRACTICE_SETTINGS,
	PRACTICE_REPLAYS,
	PRACTICE_MENU_COUNT
};

enum PracticeType {
	PRACTICE_TYPE_STAR_GRAB,
	PRACTICE_TYPE_XCAM,
	PRACTICE_TYPE_STAGE,
	PRACTICE_TYPE_GAME,
	PRACTICE_TYPE_COUNT
};

extern u8 gDisableRendering;

extern struct WarpDest gPracticeDest;
extern u8 gPracticeWarping;
extern u8 gSaveStateWarpDelay;
extern u8 gNoStarSelectWarp;
extern struct WarpDest gLastWarpDest;
extern u8 gRenderPracticeMenu;
extern u8 gPracticeMenuPage;
extern u8 gFrameAdvance;
extern u8 gPlaybackPrimed;
extern u16 gLastButtons;

extern u8 gReplaySkipToNextArea;
extern u8 gReplaySkipToEnd;

extern u8 gIsRTA;

extern s32 gSectionTimer;
extern s32 gSectionTimerResult;

extern f32 gHeightLock;

extern u8 gWallkickFrame;
extern u16 gWallkickAngle;
extern s32 gWallkickTimer;

extern Replay* gPracticeRecordingReplay;
extern Replay* gPracticeFinishedReplay;

void practice_menu_audio_enabled(u8);

void practice_init(void);
void practice_deinit(void);
void practice_update(void);
void practice_reset(void);
void save_state_update(void);
void load_all_settings(void);

void practice_level_init(void);
void practice_star_grab(void);
void practice_star_xcam(void);
void practice_death_exit(void);
void practice_pause_exit(void);
void practice_level_change_trigger(void);
void practice_game_win(void);
void practice_file_select(void);

void practice_update_wdw_height(void);

void start_recording_replay(void);
void save_replay(void);

void timer_freeze(void);

void update_practice_bowser_info(s16 angleVel,s16 spin);

void render_practice_menu(void);
void render_practice_info(void);

#endif