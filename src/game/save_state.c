#include <PR/ultratypes.h>

#include "sm64.h"
#include "save_state.h"
#include "ingame_menu.h"
#include "engine/math_util.h"
#include "game_init.h"
#include "sound_init.h"
#include "audio/external.h"
#include "audio/internal.h"
#include "area.h"
#include "mario_misc.h"
#include "paintings.h"
#include "envfx_snow.h"
#include "practice.h"
#include "pc/configfile.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

u8 gHasStateSaved = FALSE;
SaveStateList* gCurrSaveStateSlot;
SaveStateList* gCurrLoadStateSlot;
u32 gCurrSaveStateIndex = 0;
u32 gCurrLoadStateIndex = 0;

SaveStateList* sSaveStateSlotsHead = NULL;

extern s32 gPracticeSubStatus;
extern struct Object *gMarioPlatform;
extern s32 sBubbleParticleCount;
extern s16 gSparklePhase;
extern struct PlayerGeometry sMarioGeometry;
extern struct CameraFOVStatus sFOVState;
extern struct ModeTransitionInfo sModeInfo;
extern struct TransitionInfo sModeTransition;
extern s16 gCutsceneTimer,sSelectionFlags,sCSideButtonYaw,s8DirModeBaseYaw,s8DirModeYawOffset;
extern s16 sCutsceneShot;
extern u8 sFramesSinceCutsceneEnded;
extern u8 sCutsceneDialogResponse;
extern s16 sAreaYaw;
extern s16 sAreaYawChange;
extern s16 sAvoidYawVel;
extern s16 sYawSpeed;
extern s16 sCameraYawAfterDoorCutscene;
extern u32 gCutsceneObjSpawn;
extern struct HandheldShakePoint sHandheldShakeSpline[4];

extern s16 sHandheldShakePitch;
extern s16 sHandheldShakeYaw;
extern s16 sHandheldShakeRoll;

extern u32 sParTrackIndex;

extern Vec3f sFixedModeBasePosition;
extern Vec3f sCastleEntranceOffset;

extern s16 s2ndRotateFlags;

extern s16 sModeOffsetYaw;
extern s16 sCUpCameraPitch;
extern s16 sLakituDist,sLakituPitch;
extern s16 sStatusFlags;
extern f32 sPanDistance;
extern f32 sCannonYOffset;
extern s16 sSpiralStairsYawOffset;

extern s32 gCurrLevelArea;
extern u32 gPrevLevel;

extern s16 sSwimStrength;

extern f32 gCameraZoomDist;

extern struct ParallelTrackingPoint* sParTrackPath;
extern struct CutsceneVariable sCutsceneVars[10];
extern struct CameraStoredInfo sParTrackTransOff;
extern struct CameraStoredInfo sCameraStoreCUp;
extern struct CameraStoredInfo sCameraStoreCutscene;
extern s16 sBehindMarioSoundTimer;

extern s8 sWarpCheckpointIsActive;
extern s16 gWarpTransDelay;
extern u32 gFBSetColor;
extern u32 gWarpTransFBSetColor;
extern u8 gWarpTransRed;
extern u8 gWarpTransGreen;
extern u8 gWarpTransBlue;

extern u8 sTransitionColorFadeCount[4];
extern u16 sTransitionTextureFadeCount[2];

extern u8 sDisplayingDoorText;
extern u8 sJustTeleported;
extern u8 sPssSlideStarted;

extern s32 sNumActiveFirePiranhaPlants;
extern s32 sNumKilledFirePiranhaPlants;
extern s32 sMontyMoleKillStreak;
extern f32 sMontyMoleLastKilledPosX;
extern f32 sMontyMoleLastKilledPosY;
extern f32 sMontyMoleLastKilledPosZ;

extern s16 sPrevCheckMarioRoom;
extern s8 sYoshiDead;
extern s16 gMenuMode;

extern u16 sCurrentMusic;
extern u16 sCurrentShellMusic;
extern u16 sCurrentCapMusic;
extern u8 sPlayingInfiniteStairs;

extern u8 sSoundFlags;
extern u8 sBackgroundSoundDisabled;
extern u8 gUnbreakMusic;

extern s32 gCourseDoneMenuTimer;
extern s32 gCourseCompleteCoins;
extern s8 gCourseCompleteCoinsEqual;

extern struct SaveBuffer gSaveBuffer;

extern s32 gWdwWaterLevelSet;

extern u16 gRandomSeed16;
extern u16 gRandomCalls;

void init_state(SaveState* state){
	for (u32 i=0;i<8;++i){
		state->objState.areaData.macroData[i].data = NULL;
		state->objState.areaData.respawnData[i].data = NULL;
		state->objState.areaData.warpData[i].data = NULL;
	}
	
	state->replayState = NULL;
	state->levelState.envBuffer = NULL;
}

SaveState* alloc_state(void){
	SaveState* state = (SaveState*)malloc(sizeof(SaveState));
	init_state(state);
	return state;
}

void free_state(SaveState* state){
	if (state->levelState.envBuffer!=NULL)
		free(state->levelState.envBuffer);
	
	for (u32 i=0;i<8;++i){
		if (state->objState.areaData.macroData[i].data)
			free(state->objState.areaData.macroData[i].data);
		if (state->objState.areaData.respawnData[i].data)
			free(state->objState.areaData.respawnData[i].data);
		if (state->objState.areaData.warpData[i].data)
			free(state->objState.areaData.warpData[i].data);
	}
	
	if (state->replayState!=NULL)
		free_replay(state->replayState);
	
	free(state);
}

void alloc_state_at_save_slot(void){
	if (gCurrSaveStateSlot->state)
		free_state(gCurrSaveStateSlot->state);
	gCurrSaveStateSlot->state = alloc_state();
}

SaveStateList* alloc_list_slot(SaveStateList* prev){
	SaveStateList* s = (SaveStateList*)malloc(sizeof(SaveStateList));
	s->next = NULL;
	s->prev = prev;
	s->state = NULL;
	return s;
}

void init_state_list(void){
	sSaveStateSlotsHead = alloc_list_slot(NULL);
	gCurrSaveStateSlot = sSaveStateSlotsHead;
	gCurrSaveStateIndex = 0;
	gCurrLoadStateSlot = sSaveStateSlotsHead;
	gCurrLoadStateIndex = 0;
}

void free_state_list(void){
	SaveStateList* curr = sSaveStateSlotsHead;
	SaveStateList* next;
	while (curr){
		next = curr->next;
		if (curr->state){
			free_state(curr->state);
		}
		free(curr);
		curr = next;
	}
}

void clear_save_states(void){
	free_state_list();
	init_state_list();
}

void save_slot_increment(void){
	if (gCurrSaveStateSlot->next==NULL){
		gCurrSaveStateSlot->next = alloc_list_slot(gCurrSaveStateSlot);
	}
	gCurrSaveStateSlot = gCurrSaveStateSlot->next;
	++gCurrSaveStateIndex;
}

void save_slot_decrement(void){
	if (gCurrSaveStateSlot->prev){
		gCurrSaveStateSlot = gCurrSaveStateSlot->prev;
		--gCurrSaveStateIndex;
	}
}

void load_slot_increment(void){
	if (gCurrLoadStateSlot->next==NULL){
		gCurrLoadStateSlot->next = alloc_list_slot(gCurrLoadStateSlot);
	}
	gCurrLoadStateSlot = gCurrLoadStateSlot->next;
	++gCurrLoadStateIndex;
}

void load_slot_decrement(void){
	if (gCurrLoadStateSlot->prev){
		gCurrLoadStateSlot = gCurrLoadStateSlot->prev;
		--gCurrLoadStateIndex;
	}
}

