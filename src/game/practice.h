#ifndef PRACTICE_H
#define PRACTICE_H

#include "sm64.h"
#include "replay.h"

#define NULL_SECTION_TIMER_RESULT -1

#define PRACTICE_REPLAY_EXT "pre"
#define PRACTICE_SAVE_FILE_EXT "psv"
#define MARIOTIMER_REPLAY_EXT "mtr"

#ifndef VERSION_JP
#define INTRO_SKIP_TIME_START 1648
#define INTRO_SKIP_RNG_START 0x4ac4
#define INTRO_SKIP_RNG_CALLS_START 40058
#else
#define INTRO_SKIP_TIME_START 1581
#define INTRO_SKIP_RNG_START 0x015d
#define INTRO_SKIP_RNG_CALLS_START 38246
#endif

enum PracticeMenu {
	PRACTICE_LEVEL_SELECT = 0,
	PRACTICE_SETTINGS,
	PRACTICE_REPLAYS,
	PRACTICE_SAVE_EDITOR,
	PRACTICE_MENU_COUNT
};

enum PracticeType {
	PRACTICE_TYPE_STAR_GRAB = 0,
	PRACTICE_TYPE_XCAM,
	PRACTICE_TYPE_STAGE,
	PRACTICE_TYPE_GAME,
	PRACTICE_TYPE_COUNT
};

enum PracticeInitOption {
	PRACTICE_OP_DEFAULT = 0,
	PRACTICE_OP_ALWAYS,
	PRACTICE_OP_NEVER
};

typedef enum {
	PRACTICE_OPTION_BOOL = 0,
	PRACTICE_OPTION_ENUM,
	PRACTICE_OPTION_INT,
	PRACTICE_OPTION_UINT,
	PRACTICE_OPTION_TOGGLE_INT,
	PRACTICE_OPTION_TOGGLE_UINT,
	PRACTICE_OPTION_TOGGLE_FLOAT,
	PRACTICE_OPTION_TOGGLE_ENUM,
	
	PRACTICE_OPTION_TOGGLE_BIG_UINT,
	PRACTICE_OPTION_TOGGLE_SAVE,
	
	PRACTICE_INFO,
	PRACTICE_BUTTON,
	PRACTICE_INFO_BUTTON,
	PRACTICE_SPACER
} PracticeSettingType;

typedef struct {
	u8 enabled;
	//u8 type;
	union {
		u32 varU32;
		s32 varS32;
		f32 varF32;
		void* varPtr;
	};
} PracticeResetVar;

typedef struct {
	const char* name;
	void** values;
	PracticeSettingType type;
	u32 index;
	void* var;
} PracticeSetting;

typedef struct {
	s32 min;
	s32 max;
} NumberRange;

extern u8 gDisableRendering;

extern u8 gRTAMode;

extern struct WarpDest gPracticeDest;
extern u8 gPracticeWarping;
extern u8 gSaveStateWarpDelay;
extern u8 gNoStarSelectWarp;
extern struct WarpDest gLastWarpDest;
extern s32 gLastLevelNum;
extern u8 gRenderPracticeMenu;
extern u8 gPracticeMenuPage;
extern u8 gFrameAdvance;
extern u8 gFrameAdvancedThisFrame;
extern u8 gPlaybackPrimed;
extern u16 gLastButtons;

extern u8 gWillPracticeReset;

extern u8 gReplaySkipToNextArea;
extern u8 gReplaySkipToEnd;

extern u8 gIsRTA;

extern u32 gAPressCounter;

extern s32 gSectionTimer;
extern s32 gSectionTimerResult;

extern f32 gHeightLock;

extern u8 gWallkickFrame;
extern u16 gWallkickAngle;
extern s32 gWallkickTimer;

extern Replay* gPracticeRecordingReplay;
extern Replay* gPracticeFinishedReplay;

// maps RNG values to indices
// 128kb
extern u16 gRNGTable[65536];

void practice_warp(void);

void practice_set_message(const char* msg);
void practice_menu_audio_enabled(u8);

void update_save_file_vars(void);

void practice_init(void);
void practice_deinit(void);
void practice_update(void);
void practice_reset(void);
void save_state_update(void);
void load_all_settings(void);

void practice_fix_mario_rotation(void);

void practice_level_init(void);
void practice_star_grab(void);
void practice_star_xcam(void);
void practice_death_exit(void);
void practice_pause_exit(void);
void practice_door_exit(void);
void practice_level_change_trigger(void);
void practice_painting_trigger(void);
void practice_game_win(void);
void practice_file_select(void);
void practice_intro_skip_start(void);
void practice_soft_reset(void);
void practice_game_start(void);

void practice_update_wdw_height(void);

void start_recording_replay(void);
void save_replay(void);
void replay_warp(void);

void timer_freeze(void);

void update_practice_bowser_info(s16 angleVel,s16 spin);

void render_practice_menu(void);
void render_practice_info(void);

#endif