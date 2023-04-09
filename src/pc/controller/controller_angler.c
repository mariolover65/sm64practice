#include <stdbool.h>
#include <ultra64.h>

#include "controller_api.h"

#include "../configfile.h"
#include "controller_angler.h"

#define ANGLER_Q_SCANCODE 16
#define ANGLER_W_SCANCODE 17
#define ANGLER_E_SCANCODE 18
#define ANGLER_R_SCANCODE 19

#define ANGLER_A_SCANCODE 30
#define ANGLER_S_SCANCODE 31
#define ANGLER_D_SCANCODE 32
#define ANGLER_F_SCANCODE 33

#define ANGLER_Z_SCANCODE 44
#define ANGLER_X_SCANCODE 45
#define ANGLER_NP_DIVIDE_SCANCODE 309
#define ANGLER_NP_TIMES_SCANCODE 55
#define ANGLER_LCTRL_SCANCODE 29

#define ANGLER_C_SCANCODE 46
#define ANGLER_V_SCANCODE 47
#define ANGLER_G_SCANCODE 34

#define ANGLER_TAB_SCANCODE 15

#define ANGLER_NP_0_SCANCODE 82
#define ANGLER_NP_1_SCANCODE 79
#define ANGLER_NP_2_SCANCODE 80
#define ANGLER_NP_3_SCANCODE 81
#define ANGLER_NP_3_SCANCODE 81
#define ANGLER_NP_4_SCANCODE 75
#define ANGLER_NP_5_SCANCODE 76
#define ANGLER_NP_6_SCANCODE 77
#define ANGLER_NP_7_SCANCODE 71
#define ANGLER_NP_8_SCANCODE 72
#define ANGLER_NP_9_SCANCODE 73

#define ANGLER_NP_ENTER_SCANCODE 284
#define ANGLER_NP_DOT_SCANCODE 83
#define ANGLER_NP_PLUS_SCANCODE 78
#define ANGLER_NP_MINUS_SCANCODE 74

#define ANGLER_UP_SCANCODE 328
#define ANGLER_LEFT_SCANCODE 331
#define ANGLER_DOWN_SCANCODE 336
#define ANGLER_RIGHT_SCANCODE 333

#define ANGLER_SPACE_SCANCODE 57

static u32 angler_buttons;

static float accX,accY;
static int accCount;

static const float upleftX = -0.38268343237f; // cos(PI*5.0f/8.0f);
static const float upleftY =  0.92387953251f; // sin(PI*5.0f/8.0f);

static u8 angler_keys_array[384] = {};

bool angler_on_key_down(int scancode) {
	if (scancode<384){
		angler_keys_array[scancode] = 1;
		return true;
	}
	return false;
}

bool angler_on_key_up(int scancode) {
	if (scancode<384){
		angler_keys_array[scancode] = 0;
		return true;
	}
	return false;
}

void angler_on_all_keys_up(void) {
	bzero(angler_keys_array,384);
}

static void angler_init(void) {
	bzero(angler_keys_array,384);
}

static void add_left(void) {
	accX -= 1.0f;
	++accCount;
}

static void add_right(void) {
	accX += 1.0f;
	++accCount;
}

static void add_up(void) {
	accY += 1.0f;
	++accCount;
}

static void add_down(void) {
	accY -= 1.0f;
	++accCount;
}

static void add_upleft(void) {
	accX += upleftX;
	accY += upleftY;
	++accCount;
}

static void add_upright(void) {
	accX -= upleftX;
	accY += upleftY;
	++accCount;
}

static void add_downleft(void) {
	accX += upleftX;
	accY -= upleftY;
	++accCount;
}

static void add_downright(void) {
	accX -= upleftX;
	accY -= upleftY;
	++accCount;
}

static void add_slight_upleft(void) {
	accX -= upleftY;
	accY -= upleftX;
	++accCount;
}

static void add_slight_upright(void) {
	accX += upleftY;
	accY -= upleftX;
	++accCount;
}

