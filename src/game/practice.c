#include <PR/ultratypes.h>

#include "sm64.h"
#include "practice.h"
#include "stats.h"
#include "save_state.h"
#include "level_update.h"
#include "area.h"
#include "hud.h"
#include "game_init.h"
#include "ingame_menu.h"
#include "gfx_dimensions.h"
#include "pc/configfile.h"
#include "segment2.h"
#include "print.h"
#include "screen_transition.h"
#include "engine/math_util.h"
#include "sound_init.h"
#include "audio/external.h"
#include "browser.h"
#include "save_editor.h"
#include "mario_actions_submerged.h"
#include "engine/behavior_script.h"

#include "pc/fs/fs.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

const char* gPracticeMessage = NULL;
s32 gPracticeMessageTimer = 0;

//extern u8 texture_a_button[];
extern s16 gMenuMode;
extern s32 gCourseDoneMenuTimer;
extern u16 gRandomSeed16;
extern u16 gRandomCalls;
extern s16 sSwimStrength;

s32 gPracticeSubStatus = PRACTICE_OP_DEFAULT;

u16 gRNGTable[65536];

static PracticeResetVar sResetActNum;
static PracticeResetVar sResetRNG;
static PracticeResetVar sResetGlobalTimer;
static PracticeResetVar sResetLives;
static PracticeResetVar sResetSwimStrength;
static PracticeResetVar sResetCamMode;
static PracticeResetVar sResetSave;

u8 gDebug = FALSE;

u8 gDisableRendering = FALSE;

u8 gIsRTA = TRUE;

static u8 sStageRTAPrimed = FALSE;
static s32 sStageRTAStarsCompleted = 0;
static s32 sStageRTAStarsTarget = 7;

struct WarpDest gPracticeDest;
struct WarpDest gLastWarpDest;
s32 gLastLevelNum = 0;
extern s32 gCurrLevelArea;

u8 gPracticeWarping = FALSE;
u8 gNoStarSelectWarp = FALSE;
u8 gSaveStateWarpDelay = 0;
u8 gRenderPracticeMenu = FALSE;
u8 gPracticeMenuPage = 0;

u8 gRTAMode = FALSE;

u8 gIntroSkipSetValsPrimed = 0;
u8 gWillPracticeReset = FALSE;

static u8 sLoadSaveStateNow = FALSE;

u8 gPlaybackPrimed = FALSE;

u32 gAPressCounter = 0;

s32 gSectionTimer = 1;
s32 gSectionTimerResult = NULL_SECTION_TIMER_RESULT;

static s32 sSectionTimerLock = 0;
static s32 sSectionTimerLockTime = 0;

u8 gFrameAdvance = FALSE;
u8 gFrameAdvancedThisFrame = FALSE;
static u8 sFrameAdvanceStorage = FALSE;

u16 gLastButtons = 0;

f32 gHeightLock = 0.0f;

u8 gReplaySkipToNextArea = FALSE;
u8 gReplaySkipToEnd = FALSE;

u8 gWallkickFrame = 0;
u16 gWallkickAngle = 0;
s32 gWallkickTimer = 0;

static s16 sBowserAngleVel = 0;
static s16 sSpinArray[30];
static u16 sSpinIndex = 0;
static u8 sHoldingBowser = 0;

Replay* gPracticeRecordingReplay = NULL;
Replay* gPracticeFinishedReplay = NULL;

// in level_script
extern u8 gOpenPracticeMenuNextFrame;
extern u8 gClosePracticeMenuNextFrame;

extern u8 sSoundFlags;
static u8 sSavedSoundFlags = 0;

void practice_menu_audio_enabled(u8 b){
	if (b){
		if (!(sSavedSoundFlags & SOUND_FLAG_SILENT))
			set_sound_disabled(FALSE);
		
		// fade out music over 2 frames if disabled
		if (configDisableMusic)
			fadeout_level_music(2);
	} else {
		sSavedSoundFlags = sSoundFlags;
		set_sound_disabled(TRUE);
	}
}

void practice_set_message(const char* msg){
	gPracticeMessage = msg;
	gPracticeMessageTimer = 40;
}

static u8 sWallkickColors[5][3] = {
	{255,255,0},
	{0,64,255},
	{255,128,0},
	{0,255,0},
	{255,0,255},
};

static void section_timer_start(void){
	gSectionTimerResult = NULL_SECTION_TIMER_RESULT;
	gSectionTimer = 0;
}

static void section_timer_finish(void){
	if (gSectionTimerResult==NULL_SECTION_TIMER_RESULT){
		gSectionTimerResult = gSectionTimer;
		sSectionTimerLockTime = 0;
	}
}

void timer_freeze(void){
	sSectionTimerLock = gSectionTimer;
	sSectionTimerLockTime = 20;
}

static void clear_transit_state(void){
	gMenuMode = -1;
	gCourseDoneMenuTimer = 0;
	sTransitionTimer = 0;
	gDialogID = -1;
	gDialogBoxState = 0;
	// store the current frame advance state
	// until the level inits at which point we restore it
	if (!sFrameAdvanceStorage)
		sFrameAdvanceStorage = gFrameAdvance;
	gFrameAdvance = FALSE;
	gFrameAdvancedThisFrame = FALSE;
	gAPressCounter = 0;
	gNoStarSelectWarp = TRUE;
}

bool can_load_save_state(const SaveState* state){
	if (!state) return FALSE;
	if (state->levelState.loc.levelNum==1) return TRUE;
	
	return gCurrLevelNum==state->levelState.loc.levelNum&&
		gCurrAreaIndex==state->levelState.loc.areaIdx&&
		gCurrActNum==state->levelState.currActNum&&
		gPracticeDest.type==WARP_TYPE_NOT_WARPING&&(sWarpDest.type==WARP_TYPE_NOT_WARPING||(sTransitionTimer>=1||sTransitionTimer==-1));
}

void practice_load_state(const SaveState* state){
	gCurrPlayingReplay = NULL;
	
	if (state->levelState.loc.levelNum==1){
		gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
		gPracticeDest.levelNum = 1;
		gPracticeDest.areaIdx = 1;
		gPracticeDest.arg = 0;
		clear_transit_state();
		return;
	}
	
	load_state(state);
	gIsRTA = FALSE;
	++gPracticeStats.stateLoadCount;
}

void save_replay(void){
	if (!gCurrRecordingReplay)
		return;
	
	end_replay_record();
	if (gPracticeFinishedReplay!=NULL)
		archive_replay(gPracticeFinishedReplay);
	
	gPracticeFinishedReplay = gPracticeRecordingReplay;
	gPracticeRecordingReplay = NULL;
}

void button_stop_recording(PracticeSetting*){
	if (gCurrRecordingReplay){
		gCurrRecordingReplay->flags |= REPLAY_FLAG_RESET;
		save_replay();
	}
}

void start_recording_replay(void){
	if (gPracticeRecordingReplay){
		free_replay(gPracticeRecordingReplay);
	}
	
	gPracticeRecordingReplay = alloc_replay();
	init_replay_record(gPracticeRecordingReplay,configPracticeType,sStageRTAStarsTarget);
	add_frame();
}

void start_replay_playback(u8 instantUpdate){
	if (gPracticeRecordingReplay){
		archive_replay(gPracticeRecordingReplay);
		gPracticeRecordingReplay = NULL;
		gCurrRecordingReplay = NULL;
	}
	
	if (gPracticeFinishedReplay->state.type==LEVEL_INIT){
		load_post_level_init_state(gPracticeFinishedReplay->state.levelState);
	}
	
	init_playing_replay(gPracticeFinishedReplay);
	gPlaybackPrimed = FALSE;
	gDisableRendering = FALSE;
	gIsRTA = FALSE;
	if (instantUpdate){
		update_replay();
		replay_overwrite_inputs(gPlayer1Controller);
	}
	
	if (gReplaySkipToEnd){
		gDisableRendering = TRUE;
	}
	
	configPracticeType = gPracticeFinishedReplay->practiceType;
	if (configPracticeType==PRACTICE_TYPE_STAGE){
		sStageRTAStarsTarget = gPracticeFinishedReplay->starCount;
	}
	
	if (configPracticeType!=PRACTICE_TYPE_GAME){
		section_timer_start();
		gSectionTimer = 1;
	}
}

static void set_practice_type(u8 practiceType){
	configPracticeType = practiceType;
	if (configPracticeType==PRACTICE_TYPE_XCAM||
		configPracticeType==PRACTICE_TYPE_STAR_GRAB||
		configPracticeType==PRACTICE_TYPE_STAGE){
		configStageText = PRACTICE_OP_NEVER;
		configYellowStars = TRUE;
		if (configPracticeType==PRACTICE_TYPE_STAGE)
			gPracticeSubStatus = PRACTICE_OP_DEFAULT;
	} else if (configPracticeType==PRACTICE_TYPE_GAME){
		configStageText = PRACTICE_OP_DEFAULT;
		gPracticeSubStatus = PRACTICE_OP_DEFAULT;
		configYellowStars = FALSE;
	}
}

extern u8 sTransitionColorFadeCount[4];
extern u16 sTransitionTextureFadeCount[2];
extern s16 gWarpTransDelay;

void practice_warp(void){
	sWarpDest = gPracticeDest;
	gPracticeDest.type = WARP_TYPE_NOT_WARPING;
	reset_dialog_render_state();
	sTransitionTimer = 0;
	sTransitionUpdate = NULL;
	gMarioState->numCoins = 0;
	gHudDisplay.coins = 0;
	gHudDisplay.wedges = 8;
	gMarioState->health = 0x880;
	gMenuMode = -1;
	gHudFlash = FALSE;
	sPowerMeterVisibleTimer = 0;
	sPowerMeterStoredHealth = 8;
	sPowerMeterHUD.animation = POWER_METER_HIDDEN;
	gWarpTransDelay = 0;
	gWarpTransition.isActive = TRUE;
	gWarpTransition.type = WARP_TRANSITION_FADE_FROM_COLOR;
	gWarpTransition.time = 16;
	
	// set transition colors properly
	s32 courseNum = gLevelToCourseNumTable[gPracticeDest.levelNum - 1];
	if ((courseNum == COURSE_NONE || courseNum > COURSE_STAGES_MAX) && 
		!(sWarpDest.levelNum==LEVEL_CASTLE_GROUNDS&&sWarpDest.nodeId==0x04)){
		gWarpTransition.data.red = 0;
		gWarpTransition.data.green = 0;
		gWarpTransition.data.blue = 0;
		set_warp_transition_rgb(0,0,0);
	} else {
		gWarpTransition.data.red = 255;
		gWarpTransition.data.green = 255;
		gWarpTransition.data.blue = 255;
		set_warp_transition_rgb(255,255,255);
	}
	
	bzero(sTransitionColorFadeCount,sizeof(sTransitionColorFadeCount));
	bzero(sTransitionTextureFadeCount,sizeof(sTransitionTextureFadeCount));
	gPracticeWarping = TRUE;
}

extern s16 gMenuMode;
extern u16 gRandomSeed16;
extern u16 gRandomCalls;
extern s16 gMovtexCounter;
extern s16 gMovtexCounterPrev;
extern s16 gPaintingUpdateCounter;
extern s16 gLastPaintingUpdateCounter;
extern s16 sPrevCheckMarioRoom;
extern s8 gEnvFxMode;
extern Vec3i gSnowCylinderLastPos;
extern s16 gSparklePhase;
extern u8 sPssSlideStarted;
extern struct CutsceneVariable sCutsceneVars[10];
extern struct ModeTransitionInfo sModeInfo;
extern s16 sHandheldShakeMag;
extern f32 sHandheldShakeTimer;
extern f32 sHandheldShakeInc;
extern s16 sStatusFlags;
extern struct CameraFOVStatus sFOVState;
extern struct TransitionInfo sModeTransition;
extern Vec3f sOldPosition;
extern Vec3f sOldFocus;
extern struct PlayerGeometry sMarioGeometry;
extern s16 sAvoidYawVel;
extern struct HandheldShakePoint sHandheldShakeSpline[4];
extern u16 gCutsceneMsgFade;
extern s16 gCutsceneMsgIndex;
extern s16 gCutsceneMsgDuration;
extern s16 gCutsceneMsgTimer;