void save_env_buffer(LevelState* levelState){
	if (levelState->envBuffer!=NULL){
		free(levelState->envBuffer);
		levelState->envBuffer = NULL;
	}
	
	if (gEnvFxMode!=ENVFX_MODE_NONE){
		if (gEnvFxMode<ENVFX_BUBBLE_START){
			levelState->snowParticleCount = gSnowParticleCount;
			levelState->bubbleParticleCount = 0;
			
			u32 count = gSnowParticleCount*sizeof(struct EnvFxParticle);
			levelState->envBuffer = malloc(count);
			memcpy(levelState->envBuffer,gEnvFxBuffer,count);
		} else {
			levelState->bubbleParticleCount = sBubbleParticleCount;
			levelState->snowParticleCount = 0;
			
			u32 count = sBubbleParticleCount*sizeof(struct EnvFxParticle);
			levelState->envBuffer = malloc(count);
			memcpy(levelState->envBuffer,gEnvFxBuffer,count);
		}
	}
}

void load_env_buffer(const LevelState* levelState){
	if (levelState->envBuffer==NULL)
		return;
	
	if (levelState->snowParticleCount){
		gSnowParticleCount = levelState->snowParticleCount;
		u32 count = levelState->snowParticleCount*sizeof(struct EnvFxParticle);
		memcpy(gEnvFxBuffer,levelState->envBuffer,count);
	} else if (levelState->bubbleParticleCount){
		sBubbleParticleCount = levelState->bubbleParticleCount;
		u32 count = levelState->bubbleParticleCount*sizeof(struct EnvFxParticle);
		memcpy(gEnvFxBuffer,levelState->envBuffer,count);
	}
}

void save_macro_objects(MacroObjectData* macroData,const s16* data){
	if (macroData->data!=NULL){
		free(macroData->data);
		macroData->data = NULL;
	}
	
	s32 l = 0;
	s32 presetID;
	const s16* node = data;
	// get length
	while (*node!=-1){
		presetID = (*node & 0x1FF) - 31;
        if (presetID < 0) {
            break;
        }
		
		++l;
		node += 5;
	}
	
	macroData->length = l;
	macroData->data = malloc(sizeof(s16)*l);
	l = 0;
	node = data;
	// save data
	while (*node!=-1){
		presetID = (*node & 0x1FF) - 31;
		if (presetID < 0) {
            break;
        }
		
		macroData->data[l] = *(node+4);
		++l;
		node += 5;
	}
}

void save_respawn_info(RespawnInfoData* respawnData,const struct SpawnInfo* spawnInfos){
	if (respawnData->data!=NULL){
		free(respawnData->data);
		respawnData->data = NULL;
	}
	s32 l = 0;
	const struct SpawnInfo* node = spawnInfos;
	while (node!=NULL){
		++l;
		node = node->next;
	}
	
	respawnData->length = l;
	respawnData->data = malloc(sizeof(u32)*l);
	l = 0;
	
	node = spawnInfos;
	while (node!=NULL){
		respawnData->data[l] = node->behaviorArg;
		++l;
		node = node->next;
	}
}

void save_warp_objects(WarpObjectData* warpData,struct ObjectWarpNode* warpNodes){
	struct ObjectWarpNode* it = warpNodes;
	u32 l = 0;
	while (it){
		++l;
		it = it->next;
	}
	
	warpData->length = l;
	warpData->data = malloc(l*sizeof(struct Object*));
	
	it = warpNodes;
	l = 0;
	while (it){
		warpData->data[l++] = it->object;
		it = it->next;
	}
}

void save_area_data(AreaData* areaData){
	areaData->macroMask = 0;
	areaData->respawnMask = 0;
	areaData->warpMask = 0;
	for (u32 i=0;i<8;++i){
		if (areaData->macroData[i].data){
			free(areaData->macroData[i].data);
			areaData->macroData[i].data = NULL;
		}
		
		if (areaData->respawnData[i].data){
			free(areaData->respawnData[i].data);
			areaData->respawnData[i].data = NULL;
		}
		
		if (areaData->warpData[i].data){
			free(areaData->warpData[i].data);
			areaData->warpData[i].data = NULL;
		}
	}
	
	for (u32 i=0;i<8;++i){
		if (gAreaData[i].macroObjects!=NULL){
			areaData->macroMask |= (1<<i);
			save_macro_objects(&areaData->macroData[i],gAreaData[i].macroObjects);
		}
		if (gAreaData[i].objectSpawnInfos!=NULL){
			areaData->respawnMask |= (1<<i);
			save_respawn_info(&areaData->respawnData[i],gAreaData[i].objectSpawnInfos);
		}
		if (gAreaData[i].warpNodes!=NULL){
			areaData->warpMask |= (1<<i);
			save_warp_objects(&areaData->warpData[i],gAreaData[i].warpNodes);
		}
	}
}

void load_macro_objects(const MacroObjectData* macroData,s16* data){
	for (u32 i=0;i<macroData->length;++i){
		data[i*5+4] = macroData->data[i];
	}
}

void load_respawn_info(const RespawnInfoData* respawnData,struct SpawnInfo* spawnInfos){
	for (u32 i=0;i<respawnData->length;++i){
		assert(spawnInfos!=NULL);
		/*if (spawnInfos==NULL){
			printf("%d/%d\n",i,respawnData->length);
			break;
		}*/
		spawnInfos->behaviorArg = respawnData->data[i];
		spawnInfos = spawnInfos->next;
	}
}

void load_warp_data(const WarpObjectData* warpData,struct ObjectWarpNode* warpNodes){
	for (u32 i=0;i<warpData->length;++i){
		assert(warpNodes!=NULL);
		
		warpNodes->object = warpData->data[i];
		warpNodes = warpNodes->next;
	}
}

void load_area_data(const AreaData* areaData){
	for (u32 i=0;i<8;++i){
		if (areaData->macroMask&(1<<i)){
			load_macro_objects(&areaData->macroData[i],gAreaData[i].macroObjects);
		}
		if (areaData->respawnMask&(1<<i)){
			load_respawn_info(&areaData->respawnData[i],gAreaData[i].objectSpawnInfos);
		}
		if (areaData->warpMask&(1<<i)){
			load_warp_data(&areaData->warpData[i],gAreaData[i].warpNodes);
		}
	}
}

void save_all_objects(ObjectState* objState){
	memcpy(&objState->objectPoolCopy[0],&gObjectPool[0],sizeof(struct Object)*OBJECT_POOL_CAPACITY);
	objState->freeList = gFreeObjectList;
	memcpy(&objState->objectListArray[0],&gObjectListArray[0],sizeof(struct ObjectNode)*16);
	memcpy(&objState->objectMemoryPoolData[0],gObjectMemoryPool,0x800);
	objState->hudState = gHudDisplay;
	objState->marioState = gMarioStates[0];
	objState->marioBodyState = gBodyStates[0];
	objState->marioSpawnInfo = gPlayerSpawnInfos[0];
	objState->marioPlatform = gMarioPlatform;
	memcpy(&objState->doorAdjacentRooms[0],&gDoorAdjacentRooms[0][0],sizeof(s8)*120);
	save_area_data(&objState->areaData);
	objState->lastButtons = gLastButtons;
}

