#ifndef BROWSER_H
#define BROWSER_H

#include "sm64.h"
#include "practice.h"
#include "replay.h"

#define REPLAY_MAX_PATH 512

void export_practice_replay(const char* path,const Replay* replay);
void import_practice_replay(const char* path,Replay** replay);
void reexport_practice_replay(const char* path);

void button_export_current_replay(PracticeSetting*);
void button_reexport_all_replays(PracticeSetting*);

void browser_export_current_save(void);

void browser_init(void);
void browser_free(void);
u8 render_browser(void);

#endif