#include <stdlib.h>
#include <stdio.h>

#ifdef TARGET_WEB
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include "sm64.h"

#include "game/memory.h"
#include "audio/external.h"

#include "gfx/gfx_pc.h"

#include "gfx/gfx_opengl.h"
#include "gfx/gfx_direct3d11.h"
#include "gfx/gfx_direct3d12.h"

#include "gfx/gfx_dxgi.h"
#include "gfx/gfx_sdl.h"

#include "audio/audio_api.h"
#include "audio/audio_sdl.h"
#include "audio/audio_null.h"

#include "pc_main.h"
#include "cliopts.h"
#include "configfile.h"
#include "controller/controller_api.h"
#include "controller/controller_keyboard.h"
#include "controller/controller_angler.h"
#include "controller/controller_recorded_tas.h"
#include "fs/fs.h"

#include "game/game_init.h"
#include "game/main.h"
#include "game/thread6.h"

#ifdef DISCORDRPC
#include "pc/discord/discordrpc.h"
#endif

#include "game/save_state.h"
#include "game/stats.h"
#include "game/practice.h"

OSMesg D_80339BEC;
OSMesgQueue gSIEventMesgQueue;

s8 gResetTimer;
s8 D_8032C648;
s8 gDebugLevelSelect;
s8 gShowProfiler;
s8 gShowDebugText;
s8 gResetTrigger;

s32 gRumblePakPfs;
struct RumbleData gRumbleDataQueue[3];
struct StructSH8031D9B0 gCurrRumbleSettings;

static struct AudioAPI *audio_api;
static struct GfxWindowManagerAPI *wm_api;
static struct GfxRenderingAPI *rendering_api;

extern void gfx_run(Gfx *commands);
extern void thread5_game_loop(void *arg);
extern void create_next_audio_buffer(s16 *samples, u32 num_samples);
void game_loop_one_iteration(void);

void dispatch_audio_sptask(UNUSED struct SPTask *spTask) {
}

void set_vblank_handler(UNUSED s32 index, UNUSED struct VblankHandler *handler, UNUSED OSMesgQueue *queue, UNUSED OSMesg *msg) {
}

static bool inited = false;

#include "game/display.h" // for gGlobalTimer
void send_display_list(struct SPTask *spTask) {
    if (!inited) return;
    gfx_run((Gfx *)spTask->task.t.data_ptr);
}

#ifdef VERSION_EU
#define SAMPLES_HIGH 656
#define SAMPLES_LOW 640
#else
#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528
#endif

void produce_one_frame(void) {
    gfx_start_frame();

    const f32 master_mod = (f32)configMasterVolume / 127.0f;
    set_sequence_player_volume(SEQ_PLAYER_LEVEL, (f32)configMusicVolume / 127.0f * master_mod);
    set_sequence_player_volume(SEQ_PLAYER_SFX, (f32)configSfxVolume / 127.0f * master_mod);
    set_sequence_player_volume(SEQ_PLAYER_ENV, (f32)configEnvVolume / 127.0f * master_mod);

    game_loop_one_iteration();
    thread6_rumble_loop(NULL);

    int samples_left = audio_api->buffered();
    u32 num_audio_samples = samples_left < audio_api->get_desired_buffered() ? SAMPLES_HIGH : SAMPLES_LOW;
    //printf("Audio samples: %d %u\n", samples_left, num_audio_samples);
    s16 audio_buffer[SAMPLES_HIGH * 2 * 2];
    for (int i = 0; i < 2; i++) {
        /*if (audio_cnt-- == 0) {
            audio_cnt = 2;
        }
        u32 num_audio_samples = audio_cnt < 2 ? 528 : 544;*/
        create_next_audio_buffer(audio_buffer + i * (num_audio_samples * 2), num_audio_samples);
    }
    //printf("Audio samples before submitting: %d\n", audio_api->buffered());
	
	audio_api->play((u8 *)audio_buffer, 2 * num_audio_samples * 4);

    gfx_end_frame();
}

void audio_shutdown(void) {
    if (audio_api) {
        if (audio_api->shutdown) audio_api->shutdown();
        audio_api = NULL;
    }
}

void game_deinit(void) {
#ifdef DISCORDRPC
    discord_shutdown();
#endif
    configfile_save(configfile_name());
	stats_save(PRACTICE_STATS_PATH);
	stats_cleanup();
	practice_deinit();
    controller_shutdown();
    audio_shutdown();
    gfx_shutdown();
    inited = false;
}

void game_exit(void) {
    game_deinit();
#ifndef TARGET_WEB
    exit(0);
#endif
}

#ifdef TARGET_WEB
static void em_main_loop(void) {
}

static void request_anim_frame(void (*func)(double time)) {
    EM_ASM(requestAnimationFrame(function(time) {
        dynCall("vd", $0, [time]);
    }), func);
}

static void on_anim_frame(double time) {
    static double target_time = 0.0;

    time *= 0.03; // milliseconds to frame count (33.333 ms -> 1)

    if (time >= target_time + 10.0) {
        // We are lagging 10 frames behind, probably due to coming back after inactivity,
        // so reset, with a small margin to avoid potential jitter later.
        target_time = time - 0.010;
    }
	
	while (time >= target_time){
		produce_one_frame();
		target_time += 1.0;
	}
    
    if (inited) // only continue if the init flag is still set
        request_anim_frame(on_anim_frame);
}
#endif
//void soft_reset(void);