// reset state like the game was powercycled
void reset_game_state(void){
	gCurrSaveFileNum = 1;
	gMenuHoldKeyIndex = 0;
	gMenuHoldKeyTimer = 0;
	gCutsceneMsgTimer = 0;
	gCutsceneMsgFade = 0;
	gCutsceneMsgIndex = -1;
	gCutsceneMsgDuration = -1;
	gWarpTransition.isActive = FALSE;
	gWarpTransition.time = 0;
	gAPressCounter = 0;
	disable_warp_checkpoint();
	reset_dialog_render_state();
	gRandomSeed16 = 0;
	gRandomCalls = 0;
	gGlobalTimer = -1;
	gPaintingUpdateCounter = 1;
	gLastPaintingUpdateCounter = 0;
	gMovtexCounter = 1;
	gMovtexCounterPrev = 0;
	gAreaUpdateCounter = 0;
	sPrevCheckMarioRoom = 0;
	sPssSlideStarted = FALSE;
	bzero(gSnowCylinderLastPos,sizeof(gSnowCylinderLastPos));
	sAvoidYawVel = 0;
	sTransitionTimer = 0;
	sTransitionUpdate = NULL;
	bzero(&sModeTransition,sizeof(struct TransitionInfo));
	bzero(sOldPosition,sizeof(Vec3f));
	bzero(sOldFocus,sizeof(Vec3f));
	bzero(&sMarioGeometry,sizeof(struct PlayerGeometry));
	bzero(&sHandheldShakeSpline,sizeof(struct HandheldShakePoint)*4);
	gEnvFxMode = 0;
	gSparklePhase = 0;
	gMarioState->slideVelX = 0.0f;
	gMarioState->slideVelZ = 0.0f;
	gMarioState->twirlYaw = 0;
	gMarioState->slideYaw = 0;
	gMarioState->numCoins = 0;
	gHudDisplay.coins = 0;
	gHudDisplay.wedges = 8;
	gMarioState->health = 0x880;
	if (gMarioObject)
		gMarioObject->platform = NULL;
	bzero(gMarioState->marioBodyState->heldObjLastPosition,sizeof(Vec3f));
	bzero(&gLakituState,sizeof(struct LakituState));
	bzero(&gPlayerCameraState[0],sizeof(struct PlayerCameraState)*2);
	bzero(&sCutsceneVars[0],sizeof(struct CutsceneVariable)*10);
	bzero(&sModeInfo,sizeof(struct ModeTransitionInfo));
	bzero(&sFOVState,sizeof(struct CameraFOVStatus));
	gHudFlash = FALSE;
	sStatusFlags = 0;
	sHandheldShakeMag = 0;
	sHandheldShakeInc = 0.0f;
	sHandheldShakeTimer = 0.0f;
	gLastCompletedStarNum = 0;
	gCameraMovementFlags = 0;
	gObjCutsceneDone = 0;
	gCurrCourseNum = 0;
	gMenuMode = -1;
	gWarpTransDelay = 0;
	sPowerMeterVisibleTimer = 0;
	sPowerMeterStoredHealth = 8;
	sPowerMeterHUD.animation = POWER_METER_HIDDEN;
	bzero(sTransitionColorFadeCount,sizeof(sTransitionColorFadeCount));
	bzero(sTransitionTextureFadeCount,sizeof(sTransitionTextureFadeCount));
}

void soft_reset(void){
	if (gCurrLevelNum==1)
		return;
	
	if (gCurrRecordingReplay){
		gCurrRecordingReplay->flags |= REPLAY_FLAG_RESET;
		save_replay();
	}
	
	gDisableRendering = FALSE;
	reset_game_state();
	
	sCurrPlayMode = PLAY_MODE_CHANGE_LEVEL;
	gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
	gPracticeDest.levelNum = 1;
	gPracticeDest.areaIdx = 1;
	gPracticeDest.nodeId = 0;
	gPracticeDest.arg = 0;
	practice_warp();
	
	practice_soft_reset();
	clear_save_states();
	// 5.73
}

void practice_choose_level(void){
	gCurrPlayingReplay = NULL;
	gCurrRecordingReplay = NULL;
	
	clear_transit_state();
	gNoStarSelectWarp = FALSE;
	
	// don't activate HUD if already in star select
	if (gCurrentArea&&gCurrentArea->camera)
		gHudDisplay.flags = HUD_DISPLAY_DEFAULT;
	
	sResetActNum.enabled = FALSE;
	
	if (configPracticeType==PRACTICE_TYPE_XCAM||
		configPracticeType==PRACTICE_TYPE_STAR_GRAB){
		gIsRTA = TRUE;
		section_timer_start();
	} else if (configPracticeType==PRACTICE_TYPE_STAGE){
		section_timer_start();
		sStageRTAPrimed = TRUE;
		gIsRTA = TRUE;
	} else if (configPracticeType==PRACTICE_TYPE_GAME){
		gIsRTA = FALSE;
	}
	
	if (configUseGhost)
		ghost_start_playback();
	
	stats_level_reset(gPracticeDest.levelNum,gPracticeDest.areaIdx);
}

void practice_game_start(void){
	gCurrSaveFileNum = 1;
	clear_current_save_file();
	if (sResetSave.enabled){
		memcpy(&gSaveBuffer.files[0][0],sResetSave.varPtr,sizeof(struct SaveFile));
	}
	
	if (gPlaybackPrimed){
		start_replay_playback(TRUE);
		update_replay();
		update_replay();
	} else if (configPracticeType==PRACTICE_TYPE_GAME){
		start_recording_replay();
	}
}

extern f32 gPaintingMarioYEntry;

s32 gPracticeWDWHeight = 0;
static u8 sSetWDWHeight = FALSE;
static f32 sWDWPaintingHeights[3] = {1000.0f,1500.0f,2000.0f};

void practice_update_wdw_height(void){
	if (gCurrRecordingReplay&&gCurrRecordingReplay->state.type==LEVEL_INIT&&gCurrRecordingReplay->state.levelState){
		LevelInitState* state = (LevelInitState*)gCurrRecordingReplay->state.levelState;
		
		state->envRegionHeights[0] = gEnvironmentRegions[6];
		state->envRegionHeights[1] = gEnvironmentRegions[12];
		state->envRegionHeights[2] = gEnvironmentRegions[18];
	}
}

void update_save_file_vars(void){
	gMarioState->numStars = save_file_get_total_star_count(gCurrSaveFileNum - 1, COURSE_MIN - 1, COURSE_MAX - 1);
	if (save_file_get_flags()
        & (SAVE_FLAG_CAP_ON_GROUND | SAVE_FLAG_CAP_ON_KLEPTO | SAVE_FLAG_CAP_ON_UKIKI
           | SAVE_FLAG_CAP_ON_MR_BLIZZARD)) {
        gMarioState->flags &= ~MARIO_NORMAL_CAP;
		if (!(gMarioState->flags & (MARIO_VANISH_CAP | MARIO_WING_CAP | MARIO_METAL_CAP))){
			gMarioState->flags &= ~MARIO_CAP_ON_HEAD;
		}
    } else {
        gMarioState->flags |= MARIO_CAP_ON_HEAD | MARIO_NORMAL_CAP;
    }
}

static void init_reset_vars(void){
	sResetActNum.enabled = FALSE;
	sResetActNum.varU32 = 1;
	
	sResetGlobalTimer.enabled = FALSE;
	sResetGlobalTimer.varU32 = 0;
	
	sResetRNG.enabled = FALSE;
	sResetRNG.varU32 = 0;
	
	sResetLives.enabled = FALSE;
	sResetLives.varS32 = 4;
	
	sResetSwimStrength.enabled = FALSE;
	sResetSwimStrength.varU32 = 16;
	
	sResetCamMode.enabled = FALSE;
	sResetCamMode.varU32 = 0;
	
	sResetSave.enabled = FALSE;
	sResetSave.varPtr = malloc(sizeof(struct SaveFile));
	memset(sResetSave.varPtr,0,sizeof(struct SaveFile));
}

static void apply_reset_vars(void){
	if (sResetActNum.enabled){
		gCurrActNum = sResetActNum.varU32;
	}
	
	if (sResetGlobalTimer.enabled){
		gGlobalTimer = sResetGlobalTimer.varU32;
	}
	
	if (sResetRNG.enabled){
		gRandomSeed16 = sResetRNG.varU32;
		gRandomCalls = gRNGTable[gRandomSeed16];
	}
	
	if (sResetLives.enabled){
		gMarioState->numLives = sResetLives.varS32;
	}
	
	if (sResetSwimStrength.enabled){
		sSwimStrength = sResetSwimStrength.varU32*10;
	}
	
	if (sResetCamMode.enabled){
		// mario cam
		if (sResetCamMode.varU32==0)
			sSelectionFlags = CAM_MODE_MARIO_SELECTED;
		else // fixed cam
			sSelectionFlags &= ~CAM_MODE_MARIO_SELECTED;
	}
	
	if (sResetSave.enabled){
		gSaveBuffer.files[gCurrSaveFileNum-1][0] = *(struct SaveFile*)sResetSave.varPtr;
		update_save_file_vars();
	}
}

void copy_save_file_to_reset_var(PracticeSetting*){
	memcpy(sResetSave.varPtr,&gSaveBuffer.files[gCurrSaveFileNum-1][0],sizeof(struct SaveFile));
}

void apply_reset_vars_to_replay(PracticeSetting*){
	LevelInitState* state;
	
	if (gCurrPlayingReplay){
		if (gCurrPlayingReplay->state.type!=LEVEL_INIT)
			return;
		state = gCurrPlayingReplay->state.levelState;
	} else if (gCurrRecordingReplay){
		if (gCurrRecordingReplay->state.type!=LEVEL_INIT)
			return;
		state = gCurrRecordingReplay->state.levelState;
	} else if (gPracticeFinishedReplay){
		if (gPracticeFinishedReplay->state.type!=LEVEL_INIT)
			return;
		state = gPracticeFinishedReplay->state.levelState;
	} else {
		return;
	}
	
	u8 applied = FALSE;
	
	if (sResetActNum.enabled){
		state->currActNum = sResetActNum.varU32;
		applied = TRUE;
	}
	
	if (sResetGlobalTimer.enabled){
		state->globalTimer = sResetGlobalTimer.varU32;
		applied = TRUE;
	}
	
	if (sResetRNG.enabled){
		state->randState = sResetRNG.varU32;
		state->randCalls = gRNGTable[state->randState];
		applied = TRUE;
	}
	
	if (sResetLives.enabled){
		state->numLives = sResetLives.varS32;
		applied = TRUE;
	}
	
	if (sResetSwimStrength.enabled){
		state->swimStrength = sResetSwimStrength.varU32*10;
		applied = TRUE;
	}
	
	if (sResetCamMode.enabled){
		if (sResetCamMode.varU32==0)
			state->camSelectionFlags = CAM_MODE_MARIO_SELECTED;
		else
			state->camSelectionFlags &= ~CAM_MODE_MARIO_SELECTED;
		applied = TRUE;
	}
	
	if (sResetSave.enabled){
		state->saveFile = *(struct SaveFile*)sResetSave.varPtr;
		applied = TRUE;
	}
	
	if (applied)
		practice_set_message("Applied reset vars to replay");
}

void update_mario_info_for_cam(struct MarioState*);
extern Mat4 sFloorAlignMatrix[2];
extern u8 gShouldSetMarioThrow;

void practice_fix_mario_rotation(void){
	if (gMarioState->marioObj){
		update_mario_info_for_cam(gMarioState);
		if (gShouldSetMarioThrow){
			//switch (gMarioState->action){
				//case ACT_CRAWLING:
				//case ACT_DIVE_SLIDE:
					gMarioState->marioObj->header.gfx.throwMatrix = &sFloorAlignMatrix[gMarioState->unk00];
					//break;
				
				//default:
//					break;
			//}
		}
	}
}

void practice_level_init(void){
	if (sSetWDWHeight){
		gPaintingMarioYEntry = sWDWPaintingHeights[gPracticeWDWHeight];
		sSetWDWHeight = FALSE;
	}
	
	if (gPlaybackPrimed&&gPracticeWarping){
		start_replay_playback(TRUE);
		if (configPracticeType==PRACTICE_TYPE_GAME&&configSkipIntro){
			update_replay();
			update_replay();
			update_replay();
		}
	} else if (gCurrPlayingReplay==NULL){
		if (configPracticeType==PRACTICE_TYPE_XCAM||
			configPracticeType==PRACTICE_TYPE_STAR_GRAB){
			apply_reset_vars();
			section_timer_start();
			gSectionTimer = 1;
			start_recording_replay();
		}
		
		if (sStageRTAPrimed){
			apply_reset_vars();
			section_timer_start();
			gSectionTimer = 1;
			start_recording_replay();
			
			//sStageRTAAct = gCurrActNum;
			// fix 0 act bug
			//if (sStageRTAAct==0) sStageRTAAct = 1;
			sStageRTAPrimed = FALSE;
		}
	}
	
	gHeightLock = 0.0f;
	
	if (sFrameAdvanceStorage){
		gFrameAdvance = TRUE;
		sFrameAdvanceStorage = FALSE;
	}
}

void practice_star_grab(void){
	if (configPracticeType==PRACTICE_TYPE_STAR_GRAB){
		section_timer_finish();
		save_replay();
	} else if (configPracticeType==PRACTICE_TYPE_STAGE){
		++sStageRTAStarsCompleted;
	} else {
		timer_freeze();
	}
}

// finishes if xcam or if the stage RTA is completed (for nonstop)
static void practice_common_xcam_finish(void){
	if (configPracticeType==PRACTICE_TYPE_XCAM){
		section_timer_finish();
		save_replay();
	} else if (configPracticeType==PRACTICE_TYPE_STAGE){
		if (sStageRTAStarsCompleted>=sStageRTAStarsTarget){
			section_timer_finish();
			save_replay();
		} else {
			timer_freeze();
		}
	} else {
		timer_freeze();
	}
}

void practice_star_xcam(void){
	practice_common_xcam_finish();
}

void practice_death_exit(void){
	practice_common_xcam_finish();
}

void practice_pause_exit(void){
	practice_common_xcam_finish();
}

void practice_door_exit(void){
	practice_common_xcam_finish();
}

void practice_level_change_trigger(void){
	struct ObjectWarpNode* warpNode = area_get_warp_node(sSourceWarpNodeId);
	// level change, not area change
	if ((warpNode && (warpNode->node.destLevel & 0x7F)!=gCurrLevelNum) ||
		sWarpDest.type==WARP_TYPE_CHANGE_LEVEL){
		if (configPracticeType==PRACTICE_TYPE_XCAM){
			section_timer_finish();
			save_replay();
		} else {
			timer_freeze();
		}
	}
}

void practice_painting_trigger(void){
	if (sWarpDest.type==WARP_TYPE_CHANGE_LEVEL){
		if (configPracticeType==PRACTICE_TYPE_XCAM){
			section_timer_finish();
			save_replay();
		} else {
			timer_freeze();
		}
	} else {
		// area painting warp (TTM slide)
		timer_freeze();
	}
}

