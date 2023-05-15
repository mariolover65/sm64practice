#include <PR/ultratypes.h>

#include "sm64.h"
#include "practice.h"
#include "save_state.h"
#include "level_update.h"
#include "game_init.h"
#include "ingame_menu.h"
#include "gfx_dimensions.h"
#include "pc/configfile.h"

#include <stdio.h>

struct WarpDest gPracticeDest;
struct WarpDest gLastWarpDest;
u8 gPracticeWarping = FALSE;
u8 gNoStarSelectWarp = FALSE;
u8 gSaveStateWarpDelay = 0;
u8 gRenderPracticeMenu = 0;
u8 gPracticeMenuPage = 0;

u8 gPlaybackPrimed = 0;

s32 gSectionTimer = 0;
s32 gSectionTimerResult = NULL_SECTION_TIMER_RESULT;

static s32 sSectionTimerLock = 0;
static s32 sSectionTimerLockTime = 0;

u8 gFrameAdvance = 0;
u32 gLastButtons = 0;

f32 gHeightLock = 0.0f;

u8 gWallkickFrame = 0;
u16 gWallkickAngle = 0;
s32 gWallkickTimer = 0;

Replay gPracticeReplay;

static u8 sWallkickColors[5][3] = {
	{255,255,0},
	{0,64,255},
	{255,128,0},
	{0,255,0},
	{255,0,255},
};

//s32 practice_update(void){
	
	/* else if ((gPlayer1Controller->buttonPressed & R_JPAD)&&gCurrRecordingReplay){
		end_replay_record();
	}*/
	
	/*if (gPlayer1Controller->buttonPressed & L_TRIG){
		gPracticeDest = gPracticeReplay.state.levelState->loc;
		gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
		gNoStarSelectWarp = TRUE;
		gPlaybackPrimed = 1;
		
		//init_playing_replay(&gPracticeReplay);
	}*/
	
	
	/*
	if (gPlaybackPrimed&&gCurrLevelNum==gPracticeReplay.state.levelState->loc.levelNum&&
		gCurrAreaIndex==gPracticeReplay.state.levelState->loc.areaIdx){
		init_playing_replay(&gPracticeReplay);
		gPlaybackPrimed = 0;
	}
	
	
	
	return 0;
}
*/

bool can_load_save_state(void){
	return gCurrLevelNum==gCurrSaveState.initState.loc.levelNum&&
		gCurrAreaIndex==gCurrSaveState.initState.loc.areaIdx&&
		gPracticeDest.type==WARP_TYPE_NOT_WARPING&&(sWarpDest.type==WARP_TYPE_NOT_WARPING||sTransitionTimer>=1);
}

void practice_update(void){
	u8 canLoad = can_load_save_state();
	
	if (!gRenderPracticeMenu&&(gPlayer1Controller->buttonPressed & R_JPAD)){
		save_state(&gCurrSaveState);
		printf("gCurrSaveState size: %u\n",get_state_size(&gCurrSaveState));
	} else if (!gRenderPracticeMenu&&(gPlayer1Controller->buttonPressed & U_JPAD)&&gHasStateSaved){
		if (canLoad){
			load_state(&gCurrSaveState);
		} else {
			gPracticeDest = gCurrSaveState.initState.loc;
			gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
			gPracticeDest.nodeId = 0xA;
			gPracticeDest.arg = 0;
			gSaveStateWarpDelay = 1;
			gNoStarSelectWarp = TRUE;
		}
	}
	
	if (!gFrameAdvance && !gRenderPracticeMenu && gPlayer1Controller->buttonPressed & L_TRIG){
		gPracticeDest = gLastWarpDest;
		gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
		gNoStarSelectWarp = TRUE;
	}
	
	if (gWallkickTimer){
		--gWallkickTimer;
	}
}

void save_state_update(void){
	u8 canLoad = can_load_save_state();
		
	if (gSaveStateWarpDelay!=0&&canLoad){
		if (--gSaveStateWarpDelay==0){
			load_state(&gCurrSaveState);
		}
	}
}

typedef struct {
	const char* name;
	u16 levelNum;
	u16 areaNum;
	u16 nodeId;
	
} PracticeDest;

#define PRACTICE_LEVEL_COUNT 30

