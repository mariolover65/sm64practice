#include <PR/ultratypes.h>

#include "replay.h"
#include "game_init.h"
#include "practice.h"
#include "save_state.h"
#include "shadow.h"
#include "engine/math_util.h"
#include "engine/graph_node.h"
#include "pc/configfile.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>

static struct MarioAnimation sGhostAnim;
static u8 sGhostTargetAnim[0x4000];

struct Controller gStoredReplayController;

struct GraphNodeGenerated gGhostFuncNode;
struct GraphNodeObject gGhostMario;

Replay* gCurrRecordingReplay = NULL;
Replay* gCurrPlayingReplay = NULL;

Replay* gReplayHistory[REPLAY_HISTORY_LENGTH];

u8 gIsRecordingGhost = FALSE;
GhostData* gCurrGhostData = NULL;
GhostFrame* gCurrGhostFrame = NULL;
GhostAreaChange* gCurrGhostArea = NULL;
u32 gGhostAreaCounter = 0;
u32 gGhostDataIndex = 0;

f32 gGhostDistanceScaling = 0.0f;

static RLEChunk* sCurrRecordFrame = NULL;
static RLEChunk* sCurrReplayFrame = NULL;
u8 sCurrReplaySubframe = 0;

static OSContPad sReplayController;
static s16 sReplayLastButtons = 0;

s32 gReplayBalance = 0;

InputFrame get_current_inputs(void){
	InputFrame fr;
	if (!sCurrReplayFrame){
		fr.buttons = 0;
		fr.stickX = 0;
		fr.stickY = 0;
		return fr;
	}
	
	fr = sCurrReplayFrame->controls;
	return fr;
}

Replay* alloc_replay(void){
	Replay* replay = malloc(sizeof(Replay));
	replay->flags = 0;
	replay->length = 0;
	replay->practiceType = 0;
	replay->starCount = 1;
	replay->state.levelState = NULL;
	replay->data = NULL;
	++gReplayBalance;
	
	return replay;
}

void free_replay(Replay* replay){
	if (replay->state.levelState!=NULL){
		if (replay->state.type==LEVEL_INIT){
			free(replay->state.levelState);
		} else if (replay->state.type==GAME_INIT){
			free(replay->state.gameState);
		}
	}
	RLEChunk* curr = replay->data;
	RLEChunk* next;
	while (curr!=NULL){
		next = curr->next;
		free(curr);
		curr = next;
	}
	
	free(replay);
	--gReplayBalance;
}

void archive_replay(Replay* replay){
	if (gReplayHistory[REPLAY_HISTORY_LENGTH-1])
		free_replay(gReplayHistory[REPLAY_HISTORY_LENGTH-1]);
	
	// move forward
	for (u32 i=REPLAY_HISTORY_LENGTH-1;i>0;--i){
		gReplayHistory[i] = gReplayHistory[i-1];
	}
	
	gReplayHistory[0] = replay;
}

void archive_load_replay(Replay** into,u32 index){
	if (index>=REPLAY_HISTORY_LENGTH)
		return;
	
	Replay* unarch = gReplayHistory[index];
	for (s32 i=index;i>0;--i){
		gReplayHistory[i] = gReplayHistory[i-1];
	}
	gReplayHistory[0] = *into;
	*into = unarch;
}

void clear_replay_history(void){
	for (u32 i=0;i<REPLAY_HISTORY_LENGTH;++i){
		if (gReplayHistory[i]){
			free_replay(gReplayHistory[i]);
			gReplayHistory[i] = NULL;
		}
	}
}

static void pop_input_at_end(Replay* replay){
	RLEChunk* prev = NULL;
	RLEChunk* frame = replay->data->next;
	if (frame==NULL) return;
	
	while (frame->next!=NULL){
		prev = frame;
		frame = frame->next;
	}
	
	if (frame->length==0){
		free(frame);
		prev->next = NULL;
	} else {
		--frame->length;
	}
}

