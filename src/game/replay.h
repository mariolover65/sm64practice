#ifndef REPLAY_H
#define REPLAY_H

#include "sm64.h"

#include <stdio.h>

#define REPLAY_BUTTON_MASK (A_BUTTON|B_BUTTON|Z_TRIG|R_TRIG| \
							L_CBUTTONS|R_CBUTTONS|U_CBUTTONS|D_CBUTTONS| \
							START_BUTTON)

struct RLEChunk;
typedef struct RLEChunk RLEChunk;
struct GhostFrame;
typedef struct GhostFrame GhostFrame;
struct GhostAreaChange;
typedef struct GhostAreaChange GhostAreaChange;

typedef struct {
	u16 buttons;
	s8 stickX;
	s8 stickY;
} InputFrame;

struct RLEChunk {
	RLEChunk* next;
	InputFrame controls;
	u8 length;
};

enum ReplayInitStateType {
	LEVEL_INIT = 0,
	GAME_INIT
};

typedef struct {
	u8 type;
	union {
		void* levelState;
		void* gameState;
		void* saveState;
	};
} ReplayInitState;

enum ReplayFlags {
	REPLAY_FLAG_SAVE_STATED = 1,
	REPLAY_FLAG_FRAME_ADVANCED = 2,
	REPLAY_FLAG_A_PRESS = 4,
	REPLAY_FLAG_BLJ = 8,
	REPLAY_FLAG_RESET = 16,
	REPLAY_FLAG_OPENED_PRACTICE_MENU = 32,
	REPLAY_FLAG_CHEAT_HUD = 64
};

typedef struct {
	ReplayInitState state;
	u64 date;
	u32 flags;
	
	u8 practiceType;
	u16 starCount;
	
	u32 length;
	RLEChunk* data;
} Replay;

typedef struct {
	u64 date;
	u32 length;
	u32 flags;
	u16 starCount;
	u8 isLevelInit;
	u8 practiceType;
	u8 introSkip;
	u8 introSkipTiming;
} ReplayFileHeader;

struct GhostFrame {
	GhostFrame* next;
	Vec3f pos;
	Vec3s angle;
	s16 animID;
	s16 animFrame;
	u8 model;
	u8 cap;
};

struct GhostAreaChange {
	GhostAreaChange* next;
	u32 length;
	u8 levelNum;
	u8 areaIdx;
};

typedef struct {
	GhostFrame* data;
	GhostAreaChange* changeData;
} GhostData;

extern Replay* gCurrRecordingReplay;
extern Replay* gCurrPlayingReplay;

#define REPLAY_HISTORY_LENGTH 10
extern Replay* gReplayHistory[REPLAY_HISTORY_LENGTH];
extern s32 gReplayBalance;

extern u8 gIsRecordingGhost;
extern GhostData* gCurrGhostData;
extern GhostFrame* gCurrGhostFrame;
extern GhostAreaChange* gCurrGhostArea;
extern u32 gGhostAreaCounter;
extern u32 gGhostDataIndex;
extern struct Controller gStoredReplayController;

extern f32 gGhostDistanceScaling;

InputFrame get_current_inputs(void);

Replay* alloc_replay(void);
void free_replay(Replay* replay);
void archive_replay(Replay* replay);
void archive_load_replay(Replay** into,u32 index);
void clear_replay_history(void);

Replay* copy_replay(const Replay* replayToCopy,const RLEChunk* until,u8 subframe);

void init_replay_record(Replay*,u8 practiceType,u16 starCount);
void init_replay_record_at_end(Replay*);
void add_frame(void);
void end_replay_record(void);
u32 get_replay_length(const Replay*);
u32 get_replay_header_length(const ReplayFileHeader*);

void init_playing_replay(Replay*);
void update_replay(void);
void replay_overwrite_inputs(struct Controller*);
void get_current_replay_pos(RLEChunk** replayFrame,u8* replaySubframe);

void serialize_replay(FILE* file,const Replay* replay);
u8 deserialize_replay(FILE* file,Replay* replay);
u8 deserialize_replay_header(FILE* file,ReplayFileHeader*);
u8 deserialize_mtr_replay(FILE* file,Replay* replay);
u8 deserialize_mtr_replay_header(FILE* file,ReplayFileHeader*);

void ghost_init(void);
void ghost_load(struct GraphNode* root);
void ghost_unload(void);
u8 ghost_is_parent(struct GraphNode*);
void ghost_data_create(void);
void ghost_data_free(GhostData*);
void ghost_add_frame(void);
void ghost_advance_frame(void);
void ghost_finish_record(void);
void ghost_start_playback(void);

#endif