#include <PR/ultratypes.h>

#include "sm64.h"
#include "practice.h"
#include "stats.h"
#include "save_state.h"
#include "level_update.h"
#include "game_init.h"
#include "ingame_menu.h"
#include "gfx_dimensions.h"
#include "pc/configfile.h"
#include "segment2.h"
#include "print.h"
#include "engine/math_util.h"
#include "sound_init.h"
#include "audio/external.h"

#include "pc/fs/fs.h"

#include <stdio.h>
#include <string.h>

#define REPLAY_MAX_PATH 512

enum ReplayFileHeaderType {
	HEADER_TYPE_REPLAY_FILE,
	HEADER_TYPE_DIR
};

typedef struct {
	u8 type;
	const char* path;
	ReplayFileHeader header;
} ReplayPathItem;

static fs_pathlist_t sCurrReplayDirPaths;
static char sCurrReplayPath[REPLAY_MAX_PATH];

static ReplayPathItem* sReplayHeaderStorage = NULL;
static u32 sReplayHeaderStorageCap = 0;
static u32 sReplayHeaderStorageSize = 0;

static char sReplaysStaticDir[REPLAY_MAX_PATH];

char gTextInputString[256];
u8 gTextInputSubmitted = FALSE;

static char sReplayNameTempStorage[256];

enum ReplayBrowserMode {
	REPLAY_BROWSER_BROWSE,
	REPLAY_BROWSER_SAVE_REPLAY,
	REPLAY_BROWSER_CREATE_DIR,
	REPLAY_BROWSER_OVERWRITE_CONFIRM,
	REPLAY_BROWSER_DELETE_CONFIRM,
};

static u8 sReplayBrowserMode = REPLAY_BROWSER_BROWSE;

extern u8 texture_a_button[];
extern s16 gMenuMode;
extern s32 gCourseDoneMenuTimer;
extern u16 gRandomSeed16;
extern u16 gRandomCalls;

u8 gDebug = FALSE;

u8 gDisableRendering = FALSE;

u8 gIsRTA = TRUE;

static u8 sStageRTAPrimed = FALSE;
static u8 sStageRTAAct = 1;
static s32 sStageRTAStarsCompleted = 0;
static s32 sStageRTAStarsTarget = 7;

struct WarpDest gPracticeDest;
struct WarpDest gLastWarpDest;
u8 gPracticeWarping = FALSE;
u8 gNoStarSelectWarp = FALSE;
u8 gSaveStateWarpDelay = 0;
u8 gRenderPracticeMenu = 0;
u8 gPracticeMenuPage = 0;

static u8 sLoadSaveStateNow = FALSE;

u8 gPlaybackPrimed = 0;

s32 gSectionTimer = 0;
s32 gSectionTimerResult = NULL_SECTION_TIMER_RESULT;

static s32 sSectionTimerLock = 0;
static s32 sSectionTimerLockTime = 0;

u8 gFrameAdvance = 0;
static u8 sFrameAdvanceStorage = 0;

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

void replay_header_storage_clear(void){
	sReplayHeaderStorageSize = 0;
}

ReplayPathItem* replay_header_storage_alloc(void){
	if (sReplayHeaderStorageSize==sReplayHeaderStorageCap){
		sReplayHeaderStorageCap *= 2;
		sReplayHeaderStorage = realloc(sReplayHeaderStorage,sizeof(ReplayPathItem)*sReplayHeaderStorageCap);
	}
	return &sReplayHeaderStorage[sReplayHeaderStorageSize++];
}

void replay_header_storage_add_dir(const char* path){
	ReplayPathItem* header = replay_header_storage_alloc();
	header->type = HEADER_TYPE_DIR;
	header->path = path;
}

void replay_header_storage_add_replay(const char* path){
	FILE* file = fopen(path,"rb");
	if (!file) return;
	
	ReplayPathItem* item = replay_header_storage_alloc();
	
	if (!deserialize_replay_header(file,&item->header)){
		--sReplayHeaderStorageSize;
		printf("could not deserialize header!\n%s\n",path);
		fclose(file);
		return;
	}
	
	item->path = path;
	item->type = HEADER_TYPE_REPLAY_FILE;
	fclose(file);
}

u8 is_replay_file(const char* path){
	const char* ext = sys_file_extension(path);
	if (ext==NULL) return FALSE;
	
	if (strncmp(ext,PRACTICE_REPLAY_EXT,3)!=0) return FALSE;
	return TRUE;
}

u8 path_exists(const char* path){
	return fs_sys_file_exists(path) || fs_sys_dir_exists(path);
}

static u8 sWillEnumerate = FALSE;

void enumerate_replays(void){
	static char tempPath[REPLAY_MAX_PATH];
	
	replay_header_storage_clear();
	fs_pathlist_free(&sCurrReplayDirPaths);
	
	if (!sCurrReplayPath[0])
		snprintf(tempPath,sizeof(tempPath),"%s",sReplaysStaticDir);
	else
		snprintf(tempPath,sizeof(tempPath),"%s/%s",sReplaysStaticDir,sCurrReplayPath);
	if (!path_exists(tempPath)){
		return;
	}
	sCurrReplayDirPaths = fs_sys_enumerate_with_dir(tempPath);
	for (s32 i=0;i<sCurrReplayDirPaths.numpaths;++i){
		const char* path = sCurrReplayDirPaths.paths[i];
		if (fs_sys_dir_exists(path)){
			replay_header_storage_add_dir(path);
		} else if (is_replay_file(path)){
			replay_header_storage_add_replay(path);
		}
	}
}

void practice_replay_enter_dir(const char* dirName){
	u32 end = strnlen(sCurrReplayPath,REPLAY_MAX_PATH);
	u32 copyLen = strnlen(dirName,REPLAY_MAX_PATH);
	if (end+1+copyLen+1>=REPLAY_MAX_PATH) return;
	
	if (end!=0){
		sCurrReplayPath[end++] = '/';
	}
	
	memcpy(&sCurrReplayPath[end],dirName,copyLen+1);
	sWillEnumerate = TRUE;
}