extern u16 gRandomSeed16;
extern struct SaveBuffer gSaveBuffer;

void main_func(void) {
    const char *gamedir = gCLIOpts.GameDir[0] ? gCLIOpts.GameDir : FS_BASEDIR;
    const char *userpath = gCLIOpts.SavePath[0] ? gCLIOpts.SavePath : sys_user_path();
    fs_init(sys_ropaths, gamedir, userpath);
	
	stats_init();
	
    configfile_load(configfile_name());
	//configAnglerOverride = false;
	stats_load(PRACTICE_STATS_PATH);

    if (gCLIOpts.FullScreen == 1)
        configWindow.fullscreen = true;
    else if (gCLIOpts.FullScreen == 2)
        configWindow.fullscreen = false;

    const size_t poolsize = gCLIOpts.PoolSize ? gCLIOpts.PoolSize : DEFAULT_POOL_SIZE;
    u64 *pool = malloc(poolsize);
    if (!pool) sys_fatal("Could not alloc %u bytes for main pool.\n", poolsize);
    main_pool_init(pool, pool + poolsize / sizeof(pool[0]));
    gEffectsMemoryPool = mem_pool_init(0x4000, MEMORY_POOL_LEFT);

    #if defined(WAPI_SDL1) || defined(WAPI_SDL2)
    wm_api = &gfx_sdl;
    #elif defined(WAPI_DXGI)
    wm_api = &gfx_dxgi;
    #else
    #error No window API!
    #endif

    #if defined(RAPI_D3D11)
    rendering_api = &gfx_direct3d11_api;
    # define RAPI_NAME "DirectX 11"
    #elif defined(RAPI_D3D12)
    rendering_api = &gfx_direct3d12_api;
    # define RAPI_NAME "DirectX 12"
    #elif defined(RAPI_GL) || defined(RAPI_GL_LEGACY)
    rendering_api = &gfx_opengl_api;
    # ifdef USE_GLES
    #  define RAPI_NAME "OpenGL ES"
    # else
    #  define RAPI_NAME "OpenGL"
    # endif
    #else
    #error No rendering API!
    #endif

    char window_title[128] =
    "sm64practice (" RAPI_NAME ")"
    #ifdef NIGHTLY
    " nightly " GIT_HASH
    #endif
    ;
    
    gfx_init(wm_api, rendering_api, window_title);
    
    if (configAnglerOverride)
        wm_api->set_keyboard_callbacks(angler_on_key_down, angler_on_key_up, angler_on_all_keys_up);
    else
        wm_api->set_keyboard_callbacks(keyboard_on_key_down, keyboard_on_key_up, keyboard_on_all_keys_up);

    #if defined(AAPI_SDL1) || defined(AAPI_SDL2)
    if (audio_api == NULL && audio_sdl.init()) 
        audio_api = &audio_sdl;
    #endif

    if (audio_api == NULL) {
        audio_api = &audio_null;
    }
	
	printf("Object size: %lld\n",sizeof(struct Object));
	printf("SaveState size: %lld\n",sizeof(SaveState));
	printf("Save size: %lld\n",2*sizeof(struct SaveFile));
	printf("practice type: %p\n",&configPracticeType);
	printf("globaltimer: %p\n",&gGlobalTimer);
	printf("sectiontimer: %p\n",&gSectionTimer);
	printf("controllerbits: %p\n",&gControllerBits);
	printf("controller pad: %p\n",&gControllerPads[0]);
	printf("anim: %p\n",&gMarioState->action);
	printf("anim timer: %p\n",&gMarioState->actionTimer);
	printf("level: %p\n",&gCurrLevelNum);
	printf("nonstop: %p\n",&configNonstop);
	printf("introskip: %p\n",&configSkipIntro);
	printf("saveflags: %p\n",&gSaveBuffer.files[0]->flags);
	printf("stars: %p\n",&gSaveBuffer.files[0]->courseStars);
	printf("star count: %p\n",&gMarioState->numStars);
	printf("file num: %p\n",&gCurrSaveFileNum);
	printf("mario obj: %p\n",&gMarioObject);
	printf("anim timer offset: %lld\n",(u8*)&gMarioObject->header.gfx.unk38.animTimer-(u8*)gMarioObject);
	printf("rng: %p\n",&gRandomSeed16);
	printf("coin count: %p\n",&gMarioState->numCoins);

/*


#define EX_FILE_SELECT_TIMER_OFFSET 0xf9e3c6
#define EX_TRANSITION_OFFSET 0xf957b4
*/
    audio_init();
    sound_init();

    thread5_game_loop(NULL);
	
	practice_init();

    inited = true;

#ifdef EXTERNAL_DATA
    // precache data if needed
    if (configPrecacheRes) {
        fprintf(stdout, "precaching data\n");
        fflush(stdout);
        gfx_precache_textures();
    }
#endif

#ifdef DISCORDRPC
    discord_init();
#endif

#ifdef TARGET_WEB
    emscripten_set_main_loop(em_main_loop, 0, 0);
    request_anim_frame(on_anim_frame);
#else
    while (true) {
        /*if (gResetTrigger){
            gResetTrigger = 0;
            soft_reset();
        }*/
		if (gDisableRendering)
			game_loop_one_iteration();
		else
			wm_api->main_loop(produce_one_frame);
#ifdef DISCORDRPC
        discord_update_rich_presence();
#endif
    }
#endif
}

int main(int argc, char *argv[]) {
    parse_cli_opts(argc, argv);
    main_func();
    return 0;
}