static const PracticeDest stageNames[PRACTICE_LEVEL_COUNT] = {
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
	
	{"BitDW",LEVEL_BITDW,1,0xA},
	{"BitFS",LEVEL_BITFS,1,0xA},
	{"BitS",LEVEL_BITS,1,0xA},
	{"Bow1",LEVEL_BOWSER_1,1,0xA},
	{"Bow2",LEVEL_BOWSER_2,1,0xA},
	{"Bow3",LEVEL_BOWSER_3,1,0xA},
	
	{"Lobby",LEVEL_CASTLE,1,0x0},
	{"Basement",LEVEL_CASTLE,3,0x0},
	{"Upstairs",LEVEL_CASTLE,2,0x1},
};

s32 gPracticeStageSelect = 0;
static s32 sYScroll = 0;

#define LEVEL_SPACING 24

static void render_level_select(void){
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
	s32 yVal = 200-(ySelect-sYScroll)*LEVEL_SPACING;
	if (yVal<40){
		sYScroll += (40-yVal)/LEVEL_SPACING;
	}
	
	if (yVal>SCREEN_HEIGHT-40){
		sYScroll += ((SCREEN_HEIGHT-40)-yVal)/LEVEL_SPACING;
	}
	
	gCurrTextScale = 1.25f;
	u32 x;
	for (s32 i=0;i<PRACTICE_LEVEL_COUNT;++i){
		set_text_color(255,255,255,255);
		if (i==gPracticeStageSelect)
			set_text_color(255,0,0,255);
		
		x = i%2;
		x *= 100;
		x -= 50;
		render_shadow_text_string_at(175+x,200-(i/2-sYScroll)*LEVEL_SPACING,stageNames[i].name);
	}
	
	const PracticeDest* curr = &stageNames[gPracticeStageSelect];
	
	if (gPlayer1Controller->buttonPressed & A_BUTTON){
		gPracticeDest.type = WARP_TYPE_CHANGE_LEVEL;
		gPracticeDest.levelNum = curr->levelNum;
		gPracticeDest.areaIdx = curr->areaNum;
		gPracticeDest.nodeId = curr->nodeId;
		if (gPracticeDest.levelNum==LEVEL_CASTLE&&gPracticeDest.areaIdx==1&&gPracticeDest.nodeId==0){
			gPracticeDest.arg = 6;
		} else {
			gPracticeDest.arg = 0;
		}
		gNoStarSelectWarp = FALSE;
		gRenderPracticeMenu = FALSE;
		gDialogID = -1;
		section_timer_choose_level();
	}
}

typedef enum {
	PRACTICE_OPTION_BOOL = 0,
	PRACTICE_OPTION_INT,
	PRACTICE_OPTION_ENUM
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

static const char* sSectionTimerValues[SECTION_TIMER_TYPE_COUNT+1] = {
	"XCam",
	"Level",
	"Game",
	NULL
};

#define PRACTICE_GAME_SETTINGS_COUNT 3
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
		"Section timer",
		sSectionTimerValues,
		PRACTICE_OPTION_ENUM,
		0,
		&configSectionTimerType
	},
};

#define PRACTICE_HUD_SETTINGS_COUNT 5
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
		"Max height",
		sShowBoolValues,
		PRACTICE_OPTION_BOOL,
		0,
		&configShowMaxHeight
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
};