Replay* copy_replay(const Replay* replayToCopy,const RLEChunk* until,u8 subframe){
	Replay* newReplay = alloc_replay();
	
	newReplay->date = replayToCopy->date;
	newReplay->flags = replayToCopy->flags;
	newReplay->practiceType = replayToCopy->practiceType;
	newReplay->starCount = replayToCopy->starCount;
	
	newReplay->state.type = replayToCopy->state.type;
	if (replayToCopy->state.type==LEVEL_INIT){
		newReplay->state.levelState = malloc(sizeof(LevelInitState));
		memcpy(newReplay->state.levelState,replayToCopy->state.levelState,sizeof(LevelInitState));
	} else if (replayToCopy->state.type==GAME_INIT){
		newReplay->state.gameState = malloc(sizeof(GameInitState));
		memcpy(newReplay->state.gameState,replayToCopy->state.gameState,sizeof(GameInitState));
	}
	
	RLEChunk* newFrame = malloc(sizeof(RLEChunk));
	newFrame->next = NULL;
	newReplay->data = newFrame;
	newFrame->length = 255;
	
	const RLEChunk* frame = replayToCopy->data->next;
	if (frame){
		newFrame->next = malloc(sizeof(RLEChunk));
		newFrame = newFrame->next;
		newFrame->next = NULL;
		while (1){
			newFrame->controls = frame->controls;
			newFrame->length = frame->length;
			newReplay->length += newFrame->length+1;
			
			frame = frame->next;
			if (frame==NULL){
				break;
			}
			
			if (frame==until){
				if (subframe>=1){
					newFrame->next = malloc(sizeof(RLEChunk));
					newFrame = newFrame->next;
					newFrame->next = NULL;
					newFrame->controls = frame->controls;
					newFrame->length = subframe-1;
					newReplay->length += subframe;
				}
				break;
			}
			
			newFrame->next = malloc(sizeof(RLEChunk));
			newFrame = newFrame->next;
			newFrame->next = NULL;
		}
	}
	
	return newReplay;
}

void init_replay_record(Replay* replay,u8 practiceType,u16 starCount){
	gCurrRecordingReplay = replay;
	replay->length = 0;
	replay->flags = 0;
	replay->date = time(NULL);
	replay->practiceType = practiceType;
	replay->starCount = starCount;
	if (practiceType!=PRACTICE_TYPE_GAME){
		replay->state.type = LEVEL_INIT;
		replay->state.levelState = malloc(sizeof(LevelInitState));
		save_level_init_state(replay->state.levelState,&gLastWarpDest);
	} else {
		replay->state.type = GAME_INIT;
		replay->state.gameState = malloc(sizeof(GameInitState));
		save_game_init_state(replay->state.gameState);
	}
	
	replay->data = malloc(sizeof(RLEChunk));
	replay->data->next = NULL;
	sCurrRecordFrame = replay->data;
	replay->data->controls.buttons = 0;
	replay->data->controls.stickX = 0;
	replay->data->controls.stickY = 0;
	replay->data->length = 255;
	// skip this frame
}

void init_replay_record_at_end(Replay* replay){
	gCurrRecordingReplay = replay;
	replay->flags &= ~REPLAY_FLAG_RESET;
	
	if (replay->data->next==NULL){
		sCurrRecordFrame = replay->data;
		return;
	}
	
	sCurrRecordFrame = replay->data->next;
	while (sCurrRecordFrame->next!=NULL){
		sCurrRecordFrame = sCurrRecordFrame->next;
	}
}

u8 frames_match(InputFrame a,InputFrame b){
	return a.buttons==b.buttons && a.stickX==b.stickX && a.stickY==b.stickY;
}

