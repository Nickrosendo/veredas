#include "frame_log.h"

#include <errno.h>
#include <string.h> /* strerror */

void frame_log_init(FrameLog *f)
{
    *f = (FrameLog){0}; /* out is NULL: off */
}

bool frame_log_open(FrameLog *f, const char *path, char *err, size_t len)
{
    frame_log_init(f);
    f->out = fopen(path, "w");
    if (f->out == NULL) {
        snprintf(err, len, "--frame-log: cannot write %s: %s", path, strerror(errno));
        return false;
    }
    /* gap: from the last frame's start to this one's, what the eye sees; work:
     * this frame's own time, the gap less the wait before the next. */
    fputs("frame gap_ms work_ms world_ms camera_ms other_ms draw_ms present_ms "
          "ticks camera_ticks camera_copied drawn prelude zoom tile_builds "
          "sprite_builds\n",
          f->out);
    return true;
}

void frame_log_begin(FrameLog *f, uint64_t now_us)
{
    if (f->out == NULL) {
        return;
    }
    f->start = now_us;
    f->mark = now_us;
    for (int i = 0; i < FRAME_PARTS; i++) {
        f->part_us[i] = 0;
    }
}

void frame_log_part(FrameLog *f, int part, uint64_t now_us)
{
    if (f->out == NULL || part < 0 || part >= FRAME_PARTS) {
        return;
    }
    /* += : a part may be cut in two places (the world's ticks before the
     * camera, say, and more of the same after). */
    f->part_us[part] += now_us - f->mark;
    f->mark = now_us;
}

/* Microseconds as milliseconds. The cast: a frame's time is far below the
 * 2^53 microseconds where a double stops holding every whole number. */
static double ms(uint64_t us)
{
    return (double)us / 1000.0;
}

void frame_log_end(FrameLog *f, uint64_t now_us, const FrameFacts *facts)
{
    if (f->out == NULL) {
        return;
    }
    /* The first frame has no frame before it: its gap is 0. */
    uint64_t gap = f->last_start > 0 ? f->start - f->last_start : 0;
    fprintf(f->out, "%ld %.2f %.2f %.2f %.2f %.2f %.2f %.2f %d %d %d %d %d %d %ld %ld\n",
            f->frame, ms(gap), ms(now_us - f->start), ms(f->part_us[FRAME_PART_WORLD]),
            ms(f->part_us[FRAME_PART_CAMERA]), ms(f->part_us[FRAME_PART_OTHER]),
            ms(f->part_us[FRAME_PART_DRAW]), ms(f->part_us[FRAME_PART_PRESENT]),
            facts->ticks, facts->camera_ticks, facts->camera_copied ? 1 : 0,
            facts->drawn ? 1 : 0, facts->prelude ? 1 : 0, facts->zoom, facts->tile_builds,
            facts->sprite_builds);
    f->last_start = f->start;
    f->frame++;
}

void frame_log_close(FrameLog *f)
{
    if (f->out != NULL) {
        fclose(f->out);
        f->out = NULL;
    }
}
