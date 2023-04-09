#ifndef REPLAY_H
#define REPLAY_H

#include "save_state.h"

struct RLEChunk;
typedef struct RLEChunk RLEChunk;

struct RLEChunk {
	RLEChunk* next;
	u32 controls;
	u8 length;
};

enum ReplayInitStateType {
	LEVEL_INIT,
	SAVE_STATE
};

typedef struct {
	u8 type;
	union {
		LevelInitState* levelState;
		SaveState* saveState;
	};
} ReplayInitState;

typedef struct {
	ReplayInitState state;
	u32 length;
	RLEChunk* data;
} Replay;

extern Replay* gCurrRecordingReplay;
extern Replay* gCurrPlayingReplay;

void init_replay_record(Replay*,u8 isLevelInit);
void add_frame(void);
void end_replay_record(void);
u32 get_replay_length(Replay*);

void init_playing_replay(Replay*);
void update_replay(void);

#endif