void save_camera_state(CameraState* camState){
	for (int i=0;i<8;++i){
		if (gAreaData[i].camera!=NULL){
			camState->areaCams[i] = *gAreaData[i].camera;
		}
	}
	
	memcpy(&camState->cutsceneVars[0],&sCutsceneVars[0],sizeof(struct CutsceneVariable)*10);
	memcpy(camState->handheldShakeSpline,sHandheldShakeSpline,sizeof(sHandheldShakeSpline));
	
	camState->lakituState = gLakituState;
	camState->playerCam = gPlayerCameraState[0];
	camState->marioGeometry = sMarioGeometry;
	camState->fovState = sFOVState;
	camState->modeInfo = sModeInfo;
	camState->modeTransition = sModeTransition;
	camState->cameraMovementFlags = gCameraMovementFlags;
	camState->cutsceneTimer = gCutsceneTimer;
	camState->cutsceneShot = sCutsceneShot;
	camState->camSelectionFlags = sSelectionFlags;
	camState->rotateFlags = s2ndRotateFlags;
	camState->cSideButtonYaw = sCSideButtonYaw;
	camState->mode8BaseYaw = s8DirModeBaseYaw;
	camState->mode8YawOffset = s8DirModeYawOffset;
	camState->handheldShakePitch = sHandheldShakePitch;
	camState->handheldShakeYaw = sHandheldShakeYaw;
	camState->handheldShakeRoll = sHandheldShakeRoll;
	camState->areaYaw = sAreaYaw;
	camState->areaYawChange = sAreaYawChange;
	camState->yawSpeed = sYawSpeed;
	camState->avoidYawVel = sAvoidYawVel;
	camState->yawAfterDoorCutscene = sCameraYawAfterDoorCutscene;
	camState->modeOffsetYaw = sModeOffsetYaw;
	camState->spiralStairsYawOffset = sSpiralStairsYawOffset;
	camState->cUpCameraPitch = sCUpCameraPitch;
	camState->lakituDist = sLakituDist;
	camState->lakituPitch = sLakituPitch;
	camState->camStatusFlags = sStatusFlags;
	camState->horizCamHold = sBehindMarioSoundTimer;
	camState->panDist = sPanDistance;
	camState->cannonYOffset = sCannonYOffset;
	camState->zoomDist = gCameraZoomDist;
	camState->parallelTrackIndex = sParTrackIndex;
	camState->currLevelArea = gCurrLevelArea;
	camState->prevLevel = gPrevLevel;
	
	camState->parallelTrackPath = sParTrackPath;
	camState->parallelTrackInfo = sParTrackTransOff;
	camState->cameraStoreCUp = sCameraStoreCUp;
	camState->cameraStoreCutscene = sCameraStoreCutscene;
	
	vec3f_copy(camState->fixedModeBasePosition,sFixedModeBasePosition);
	vec3f_copy(camState->castleEntranceOffset,sCastleEntranceOffset);
	
	camState->recentCutscene = gRecentCutscene;
	camState->framesSinceLastCutscene = sFramesSinceCutsceneEnded;
	camState->cutsceneDialogResponse = sCutsceneDialogResponse;
	
	camState->objectCutsceneDone = gObjCutsceneDone;
	camState->objectCutsceneSpawn = gCutsceneObjSpawn;
	camState->cutsceneFocusObj = gCutsceneFocus;
	camState->secondaryCutsceneFocusObj = gSecondCameraFocus;
}

void save_level_state(LevelState* levelState){
	levelState->warpDest = sWarpDest;
	
	levelState->loc.type = WARP_TYPE_CHANGE_LEVEL;
	levelState->loc.levelNum = gCurrLevelNum;
	levelState->loc.areaIdx = gCurrAreaIndex;
	levelState->loc.nodeId = 0xA;
	levelState->loc.arg = 0;
	
	levelState->specialWarpLevelDest = sSpecialWarpLevelNum;
	levelState->delayedWarpOp = sDelayedWarpOp;
	levelState->delayedWarpTimer = sDelayedWarpTimer;
	levelState->sourceWarpNodeId = sSourceWarpNodeId;
	levelState->delayedWarpArg = sDelayedWarpArg;
	levelState->warpTransition = gWarpTransition;
	levelState->warpCheckpoint = gWarpCheckpoint;
	levelState->lastLevelNum = gLastLevelNum;
	
	levelState->areaUpdateCounter = gAreaUpdateCounter;
	
	levelState->nonstop = configNonstop;
	levelState->currSaveFileNum = gCurrSaveFileNum;
	levelState->saveFile = gSaveBuffer.files[gCurrSaveFileNum-1][0];
	levelState->soundMode = gSaveBuffer.menuData[0].soundMode;
	levelState->globalTimer = gGlobalTimer;
	levelState->lastCompletedCourseNum = gLastCompletedCourseNum;
	levelState->lastCompletedStarNum = gLastCompletedStarNum;
	levelState->currActNum = gCurrActNum;
	levelState->currCourseNum = gCurrCourseNum;
	levelState->savedCourseNum = gSavedCourseNum;
	levelState->shouldNotPlayCastleMusic = gShouldNotPlayCastleMusic;
	
	levelState->THIWaterDrained = gTHIWaterDrained;
	levelState->TTCSpeedSetting = gTTCSpeedSetting;
	levelState->CCMEnteredSlide = gCCMEnteredSlide;
	levelState->pssSlideStarted = sPssSlideStarted;
	
	levelState->specialTripleJump = gSpecialTripleJump;
	levelState->yoshiDead = sYoshiDead;
	
	levelState->randState = gRandomSeed16;
	levelState->randCalls = gRandomCalls;
	
	levelState->hudAnim = sPowerMeterHUD;
	levelState->powerMeterStoredHealth = sPowerMeterStoredHealth;
	levelState->powerMeterVisibleTimer = sPowerMeterVisibleTimer;
	
	levelState->warpTransDelay = gWarpTransDelay;
	levelState->fbSetColor = gFBSetColor;
	levelState->warpFbSetColor = gWarpTransFBSetColor;
	levelState->warpRed = gWarpTransRed;
	levelState->warpGreen = gWarpTransGreen;
	levelState->warpBlue = gWarpTransBlue;
	memcpy(levelState->transitionColorFadeCount,sTransitionColorFadeCount,sizeof(u8)*4);
	memcpy(levelState->transitionTextureFadeCount,sTransitionTextureFadeCount,sizeof(u16)*2);
	
	levelState->pauseScreenMode = gPauseScreenMode;
	levelState->saveOptSelectIndex = gSaveOptSelectIndex;
	levelState->currPlayMode = sCurrPlayMode;
	levelState->menuMode = gMenuMode;
	levelState->transitionTimer = sTransitionTimer;
	levelState->transitionUpdate = sTransitionUpdate;
	levelState->currCreditsEntry = gCurrCreditsEntry;
	levelState->timerRunning = sTimerRunning;
	
	levelState->timeStopState = gTimeStopState;
	
	levelState->marioOnMerryGoRound = gMarioOnMerryGoRound;
	levelState->marioCurrentRoom = gMarioCurrentRoom;
	levelState->prevCheckMarioRoom = sPrevCheckMarioRoom;
	levelState->marioShotFromCannon = gMarioShotFromCannon;
	
	levelState->numActivePiranhaPlants = sNumActiveFirePiranhaPlants;
	levelState->numKilledPiranhaPlants = sNumKilledFirePiranhaPlants;
	levelState->montyMoleKillStreak = sMontyMoleKillStreak;
	levelState->montyMoleLastKilledPosX = sMontyMoleLastKilledPosX;
	levelState->montyMoleLastKilledPosY = sMontyMoleLastKilledPosY;
	levelState->montyMoleLastKilledPosZ = sMontyMoleLastKilledPosZ;
	
	levelState->displayingDoorText = sDisplayingDoorText;
	levelState->justTeleported = sJustTeleported;
	levelState->warpCheckpointIsActive = sWarpCheckpointIsActive;
	levelState->dddPaintingStatus = gDddPaintingStatus;
	levelState->wdwWaterLevelChanging = gWDWWaterLevelChanging;
	levelState->wdwWaterLevelSet = gWdwWaterLevelSet;
	
	levelState->aPressCount = gAPressCounter;
	
	levelState->sparklePhase = gSparklePhase;
	levelState->swimStrength = sSwimStrength;
	
	save_env_buffer(levelState);
	
	levelState->envLevels[0] = gEnvironmentLevels[0];
	levelState->envLevels[1] = gEnvironmentLevels[2];
	
	if (gEnvironmentRegions!=NULL){
		levelState->envRegionHeights[0] = gEnvironmentRegions[6];
		levelState->envRegionHeights[1] = gEnvironmentRegions[12];
		levelState->envRegionHeights[2] = gEnvironmentRegions[18];
	}
}