void add_frame(void){
	if (gCurrRecordingReplay!=NULL){
		OSContPad* controller = gPlayer1Controller->controllerData;
		
		InputFrame controls;
		controls.buttons = controller->button & REPLAY_BUTTON_MASK;
		controls.stickX = controller->stick_x;
		controls.stickY = controller->stick_y;
		
		if (!(sCurrRecordFrame->controls.buttons & A_BUTTON) && (controls.buttons & A_BUTTON)){
			gCurrRecordingReplay->flags |= REPLAY_FLAG_A_PRESS;
		}
		
		if (!frames_match(sCurrRecordFrame->controls,controls)||sCurrRecordFrame->length==255){
			sCurrRecordFrame->next = malloc(sizeof(RLEChunk));
			sCurrRecordFrame = sCurrRecordFrame->next;
			sCurrRecordFrame->length = 0;
			sCurrRecordFrame->controls = controls;
			sCurrRecordFrame->next = NULL;
		} else {
			++sCurrRecordFrame->length;
		}
		
		++gCurrRecordingReplay->length;
		
		if (gCurrGhostFrame){
			ghost_advance_frame();
		}
	}
}

void end_replay_record(void){
	if (!gCurrRecordingReplay) return;
	gCurrRecordingReplay = NULL;
}

u32 get_replay_length(const Replay* replay){
	/*u32 s = 0;
	RLEChunk* chunk = replay->data->next;
	while (chunk!=NULL){
		s += chunk->length+1;
		chunk = chunk->next;
	}
	return s;*/
	if (replay->state.type!=GAME_INIT){
		return replay->length;
	}
	
	GameInitState* state = replay->state.gameState;
	if (state->introSkip&&state->introSkipTiming){
		return replay->length+INTRO_SKIP_TIME_START-14;
	}
	return replay->length;
}

u32 get_replay_header_length(const ReplayFileHeader* replay){
	if (!replay->length)
		return 0;
	
	if (replay->isLevelInit){
		return replay->length;
	}
	
	if (replay->introSkip&&replay->introSkipTiming){
		return replay->length+INTRO_SKIP_TIME_START-14;
	}
	return replay->length;
}

void init_playing_replay(Replay* replay){
	gCurrPlayingReplay = replay;
	sCurrReplayFrame = replay->data->next;
	sCurrReplaySubframe = 0;
	
	if (gIsRecordingGhost)
		ghost_data_create();
	
	sReplayLastButtons = 0;
	if (replay->state.levelState)
		sReplayLastButtons = ((LevelInitState*)replay->state.levelState)->lastButtons;
	sReplayController.button = sReplayLastButtons;
}

void finish_replay(void){
	gCurrPlayingReplay = NULL;
	//gCurrGhostFrame = NULL;
	//gCurrGhostArea = NULL;
	//gGhostAreaCounter = 0;
	if (gReplaySkipToEnd){
		gReplaySkipToEnd = FALSE;
		gDisableRendering = FALSE;
		
	}
	if (gIsRecordingGhost){
		ghost_finish_record();
		ghost_start_playback();
		gWillPracticeReset = TRUE;
		gIsRecordingGhost = FALSE;
		practice_set_message("Ghost created");
	} else {
		practice_set_message("Playback finished");
	}
}

void update_replay(void){
	if (gCurrPlayingReplay!=NULL){
		if (sCurrReplayFrame==NULL){
			finish_replay();
			return;
		}
		
		if (gIsRecordingGhost){
			ghost_add_frame();
		} else if (gCurrGhostFrame){
			ghost_advance_frame();
		}
		
		InputFrame controls = sCurrReplayFrame->controls;
		sReplayLastButtons = sReplayController.button;
		sReplayController.button = controls.buttons;
		sReplayController.stick_x = controls.stickX;
		sReplayController.stick_y = controls.stickY;
		
		if (++sCurrReplaySubframe==(u8)(sCurrReplayFrame->length+1)){
			sCurrReplayFrame = sCurrReplayFrame->next;
			sCurrReplaySubframe = 0;
		}
	}
}