void practice_game_win(void){
	if (configPracticeType==PRACTICE_TYPE_STAR_GRAB||
		configPracticeType==PRACTICE_TYPE_XCAM||
		configPracticeType==PRACTICE_TYPE_GAME){
		if (gSectionTimerResult==NULL_SECTION_TIMER_RESULT){
			section_timer_finish();
			save_replay();
		}
	}
}
//23253
void practice_file_select(void){
	sStageRTAStarsCompleted = 0;
}

// called after file select
void practice_intro_skip_start(void){
	if (configPracticeType!=PRACTICE_TYPE_GAME || !configIntroSkipTiming){
		section_timer_start();
		gSectionTimer -= 1;
	} else {
		section_timer_start();
		gSectionTimer = INTRO_SKIP_TIME_START;
		gSectionTimer -= 1;
	}
	
	gGlobalTimer = INTRO_SKIP_TIME_START;
	gRandomSeed16 = INTRO_SKIP_RNG_START;
	gRandomCalls = INTRO_SKIP_RNG_CALLS_START;
}

void practice_soft_reset(void){
	section_timer_start();
	gSectionTimer = -1;
	sSectionTimerLockTime = 0;
	gIsRTA = TRUE;
	if (gCurrPlayingReplay)
		gCurrPlayingReplay = NULL;
}

void intro_skip_reset(void){
	reset_game_state();
	
	sCurrPlayMode = PLAY_MODE_CHANGE_LEVEL;
	clear_current_save_file();
	if (sResetSave.enabled){
		memcpy(&gSaveBuffer.files[gCurrSaveFileNum-1][0],sResetSave.varPtr,sizeof(struct SaveFile));
	}
	lvl_init_from_save_file(0,0);
	
	gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
	gPracticeDest.levelNum = LEVEL_CASTLE_GROUNDS;
	gPracticeDest.areaIdx = 1;
	gPracticeDest.nodeId = 0x04;
	practice_warp();
	
	section_timer_start();
	gSectionTimer = -1;
	sSectionTimerLockTime = 0;
	gIsRTA = TRUE;
	if (gCurrPlayingReplay)
		gCurrPlayingReplay = NULL;
	
	if (configPracticeType==PRACTICE_TYPE_GAME){
		if (!gPlaybackPrimed)
			start_recording_replay();
	}
}

extern u16 sCurrentMusic;
extern u8 gInStarSelect;
void practice_reset(void){
	if (gInStarSelect)
		return;
	if (gCurrLevelNum==1)
		return;
	
	if (sWarpDest.type==WARP_TYPE_CHANGE_LEVEL&&
		 sWarpDest.levelNum==gLastWarpDest.levelNum&&
		 sWarpDest.areaIdx==gLastWarpDest.areaIdx&&
		 sWarpDest.nodeId==gLastWarpDest.nodeId)
		return;
	
	if (configUseGhost)
		ghost_start_playback();
	
	if (gCurrRecordingReplay){
		gCurrRecordingReplay->flags |= REPLAY_FLAG_RESET;
		save_replay();
	}
	
	if (gCurrPlayingReplay)
		gCurrPlayingReplay = NULL;
	
	gIsRTA = TRUE;
	stats_level_reset(gCurrLevelNum,gCurrAreaIndex);
	gPracticeDest = gLastWarpDest;
	//gCurrLevelArea = gLastLevelNum*16+1;
	gCurrLevelNum = gLastLevelNum;
	gCurrAreaIndex = 1;
	
	gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
	clear_transit_state();
	stop_sounds_in_continuous_banks();
	if (configResetMusic)
		sCurrentMusic = 0;
	
	if (configPracticeType==PRACTICE_TYPE_XCAM||
		configPracticeType==PRACTICE_TYPE_STAR_GRAB){
		//if (gCurrLevelNum==6){
			//gPracticeDest.areaIdx = gCurrAreaIndex;
		//}
		if (sResetActNum.enabled)
			gCurrActNum = sResetActNum.varU32;
	} else if (configPracticeType==PRACTICE_TYPE_STAGE){
		if (sResetActNum.enabled)
			gCurrActNum = sResetActNum.varU32;
		//gCurrActNum = sStageRTAAct;
		sStageRTAStarsCompleted = 0;
		sStageRTAPrimed = TRUE;
	} else if (configPracticeType==PRACTICE_TYPE_GAME){
		if (!configSkipIntro)
			soft_reset();
		else
			intro_skip_reset();
	}
	if (gCurrentArea)
		reset_camera(gCurrentArea->camera);
	
	if (gCurrLevelNum==LEVEL_WDW)
		sSetWDWHeight = TRUE;
}

extern u32 gPrevLevel;
// 1648
// 54.90
void replay_warp(void){
	if (!gPracticeFinishedReplay) return;
	if (gCurrLevelNum==1) return;
	if (gPlaybackPrimed) return;
	if (gFrameAdvance){
		gFrameAdvance = FALSE;
	}
	
	if (configUseGhost)
		ghost_start_playback();
	
	gPlaybackPrimed = TRUE;
	if (gPracticeFinishedReplay->state.type==LEVEL_INIT){
		LevelInitState* levelInit = (LevelInitState*)gPracticeFinishedReplay->state.levelState;
		load_level_init_state(levelInit);
		gPracticeDest = levelInit->loc;
		gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
		gCurrActNum = levelInit->currActNum;
		sStageRTAStarsCompleted = 0;
		if (gCurrLevelNum!=levelInit->loc.levelNum||gCurrAreaIndex!=levelInit->loc.areaIdx){
			gDisableRendering = TRUE;
		}
		gCurrLevelArea = gLastLevelNum*16+1;
		clear_transit_state();
	} else if (gPracticeFinishedReplay->state.type==GAME_INIT){
		load_game_init_state(gPracticeFinishedReplay->state.gameState);
		clear_transit_state();
		gNoStarSelectWarp = FALSE;
		if (configSkipIntro)
			intro_skip_reset();
		else
			soft_reset();
	}
	
	if (gCurrRecordingReplay){
		free_replay(gPracticeRecordingReplay);
		gPracticeRecordingReplay = NULL;
		gCurrRecordingReplay = NULL;
	}
	if (gCurrPlayingReplay){
		gCurrPlayingReplay = NULL;
	}
}

u8 has_save_state(void){
	return gCurrLoadStateSlot->state!=NULL;
}

static u8 sReplaySkipLastLevel = 0;
static u8 sReplaySkipLastArea = 0;
static u8 sDownTriggered = FALSE;
static u8 sDownPrimed = FALSE;
extern s8 gResetTrigger;

void practice_update(void){
	u8 canLoad = can_load_save_state(gCurrLoadStateSlot->state);
	static char practiceMsg[128];
	
	if (sLoadSaveStateNow&&canLoad){
		gDisableRendering = FALSE;
		sLoadSaveStateNow = FALSE;
		practice_load_state(gCurrLoadStateSlot->state);
		if (sFrameAdvanceStorage){
			// remove this?
			gFrameAdvance = TRUE;
			sFrameAdvanceStorage = FALSE;
		}
	}
	
	if (gFrameAdvance){
		gIsRTA = FALSE;
		if (gCurrRecordingReplay){
			gCurrRecordingReplay->flags |= REPLAY_FLAG_FRAME_ADVANCED;
		}
	}
	
	if (!gRTAMode){
		if (gPlayer1Controller->buttonPressed & D_JPAD){
			sDownTriggered = TRUE;
			sDownPrimed = TRUE;
		}
	} else {
		if ((gPlayer1Controller->buttonPressed & D_JPAD) && (gPlayer1Controller->buttonDown & L_TRIG)){
			sDownTriggered = TRUE;
			sDownPrimed = TRUE;
		}
	}
	
	if (sDownTriggered){
		if (gPlayer1Controller->buttonPressed & (L_JPAD|R_JPAD|U_JPAD|L_CBUTTONS|R_CBUTTONS|L_TRIG|R_TRIG|B_BUTTON)){
			// don't trigger level reset/advance
			sDownPrimed = FALSE;
		}
		
		if (!gDisableRendering){
			// savestate slot controls
			if (!((gPlayer1Controller->buttonPressed & R_JPAD) && (gPlayer1Controller->buttonPressed & L_JPAD))){
				if (gPlayer1Controller->buttonPressed & R_JPAD){
					save_slot_increment();
					snprintf(practiceMsg,sizeof(practiceMsg),"Save slot %d",gCurrSaveStateIndex+1);
					practice_set_message(practiceMsg);
				} else if (gPlayer1Controller->buttonPressed & L_JPAD){
					save_slot_decrement();
					snprintf(practiceMsg,sizeof(practiceMsg),"Save slot %d",gCurrSaveStateIndex+1);
					practice_set_message(practiceMsg);
				}
			}
			
			if (!((gPlayer1Controller->buttonPressed & R_CBUTTONS) && (gPlayer1Controller->buttonPressed & L_CBUTTONS))){
				if (gPlayer1Controller->buttonPressed & R_CBUTTONS){
					load_slot_increment();
					snprintf(practiceMsg,sizeof(practiceMsg),"Load slot %d",gCurrLoadStateIndex+1);
					practice_set_message(practiceMsg);
				} else if (gPlayer1Controller->buttonPressed & L_CBUTTONS){
					load_slot_decrement();
					snprintf(practiceMsg,sizeof(practiceMsg),"Load slot %d",gCurrLoadStateIndex+1);
					practice_set_message(practiceMsg);
				}
			}
		}
		
		if (!(gPlayer1Controller->buttonDown & D_JPAD)){
			if (sDownPrimed){
				gOpenPracticeMenuNextFrame = TRUE;
			}
			
			sDownTriggered = FALSE;
			sDownPrimed = FALSE;
		} else {
			// still holding D_JPAD
			if (!gRTAMode && (gPlayer1Controller->buttonPressed & R_TRIG)){
				// playback replay
				replay_warp();
			}
		}
	} else {
		// not holding D_JPAD
		u8 levelResetButtonCombo;
		if (gRTAMode){
			levelResetButtonCombo = (gPlayer1Controller->buttonPressed & START_BUTTON) && 
									(gPlayer1Controller->buttonDown & L_TRIG) &&
									(gPlayer1Controller->buttonDown & R_TRIG);
		} else {
			levelResetButtonCombo = gPlayer1Controller->buttonPressed & L_TRIG;
		}
		if (levelResetButtonCombo || gWillPracticeReset){
			if (!gFrameAdvance || gWillPracticeReset){
				gWillPracticeReset = FALSE;
				// level reset
				practice_reset();
			} else {
				gFrameAdvancedThisFrame = TRUE;
			}
		} else if (gPlayer1Controller->buttonPressed & R_JPAD){
			if (!gCurrSaveStateSlot->state){
				gCurrSaveStateSlot->state = alloc_state();
			}
			save_state(gCurrSaveStateSlot->state);
			snprintf(practiceMsg,sizeof(practiceMsg),"Saved to slot %d",gCurrSaveStateIndex+1);
			practice_set_message(practiceMsg);
		} else if (!gRTAMode&&(gPlayer1Controller->buttonPressed & U_JPAD)&&has_save_state()){
			snprintf(practiceMsg,sizeof(practiceMsg),"Loaded from slot %d",gCurrLoadStateIndex+1);
			practice_set_message(practiceMsg);
			if (canLoad){
				gDisableRendering = FALSE;
				practice_load_state(gCurrLoadStateSlot->state);
			} else {
				gPracticeDest = gCurrLoadStateSlot->state->levelState.loc;
				gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
				gPracticeDest.nodeId = 0xA;
				gPracticeDest.arg = 0;
				gCurrActNum = gCurrLoadStateSlot->state->levelState.currActNum;
				gSaveStateWarpDelay = 1;
				clear_transit_state();
				gDisableRendering = TRUE;
			}
		}
		
		if (!gRTAMode && (gPlayer1Controller->buttonPressed & L_JPAD)){
			gFrameAdvance = !gFrameAdvance;
		}
		
		if (!gFrameAdvance&&gCurrPlayingReplay){
			if (gStoredReplayController.buttonPressed & R_TRIG){
				gReplaySkipToNextArea = TRUE;
				sReplaySkipLastLevel = gCurrLevelNum;
				sReplaySkipLastArea = gCurrAreaIndex;
			}
		}
	}
	
	if (gReplaySkipToNextArea){
		if (!gCurrPlayingReplay){
			gDisableRendering = FALSE;
			gReplaySkipToNextArea = FALSE;
		} else {
			gDisableRendering = TRUE;
			if (sReplaySkipLastLevel!=gCurrLevelNum||sReplaySkipLastArea!=gCurrAreaIndex){
				gDisableRendering = FALSE;
				gReplaySkipToNextArea = FALSE;
			}
		}
	}
	
	if (gWallkickTimer){
		--gWallkickTimer;
	}
	
	if (gResetTrigger){
		gResetTrigger = 0;
		soft_reset();
	}
}

void save_state_update(void){
	u8 canLoad = can_load_save_state(gCurrLoadStateSlot->state);
	
	if (gSaveStateWarpDelay!=0&&canLoad){
		if (--gSaveStateWarpDelay==0){
			sLoadSaveStateNow = TRUE;
		}
	}
}

void update_practice_bowser_info(s16 angleVel,s16 spin){
	sBowserAngleVel = abs(angleVel);
	
	sSpinArray[sSpinIndex++] = abs(spin);
	sSpinIndex %= sizeof(sSpinArray)/sizeof(sSpinArray[0]);
	sHoldingBowser = 20;
}