void save_game_init_state(GameInitState* state){
	state->introSkip = configSkipIntro;
	state->introSkipTiming = configIntroSkipTiming;
	state->nonstop = configNonstop;
	state->noInvisibleWalls = configNoInvisibleWalls;
	state->stageText = configStageText;
}

void save_level_init_state(LevelInitState* initState,struct WarpDest* warpSave){
	if (warpSave!=NULL){
		initState->loc = *warpSave;
	} else {
		initState->loc.type = WARP_TYPE_CHANGE_LEVEL;
		initState->loc.levelNum = gCurrLevelNum;
		initState->loc.areaIdx = gCurrAreaIndex;
		initState->loc.nodeId = 0xA;
		initState->loc.arg = 0;
	}
	memcpy(initState->handheldShakeSpline,sHandheldShakeSpline,sizeof(sHandheldShakeSpline));
	initState->currSaveFileNum = gCurrSaveFileNum;
	initState->saveFile = gSaveBuffer.files[gCurrSaveFileNum-1][0];
	initState->soundMode = gSaveBuffer.menuData[0].soundMode;
	initState->globalTimer = gGlobalTimer;
	initState->lastCompletedCourseNum = gLastCompletedCourseNum;
	initState->lastCompletedStarNum = gLastCompletedStarNum;
	initState->nonstop = configNonstop;
	initState->introSkip = configSkipIntro;
	initState->currActNum = gCurrActNum;
	initState->currCourseNum = gCurrCourseNum;
	initState->savedCourseNum = gSavedCourseNum;
	initState->shouldNotPlayCastleMusic = gShouldNotPlayCastleMusic;
	initState->camSelectionFlags = sSelectionFlags;
	
	initState->THIWaterDrained = gTHIWaterDrained;
	initState->TTCSpeedSetting = gTTCSpeedSetting;
	initState->CCMEnteredSlide = gCCMEnteredSlide;
	initState->pssSlideStarted = sPssSlideStarted;
	initState->dddPaintingStatus = gDddPaintingStatus;
	
	initState->practiceSubStatus = gPracticeSubStatus;
	initState->practiceStageText = configStageText;
	initState->lastLevelNum = gLastLevelNum;
	
	initState->lastButtons = gLastButtons;
	
	initState->specialTripleJump = gSpecialTripleJump;
	initState->yoshiDead = sYoshiDead;
	initState->randState = gRandomSeed16;
	initState->randCalls = gRandomCalls;
	
	initState->numStars = gMarioStates[0].numStars;
	initState->numLives = gMarioStates[0].numLives;
	initState->health = gMarioStates[0].health;
	
	switch (gWarpCheckpoint.courseNum){
		case COURSE_NONE:
			initState->warpCheckpointType = WARP_CHECKPOINT_STATE_NONE;
			break;
		case COURSE_LLL:
			initState->warpCheckpointType = WARP_CHECKPOINT_STATE_LLL;
			break;
		case COURSE_SSL:
			initState->warpCheckpointType = WARP_CHECKPOINT_STATE_SSL;
			break;
		case COURSE_TTM:
			initState->warpCheckpointType = WARP_CHECKPOINT_STATE_TTM;
			break;
		default:
			assert(FALSE);
			break;
	}
	
	initState->holp[0] = gBodyStates[0].heldObjLastPosition[0];
	initState->holp[1] = gBodyStates[0].heldObjLastPosition[1];
	initState->holp[2] = gBodyStates[0].heldObjLastPosition[2];
	
	initState->envLevels[0] = gEnvironmentLevels[0];
	initState->envLevels[1] = gEnvironmentLevels[2];
	
	if (gEnvironmentRegions!=NULL){
		initState->envRegionHeights[0] = gEnvironmentRegions[6];
		initState->envRegionHeights[1] = gEnvironmentRegions[12];
		initState->envRegionHeights[2] = gEnvironmentRegions[18];
	}
	
	initState->swimStrength = sSwimStrength;
	
	// slide yaw not needed?
	initState->slideYaw = gMarioStates[0].slideYaw;
	initState->twirlYaw = gMarioStates[0].twirlYaw;
	initState->slideVelX = gMarioStates[0].slideVelX;
	initState->slideVelZ = gMarioStates[0].slideVelZ;
	
	initState->sparklePhase = gSparklePhase;
	
	initState->menuHoldKeyIndex = gMenuHoldKeyIndex;
	initState->menuHoldKeyTimer = gMenuHoldKeyTimer;
}

void save_dialog_state(DialogState* dialogState){
	dialogState->hudFlash = gHudFlash;
	dialogState->dialogCourseActNum = gDialogCourseActNum;
	dialogState->lastDialogLineNum = gLastDialogLineNum;
	dialogState->dialogId = gDialogID;
	dialogState->dialogColorFadeTimer = gDialogColorFadeTimer;
	dialogState->dialogTextAlpha = gDialogTextAlpha;
	dialogState->dialogResponse = gDialogResponse;
	dialogState->dialogVar = gDialogVariable;
	dialogState->cutsceneMsgX = gCutsceneMsgXOffset;
	dialogState->cutsceneMsgY = gCutsceneMsgYOffset;
	
	dialogState->dialogBoxState = gDialogBoxState;
	dialogState->dialogBoxOpenTimer = gDialogBoxOpenTimer;
	dialogState->dialogBoxScale = gDialogBoxScale;
	dialogState->dialogScrollOffsetY = gDialogScrollOffsetY;
	dialogState->dialogBoxType = gDialogBoxType;
	dialogState->lastDialogPageStrPos = gLastDialogPageStrPos;
	dialogState->dialogTextPos = gDialogTextPos;
	dialogState->dialogLineNum = gDialogLineNum;
	dialogState->lastDialogResponse = gLastDialogResponse;
	dialogState->menuHoldKeyIndex = gMenuHoldKeyIndex;
	dialogState->menuHoldKeyTimer = gMenuHoldKeyTimer;
	dialogState->courseDoneMenuTimer = gCourseDoneMenuTimer;
	dialogState->courseCompleteCoins = gCourseCompleteCoins;
	dialogState->courseCompleteCoinsEqual = gCourseCompleteCoinsEqual;
}

void save_sound_state(SoundState* soundState){
	soundState->soundFlags = sSoundFlags;
	soundState->backgroundSoundDisabled = sBackgroundSoundDisabled;
	soundState->infiniteStairs = sPlayingInfiniteStairs;
	soundState->currentMusic = sCurrentMusic;
	soundState->currentShellMusic = sCurrentShellMusic;
	soundState->currentCapMusic = sCurrentCapMusic;
	
	soundState->musicParam1 = gCurrentArea->musicParam;
	soundState->musicParam2 = gCurrentArea->musicParam2;
}

void save_practice_state(PracticeState* practiceState){
	practiceState->lastWarpDest = gLastWarpDest;
	practiceState->sectionTimer = gSectionTimer;
	practiceState->sectionTimerResult = gSectionTimerResult;
	
	practiceState->ghostFrame = gCurrGhostFrame;
	practiceState->ghostArea = gCurrGhostArea;
	practiceState->ghostAreaCounter = gGhostAreaCounter;
	practiceState->ghostIndex = gGhostDataIndex;
	
	practiceState->practiceSubStatus = gPracticeSubStatus;
	practiceState->practiceStageText = configStageText;
	practiceState->introSkip = configSkipIntro;
}