void practice_replay_pop_dir(void){
	char* lastSlash = strrchr(sCurrReplayPath,'/');
	if (!lastSlash){
		sCurrReplayPath[0] = 0;
	} else {
		*lastSlash = 0;
	}
	
	sWillEnumerate = TRUE;
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
	if (gSectionTimerResult==NULL_SECTION_TIMER_RESULT)
		gSectionTimerResult = gSectionTimer;
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
	if (!sFrameAdvanceStorage)
		sFrameAdvanceStorage = gFrameAdvance;
	gFrameAdvance = FALSE;
	gNoStarSelectWarp = TRUE;
}

bool can_load_save_state(const SaveState* state){
	if (state->levelState.loc.levelNum==1) return TRUE;
	
	return gCurrLevelNum==state->levelState.loc.levelNum&&
		gCurrAreaIndex==state->levelState.loc.areaIdx&&
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
}

void save_replay(void){
	if (!gCurrRecordingReplay)
		return;
	
	end_replay_record();
	if (gPracticeFinishedReplay!=NULL)
		free_replay(gPracticeFinishedReplay);
	
	gPracticeFinishedReplay = gPracticeRecordingReplay;
	gPracticeRecordingReplay = NULL;
}

void start_recording_replay(void){
	if (gPracticeRecordingReplay){
		free_replay(gPracticeRecordingReplay);
	}
	
	gPracticeRecordingReplay = alloc_replay();
	init_replay_record(gPracticeRecordingReplay,TRUE,configPracticeType,sStageRTAStarsTarget);
	add_frame();
}

void start_replay_playback(void){
	if (gPracticeRecordingReplay){
		free_replay(gPracticeRecordingReplay);
		gPracticeRecordingReplay = NULL;
		gCurrRecordingReplay = NULL;
	}
	
	if (gPracticeFinishedReplay->state.type==LEVEL_INIT){
		load_post_level_init_state(gPracticeFinishedReplay->state.levelState);
	} else if (gPracticeFinishedReplay->state.type==SAVE_STATE){
		practice_load_state(gPracticeFinishedReplay->state.saveState);
	}
	
	init_playing_replay(gPracticeFinishedReplay);
	gPlaybackPrimed = FALSE;
	update_replay();
	replay_overwrite_inputs(gPlayer1Controller);
	gIsRTA = FALSE;
	
	gDisableRendering = FALSE;
	if (gReplaySkipToEnd){
		gDisableRendering = TRUE;
	}
	
	section_timer_start();
	gSectionTimer = 1;
	
	configPracticeType = gPracticeFinishedReplay->practiceType;
	if (configPracticeType==PRACTICE_TYPE_STAGE)
		sStageRTAStarsTarget = gPracticeFinishedReplay->starCount;
}

void practice_choose_level(void){
	gCurrPlayingReplay = NULL;
	gCurrRecordingReplay = NULL;
	
	clear_transit_state();
	gNoStarSelectWarp = FALSE;
	
	if (configPracticeType==PRACTICE_TYPE_XCAM||
		configPracticeType==PRACTICE_TYPE_STAR_GRAB){
		gIsRTA = TRUE;
	} else if (configPracticeType==PRACTICE_TYPE_STAGE){
		section_timer_start();
		sStageRTAPrimed = TRUE;
		gIsRTA = TRUE;
	} else if (configPracticeType==PRACTICE_TYPE_GAME){
		gIsRTA = FALSE;
	}
	
	stats_level_reset(gPracticeDest.levelNum,gPracticeDest.areaIdx);
}

extern f32 gPaintingMarioYEntry;

s32 gPracticeWDWHeight = 0;
static u8 sSetWDWHeight = FALSE;
static f32 sWDWPaintingHeights[3] = {1000.0f,1500.0f,2000.0f};

void practice_update_wdw_height(void){
	if (gCurrRecordingReplay&&gCurrRecordingReplay->state.type==LEVEL_INIT){
		LevelInitState* state = (LevelInitState*)gCurrRecordingReplay->state.levelState;
		
		state->envRegionHeights[0] = gEnvironmentRegions[6];
		state->envRegionHeights[1] = gEnvironmentRegions[12];
		state->envRegionHeights[2] = gEnvironmentRegions[18];
	}
}

