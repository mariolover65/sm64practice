#include <PR/ultratypes.h>

#include "sm64.h"
#include "save_editor.h"
#include "practice.h"
#include "save_file.h"
#include "ingame_menu.h"
#include "game_init.h"
#include "print.h"
#include "hud.h"
#include "segment2.h"
#include "browser.h"
#include "level_update.h"

#define COURSE_COUNT 15
#define BONUS_COURSE_COUNT 6
#define PRACTICE_SAVE_OPTION_HEIGHT 24

enum {
	KEYS_INDEX = COURSE_COUNT+BONUS_COURSE_COUNT,
	KEY_DOORS_INDEX,
	STAR_DOORS_INDEX,
	MISC_INDEX,
	CAP_LOCATION_INDEX,
	CAPS_INDEX,
	SET_CAP_POS_BUTTON_INDEX,
	EXPORT_BUTTON_INDEX,
	SLOT_COUNT
};

static s32 sEditIndex = 0;
static s32 sSelectX = 0;
static s32 sEditScroll = 0;
static const s32 sEditSlotCount = SLOT_COUNT;

extern const u8 texture_dotted_box[];

// wing
extern const u8 exclamation_box_seg8_texture_08015E28[];
// metal
extern const u8 exclamation_box_seg8_texture_08014628[];
// vanish
extern const u8 exclamation_box_seg8_texture_08012E28[];

static const char* sStageNames[COURSE_COUNT] = {
	"BoB",
	"WF",
	"JRB",
	"CCM",
	"BBH",
	"HMC",
	"LLL",
	"SSL",
	"DDD",
	"SL",
	"WDW",
	"TTM",
	"THI",
	"TTC",
	"RR"
};

static const u8 sCannonStages[COURSE_COUNT] = {
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	FALSE,
	FALSE,
	FALSE,
	TRUE,
	FALSE,
	TRUE,
	TRUE,
	TRUE,
	FALSE,
	FALSE,
	TRUE
};

enum BonusStageEditNames {
	SAVE_EDIT_BONUS_STAGE_BOWSER,
	SAVE_EDIT_BONUS_STAGE_SA_WMOTR,
	SAVE_EDIT_BONUS_STAGE_CAP,
	SAVE_EDIT_BONUS_STAGE_PSS,
	SAVE_EDIT_BONUS_STAGE_TOAD,
	SAVE_EDIT_BONUS_STAGE_MIPS,
};

static const char* sBonusStageNames[BONUS_COURSE_COUNT] = {
	"Bowser",
	"SA/WMotR",
	"Cap",
	"PSS",
	"Toad",
	"MIPS"
};

static void calculate_save_edit_scroll(void){
	s32 yVal = SCREEN_HEIGHT-48-(sEditIndex-sEditScroll)*PRACTICE_SAVE_OPTION_HEIGHT;
	if (yVal<40){
		sEditScroll += (40-yVal)/PRACTICE_SAVE_OPTION_HEIGHT;
	}
	
	if (yVal>SCREEN_HEIGHT-16-40){
		sEditScroll += ((SCREEN_HEIGHT-16-40)-yVal)/PRACTICE_SAVE_OPTION_HEIGHT;
	}
	if (sEditScroll<0){
		sEditScroll = 0;
	}
}

static void render_cannon_at(s32 x,s32 y,u8 opened){
	render_cannon_sprite(x,y,opened);
}

static void render_star_at(s32 x,s32 y,u8 obtained){
	if (obtained){
		render_colored_sprite(main_hud_lut[GLYPH_STAR],x,y,255,255,255,255);
	} else {
		render_colored_sprite(main_hud_lut[GLYPH_STAR],x,y,0,0,0,255);
	}
}

static void render_cap_box_at(s32 x,s32 y,u8 capIndex,u8 obtained){
	y = SCREEN_HEIGHT-y-16;
	if (obtained){
		const u8* sprite;
		switch (capIndex){
			case 0:
				sprite = exclamation_box_seg8_texture_08015E28;
				break;
			case 1:
				sprite = exclamation_box_seg8_texture_08014628;
				break;
			case 2:
				sprite = exclamation_box_seg8_texture_08012E28;
				break;
		}
		render_big_colored_sprite(sprite,x,y,255,255,255,255);
	} else {
		u8 r,g,b;
		switch (capIndex){
			case 0:
				r = 255;
				g = 0;
				b = 0;
				break;
			case 1:
				r = 0;
				g = 255;
				b = 0;
				break;
			case 2:
				r = 0;
				g = 0;
				b = 255;
				break;
		}
		render_colored_sprite(texture_dotted_box,x,y,r,g,b,255);
	}
}

