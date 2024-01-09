#include <stdio.h>
#include <ultra64.h>

#include "controller_api.h"

static FILE *fp;

#define TAS_VALID_BUTTONS (A_BUTTON   | B_BUTTON   | Z_TRIG     | START_BUTTON | \
                           R_TRIG     |                                          \
                           U_CBUTTONS | D_CBUTTONS | L_CBUTTONS | R_CBUTTONS   )

static void tas_init(void) {
    fp = fopen("cont.m64", "rb");
    if (fp != NULL) {
        uint8_t buf[0x400];
        fread(buf, 1, sizeof(buf), fp);
    }
}

static void tas_read(OSContPad *pad) {
    if (fp != NULL) {
        uint8_t bytes[4] = {0};
		if (!feof(fp))
			fread(bytes, 1, 4, fp);
		else {
			fclose(fp);
			fp = NULL;
		}
        pad->button = (bytes[0] << 8) | bytes[1];
		pad->button &= TAS_VALID_BUTTONS;
        pad->stick_x = bytes[2];
        pad->stick_y = bytes[3];
    }
}

static void tas_shutdown(void) {
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
}

static u32 tas_rawkey(void) {
    return VK_INVALID;
}

struct ControllerAPI controller_recorded_tas = {
    VK_INVALID,
    tas_init,
    tas_read,
    tas_rawkey,
    NULL, // no rumble_play
    NULL, // no rumble_stop
    NULL, // no rebinding
    tas_shutdown
};