void replay_overwrite_inputs(struct Controller* controller){
	gStoredReplayController.buttonPressed = 
		controller->controllerData->button & (controller->controllerData->button ^ gStoredReplayController.buttonDown);
	gStoredReplayController.buttonDown = controller->controllerData->button;
	
	controller->rawStickX = sReplayController.stick_x;
	controller->rawStickY = sReplayController.stick_y;
	
	controller->buttonPressed &= ~REPLAY_BUTTON_MASK;
	controller->buttonPressed |= sReplayController.button & (~sReplayLastButtons);
	controller->buttonDown &= ~REPLAY_BUTTON_MASK;
	controller->buttonDown |= sReplayController.button;
	
	if (controller->buttonPressed & A_BUTTON){
		++gAPressCounter;
	}
	
	adjust_analog_stick(controller);
	copy_to_player_3();
}

void get_current_replay_pos(RLEChunk** replayFrame,u8* replaySubframe){
	*replayFrame = sCurrReplayFrame;
	*replaySubframe = sCurrReplaySubframe;
}

#define SERIALIZE_VAR(v) fwrite(&(v),sizeof(v),1,file)
#define DESERIALIZE_VAR(v) fread(&(v),sizeof(v),1,file)

#define REPLAY_MAGIC "pre\x05"
#define REPLAY_VERSION 0x1

#define MTR_REPLAY_MAGIC "MTRR"
#define MTR_REPLAY_VERSION 0x0

void serialize_replay(FILE* file,const Replay* replay){
	fwrite(REPLAY_MAGIC,4,1,file);
	
	u32 version = REPLAY_VERSION;
	SERIALIZE_VAR(version);
	
	SERIALIZE_VAR(replay->date);
	
	SERIALIZE_VAR(replay->practiceType);
	SERIALIZE_VAR(replay->starCount);
	
	SERIALIZE_VAR(replay->flags);
	SERIALIZE_VAR(replay->length);
	
	SERIALIZE_VAR(replay->state.type);
	if (replay->state.type==LEVEL_INIT)
		serialize_level_init_state(file,replay->state.levelState);
	else if (replay->state.type==GAME_INIT)
		serialize_game_init_state(file,replay->state.gameState);
	
	RLEChunk* data = replay->data->next;
	while (data!=NULL){
		SERIALIZE_VAR(data->controls);
		SERIALIZE_VAR(data->length);
		
		data = data->next;
	}
}

u8 deserialize_replay(FILE* file,Replay* replay){
	char magic[4];
	DESERIALIZE_VAR(magic);
	if (memcmp(magic,REPLAY_MAGIC,4)!=0){
		printf("Bad replay magic! %u %u %u %u\n",magic[0],magic[1],magic[2],magic[3]);
		return FALSE;
	}
	
	u32 version = 0;
	DESERIALIZE_VAR(version);
	if (version!=REPLAY_VERSION){
		printf("Bad replay version! %u\n",version);
		return FALSE;
	}
	
	DESERIALIZE_VAR(replay->date);
	DESERIALIZE_VAR(replay->practiceType);
	DESERIALIZE_VAR(replay->starCount);
	
	DESERIALIZE_VAR(replay->flags);
	DESERIALIZE_VAR(replay->length);
	u32 l = replay->length;
	
	replay->state.levelState = NULL;
	DESERIALIZE_VAR(replay->state.type);
	if (replay->state.type==LEVEL_INIT){
		replay->state.levelState = malloc(sizeof(LevelInitState));
		if (!deserialize_level_init_state(file,replay->state.levelState)){
			return FALSE;
		}
	} else if (replay->state.type==GAME_INIT){
		replay->state.gameState = malloc(sizeof(GameInitState));
		if (!deserialize_game_init_state(file,replay->state.gameState)){
			return FALSE;
		}
	}
	
	assert(replay->data==NULL);
	if (replay->data==NULL){
		replay->data = malloc(sizeof(RLEChunk));
		replay->data->length = 255;
		replay->data->next = NULL;
	}
	
	RLEChunk* data = replay->data;
	while (l!=0){
		data->next = malloc(sizeof(RLEChunk));
		data = data->next;
		
		DESERIALIZE_VAR(data->controls);
		DESERIALIZE_VAR(data->length);
		
		if ((u32)((u32)data->length+1)>l){
			return FALSE;
		}
		
		l -= data->length+1;
	}
	data->next = NULL;
	
	return TRUE;
}