static void render_course_stars(s32 x,s32 y,u8 courseStars,u8 hasCannon,u8 cannonOpen){
	y = SCREEN_HEIGHT-y-16;
	for (s32 i=0;i<7;++i){
		render_star_at(x,y,courseStars & (1<<i));
		x += 16;
	}
	if (hasCannon)
		render_cannon_at(x,y,cannonOpen);
}

static void render_bonus_stars(s32 x,s32 y,const u8* courseStars,u32 saveFlags,s32 index){
	y = SCREEN_HEIGHT-y-16;
	switch (index){
		case SAVE_EDIT_BONUS_STAGE_BOWSER:
			render_star_at(x,y,courseStars[COURSE_BITDW-1] & 1);
			render_star_at(x+16,y,courseStars[COURSE_BITFS-1] & 1);
			render_star_at(x+32,y,courseStars[COURSE_BITS-1] & 1);
			break;
		case SAVE_EDIT_BONUS_STAGE_SA_WMOTR:
			render_star_at(x,y,courseStars[COURSE_SA-1] & 1);
			render_star_at(x+16,y,courseStars[COURSE_WMOTR-1] & 1);
			render_cannon_at(x+32,y,courseStars[COURSE_WMOTR]&0x80);
			break;
		case SAVE_EDIT_BONUS_STAGE_CAP:
			render_star_at(x,y,courseStars[COURSE_TOTWC-1] & 1);
			render_star_at(x+16,y,courseStars[COURSE_COTMC-1] & 1);
			render_star_at(x+32,y,courseStars[COURSE_VCUTM-1] & 1);
			break;
		case SAVE_EDIT_BONUS_STAGE_PSS:
			render_star_at(x,y,courseStars[COURSE_PSS-1] & 1);
			render_star_at(x+16,y,courseStars[COURSE_PSS-1] & 2);
			break;
		case SAVE_EDIT_BONUS_STAGE_TOAD: {
			u8 saveStars = (saveFlags>>24)&0x1F;
			render_star_at(x,y,saveStars & 1);
			render_star_at(x+16,y,saveStars & 2);
			render_star_at(x+32,y,saveStars & 4);
			break;
		}
		case SAVE_EDIT_BONUS_STAGE_MIPS: {
			u8 saveStars = (saveFlags>>24)&0x1F;
			render_star_at(x,y,saveStars & 8);
			render_star_at(x+16,y,saveStars & 16);
			break;
		}
	}
}

static void toggle_bonus_star(struct SaveFile* save,s32 index,s32 x){
	switch (index){
		case SAVE_EDIT_BONUS_STAGE_BOWSER:
			save->courseStars[COURSE_BITDW-1+x] ^= 1;
			break;
		case SAVE_EDIT_BONUS_STAGE_SA_WMOTR:
			if (x==0){
				save->courseStars[COURSE_SA-1] ^= 1;
			} else if (x==1){
				save->courseStars[COURSE_WMOTR-1] ^= 1;
			} else {
				save->courseStars[COURSE_WMOTR] ^= 0x80;
			}
			break;
		case SAVE_EDIT_BONUS_STAGE_CAP:
			if (x==0)
				save->courseStars[COURSE_TOTWC-1] ^= 1;
			else if (x==1)
				save->courseStars[COURSE_COTMC-1] ^= 1;
			else
				save->courseStars[COURSE_VCUTM-1] ^= 1;
			break;
		case SAVE_EDIT_BONUS_STAGE_PSS:
			save->courseStars[COURSE_PSS-1] ^= (1<<x);
			break;
		case SAVE_EDIT_BONUS_STAGE_TOAD: {
			save->flags ^= (1<<(24+x));
			break;
		}
		case SAVE_EDIT_BONUS_STAGE_MIPS: {
			save->flags ^= (1<<(27+x));
			break;
		}
	}
}

static void render_selector_at(s32 x,s32 y){
	gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
	render_hud_small_tex_lut(x+4,SCREEN_HEIGHT-y-2,main_hud_camera_lut[GLYPH_CAM_ARROW_UP]);
	render_hud_small_tex_lut(x+4,SCREEN_HEIGHT-y-6-16,main_hud_camera_lut[GLYPH_CAM_ARROW_DOWN]);
	gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
}

#define COLOR_IF_FLAG(flags,flag) \
	if ((flags) & (flag)){ \
		set_text_color(80,255,80,255); \
	} else { \
		set_text_color(255,80,80,255); \
	}

