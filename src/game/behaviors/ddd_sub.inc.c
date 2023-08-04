// ddd_sub.c.inc

extern s32 gPracticeSubStatus;

void bhv_bowsers_sub_loop(void) {
	if (gPracticeSubStatus!=1){
		if ((save_file_get_flags() & (SAVE_FLAG_HAVE_KEY_2 | SAVE_FLAG_UNLOCKED_UPSTAIRS_DOOR)) || gPracticeSubStatus==2)
			obj_mark_for_deletion(o);
	}
}