static void update_angles(void) {
	float mag;
	
	accX = 0.0f;
	accY = 0.0f;
	accCount = 0;
	
	if (angler_keys_array[ANGLER_W_SCANCODE]&&
		angler_keys_array[ANGLER_R_SCANCODE]){
		add_slight_upright();
		++accCount;
	} else if (angler_keys_array[ANGLER_W_SCANCODE]){
		add_upleft();
	} else if (angler_keys_array[ANGLER_R_SCANCODE]){
		add_downright();
	}
	
	if (angler_keys_array[ANGLER_Q_SCANCODE]&&
		angler_keys_array[ANGLER_E_SCANCODE]){
		add_slight_upleft();
		++accCount;
	} else if (angler_keys_array[ANGLER_Q_SCANCODE]){
		add_downleft();
	} else if (angler_keys_array[ANGLER_E_SCANCODE]){
		add_upright();
	}
	
	if (angler_keys_array[ANGLER_A_SCANCODE]&&
		angler_keys_array[ANGLER_F_SCANCODE]){
		if (!angler_keys_array[ANGLER_S_SCANCODE])
			add_up();
	} else if (angler_keys_array[ANGLER_A_SCANCODE]){
		add_left();
	} else if (angler_keys_array[ANGLER_F_SCANCODE]){
		add_right();
	}
	if (angler_keys_array[ANGLER_S_SCANCODE]&&
		angler_keys_array[ANGLER_D_SCANCODE]){
		add_down();
	} else if (angler_keys_array[ANGLER_S_SCANCODE]){
		add_down();
	} else if (angler_keys_array[ANGLER_D_SCANCODE]){
		add_up();
	}
	
	if (angler_keys_array[ANGLER_V_SCANCODE]){
		accY *= 0.5f;
	}
	
	if (angler_keys_array[ANGLER_C_SCANCODE]){
		accX *= 0.33f;
	}
	
	if (angler_keys_array[ANGLER_G_SCANCODE]){
		accX *= 0.5f;
	}
	
	if (accCount!=0){
		mag = sqrt(accX*accX+accY*accY);
		accX /= mag;
		accY /= mag;
	}
	
	if (angler_keys_array[ANGLER_NP_MINUS_SCANCODE]){
		accX = 1.0f;
		accY = 0.0f;
		accCount = 1;
	} else if (angler_keys_array[ANGLER_Z_SCANCODE]){
		accX = -1.0f;
		accY = 0.0f;
		accCount = 1;
	}
	
	if (angler_keys_array[ANGLER_SPACE_SCANCODE]){
		accX *= 0.32f;
		accY *= 0.32f;
	}
	if (angler_keys_array[ANGLER_NP_DOT_SCANCODE]){
		accX *= 0.1f;
		accY *= 0.1f;
	}
}

static void update_buttons(void) {
	angler_buttons = 0;
	
	if (angler_keys_array[ANGLER_NP_3_SCANCODE]||angler_keys_array[ANGLER_NP_1_SCANCODE]){
		angler_buttons |= A_BUTTON;
	}
	if (angler_keys_array[ANGLER_NP_2_SCANCODE]||angler_keys_array[ANGLER_LCTRL_SCANCODE]){
		angler_buttons |= B_BUTTON;
	}
	if (angler_keys_array[ANGLER_NP_ENTER_SCANCODE]){
		angler_buttons |= Z_TRIG;
	}
	if (angler_keys_array[ANGLER_NP_4_SCANCODE]){
		angler_buttons |= L_CBUTTONS;
	}
	if (angler_keys_array[ANGLER_NP_5_SCANCODE]){
		angler_buttons |= D_CBUTTONS;
	}
	if (angler_keys_array[ANGLER_NP_6_SCANCODE]){
		angler_buttons |= R_CBUTTONS;
	}
	if (angler_keys_array[ANGLER_NP_8_SCANCODE]){
		angler_buttons |= U_CBUTTONS;
	}
	if (angler_keys_array[ANGLER_NP_9_SCANCODE]){
		angler_buttons |= R_TRIG;
	}
	if (angler_keys_array[ANGLER_NP_7_SCANCODE]){
		angler_buttons |= L_TRIG;
	}
	if (angler_keys_array[ANGLER_NP_0_SCANCODE]){
		angler_buttons |= START_BUTTON;
	}
	if (angler_keys_array[ANGLER_LEFT_SCANCODE]){
		angler_buttons |= L_JPAD;
	}
	if (angler_keys_array[ANGLER_RIGHT_SCANCODE]){
		angler_buttons |= R_JPAD;
	}
	if (angler_keys_array[ANGLER_UP_SCANCODE]){
		angler_buttons |= U_JPAD;
	}
	if (angler_keys_array[ANGLER_DOWN_SCANCODE]){
		angler_buttons |= D_JPAD;
	}
}

static void angler_read(OSContPad *pad) {
	update_angles();
	update_buttons();
	
	pad->button |= angler_buttons;
    
	const s8 xstick = (s8)(accX*127.0f);
	const s8 ystick = (s8)(accY*127.0f);
	
	pad->stick_x = xstick;
	pad->stick_y = ystick;
}


struct ControllerAPI controller_angler = {
    0x0000,
    angler_init,
    angler_read,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};
