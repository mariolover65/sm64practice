#ifndef STATS_H
#define STATS_H

#include "sm64.h"

#define PRACTICE_STATS_PATH "practice_stats.dat"
#define STATS_INACTIVITY_THRESHOLD 600
#define STATS_AUTOSAVE_THRESHOLD 10000

typedef struct LevelStats LevelStats;

struct LevelStats {
	LevelStats* next;
	u8 levelNum;
	u8 areaIdx;
	
	u32 timePracticing;
	u32 starGrabCount;
	u32 coinCount;
	u32 levelResetCount;
};

typedef struct {
	u32 timePracticing;
	u32 levelResetCount;
	u32 stateLoadCount;
	u32 starGrabCount;
	u32 coinCount;
	f64 distanceMoved;
	
	LevelStats* levelStatsList;
} PracticeStats;

extern PracticeStats gPracticeStats;
extern u32 gInactivityTimer;

void stats_init(void);
void stats_cleanup(void);
u8 stats_load(const char* path);
u8 stats_save(const char* path);

void stats_update(void);
void stats_get_level_info(u8 levelNum,u8 areaIdx,u32* time,u32* stars,u32* coins,u32* resets);

void stats_star_grab(void);
void stats_collect_coins(u32 count);
void stats_level_reset(u8 level,u8 area);

#endif