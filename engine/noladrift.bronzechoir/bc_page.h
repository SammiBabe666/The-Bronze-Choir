/* The Bronze Choir's pages (bc_page.c): T1, the four swinging bells; T2, the struck bell's modes. */
#ifndef BC_PAGE_H
#define BC_PAGE_H

#include "opus_plugin.h"
#include "bc_dsp.h"

void bc_page_init(struct opl_self *self);
void bc_page_t1(void);                     /* display thread, every frame on T1 */
void bc_page_t2(const struct bc_voice *v); /* display thread, every frame on T2 */

/* bronzechoir.c's, for the page: the sound's preset (1-8, 0 none) and whether its knobs are still the preset's. */
int bc_preset_now(void);
int bc_preset_intact(const short *k);

/* The page's physics, one frame (also run by the computer's renders): exposed for the tests. */
struct bc_swing {
    int ang, vel;                          /* the bell's angle and speed, 1/16 of opus_sincos's 1/4096 turn */
    int cl, cv;                            /* the clapper's */
    int flash;                             /* the strike light's frames left */
    unsigned seen;                         /* the strikes counted so far */
    int dir;                               /* the next push's way */
};
extern struct bc_swing bc_swings[BC_BELLS];
void bc_swing_step(const short *k);

#endif
