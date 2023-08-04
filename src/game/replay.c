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

static struct MarioAnimation sGhostAnim;
static u8 sGhostTargetAnim[0x4000];

struct Controller gStoredReplayController;

struct GraphNodeGenerated gGhostFuncNode;
struct GraphNodeObject gGhostMario;

Replay* gCurrRecordingReplay = NULL;
Replay* gCurrPlayingReplay = NULL;

GhostData* gCurrGhostData = NULL;
GhostFrame* gCurrGhostFrame = NULL;
GhostAreaChange* gCurrGhostArea = NULL;
u32 gGhostAreaCounter = 0;

f32 gGhostDistanceScaling = 0.0f;

static RLEChunk* sCurrRecordFrame = NULL;
static RLEChunk* sCurrReplayFrame = NULL;
u8 sCurrReplaySubframe = 0;

static OSContPad* sOldControllerData;
static OSContPad sReplayController;
static s16 sReplayLastButtons = 0;

static s32 sReplayBalance = 0;

Replay* alloc_replay(void){
	Replay* replay = malloc(sizeof(Replay));
	replay->flags = 0;
	replay->length = 0;
	replay->practiceType = 0;
	replay->starCount = 1;
	replay->state.levelState = NULL;
	replay->data = NULL;
	++sReplayBalance;
	
	return replay;
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
	
	newReplay->flags = replayToCopy->flags;
	newReplay->practiceType = replayToCopy->practiceType;
	newReplay->starCount = replayToCopy->starCount;
	
	newReplay->state.type = replayToCopy->state.type;
	if (replayToCopy->state.type==LEVEL_INIT){
		newReplay->state.levelState = malloc(sizeof(LevelInitState));
		memcpy(newReplay->state.levelState,replayToCopy->state.levelState,sizeof(LevelInitState));
	} else if (replayToCopy->state.type==SAVE_STATE){
		newReplay->state.saveState = malloc(sizeof(SaveState));
		memcpy(newReplay->state.saveState,replayToCopy->state.saveState,sizeof(SaveState));
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
	
	printf("copied length: %d\n",newReplay->length);
	
	return newReplay;
}

void free_replay(Replay* replay){
	if (replay->state.levelState!=NULL){
		if (replay->state.type==LEVEL_INIT){
			free(replay->state.levelState);
		} else if (replay->state.type==SAVE_STATE){
			free(replay->state.saveState);
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
	--sReplayBalance;
}

void init_replay_record(Replay* replay,u8 isLevelInit,u8 practiceType,u16 starCount){
	printf("recording started...\n");
	
	gCurrRecordingReplay = replay;
	replay->length = 0;
	replay->practiceType = practiceType;
	replay->starCount = starCount;
	if (isLevelInit){
		replay->state.type = LEVEL_INIT;
		replay->state.levelState = malloc(sizeof(LevelInitState));
		save_level_init_state(replay->state.levelState,&gLastWarpDest);
	} else {
		replay->state.type = SAVE_STATE;
		replay->state.saveState = malloc(sizeof(SaveState));
		save_state(replay->state.saveState);
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
	printf("continuing recording...\n");
	gCurrRecordingReplay = replay;
	
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
	
	gCurrRecordingReplay->length = get_replay_length(gCurrRecordingReplay);
	printf("Replay length: %d\n",gCurrRecordingReplay->length);
	gCurrRecordingReplay = NULL;
}

u32 get_replay_length(Replay* replay){
	u32 s = 0;
	RLEChunk* chunk = replay->data->next;
	while (chunk!=NULL){
		s += chunk->length+1;
		chunk = chunk->next;
	}
	return s;
}

void init_playing_replay(Replay* replay){
	printf("playing back...\n");
	printf("length: %d\n",replay->length);
	gCurrPlayingReplay = replay;
	sCurrReplayFrame = replay->data->next;
	sCurrReplaySubframe = 0;
	
	if (configUseGhost)
		ghost_data_create();
	
	sReplayLastButtons = ((LevelInitState*)replay->state.levelState)->lastButtons;
	sReplayController.button = sReplayLastButtons;
}

void finish_replay(void){
	printf("Playback finished\n");
	gCurrPlayingReplay = NULL;
	gCurrGhostFrame = NULL;
	gCurrGhostArea = NULL;
	gGhostAreaCounter = 0;
	if (gReplaySkipToEnd){
		gReplaySkipToEnd = FALSE;
		gDisableRendering = FALSE;
		
		if (configUseGhost){
			ghost_start_playback();
			practice_reset();
		}
	}
}

void update_replay(void){
	if (gCurrPlayingReplay!=NULL){
		if (sCurrReplayFrame==NULL){
			finish_replay();
			return;
		}
		
		if (configUseGhost)
			ghost_add_frame();
		
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
	memcpy(&gStoredReplayController,controller,sizeof(struct Controller));
	
	controller->rawStickX = sReplayController.stick_x;
	controller->rawStickY = sReplayController.stick_y;
	
	controller->buttonPressed &= ~REPLAY_BUTTON_MASK;
	controller->buttonPressed |= sReplayController.button & (~sReplayLastButtons);
	controller->buttonDown &= ~REPLAY_BUTTON_MASK;
	controller->buttonDown |= sReplayController.button;
	
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

void serialize_replay(FILE* file,const Replay* replay){
	fwrite(REPLAY_MAGIC,4,1,file);
	
	u32 version = REPLAY_VERSION;
	SERIALIZE_VAR(version);
	
	SERIALIZE_VAR(replay->practiceType);
	SERIALIZE_VAR(replay->starCount);
	
	SERIALIZE_VAR(replay->flags);
	SERIALIZE_VAR(replay->length);
	
	serialize_level_init_state(file,replay->state.levelState);
	
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
	
	DESERIALIZE_VAR(replay->practiceType);
	DESERIALIZE_VAR(replay->starCount);
	
	DESERIALIZE_VAR(replay->flags);
	DESERIALIZE_VAR(replay->length);
	u32 l = replay->length;
	
	replay->state.type = LEVEL_INIT;
	replay->state.levelState = malloc(sizeof(LevelInitState));
	if (!deserialize_level_init_state(file,replay->state.levelState)){
		return FALSE;
	}
	
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
	
	DESERIALIZE_VAR(header->practiceType);
	DESERIALIZE_VAR(header->starCount);
	
	DESERIALIZE_VAR(header->flags);
	DESERIALIZE_VAR(header->length);
	
	header->isLevelInit = TRUE;
	
	return TRUE;
}

static f32 sGhostMinDist = 80.0f;
static f32 sGhostFadeDist = 96.0f;

Gfx* ghost_update(s32 callContext, UNUSED struct GraphNode* node, UNUSED void* context){
	if (!configUseGhost){
		gGhostMario.node.flags &= ~1;
		return NULL;
	}
	
	struct Object* mario = gMarioState->marioObj;
	if (!mario) return NULL;
	
	switch (callContext){
        case GEO_CONTEXT_RENDER:
			if (!gCurrGhostData||!gCurrGhostFrame||gCurrPlayingReplay){
				gGhostMario.node.flags &= ~1;
				break;
			}
			
			if (gCurrGhostArea->levelNum!=gCurrLevelNum||gCurrGhostArea->areaIdx!=gCurrAreaIndex){
				gGhostMario.node.flags &= ~1;
				break;
			}
			
			gGhostMario.sharedChild = mario->header.gfx.sharedChild;
			gGhostMario.unk18 = mario->header.gfx.unk18;
			
			vec3s_copy(gGhostMario.angle, gCurrGhostFrame->angle);
			vec3f_copy(gGhostMario.pos, gCurrGhostFrame->pos);
			
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
	
	gCurrGhostData = malloc(sizeof(GhostData));
	gCurrGhostData->data = NULL;
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
		if (gCurrGhostArea)
			gGhostAreaCounter = gCurrGhostArea->length;
	}
}

void ghost_finish_record(void){
	if (!gCurrGhostData) return;
	
	gCurrGhostFrame = NULL;
	gCurrGhostArea = NULL;
	gGhostAreaCounter = 0;
	printf("Finished recording ghost\n");
}

void ghost_start_playback(void){
	if (!gCurrGhostData) return;
	
	gCurrGhostFrame = gCurrGhostData->data;
	gCurrGhostArea = gCurrGhostData->changeData;
	gGhostAreaCounter = gCurrGhostArea->length;
	printf("Starting ghost playback\n");
	
}