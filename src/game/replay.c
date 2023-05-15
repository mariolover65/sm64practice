#include <PR/ultratypes.h>

#include "replay.h"
#include "game_init.h"
#include "practice.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

Replay* gCurrRecordingReplay = NULL;
Replay* gCurrPlayingReplay = NULL;

static RLEChunk* sCurrRecordFrame = NULL;
static RLEChunk* sCurrReplayFrame = NULL;
u8 sCurrReplaySubframe = 0;

static OSContPad* sOldControllerData;
static OSContPad sReplayController;

void free_replay(Replay* replay){
	if (replay->state.type==LEVEL_INIT){
		free(replay->state.levelState);
	} else if (replay->state.type==SAVE_STATE){
		free(replay->state.saveState);
	}
	RLEChunk* curr = replay->data;
	RLEChunk* next;
	while (curr!=NULL){
		next = curr->next;
		free(curr);
		curr = next;
	}
	replay->data = NULL;
}

void init_replay_record(Replay* replay,u8 isLevelInit){
	if (gCurrRecordingReplay!=NULL){
		free_replay(replay);
	}
	
	gCurrRecordingReplay = replay;
	replay->length = 0;
	if (isLevelInit){
		replay->state.type = LEVEL_INIT;
		replay->state.levelState = malloc(sizeof(LevelInitState));
		save_level_init_state(replay->state.levelState,&gLastWarpDest);
	} else {
		replay->state.type = SAVE_STATE;
		replay->state.saveState = malloc(sizeof(SaveState));
		save_state(replay->state.saveState);
	}
	replay->data = malloc(sizeof(RLEChunk));
	sCurrRecordFrame = replay->data;
	replay->data->controls = 0;
	replay->data->length = 1;
}

void add_frame(void){
	if (gCurrRecordingReplay!=NULL){
		OSContPad* controller = gPlayer1Controller->controllerData;
		u32 controls;
		memcpy(&controls,controller,sizeof(u32));
		if (sCurrRecordFrame->controls!=controls||sCurrRecordFrame->length==255){
			sCurrRecordFrame->next = malloc(sizeof(RLEChunk));
			sCurrRecordFrame = sCurrRecordFrame->next;
			sCurrRecordFrame->length = 0;
			sCurrRecordFrame->controls = controls;
			sCurrRecordFrame->next = NULL;
		} else {
			++sCurrRecordFrame->length;
		}
	}
}

void end_replay_record(void){
	printf("Replay length: %d\n",get_replay_length(gCurrRecordingReplay));
	gCurrRecordingReplay = NULL;
}

u32 get_replay_length(Replay* replay){
	u32 s = 0;
	RLEChunk* chunk = replay->data;
	while (chunk!=NULL){
		s += chunk->length;
		chunk = chunk->next;
	}
	return s;
}

void init_playing_replay(Replay* replay){
	sOldControllerData = gPlayer1Controller->controllerData;
	gPlayer1Controller->controllerData = &sReplayController;
	
	gCurrPlayingReplay = replay;
	sCurrReplayFrame = replay->data;
	sCurrReplaySubframe = 0;
}

void finish_replay(void){
	printf("Playback finished\n");
	gPlayer1Controller->controllerData = sOldControllerData;
	gCurrPlayingReplay = NULL;
}

void update_replay(void){
	if (gCurrPlayingReplay!=NULL){
		if (sCurrReplayFrame==NULL){
			finish_replay();
			return;
		}
		
		u32 controls = sCurrReplayFrame->controls;
		memcpy(&sReplayController,&controls,sizeof(u32));
		
		if (++sCurrReplaySubframe==(u8)(sCurrReplayFrame->length+1)){
			sCurrReplayFrame = sCurrReplayFrame->next;
			sCurrReplaySubframe = 0;
		}
	}
}
