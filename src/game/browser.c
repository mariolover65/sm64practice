#include "browser.h"

#include "practice.h"
#include "pc/fs/fs.h"
#include "ingame_menu.h"
#include "save_file.h"
#include "game_init.h"

#define PRACTICE_SAVE_SIZE (sizeof(struct SaveFile)-sizeof(struct SaveBlockSignature))
// must be 4 bytes
#define PRACTICE_SAVE_MAGIC "ps\x0A\x0B"

enum ReplayFileHeaderType {
	HEADER_TYPE_REPLAY_FILE,
	HEADER_TYPE_SAVE_FILE,
	HEADER_TYPE_DIR
};

typedef struct {
	u8 type;
	const char* path;
	union {
		ReplayFileHeader header;
		s32 starCount;
	};
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
	REPLAY_BROWSER_SAVE_FILE,
	REPLAY_BROWSER_CREATE_DIR,
	REPLAY_BROWSER_OVERWRITE_CONFIRM,
	REPLAY_BROWSER_DELETE_CONFIRM,
};

enum BrowserOverwriteType {
	OVERWRITE_REPLAY,
	OVERWRITE_SAVE_FILE
};

static u8 sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
static u8 sBrowserOverwriteType;

u8 is_replay_file(const char* path){
	const char* ext = sys_file_extension(path);
	if (ext==NULL) return FALSE;
	
	if (strncmp(ext,PRACTICE_REPLAY_EXT,3)==0) return TRUE;
	if (strncmp(ext,MARIOTIMER_REPLAY_EXT,3)==0) return TRUE;
	return FALSE;
}

u8 is_practice_replay_file(const char* path){
	const char* ext = sys_file_extension(path);
	if (ext==NULL) return FALSE;
	
	if (strncmp(ext,PRACTICE_REPLAY_EXT,3)!=0) return FALSE;
	return TRUE;
}

u8 is_save_file(const char* path){
	const char* ext = sys_file_extension(path);
	if (ext==NULL) return FALSE;
	
	if (strncmp(ext,PRACTICE_SAVE_FILE_EXT,3)!=0) return FALSE;
	return TRUE;
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
	
	if (is_practice_replay_file(path)){
		if (!deserialize_replay_header(file,&item->header)){
			--sReplayHeaderStorageSize;
			printf("could not deserialize header!\n%s\n",path);
			fflush(stdout);
			fclose(file);
			return;
		}
	} else {
		if (!deserialize_mtr_replay_header(file,&item->header)){
			--sReplayHeaderStorageSize;
			printf("could not deserialize mtr header!\n%s\n",path);
			fflush(stdout);
			fclose(file);
			return;
		}
	}
	
	item->path = path;
	item->type = HEADER_TYPE_REPLAY_FILE;
	fclose(file);
}

STATIC_ASSERT(PRACTICE_SAVE_SIZE==12+COURSE_COUNT+COURSE_STAGES_COUNT,"msg");

static s32 count_bits(u32 val){
	s32 c = 0;
	while (val){
		if (val&1) ++c;
		val >>= 1;
	}
	return c;
}

static u8 get_save_file_star_count(FILE* file,s32* starCount){
	*starCount = 0;
	fseek(file,12,SEEK_SET);
	u32 flags;
	if (fread(&flags,sizeof(u32),1,file)!=1)
		return FALSE;
	*starCount += count_bits((flags>>24)&0x1F);
	
	u8 courseStars[COURSE_COUNT];
	if (fread(courseStars,sizeof(u8),COURSE_COUNT,file)!=COURSE_COUNT)
		return FALSE;
	
	for (s32 i=0;i<COURSE_COUNT;++i){
		*starCount += count_bits(courseStars[i]&0x7F);
	}
	return TRUE;
}

void replay_header_storage_add_save_file(const char* path){
	FILE* file = fopen(path,"rb");
	if (!file) return;
	char magic[4];
	fread(magic,sizeof(magic),1,file);
	if (memcmp(magic,PRACTICE_SAVE_MAGIC,sizeof(magic))!=0)
		return;
	
	ReplayPathItem* item = replay_header_storage_alloc();
	item->type = HEADER_TYPE_SAVE_FILE;
	
	if (!get_save_file_star_count(file,&item->starCount)){
		--sReplayHeaderStorageSize;
		printf("could not deserialize save file stars!\n%s\n",path);
		fflush(stdout);
		fclose(file);
		return;
	}
	
	item->path = path;
	fclose(file);
}