void practice_level_init(void){
	if (sSetWDWHeight){
		gPaintingMarioYEntry = sWDWPaintingHeights[gPracticeWDWHeight];
		sSetWDWHeight = FALSE;
	}
	
	if (gPlaybackPrimed&&gPracticeWarping){
		start_replay_playback();
		
	} else if (gCurrPlayingReplay==NULL){
		if (configPracticeType==PRACTICE_TYPE_XCAM||
			configPracticeType==PRACTICE_TYPE_STAR_GRAB){
			section_timer_start();
			gSectionTimer = 1;
			start_recording_replay();
		}
		
		if (sStageRTAPrimed){
			section_timer_start();
			gSectionTimer = 1;
			start_recording_replay();
			
			sStageRTAAct = gCurrActNum;
			// fix 0 act bug
			if (sStageRTAAct==0) sStageRTAAct = 1;
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

void practice_game_win(void){
	if (configPracticeType==PRACTICE_TYPE_STAR_GRAB||
		configPracticeType==PRACTICE_TYPE_XCAM||
		configPracticeType==PRACTICE_TYPE_GAME){
		section_timer_finish();
		save_replay();
	}
}

void practice_file_select(void){
	sStageRTAStarsCompleted = 0;
	if (configPracticeType==PRACTICE_TYPE_GAME&&configSkipIntro){
		section_timer_start();
		if (!gPlaybackPrimed&&!gCurrPlayingReplay)
			start_recording_replay();
	}
}

extern u16 sCurrentMusic;

void practice_reset(void){
	gPracticeDest = gLastWarpDest;
	gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
	clear_transit_state();
	if (configResetMusic)
		sCurrentMusic = 0;
	
	gIsRTA = TRUE;
	if (configPracticeType==PRACTICE_TYPE_XCAM||
		configPracticeType==PRACTICE_TYPE_STAR_GRAB){
		
	} else if (configPracticeType==PRACTICE_TYPE_STAGE){
		gCurrActNum = sStageRTAAct;
		sStageRTAStarsCompleted = 0;
		sStageRTAPrimed = TRUE;
	} else if (configPracticeType==PRACTICE_TYPE_GAME){
		soft_reset();
	}
	
	stats_level_reset(gCurrLevelNum,gCurrAreaIndex);
	if (gCurrLevelNum==LEVEL_WDW)
		sSetWDWHeight = TRUE;
}

void replay_warp(void){
	LevelInitState* levelInit = (LevelInitState*)gPracticeFinishedReplay->state.levelState;
	load_level_init_state(levelInit);
	gPracticeDest = levelInit->loc;
	gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
	gCurrActNum = levelInit->currActNum;
	sStageRTAStarsCompleted = 0;
	if (gCurrLevelNum!=levelInit->loc.levelNum||gCurrAreaIndex!=levelInit->loc.areaIdx){
		gDisableRendering = TRUE;
	}
	clear_transit_state();
	gPlaybackPrimed = TRUE;
	if (gCurrRecordingReplay){
		free_replay(gPracticeRecordingReplay);
		gPracticeRecordingReplay = NULL;
		gCurrRecordingReplay = NULL;
	}
}

static u8 sReplaySkipLastLevel = 0;
static u8 sReplaySkipLastArea = 0;

void practice_update(void){
	u8 canLoad = can_load_save_state(&gCurrSaveState);
	
	if (sLoadSaveStateNow&&canLoad){
		gDisableRendering = FALSE;
		sLoadSaveStateNow = FALSE;
		practice_load_state(&gCurrSaveState);
		if (sFrameAdvanceStorage){
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
	
	if ((gPlayer1Controller->buttonPressed & R_JPAD)){
		save_state(&gCurrSaveState);
		gHasStateSaved = TRUE;
		printf("gCurrSaveState size: %u\n",get_state_size(&gCurrSaveState));
	} else if ((gPlayer1Controller->buttonPressed & U_JPAD)&&gHasStateSaved){
		if (canLoad){
			gDisableRendering = FALSE;
			practice_load_state(&gCurrSaveState);
		} else {
			gPracticeDest = gCurrSaveState.levelState.loc;
			gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
			gPracticeDest.nodeId = 0xA;
			gPracticeDest.arg = 0;
			gCurrActNum = gCurrSaveState.levelState.currActNum;
			gSaveStateWarpDelay = 1;
			clear_transit_state();
			gDisableRendering = TRUE;
		}
	}
	
	if (!gFrameAdvance){
		if ((gPlayer1Controller->buttonDown & R_TRIG) &&
			(gPlayer1Controller->buttonPressed & L_TRIG) && gPracticeFinishedReplay && !gPracticeWarping){
			// playback replay
			replay_warp();
		} else if ((gPlayer1Controller->buttonPressed & L_TRIG) && !gPracticeWarping){
			// level reset
			if (gCurrRecordingReplay){
				gCurrRecordingReplay->flags |= REPLAY_FLAG_RESET;
				save_replay();
			}
			
			if (configUseGhost)
				ghost_start_playback();
			
			if (gCurrPlayingReplay)
				gCurrPlayingReplay = NULL;
			
			practice_reset();
		} else if (gCurrPlayingReplay){
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
}

void save_state_update(void){
	u8 canLoad = can_load_save_state(&gCurrSaveState);
	
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

const char* replay_length_info(void){
	if (!gPracticeFinishedReplay){
		set_timer_text(0);
		return gTimerText;
	}
	
	set_timer_text(gPracticeFinishedReplay->length);
	return gTimerText;
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

void export_practice_replay(const char* path,const Replay* replay){
	if (!replay) return;
	
	FILE* file = fopen(path,"wb");
	if (!file){
		printf("Could not open file!\nPath: %s\n",path);
	}
	
	serialize_replay(file,replay);
	fclose(file);
}

void import_practice_replay(const char* path,Replay** replay){
	FILE* file = fopen(path,"rb");
	if (!file){
		printf("Could not open file!\n");
		return;
	}
	
	if (*replay){
		free_replay(*replay);
	}
	
	*replay = alloc_replay();
	
	if (!deserialize_replay(file,*replay)){
		printf("Could not deserialize!\n");
		free_replay(*replay);
		*replay = NULL;
	} else {
		printf("Loaded replay of length %u\n",(*replay)->length);
	}
	
	fclose(file);
}

void reexport_practice_replay(const char* path){
	Replay* replay = NULL;
	import_practice_replay(path,&replay);
	export_practice_replay(path,replay);
	free_replay(replay);
	printf("Reexported %s\n",path);
}

static void print_curr_replay_path(char* path,const char* name){
	// 512 max replay path length
	if (sCurrReplayPath[0])
		snprintf(path,REPLAY_MAX_PATH,"%s/%s/%s",sReplaysStaticDir,sCurrReplayPath,name);
	else
		snprintf(path,REPLAY_MAX_PATH,"%s/%s",sReplaysStaticDir,name);
}

static s32 sSelectFileIndex = 0;
static s32 sReplaysScroll = 0;

u8 replay_delete_at_cursor(void){
	if (sSelectFileIndex<0 || sSelectFileIndex>=(s32)sReplayHeaderStorageSize) return FALSE;
	const ReplayPathItem* item = &sReplayHeaderStorage[sSelectFileIndex];
	sWillEnumerate = TRUE;
	if (item->type==HEADER_TYPE_DIR){
		return fs_sys_rmdir(item->path);
	}
	return fs_sys_remove_file(item->path);
}

u8 replay_export_as(const char* name,const Replay* replay,u8 overwrite){
	if (!replay) return FALSE;
	if (strchr(name,'/')) return FALSE;
	const char* ext = sys_file_extension(name);
	
	static char path[REPLAY_MAX_PATH];
	
	print_curr_replay_path(path,name);
	
	if (!ext){
		strncat(path,".pre",sizeof(path)-1);
	}
	
	if (fs_sys_dir_exists(path))
		return FALSE;
	
	if (fs_sys_file_exists(path)){
		if (!overwrite){
			memcpy(sReplayNameTempStorage,name,256);
			sReplayBrowserMode = REPLAY_BROWSER_OVERWRITE_CONFIRM;
			return TRUE;
		} else {
			if (!fs_sys_remove_file(path))
				return FALSE;
		}
	}
	
	export_practice_replay(path,replay);
	return TRUE;
}

u8 replay_create_dir(const char* name){
	if (!name) return FALSE;
	if (strchr(name,'/')) return FALSE;
	
	static char path[REPLAY_MAX_PATH];
	
	print_curr_replay_path(path,name);
	
	if (fs_sys_dir_exists(path))
		return TRUE;
	
	if (fs_sys_file_exists(path))
		return FALSE;
	
	return fs_sys_mkdir(path);
}

typedef struct {
	const char* name;
	u16 levelNum;
	u16 areaIdx;
	u16 nodeId;
} PracticeDest;

#define PRACTICE_LEVEL_COUNT 32

static const PracticeDest sPracticeDests[PRACTICE_LEVEL_COUNT] = {
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
	
	if (currLevel->levelNum!=LEVEL_CASTLE){
		stats_get_level_info(currLevel->levelNum,0,&time,&stars,&coins,&resets);
	} else {
		stats_get_level_info(currLevel->levelNum,currLevel->areaIdx,&time,&stars,&coins,&resets);
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

static void render_level_select(void){
	if (sInLevelStats){
		render_practice_level_stats();
		return;
	}
	
	gCurrTextScale = 1.25f;
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
	for (s32 i=0;i<PRACTICE_LEVEL_COUNT;++i){
		set_text_color(255,255,255,255);
		if (i==gPracticeStageSelect)
			set_text_color(255,0,0,255);
		
		x = i%2;
		x *= 100;
		x -= 50;
		render_shadow_text_string_at(175+x,SCREEN_HEIGHT-40-(i/2-sYScroll)*LEVEL_SPACING,sPracticeDests[i].name);
	}
	
	const PracticeDest* curr = &sPracticeDests[gPracticeStageSelect];
	
	if (gPlayer1Controller->buttonPressed & A_BUTTON){
		gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
		gPracticeDest.levelNum = curr->levelNum;
		gPracticeDest.areaIdx = curr->areaIdx;
		gPracticeDest.nodeId = curr->nodeId;
		if (gPracticeDest.levelNum==LEVEL_CASTLE&&gPracticeDest.areaIdx==1&&gPracticeDest.nodeId==0){
			gPracticeDest.arg = 6;
		} else {
			gPracticeDest.arg = 0;
		}
		gClosePracticeMenuNextFrame = TRUE;
		
		sFrameAdvanceStorage = gFrameAdvance;
		gFrameAdvance = FALSE;
		practice_choose_level();
		if (gPracticeDest.levelNum==LEVEL_WDW)
			sSetWDWHeight = TRUE;
	} else if (gPlayer1Controller->buttonPressed & START_BUTTON){
		sInLevelStats = TRUE;
	}
}

enum AngleDisplayType {
	ANGLE_DISPLAY_U16,
	ANGLE_DISPLAY_HEX,
	ANGLE_DISPLAY_DEGREES,
	ANGLE_DISPLAY_COUNT
};

typedef enum {
	PRACTICE_OPTION_BOOL = 0,
	PRACTICE_OPTION_INT,
	PRACTICE_OPTION_UINT,
	PRACTICE_OPTION_ENUM,
	
	PRACTICE_INFO,
	PRACTICE_BUTTON
} PracticeSettingType;

typedef enum {
	PRACTICE_PAGE_SETTINGS = 0,
	PRACTICE_PAGE_INTERACT
} PracticePageType;

typedef struct {
	const char* name;
	const char** values;
	PracticeSettingType type;
	u32 index;
	void* var;
} PracticeSetting;

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

typedef void ButtonFunc(void);
typedef const char* InfoFunc(void);

#define PRACTICE_GAME_SETTINGS_COUNT 9
static PracticeSetting sPracticeGameSettings[PRACTICE_GAME_SETTINGS_COUNT] = {
	{
		"Nonstop",
		sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configNonstop
	},
	{
		"Intro skip",
		sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configSkipIntro
	},
	{
		"Ghost enabled",
		sBoolValues,
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
		sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configGhostDistanceFade
	},
	{
		"Practice type",
		sPracticeTypeValues,
		PRACTICE_OPTION_ENUM,
		0,
		&configPracticeType
	},
	{
		"Stage star target",
		NULL,
		PRACTICE_OPTION_UINT,
		7,
		&sStageRTAStarsTarget
	},
	{
		"Reset music",
		sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configResetMusic
	},
	{
		"Disable music",
		sBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configDisableMusic
	}
};

static void start_text_entry(void){
	gTextInputString[0] = 0;
	gTextInputSubmitted = FALSE;
}

static void button_export_current_replay(void){
	gPracticeMenuPage = PRACTICE_REPLAYS;
	sReplayBrowserMode = REPLAY_BROWSER_SAVE_REPLAY;
	start_text_entry();
}

static void button_reexport_all_replays(void){
	fs_pathlist_t replayPaths = fs_sys_enumerate(sReplaysStaticDir,TRUE);
	for (s32 i=0;i<replayPaths.numpaths;++i){
		if (is_replay_file(replayPaths.paths[i])){
			reexport_practice_replay(replayPaths.paths[i]);
		}
	}
}

#define PRACTICE_REPLAY_SETTINGS_COUNT 3
static PracticeSetting sPracticeReplaySettings[PRACTICE_REPLAY_SETTINGS_COUNT] = {
	{
		"Replay length",
		NULL,
		PRACTICE_INFO,
		0,
		replay_length_info
	},
	{
		"Export replay",
		NULL,
		PRACTICE_BUTTON,
		0,
		button_export_current_replay
	},
	{
		"Reexport all replays",
		NULL,
		PRACTICE_BUTTON,
		0,
		button_reexport_all_replays
	}
};

#define PRACTICE_STATS_SETTINGS_COUNT 2
static PracticeSetting sPracticeStatsSettings[PRACTICE_STATS_SETTINGS_COUNT] = {
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

#define PRACTICE_HUD_SETTINGS_COUNT 11
static PracticeSetting sPracticeHUDSettings[PRACTICE_HUD_SETTINGS_COUNT] = {
	{
		"Vel",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowVel
	},
	{
		"Pos",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowPos
	},
	{
		"Angle",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowAngle
	},
	{
		"Angle Display",
		sAngleDisplayValues,
		PRACTICE_OPTION_ENUM,
		0,
		&configAngleDisplayType
	},
	{
		"Max Height",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowMaxHeight
	},
	{
		"HOLP",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowHOLP
	},
	
	{
		"Wallkick Frame",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowWallkickFrame
	},
	{
		"Wallkick Angle",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowWallkickAngle
	},
	{
		"Bowser Info",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowBowserInfo
	},
	{
		"RNG Info",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowRNGInfo
	},
	{
		"Angle Efficiency",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowEfficiency
	}
};

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

s32 gPracticeSubStatus = 0;

#define PRACTICE_STAGE_SETTINGS_COUNT 4
static PracticeSetting sPracticeStageSettings[PRACTICE_STAGE_SETTINGS_COUNT] = {
	{
		"TTC Speed",
		sTTCSpeedValues,
		PRACTICE_OPTION_ENUM,
		0,
		&gTTCSpeedSetting
	},
	{
		"WDW Water",
		sWDWWaterValues,
		PRACTICE_OPTION_ENUM,
		0,
		&gPracticeWDWHeight
	},
	{
		"DDD Sub",
		sDDDSubValues,
		PRACTICE_OPTION_ENUM,
		0,
		&gPracticeSubStatus
	},
	{
		"Stage Text",
		sDDDSubValues,
		PRACTICE_OPTION_ENUM,
		0,
		&configStageText
	}
};

#define PRACTICE_PAGE_COUNT 5
static PracticePage sPracticePages[PRACTICE_PAGE_COUNT] = {
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

static s32 sPracticePageSelect = 0;
static u8 sInPracticePage = 1;

static void wrap_index(PracticeSetting* setting){
	s32 l = 0;
	const char** it = setting->values;
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
}

void load_all_settings(void){
	for (s32 i=0;i<PRACTICE_PAGE_COUNT;++i){
		PracticePage* page = &sPracticePages[i];
		for (s32 j=0;j<page->size;++j){
			load_setting(&page->settings[j]);
		}
	}
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

static void render_practice_settings(void){
	if (gPlayer1Controller->buttonPressed & R_JPAD){
		sInPracticePage = 1;
	}
	
	if (gPlayer1Controller->buttonPressed & L_JPAD){
		sInPracticePage = 0;
	}
	
	gCurrTextScale = 1.25f;
	for (s32 i=0;i<PRACTICE_PAGE_COUNT;++i){
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
			currPage->index += 1;
			currPage->index %= currPage->size;
			calculate_settings_scroll();
		}
		
		if (gPlayer1Controller->buttonPressed & U_JPAD){
			currPage->index -= 1;
			currPage->index += currPage->size;
			currPage->index %= currPage->size;
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
			if (gPlayer1Controller->buttonPressed & R_CBUTTONS){
				++(*(s32*)selected->var);
			}
			
			if (gPlayer1Controller->buttonPressed & L_CBUTTONS){
				--(*(s32*)selected->var);
			}
		} else if (selected->type==PRACTICE_OPTION_UINT){
			if (gPlayer1Controller->buttonPressed & R_CBUTTONS){
				++(*(s32*)selected->var);
			}
			
			if (gPlayer1Controller->buttonPressed & L_CBUTTONS){
				--(*(s32*)selected->var);
			}
			if (*(s32*)selected->var<0){
				*(s32*)selected->var = 0;
			}
		} else if (selected->type==PRACTICE_BUTTON){
			if (gPlayer1Controller->buttonPressed & A_BUTTON){
				((ButtonFunc*)selected->var)();
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
		char intStorage[24];
		const char* result;
		switch (currSetting->type){
			case PRACTICE_INFO:
				render_shadow_text_string_at(nameX,y,currSetting->name);
				if (highlighted)
					set_text_color(255,230,0,255);
				
				result = ((InfoFunc*)currSetting->var)();
				textWidth = get_text_width(result);
				render_shadow_text_string_at(opX-textWidth,y,result);
				break;
			case PRACTICE_BUTTON:
				if (highlighted)
					set_text_color(255,230,0,255);
				
				textWidth = get_text_width(currSetting->name);
				render_shadow_text_string_at(opX-textWidth,y,currSetting->name);
				break;
			case PRACTICE_OPTION_BOOL:
			case PRACTICE_OPTION_ENUM:
				render_shadow_text_string_at(nameX,y,currSetting->name);
				if (highlighted)
					set_text_color(255,0,0,255);
				
				result = currSetting->values[currSetting->index];
				textWidth = get_text_width(result);
				render_shadow_text_string_at(opX-textWidth,y,result);
				break;
			
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
}

void print_replay_time(char* buffer,size_t bufferSize,const ReplayPathItem* item){
	set_timer_text_small(item->header.length);
	if (item->header.flags & REPLAY_FLAG_RESET){
		snprintf(buffer,bufferSize,"%sr",gTimerText);
	} else {
		switch (item->header.practiceType){
			case PRACTICE_TYPE_XCAM:
				snprintf(buffer,bufferSize,"%sx",gTimerText);
				break;
			case PRACTICE_TYPE_STAR_GRAB:
				snprintf(buffer,bufferSize,"%sg",gTimerText);
				break;
			case PRACTICE_TYPE_GAME:
				snprintf(buffer,bufferSize,"%s",gTimerText);
				break;
			case PRACTICE_TYPE_STAGE:
				snprintf(buffer,bufferSize,"%s/%d",gTimerText,item->header.starCount);
				break;
		}
	}
}

#define PRACTICE_REPLAY_HEIGHT 16

static void calculate_replays_scroll(void){
	s32 yVal = SCREEN_HEIGHT-48-(sSelectFileIndex-sReplaysScroll)*PRACTICE_REPLAY_HEIGHT;
	if (yVal<40){
		sReplaysScroll += (40-yVal)/PRACTICE_REPLAY_HEIGHT;
	}
	
	if (yVal>SCREEN_HEIGHT-16-40){
		sReplaysScroll += ((SCREEN_HEIGHT-16-40)-yVal)/PRACTICE_REPLAY_HEIGHT;
	}
	if (sReplaysScroll<0){
		sReplaysScroll = 0;
	}
}

static void handle_replay_index(void){
	if (sReplayHeaderStorageSize==0){
		sSelectFileIndex = 0;
		return;
	}
	
	if (gPlayer1Controller->buttonPressed & U_JPAD){
		--sSelectFileIndex;
		sSelectFileIndex += sReplayHeaderStorageSize;
		sSelectFileIndex %= sReplayHeaderStorageSize;
		calculate_replays_scroll();
	}
	
	if (gPlayer1Controller->buttonPressed & D_JPAD){
		++sSelectFileIndex;
		sSelectFileIndex %= sReplayHeaderStorageSize;
		calculate_replays_scroll();
	}
}

static u8 check_will_enter(void){
	return (gPlayer1Controller->buttonPressed & R_JPAD)!=0 || (gPlayer1Controller->buttonPressed & A_BUTTON)!=0;
}

static u8 check_will_pop(void){
	return (gPlayer1Controller->buttonPressed & L_JPAD)!=0;
}

void render_practice_replays(void){
	gCurrTextScale = 1.25f;
	
	render_shadow_text_string_at(12,SCREEN_HEIGHT-32,"Replays");
	
	gCurrTextScale = 1.0f;
	//if (!sCurrReplayDirPaths.paths) return;
	
	s32 pathIndex = 0;
	static char buffer[128];
	u8 willPop = FALSE;
	u8 willEnter = FALSE;
	
	switch (sReplayBrowserMode){
		case REPLAY_BROWSER_BROWSE:
			handle_replay_index();
			if (gPlayer1Controller->buttonPressed & B_BUTTON){
				gClosePracticeMenuNextFrame = TRUE;
			} else if (check_will_pop()){
				willPop = TRUE;
			} else if (check_will_enter()){
				willEnter = TRUE;
			} else if ((gPlayer1Controller->buttonDown & Z_TRIG) && 
					   (gPlayer1Controller->buttonPressed & START_BUTTON) &&
					   sReplayHeaderStorageSize!=0){
				sReplayBrowserMode = REPLAY_BROWSER_DELETE_CONFIRM;
			} else if (!(gPlayer1Controller->buttonDown & Z_TRIG) && 
					   (gPlayer1Controller->buttonPressed & START_BUTTON)){
				sReplayBrowserMode = REPLAY_BROWSER_CREATE_DIR;
				start_text_entry();
			}
			break;
		case REPLAY_BROWSER_SAVE_REPLAY:
			handle_replay_index();
			snprintf(buffer,sizeof(buffer),"Enter replay name: %s",gTextInputString);
			render_shadow_text_string_at(70,SCREEN_HEIGHT-24,buffer);
			
			if (gTextInputSubmitted){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
				// check error
				replay_export_as(gTextInputString,gPracticeFinishedReplay,FALSE);
				sWillEnumerate = TRUE;
			} else if (gPlayer1Controller->buttonPressed & B_BUTTON){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
			} else if (gPlayer1Controller->buttonPressed & A_BUTTON){
				if (sReplayHeaderStorageSize>0&&sSelectFileIndex<(s32)sReplayHeaderStorageSize){
					sReplayBrowserMode = REPLAY_BROWSER_OVERWRITE_CONFIRM;
					const ReplayPathItem* item = &sReplayHeaderStorage[sSelectFileIndex];
					const char* fileName = sys_file_name(item->path);
					memcpy(sReplayNameTempStorage,fileName,strnlen(fileName,256));
				}
			} else if (check_will_pop()){
				willPop = TRUE;
			} else if (check_will_enter()){
				willEnter = TRUE;
			}
			break;
		case REPLAY_BROWSER_CREATE_DIR:
			handle_replay_index();
			snprintf(buffer,sizeof(buffer),"Dir to create: %s",gTextInputString);
			render_shadow_text_string_at(70,SCREEN_HEIGHT-24,buffer);
			
			if (gTextInputSubmitted){
				// handle error
				replay_create_dir(gTextInputString);
				sWillEnumerate = TRUE;
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
			} else if (gPlayer1Controller->buttonPressed & B_BUTTON){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
			} else if (check_will_pop()){
				willPop = TRUE;
			} else if (check_will_enter()){
				willEnter = TRUE;
			}
			break;
		case REPLAY_BROWSER_DELETE_CONFIRM:
			render_shadow_text_string_at(70,SCREEN_HEIGHT-24,"Delete? A/B");
			if (gPlayer1Controller->buttonPressed & B_BUTTON){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
			} else if (gPlayer1Controller->buttonPressed & A_BUTTON){
				replay_delete_at_cursor();
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
			} else if (check_will_pop()){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
				willPop = TRUE;
			}
			break;
		case REPLAY_BROWSER_OVERWRITE_CONFIRM:
			snprintf(buffer,sizeof(buffer),"Overwrite %s? A/B",sReplayNameTempStorage);
			render_shadow_text_string_at(70,SCREEN_HEIGHT-24,buffer);
			if (gPlayer1Controller->buttonPressed & B_BUTTON){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
			} else if (gPlayer1Controller->buttonPressed & A_BUTTON){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
				// check error
				replay_export_as(sReplayNameTempStorage,gPracticeFinishedReplay,TRUE);
				sWillEnumerate = TRUE;
			} else if (check_will_pop()){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
				willPop = TRUE;
			}
			break;
	}
	
	if (willPop){
		practice_replay_pop_dir();
		sSelectFileIndex = 0;
	}
	
	if (sWillEnumerate){
		sWillEnumerate = FALSE;
		enumerate_replays();
		calculate_replays_scroll();
		if (sSelectFileIndex>=(s32)sReplayHeaderStorageSize && sReplayHeaderStorageSize!=0){
			sSelectFileIndex = sReplayHeaderStorageSize-1;
		}
	}
	
	s32 xPos = 70;
	s32 yPos = SCREEN_HEIGHT-64+sReplaysScroll*PRACTICE_REPLAY_HEIGHT;
	s32 boxRightEdge = SCREEN_WIDTH-48;
	
	// current dir path
	set_text_color(255,255,255,255);
	snprintf(buffer,sizeof(buffer),"/%s",sCurrReplayPath);
	render_shadow_text_string_at(48,SCREEN_HEIGHT-48,buffer);
	
	s32 bottomY = SCREEN_HEIGHT-16;
	
	shade_screen_rect(48,48,SCREEN_WIDTH-48-48,bottomY-48);
	gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 48, 48, SCREEN_WIDTH-48, bottomY);
	
	s32 maxNameSize = 80;
	s32 maxTimeSize = 0;
	// size pass
	for (u32 i=0;i<sReplayHeaderStorageSize;++i){
		const ReplayPathItem* item = &sReplayHeaderStorage[i];
		const char* fileName = sys_file_name(item->path);
		s32 size = get_text_width(fileName);
		if (size>maxNameSize){
			maxNameSize = size;
		}
		
		if (item->type!=HEADER_TYPE_REPLAY_FILE){
			continue;
		}
		print_replay_time(buffer,sizeof(buffer),item);
		size = get_text_width(buffer);
		if (size>maxTimeSize){
			maxTimeSize = size;
		}
	}
	
	// dir pass
	for (u32 i=0;i<sReplayHeaderStorageSize;++i){
		const ReplayPathItem* item = &sReplayHeaderStorage[i];
		
		if (item->type!=HEADER_TYPE_DIR){
			continue;
		}
		
		if (yPos<-PRACTICE_REPLAY_HEIGHT){
			++pathIndex;
			continue;
		}
		
		const char* fileName = sys_file_name(item->path);
		
		if (sSelectFileIndex==pathIndex){
			set_text_color(255,0,0,255);
			render_shadow_text_string_at(xPos-16,yPos,"--");
			if (willEnter){
				practice_replay_enter_dir(fileName);
				sSelectFileIndex = 0;
			}
		}
		set_text_color(0,255,0,255);
		
		render_shadow_text_string_at(xPos,yPos,fileName);
		yPos -= PRACTICE_REPLAY_HEIGHT;
		++pathIndex;
	}
	
	// file pass
	for (u32 i=0;i<sReplayHeaderStorageSize;++i){
		const ReplayPathItem* item = &sReplayHeaderStorage[i];
		if (item->type!=HEADER_TYPE_REPLAY_FILE){
			continue;
		}
		
		if (yPos<-PRACTICE_REPLAY_HEIGHT){
			++pathIndex;
			continue;
		}
		
		const char* fileName = sys_file_name(item->path);
		
		u8 isRTA = (item->header.flags & (REPLAY_FLAG_FRAME_ADVANCED | REPLAY_FLAG_SAVE_STATED))==0;
		
		if (sSelectFileIndex==pathIndex){
			set_text_color(255,0,0,255);
			render_shadow_text_string_at(xPos-16,yPos,"--");
			// only activate replays when browsing
			if (sReplayBrowserMode==REPLAY_BROWSER_BROWSE){
				if (willEnter){
					gCurrPlayingReplay = NULL;
					import_practice_replay(item->path,&gPracticeFinishedReplay);
					if (gPlayer1Controller->buttonDown & Z_TRIG){
						gReplaySkipToEnd = TRUE;
					}
					replay_warp();
					gClosePracticeMenuNextFrame = TRUE;
				}
			}
		}
		
		if (isRTA){
			set_text_color(255,255,255,255);
		} else {
			set_text_color(255,180,180,255);
		}
		
		render_shadow_text_string_at(xPos,yPos,fileName);
		
		set_text_color(255,255,255,255);
		print_replay_time(buffer,sizeof(buffer),item);
		
		s32 size = get_text_width(buffer);
		render_shadow_text_string_at(boxRightEdge-maxTimeSize-16+(maxTimeSize-size),yPos,buffer);
		
		yPos -= PRACTICE_REPLAY_HEIGHT;
		++pathIndex;
	}
	
	if (sReplayHeaderStorageSize==0){
		set_text_color(168,168,168,255);
		render_shadow_text_string_at(xPos,yPos,"(empty)");
	}
}

void render_practice_menu(void){
	create_dl_ortho_matrix();
	shade_screen();
	
	if (gPlayer1Controller->buttonPressed & L_TRIG){
		--gPracticeMenuPage;
		gPracticeMenuPage += PRACTICE_MENU_COUNT;
		gPracticeMenuPage %= PRACTICE_MENU_COUNT;
	}
	
	if (gPlayer1Controller->buttonPressed & R_TRIG){
		++gPracticeMenuPage;
		gPracticeMenuPage %= PRACTICE_MENU_COUNT;
	}
	
	switch (gPracticeMenuPage){
		case PRACTICE_LEVEL_SELECT:
			render_level_select();
			break;
		case PRACTICE_SETTINGS:
			render_practice_settings();
			break;
		case PRACTICE_REPLAYS:
			render_practice_replays();
			break;
	}
	
	gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, BORDER_HEIGHT, SCREEN_WIDTH,
                      SCREEN_HEIGHT - BORDER_HEIGHT);
					  
	if (gPracticeMenuPage!=PRACTICE_REPLAYS&&(gPlayer1Controller->buttonPressed & B_BUTTON)){
		gClosePracticeMenuNextFrame = TRUE;
	}
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

static void render_test(void){
	/*create_dl_ortho_matrix();
	gDPSetFillColor(gDisplayListHead++,GPACK_RGBA5551(255,0,0,0)<<16 | 
										GPACK_RGBA5551(255,0,0,0));
	gDPFillRectangle(gDisplayListHead++,50,50,75,75);
	
	u8* tex = segmented_to_virtual(texture_a_button);

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

static void render_mario_info(void){
	char coord[32];
	
	s32 xPos = GFX_DIMENSIONS_FROM_LEFT_EDGE(0)+8;
	s32 yPos = 4;
	if (configShowPos||configShowAngle){
		gCurrTextScale = 0.75f;
		if (configShowPos){
			sprintf(coord,"Z: % 8.2f",gMarioState->pos[2]);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 10;
			sprintf(coord,"Y: % 8.2f",gMarioState->pos[1]);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 10;
			sprintf(coord,"X: % 8.2f",gMarioState->pos[0]);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 10;
		}
		if (configShowAngle){
			sprintf(coord,"A: ");
			get_angle_text(&coord[3],gMarioState->faceAngle[1]);
			render_shadow_text_string_at(xPos,yPos,coord);
			yPos += 10;
		}
		
		yPos += INFO_SPACING;
	}
	if (configShowVel){
		gCurrTextScale = 1.0f;
		
		sprintf(coord,"VS: % 7.1f",gMarioState->vel[1]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 16;
		sprintf(coord,"FS: % 7.1f",gMarioState->forwardVel);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 16;
		yPos += INFO_SPACING;
	}
	if (configShowMaxHeight){
		gCurrTextScale = 0.8f;
		sprintf(coord,"Max Y: % 8.2f",gHeightLock);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	if (configShowHOLP){
		const struct MarioBodyState* bodyState = gMarioState->marioBodyState;
		Vec3f holp;
		if (bodyState==NULL){
			holp[0] = 0.0f;
			holp[1] = 0.0f;
			holp[2] = 0.0f;
		} else {
			memcpy(holp,bodyState->heldObjLastPosition,sizeof(float)*3);
		}
		gCurrTextScale = 0.75f;
		sprintf(coord,"Z: % 8.2f",holp[2]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"Y: % 8.2f",holp[1]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"X: % 8.2f",holp[0]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"HOLP");
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	
	if (configShowRNGInfo){
		gCurrTextScale = 0.75f;
		sprintf(coord,"RNG: %d",gRandomSeed16);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"Idx: %d",gRandomCalls);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	
	if (configShowEfficiency){
		yPos += INFO_SPACING;
		gCurrTextScale = 0.8f;
		f32 stickMag = 0.0f;
		s16 yaw = 0;
		if (gMarioState->controller)
			stickMag = gMarioState->controller->stickMag/64.0f;
		
		if (stickMag>0.01f&&gMarioState->area&&gMarioState->area->camera){
			yaw = atan2s(-gMarioState->controller->stickY, gMarioState->controller->stickX) + gMarioState->area->camera->yaw;
		}
		s16 diff = gMarioState->faceAngle[1]-yaw;
		f32 val = coss(diff)*stickMag*stickMag*100.0f;
		if (val==-0.0f) val = 0.0f;
		sprintf(coord,"Eff: %3.1f%%",val);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		yPos += INFO_SPACING;
	}
	
	if (configShowWallkickAngle||configShowWallkickFrame){
		yPos += INFO_SPACING;
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
		if (sHoldingBowser){
			gCurrTextScale = 0.8f;
			float rate = (sBowserAngleVel/4096.0f)*100.0f;
			sprintf(coord,"Spin Rate: %3.0f%%",rate);
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
			
			sprintf(coord,"Spin Eff: %3.0f%%",spinSum);
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

void render_debug(void){
	create_dl_ortho_matrix();
	set_text_color(255,255,255,255);
	
	char text[30];
	
	gCurrTextScale = 1.0f;
	
	sprintf(text,"%d",gGlobalTimer);
	render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-50,text);
	
	sprintf(text,"%u",gIsRTA);
	render_shadow_text_string_at(GFX_DIMENSIONS_FROM_RIGHT_EDGE(0)-80,SCREEN_HEIGHT-BORDER_HEIGHT-90,text);
}

void render_practice_info(void){
	render_game_timer();
	render_mario_info();
	render_test();
	if (gDebug){
		render_debug();
	}
}

static void practice_data_init(void){
	sReplayHeaderStorage = malloc(sizeof(ReplayPathItem)*32);
	sReplayHeaderStorageCap = 32;
	sReplayHeaderStorageSize = 0;
	
	const char* replaysPath = fs_get_write_path("replays");
	snprintf(sReplaysStaticDir,sizeof(sReplaysStaticDir),"%s",replaysPath);
	
	if (!fs_sys_dir_exists(sReplaysStaticDir)){
		fs_sys_mkdir(sReplaysStaticDir);
	}
	
	sCurrReplayPath[0] = 0;
	
	enumerate_replays();
}

void practice_init(void){
	gTextInputString[0] = 0;
	memset(sSpinArray,0,sizeof(sSpinArray));
	
	gPracticeDest.type = WARP_TYPE_NOT_WARPING;
	init_state(&gCurrSaveState);
	
	load_all_settings();
	
	ghost_init();
	
	practice_data_init();
}

void practice_deinit(void){
	fs_pathlist_free(&sCurrReplayDirPaths);
	free(sReplayHeaderStorage);
}