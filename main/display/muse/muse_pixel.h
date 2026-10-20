/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * Adapted for xiaozhi-esp32.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MUSE_MODE_BOOT = 0,
    MUSE_MODE_IDLE,
    MUSE_MODE_LISTENING,
    MUSE_MODE_THINKING,
    MUSE_MODE_SPEAKING,
    MUSE_MODE_ERROR,
    MUSE_MODE_OFF,
    MUSE_MODE_COUNT,
} muse_mode_t;

typedef enum {
    MUSE_ACTION_NONE = 0,
    MUSE_ACTION_HAPPY,       /* 1. 摸头欢呼眯眼笑 (Petting / Happy) */
    MUSE_ACTION_WAVE,        /* 2. 挥手打招呼 (Wave Hello / Bye) */
    MUSE_ACTION_POKE,        /* 3. 果冻Q弹挤压下蹲 (Jelly Poke / Squash) */
    MUSE_ACTION_SLEEPY,      /* 4. 打哈欠揉眼瞌睡 (Sleepy / Nap) */
    MUSE_ACTION_SHY,         /* 5. 害羞捂脸全身粉红 (Shy / Blush) */
    MUSE_ACTION_EXCITED,     /* 6. 得意狂喜连续跳跃 (Excited / Yay) */
    MUSE_ACTION_SAD,         /* 7. 委屈巴巴八字眉泪花 (Sad / Pout) */
    MUSE_ACTION_SULKY,       /* 8. 傲娇偏头翻白眼 (Sulky / Hmph) */
    MUSE_ACTION_SHOCKED,     /* 9. 目瞪口呆大铜铃眼 (Shocked / Wow) */
    MUSE_ACTION_SNEEZE,      /* 10. 仰头蓄力大喷嚏 (Sneezing) */
    MUSE_ACTION_DIZZY,       /* 11. 蚊香眼眩晕失衡 (Dizzy) */
    MUSE_ACTION_KISS,        /* 12. 眨眼比心飞吻 (Blowing Kiss) */
    MUSE_ACTION_COUNT,
} muse_action_t;

#define MUSE_PX_W 64
#define MUSE_PX_H 64

typedef struct {
    muse_mode_t mode;
    muse_action_t action;
    float t;         /* seconds since boot */
    float mode_t;    /* seconds in current mode */
    float action_t;  /* seconds in current action */
    float level;     /* 0..1 live audio level */
    float happy;     /* 0..1 pet reaction */
    float action_intensity; /* 0..1 action intensity */
} muse_pose_t;

/* Accent colour of a mode (for the surrounding UI), as 0xRRGGBB. */
uint32_t muse_pixel_accent(muse_mode_t mode);

/* Render one frame into Muse's own MUSE_PX_W x MUSE_PX_H grid. */
void muse_pixel_render(const muse_pose_t *pose);

/* Size (square, in screen pixels) muse_pixel_scale() blows the grid up to. */
void muse_pixel_set_size(int px);

/*
 * Write screen pixels [x0, x1] x [y0, y1] of the blown-up frame as RGB565,
 * stride_px apart.
 */
void muse_pixel_scale(uint16_t *dst, int stride_px, int x0, int x1, int y0, int y1);

#ifdef __cplusplus
}
#endif