const char* replay_length_info(PracticeSetting* setting){
	if (setting->index==0){
		if (!gPracticeFinishedReplay){
			return "empty";
		}
		
		set_timer_text(get_replay_length(gPracticeFinishedReplay));
		return gTimerText;
	} else {
		if (!gPracticeRecordingReplay){
			return "empty";
		}
		
		set_timer_text(get_replay_length(gPracticeRecordingReplay));
		return gTimerText;
	}
}

const char* archive_length_info(PracticeSetting* s){
	const char* nullResp = "empty";
	u32 index = s->index;
	if (gReplayHistory[index]==NULL)
		return nullResp;
	
	set_timer_text(get_replay_length(gReplayHistory[index]));
	return gTimerText;
}

void button_archive_load_replay(PracticeSetting* s){
	if (gReplayHistory[s->index]==NULL)
		return;
	
	archive_load_replay(&gPracticeFinishedReplay,s->index);
}

void button_play_current_replay(PracticeSetting*){
	if (!gPracticeFinishedReplay)
		return;
	
	replay_warp();
	gClosePracticeMenuNextFrame = TRUE;
}

const char* practice_stats_total_playtime(void){
	set_timer_text_rough(gPracticeStats.timePracticing);
	return gTimerText;
}

const char* practice_stats_total_resets(void){
	static char resetsText[20];
	
	snprintf(resetsText,sizeof(resetsText),"%u",gPracticeStats.levelResetCount);
	return resetsText;
}

void enable_rta_mode(void){
	configShowAngle = FALSE;
	configShowPos = FALSE;
	configShowVel = FALSE;
	configShowMaxHeight = FALSE;
	configShowSlidingVel = FALSE;
	configShowTwirlYaw = FALSE;
	configShowHOLP = FALSE;
	configShowRNGInfo = FALSE;
	configShowSwimStrength = FALSE;
	configShowSwimTrainer = FALSE;
	configShowEfficiency = FALSE;
	configShowBowserInfo = FALSE;
	configShowWallkickAngle = FALSE;
	configShowWallkickFrame = FALSE;
	configShowGlobalTimer = FALSE;
	gRTAMode = TRUE;
	
	set_practice_type(PRACTICE_TYPE_GAME);
	
	practice_reset();
	gClosePracticeMenuNextFrame = TRUE;
}

typedef struct {
	const char* name;
	u16 levelNum;
	u16 areaIdx;
	u16 nodeId;
} PracticeDest;

static const PracticeDest sPracticeDests[] = {
	{"BoB",LEVEL_BOB,1,0xA},
	{"WF",LEVEL_WF,1,0xA},
	{"JRB",LEVEL_JRB,1,0xA},
	{"CCM",LEVEL_CCM,1,0xA},
	{"BBH",LEVEL_BBH,1,0xA},
	{"HMC",LEVEL_HMC,1,0xA},
	{"LLL",LEVEL_LLL,1,0xA},
	{"SSL",LEVEL_SSL,1,0xA},
	{"DDD",LEVEL_DDD,1,0xA},
	{"SL",LEVEL_SL,1,0xA},
	{"WDW",LEVEL_WDW,1,0xA},
	{"TTM",LEVEL_TTM,1,0xA},
	{"THIT",LEVEL_THI,2,0xA},
	{"THIH",LEVEL_THI,1,0xA},
	{"TTC",LEVEL_TTC,1,0xA},
	{"RR",LEVEL_RR,1,0xA},
	
	{"TotWC",LEVEL_TOTWC,1,0xA},
	{"CotMC",LEVEL_COTMC,1,0xA},
	{"VCutM",LEVEL_VCUTM,1,0xA},
	{"WMotR",LEVEL_WMOTR,1,0xA},
	{"PSS",LEVEL_PSS,1,0xA},
	{"SA",LEVEL_SA,1,0xA},
	
	{"BitDW",LEVEL_BITDW,1,0xA},
	{"BitFS",LEVEL_BITFS,1,0xA},
	{"BitS",LEVEL_BITS,1,0xA},
	{"Bow1",LEVEL_BOWSER_1,1,0xA},
	{"Bow2",LEVEL_BOWSER_2,1,0xA},
	{"Bow3",LEVEL_BOWSER_3,1,0xA},
	
	{"Lobby",LEVEL_CASTLE,1,0x0},
	{"Basement",LEVEL_CASTLE,3,0x0},
	{"Upstairs",LEVEL_CASTLE,2,0x1},
	{"Outside",LEVEL_CASTLE_GROUNDS,1,0x4},
	{"Courtyard",LEVEL_CASTLE_COURTYARD,1,0x1},
};
#define PRACTICE_LEVEL_COUNT ARRAY_LEN(sPracticeDests)

typedef struct {
	u16 levelNumA;
	u16 areaIdxA;
	u16 nodeIdA;
	u16 levelNumB;
	u16 areaIdxB;
	u16 nodeIdB;
} PracticeAltDest;

static const PracticeAltDest sPracticeAltDests[] = {
	{LEVEL_CASTLE,1,0x32,LEVEL_CASTLE,1,0x64}, // bob
	{LEVEL_CASTLE,1,0x34,LEVEL_CASTLE,1,0x66}, // wf
	{LEVEL_CASTLE,1,0x35,LEVEL_CASTLE,1,0x67}, // jrb
	{LEVEL_CASTLE,1,0x33,LEVEL_CASTLE,1,0x65}, // ccm
	{LEVEL_CASTLE_COURTYARD,1,0xA,LEVEL_CASTLE_COURTYARD,1,0xB}, // bbh
	{LEVEL_CASTLE,3,0x34,LEVEL_CASTLE,3,0x66}, // hmc
	{LEVEL_CASTLE,3,0x32,LEVEL_CASTLE,3,0x64}, // lll
	{LEVEL_CASTLE,3,0x33,LEVEL_CASTLE,3,0x65}, // ssl
	{LEVEL_CASTLE,3,0x35,LEVEL_CASTLE,3,0x67}, // ddd
	{LEVEL_CASTLE,2,0x36,LEVEL_CASTLE,2,0x68}, // sl
	{LEVEL_CASTLE,2,0x32,LEVEL_CASTLE,2,0x64}, // wdw
	{LEVEL_CASTLE,2,0x34,LEVEL_CASTLE,2,0x66}, // ttm
	{LEVEL_CASTLE,2,0x33,LEVEL_CASTLE,2,0x65}, // thit
	{LEVEL_CASTLE,2,0x37,LEVEL_CASTLE,2,0x69}, // thih
	{LEVEL_CASTLE,2,0x35,LEVEL_CASTLE,2,0x67}, // ttc
	{LEVEL_CASTLE,2,0x3A,LEVEL_CASTLE,2,0x6C}, // rr
	
	{LEVEL_CASTLE,1,0x26,LEVEL_CASTLE,1,0x23}, // totwc
	{LEVEL_CASTLE,3,0x34,LEVEL_CASTLE,3,0x66}, // cotmc
	{LEVEL_CASTLE_GROUNDS,1,0x8,LEVEL_CASTLE_GROUNDS,1,0x6}, // vcutm
	{LEVEL_CASTLE,2,0x38,LEVEL_CASTLE,2,0x6D}, // wmotr
	{LEVEL_CASTLE,1,0x26,LEVEL_CASTLE,1,0x23}, // pss
	{LEVEL_CASTLE,1,0x27,LEVEL_CASTLE,1,0x28}, // sa
	
	{LEVEL_BOWSER_1,1,0xA,LEVEL_CASTLE,1,0x25}, // bitdw
	{LEVEL_BOWSER_2,1,0xA,LEVEL_CASTLE,3,0x68}, // bitfs
	{LEVEL_BOWSER_3,1,0xA,LEVEL_CASTLE,2,0x6B}, // bits
	
	{LEVEL_CASTLE,1,0x24,LEVEL_BITDW,1,0xC}, // bowser1
	{LEVEL_CASTLE,3,0x36,LEVEL_BITFS,1,0xC}, // bowser2
	{0,0,0,LEVEL_BITS,1,0xC}, // bowser3
	
	{LEVEL_CASTLE,1,0x1F,LEVEL_CASTLE_GROUNDS,1,0x3}, // lobby
	{0,0,0,LEVEL_CASTLE_GROUNDS,1,0x3}, // basement
	{0,0,0,LEVEL_CASTLE_GROUNDS,1,0x3}, // upstairs
	{LEVEL_CASTLE_GROUNDS,1,0xA,LEVEL_CASTLE_GROUNDS,1,0x3}, // outside
	{0,0,0,LEVEL_CASTLE_GROUNDS,1,0x3}, // courtyard
};

s32 gPracticeStageSelect = 0;
static s32 sYScroll = 0;
static u8 sInLevelStats = FALSE;

static u32 get_num_width(s32 num){
	if (num==0) return 1;
	
	u32 width = 0;
	if (num<0){
		++width;
		num *= -1;
	}
	
	while (num>0){
		num /= 10;
		++width;
	}
	
	return width;
}

static char* justify_text(char* start,u32 width){
	for (u32 i=0;i<width;++i){
		*start++ = ' ';
	}
	return start;
}

void render_practice_level_stats(void){
	char text[32];
	gCurrTextScale = 1.25f;
	const PracticeDest* currLevel = &sPracticeDests[gPracticeStageSelect];
	
	sprintf(text,"%s Level Stats",currLevel->name);
	render_shadow_text_string_at(12,SCREEN_HEIGHT-32,text);
	s32 width;
	
	u32 time = 0;
	u32 stars = 0;
	u32 coins = 0;
	u32 resets = 0;
	f64 dist = 0.0;
	
	if (currLevel->levelNum!=LEVEL_CASTLE){
		stats_get_level_info(currLevel->levelNum,0,&time,&stars,&coins,&resets,&dist);
	} else {
		stats_get_level_info(currLevel->levelNum,currLevel->areaIdx,&time,&stars,&coins,&resets,&dist);
	}
	s32 yPos = SCREEN_HEIGHT-64;
	render_shadow_text_string_at(100,yPos,"Playtime");
	set_timer_text_rough(time);
	
	width = get_text_width(gTimerText);
	render_shadow_text_string_at(276-width,yPos,gTimerText);
	
	yPos -= 24;
	
	render_shadow_text_string_at(100,yPos,"Resets");
	sprintf(text,"%u",resets);
	width = get_text_width(text);
	render_shadow_text_string_at(276-width,yPos,text);
	yPos -= 24;
	
	render_shadow_text_string_at(100,yPos,"Distance");
	dist /= 100.0;
	if (dist>=10000.0){
		dist /= 1000.0;
		sprintf(text,"%.2fkm",dist);
	} else {
		sprintf(text,"%.2fm",dist);
	}
	width = get_text_width(text);
	render_shadow_text_string_at(276-width,yPos,text);
	yPos -= 24;
	
	s32 starsW = get_num_width(stars);
	s32 coinsW = get_num_width(coins);
	s32 justWidth = (starsW>coinsW) ? starsW : coinsW;
	char* jText;
	
	s32 xPos = 240-(justWidth+1)*12;
	print_text(xPos, yPos, "-"); // star
	print_text(xPos + 17, yPos, "*"); // x
	
	jText = justify_text(text,justWidth-starsW);
	sprintf(jText," %u",stars);
	print_text(xPos + 17 + 15, yPos, text);
	
	yPos -= 24;
	
	print_text(xPos, yPos, "+"); // coin
	print_text(xPos + 17, yPos, "*"); // x
	jText = justify_text(text,justWidth-coinsW);
	sprintf(jText," %u",coins);
	print_text(xPos + 17 + 15, yPos, text);
	render_text_labels();
	
	if (gPlayer1Controller->buttonPressed & (B_BUTTON|START_BUTTON)){
		sInLevelStats = FALSE;
	}
}

