#include <PR/ultratypes.h>

#include "sm64.h"
#include "practice.h"
#include "save_state.h"
#include "level_update.h"
#include "game_init.h"

#include <stdio.h>

struct WarpDest gPracticeDest;
struct WarpDest gLastWarpDest;
u8 gPracticeWarping = FALSE;
u8 gNoStarSelectWarp = FALSE;
u8 gSaveStateWarpDelay = 0;

u8 gPlaybackPrimed = 0;

Replay gPracticeReplay;

void practice_init(void){
	gPracticeDest.type = WARP_TYPE_NOT_WARPING;
	init_state(&gCurrSaveState);
}

s32 practice_update(void){
	if (gSaveStateWarpDelay!=0&&
		gCurrLevelNum==gCurrSaveState.initState.loc.levelNum&&
		gCurrAreaIndex==gCurrSaveState.initState.loc.areaIdx){
		if (--gSaveStateWarpDelay==0){
			load_state(&gCurrSaveState);
		}
	} else if ((gPlayer1Controller->buttonPressed & D_JPAD)){
		save_state(&gCurrSaveState);
		printf("gCurrSaveState size: %lld\n",get_state_size(&gCurrSaveState));
	} else if ((gPlayer1Controller->buttonPressed & U_JPAD)&&gHasStateSaved){
		if (gCurrLevelNum==gCurrSaveState.initState.loc.levelNum&&
			gCurrAreaIndex==gCurrSaveState.initState.loc.areaIdx){
			load_state(&gCurrSaveState);
		} else {
			gPracticeDest = gCurrSaveState.initState.loc;
			gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
			gPracticeDest.nodeId = 0xA;
			gPracticeDest.arg = 0;
			gSaveStateWarpDelay = 2;
			gNoStarSelectWarp = TRUE;
		}
	} else if ((gPlayer1Controller->buttonPressed & R_JPAD)&&gCurrRecordingReplay){
		end_replay_record();
	}
	
	if (gPlayer1Controller->buttonPressed & L_TRIG){
		gPracticeDest = gPracticeReplay.state.levelState->loc;
		gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
		gNoStarSelectWarp = TRUE;
		gPlaybackPrimed = 1;
		
		//init_playing_replay(&gPracticeReplay);
	}
	if (gPlaybackPrimed&&gCurrLevelNum==gPracticeReplay.state.levelState->loc.levelNum&&
		gCurrAreaIndex==gPracticeReplay.state.levelState->loc.areaIdx){
		init_playing_replay(&gPracticeReplay);
		gPlaybackPrimed = 0;
	}
	
	if ((gPlayer1Controller->buttonPressed & L_JPAD)){
		gPracticeDest.levelNum = LEVEL_SL;
		gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
		gPracticeDest.areaIdx = 1;
		gPracticeDest.nodeId = 0xA;
		gPracticeDest.arg = 0;
		gNoStarSelectWarp = gPracticeDest.levelNum==gCurrLevelNum;
	}
	return 0;
}