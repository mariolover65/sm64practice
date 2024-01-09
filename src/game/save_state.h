#ifndef SAVE_STATE_H
#define SAVE_STATE_H

#include <PR/ultratypes.h>

#include "object_list_processor.h"
#include "camera.h"
#include "hud.h"
#include "level_update.h"
#include "save_file.h"
#include "replay.h"

#include <stdio.h>

enum WarpCheckpointStateType {
	WARP_CHECKPOINT_STATE_NONE,
	WARP_CHECKPOINT_STATE_LLL,
	WARP_CHECKPOINT_STATE_SSL,
	WARP_CHECKPOINT_STATE_TTM
};

typedef struct {
	struct Camera areaCams[8];
	struct LakituState lakituState;
	struct PlayerCameraState playerCam;
	struct CameraFOVStatus fovState;
	struct ModeTransitionInfo modeInfo;
	struct TransitionInfo modeTransition;
	struct CutsceneVariable cutsceneVars[10];
	struct CameraStoredInfo parallelTrackInfo;
	struct CameraStoredInfo cameraStoreCUp;
	struct CameraStoredInfo cameraStoreCutscene;
	struct ParallelTrackingPoint* parallelTrackPath;
	struct PlayerGeometry marioGeometry;
	struct HandheldShakePoint handheldShakeSpline[4];
	
	s16 cameraMovementFlags;
	s16 cutsceneTimer;
	s16 cutsceneShot;
	s16 camSelectionFlags;
	s16 rotateFlags;
	s16 cSideButtonYaw;
	s16 mode8BaseYaw;
	s16 mode8YawOffset;
	s16 areaYaw;
	s16 areaYawChange;
	s16 avoidYawVel;
	s16 yawSpeed;
	s16 yawAfterDoorCutscene;
	s16 modeOffsetYaw;
	s16 spiralStairsYawOffset;
	s16 cUpCameraPitch;
	s16 lakituDist;
	s16 lakituPitch;
	s16 camStatusFlags;
	s16 handheldShakePitch;
	s16 handheldShakeYaw;
	s16 handheldShakeRoll;
	s16 horizCamHold;
	f32 panDist;
	f32 cannonYOffset;
	f32 zoomDist;
	u32 parallelTrackIndex;
	s32 currLevelArea;
	u32 prevLevel;
	
	Vec3f fixedModeBasePosition;
	Vec3f castleEntranceOffset;
	
	u8 recentCutscene;
	u8 framesSinceLastCutscene;
	u8 cutsceneDialogResponse;
	s32 objectCutsceneDone;
	u32 objectCutsceneSpawn;
	
	struct Object* cutsceneFocusObj;
	struct Object* secondaryCutsceneFocusObj;
} CameraState;

typedef struct {
	u32 length;
	u32* data;
} RespawnInfoData;

typedef struct {
	u32 length;
	s16* data;
} MacroObjectData;

typedef struct {
	u32 length;
	struct Object** data;
} WarpObjectData;

typedef struct {
	u16 macroMask;
	u16 respawnMask;
	u16 warpMask;
	MacroObjectData macroData[8];
	RespawnInfoData respawnData[8];
	WarpObjectData warpData[8];
} AreaData;

// list of saved objects
typedef struct {
	struct Object objectPoolCopy[OBJECT_POOL_CAPACITY];
	struct ObjectNode freeList;
	struct ObjectNode objectListArray[16];
	struct HudDisplay hudState;
	struct MarioState marioState;
	struct MarioBodyState marioBodyState;
	struct SpawnInfo marioSpawnInfo;
	struct Object* marioPlatform;
	AreaData areaData;
	u8 objectMemoryPoolData[0x800];
	s8 doorAdjacentRooms[120];
	u16 lastButtons;
} ObjectState;