#define LEVEL_SPACING 24
static u8 render_level_select(void){
	if (sInLevelStats){
		render_practice_level_stats();
		return FALSE;
	}
	
	u8 close = FALSE;
	
	gCurrTextScale = 1.25f;
	set_text_color(255,255,255,255);
	render_shadow_text_string_at(12,SCREEN_HEIGHT-32,"Level Select");
	
	if (gPlayer1Controller->buttonPressed & D_JPAD){
		gPracticeStageSelect += 2;
		gPracticeStageSelect %= PRACTICE_LEVEL_COUNT;
	}
	
	if (gPlayer1Controller->buttonPressed & U_JPAD){
		gPracticeStageSelect -= 2;
		gPracticeStageSelect += PRACTICE_LEVEL_COUNT;
		gPracticeStageSelect %= PRACTICE_LEVEL_COUNT;
	}
	
	if (gPlayer1Controller->buttonPressed & R_JPAD){
		gPracticeStageSelect += 1;
		gPracticeStageSelect %= PRACTICE_LEVEL_COUNT;
	}
	
	if (gPlayer1Controller->buttonPressed & L_JPAD){
		gPracticeStageSelect -= 1;
		gPracticeStageSelect += PRACTICE_LEVEL_COUNT;
		gPracticeStageSelect %= PRACTICE_LEVEL_COUNT;
	}
	
	s32 ySelect = gPracticeStageSelect/2;
	s32 yVal = SCREEN_HEIGHT-40-(ySelect-sYScroll)*LEVEL_SPACING;
	if (yVal<40){
		sYScroll += (40-yVal)/LEVEL_SPACING;
	}
	
	if (yVal>SCREEN_HEIGHT-40-40){
		sYScroll += ((SCREEN_HEIGHT-40-40)-yVal)/LEVEL_SPACING;
	}
	if (sYScroll<0)
		sYScroll = 0;
	
	gCurrTextScale = 1.25f;
	u32 x;
	for (s32 i=0;i<(s32)PRACTICE_LEVEL_COUNT;++i){
		set_text_color(255,255,255,255);
		if (i==gPracticeStageSelect)
			set_text_color(255,0,0,255);
		
		x = i%2;
		x *= 100;
		x -= 50;
		render_shadow_text_string_at(175+x,SCREEN_HEIGHT-40-(i/2-sYScroll)*LEVEL_SPACING,sPracticeDests[i].name);
	}
	
	const PracticeDest* curr = &sPracticeDests[gPracticeStageSelect];
	const PracticeAltDest* alts = &sPracticeAltDests[gPracticeStageSelect];
	
	if (gPlayer1Controller->buttonPressed & A_BUTTON){
		close = TRUE;
		if (gCurrLevelNum!=1){
			gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
			gPracticeDest.levelNum = curr->levelNum;
			gPracticeDest.areaIdx = curr->areaIdx;
			gPracticeDest.nodeId = curr->nodeId;
			if ((gPracticeDest.levelNum==LEVEL_CASTLE&&gPracticeDest.areaIdx==1&&gPracticeDest.nodeId==0)||
				(gPracticeDest.levelNum==LEVEL_CASTLE_COURTYARD)){
				gPracticeDest.arg = 6;
			} else {
				gPracticeDest.arg = 0;
			}
			
			practice_choose_level();
			if (gPracticeDest.levelNum==LEVEL_WDW)
				sSetWDWHeight = TRUE;
		}
	} else if (gPlayer1Controller->buttonPressed & U_CBUTTONS){
		close = TRUE;
		if (gCurrLevelNum!=1&&alts->levelNumA!=0){
			gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
			gPracticeDest.levelNum = alts->levelNumA;
			gPracticeDest.areaIdx = alts->areaIdxA;
			gPracticeDest.nodeId = alts->nodeIdA;

			gLastLevelNum = curr->levelNum;
			gCurrLevelArea = curr->levelNum*16+1;
			practice_choose_level();
			gLastCompletedCourseNum = gLevelToCourseNumTable[curr->levelNum-1];
			gLastCompletedStarNum = 1;
		}
	} else if (gPlayer1Controller->buttonPressed & R_CBUTTONS){
		close = TRUE;
		if (gCurrLevelNum!=1&&alts->levelNumB!=0){
			gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
			gPracticeDest.levelNum = alts->levelNumB;
			gPracticeDest.areaIdx = alts->areaIdxB;
			gPracticeDest.nodeId = alts->nodeIdB;
			
			//gPrevLevel = curr->levelNum;
			gLastLevelNum = curr->levelNum;
			gCurrLevelArea = curr->levelNum*16+1;
			practice_choose_level();
		}
	} else if (gPlayer1Controller->buttonPressed & START_BUTTON){
		sInLevelStats = TRUE;
	}
	
	if (gPlayer1Controller->buttonPressed & B_BUTTON)
		close = TRUE;
	return close;
}

enum AngleDisplayType {
	ANGLE_DISPLAY_U16,
	ANGLE_DISPLAY_HEX,
	ANGLE_DISPLAY_DEGREES,
	ANGLE_DISPLAY_COUNT
};

typedef enum {
	PRACTICE_PAGE_SETTINGS = 0,
	PRACTICE_PAGE_INTERACT
} PracticePageType;

typedef struct {
	const char* name;
	PracticePageType type;
	s32 index;
	s32 size;
	PracticeSetting* settings;
} PracticePage;

static const char* sBoolValues[] = {
	"Off",
	"On",
	NULL
};

static const char* sShowBoolValues[] = {
	"Hide",
	"Show",
	NULL
};

static const char* sPracticeTypeValues[PRACTICE_TYPE_COUNT+1] = {
	"Star grab",
	"XCam",
	"Stage RTA",
	"Game RTA",
	NULL
};

static const char* sAngleDisplayValues[ANGLE_DISPLAY_COUNT+1] = {
	"u16",
	"Hex",
	"Degrees",
	NULL
};

typedef void ButtonFunc(PracticeSetting*);
typedef const char* InfoFunc(PracticeSetting*);

void toggle_rta_mode(PracticeSetting* setting){
	if (gCurrLevelNum==1)
		return;
	
	if (gRTAMode){
		setting->name = "Enable RTA mode";
		gRTAMode = FALSE;
	} else {
		setting->name = "Disable RTA mode";
		enable_rta_mode();
	}
}

static PracticeSetting sPracticeGameSettings[] = {
	{
		"Enable RTA mode",
		NULL,
		PRACTICE_BUTTON,
		0,
		toggle_rta_mode
	},
	{
		"Practice type",
		(void**)sPracticeTypeValues,
		PRACTICE_OPTION_ENUM,
		0,
		&configPracticeType
	},
	{
		"Stage star target",
		NULL,
		PRACTICE_OPTION_UINT,
		0,
		&sStageRTAStarsTarget
	},
	{
		"Nonstop",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configNonstop
	},
	{
		"Intro skip",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configSkipIntro
	},
	{
		"Ghost enabled",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configUseGhost
	},
	{
		"Ghost opacity",
		NULL,
		PRACTICE_OPTION_UINT,
		0,
		&configGhostOpacity
	},
	{
		"Ghost dist fade",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configGhostDistanceFade
	},
	{
		"Reset music",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configResetMusic
	},
	{
		"Disable music",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configDisableMusic
	},
	{
		"Intro skip timing",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configIntroSkipTiming
	},
	{
		"File select start",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configFileSelectStart
	},
	{
		"Fix invisible walls",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configNoInvisibleWalls
	},
	{
		"Disable A button",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configDisableA
	},
	{
		"Yellow stars",
		(void**)sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configYellowStars
	}
};
#define PRACTICE_GAME_SETTINGS_COUNT ARRAY_LEN(sPracticeGameSettings)

static NumberRange sActNumberRange = {1,7};
static NumberRange sU16NumberRange = {0,65535};
static NumberRange sS8NumberRange = {-128,127};
static NumberRange sSwimStrengthNumberRange = {16,28};
static const char* sCamModesEnumValues[] = {
	"Mario",
	"Fixed",
	NULL
};

static PracticeSetting sPracticeResetSettings[] = {
	{
		"Stage act",
		(void**)&sActNumberRange,
		PRACTICE_OPTION_TOGGLE_UINT,
		0,
		&sResetActNum
	},
	{
		"Global timer",
		NULL,
		PRACTICE_OPTION_TOGGLE_BIG_UINT,
		0,
		&sResetGlobalTimer
	},
	{
		"RNG",
		(void**)&sU16NumberRange,
		PRACTICE_OPTION_TOGGLE_BIG_UINT,
		0,
		&sResetRNG
	},
	{
		"Lives",
		(void**)&sS8NumberRange,
		PRACTICE_OPTION_TOGGLE_INT,
		0,
		&sResetLives
	},
	{
		"Swim strength",
		(void**)&sSwimStrengthNumberRange,
		PRACTICE_OPTION_TOGGLE_UINT,
		0,
		&sResetSwimStrength
	},
	{
		"Camera mode",
		(void**)sCamModesEnumValues,
		PRACTICE_OPTION_TOGGLE_ENUM,
		0,
		&sResetCamMode
	},
	{
		"Save file",
		NULL,
		PRACTICE_OPTION_TOGGLE_SAVE,
		0,
		&sResetSave
	},
	{
		"Copy save from current",
		NULL,
		PRACTICE_BUTTON,
		0,
		copy_save_file_to_reset_var
	},
	{
		"Apply to replay",
		NULL,
		PRACTICE_BUTTON,
		0,
		apply_reset_vars_to_replay
	}
};
#define PRACTICE_RESET_SETTINGS_COUNT ARRAY_LEN(sPracticeResetSettings)

static PracticeSetting sPracticeReplaySettings[] = {
	{
		"Finished replay",
		NULL,
		PRACTICE_INFO,
		0,
		replay_length_info
	},
	{
		"Play",
		NULL,
		PRACTICE_BUTTON,
		0,
		button_play_current_replay
	},
	{
		"Export",
		NULL,
		PRACTICE_BUTTON,
		0,
		button_export_current_replay
	},
	/*{
		"Reexport all replays",
		NULL,
		PRACTICE_BUTTON,
		0,
		button_reexport_all_replays
	},*/
	{
		"",
		NULL,
		PRACTICE_SPACER,
		0,
		NULL
	},
	{
		"Recording replay",
		NULL,
		PRACTICE_INFO,
		1,
		replay_length_info
	},
	{
		"Stop recording",
		NULL,
		PRACTICE_BUTTON,
		0,
		button_stop_recording
	},
	{
		"",
		NULL,
		PRACTICE_SPACER,
		0,
		NULL
	},
	{
		"Replay history",
		NULL,
		PRACTICE_SPACER,
		0,
		NULL
	},
	{
		"",
		(void**)archive_length_info,
		PRACTICE_INFO_BUTTON,
		0,
		button_archive_load_replay
	},
	{
		"",
		(void**)archive_length_info,
		PRACTICE_INFO_BUTTON,
		1,
		button_archive_load_replay
	},
	{
		"",
		(void**)archive_length_info,
		PRACTICE_INFO_BUTTON,
		2,
		button_archive_load_replay
	},
	{
		"",
		(void**)archive_length_info,
		PRACTICE_INFO_BUTTON,
		3,
		button_archive_load_replay
	},
	{
		"",
		(void**)archive_length_info,
		PRACTICE_INFO_BUTTON,
		4,
		button_archive_load_replay
	},
	{
		"",
		(void**)archive_length_info,
		PRACTICE_INFO_BUTTON,
		5,
		button_archive_load_replay
	},
	{
		"",
		(void**)archive_length_info,
		PRACTICE_INFO_BUTTON,
		6,
		button_archive_load_replay
	},
	{
		"",
		(void**)archive_length_info,
		PRACTICE_INFO_BUTTON,
		7,
		button_archive_load_replay
	},
	{
		"",
		(void**)archive_length_info,
		PRACTICE_INFO_BUTTON,
		8,
		button_archive_load_replay
	},
	{
		"",
		(void**)archive_length_info,
		PRACTICE_INFO_BUTTON,
		9,
		button_archive_load_replay
	}
};
#define PRACTICE_REPLAY_SETTINGS_COUNT ARRAY_LEN(sPracticeReplaySettings)

static PracticeSetting sPracticeStatsSettings[] = {
	{
		"Total playtime",
		NULL,
		PRACTICE_INFO,
		0,
		practice_stats_total_playtime
	},
	{
		"Total resets",
		NULL,
		PRACTICE_INFO,
		0,
		practice_stats_total_resets
	}
};
#define PRACTICE_STATS_SETTINGS_COUNT ARRAY_LEN(sPracticeStatsSettings)

static PracticeSetting sPracticeHUDSettings[] = {
	{
		"Show HUD",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configHUD
	},
	{
		"Timer",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowTimer
	},
	{
		"Controller display",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowControls
	},
	{
		"Vel",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowVel
	},
	{
		"Sliding speed",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowSlidingVel
	},
	{
		"Twirl yaw",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowTwirlYaw
	},
	{
		"Pos",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowPos
	},
	{
		"Angle",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowAngle
	},
	{
		"Angle units",
		(void**)sAngleDisplayValues,
		PRACTICE_OPTION_ENUM,
		0,
		&configAngleDisplayType
	},
	{
		"Max height",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowMaxHeight
	},
	{
		"HOLP",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowHOLP
	},
	
	{
		"Wallkick frame",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowWallkickFrame
	},
	{
		"Wallkick angle",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowWallkickAngle
	},
	{
		"Bowser info",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowBowserInfo
	},
	{
		"RNG info",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowRNGInfo
	},
	{
		"Angle efficiency",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowEfficiency
	},
	{
		"Swim strength",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowSwimStrength
	},
	{
		"Swim trainer",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowSwimTrainer
	},
	{
		"A press counter",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowAPressCount
	},
	{
		"Global timer",
		(void**)sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowGlobalTimer
	}
};
#define PRACTICE_HUD_SETTINGS_COUNT ARRAY_LEN(sPracticeHUDSettings)

/*
TTC_SPEED_SLOW    0
TTC_SPEED_FAST    1
TTC_SPEED_RANDOM  2
TTC_SPEED_STOPPED 3
*/

/*
wdwWaterHeight = 31;
wdwWaterHeight = 1024;
wdwWaterHeight = 2816;  

1000
1500
2000      
*/

static const char* sTTCSpeedValues[] = {
	"Slow",
	"Fast",
	"Random",
	"Stop",
	NULL
};

static const char* sWDWWaterValues[] = {
	"Low",
	"Medium",
	"High",
	NULL
};

static const char* sDDDSubValues[] = {
	"Default",
	"Always",
	"Never",
	NULL
};

static PracticeSetting sPracticeStageSettings[] = {
	{
		"TTC Speed",
		(void**)sTTCSpeedValues,
		PRACTICE_OPTION_ENUM,
		0,
		&gTTCSpeedSetting
	},
	{
		"WDW Water",
		(void**)sWDWWaterValues,
		PRACTICE_OPTION_ENUM,
		0,
		&gPracticeWDWHeight
	},
	{
		"DDD Sub",
		(void**)sDDDSubValues,
		PRACTICE_OPTION_ENUM,
		0,
		&gPracticeSubStatus
	},
	{
		"Stage Text",
		(void**)sDDDSubValues,
		PRACTICE_OPTION_ENUM,
		0,
		&configStageText
	}
};
#define PRACTICE_STAGE_SETTINGS_COUNT ARRAY_LEN(sPracticeStageSettings)