u8 deserialize_replay_header(FILE* file,ReplayFileHeader* header){
	char magic[4];
	DESERIALIZE_VAR(magic);
	if (memcmp(magic,REPLAY_MAGIC,4)!=0){
		printf("Bad replay magic! %u %u %u %u\n",magic[0],magic[1],magic[2],magic[3]);
		return FALSE;
	}
	
	u32 version = 0;
	DESERIALIZE_VAR(version);
	if (version!=REPLAY_VERSION){
		printf("Bad replay version! %u\n",version);
		return FALSE;
	}
	
	DESERIALIZE_VAR(header->date);
	DESERIALIZE_VAR(header->practiceType);
	DESERIALIZE_VAR(header->starCount);
	
	DESERIALIZE_VAR(header->flags);
	DESERIALIZE_VAR(header->length);
	
	header->isLevelInit = TRUE;
	u8 type;
	DESERIALIZE_VAR(type);

	if (type==GAME_INIT){
		header->isLevelInit = FALSE;
		GameInitState state;
		if (!deserialize_game_init_state(file,&state)){
			return FALSE;
		}
		header->introSkip = state.introSkip;
		header->introSkipTiming = state.introSkipTiming;
	}
	
	return TRUE;
}

#define MTR_FILE_SELECT_REPLAY_FLAG 	0x1
#define MTR_INTRO_SKIP_REPLAY_FLAG		0x2
#define MTR_NONSTOP_REPLAY_FLAG 		0x4

u8 deserialize_mtr_replay(FILE* file,Replay* replay){
	char magic[4];
	DESERIALIZE_VAR(magic);
	if (memcmp(magic,MTR_REPLAY_MAGIC,4)!=0){
		printf("Bad replay magic! %u %u %u %u\n",magic[0],magic[1],magic[2],magic[3]);
		return FALSE;
	}
	
	u32 version = 0;
	DESERIALIZE_VAR(version);
	if (version!=MTR_REPLAY_VERSION){
		printf("Bad replay version! %u\n",version);
		return FALSE;
	}
	u32 startTime;
	DESERIALIZE_VAR(startTime);
	u16 rngVal;
	DESERIALIZE_VAR(rngVal);
	
	replay->date = 0;
	replay->practiceType = PRACTICE_TYPE_GAME;
	replay->starCount = 0;
	replay->flags = 0;
	replay->length = 0;
	
	
	replay->state.gameState = malloc(sizeof(GameInitState));
	GameInitState* gis = replay->state.gameState;
	u8 options;
	DESERIALIZE_VAR(options);
	gis->introSkip = FALSE;
	gis->introSkipTiming = TRUE;
	gis->nonstop = FALSE;
	gis->noInvisibleWalls = configNoInvisibleWalls;
	gis->stageText = PRACTICE_OP_DEFAULT;
	if (options & MTR_INTRO_SKIP_REPLAY_FLAG){
		gis->introSkip = TRUE;
	}
	if (options & MTR_NONSTOP_REPLAY_FLAG){
		gis->nonstop = TRUE;
	}
	
	replay->state.type = GAME_INIT;
	
	u64 chunkCount;
	DESERIALIZE_VAR(chunkCount);
	assert(replay->data==NULL);
	if (replay->data==NULL){
		replay->data = malloc(sizeof(RLEChunk));
		replay->data->controls.stickX = 0;
		replay->data->controls.stickY = 0;
		replay->data->controls.buttons = 0;
		replay->data->length = 255;
		replay->data->next = NULL;
	}
	
	RLEChunk* data = replay->data;
	
	// add 12 empty frames
	data->next = malloc(sizeof(RLEChunk));
	data = data->next;
	data->controls.stickX = 0;
	data->controls.stickY = 0;
	data->controls.buttons = 0;
	data->length = 11;
	
	u32 len = data->length+1;
	u32 controls;
	for (u64 i=0;i<chunkCount;++i){
		data->next = malloc(sizeof(RLEChunk));
		data = data->next;
		
		DESERIALIZE_VAR(controls);
		DESERIALIZE_VAR(data->length);
		len += data->length+1;
		
		data->controls.stickY = controls&0xFF;
		data->controls.stickX = (controls>>8)&0xFF;
		data->controls.buttons = (controls>>16)&0xFFFF;
		//controls = BE_TO_HOST32(controls);
		//memcpy(&data->controls,&controls,sizeof(InputFrame));
	}
	data->next = NULL;
	replay->length = len;
	if (chunkCount>=1){
		// copy first frame of input to the empty frames
		replay->data->next->controls = replay->data->next->next->controls;
	}
	
	return TRUE;
}