#define PRACTICE_PAGE_COUNT 2
static PracticePage sPracticePages[PRACTICE_PAGE_COUNT] = {
	{
		"Game",
		PRACTICE_PAGE_SETTINGS,
		0,
		PRACTICE_GAME_SETTINGS_COUNT,
		sPracticeGameSettings
	},
	{
		"HUD",
		PRACTICE_PAGE_SETTINGS,
		0,
		PRACTICE_HUD_SETTINGS_COUNT,
		sPracticeHUDSettings
	},
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
		}
		
		if (gPlayer1Controller->buttonPressed & U_JPAD){
			sPracticePageSelect -= 1;
			sPracticePageSelect += PRACTICE_PAGE_COUNT;
			sPracticePageSelect %= PRACTICE_PAGE_COUNT;
		}
	}
	
	PracticePage* currPage = &sPracticePages[sPracticePageSelect];
	if (sInPracticePage){
		if (gPlayer1Controller->buttonPressed & D_JPAD){
			currPage->index += 1;
			currPage->index %= currPage->size;
		}
		
		if (gPlayer1Controller->buttonPressed & U_JPAD){
			currPage->index -= 1;
			currPage->index += currPage->size;
			currPage->index %= currPage->size;
		}
		
		PracticeSetting* selected = &currPage->settings[currPage->index];
		
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
	}
	
	for (s32 i=0;i<currPage->size;++i){
		PracticeSetting* currSetting = &currPage->settings[i];
		
		if (sInPracticePage)
			set_text_color(255,255,255,255);
		else
			set_text_color(168,168,168,255);
		
		render_shadow_text_string_at(120,SCREEN_HEIGHT-32-i*24,currSetting->name);
		
		if (sInPracticePage&&currPage->index==i)
			set_text_color(255,0,0,255);
		render_shadow_text_string_at(230,SCREEN_HEIGHT-32-i*24,currSetting->values[currSetting->index]);
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
	}
}

static void render_game_timer(void){
	create_dl_ortho_matrix();
	set_text_color(255,255,255,255);
	
	s32 val = gSectionTimer;
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

static void render_mario_info(void){
	char coord[32];
	
	s32 xPos = GFX_DIMENSIONS_FROM_LEFT_EDGE(0)+8;
	s32 yPos = 8;
	if (configShowPos){
		gCurrTextScale = 0.75f;
		
		sprintf(coord,"Z: % 8.2f",gMarioState->pos[2]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"Y: % 8.2f",gMarioState->pos[1]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
		sprintf(coord,"X: % 8.2f",gMarioState->pos[0]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 18;
	}
	if (configShowVel){
		gCurrTextScale = 0.8f;
		
		sprintf(coord,"VS: % 7.1f",gMarioState->vel[1]);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 12;
		sprintf(coord,"FS: % 7.1f",gMarioState->forwardVel);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 12;
	}
	if (configShowMaxHeight){
		gCurrTextScale = 0.8f;
		sprintf(coord,"Max Y: % 8.2f",gHeightLock);
		render_shadow_text_string_at(xPos,yPos,coord);
		yPos += 10;
	}
	if (configShowWallkickAngle&&gWallkickTimer!=0){
		yPos += 10;
		float angle = gWallkickAngle/65536.0f*360.0f-180.0f;
		gCurrTextScale = 0.8f;
		sprintf(coord,"% 3.2fdeg",angle);
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
		yPos += 16;
	}
	set_text_color(255,255,255,255);
}

void render_practice_info(void){
	render_game_timer();
	render_mario_info();
}

static void section_timer_start(void){
	gSectionTimerResult = NULL_SECTION_TIMER_RESULT;
	gSectionTimer = 0;
}

static void section_timer_finish(void){
	if (gSectionTimerResult==NULL_SECTION_TIMER_RESULT)
		gSectionTimerResult = gSectionTimer;
}

void section_timer_level_init(void){
	if (configSectionTimerType==SECTION_TIMER_TYPE_XCAM){
		section_timer_start();
		gSectionTimer = 1;
	}
	gHeightLock = 0.0f;
}

void section_timer_choose_level(void){
	if (configSectionTimerType==SECTION_TIMER_TYPE_LEVEL){
		section_timer_start();
	}
}

void section_timer_star_xcam(void){
	if (configSectionTimerType==SECTION_TIMER_TYPE_XCAM){
		section_timer_finish();
	}
}

void section_timer_game_win(void){
	if (configSectionTimerType==SECTION_TIMER_TYPE_GAME){
		section_timer_finish();
	}
}

void section_timer_game_reset(void){
	if (configSectionTimerType==SECTION_TIMER_TYPE_GAME){
		section_timer_start();
	}
}

void timer_freeze(void){
	sSectionTimerLock = gSectionTimer;
	sSectionTimerLockTime = 30;
}

void practice_init(void){
	gPracticeDest.type = WARP_TYPE_NOT_WARPING;
	init_state(&gCurrSaveState);
	
	for (s32 i=0;i<PRACTICE_PAGE_COUNT;++i){
		PracticePage* page = &sPracticePages[i];
		for (s32 j=0;j<page->size;++j){
			load_setting(&page->settings[j]);
		}
	}
}