void save_state(SaveState* state){
	save_all_objects(&state->objState);
	save_level_state(&state->levelState);
	save_dialog_state(&state->dialogState);
	save_camera_state(&state->camState);
	save_sound_state(&state->soundState);
	save_practice_state(&state->practiceState);
	
	Replay* replayCopy = NULL;
	// copy before freeing
	if (gCurrRecordingReplay){
		// will likely be gPracticeRecordingReplay
		replayCopy = copy_replay(gCurrRecordingReplay,NULL,0);
	} else if (gCurrPlayingReplay){
		RLEChunk* frame;
		u8 subframe;
		
		// will likely be gPracticeFinishedReplay
		get_current_replay_pos(&frame,&subframe);
		replayCopy = copy_replay(gCurrPlayingReplay,frame,subframe);
	}
	
	if (state->replayState){
		free_replay(state->replayState);
		state->replayState = NULL;
	}
	
	state->replayState = replayCopy;
}

void load_all_objects(const ObjectState* objState){
	memcpy(&gObjectPool[0],&objState->objectPoolCopy[0],sizeof(struct Object)*OBJECT_POOL_CAPACITY);
	gFreeObjectList = objState->freeList;
	memcpy(&gObjectListArray[0],&objState->objectListArray[0],sizeof(struct ObjectNode)*16);
	memcpy(gObjectMemoryPool,&objState->objectMemoryPoolData[0],0x800);
	gHudDisplay = objState->hudState;
	gMarioStates[0] = objState->marioState;
	gMarioObject = gMarioStates[0].marioObj;
	gBodyStates[0] = objState->marioBodyState;
	if (gMarioStates[0].marioObj!=NULL){
		s32 animId = gMarioStates[0].marioObj->header.gfx.unk38.animID;
		struct Animation* targetAnim = gMarioStates[0].animation->targetAnim;
		
		if (load_patchable_table(gMarioStates[0].animation,animId)) {
			targetAnim->values = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->values);
			targetAnim->index = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->index);
		}
	}
	gPlayerSpawnInfos[0] = objState->marioSpawnInfo;
	gMarioPlatform = objState->marioPlatform;
	load_area_data(&objState->areaData);
	memcpy(&gDoorAdjacentRooms[0][0],&objState->doorAdjacentRooms[0],sizeof(s8)*120);
	gLastButtons = objState->lastButtons;
	gPlayer1Controller->buttonPressed = gPlayer1Controller->buttonDown & (~gLastButtons);
	copy_to_player_3();
}

void load_camera_state(const CameraState* camState){
	for (int i=0;i<8;++i){
		if (gAreaData[i].camera!=NULL){
			*gAreaData[i].camera = camState->areaCams[i];
		}
	}
	
	memcpy(&sCutsceneVars[0],&camState->cutsceneVars[0],sizeof(struct CutsceneVariable)*10);
	memcpy(sHandheldShakeSpline,camState->handheldShakeSpline,sizeof(sHandheldShakeSpline));
	
	gLakituState = camState->lakituState;
	gPlayerCameraState[0] = camState->playerCam;
	sMarioGeometry = camState->marioGeometry;
	sFOVState = camState->fovState;
	sModeInfo = camState->modeInfo;
	sModeTransition = camState->modeTransition;
	gCameraMovementFlags = camState->cameraMovementFlags;
	gCutsceneTimer = camState->cutsceneTimer;
	sCutsceneShot = camState->cutsceneShot;
	sSelectionFlags = camState->camSelectionFlags;
	s2ndRotateFlags = camState->rotateFlags;
	s8DirModeBaseYaw = camState->mode8BaseYaw;
	s8DirModeYawOffset = camState->mode8YawOffset;
	sCSideButtonYaw = camState->cSideButtonYaw;
	sHandheldShakePitch = camState->handheldShakePitch;
	sHandheldShakeYaw = camState->handheldShakeYaw;
	sHandheldShakeRoll = camState->handheldShakeRoll;
	sAreaYaw = camState->areaYaw;
	sAreaYawChange = camState->areaYawChange;
	sYawSpeed = camState->yawSpeed;
	sAvoidYawVel = camState->avoidYawVel;
	sCameraYawAfterDoorCutscene = camState->yawAfterDoorCutscene;
	sModeOffsetYaw = camState->modeOffsetYaw;
	sSpiralStairsYawOffset = camState->spiralStairsYawOffset;
	sCUpCameraPitch = camState->cUpCameraPitch;
	sLakituDist = camState->lakituDist;
	sLakituPitch = camState->lakituPitch;
	sStatusFlags = camState->camStatusFlags;
	sBehindMarioSoundTimer = camState->horizCamHold;
	sPanDistance = camState->panDist;
	sCannonYOffset = camState->cannonYOffset;
	gCameraZoomDist = camState->zoomDist;
	sParTrackIndex = camState->parallelTrackIndex;
	gCurrLevelArea = camState->currLevelArea;
	gPrevLevel = camState->prevLevel;
	
	sParTrackPath = camState->parallelTrackPath;
	sParTrackTransOff = camState->parallelTrackInfo;
	sCameraStoreCUp = camState->cameraStoreCUp;
	sCameraStoreCutscene = camState->cameraStoreCutscene;
	
	vec3f_copy(sFixedModeBasePosition,camState->fixedModeBasePosition);
	vec3f_copy(sCastleEntranceOffset,camState->castleEntranceOffset);
	
	gRecentCutscene = camState->recentCutscene;
	sFramesSinceCutsceneEnded = camState->framesSinceLastCutscene;
	
	gObjCutsceneDone = camState->objectCutsceneDone;
	gCutsceneObjSpawn = camState->objectCutsceneSpawn;
	gCutsceneFocus = camState->cutsceneFocusObj;
	gSecondCameraFocus = camState->secondaryCutsceneFocusObj;
}