static PracticePage sPracticePages[] = {
	{
		"Game",
		PRACTICE_PAGE_SETTINGS,
		0,
		PRACTICE_GAME_SETTINGS_COUNT,
		sPracticeGameSettings
	},
	{
		"Replay",
		PRACTICE_PAGE_SETTINGS,
		0,
		PRACTICE_REPLAY_SETTINGS_COUNT,
		sPracticeReplaySettings
	},
	{
		"Reset",
		PRACTICE_PAGE_SETTINGS,
		0,
		PRACTICE_RESET_SETTINGS_COUNT,
		sPracticeResetSettings
	},
	{
		"HUD",
		PRACTICE_PAGE_SETTINGS,
		0,
		PRACTICE_HUD_SETTINGS_COUNT,
		sPracticeHUDSettings
	},
	{
		"Stage",
		PRACTICE_PAGE_SETTINGS,
		0,
		PRACTICE_STAGE_SETTINGS_COUNT,
		sPracticeStageSettings
	},
	{
		"Stats",
		PRACTICE_PAGE_SETTINGS,
		0,
		PRACTICE_STATS_SETTINGS_COUNT,
		sPracticeStatsSettings
	}
};
#define PRACTICE_PAGE_COUNT ARRAY_LEN(sPracticePages)

static s32 sPracticePageSelect = 0;
static u8 sInPracticePage = 1;

static void wrap_index(PracticeSetting* setting){
	s32 l = 0;
	const char** it = (const char**)setting->values;
	while (*it!=NULL){
		++it;
		++l;
	}
	
	setting->index += l;
	setting->index %= l;
}

static void load_setting(PracticeSetting* setting){
	if (setting->var==NULL) return;
	
	switch (setting->type){
		case PRACTICE_OPTION_BOOL:
			if (*((u8*)setting->var)!=0){
				setting->index = 1;
			} else {
				setting->index = 0;
			}
			break;
		case PRACTICE_OPTION_ENUM:
			setting->index = *((u32*)setting->var);
			break;
		default:
			break;
	}
}

void load_all_settings(void){
	for (u32 i=0;i<PRACTICE_PAGE_COUNT;++i){
		PracticePage* page = &sPracticePages[i];
		for (s32 j=0;j<page->size;++j){
			load_setting(&page->settings[j]);
		}
	}
}

static u8 special_apply_setting(PracticeSetting* setting){
	if (setting->var==&configPracticeType){
	//if (strncmp(setting->name,PRACTICE_TYPE_SETTING_NAME,sizeof(PRACTICE_TYPE_SETTING_NAME))==0){
		set_practice_type(setting->index);
		load_all_settings();
		return TRUE;
	}
	
	return FALSE;
}

static void apply_setting(PracticeSetting* setting){
	if (setting->var==NULL) return;
	
	switch (setting->type){
		case PRACTICE_OPTION_BOOL:
			u8* var = (u8*)setting->var;
			if (setting->index==0)
				*var = FALSE;
			else if (setting->index==1)
				*var = TRUE;
			break;
		case PRACTICE_OPTION_ENUM:
			u32* val = (u32*)setting->var;
			*val = setting->index;
			break;
		default:
			break;
	}
	
	special_apply_setting(setting);
}

#define PRACTICE_OPTION_HEIGHT 24

static s32 sSettingsScroll = 0;

static void calculate_settings_scroll(void){
	const PracticePage* page = &sPracticePages[sPracticePageSelect];
	
	s32 yVal = SCREEN_HEIGHT-32-(page->index-sSettingsScroll)*PRACTICE_OPTION_HEIGHT;
	if (yVal<40){
		sSettingsScroll += (40-yVal)/PRACTICE_OPTION_HEIGHT;
	}
	
	if (yVal>SCREEN_HEIGHT-32-40){
		sSettingsScroll += ((SCREEN_HEIGHT-32-40)-yVal)/PRACTICE_OPTION_HEIGHT;
	}
	if (sSettingsScroll<0){
		sSettingsScroll = 0;
	}
}

static const char* get_replay_level_name(const Replay* replay){
	assert(replay->state.type==LEVEL_INIT);
	LevelInitState* state = (LevelInitState*)replay->state.levelState;
	u32 levelNum = state->loc.levelNum;
	u32 areaIndex = state->loc.areaIdx;
	const char* best = "Unknown";
	u32 matchType = 0;
	
	for (u32 i=0;i<PRACTICE_LEVEL_COUNT;++i){
		const PracticeDest* pd = &sPracticeDests[i];
		if (pd->levelNum==levelNum&&pd->areaIdx==areaIndex){
			matchType = 2;
			best = pd->name;
		} else if (pd->levelNum==levelNum&&matchType<2){
			matchType = 1;
			best = pd->name;
		}
	}
	return best;
}

static void set_archive_names(void){
	u32 base = PRACTICE_REPLAY_SETTINGS_COUNT-10;
	for (u32 i=0;i<10;++i){
		PracticeSetting* archiveS = &sPracticeReplaySettings[base+i];
		if (!gReplayHistory[i]){
			archiveS->name = "";
			continue;
		}
		if (gReplayHistory[i]->state.type==GAME_INIT){
			archiveS->name = "Game";
		} else if (gReplayHistory[i]->state.type==LEVEL_INIT){
			archiveS->name = get_replay_level_name(gReplayHistory[i]);
		} else {
			archiveS->name = "";
		}
	}
}

static void practice_page_move_down(PracticePage* page){
	// skip past spacers
	do {
		page->index += 1;
		page->index %= page->size;
	} while (page->settings[page->index].type == PRACTICE_SPACER);
}

static void practice_page_move_up(PracticePage* page){
	// skip past spacers
	do {
		page->index -= 1;
		page->index += page->size;
		page->index %= page->size;
	} while (page->settings[page->index].type == PRACTICE_SPACER);
}

static void practice_setting_get_bounds_s32(const PracticeSetting* setting,s32* min,s32* max){
	*max = INT32_MAX;
	*min = INT32_MIN;
	if (setting->values){
		NumberRange* range = (NumberRange*)setting->values;
		if (*max>range->max) *max = range->max;
		if (*min<range->min) *min = range->min;
	}
}

static void practice_setting_get_bounds_u32(const PracticeSetting* setting,u32* min,u32* max){
	*max = UINT32_MAX;
	*min = 0;
	if (setting->values){
		NumberRange* range = (NumberRange*)setting->values;
		if (*max>(u32)range->max) *max = (u32)range->max;
		if (*min<(u32)range->min) *min = (u32)range->min;
	}
}

static u32 sDigitPos = 0;

static void handle_big_uint_setting(PracticeSetting* setting){
	u32 min,max;
	u32 maxDigit = 0;
	practice_setting_get_bounds_u32(setting,&min,&max);
	
	for (u32 i=max;i>9;i/=10){
		++maxDigit;
	}
	
	PracticeResetVar* resetVar = (PracticeResetVar*)setting->var;
	if (gPlayer1Controller->buttonPressed & A_BUTTON){
		resetVar->enabled = !resetVar->enabled;
	} else if (gPlayer1Controller->buttonPressed & (L_CBUTTONS | R_CBUTTONS | U_CBUTTONS | D_CBUTTONS)){
		resetVar->enabled = TRUE;
	}
	
	if (sDigitPos>maxDigit)
		sDigitPos = maxDigit;
	
	if (sDigitPos<maxDigit && (gPlayer1Controller->buttonPressed & L_CBUTTONS)){
		++sDigitPos;
	}
	
	if (sDigitPos>0 && (gPlayer1Controller->buttonPressed & R_CBUTTONS)){
		--sDigitPos;
	}
	
	u32 powerOfTen = 1;
	for (u32 i=0;i<sDigitPos;++i){
		powerOfTen *= 10;
	}
	
	if (resetVar->varU32 < max && (gPlayer1Controller->buttonPressed & U_CBUTTONS)){
		if (resetVar->varU32+powerOfTen>max||resetVar->varU32+powerOfTen<resetVar->varU32)
			resetVar->varU32 = max;
		else
			resetVar->varU32 += powerOfTen;
	}
	if (resetVar->varU32 > min && (gPlayer1Controller->buttonPressed & D_CBUTTONS)){
		if (resetVar->varU32<powerOfTen||resetVar->varU32-powerOfTen<min)
			resetVar->varU32 = min;
		else
			resetVar->varU32 -= powerOfTen;
	}
}

static s32 count_bits(u32 val){
	s32 c = 0;
	while (val){
		if (val&1) ++c;
		val >>= 1;
	}
	return c;
}

static s32 get_save_file_star_count(const struct SaveFile* file){
	s32 starCount = 0;
	starCount += count_bits((file->flags>>24)&0x1F);
	
	for (s32 i=0;i<COURSE_COUNT;++i){
		starCount += count_bits(file->courseStars[i]&0x7F);
	}
	return starCount;
}

static u32 count_option_values(void** values){
	u32 i = 0;
	while (values[i]){
		++i;
	}
	return i;
}