// misc level state
typedef struct {
	struct WarpDest warpDest;
	struct WarpDest loc;
	u32 globalTimer;
	struct SaveFile saveFile;
	u16 soundMode;
	s16 currSaveFileNum;
	s16 currActNum;
	s16 currCourseNum;
	s16 savedCourseNum;
	s16 THIWaterDrained;
	s16 TTCSpeedSetting;
	s16 CCMEnteredSlide;
	s8 shouldNotPlayCastleMusic;
	u8 specialTripleJump;
	u8 pssSlideStarted;
	s8 yoshiDead;
	u8 nonstop;
	u8 lastCompletedCourseNum;
	u8 lastCompletedStarNum;
	s32 lastLevelNum;
	
	u16 areaUpdateCounter;
	
	s16 specialWarpLevelDest;
	s16 delayedWarpOp;
	s16 delayedWarpTimer;
	s16 sourceWarpNodeId;
	s32 delayedWarpArg;
	struct WarpTransition warpTransition;
	struct WarpCheckpoint warpCheckpoint;
	struct PowerMeterHUD hudAnim;
	s16 powerMeterStoredHealth;
	s32 powerMeterVisibleTimer;
	
	u16 randState;
	u16 randCalls;
	
	s16 warpTransDelay;
	u32 fbSetColor;
	u32 warpFbSetColor;
	u8 warpRed;
	u8 warpGreen;
	u8 warpBlue;
	u8 transitionColorFadeCount[4];
	u16 transitionTextureFadeCount[2];
	
	s16 pauseScreenMode;
	s16 saveOptSelectIndex;
	s16 menuMode;
	u32 timeStopState;
	s16 currPlayMode;
	s16 transitionTimer;
	void (*transitionUpdate)(s16 *);
	struct CreditsEntry* currCreditsEntry;
	s8 timerRunning;
	
	s16 marioOnMerryGoRound;
	s16 marioCurrentRoom;
	s16 prevCheckMarioRoom;
	s16 marioShotFromCannon;
	s32 numActivePiranhaPlants;
	s32 numKilledPiranhaPlants;
	s32 montyMoleKillStreak;
	f32 montyMoleLastKilledPosX;
	f32 montyMoleLastKilledPosY;
	f32 montyMoleLastKilledPosZ;
	
	u8 displayingDoorText;
	u8 justTeleported;
	s8 warpCheckpointIsActive;
	s8 dddPaintingStatus;
	s16 wdwWaterLevelChanging;
	s32 wdwWaterLevelSet;
	
	u32 aPressCount;
	
	s32 envLevels[2];
	s16 envRegionHeights[3];
	s16 sparklePhase;
	s16 swimStrength;
	
	s16 snowParticleCount;
	s16 bubbleParticleCount;
	struct EnvFxParticle* envBuffer;
} LevelState;

// state needed to start a full-game replay
typedef struct {
	u8 introSkip;
	u8 introSkipTiming;
	u8 nonstop;
	u8 noInvisibleWalls;
	u8 stageText;
} GameInitState;

// only the state needed for a level init
typedef struct {
	struct WarpDest loc;
	
	u32 globalTimer;
	struct SaveFile saveFile;
	u16 soundMode;
	struct HandheldShakePoint handheldShakeSpline[4];
	Vec3f holp;
	s16 currSaveFileNum;
	s16 currActNum;
	s16 currCourseNum;
	s16 savedCourseNum;
	s16 THIWaterDrained;
	s16 TTCSpeedSetting;
	s16 CCMEnteredSlide;
	u16 lastButtons;
	s8 shouldNotPlayCastleMusic;
	u8 specialTripleJump;
	u8 pssSlideStarted;
	s8 yoshiDead;
	u8 lastCompletedCourseNum;
	u8 lastCompletedStarNum;
	u8 nonstop;
	u8 introSkip;
	s8 dddPaintingStatus;
	
	u8 menuHoldKeyIndex;
	u8 menuHoldKeyTimer;
	
	s32 lastLevelNum;
	
	u8 practiceSubStatus;
	u8 practiceStageText;
	
	u16 randState;
	u16 randCalls;
	s16 numStars;
	s16 camSelectionFlags;
	u16 sparklePhase;
	s8 numLives;
	u8 warpCheckpointType;
	s16 health;
	s32 envLevels[2];
	s16 envRegionHeights[3];
	s16 swimStrength;
	
	s16 slideYaw;
	s16 twirlYaw;
	f32 slideVelX;
	f32 slideVelZ;
} LevelInitState;