void load_level_state(const LevelState* levelState){
	sWarpDest = levelState->warpDest;
	sSpecialWarpLevelNum = levelState->specialWarpLevelDest;
	sDelayedWarpOp = levelState->delayedWarpOp;
	sDelayedWarpTimer = levelState->delayedWarpTimer;
	sSourceWarpNodeId = levelState->sourceWarpNodeId;
	sDelayedWarpArg = levelState->delayedWarpArg;
	gWarpTransition = levelState->warpTransition;
	gWarpCheckpoint = levelState->warpCheckpoint;
	gLastLevelNum = levelState->lastLevelNum;
	
	gAreaUpdateCounter = levelState->areaUpdateCounter;
	
	configNonstop = levelState->nonstop;
	gCurrSaveFileNum = levelState->currSaveFileNum;
	gSaveBuffer.files[gCurrSaveFileNum-1][0] = levelState->saveFile;
	gSaveBuffer.menuData[0].soundMode = levelState->soundMode;
	
	gGlobalTimer = levelState->globalTimer;
	gLastCompletedCourseNum = levelState->lastCompletedCourseNum;
	gLastCompletedStarNum = levelState->lastCompletedStarNum;
	gCurrActNum = levelState->currActNum;
	gCurrCourseNum = levelState->currCourseNum;
	gSavedCourseNum = levelState->savedCourseNum;
	gShouldNotPlayCastleMusic = levelState->shouldNotPlayCastleMusic;
	
	gTHIWaterDrained = levelState->THIWaterDrained;
	gTTCSpeedSetting = levelState->TTCSpeedSetting;
	gCCMEnteredSlide = levelState->CCMEnteredSlide;
	sPssSlideStarted = levelState->pssSlideStarted;
	
	gSpecialTripleJump = levelState->specialTripleJump;
	sYoshiDead = levelState->yoshiDead;
	
	gRandomSeed16 = levelState->randState;
	gRandomCalls = levelState->randCalls;
	
	sPowerMeterHUD = levelState->hudAnim;
	sPowerMeterStoredHealth = levelState->powerMeterStoredHealth;
	sPowerMeterVisibleTimer = levelState->powerMeterVisibleTimer;
	
	gWarpTransDelay = levelState->warpTransDelay;
	gFBSetColor = levelState->fbSetColor;
	gWarpTransFBSetColor = levelState->warpFbSetColor;
	gWarpTransRed = levelState->warpRed;
	gWarpTransGreen = levelState->warpGreen;
	gWarpTransBlue = levelState->warpBlue;
	memcpy(sTransitionColorFadeCount,levelState->transitionColorFadeCount,sizeof(u8)*4);
	memcpy(sTransitionTextureFadeCount,levelState->transitionTextureFadeCount,sizeof(u16)*2);
	
	gPauseScreenMode = levelState->pauseScreenMode;
	gSaveOptSelectIndex = levelState->saveOptSelectIndex;
	sCurrPlayMode = levelState->currPlayMode;
	gMenuMode = levelState->menuMode;
	sTransitionTimer = levelState->transitionTimer;
	
	// TODO: serialize this better
	sTransitionUpdate = levelState->transitionUpdate;
	
	gCurrCreditsEntry = levelState->currCreditsEntry;
	sTimerRunning = levelState->timerRunning;
	sWarpCheckpointIsActive = levelState->warpCheckpointIsActive;
	gTimeStopState = levelState->timeStopState;
	
	gMarioOnMerryGoRound = levelState->marioOnMerryGoRound;
	gMarioCurrentRoom = levelState->marioCurrentRoom;
	sPrevCheckMarioRoom = levelState->prevCheckMarioRoom;
	if (gMarioCurrentRoom==0)
		gMarioCurrentRoom = sPrevCheckMarioRoom;
	gMarioShotFromCannon = levelState->marioShotFromCannon;
	
	sNumActiveFirePiranhaPlants = levelState->numActivePiranhaPlants;
	sNumKilledFirePiranhaPlants = levelState->numKilledPiranhaPlants;
	sMontyMoleKillStreak = levelState->montyMoleKillStreak;
	sMontyMoleLastKilledPosX = levelState->montyMoleLastKilledPosX;
	sMontyMoleLastKilledPosY = levelState->montyMoleLastKilledPosY;
	sMontyMoleLastKilledPosZ = levelState->montyMoleLastKilledPosZ;
	
	sDisplayingDoorText = levelState->displayingDoorText;
	sJustTeleported = levelState->justTeleported;
	gDddPaintingStatus = levelState->dddPaintingStatus;
	gWDWWaterLevelChanging = levelState->wdwWaterLevelChanging;
	gWdwWaterLevelSet = levelState->wdwWaterLevelSet;
	
	gAPressCounter = levelState->aPressCount;
	
	gSparklePhase = levelState->sparklePhase;
	sSwimStrength = levelState->swimStrength;
	
	gEnvironmentLevels[0] = levelState->envLevels[0];
	gEnvironmentLevels[2] = levelState->envLevels[1];
	
	load_env_buffer(levelState);
	
	if (gEnvironmentRegions!=NULL){
		gEnvironmentRegions[6] = levelState->envRegionHeights[0];
		gEnvironmentRegions[12] = levelState->envRegionHeights[1];
		gEnvironmentRegions[18] = levelState->envRegionHeights[2];
	}
}

void load_game_init_state(const GameInitState* state){
	configSkipIntro = state->introSkip;
	configIntroSkipTiming = state->introSkipTiming;
	configNonstop = state->nonstop;
	configNoInvisibleWalls = state->noInvisibleWalls;
	configStageText = state->stageText;
	
	gPracticeSubStatus = PRACTICE_OP_DEFAULT;
	configYellowStars = FALSE;
}

void load_level_init_state(const LevelInitState* initState){
	gHudFlash = FALSE;
	
	memcpy(sHandheldShakeSpline,initState->handheldShakeSpline,sizeof(sHandheldShakeSpline));
	gCurrSaveFileNum = initState->currSaveFileNum;
	gSaveBuffer.files[gCurrSaveFileNum-1][0] = initState->saveFile;
	gSaveBuffer.menuData[0].soundMode = initState->soundMode;
	gGlobalTimer = initState->globalTimer;
	gLastButtons = initState->lastButtons;
	gLastCompletedCourseNum = initState->lastCompletedCourseNum;
	gLastCompletedStarNum = initState->lastCompletedStarNum;
	gCurrActNum = initState->currActNum;
	gCurrCourseNum = initState->currCourseNum;
	gSavedCourseNum = initState->savedCourseNum;
	gShouldNotPlayCastleMusic = initState->shouldNotPlayCastleMusic;
	sSelectionFlags = initState->camSelectionFlags;
	configNonstop = initState->nonstop;
	configSkipIntro = initState->introSkip;
	
	gTHIWaterDrained = initState->THIWaterDrained;
	gTTCSpeedSetting = initState->TTCSpeedSetting;
	gCCMEnteredSlide = initState->CCMEnteredSlide;
	sPssSlideStarted = initState->pssSlideStarted;
	gDddPaintingStatus = initState->dddPaintingStatus;
	
	gLastLevelNum = initState->lastLevelNum;
	
	gPracticeSubStatus = initState->practiceSubStatus;
	configStageText = initState->practiceStageText;
	
	gSpecialTripleJump = initState->specialTripleJump;
	sYoshiDead = initState->yoshiDead;
	gRandomSeed16 = initState->randState;
	gRandomCalls = initState->randCalls;
	
	gMarioStates[0].numStars = initState->numStars;
	gMarioStates[0].numLives = initState->numLives;
	gMarioStates[0].health = initState->health;
	
	switch (initState->warpCheckpointType){
		case WARP_CHECKPOINT_STATE_NONE:
			gWarpCheckpoint.actNum = 1;
			gWarpCheckpoint.courseNum = COURSE_NONE;
			break;
		case WARP_CHECKPOINT_STATE_LLL:
			gWarpCheckpoint.actNum = 1;
			gWarpCheckpoint.courseNum = COURSE_LLL;
			gWarpCheckpoint.levelID = 22;
			gWarpCheckpoint.areaNum = 2;
			gWarpCheckpoint.warpNode = 0xA;
			break;
		case WARP_CHECKPOINT_STATE_SSL:
			gWarpCheckpoint.actNum = 1;
			gWarpCheckpoint.courseNum = COURSE_SSL;
			gWarpCheckpoint.levelID = 8;
			gWarpCheckpoint.areaNum = 2;
			gWarpCheckpoint.warpNode = 0xA;
			break;
		case WARP_CHECKPOINT_STATE_TTM:
			gWarpCheckpoint.actNum = 1;
			gWarpCheckpoint.courseNum = COURSE_TTM;
			gWarpCheckpoint.levelID = 36;
			gWarpCheckpoint.areaNum = 2;
			gWarpCheckpoint.warpNode = 0xA;
			break;
		default:
			assert(FALSE);
			break;
	}
	
	// reset dialog star counter
	gMarioStates[0].prevNumStarsForDialog = gMarioStates[0].numStars;
	
	gBodyStates[0].heldObjLastPosition[0] = initState->holp[0];
	gBodyStates[0].heldObjLastPosition[1] = initState->holp[1];
	gBodyStates[0].heldObjLastPosition[2] = initState->holp[2];
	
	gMarioStates[0].slideYaw = initState->slideYaw;
	gMarioStates[0].twirlYaw = initState->twirlYaw;
	gMarioStates[0].slideVelX = initState->slideVelX;
	gMarioStates[0].slideVelZ = initState->slideVelZ;
	
	gEnvironmentLevels[0] = initState->envLevels[0];
	gEnvironmentLevels[2] = initState->envLevels[1];
	
	sSwimStrength = initState->swimStrength;
	gSparklePhase = initState->sparklePhase;
	
	gMenuHoldKeyIndex = initState->menuHoldKeyIndex;
	gMenuHoldKeyTimer = initState->menuHoldKeyTimer;
	
	update_save_file_vars();
}

