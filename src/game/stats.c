#include <PR/ultratypes.h>

#include "stats.h"
#include "practice.h"
#include "area.h"
#include "game_init.h"
#include "pc/fs/fs.h"

#include <stdio.h>

#define PRACTICE_STATS_VERSION 0x1

#define SERIALIZE_VAR(v) fwrite(&(v),sizeof(v),1,file)
#define DESERIALIZE_VAR(v) fread(&(v),sizeof(v),1,file)

PracticeStats gPracticeStats;
u32 gInactivityTimer = 0;
static u8 sIsInactive = FALSE;
static u32 sAutosaveTimer = 0;

static LevelStats* make_level_stats(u8 level,u8 area){
	LevelStats* stats = malloc(sizeof(LevelStats));
	stats->levelNum = level;
	stats->areaIdx = area;
	stats->next = NULL;
	
	stats->distanceMoved = 0.0;
	stats->timePracticing = 0;
	stats->starGrabCount = 0;
	stats->coinCount = 0;
	stats->levelResetCount = 0;
	
	return stats;
}

void stats_init(void){
	gPracticeStats.timePracticing = 0;
	gPracticeStats.levelResetCount = 0;
	gPracticeStats.stateLoadCount = 0;
	gPracticeStats.starGrabCount = 0;
	gPracticeStats.coinCount = 0;
	gPracticeStats.distanceMoved = 0.0;
	
	gPracticeStats.levelStatsList = NULL;
}

static void stats_serialize_level_stats(FILE* file,const LevelStats* levelStats){
	SERIALIZE_VAR(levelStats->levelNum);
	SERIALIZE_VAR(levelStats->areaIdx);
	u32 time = levelStats->timePracticing;
	
	if (gInactivityTimer<STATS_INACTIVITY_THRESHOLD){
		// subtract inactive time if we are here
		if (levelStats->levelNum==gCurrLevelNum&&
			levelStats->areaIdx==gCurrAreaIndex){
			if (time>=gInactivityTimer){
				time -= gInactivityTimer;
			} else {
				time = 0;
			}
		}
	}
	
	SERIALIZE_VAR(time);
	//f64 dist = 0.0;
	SERIALIZE_VAR(levelStats->distanceMoved);
	SERIALIZE_VAR(levelStats->starGrabCount);
	SERIALIZE_VAR(levelStats->coinCount);
	SERIALIZE_VAR(levelStats->levelResetCount);
}

static void stats_deserialize_level_stats(FILE* file,LevelStats* levelStats){
	DESERIALIZE_VAR(levelStats->levelNum);
	DESERIALIZE_VAR(levelStats->areaIdx);
	DESERIALIZE_VAR(levelStats->timePracticing);
	DESERIALIZE_VAR(levelStats->distanceMoved);
	DESERIALIZE_VAR(levelStats->starGrabCount);
	DESERIALIZE_VAR(levelStats->coinCount);
	DESERIALIZE_VAR(levelStats->levelResetCount);
}

static u16 stats_get_level_stats_count(const LevelStats* levelStats){
	u16 s=0;
	while (levelStats!=NULL){
		++s;
		levelStats = levelStats->next;
	}
	
	return s;
}

u8 stats_load(const char* path){
	FILE* file;
	
	printf("Loading stats from '%s'\n", path);
	
	file = fopen(fs_get_write_path(path),"rb");
    if (file == NULL) {
        // Create a new stats file and save defaults
        printf("Stats file '%s' not found. Creating it.\n", path);
        stats_save(path);
        return TRUE;
    }
	
	u32 version;
	DESERIALIZE_VAR(version);
	
	if (version!=PRACTICE_STATS_VERSION){
		printf("Stats file has bad version! (%u)\n",version);
		return FALSE;
	}
	DESERIALIZE_VAR(gPracticeStats.timePracticing);
	DESERIALIZE_VAR(gPracticeStats.levelResetCount);
	DESERIALIZE_VAR(gPracticeStats.stateLoadCount);
	DESERIALIZE_VAR(gPracticeStats.starGrabCount);
	DESERIALIZE_VAR(gPracticeStats.coinCount);
	DESERIALIZE_VAR(gPracticeStats.distanceMoved);
	
	u16 count;
	DESERIALIZE_VAR(count);
	if (count==65535)
		--count;
	
	gPracticeStats.levelStatsList = NULL;
	LevelStats* currLevel;
	
	for (u16 i=0;i<count;++i){
		currLevel = make_level_stats(0,0);
		stats_deserialize_level_stats(file,currLevel);
		currLevel->next = gPracticeStats.levelStatsList;
		gPracticeStats.levelStatsList = currLevel;
	}
	
	fclose(file);
	
	return TRUE;
}