u8 deserialize_mtr_replay_header(FILE* file,ReplayFileHeader* header){
	char magic[4];
	DESERIALIZE_VAR(magic);
	if (memcmp(magic,MTR_REPLAY_MAGIC,4)!=0){
		printf("Bad replay magic! %u %u %u %u\n",magic[0],magic[1],magic[2],magic[3]);
		return FALSE;
	}
	
	u32 version = 0;
	DESERIALIZE_VAR(version);
	if (version!=MTR_REPLAY_VERSION){
		printf("Bad replay version! %u\n",version);
		return FALSE;
	}
	
	u32 startTime;
	DESERIALIZE_VAR(startTime);
	u16 rngVal;
	DESERIALIZE_VAR(rngVal);
	
	header->date = 0;
	header->practiceType = PRACTICE_TYPE_GAME;
	header->starCount = 0;
	header->flags = 0;
	header->length = 0;
	header->isLevelInit = FALSE;
	
	header->introSkipTiming = TRUE;
	u8 options;
	DESERIALIZE_VAR(options);
	if (options & MTR_INTRO_SKIP_REPLAY_FLAG){
		header->introSkip = TRUE;
	}
	
	//DESERIALIZE_VAR(header->length);

	return TRUE;
}

static f32 sGhostMinDist = 80.0f;
static f32 sGhostFadeDist = 96.0f;

u8 get_ghost_instant_warp_offset(Vec3f offset){
	u8 ghostArea = gCurrGhostArea->areaIdx;
	struct InstantWarp* warp;
	Vec3f into;
	for (u32 i=0;i<4;++i){
		warp = &gCurrentArea->instantWarps[i];
		
		if (warp->id&&warp->area==ghostArea){
			vec3s_to_vec3f(into,warp->displacement);
			vec3f_sub(offset,into);
			return TRUE;
		}
	}
	return FALSE;
}