static void render_keys(s32 x,s32 y,u32 saveFlags,s32 selectX){
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_HAVE_KEY_1);
	render_shadow_text_string_at(x,y,"Basement");
	if (selectX==0){
		s32 textW = get_text_width("Basement");
		render_selector_at(x+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_HAVE_KEY_2);
	render_shadow_text_string_at(x+56,y,"Upstairs");
	if (selectX==1){
		s32 textW = get_text_width("Upstairs");
		render_selector_at(x+56+textW/2-8,y);
	}
}

static void render_key_doors(s32 x,s32 y,u32 saveFlags,s32 selectX){
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_UNLOCKED_BASEMENT_DOOR);
	render_shadow_text_string_at(x,y,"Basement");
	if (selectX==0){
		s32 textW = get_text_width("Basement");
		render_selector_at(x+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_UNLOCKED_UPSTAIRS_DOOR);
	render_shadow_text_string_at(x+56,y,"Upstairs");
	if (selectX==1){
		s32 textW = get_text_width("Upstairs");
		render_selector_at(x+56+textW/2-8,y);
	}
}

static void render_star_doors(s32 x,s32 y,u32 saveFlags,s32 selectX){
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_UNLOCKED_PSS_DOOR);
	render_shadow_text_string_at(x,y,"PSS");
	if (selectX==0){
		s32 textW = get_text_width("PSS");
		render_selector_at(x+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_UNLOCKED_WF_DOOR);
	render_shadow_text_string_at(x+24,y,"WF");
	if (selectX==1){
		s32 textW = get_text_width("WF");
		render_selector_at(x+24+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_UNLOCKED_CCM_DOOR);
	render_shadow_text_string_at(x+42,y,"CCM");
	if (selectX==2){
		s32 textW = get_text_width("CCM");
		render_selector_at(x+42+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_UNLOCKED_JRB_DOOR);
	render_shadow_text_string_at(x+66,y,"JRB");
	if (selectX==3){
		s32 textW = get_text_width("JRB");
		render_selector_at(x+66+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_UNLOCKED_BITDW_DOOR);
	render_shadow_text_string_at(x+90,y,"8");
	if (selectX==4){
		s32 textW = get_text_width("8");
		render_selector_at(x+90+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_UNLOCKED_BITFS_DOOR);
	render_shadow_text_string_at(x+102,y,"30");
	if (selectX==5){
		s32 textW = get_text_width("30");
		render_selector_at(x+102+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_UNLOCKED_50_STAR_DOOR);
	render_shadow_text_string_at(x+122,y,"50");
	if (selectX==6){
		s32 textW = get_text_width("50");
		render_selector_at(x+122+textW/2-8,y);
	}
}

static void render_misc_info(s32 x,s32 y,u32 saveFlags,s32 selectX){
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_FILE_EXISTS);
	render_shadow_text_string_at(x,y,"Exists");
	if (selectX==0){
		s32 textW = get_text_width("Exists");
		render_selector_at(x+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_DDD_MOVED_BACK);
	render_shadow_text_string_at(x+40,y,"DDD Moved");
	if (selectX==1){
		s32 textW = get_text_width("DDD Moved");
		render_selector_at(x+40+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_MOAT_DRAINED);
	render_shadow_text_string_at(x+100,y,"Moat Drained");
	if (selectX==2){
		s32 textW = get_text_width("Moat Drained");
		render_selector_at(x+100+textW/2-8,y);
	}
}

static void render_cap_location(s32 x,s32 y,u32 saveFlags,s32 selectX){
	u8 onMario = (saveFlags & 
		(SAVE_FLAG_CAP_ON_GROUND|SAVE_FLAG_CAP_ON_KLEPTO|
		SAVE_FLAG_CAP_ON_UKIKI|SAVE_FLAG_CAP_ON_MR_BLIZZARD))==0;
	
	if (onMario){
		set_text_color(80,255,80,255);
	} else {
		set_text_color(255,80,80,255);
	}
	render_shadow_text_string_at(x,y,"Mario");
	if (selectX==0){
		s32 textW = get_text_width("Mario");
		render_selector_at(x+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_CAP_ON_GROUND);
	render_shadow_text_string_at(x+32,y,"Ground");
	if (selectX==1){
		s32 textW = get_text_width("Ground");
		render_selector_at(x+32+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_CAP_ON_KLEPTO);
	render_shadow_text_string_at(x+68,y,"Klepto");
	if (selectX==2){
		s32 textW = get_text_width("Klepto");
		render_selector_at(x+68+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_CAP_ON_UKIKI);
	render_shadow_text_string_at(x+102,y,"Ukiki");
	if (selectX==3){
		s32 textW = get_text_width("Ukiki");
		render_selector_at(x+102+textW/2-8,y);
	}
	
	COLOR_IF_FLAG(saveFlags,SAVE_FLAG_CAP_ON_MR_BLIZZARD);
	render_shadow_text_string_at(x+132,y,"Blizzard");
	if (selectX==4){
		s32 textW = get_text_width("Blizzard");
		render_selector_at(x+132+textW/2-8,y);
	}
}

static void render_caps(s32 x,s32 y,u32 saveFlags,s32 selectX){
	render_cap_box_at(x,y,0,saveFlags & SAVE_FLAG_HAVE_WING_CAP);
	render_cap_box_at(x+24,y,1,saveFlags & SAVE_FLAG_HAVE_METAL_CAP);
	render_cap_box_at(x+48,y,2,saveFlags & SAVE_FLAG_HAVE_VANISH_CAP);
	
	if (selectX!=-1)
		render_selector_at(x+24*selectX,y);
}

static void wrap_x_with_width(s32 width){
	sSelectX += width;
	sSelectX %= width;
}

u8 render_save_editor(void){
	u8 close = FALSE;
	gCurrTextScale = 1.25f;
	set_text_color(255,255,255,255);
	render_shadow_text_string_at(12,SCREEN_HEIGHT-32,"Save Editor");
	
	if (gPlayer1Controller->buttonPressed & B_BUTTON){
		close = TRUE;
	}
	
	struct SaveFile* currSave = &gSaveBuffer.files[gCurrSaveFileNum-1][0];
	
	if (gPlayer1Controller->buttonPressed & U_JPAD){
		--sEditIndex;
		sEditIndex += sEditSlotCount;
		sEditIndex %= sEditSlotCount;
		calculate_save_edit_scroll();
	}
	
	if (gPlayer1Controller->buttonPressed & D_JPAD){
		++sEditIndex;
		sEditIndex %= sEditSlotCount;
		calculate_save_edit_scroll();
	}
	
	if (gPlayer1Controller->buttonPressed & L_JPAD){
		--sSelectX;
	}
	if (gPlayer1Controller->buttonPressed & R_JPAD){
		++sSelectX;
	}
	
	gCurrTextScale = 1.0f;
	s32 xPos = 132;
	s32 yPos = SCREEN_HEIGHT-64+sEditScroll*PRACTICE_SAVE_OPTION_HEIGHT;
	u8 pressedA = (gPlayer1Controller->buttonPressed & A_BUTTON)!=0;
	
	for (s32 i=0;i<sEditSlotCount;++i){
		if (yPos>SCREEN_HEIGHT+PRACTICE_SAVE_OPTION_HEIGHT){
			yPos -= PRACTICE_SAVE_OPTION_HEIGHT;
			continue;
		}
		if (yPos<0)
			break;
		
		const char* text;
		if (i<COURSE_COUNT){
			if (sEditIndex==i){
				if (sCannonStages[i]){
					wrap_x_with_width(8);
				} else {
					wrap_x_with_width(7);
				}
				if (pressedA){
					// if cannon
					if (sSelectX==7){
						currSave->courseStars[i+1] ^= 0x80;
					} else {
						currSave->courseStars[i] ^= (1<<sSelectX);
					}
				}
			}
			text = sStageNames[i];
			render_course_stars(xPos+10,yPos,currSave->courseStars[i],sCannonStages[i],currSave->courseStars[i+1]&0x80);
		} else if (i<COURSE_COUNT+BONUS_COURSE_COUNT){
			if (sEditIndex==i){
				if (i%2==1||i-COURSE_COUNT==SAVE_EDIT_BONUS_STAGE_SA_WMOTR){
					wrap_x_with_width(3);
				} else {
					wrap_x_with_width(2);
				}
				if (pressedA){
					toggle_bonus_star(currSave,i-COURSE_COUNT,sSelectX);
				}
			}
			text = sBonusStageNames[i-COURSE_COUNT];
			render_bonus_stars(xPos+10,yPos,currSave->courseStars,currSave->flags,i-COURSE_COUNT);
		} else if (i==KEYS_INDEX){
			s32 selectX = -1;
			if (i==sEditIndex){
				wrap_x_with_width(2);
				selectX = sSelectX;
				if (pressedA){
					currSave->flags ^= (1<<(4+sSelectX));
					if (currSave->flags & (1<<(6+sSelectX)))
						currSave->flags ^= (1<<(6+sSelectX));
				}
			}
			text = "Keys";
			render_keys(xPos+10,yPos,currSave->flags,selectX);
		} else if (i==KEY_DOORS_INDEX){
			s32 selectX = -1;
			if (i==sEditIndex){
				wrap_x_with_width(2);
				selectX = sSelectX;
				if (pressedA){
					currSave->flags ^= (1<<(6+sSelectX));
					if (currSave->flags & (1<<(4+sSelectX)))
						currSave->flags ^= (1<<(4+sSelectX));
				}
			}
			text = "Key Doors";
			render_key_doors(xPos+10,yPos,currSave->flags,selectX);
		} else if (i==STAR_DOORS_INDEX){
			s32 selectX = -1;
			if (i==sEditIndex){
				wrap_x_with_width(7);
				selectX = sSelectX;
				if (pressedA){
					if (sSelectX!=6){
						currSave->flags ^= (1<<(10+sSelectX));
					} else {
						currSave->flags ^= SAVE_FLAG_UNLOCKED_50_STAR_DOOR;
					}
				}
			}
			text = "Star Doors";
			render_star_doors(xPos+10,yPos,currSave->flags,selectX);
		} else if (i==MISC_INDEX){
			s32 selectX = -1;
			if (i==sEditIndex){
				wrap_x_with_width(3);
				selectX = sSelectX;
				if (pressedA){
					if (selectX==0){
						currSave->flags ^= SAVE_FLAG_FILE_EXISTS;
					} else {
						currSave->flags ^= (1<<(8+sSelectX-1));
					}
				}
			}
			text = "Misc";
			render_misc_info(xPos+10,yPos,currSave->flags,selectX);
		} else if (i==CAP_LOCATION_INDEX){
			s32 selectX = -1;
			if (i==sEditIndex){
				wrap_x_with_width(5);
				selectX = sSelectX;
				if (pressedA){
					currSave->flags &= 
						~(SAVE_FLAG_CAP_ON_GROUND|SAVE_FLAG_CAP_ON_KLEPTO|
							SAVE_FLAG_CAP_ON_UKIKI|SAVE_FLAG_CAP_ON_MR_BLIZZARD);
					if (selectX>0){
						currSave->flags |= (1<<(16+selectX-1));
					}
				}
			}
			text = "Cap Location";
			render_cap_location(xPos+10,yPos,currSave->flags,selectX);
		} else if (i==CAPS_INDEX){
			s32 selectX = -1;
			if (i==sEditIndex){
				wrap_x_with_width(3);
				selectX = sSelectX;
				if (pressedA){
					currSave->flags ^= (1<<(1+selectX));
				}
			}
			text = "Cap Switches";
			render_caps(xPos+10,yPos,currSave->flags,selectX);
		} else if (i==SET_CAP_POS_BUTTON_INDEX){
			if (i==sEditIndex&&(gPlayer1Controller->buttonPressed & A_BUTTON)){
				wrap_x_with_width(1);
				currSave->capLevel = gCurrLevelNum;
				currSave->capArea = gCurrAreaIndex;
				currSave->capPos[0] = (s16)gMarioState->pos[0];
				currSave->capPos[1] = (s16)gMarioState->pos[1];
				currSave->capPos[2] = (s16)gMarioState->pos[2];
			}
			text = "Set cap pos";
		} else if (i==EXPORT_BUTTON_INDEX){
			if (i==sEditIndex&&(gPlayer1Controller->buttonPressed & A_BUTTON)){
				wrap_x_with_width(1);
				browser_export_current_save();
			}
			text = "Export";
		}
		s32 w = get_text_width(text);
		set_text_color(255,255,255,255);
		s32 offset = 0;
		if (i==EXPORT_BUTTON_INDEX||i==SET_CAP_POS_BUTTON_INDEX)
			offset = 10+w;
		
		if (i==sEditIndex){
			if (i==EXPORT_BUTTON_INDEX||i==SET_CAP_POS_BUTTON_INDEX)
				set_text_color(255,230,0,255);
			else
				set_text_color(255,0,0,255);
			if (i<KEYS_INDEX){
				render_selector_at(xPos+10+16*sSelectX,yPos);
			}
		}
		render_shadow_text_string_at(xPos-w+offset,yPos,text);
		yPos -= PRACTICE_SAVE_OPTION_HEIGHT;
	}
	
	return close;
}