u8 stats_save(const char* path){
	FILE* file;
	
    printf("Saving stats to '%s'\n", path);

    file = fopen(fs_get_write_path(path), "wb");
    if (file == NULL){
		printf("Failed to write stats to '%s'\n", path);
        return FALSE;
    }
	
	u32 version = PRACTICE_STATS_VERSION;
	SERIALIZE_VAR(version);
	
	u32 time = gPracticeStats.timePracticing;
	if (gInactivityTimer<STATS_INACTIVITY_THRESHOLD){
		if (time>=gInactivityTimer){
			time -= gInactivityTimer;
		} else {
			time = 0;
		}
	}
	
	SERIALIZE_VAR(time);
	SERIALIZE_VAR(gPracticeStats.levelResetCount);
	SERIALIZE_VAR(gPracticeStats.stateLoadCount);
	SERIALIZE_VAR(gPracticeStats.starGrabCount);
	SERIALIZE_VAR(gPracticeStats.coinCount);
	SERIALIZE_VAR(gPracticeStats.distanceMoved);
	
	u16 levelCount = stats_get_level_stats_count(gPracticeStats.levelStatsList);
	SERIALIZE_VAR(levelCount);
	LevelStats* currLevel = gPracticeStats.levelStatsList;
	for (u16 i=0;i<levelCount;++i){
		stats_serialize_level_stats(file,currLevel);
		currLevel = currLevel->next;
	}
	
	fclose(file);
	
	return TRUE;
}

void stats_cleanup(void){
	LevelStats* list = gPracticeStats.levelStatsList;
	if (list==NULL) return;
	
	LevelStats* next = list->next;
	while (next!=NULL){
		free(list);
		list = next;
		next = next->next;
	}
	free(list);
	gPracticeStats.levelStatsList = NULL;
}

static LevelStats* get_or_make_level_stats(u8 level,u8 area){
	LevelStats* levelStats = gPracticeStats.levelStatsList;
	
	while (levelStats!=NULL){
		if (levelStats->levelNum==level&&levelStats->areaIdx==area)
			return levelStats;
		levelStats = levelStats->next;
	}
	
	LevelStats* newStats = make_level_stats(level,area);
	newStats->next = gPracticeStats.levelStatsList;
	
	gPracticeStats.levelStatsList = newStats;
	printf("Made new stats for level %d area %d\n",level,area);
	
	return newStats;
}

static LevelStats* get_or_make_curr_level_stats(void){
	return get_or_make_level_stats(gCurrLevelNum,gCurrAreaIndex);
}

static LevelStats* sCurrLevelStats = NULL;

void stats_get_level_info(u8 levelNum,u8 areaIdx,u32* time,u32* stars,u32* coins,u32* resets,f64* dist){
	LevelStats* stats = gPracticeStats.levelStatsList;
	while (stats!=NULL){
		if (stats->levelNum==levelNum){
			if (areaIdx==0||areaIdx==stats->areaIdx){
				*time += stats->timePracticing;
				*stars += stats->starGrabCount;
				*coins += stats->coinCount;
				*resets += stats->levelResetCount;
				*dist += stats->distanceMoved;
			}
		}
		
		stats = stats->next;
	}
}

void stats_star_grab(void){
	if (gIsRTA&&!gCurrPlayingReplay&&!gCurrDemoInput){
		++gPracticeStats.starGrabCount;
		LevelStats* stats = get_or_make_curr_level_stats();
		++stats->starGrabCount;
	}
}

void stats_collect_coins(u32 count){
	if (gIsRTA&&!gCurrPlayingReplay&&!gCurrDemoInput){
		gPracticeStats.coinCount += count;
		LevelStats* stats = get_or_make_curr_level_stats();
		stats->coinCount += count;
	}
}

void stats_add_dist(f64 dist){
	if (gIsRTA&&!gCurrPlayingReplay&&!gCurrDemoInput){
		gPracticeStats.distanceMoved += dist;
		LevelStats* stats = get_or_make_curr_level_stats();
		stats->distanceMoved += dist;
	}
}

void stats_level_reset(u8 level,u8 area){
	//if (!gCurrPlayingReplay&&!gCurrDemoInput){
		++gPracticeStats.levelResetCount;
		
		LevelStats* stats = get_or_make_level_stats(level,area);
		++stats->levelResetCount;
	//}
}

void stats_update(void){
	if (sCurrLevelStats==NULL||
		sCurrLevelStats->levelNum!=gCurrLevelNum||
		sCurrLevelStats->areaIdx!=gCurrAreaIndex){
		sCurrLevelStats = get_or_make_curr_level_stats();
	}
	
	if (gInactivityTimer>=STATS_INACTIVITY_THRESHOLD){
		if (!sIsInactive){
			sIsInactive = TRUE;
			// remove time spent inactive
			if (gPracticeStats.timePracticing>=gInactivityTimer)
				gPracticeStats.timePracticing -= gInactivityTimer;
			else
				gPracticeStats.timePracticing = 0;
			
			if (sCurrLevelStats->timePracticing>=gInactivityTimer)
				sCurrLevelStats->timePracticing -= gInactivityTimer;
			else
				sCurrLevelStats->timePracticing = 0;
				
		}
		return;
	}
	
	sIsInactive = FALSE;
	
	if (!gRenderPracticeMenu){
		++gInactivityTimer;
		++gPracticeStats.timePracticing;
		++sCurrLevelStats->timePracticing;
	}
	
	++sAutosaveTimer;
	
	if (sAutosaveTimer>STATS_AUTOSAVE_THRESHOLD){
		stats_save(PRACTICE_STATS_PATH);
		sAutosaveTimer = 0;
		printf("Autosaved stats!\n");
	}
}