Gfx* ghost_update(s32 callContext, UNUSED struct GraphNode* node, UNUSED void* context){
	if (!configUseGhost){
		gGhostMario.node.flags &= ~1;
		return NULL;
	}
	
	struct Object* mario = gMarioState->marioObj;
	if (!mario) return NULL;
	
	Vec3f instantWarpOffset;
	vec3f_copy(instantWarpOffset,gVec3fZero);
	
	switch (callContext){
        case GEO_CONTEXT_RENDER:
			if (!gCurrGhostData||!gCurrGhostFrame||gIsRecordingGhost){
				gGhostMario.node.flags &= ~1;
				break;
			}
			
			if (gCurrGhostArea->levelNum!=gCurrLevelNum){
				gGhostMario.node.flags &= ~1;
				break;
			}
			
			// fix instant warps
			if (gCurrentArea && gCurrentArea->instantWarps && gCurrGhostArea->areaIdx!=gCurrAreaIndex){
				if (!get_ghost_instant_warp_offset(instantWarpOffset)){
					gGhostMario.node.flags &= ~1;
					break;
				}
			} else if (gCurrGhostArea->areaIdx!=gCurrAreaIndex){
				gGhostMario.node.flags &= ~1;
				break;
			}
			
			gGhostMario.sharedChild = mario->header.gfx.sharedChild;
			gGhostMario.unk18 = mario->header.gfx.unk18;
			
			vec3s_copy(gGhostMario.angle, gCurrGhostFrame->angle);
			vec3f_copy(gGhostMario.pos, gCurrGhostFrame->pos);
			vec3f_add(gGhostMario.pos,instantWarpOffset);
			
			Vec3f dest;
			vec3f_dif(dest,gGhostMario.pos,gMarioState->pos);
			f32 dist = vec3f_length(dest)-sGhostMinDist;
			if (dist<0.0f) dist = 0.0f;
			dist /= sGhostFadeDist;
			if (dist>1.0f) dist = 1.0f;
			gGhostDistanceScaling = dist;
			
			struct Animation* targetAnim = sGhostAnim.targetAnim;
		
			if (load_patchable_table(&sGhostAnim,gCurrGhostFrame->animID)) {
				targetAnim->values = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->values);
				targetAnim->index = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->index);
			}
			
			if (gGhostMario.unk38.animID != gCurrGhostFrame->animID) {
				gGhostMario.unk38.animID = gCurrGhostFrame->animID;
				gGhostMario.unk38.curAnim = targetAnim;
				gGhostMario.unk38.animAccel = 0;
				gGhostMario.unk38.animYTrans = gMarioStates[0].unkB0;

				if (targetAnim->flags & ANIM_FLAG_2) {
					gGhostMario.unk38.animFrame = targetAnim->unk04;
				} else {
					if (targetAnim->flags & ANIM_FLAG_FORWARD) {
						gGhostMario.unk38.animFrame = targetAnim->unk04 + 1;
					} else {
						gGhostMario.unk38.animFrame = targetAnim->unk04 - 1;
					}
				}
			}
			
			gGhostMario.unk38.animFrame = gCurrGhostFrame->animFrame;
			gGhostMario.node.flags |= 1;
			break;
	}
	
	return NULL;
}

u8 ghost_is_parent(struct GraphNode* node){
	while (1){
		if (node==(struct GraphNode*)&gGhostMario) return TRUE;
		if (!node->parent) return FALSE;
		node = node->parent;
	}
	return FALSE;
}

void ghost_init(void){
	init_graph_node_generated(NULL, &gGhostFuncNode, ghost_update, 0);
	init_graph_node_object(NULL, &gGhostMario, NULL, gVec3fZero, gVec3sZero, gVec3fOne);
	geo_add_child((struct GraphNode*)&gGhostFuncNode, &gGhostMario.node);
	void* animStartMem = &sGhostTargetAnim[0];
	init_anim_memory(&sGhostAnim, gMarioAnims, animStartMem);
}

static struct GraphNode* find_root_camera(struct GraphNode* root){
	struct GraphNode* masterList = root->children;
	struct GraphNode* perspective = NULL;
	
	while (1){
		if (!masterList->children) continue;
		
		if (masterList->children->type==GRAPH_NODE_TYPE_PERSPECTIVE){
			perspective = masterList->children;
			break;
		}
		
		masterList = masterList->next;
		// looped
		if (masterList==root->children) break;
	}
	
	if (!perspective) return NULL;
	return perspective->children;
}