u8 path_exists(const char* path){
	return fs_sys_file_exists(path) || fs_sys_dir_exists(path);
}

static void start_text_entry(void){
	gTextInputString[0] = 0;
	gTextInputSubmitted = FALSE;
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
		} else if (is_save_file(path)){
			replay_header_storage_add_save_file(path);
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

void export_practice_replay(const char* path,const Replay* replay){
	if (!replay) return;
	
	FILE* file = fopen(path,"wb");
	if (!file){
		printf("Could not open file!\nPath: %s\n",path);
		return;
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
	
	if (is_practice_replay_file(path)){
		if (!deserialize_replay(file,*replay)){
			printf("Could not deserialize!\n");
			free_replay(*replay);
			*replay = NULL;
			fclose(file);
			return;
		}
	} else {
		if (!deserialize_mtr_replay(file,*replay)){
			printf("Could not deserialize!\n");
			free_replay(*replay);
			*replay = NULL;
			fclose(file);
			return;
		}
	}
	
	fclose(file);
	printf("Loaded replay of length %u\n",(*replay)->length);
}

void reexport_practice_replay(const char* path){
	Replay* replay = NULL;
	import_practice_replay(path,&replay);
	export_practice_replay(path,replay);
	free_replay(replay);
	printf("Reexported %s\n",path);
	fflush(stdout);
}

void export_practice_save_file(const char* path,const struct SaveFile* save){
	if (!save) return;
	
	FILE* file = fopen(path,"wb");
	if (!file){
		printf("Could not open file!\nPath: %s\n",path);
		return;
	}
	
	fwrite(PRACTICE_SAVE_MAGIC,4,1,file);
	fwrite(save,PRACTICE_SAVE_SIZE,1,file);
	fclose(file);
}

u8 import_practice_save_file(const char* path,struct SaveFile* save){
	if (!save) return FALSE;
	
	FILE* file = fopen(path,"rb");
	if (!file){
		printf("Could not open file!\nPath: %s\n",path);
		return FALSE;
	}
	
	char magic[4];
	if (fread(magic,sizeof(magic),1,file)!=1)
		return FALSE;
	
	if (memcmp(magic,PRACTICE_SAVE_MAGIC,sizeof(magic))!=0){
		printf("Invalid save file\n");
		return FALSE;
	}
	
	size_t res = fread(save,PRACTICE_SAVE_SIZE,1,file);
	fclose(file);
	return res==1;
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

static ReplayPathItem* browser_get_item_at_cursor(void){
	s32 index = sSelectFileIndex;
	s32 pos = 0;
	for (u32 i=0;i<sReplayHeaderStorageSize;++i){
		ReplayPathItem* item = &sReplayHeaderStorage[i];
		if (item->type!=HEADER_TYPE_DIR){
			continue;
		}
		if (pos==index) return item;
		++pos;
	}
	
	for (u32 i=0;i<sReplayHeaderStorageSize;++i){
		ReplayPathItem* item = &sReplayHeaderStorage[i];
		if (item->type==HEADER_TYPE_DIR){
			continue;
		}
		if (pos==index) return item;
		++pos;
	}
	return NULL;
}

u8 browser_delete_at_cursor(void){
	if (sSelectFileIndex<0 || sSelectFileIndex>=(s32)sReplayHeaderStorageSize) return FALSE;
	const ReplayPathItem* item = browser_get_item_at_cursor();
	sWillEnumerate = TRUE;
	if (item->type==HEADER_TYPE_DIR){
		return fs_sys_rmdir(item->path);
	}
	return fs_sys_remove_file(item->path);
}

u8 browser_export_replay_as(const char* name,const Replay* replay,u8 overwrite){
	if (!replay) return FALSE;
	if (strchr(name,'/')) return FALSE;
	const char* ext = sys_file_extension(name);
	
	static char path[REPLAY_MAX_PATH];
	
	print_curr_replay_path(path,name);
	
	if (!ext){
		strncat(path,"." PRACTICE_REPLAY_EXT,sizeof(path)-1);
	}
	
	if (fs_sys_dir_exists(path))
		return FALSE;
	
	if (fs_sys_file_exists(path)){
		if (!overwrite){
			bzero(sReplayNameTempStorage,sizeof(sReplayNameTempStorage));
			memcpy(sReplayNameTempStorage,name,sizeof(sReplayNameTempStorage));
			sReplayBrowserMode = REPLAY_BROWSER_OVERWRITE_CONFIRM;
			sBrowserOverwriteType = OVERWRITE_REPLAY;
			return TRUE;
		} else {
			if (!fs_sys_remove_file(path))
				return FALSE;
		}
	}
	
	export_practice_replay(path,replay);
	return TRUE;
}

u8 browser_export_save_file_as(const char* name,const struct SaveFile* save,u8 overwrite){
	if (!save) return FALSE;
	if (strchr(name,'/')) return FALSE;
	const char* ext = sys_file_extension(name);
	
	static char path[REPLAY_MAX_PATH];
	
	print_curr_replay_path(path,name);
	
	if (!ext){
		strncat(path,"." PRACTICE_SAVE_FILE_EXT,sizeof(path)-1);
	}
	
	if (fs_sys_dir_exists(path))
		return FALSE;
	
	if (fs_sys_file_exists(path)){
		if (!overwrite){
			bzero(sReplayNameTempStorage,sizeof(sReplayNameTempStorage));
			memcpy(sReplayNameTempStorage,name,sizeof(sReplayNameTempStorage));
			sReplayBrowserMode = REPLAY_BROWSER_OVERWRITE_CONFIRM;
			sBrowserOverwriteType = OVERWRITE_SAVE_FILE;
			return TRUE;
		} else {
			if (!fs_sys_remove_file(path))
				return FALSE;
		}
	}
	
	export_practice_save_file(path,save);
	return TRUE;
}

u8 browser_create_dir(const char* name){
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

void print_replay_time(char* buffer,size_t bufferSize,const ReplayPathItem* item){
	set_timer_text_small(get_replay_header_length(&item->header));
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
			default:
				snprintf(buffer,bufferSize,"?");
				break;
		}
	}
}

void print_star_count(char* buffer,size_t bufferSize,s32 starCount){
	snprintf(buffer,bufferSize,"^*%d",starCount);
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

// practice buttons
void button_export_current_replay(PracticeSetting*){
	if (!gPracticeFinishedReplay)
		return;
	
	gPracticeMenuPage = PRACTICE_REPLAYS;
	sReplayBrowserMode = REPLAY_BROWSER_SAVE_REPLAY;
	start_text_entry();
}

void button_reexport_all_replays(PracticeSetting*){
	fs_pathlist_t replayPaths = fs_sys_enumerate(sReplaysStaticDir,TRUE);
	for (s32 i=0;i<replayPaths.numpaths;++i){
		if (is_practice_replay_file(replayPaths.paths[i])){
			reexport_practice_replay(replayPaths.paths[i]);
		}
	}
}

void browser_export_current_save(void){
	gPracticeMenuPage = PRACTICE_REPLAYS;
	sReplayBrowserMode = REPLAY_BROWSER_SAVE_FILE;
	start_text_entry();
}

void browser_init(void){
	sReplayHeaderStorage = malloc(sizeof(ReplayPathItem)*32);
	sReplayHeaderStorageCap = 32;
	sReplayHeaderStorageSize = 0;
	
	const char* replaysPath = fs_get_write_path("replays");
	snprintf(sReplaysStaticDir,sizeof(sReplaysStaticDir),"%s",replaysPath);
	
	if (!fs_sys_dir_exists(sReplaysStaticDir)){
		fs_sys_mkdir(sReplaysStaticDir);
	}
	
	gTextInputString[0] = 0;
	sCurrReplayPath[0] = 0;
	enumerate_replays();
}

void browser_free(void){
	fs_pathlist_free(&sCurrReplayDirPaths);
	free(sReplayHeaderStorage);
}

u8 render_browser(void){
	u8 close = FALSE;
	gCurrTextScale = 1.25f;
	set_text_color(255,255,255,255);
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
				close = TRUE;
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
				browser_export_replay_as(gTextInputString,gPracticeFinishedReplay,FALSE);
				sWillEnumerate = TRUE;
			} else if (gPlayer1Controller->buttonPressed & B_BUTTON){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
			} else if (gPlayer1Controller->buttonPressed & A_BUTTON){
				if (sReplayHeaderStorageSize>0&&sSelectFileIndex<(s32)sReplayHeaderStorageSize){
					const ReplayPathItem* item = browser_get_item_at_cursor();
					if (item->type==HEADER_TYPE_REPLAY_FILE){
						sReplayBrowserMode = REPLAY_BROWSER_OVERWRITE_CONFIRM;
						sBrowserOverwriteType = OVERWRITE_REPLAY;
						const char* fileName = sys_file_name(item->path);
						bzero(sReplayNameTempStorage,sizeof(sReplayNameTempStorage));
						memcpy(sReplayNameTempStorage,fileName,strnlen(fileName,sizeof(sReplayNameTempStorage)));
					} else if (item->type==HEADER_TYPE_DIR){
						willEnter = TRUE;
					}
				}
			} else if (check_will_pop()){
				willPop = TRUE;
			} else if (check_will_enter()){
				willEnter = TRUE;
			}
			break;
		case REPLAY_BROWSER_SAVE_FILE:
			handle_replay_index();
			snprintf(buffer,sizeof(buffer),"Enter file name: %s",gTextInputString);
			render_shadow_text_string_at(70,SCREEN_HEIGHT-24,buffer);
			
			if (gTextInputSubmitted){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
				// check error
				browser_export_save_file_as(gTextInputString,&gSaveBuffer.files[gCurrSaveFileNum-1][0],FALSE);
				sWillEnumerate = TRUE;
			} else if (gPlayer1Controller->buttonPressed & B_BUTTON){
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
			} else if (gPlayer1Controller->buttonPressed & A_BUTTON){
				if (sReplayHeaderStorageSize>0&&sSelectFileIndex<(s32)sReplayHeaderStorageSize){
					const ReplayPathItem* item = browser_get_item_at_cursor();
					if (item->type==HEADER_TYPE_SAVE_FILE){
						sReplayBrowserMode = REPLAY_BROWSER_OVERWRITE_CONFIRM;
						sBrowserOverwriteType = OVERWRITE_SAVE_FILE;
						const char* fileName = sys_file_name(item->path);
						bzero(sReplayNameTempStorage,sizeof(sReplayNameTempStorage));
						memcpy(sReplayNameTempStorage,fileName,strnlen(fileName,sizeof(sReplayNameTempStorage)));
					} else if (item->type==HEADER_TYPE_DIR){
						willEnter = TRUE;
					}
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
				browser_create_dir(gTextInputString);
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
				browser_delete_at_cursor();
				sReplayBrowserMode = REPLAY_BROWSER_BROWSE;
				gPlayer1Controller->buttonPressed &= ~A_BUTTON;
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
				if (sBrowserOverwriteType==OVERWRITE_REPLAY)
					browser_export_replay_as(sReplayNameTempStorage,gPracticeFinishedReplay,TRUE);
				else if (sBrowserOverwriteType==OVERWRITE_SAVE_FILE)
					browser_export_save_file_as(sReplayNameTempStorage,&gSaveBuffer.files[gCurrSaveFileNum-1][0],TRUE);
				sWillEnumerate = TRUE;
				gPlayer1Controller->buttonPressed &= ~A_BUTTON;
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
	
	shade_screen_rect(48,48,SCREEN_WIDTH-48-48,bottomY-48,0,0,0,110);
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
		
		if (item->type==HEADER_TYPE_REPLAY_FILE){
			if (get_replay_header_length(&item->header))
				print_replay_time(buffer,sizeof(buffer),item);
			size = get_text_width(buffer);
			if (size>maxTimeSize){
				maxTimeSize = size;
			}
		} else if (item->type==HEADER_TYPE_SAVE_FILE){
			print_star_count(buffer,sizeof(buffer),item->starCount);
			size = get_text_width(buffer);
			if (size>maxTimeSize){
				maxTimeSize = size;
			}
		}
	}
	
	// dir pass
	for (u32 i=0;i<sReplayHeaderStorageSize;++i){
		const ReplayPathItem* item = &sReplayHeaderStorage[i];
		if (item->type!=HEADER_TYPE_DIR){
			continue;
		}
		
		if (yPos<0)
			break;
		
		const char* fileName = sys_file_name(item->path);
		
		if (sSelectFileIndex==pathIndex){
			set_text_color(255,0,0,255);
			render_shadow_text_string_at(xPos-16,yPos,"--");
			if (willEnter){
				practice_replay_enter_dir(fileName);
				sSelectFileIndex = 0;
			}
		}
		
		if (yPos<SCREEN_HEIGHT+PRACTICE_REPLAY_HEIGHT){
			set_text_color(0,255,0,255);
			render_shadow_text_string_at(xPos,yPos,fileName);
		}
		yPos -= PRACTICE_REPLAY_HEIGHT;
		++pathIndex;
	}
	
	// file pass
	for (u32 i=0;i<sReplayHeaderStorageSize;++i){
		const ReplayPathItem* item = &sReplayHeaderStorage[i];
		if (item->type==HEADER_TYPE_DIR){
			continue;
		}
		
		if (yPos<0)
			break;
		
		const char* fileName = sys_file_name(item->path);
		
		if (sSelectFileIndex==pathIndex){
			set_text_color(255,0,0,255);
			render_shadow_text_string_at(xPos-16,yPos,"--");
			// only activate replays when browsing
			if (sReplayBrowserMode==REPLAY_BROWSER_BROWSE){
				if (gPlayer1Controller->buttonPressed & A_BUTTON){
					if (item->type==HEADER_TYPE_REPLAY_FILE){
						gCurrPlayingReplay = NULL;
						if (gPracticeFinishedReplay){
							archive_replay(gPracticeFinishedReplay);
							gPracticeFinishedReplay = NULL;
						}
						import_practice_replay(item->path,&gPracticeFinishedReplay);
						if (gPlayer1Controller->buttonDown & Z_TRIG){
							gReplaySkipToEnd = TRUE;
							gIsRecordingGhost = TRUE;
						}
						replay_warp();
					} else if (item->type==HEADER_TYPE_SAVE_FILE){
						if (import_practice_save_file(item->path,&gSaveBuffer.files[gCurrSaveFileNum-1][0])){
							gSaveFileModified = TRUE;
							update_save_file_vars();
							practice_set_message("Loaded save file");
						}
					}
					close = TRUE;
				} else if (gPlayer1Controller->buttonPressed & R_JPAD){
					if (item->type==HEADER_TYPE_REPLAY_FILE){
						gCurrPlayingReplay = NULL;
						if (gPracticeFinishedReplay){
							archive_replay(gPracticeFinishedReplay);
							gPracticeFinishedReplay = NULL;
						}
						import_practice_replay(item->path,&gPracticeFinishedReplay);
						practice_set_message("Imported replay");
					}
				}
			}
		}
		
		
		if (yPos<SCREEN_HEIGHT+PRACTICE_REPLAY_HEIGHT){
			if (item->type==HEADER_TYPE_REPLAY_FILE){
				u8 isRTA = (item->header.flags & (REPLAY_FLAG_FRAME_ADVANCED | REPLAY_FLAG_SAVE_STATED))==0;
				u8 isSlightCheat = (item->header.flags & (REPLAY_FLAG_OPENED_PRACTICE_MENU | REPLAY_FLAG_CHEAT_HUD));
				if (isRTA){
					if (isSlightCheat){
						set_text_color(255,255,128,255);
					} else {
						set_text_color(255,255,255,255);
					}
				} else {
					set_text_color(255,128,128,255);
				}
				
				render_shadow_text_string_at(xPos,yPos,fileName);
				if (get_replay_header_length(&item->header)){
					set_text_color(255,255,255,255);
					print_replay_time(buffer,sizeof(buffer),item);
					s32 size = get_text_width(buffer);
					render_shadow_text_string_at(boxRightEdge-maxTimeSize-16+(maxTimeSize-size),yPos,buffer);
				}
			} else if (item->type==HEADER_TYPE_SAVE_FILE){
				set_text_color(73,158,255,255);
				render_shadow_text_string_at(xPos,yPos,fileName);
				set_text_color(255,255,255,255);
				print_star_count(buffer,sizeof(buffer),item->starCount);
				s32 size = get_text_width(buffer);
				render_shadow_text_string_at(boxRightEdge-maxTimeSize-16+(maxTimeSize-size),yPos,buffer);
			}
		}
		
		yPos -= PRACTICE_REPLAY_HEIGHT;
		++pathIndex;
	}
	
	if (sReplayHeaderStorageSize==0){
		set_text_color(168,168,168,255);
		render_shadow_text_string_at(xPos,yPos,"(empty)");
	}
	
	return close;
}