static u8 render_practice_settings(void){
	if (gPlayer1Controller->buttonPressed & R_JPAD){
		sInPracticePage = 1;
	}
	
	if (gPlayer1Controller->buttonPressed & L_JPAD){
		sInPracticePage = 0;
	}
	
	set_archive_names();
	set_text_color(255,255,255,255);
	gCurrTextScale = 1.25f;
	for (s32 i=0;i<(s32)PRACTICE_PAGE_COUNT;++i){
		PracticePage* page = &sPracticePages[i];
		if (sInPracticePage){
			if (i==sPracticePageSelect)
				set_text_color(255,255,255,255);
			else
				set_text_color(168,168,168,255);
		} else {
			if (i==sPracticePageSelect)
				set_text_color(255,0,0,255);
			else
				set_text_color(255,255,255,255);
		}
		render_shadow_text_string_at(12,SCREEN_HEIGHT-32-32*i,page->name);
	}
	
	if (!sInPracticePage){
		if (gPlayer1Controller->buttonPressed & D_JPAD){
			sPracticePageSelect += 1;
			sPracticePageSelect %= PRACTICE_PAGE_COUNT;
			calculate_settings_scroll();
		}
		
		if (gPlayer1Controller->buttonPressed & U_JPAD){
			sPracticePageSelect -= 1;
			sPracticePageSelect += PRACTICE_PAGE_COUNT;
			sPracticePageSelect %= PRACTICE_PAGE_COUNT;
			calculate_settings_scroll();
		}
	}
	
	PracticePage* currPage = &sPracticePages[sPracticePageSelect];
	if (sInPracticePage){
		if (gPlayer1Controller->buttonPressed & D_JPAD){
			practice_page_move_down(currPage);
			calculate_settings_scroll();
		}
		
		if (gPlayer1Controller->buttonPressed & U_JPAD){
			practice_page_move_up(currPage);
			calculate_settings_scroll();
		}
		
		PracticeSetting* selected = &currPage->settings[currPage->index];
		
		if (selected->type==PRACTICE_OPTION_BOOL||
			selected->type==PRACTICE_OPTION_ENUM){
			if (gPlayer1Controller->buttonPressed & R_CBUTTONS){
				selected->index += 1;
				wrap_index(selected);
				apply_setting(selected);
			}
			
			if (gPlayer1Controller->buttonPressed & L_CBUTTONS){
				selected->index -= 1;
				wrap_index(selected);
				apply_setting(selected);
			}
		} else if (selected->type==PRACTICE_OPTION_INT){
			s32 min,max;
			practice_setting_get_bounds_s32(selected,&min,&max);
			s32 value = *(s32*)selected->var;
			
			if (value<max && (gPlayer1Controller->buttonPressed & R_CBUTTONS)){
				++(*(s32*)selected->var);
			}
			
			if (value>min && (gPlayer1Controller->buttonPressed & L_CBUTTONS)){
				--(*(s32*)selected->var);
			}
		} else if (selected->type==PRACTICE_OPTION_UINT){
			u32 min,max;
			practice_setting_get_bounds_u32(selected,&min,&max);
			u32 value = *(u32*)selected->var;
			
			if (value<max && (gPlayer1Controller->buttonPressed & R_CBUTTONS)){
				++(*(u32*)selected->var);
			}
			
			if (value>min && (gPlayer1Controller->buttonPressed & L_CBUTTONS)){
				--(*(u32*)selected->var);
			}
		} else if (selected->type==PRACTICE_OPTION_TOGGLE_INT){
			s32 min,max;
			practice_setting_get_bounds_s32(selected,&min,&max);
			
			PracticeResetVar* resetVar = (PracticeResetVar*)selected->var;
			if (gPlayer1Controller->buttonPressed & A_BUTTON){
				resetVar->enabled = !resetVar->enabled;
			} else if (gPlayer1Controller->buttonPressed & (L_CBUTTONS | R_CBUTTONS)){
				resetVar->enabled = TRUE;
			}
			
			if (resetVar->varS32<max && (gPlayer1Controller->buttonPressed & R_CBUTTONS)){
				++resetVar->varS32;
			}
			
			if (resetVar->varS32>min && (gPlayer1Controller->buttonPressed & L_CBUTTONS)){
				--resetVar->varS32;
			}
		} else if (selected->type==PRACTICE_OPTION_TOGGLE_UINT){
			u32 min,max;
			practice_setting_get_bounds_u32(selected,&min,&max);
			
			PracticeResetVar* resetVar = (PracticeResetVar*)selected->var;
			if (gPlayer1Controller->buttonPressed & A_BUTTON){
				resetVar->enabled = !resetVar->enabled;
			} else if (gPlayer1Controller->buttonPressed & (L_CBUTTONS | R_CBUTTONS)){
				resetVar->enabled = TRUE;
			}
			
			if (resetVar->varU32<max && (gPlayer1Controller->buttonPressed & R_CBUTTONS)){
				++resetVar->varU32;
			}
			
			if (resetVar->varU32>min && (gPlayer1Controller->buttonPressed & L_CBUTTONS)){
				--resetVar->varU32;
			}
		} else if (selected->type==PRACTICE_OPTION_TOGGLE_ENUM){
			PracticeResetVar* resetVar = (PracticeResetVar*)selected->var;
			if (gPlayer1Controller->buttonPressed & A_BUTTON){
				resetVar->enabled = !resetVar->enabled;
			} else if (gPlayer1Controller->buttonPressed & (L_CBUTTONS | R_CBUTTONS)){
				resetVar->enabled = TRUE;
			}
			
			if (gPlayer1Controller->buttonPressed & R_CBUTTONS){
				if (resetVar->varU32==count_option_values(selected->values)-1)
					resetVar->varU32 = 0;
				else
					++resetVar->varU32;
			}
			if (gPlayer1Controller->buttonPressed & L_CBUTTONS){
				if (resetVar->varU32==0)
					resetVar->varU32 = count_option_values(selected->values)-1;
				else
					--resetVar->varU32;
			}
		} else if (selected->type==PRACTICE_OPTION_TOGGLE_BIG_UINT){
			handle_big_uint_setting(selected);
		} else if (selected->type==PRACTICE_OPTION_TOGGLE_SAVE){
			PracticeResetVar* resetVar = (PracticeResetVar*)selected->var;
			if (gPlayer1Controller->buttonPressed & A_BUTTON){
				resetVar->enabled = !resetVar->enabled;
			}
		} else if (selected->type==PRACTICE_BUTTON || selected->type==PRACTICE_INFO_BUTTON){
			if (gPlayer1Controller->buttonPressed & A_BUTTON){
				((ButtonFunc*)selected->var)(selected);
			}
		}
	}
	
	s32 nameX = 100;
	s32 opX = SCREEN_WIDTH-30;
	
	for (s32 i=0;i<currPage->size;++i){
		PracticeSetting* currSetting = &currPage->settings[i];
		
		if (sInPracticePage)
			set_text_color(255,255,255,255);
		else
			set_text_color(168,168,168,255);
		
		s32 y = SCREEN_HEIGHT-32-(i-sSettingsScroll)*PRACTICE_OPTION_HEIGHT;
		
		u8 highlighted = (sInPracticePage&&currPage->index==i);
		
		s32 textWidth;
		char intStorage[32];
		const char* result;
		switch (currSetting->type){
			case PRACTICE_SPACER:
				if (currSetting->name){
					render_shadow_text_string_at(nameX,y,currSetting->name);
				}
				break;
			case PRACTICE_INFO:
				render_shadow_text_string_at(nameX,y,currSetting->name);
				if (highlighted)
					set_text_color(255,230,0,255);
				
				if (currSetting->var){
					result = ((InfoFunc*)currSetting->var)(currSetting);
					textWidth = get_text_width(result);
					render_shadow_text_string_at(opX-textWidth,y,result);
				}
				break;
			case PRACTICE_BUTTON:
				if (highlighted)
					set_text_color(255,230,0,255);
				
				textWidth = get_text_width(currSetting->name);
				render_shadow_text_string_at(opX-textWidth,y,currSetting->name);
				break;
			case PRACTICE_INFO_BUTTON:
				if (currSetting->name){
					render_shadow_text_string_at(nameX,y,currSetting->name);
				}
				
				if (highlighted)
					set_text_color(255,230,0,255);
				
				if (currSetting->values){
					result = ((InfoFunc*)currSetting->values)(currSetting);
					textWidth = get_text_width(result);
					render_shadow_text_string_at(opX-textWidth,y,result);
				}
				break;
			case PRACTICE_OPTION_BOOL:
			case PRACTICE_OPTION_ENUM:
				render_shadow_text_string_at(nameX,y,currSetting->name);
				if (highlighted)
					set_text_color(255,0,0,255);
				
				result = ((const char**)currSetting->values)[currSetting->index];
				textWidth = get_text_width(result);
				render_shadow_text_string_at(opX-textWidth,y,result);
				break;
			
			case PRACTICE_OPTION_TOGGLE_INT:
			case PRACTICE_OPTION_TOGGLE_UINT:
			case PRACTICE_OPTION_TOGGLE_FLOAT:
				if (sInPracticePage){
					if (!((PracticeResetVar*)currSetting->var)->enabled)
						set_text_color(255,192,192,255);
					else
						set_text_color(192,255,192,255);
				} else {
					if (!((PracticeResetVar*)currSetting->var)->enabled)
						set_text_color(192,168,168,255);
					else
						set_text_color(168,192,168,255);
				}
				
				render_shadow_text_string_at(nameX,y,currSetting->name);
				if (highlighted)
					set_text_color(255,0,0,255);
				
				snprintf(intStorage,sizeof(intStorage),"%d",((PracticeResetVar*)currSetting->var)->varU32);
				textWidth = get_text_width(intStorage);
				render_shadow_text_string_at(opX-textWidth,y,intStorage);
				break;
			
			case PRACTICE_OPTION_TOGGLE_ENUM: {
				PracticeResetVar* var = (PracticeResetVar*)currSetting->var;
				if (sInPracticePage){
					if (!var->enabled)
						set_text_color(255,192,192,255);
					else
						set_text_color(192,255,192,255);
				} else {
					if (!var->enabled)
						set_text_color(192,168,168,255);
					else
						set_text_color(168,192,168,255);
				}
				
				render_shadow_text_string_at(nameX,y,currSetting->name);
				if (highlighted)
					set_text_color(255,0,0,255);
				
				const char* val = ((const char**)currSetting->values)[var->varU32];
				textWidth = get_text_width(val);
				render_shadow_text_string_at(opX-textWidth,y,val);
				break;
			}
			case PRACTICE_OPTION_TOGGLE_BIG_UINT:
				if (sInPracticePage){
					if (!((PracticeResetVar*)currSetting->var)->enabled)
						set_text_color(255,192,192,255);
					else
						set_text_color(192,255,192,255);
				} else {
					if (!((PracticeResetVar*)currSetting->var)->enabled)
						set_text_color(192,168,168,255);
					else
						set_text_color(168,192,168,255);
				}
				
				render_shadow_text_string_at(nameX,y,currSetting->name);
				if (highlighted)
					set_text_color(255,0,0,255);
				
				snprintf(intStorage,sizeof(intStorage),"%u",((PracticeResetVar*)currSetting->var)->varU32);
				textWidth = get_text_width(intStorage);
				render_shadow_text_string_at(opX-textWidth,y,intStorage);
				
				if (highlighted){
					u32 digWidth = (7*(sDigitPos+1))*5/4+1;
					gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
					render_hud_small_tex_lut(opX-digWidth,SCREEN_HEIGHT-y-4,main_hud_camera_lut[GLYPH_CAM_ARROW_UP]);
					gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
				}
				
				break;
			
			case PRACTICE_OPTION_TOGGLE_SAVE: {
				PracticeResetVar* var = (PracticeResetVar*)currSetting->var;
				if (sInPracticePage){
					if (!var->enabled)
						set_text_color(255,192,192,255);
					else
						set_text_color(192,255,192,255);
				} else {
					if (!var->enabled)
						set_text_color(192,168,168,255);
					else
						set_text_color(168,192,168,255);
				}
				
				render_shadow_text_string_at(nameX,y,currSetting->name);
				if (highlighted)
					set_text_color(255,0,0,255);
				
				struct SaveFile* resetSave = (struct SaveFile*)var->varPtr;
				s32 count = get_save_file_star_count(resetSave);
				snprintf(intStorage,sizeof(intStorage),"^*%d",count);
				textWidth = get_text_width(intStorage);
				render_shadow_text_string_at(opX-textWidth,y,intStorage);
				break;
			}
			case PRACTICE_OPTION_INT:
			case PRACTICE_OPTION_UINT:
				render_shadow_text_string_at(nameX,y,currSetting->name);
				if (highlighted)
					set_text_color(255,0,0,255);
				
				snprintf(intStorage,sizeof(intStorage),"%d",*(s32*)currSetting->var);
				textWidth = get_text_width(intStorage);
				render_shadow_text_string_at(opX-textWidth,y,intStorage);
				break;
		}
	}
	
	if (gPlayer1Controller->buttonPressed & B_BUTTON)
		return TRUE;
	
	return FALSE;
}

void render_practice_menu(void){
	create_dl_ortho_matrix();
	shade_screen();
	
	sDownPrimed = FALSE;
	sDownTriggered = FALSE;
	
	if (gPlayer1Controller->buttonPressed & L_TRIG){
		--gPracticeMenuPage;
		gPracticeMenuPage += PRACTICE_MENU_COUNT;
		gPracticeMenuPage %= PRACTICE_MENU_COUNT;
	}
	
	if (gPlayer1Controller->buttonPressed & R_TRIG){
		++gPracticeMenuPage;
		gPracticeMenuPage %= PRACTICE_MENU_COUNT;
	}
	
	u8 close = FALSE;
	switch (gPracticeMenuPage){
		case PRACTICE_LEVEL_SELECT:
			close = render_level_select();
			break;
		case PRACTICE_SETTINGS:
			close = render_practice_settings();
			break;
		case PRACTICE_REPLAYS:
			close = render_browser();
			break;
		case PRACTICE_SAVE_EDITOR:
			close = render_save_editor();
			break;
	}
	
	gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, BORDER_HEIGHT, SCREEN_WIDTH,
                      SCREEN_HEIGHT - BORDER_HEIGHT);
	
	if (close)
		update_save_file_vars();
	
	if (!gClosePracticeMenuNextFrame)
		gClosePracticeMenuNextFrame = close;
}

static void render_game_timer(void){
	create_dl_ortho_matrix();
	set_text_color(255,255,255,255);
	
	s32 val = gSectionTimer-1;
	if (val<0) val = 0;
	
	if (gSectionTimerResult!=NULL_SECTION_TIMER_RESULT)
		val = gSectionTimerResult;
	else if (sSectionTimerLockTime!=0){
		val = sSectionTimerLock;
		if (!gRenderPracticeMenu)
			--sSectionTimerLockTime;
	}
	
	set_timer_text(val);
	
	gCurrTextScale = 1.0f;
	render_shadow_text_string_at(GFX_DIMENSIONS_FROM_LEFT_EDGE(0)+20,SCREEN_HEIGHT-BORDER_HEIGHT-50,gTimerText);
}

static void render_controls(void){
	static char numBuf[8];
	static const s32 buttonSize = 6;
	static const s32 cButtonSize = 4;
	static const s32 buttonPad = 2;
	static const s32 stickSize = 16;
	static const s32 stickHeadSize = 4;
	u8 aAlpha = 64;
	u8 bAlpha = 64;
	u8 zAlpha = 64;
	u8 startAlpha = 64;
	u8 cLAlpha = 64;
	u8 cRAlpha = 64;
	u8 cUAlpha = 64;
	u8 cDAlpha = 64;
	u8 rAlpha = 64;
	InputFrame rawInput;
	if (!gCurrPlayingReplay){
		rawInput.buttons = gPlayer1Controller->buttonDown;
		rawInput.stickX = gPlayer1Controller->rawStickX;
		rawInput.stickY = gPlayer1Controller->rawStickY;
	} else {
		rawInput = get_current_inputs();
	}
	
	if (rawInput.buttons & A_BUTTON)
		aAlpha = 255;
	if (rawInput.buttons & B_BUTTON)
		bAlpha = 255;
	if (rawInput.buttons & Z_TRIG)
		zAlpha = 255;
	if (rawInput.buttons & START_BUTTON)
		startAlpha = 255;
	if (rawInput.buttons & L_CBUTTONS)
		cLAlpha = 255;
	if (rawInput.buttons & R_CBUTTONS)
		cRAlpha = 255;
	if (rawInput.buttons & U_CBUTTONS)
		cUAlpha = 255;
	if (rawInput.buttons & D_CBUTTONS)
		cDAlpha = 255;
	if (rawInput.buttons & R_TRIG)
		rAlpha = 255;
	
	s32 yPos = SCREEN_HEIGHT-10;
	s32 xPos = 32;
	shade_screen_rect(xPos+buttonSize/2,yPos-1,buttonSize,buttonSize,40,40,255,aAlpha);
	shade_screen_rect(xPos,yPos-buttonSize-3,buttonSize,buttonSize,40,255,40,bAlpha);
	shade_screen_rect(xPos+buttonSize/2+buttonPad+buttonSize,yPos-1,buttonSize,buttonSize,112,54,226,zAlpha);
	shade_screen_rect(xPos+buttonPad+buttonSize,yPos-buttonSize-3,buttonSize,buttonSize,224,40,40,startAlpha);
	
	s32 cbX = xPos+buttonPad+buttonSize+buttonPad+buttonSize+buttonPad+buttonSize;
	s32 cbY = yPos-2-buttonSize;
	shade_screen_rect(cbX,cbY,cButtonSize,cButtonSize,255,255,40,cUAlpha);
	shade_screen_rect(cbX,cbY+cButtonSize*2,cButtonSize,cButtonSize,255,255,40,cDAlpha);
	shade_screen_rect(cbX-cButtonSize,cbY+cButtonSize,cButtonSize,cButtonSize,255,255,40,cLAlpha);
	shade_screen_rect(cbX+cButtonSize,cbY+cButtonSize,cButtonSize,cButtonSize,255,255,40,cRAlpha);
	shade_screen_rect(cbX+1,cbY+cButtonSize+1,cButtonSize-2,cButtonSize-2,255,40,40,rAlpha);
	
	shade_screen_rect(xPos-buttonSize-stickSize,yPos-stickSize+buttonSize,stickSize,stickSize,80,80,80,64);
	s32 movableRange = stickSize/2-stickHeadSize/2;
	s32 xVal = rawInput.stickX;
	s32 yVal = rawInput.stickY;
	float stickX = xVal/80.0f;
	float stickY = yVal/-80.0f;
	if (stickX>1.0f) stickX = 1.0f;
	if (stickX<-1.0f) stickX = -1.0f;
	if (stickY>1.0f) stickY = 1.0f;
	if (stickY<-1.0f) stickY = -1.0f;
	
	s32 stickPosX = (stickX*movableRange+0.5f) + stickSize/2 - stickHeadSize/2;
	s32 stickPosY = (stickY*movableRange+0.5f) + stickSize/2 - stickHeadSize/2;
	
	shade_screen_rect(xPos-buttonSize-stickSize + stickPosX,yPos-stickSize+buttonSize + stickPosY,stickHeadSize,stickHeadSize,224,224,224,255);
	gCurrTextScale = 0.5f;
	snprintf(numBuf,sizeof(numBuf),"%d",yVal);
	s32 tw = get_text_width(numBuf);
	render_shadow_text_string_at(xPos-buttonSize-tw-1,SCREEN_HEIGHT-(yPos-stickSize+buttonSize),numBuf);
	snprintf(numBuf,sizeof(numBuf),"%d",xVal);
	tw = get_text_width(numBuf);
	render_shadow_text_string_at(xPos-buttonSize-tw-1,SCREEN_HEIGHT-(yPos-stickSize+buttonSize-7),numBuf);
}