void ghost_load(struct GraphNode* root){
	if (!configUseGhost) return;
	
	gGhostFuncNode.fnNode.node.parent = NULL;
	if (gCurrGhostData){
		struct GraphNode* cam = find_root_camera(root);
		geo_add_child(cam, (struct GraphNode*)&gGhostFuncNode);
	}
}

void ghost_unload(void){
	if (gGhostFuncNode.fnNode.node.parent)
		geo_remove_child((struct GraphNode*)&gGhostFuncNode);
}

void ghost_data_free(GhostData* ghost){
	{
		GhostFrame* curr = ghost->data;
		GhostFrame* next;
		
		while (curr){
			next = curr->next;
			free(curr);
			curr = next;
		}
	}
	
	{
		GhostAreaChange* curr = ghost->changeData;
		GhostAreaChange* next;
		
		while (curr){
			next = curr->next;
			free(curr);
			curr = next;
		}
	}
	
	free(ghost);
}

void ghost_data_create(void){
	if (gCurrGhostData){
		ghost_data_free(gCurrGhostData);
	}
	
	++gGhostDataIndex;
	gCurrGhostData = malloc(sizeof(GhostData));
	gCurrGhostData->data = NULL;
	gCurrGhostData->changeData = NULL;
	gCurrGhostFrame = NULL;
	gCurrGhostArea = NULL;
}

void ghost_add_frame(void){
	if (!gCurrGhostData) return;
	
	struct Object* mario = gMarioState->marioObj;
	if (!mario) return;
	
	GhostFrame* newFrame = malloc(sizeof(GhostFrame));
	newFrame->next = NULL;
	
	vec3s_copy(newFrame->angle, mario->header.gfx.angle);
	vec3f_copy(newFrame->pos, mario->header.gfx.pos);
	newFrame->animID = mario->header.gfx.unk38.animID;
	if (newFrame->animID==-1){
		newFrame->animID = 0;
	}
	newFrame->animFrame = mario->header.gfx.unk38.animFrame;
	newFrame->model = gMarioState->marioBodyState->modelState >> 8;
	newFrame->cap = gMarioState->marioBodyState->capState;
	
	if (gCurrGhostFrame){
		gCurrGhostFrame->next = newFrame;
	} else {
		gCurrGhostData->data = newFrame;
	}
	
	gCurrGhostFrame = newFrame;
	
	if (!gCurrGhostArea||(gCurrGhostArea->levelNum!=gCurrLevelNum||gCurrGhostArea->areaIdx!=gCurrAreaIndex)){
		GhostAreaChange* newArea = malloc(sizeof(GhostAreaChange));
		newArea->next = NULL;
		newArea->levelNum = gCurrLevelNum;
		newArea->areaIdx = gCurrAreaIndex;
		newArea->length = 1;
		if (gCurrGhostArea){
			gCurrGhostArea->next = newArea;
		} else {
			gCurrGhostData->changeData = newArea;
		}
		gCurrGhostArea = newArea;
	} else {
		++gCurrGhostArea->length;
	}
}

void ghost_advance_frame(void){
	if (!gCurrGhostData) return;
	if (!gCurrGhostFrame) return;
	
	gCurrGhostFrame = gCurrGhostFrame->next;
	
	if (gCurrGhostFrame==NULL)
		return;
	
	--gGhostAreaCounter;
	if (gGhostAreaCounter==0){
		gCurrGhostArea = gCurrGhostArea->next;
		assert(gCurrGhostArea);
		gGhostAreaCounter = gCurrGhostArea->length;
	}
}

void ghost_finish_record(void){
	if (!gCurrGhostData) return;
	
	gCurrGhostFrame = NULL;
	gCurrGhostArea = NULL;
	gGhostAreaCounter = 0;
}

void ghost_start_playback(void){
	if (!gCurrGhostData) return;
	
	gCurrGhostFrame = gCurrGhostData->data;
	gCurrGhostArea = gCurrGhostData->changeData;
	gGhostAreaCounter = gCurrGhostArea->length;
}