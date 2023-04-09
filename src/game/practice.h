#ifndef PRACTICE_H
#define PRACTICE_H

#include "replay.h"

extern struct WarpDest gPracticeDest;
extern u8 gPracticeWarping;
extern u8 gSaveStateWarpDelay;
extern u8 gNoStarSelectWarp;
extern struct WarpDest gLastWarpDest;

extern Replay gPracticeReplay;

void practice_init(void);
s32 practice_update(void);

#endif