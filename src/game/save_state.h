#ifndef SAVE_STATE_H
#define SAVE_STATE_H

#include <PR/ultratypes.h>

#include "object_list_processor.h"
#include "camera.h"
#include "hud.h"
#include "level_update.h"
#include "save_file.h"

typedef struct {
	struct Camera areaCams[8];
	struct LakituState lakituState;
	struct PlayerCameraState playerCam;
	struct CameraFOVStatus fovState;
	struct TransitionInfo camModeTransition;
	struct CutsceneVariable cutsceneVars[10];
	struct CameraStoredInfo parallelTrackInfo;
	struct CameraStoredInfo cameraStoreCUp;
	struct CameraStoredInfo cameraStoreCutscene;
	struct ParallelTrackingPoint* parallelTrackPath;
	struct PlayerGeometry marioGeometry;
	
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
	u16 macroMask;
	u16 respawnMask;
	MacroObjectData macroData[8];
	RespawnInfoData respawnData[8];
} AreaData;

// list of saved objects
typedef struct {
	struct Object objectPoolCopy[OBJECT_POOL_CAPACITY];
	struct ObjectNode freeList;
	struct ObjectNode objectListArray[16];
	struct MarioState marioState;
	struct MarioBodyState marioBodyState;
	struct SpawnInfo marioSpawnInfo;
	struct HudDisplay hudState;
	struct Object* marioPlatform;
	AreaData areaData;
	u8 objectMemoryPoolData[0x800];
	s8 doorAdjacentRooms[120];
	u32 lastButtons;
} ObjectState;

// misc level state
typedef struct {
	struct WarpDest warpDest;
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
} LevelState;

typedef struct {
	struct WarpDest loc;
	u32 globalTimer;
	struct SaveBuffer saveBuffer;
	s16 currSaveFileNum;
	s16 currActNum;
	s16 currCourseNum;
	s16 savedCourseNum;
	s16 THIWaterDrained;
	s16 TTCSpeedSetting;
	s16 CCMEnteredSlide;
	s16 WDWWaterLevelChanging;
	s16 health;
	s8 shouldNotPlayCastleMusic;
	u8 specialTripleJump;
	u8 pssSlideStarted;
	s8 yoshiDead;
	u8 lastCompletedCourseNum;
	u8 lastCompletedStarNum;
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
	s32 sectionTimer;
	s32 sectionTimerResult;
} PracticeState;

// list of saved objects
typedef struct {
	ObjectState objState;
	LevelInitState initState;
	LevelState levelState;
	DialogState dialogState;
	CameraState camState;
	SoundState soundState;
	PracticeState practiceState;
} SaveState;


extern u8 gHasStateSaved;
extern SaveState gCurrSaveState;

void init_state(SaveState*);
void save_level_init_state(LevelInitState*,struct WarpDest*);
void save_state(SaveState*);
void load_level_init_state(const LevelInitState*);
void load_state(const SaveState*);
u32 get_state_size(const SaveState* state);

#endif