void load_post_level_init_state(const LevelInitState* initState){
	gGlobalTimer = initState->globalTimer;
	gRandomSeed16 = initState->randState;
	gRandomCalls = initState->randCalls;
	gLastButtons = initState->lastButtons;
	
	gEnvironmentLevels[0] = initState->envLevels[0];
	gEnvironmentLevels[2] = initState->envLevels[1];
	
	if (gEnvironmentRegions!=NULL){
		gEnvironmentRegions[6] = initState->envRegionHeights[0];
		gEnvironmentRegions[12] = initState->envRegionHeights[1];
		gEnvironmentRegions[18] = initState->envRegionHeights[2];
		gWdwWaterLevelSet = 1;
	}
}

void load_dialog_state(const DialogState* dialogState){
	gHudFlash = dialogState->hudFlash;
	gDialogCourseActNum = dialogState->dialogCourseActNum;
	gLastDialogLineNum = dialogState->lastDialogLineNum;
	gDialogID = dialogState->dialogId;
	gDialogColorFadeTimer = dialogState->dialogColorFadeTimer;
	gDialogTextAlpha = dialogState->dialogTextAlpha;
	gDialogResponse = dialogState->dialogResponse;
	gDialogVariable = dialogState->dialogVar;
	gCutsceneMsgXOffset = dialogState->cutsceneMsgX;
	gCutsceneMsgYOffset = dialogState->cutsceneMsgY;
	
	gDialogBoxState = dialogState->dialogBoxState;
	gDialogBoxOpenTimer = dialogState->dialogBoxOpenTimer;
	gDialogBoxScale = dialogState->dialogBoxScale;
	gDialogScrollOffsetY = dialogState->dialogScrollOffsetY;
	gDialogBoxType = dialogState->dialogBoxType;
	gLastDialogPageStrPos = dialogState->lastDialogPageStrPos;
	gDialogTextPos = dialogState->dialogTextPos;
	gDialogLineNum = dialogState->dialogLineNum;
	gLastDialogResponse = dialogState->lastDialogResponse;
	gMenuHoldKeyIndex = dialogState->menuHoldKeyIndex;
	gMenuHoldKeyTimer = dialogState->menuHoldKeyTimer;
	gCourseDoneMenuTimer = dialogState->courseDoneMenuTimer;
	gCourseCompleteCoins = dialogState->courseCompleteCoins;
	gCourseCompleteCoinsEqual = dialogState->courseCompleteCoinsEqual;
}

#define MUSIC_NONE 0xFFFF

static void apply_sound_flags(const SoundState* state){
	if (state->soundFlags&SOUND_FLAG_SILENT){
		lower_background_noise(SOUND_FLAG_SILENT);
	} else {
		raise_background_noise(SOUND_FLAG_SILENT);
	}
	
	if (state->soundFlags&SOUND_FLAG_QUIET){
		lower_background_noise(SOUND_FLAG_QUIET);
	} else {
		raise_background_noise(SOUND_FLAG_QUIET);
	}
	
	if (state->backgroundSoundDisabled){
		disable_background_sound();
	} else {
		enable_background_sound();
	}
	
	//if (sCurrentMusic!=state->musicParam2){
	//	set_background_music(state->musicParam1, state->musicParam2, 0);
	//}
}

void load_sound_state(const SoundState* soundState){
	sPlayingInfiniteStairs = soundState->infiniteStairs;
	
	if (gUnbreakMusic){
		sCurrentMusic = MUSIC_NONE;
		sCurrentCapMusic = MUSIC_NONE;
		sCurrentShellMusic = MUSIC_NONE;
		gUnbreakMusic = FALSE;
	}
	
	if (soundState->currentMusic!=sCurrentMusic)
		set_background_music(soundState->musicParam1,soundState->currentMusic,2);
	
	if (soundState->currentCapMusic!=MUSIC_NONE && soundState->currentCapMusic!=sCurrentCapMusic){
		play_cap_music(soundState->currentCapMusic);
	} else if (sCurrentCapMusic!=MUSIC_NONE && soundState->currentCapMusic==MUSIC_NONE){
		stop_cap_music();
		set_background_music(soundState->musicParam1,soundState->musicParam2,2);
	}
	if (soundState->currentShellMusic!=sCurrentShellMusic && soundState->currentShellMusic!=sCurrentShellMusic){
		play_shell_music();
	} else if (sCurrentShellMusic!=MUSIC_NONE && soundState->currentShellMusic==MUSIC_NONE){
		stop_shell_music();
		set_background_music(soundState->musicParam1,soundState->musicParam2,2);
	}
	
	apply_sound_flags(soundState);
}

void load_practice_state(const PracticeState* practiceState){
	gLastWarpDest = practiceState->lastWarpDest;
	gSectionTimer = practiceState->sectionTimer;
	gSectionTimerResult = practiceState->sectionTimerResult;
	
	if (gCurrGhostData&&gGhostDataIndex==practiceState->ghostIndex){
		gCurrGhostFrame = practiceState->ghostFrame;
		gCurrGhostArea = practiceState->ghostArea;
		gGhostAreaCounter = practiceState->ghostAreaCounter;
	} else if (gCurrGhostData){
		//ghost_data_free(gCurrGhostData);
		//gCurrGhostData = NULL;
		gCurrGhostFrame = NULL;
		gCurrGhostArea = NULL;
		gGhostAreaCounter = 0;
	}
	
	gPracticeSubStatus = practiceState->practiceSubStatus;
	configStageText = practiceState->practiceStageText;
	configSkipIntro = practiceState->introSkip;
}

void load_state(const SaveState* state){
	load_all_objects(&state->objState);
	load_level_state(&state->levelState);
	load_dialog_state(&state->dialogState);
	load_camera_state(&state->camState);
	load_sound_state(&state->soundState);
	load_practice_state(&state->practiceState);
	
	if (state->replayState!=NULL){
		if (gPracticeRecordingReplay!=NULL){
			gPracticeRecordingReplay->flags |= REPLAY_FLAG_RESET;
			archive_replay(gPracticeRecordingReplay);
		}
		gPracticeRecordingReplay = copy_replay(state->replayState,NULL,0);
		gPracticeRecordingReplay->flags |= REPLAY_FLAG_SAVE_STATED;
		init_replay_record_at_end(gPracticeRecordingReplay);
	}
}

u32 get_state_size(const SaveState* state){
	u32 s = 0;
	s += sizeof(SaveState);
	const AreaData* areaData = &state->objState.areaData;
	for (u32 i=0;i<8;++i){
		if (areaData->macroMask&(1<<i)){
			s += areaData->macroData[i].length*sizeof(s16);
		}
		if (areaData->respawnMask&(1<<i)){
			s += areaData->respawnData[i].length*sizeof(u32);
		}
	}
	
	if (state->levelState.snowParticleCount>state->levelState.bubbleParticleCount)
		s += state->levelState.snowParticleCount*sizeof(struct EnvFxParticle);
	else
		s += state->levelState.bubbleParticleCount*sizeof(struct EnvFxParticle);
	
	return s;
}

#define STATE_MAGIC "ms\x01\x02"
#define LEVEL_INIT_MAGIC "li\x03\x04"
#define STATE_VERSION 0x1
#define LEVEL_INIT_VERSION 0x1

#define SERIALIZE_VAR(v) fwrite(&(v),sizeof(v),1,file)
#define DESERIALIZE_VAR(v) fread(&(v),sizeof(v),1,file)

void serialize_state(FILE* file,const SaveState* state){
	fwrite(STATE_MAGIC,4,1,file);
	u32 version = STATE_VERSION;
	fwrite(&version,sizeof(u32),1,file);
}

u8 deserialize_state(FILE* file,SaveState* state){
	char magic[4];
	fread(magic,4,1,file);
	if (memcmp(magic,STATE_MAGIC,4)!=0){
		printf("Bad state magic! %u %u %u %u\n",magic[0],magic[1],magic[2],magic[3]);
		return FALSE;
	}
	
	u32 version = 0;
	fread(&version,sizeof(u32),1,file);
	if (version!=STATE_VERSION){
		printf("Bad state version! %u\n",version);
		return FALSE;
	}
	
	return TRUE;
}

