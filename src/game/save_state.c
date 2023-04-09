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
#include "practice.h"

#include <string.h>
#include <stdio.h>

u8 gHasStateSaved = FALSE;
SaveState gCurrSaveState;

extern struct Object *gMarioPlatform;

extern struct PlayerGeometry sMarioGeometry;
extern struct CameraFOVStatus sFOVState;
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

extern f32 gCameraZoomDist;

extern struct ParallelTrackingPoint* sParTrackPath;
extern struct CutsceneVariable sCutsceneVars[10];
extern struct CameraStoredInfo sParTrackTransOff;
extern struct CameraStoredInfo sCameraStoreCUp;
extern struct CameraStoredInfo sCameraStoreCutscene;

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

extern struct SaveBuffer gSaveBuffer;

void init_state(SaveState* state){
	for (u32 i=0;i<8;++i){
		state->objState.areaData.macroData[i].data = NULL;
		state->objState.areaData.respawnData[i].data = NULL;
	}
}

void save_macro_objects(MacroObjectData* macroData,const s16* data){
	if (macroData->data!=NULL){
		free(macroData->data);
	}
	
	s32 l = 0;
	s32 presetID;
	const s16* node = data;
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

void save_area_data(AreaData* areaData){
	areaData->macroMask = 0;
	areaData->respawnMask = 0;
	
	for (u32 i=0;i<8;++i){
		if (gAreaData[i].macroObjects!=NULL){
			areaData->macroMask |= (1<<i);
			save_macro_objects(&areaData->macroData[i],gAreaData[i].macroObjects);
		}
		if (gAreaData[i].objectSpawnInfos!=NULL){
			areaData->respawnMask |= (1<<i);
			save_respawn_info(&areaData->respawnData[i],gAreaData[i].objectSpawnInfos);
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
		spawnInfos->behaviorArg = respawnData->data[i];
		spawnInfos = spawnInfos->next;
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
	}
}

void save_all_objects(ObjectState* objState){
	memcpy(&objState->objectPoolCopy[0],&gObjectPool[0],sizeof(struct Object)*OBJECT_POOL_CAPACITY);
	objState->freeList = gFreeObjectList;
	memcpy(&objState->objectListArray[0],&gObjectListArray[0],sizeof(struct ObjectNode)*16);
	memcpy(&objState->objectMemoryPoolData[0],gObjectMemoryPool,0x800);
	objState->marioState = gMarioStates[0];
	objState->hudState = gHudDisplay;
	objState->marioBodyState = gBodyStates[0];
	objState->marioSpawnInfo = gPlayerSpawnInfos[0];
	objState->marioPlatform = gMarioPlatform;
	memcpy(&objState->doorAdjacentRooms[0],&gDoorAdjacentRooms[0][0],sizeof(s8)*120);
	save_area_data(&objState->areaData);
}

void save_camera_state(CameraState* camState){
	for (int i=0;i<8;++i){
		if (gAreaData[i].camera!=NULL){
			camState->areaCams[i] = *gAreaData[i].camera;
		}
	}
	
	memcpy(&camState->cutsceneVars[0],&sCutsceneVars[0],sizeof(struct CutsceneVariable)*10);
	
	camState->lakituState = gLakituState;
	camState->playerCam = gPlayerCameraState[0];
	camState->marioGeometry = sMarioGeometry;
	camState->fovState = sFOVState;
	camState->camModeTransition = sModeTransition;
	camState->cameraMovementFlags = gCameraMovementFlags;
	camState->cutsceneTimer = gCutsceneTimer;
	camState->cutsceneShot = sCutsceneShot;
	camState->camSelectionFlags = sSelectionFlags;
	camState->rotateFlags = s2ndRotateFlags;
	camState->cSideButtonYaw = sCSideButtonYaw;
	camState->mode8BaseYaw = s8DirModeBaseYaw;
	camState->mode8YawOffset = s8DirModeYawOffset;
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
	levelState->specialWarpLevelDest = sSpecialWarpLevelNum;
	levelState->delayedWarpOp = sDelayedWarpOp;
	levelState->delayedWarpTimer = sDelayedWarpTimer;
	levelState->sourceWarpNodeId = sSourceWarpNodeId;
	levelState->delayedWarpArg = sDelayedWarpArg;
	levelState->warpTransition = gWarpTransition;
	levelState->warpCheckpoint = gWarpCheckpoint;
	
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
}

void save_level_init_state(LevelInitState* initState){
	initState->loc = gLastWarpDest;
	
	initState->currSaveFileNum = gCurrSaveFileNum;
	initState->saveBuffer = gSaveBuffer;
	initState->globalTimer = gGlobalTimer;
	initState->lastCompletedCourseNum = gLastCompletedCourseNum;
	initState->lastCompletedStarNum = gLastCompletedStarNum;
	initState->currActNum = gCurrActNum;
	initState->currCourseNum = gCurrCourseNum;
	initState->savedCourseNum = gSavedCourseNum;
	initState->shouldNotPlayCastleMusic = gShouldNotPlayCastleMusic;
	
	initState->THIWaterDrained = gTHIWaterDrained;
	initState->TTCSpeedSetting = gTTCSpeedSetting;
	initState->CCMEnteredSlide = gCCMEnteredSlide;
	initState->WDWWaterLevelChanging = gWDWWaterLevelChanging;
	initState->pssSlideStarted = sPssSlideStarted;
	
	initState->specialTripleJump = gSpecialTripleJump;
	initState->yoshiDead = sYoshiDead;
	initState->health = gMarioStates[0].health;
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

void save_state(SaveState* state){
	save_all_objects(&state->objState);
	save_level_init_state(&state->initState);
	save_level_state(&state->levelState);
	save_dialog_state(&state->dialogState);
	save_camera_state(&state->camState);
	save_sound_state(&state->soundState);
	gHasStateSaved = TRUE;
}


void load_all_objects(const ObjectState* objState){
	memcpy(&gObjectPool[0],&objState->objectPoolCopy[0],sizeof(struct Object)*OBJECT_POOL_CAPACITY);
	gFreeObjectList = objState->freeList;
	memcpy(&gObjectListArray[0],&objState->objectListArray[0],sizeof(struct ObjectNode)*16);
	memcpy(gObjectMemoryPool,&objState->objectMemoryPoolData[0],0x800);
	gMarioStates[0] = objState->marioState;
	gMarioObject = gMarioStates[0].marioObj;
	gHudDisplay = objState->hudState;
	gBodyStates[0] = objState->marioBodyState;
	s32 animId = gMarioStates[0].marioObj->header.gfx.unk38.animID;
	struct Animation* targetAnim = gMarioStates[0].animation->targetAnim;
	if (load_patchable_table(gMarioStates[0].animation,animId)) {
        targetAnim->values = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->values);
        targetAnim->index = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->index);
    }
	gPlayerSpawnInfos[0] = objState->marioSpawnInfo;
	gMarioPlatform = objState->marioPlatform;
	load_area_data(&objState->areaData);
	memcpy(&gDoorAdjacentRooms[0][0],&objState->doorAdjacentRooms[0],sizeof(s8)*120);
}

void load_camera_state(const CameraState* camState){
	for (int i=0;i<8;++i){
		if (gAreaData[i].camera!=NULL){
			*gAreaData[i].camera = camState->areaCams[i];
		}
	}
	
	memcpy(&sCutsceneVars[0],&camState->cutsceneVars[0],sizeof(struct CutsceneVariable)*10);
	
	gLakituState = camState->lakituState;
	gPlayerCameraState[0] = camState->playerCam;
	sMarioGeometry = camState->marioGeometry;
	sFOVState = camState->fovState;
	sModeTransition = camState->camModeTransition;
	gCameraMovementFlags = camState->cameraMovementFlags;
	gCutsceneTimer = camState->cutsceneTimer;
	sCutsceneShot = camState->cutsceneShot;
	sSelectionFlags = camState->camSelectionFlags;
	s2ndRotateFlags = camState->rotateFlags;
	s8DirModeBaseYaw = camState->mode8BaseYaw;
	s8DirModeYawOffset = camState->mode8YawOffset;
	sCSideButtonYaw = camState->cSideButtonYaw;
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
}

void load_level_init_state(const LevelInitState* initState){
	gCurrSaveFileNum = initState->currSaveFileNum;
	gSaveBuffer = initState->saveBuffer;
	gGlobalTimer = initState->globalTimer;
	gLastCompletedCourseNum = initState->lastCompletedCourseNum;
	gLastCompletedStarNum = initState->lastCompletedStarNum;
	gCurrActNum = initState->currActNum;
	gCurrCourseNum = initState->currCourseNum;
	gSavedCourseNum = initState->savedCourseNum;
	gShouldNotPlayCastleMusic = initState->shouldNotPlayCastleMusic;
	
	gTHIWaterDrained = initState->THIWaterDrained;
	gTTCSpeedSetting = initState->TTCSpeedSetting;
	gCCMEnteredSlide = initState->CCMEnteredSlide;
	gWDWWaterLevelChanging = initState->WDWWaterLevelChanging;
	sPssSlideStarted = initState->pssSlideStarted;
	
	gSpecialTripleJump = initState->specialTripleJump;
	sYoshiDead = initState->yoshiDead;
	gMarioStates[0].health = initState->health;
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
}

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
	
	if (gUnbreakMusic){
		sCurrentMusic = 0;
		gUnbreakMusic = FALSE;
	}
	
	set_background_music(state->musicParam1, state->musicParam2, 0);
}

void load_sound_state(const SoundState* soundState){
	sPlayingInfiniteStairs = soundState->infiniteStairs;
	sCurrentMusic = soundState->currentMusic;
	sCurrentShellMusic = soundState->currentShellMusic;
	sCurrentCapMusic = soundState->currentCapMusic;
	apply_sound_flags(soundState);
}

void load_state(const SaveState* state){
	load_all_objects(&state->objState);
	load_level_init_state(&state->initState);
	load_level_state(&state->levelState);
	load_dialog_state(&state->dialogState);
	load_camera_state(&state->camState);
	load_sound_state(&state->soundState);
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
	
	return s;
}
/*
void reset_respawn_info_tracker(void){
	sCurrRespawnInfoIndex = 0;
}

void respawn_info_tracker(struct Object* obj,u8 bits){
	RespawnInfoState* info = &sRespawnInfos[sCurrRespawnInfoIndex++];
	info->objIndex = obj-gObjectPool[0];
	info->areaIndex = gCurrAreaIndex;
	info->bits = bits;
}*/