static void render_test(void){
	//render_colored_sprite(main_hud_lut[GLYPH_STAR],84,100,255,255,255,255);
	//render_colored_sprite(main_hud_lut[GLYPH_STAR],100,100,0,0,0,255);
	//shade_screen_rect(50,50,25,25,255,0,0,128);
	/*create_dl_ortho_matrix();
	gDPSetFillColor(gDisplayListHead++,GPACK_RGBA5551(255,0,0,128)<<16 | 
										GPACK_RGBA5551(255,0,0,128));
	gDPFillRectangle(gDisplayListHead++,50,50,75,75);*/
	//draw_xlu_rect(50,50,200,200);
	
	/*u8* tex = segmented_to_virtual(texture_a_button);

    gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
    render_hud_tex_lut(100,100,tex);
	gSPDisplayList(gDisplayListHead++, dl_hud_img_end);*/
}

static void get_angle_text(char* str,s16 angle){
	float degrees = 0.0f;
	switch (configAngleDisplayType){
		case ANGLE_DISPLAY_U16:
			sprintf(str,"%5u",(u16)angle);
			break;
		case ANGLE_DISPLAY_HEX:
			sprintf(str,"%04X",(u16)angle);
			break;
		case ANGLE_DISPLAY_DEGREES:
			degrees = ((u16)angle/65536.0f)*360.0f;
			sprintf(str,"%3.2fdeg",degrees);
			break;
	}
}

#define INFO_SPACING 4
extern s16 sSwimStrength;

#define CHEAT_HUD_SET() \
	if (gCurrRecordingReplay){ \
		gCurrRecordingReplay->flags |= REPLAY_FLAG_CHEAT_HUD;\
	}

static void render_mario_info(void){
	char coord[32];
	
	set_text_color(255,255,255,255);
	
	s32 xPos = GFX_DIMENSIONS_FROM_LEFT_EDGE(0)+8;
	s32 yPos = 4;
	if (configShowControls)
		yPos += 32;
	
	if (configShowGlobalTimer){
		gCurrTextScale = 0.75f;
		sprintf(coord,"%d",gGlobalTimer);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10+INFO_SPACING;
	}
	if (configShowPos||configShowAngle){
		gCurrTextScale = 0.75f;
		CHEAT_HUD_SET()
		if (configShowPos){
			sprintf(coord,"Z % 9.3f",gMarioState->pos[2]);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 10;
			sprintf(coord,"Y % 9.3f",gMarioState->pos[1]);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 10;
			sprintf(coord,"X % 9.3f",gMarioState->pos[0]);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 10;
		}
		if (configShowAngle){
			sprintf(coord,"A ");
			get_angle_text(&coord[2],gMarioState->faceAngle[1]);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 10;
		}
		
		yPos += INFO_SPACING;
	}
	if (configShowVel){
		gCurrTextScale = 1.0f;
		CHEAT_HUD_SET()
		
		sprintf(coord,"VS % 7.1f",gMarioState->vel[1]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 16;
		sprintf(coord,"FS % 7.1f",gMarioState->forwardVel);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 16;
		yPos += INFO_SPACING;
	}
	if (configShowSlidingVel){
		gCurrTextScale = 0.75f;
		CHEAT_HUD_SET()
		
		sprintf(coord,"SSZ % 7.1f",gMarioState->slideVelZ);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"SSX % 7.1f",gMarioState->slideVelX);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	if (configShowTwirlYaw){
		gCurrTextScale = 0.75f;
		CHEAT_HUD_SET()
		
		sprintf(coord,"TY ");
		get_angle_text(&coord[3],gMarioState->twirlYaw);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	if (configShowMaxHeight){
		gCurrTextScale = 0.8f;
		CHEAT_HUD_SET()
		sprintf(coord,"Max Y % 8.2f",gHeightLock);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	if (configShowHOLP){
		const struct MarioBodyState* bodyState = gMarioState->marioBodyState;
		Vec3f holp;
		CHEAT_HUD_SET()
		if (bodyState==NULL){
			holp[0] = 0.0f;
			holp[1] = 0.0f;
			holp[2] = 0.0f;
		} else {
			memcpy(holp,bodyState->heldObjLastPosition,sizeof(float)*3);
		}
		gCurrTextScale = 0.75f;
		sprintf(coord,"Z % 8.2f",holp[2]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"Y % 8.2f",holp[1]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"X % 8.2f",holp[0]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"HOLP");
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	
	if (configShowRNGInfo){
		gCurrTextScale = 0.75f;
		CHEAT_HUD_SET()
		sprintf(coord,"RNG %d",gRandomSeed16);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"Idx %d",gRandomCalls);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	
	if (configShowEfficiency){
		gCurrTextScale = 0.8f;
		f32 stickMag = 0.0f;
		s16 yaw = 0;
		if (gMarioState->controller)
			stickMag = gMarioState->controller->stickMag/64.0f;
		
		if (stickMag>0.01f&&gCamera){
			yaw = atan2s(-gMarioState->controller->stickY, gMarioState->controller->stickX) + gCamera->yaw;
		}
		s16 diff = gMarioState->faceAngle[1]-yaw;
		f32 val = coss(diff)*stickMag*stickMag*100.0f;
		if (val==-0.0f) val = 0.0f;
		sprintf(coord,"Eff %3.1f%%",val);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	
	if (configShowSwimStrength){
		gCurrTextScale = 0.8f;
		CHEAT_HUD_SET()
		sprintf(coord,"Ss %d",sSwimStrength);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	
	if (configShowSwimTrainer){
		CHEAT_HUD_SET()
		if ((gMarioState->action & ACT_GROUP_MASK) != ACT_GROUP_SUBMERGED){
			gSwimInfo = SWIM_PRACTICE_NONE;
		} else {
			switch (gSwimInfo){
				case SWIM_PRACTICE_GOOD:
					sprintf(coord,"Swim: GOOD %df",gSwimPressFrame);
					break;
				case SWIM_PRACTICE_TOO_LATE:
					sprintf(coord,"Swim: LATE %df",gSwimPressFrame);
					break;
				case SWIM_PRACTICE_TOO_EARLY:
					sprintf(coord,"Swim: EARLY %df",gSwimPressFrame);
					break;
				case SWIM_PRACTICE_HELD_TOO_LONG:
					sprintf(coord,"Swim: HELD TOO LONG");
					break;
				case SWIM_PRACTICE_HELD_TOO_SHORT:
					sprintf(coord,"Swim: HELD TOO SHORT");
					break;
				default:
					break;
			}
		}
		if (gSwimInfo!=SWIM_PRACTICE_NONE){
			gCurrTextScale = 0.8f;
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 10;
			yPos += INFO_SPACING;
		}
	}
	
	if (configShowAPressCount){
		gCurrTextScale = 0.8f;
		sprintf(coord,"Ax%d",gAPressCounter);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	
	if (configShowWallkickAngle||configShowWallkickFrame){
		yPos += INFO_SPACING;
		CHEAT_HUD_SET()
		if (configShowWallkickAngle&&gWallkickTimer!=0){
			gCurrTextScale = 0.8f;
			get_angle_text(coord,(gWallkickAngle<32768) ? 32768-gWallkickAngle : gWallkickAngle-32768);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 10;
		}
		if (configShowWallkickFrame&&gWallkickTimer!=0){
			set_text_color(
				sWallkickColors[gWallkickFrame-1][0],
				sWallkickColors[gWallkickFrame-1][1],
				sWallkickColors[gWallkickFrame-1][2],
				255
			);
			gCurrTextScale = 1.0f;
			sprintf(coord,"%df",gWallkickFrame);
			render_shadow_text_string_at(xPos+16,yPos,coord);
			yPos += 10;
		}
		set_text_color(255,255,255,255);
		yPos += INFO_SPACING;
	}
	
	if (configShowBowserInfo){
		CHEAT_HUD_SET()
		if (sHoldingBowser){
			gCurrTextScale = 0.8f;
			float rate = (sBowserAngleVel/4096.0f)*100.0f;
			sprintf(coord,"Spins %3.0f%%",rate);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 12;
			
			float spinSum = 0.0f;
			u32 count = sizeof(sSpinArray)/sizeof(sSpinArray[0]);
			for (u32 i=0;i<count;++i){
				spinSum += sSpinArray[i];
			}
			
			spinSum /= count;
			spinSum /= 128.0f;
			spinSum *= 100.0f;
			
			sprintf(coord,"Spin Eff %3.0f%%",spinSum);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 12;
			
			--sHoldingBowser;
			
			if (sHoldingBowser==0&&!gFrameAdvance){
				memset(sSpinArray,0,sizeof(sSpinArray));
			}
			yPos += INFO_SPACING;
		}
	}
	
	set_text_color(255,255,255,255);
}
extern u8 gInStarSelect;
static void render_debug(void){
	create_dl_ortho_matrix();
	set_text_color(255,255,255,255);
	
	char text[30];
	
	gCurrTextScale = 1.0f;
	
	sprintf(text,"%d",gGlobalTimer);
	render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-50,text);
	
	sprintf(text,"%u",gIsRTA);
	render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-70,text);
	
	sprintf(text,"%u",gInStarSelect);
	render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-90,text);
	
	sprintf(text,"%u",gCurrSaveStateIndex);
	render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-110,text);
	sprintf(text,"%u",gCurrLoadStateIndex);
	render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-125,text);
	
	//sprintf(text,"%.2fm",gPracticeStats.distanceMoved/100.0);
	//render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-145,text);
	
	
	//if (gCamera){
		/*sprintf(text,"%.7f",gLakituState.pos[0]);
		render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-110,text);
		sprintf(text,"%.7f",gLakituState.pos[1]);
		render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-120,text);
		sprintf(text,"%.7f",gLakituState.pos[2]);
		render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-130,text);
		
		sprintf(text,"%.7f",gLakituState.focus[0]);
		render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-150,text);
		sprintf(text,"%.7f",gLakituState.focus[1]);
		render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-160,text);
		sprintf(text,"%.7f",gLakituState.focus[2]);
		render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-170,text);*/
	//}
}

static void render_practice_message(void){
	if (!gPracticeMessage)
		return;
	
	s32 a = 255;
	if (gPracticeMessageTimer<8){
		a = gPracticeMessageTimer*255/8;
	}
	--gPracticeMessageTimer;
	set_text_color(255,255,255,a);
	gCurrTextScale = 1.0f;
	s32 textW = get_text_width(gPracticeMessage);
	render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-textW-16,BORDER_HEIGHT+40,gPracticeMessage);
}

void render_practice_info(void){
	if (configShowTimer)
		render_game_timer();
	render_mario_info();
	render_test();
	if (configShowControls)
		render_controls();
	if (gDebug)
		render_debug();
	if (gPracticeMessageTimer)
		render_practice_message();
}

static void init_rng_table(void){
	gRandomSeed16 = 0;
	gRNGTable[0] = 0;
	random_u16();
	u16 i=1;
	while (gRandomSeed16!=0){
		gRNGTable[gRandomSeed16] = i++;
		random_u16();
	}
	
	// second cycle
	gRandomSeed16 = 46497;
	while (gRandomSeed16!=0){
		gRNGTable[gRandomSeed16] = i++;
		random_u16();
	}
	
	// two cycle
	gRNGTable[58704] = i++;
	gRNGTable[22026] = i++;
	gRandomSeed16 = 0;
	gRandomCalls = 0;
}

void practice_init(void){
	memset(sSpinArray,0,sizeof(sSpinArray));
	memset(gRNGTable,0,sizeof(gRNGTable));
	
	init_rng_table();
	
	init_reset_vars();
	init_state_list();
	gPracticeDest.type = WARP_TYPE_NOT_WARPING;
	
	load_all_settings();
	ghost_init();
	browser_init();
}

void practice_deinit(void){
	free(sResetSave.varPtr);
	browser_free();
	free_state_list();
	clear_replay_history();
	if (gPracticeRecordingReplay){
		free_replay(gPracticeRecordingReplay);
		gPracticeRecordingReplay = NULL;
	}
	if (gPracticeFinishedReplay){
		free_replay(gPracticeFinishedReplay);
		gPracticeFinishedReplay = NULL;
	}
	assert(gReplayBalance==0);
}