typedef struct {
	s8 hudFlash;
	s8 dialogCourseActNum;
	s8 lastDialogLineNum;
	s16 dialogId;
	u16 dialogColorFadeTimer;
	u16 dialogTextAlpha;
	s32 dialogResponse;
	s32 dialogVar;
	s16 cutsceneMsgX;
	s16 cutsceneMsgY;
	
	s8 dialogBoxState;
	f32 dialogBoxOpenTimer;
	f32 dialogBoxScale;
	s16 dialogScrollOffsetY;
	s8 dialogBoxType;
	s16 lastDialogPageStrPos;
	s16 dialogTextPos;
	s8 dialogLineNum;
	s8 lastDialogResponse;
	u8 menuHoldKeyIndex;
	u8 menuHoldKeyTimer;
	s32 courseDoneMenuTimer;
	s32 courseCompleteCoins;
	s32 courseCompleteCoinsEqual;
} DialogState;

typedef struct {
	u8 soundFlags;
	u8 backgroundSoundDisabled;
	u8 infiniteStairs;
	u16 currentMusic;
	u16 currentShellMusic;
	u16 currentCapMusic;
	u16 bankMask;
	u16 musicParam1;
	u16 musicParam2;
} SoundState;

typedef struct {
	struct WarpDest lastWarpDest;
	s32 sectionTimer;
	s32 sectionTimerResult;
	
	GhostFrame* ghostFrame;
	GhostAreaChange* ghostArea;
	u32 ghostAreaCounter;
	u32 ghostIndex;
	
	u8 practiceSubStatus;
	u8 practiceStageText;
	u8 introSkip;
} PracticeState;

// list of saved objects
typedef struct {
	ObjectState objState;
	LevelState levelState;
	DialogState dialogState;
	CameraState camState;
	SoundState soundState;
	PracticeState practiceState;
	Replay* replayState;
} SaveState;

typedef struct SaveStateList SaveStateList;
struct SaveStateList {
	SaveStateList* next;
	SaveStateList* prev;
	SaveState* state;
};


extern u8 gHasStateSaved;
extern SaveStateList* gCurrSaveStateSlot;
extern SaveStateList* gCurrLoadStateSlot;
extern u32 gCurrSaveStateIndex;
extern u32 gCurrLoadStateIndex;

void init_state(SaveState*);
SaveState* alloc_state(void);
void free_state(SaveState*);
void alloc_state_at_save_slot(void);
void init_state_list(void);
void free_state_list(void);
void clear_save_states(void);
void save_slot_increment(void);
void save_slot_decrement(void);
void load_slot_increment(void);
void load_slot_decrement(void);

void save_game_init_state(GameInitState*);
void save_level_init_state(LevelInitState*,struct WarpDest*);
void save_state(SaveState*);
void load_game_init_state(const GameInitState*);
void load_level_init_state(const LevelInitState*);
void load_post_level_init_state(const LevelInitState*);
void load_state(const SaveState*);
u32 get_state_size(const SaveState* state);

void serialize_state(FILE*,const SaveState*);
u8 deserialize_state(FILE*,SaveState*);

void serialize_level_init_state(FILE*,const LevelInitState*);
u8 deserialize_level_init_state(FILE*,LevelInitState*);

void serialize_game_init_state(FILE*,const GameInitState*);
u8 deserialize_game_init_state(FILE*,GameInitState*);

#endif