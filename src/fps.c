#include "fps.h"

#include <stdio.h>  /* snprintf */
#include <string.h> /* memset */

void fps_reset(FpsMeter *m)
{
    memset(m, 0, sizeof(*m));
}

void fps_frame(FpsMeter *m, uint64_t now_ms, uint64_t work_ms)
{
    if (!m->started) {
        /* The first frame only starts the clock: there is no "since" yet. */
        m->started = true;
        m->window_start = now_ms;
        return;
    }
    m->frames++;
    m->work_ms += work_ms;
    /* A clock that went backwards (it should not) counts as no time, not as
     * the huge number an unsigned subtraction would give. */
    uint64_t span = now_ms > m->window_start ? now_ms - m->window_start : 0;
    if (span < FPS_WINDOW_MS) {
        return;
    }
    /* Frames over the time they took, not frames per fixed half second: a
     * slow frame may carry the span to 530 ms, and that is in the average. */
    m->per_second = (float)m->frames * 1000.0f / (float)span;
    m->work_per_frame = (float)m->work_ms / (float)m->frames;
    m->has_value = true;
    m->window_start = now_ms;
    m->frames = 0;
    m->work_ms = 0;
}

void fps_text(const FpsMeter *m, char *out, size_t size)
{
    if (!m->has_value) {
        snprintf(out, size, "-- fps");
        return;
    }
    /* %.0f rounds to the nearest whole number; %.1f keeps one decimal. */
    snprintf(out, size, "%.0f fps, %.1f ms", (double)m->per_second,
             (double)m->work_per_frame);
}