void serialize_level_init_state(FILE* file,const LevelInitState* initState){
	SERIALIZE_VAR(initState->loc.levelNum);
	SERIALIZE_VAR(initState->loc.areaIdx);
	SERIALIZE_VAR(initState->loc.nodeId);
	SERIALIZE_VAR(initState->loc.arg);
	
	SERIALIZE_VAR(initState->globalTimer);
	
	SERIALIZE_VAR(initState->saveFile);
	SERIALIZE_VAR(initState->soundMode);
	
	SERIALIZE_VAR(initState->handheldShakeSpline[0]);
	SERIALIZE_VAR(initState->handheldShakeSpline[1]);
	SERIALIZE_VAR(initState->handheldShakeSpline[2]);
	SERIALIZE_VAR(initState->handheldShakeSpline[3]);
	
	SERIALIZE_VAR(initState->holp[0]);
	SERIALIZE_VAR(initState->holp[1]);
	SERIALIZE_VAR(initState->holp[2]);
	
	SERIALIZE_VAR(initState->currSaveFileNum);
	SERIALIZE_VAR(initState->currActNum);
	SERIALIZE_VAR(initState->currCourseNum);
	SERIALIZE_VAR(initState->savedCourseNum);
	SERIALIZE_VAR(initState->THIWaterDrained);
	SERIALIZE_VAR(initState->TTCSpeedSetting);
	SERIALIZE_VAR(initState->CCMEnteredSlide);
	SERIALIZE_VAR(initState->lastButtons);
	SERIALIZE_VAR(initState->shouldNotPlayCastleMusic);
	SERIALIZE_VAR(initState->specialTripleJump);
	SERIALIZE_VAR(initState->pssSlideStarted);
	SERIALIZE_VAR(initState->yoshiDead);
	SERIALIZE_VAR(initState->lastCompletedCourseNum);
	SERIALIZE_VAR(initState->lastCompletedStarNum);
	SERIALIZE_VAR(initState->dddPaintingStatus);
	SERIALIZE_VAR(initState->practiceSubStatus);
	SERIALIZE_VAR(initState->practiceStageText);
	SERIALIZE_VAR(initState->nonstop);
	SERIALIZE_VAR(initState->introSkip);
	
	SERIALIZE_VAR(initState->lastLevelNum);
	
	SERIALIZE_VAR(initState->randState);
	SERIALIZE_VAR(initState->randCalls);
	SERIALIZE_VAR(initState->numStars);
	SERIALIZE_VAR(initState->camSelectionFlags);
	SERIALIZE_VAR(initState->numLives);
	SERIALIZE_VAR(initState->health);
	SERIALIZE_VAR(initState->warpCheckpointType);
	SERIALIZE_VAR(initState->envLevels[0]);
	SERIALIZE_VAR(initState->envLevels[1]);
	
	SERIALIZE_VAR(initState->envRegionHeights[0]);
	SERIALIZE_VAR(initState->envRegionHeights[1]);
	SERIALIZE_VAR(initState->envRegionHeights[2]);
	
	SERIALIZE_VAR(initState->swimStrength);
	SERIALIZE_VAR(initState->sparklePhase);
	
	SERIALIZE_VAR(initState->slideYaw);
	SERIALIZE_VAR(initState->twirlYaw);
	SERIALIZE_VAR(initState->slideVelX);
	SERIALIZE_VAR(initState->slideVelZ);
	
	SERIALIZE_VAR(initState->menuHoldKeyIndex);
	SERIALIZE_VAR(initState->menuHoldKeyTimer);
}

u8 deserialize_level_init_state(FILE* file,LevelInitState* initState){
	DESERIALIZE_VAR(initState->loc.levelNum);
	DESERIALIZE_VAR(initState->loc.areaIdx);
	DESERIALIZE_VAR(initState->loc.nodeId);
	DESERIALIZE_VAR(initState->loc.arg);
	
	DESERIALIZE_VAR(initState->globalTimer);
	
	DESERIALIZE_VAR(initState->saveFile);
	DESERIALIZE_VAR(initState->soundMode);
	
	DESERIALIZE_VAR(initState->handheldShakeSpline[0]);
	DESERIALIZE_VAR(initState->handheldShakeSpline[1]);
	DESERIALIZE_VAR(initState->handheldShakeSpline[2]);
	DESERIALIZE_VAR(initState->handheldShakeSpline[3]);
	
	DESERIALIZE_VAR(initState->holp[0]);
	DESERIALIZE_VAR(initState->holp[1]);
	DESERIALIZE_VAR(initState->holp[2]);
	
	DESERIALIZE_VAR(initState->currSaveFileNum);
	DESERIALIZE_VAR(initState->currActNum);
	DESERIALIZE_VAR(initState->currCourseNum);
	DESERIALIZE_VAR(initState->savedCourseNum);
	DESERIALIZE_VAR(initState->THIWaterDrained);
	DESERIALIZE_VAR(initState->TTCSpeedSetting);
	DESERIALIZE_VAR(initState->CCMEnteredSlide);
	
	DESERIALIZE_VAR(initState->lastButtons);
	
	DESERIALIZE_VAR(initState->shouldNotPlayCastleMusic);
	DESERIALIZE_VAR(initState->specialTripleJump);
	DESERIALIZE_VAR(initState->pssSlideStarted);
	DESERIALIZE_VAR(initState->yoshiDead);
	DESERIALIZE_VAR(initState->lastCompletedCourseNum);
	DESERIALIZE_VAR(initState->lastCompletedStarNum);
	DESERIALIZE_VAR(initState->dddPaintingStatus);
	DESERIALIZE_VAR(initState->practiceSubStatus);
	DESERIALIZE_VAR(initState->practiceStageText);
	DESERIALIZE_VAR(initState->nonstop);
	DESERIALIZE_VAR(initState->introSkip);
	
	DESERIALIZE_VAR(initState->lastLevelNum);
	
	DESERIALIZE_VAR(initState->randState);
	DESERIALIZE_VAR(initState->randCalls);
	DESERIALIZE_VAR(initState->numStars);
	DESERIALIZE_VAR(initState->camSelectionFlags);
	DESERIALIZE_VAR(initState->numLives);
	DESERIALIZE_VAR(initState->health);
	DESERIALIZE_VAR(initState->warpCheckpointType);
	DESERIALIZE_VAR(initState->envLevels[0]);
	DESERIALIZE_VAR(initState->envLevels[1]);
	
	DESERIALIZE_VAR(initState->envRegionHeights[0]);
	DESERIALIZE_VAR(initState->envRegionHeights[1]);
	DESERIALIZE_VAR(initState->envRegionHeights[2]);
	
	DESERIALIZE_VAR(initState->swimStrength);
	DESERIALIZE_VAR(initState->sparklePhase);
	
	DESERIALIZE_VAR(initState->slideYaw);
	DESERIALIZE_VAR(initState->twirlYaw);
	DESERIALIZE_VAR(initState->slideVelX);
	DESERIALIZE_VAR(initState->slideVelZ);
	
	DESERIALIZE_VAR(initState->menuHoldKeyIndex);
	DESERIALIZE_VAR(initState->menuHoldKeyTimer);
	
	return TRUE;
}

void serialize_game_init_state(FILE* file,const GameInitState* state){
	SERIALIZE_VAR(state->introSkip);
	SERIALIZE_VAR(state->introSkipTiming);
	SERIALIZE_VAR(state->nonstop);
	SERIALIZE_VAR(state->noInvisibleWalls);
	SERIALIZE_VAR(state->stageText);
}

u8 deserialize_game_init_state(FILE* file,GameInitState* state){
	DESERIALIZE_VAR(state->introSkip);
	DESERIALIZE_VAR(state->introSkipTiming);
	DESERIALIZE_VAR(state->nonstop);
	DESERIALIZE_VAR(state->noInvisibleWalls);
	DESERIALIZE_VAR(state->stageText);
	return TRUE;
}
