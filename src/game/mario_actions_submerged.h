#ifndef MARIO_ACTIONS_SUBMERGED_H
#define MARIO_ACTIONS_SUBMERGED_H

#include <PR/ultratypes.h>

#include "types.h"

enum SwimPracticeInfoType {
	SWIM_PRACTICE_NONE,
	SWIM_PRACTICE_HELD_TOO_LONG,
	SWIM_PRACTICE_HELD_TOO_SHORT,
	SWIM_PRACTICE_TOO_EARLY,
	SWIM_PRACTICE_TOO_LATE,
	SWIM_PRACTICE_GOOD
};

extern s32 gSwimPressFrame;
extern s32 gSwimInfo;

s32 mario_execute_submerged_action(struct MarioState *m);

#endif // MARIO_ACTIONS_SUBMERGED_H
