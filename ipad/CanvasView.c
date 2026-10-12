#include "CanvasView.h"
#include <math.h>
#include <string.h>

enum { IDLE, PENDING, DRAWING, GESTURE, BLOCKED };
/* Give a second finger time to join before sending any paint events. */
#define TWO_FINGER_DELAY 120

static float clamp(float value, float low, float high)
{
  return fmaxf(low, fminf(value, high));
}

void tp_view_init(TPCanvasView *v, float left, float top, float width, float height)
{
  *v = (TPCanvasView){left, top, width, height, 1, 0, 0, 0};
}

void tp_view_fit(TPCanvasView *v)
{
  v->zoom = 1;
  v->x = v->y = 0;
}

int tp_view_contains(const TPCanvasView *v, float x, float y)
{
  return x >= v->left && y >= v->top && x < v->left + v->width && y < v->top + v->height;
}

void tp_view_to_canvas(const TPCanvasView *v, float *x, float *y)
{
  if (!v->active) return;
  *x = v->left + clamp(v->x + (*x - v->left) / v->zoom, 0, v->width - 1);
  *y = v->top + clamp(v->y + (*y - v->top) / v->zoom, 0, v->height - 1);
}

void tp_view_to_screen(const TPCanvasView *v, float *x, float *y)
{
  if (!v->active) return;
  *x = v->left + (*x - v->left - v->x) * v->zoom;
  *y = v->top + (*y - v->top - v->y) * v->zoom;
}

void tp_touch_reset(TPTouchInput *in)
{
  memset(in, 0, sizeof(*in));
}

static void emit(TPTouchInput *in, int type, float x, float y)
{
  int next = (in->write + 1) % 512;
  /* The consumer drains output before processing another input event. */
  if (next == in->read) return;
  in->output[in->write] = (TPPointerEvent){type, in->canvas, x, y};
  in->write = next;
}

int tp_touch_next(TPTouchInput *in, TPPointerEvent *out)
{
  if (in->read == in->write) return 0;
  *out = in->output[in->read];
  in->read = (in->read + 1) % 512;
  return 1;
}

static void commit_stroke(TPTouchInput *in)
{
  for (int i = 0; i < in->pending_count; i++)
    emit(in, in->pending[i].type, in->pending[i].x, in->pending[i].y);
  in->pending_count = 0;
  in->mode = DRAWING;
}

void tp_touch_tick(TPTouchInput *in, uint32_t time)
{
  if (in->mode == PENDING && time - in->started >= TWO_FINGER_DELAY)
    commit_stroke(in);
}

static int pair(TPTouchInput *in, float *x, float *y, float *distance)
{
  TPFinger *a = 0, *b = 0;
  for (int i = 0; i < 8; i++) {
    if (!in->fingers[i].used) continue;
    if (!a) a = &in->fingers[i];
    else { b = &in->fingers[i]; break; }
  }
  if (!a || !b) return 0;
  *x = (a->x + b->x) * 0.5f;
  *y = (a->y + b->y) * 0.5f;
  *distance = hypotf(a->x - b->x, a->y - b->y);
  return 1;
}

void tp_touch_event(TPTouchInput *in, TPCanvasView *v, int type,
                    int64_t id, float x, float y, uint32_t time)
{
  TPFinger *finger = 0;
  tp_touch_tick(in, time);
  for (int i = 0; i < 8; i++)
    if (in->fingers[i].used && in->fingers[i].id == id) finger = &in->fingers[i];
  if (type == TP_TOUCH_DOWN) {
    if (finger) return;
    for (int i = 0; i < 8; i++)
      if (!in->fingers[i].used) { finger = &in->fingers[i]; break; }
    if (!finger) return;
    *finger = (TPFinger){1, id, x, y};
    in->count++;
    if (in->count == 1) {
      in->primary = id;
      in->started = time;
      in->canvas = v->active && tp_view_contains(v, x, y);
      in->mode = in->canvas ? PENDING : DRAWING;
      if (in->mode == PENDING)
        in->pending[in->pending_count++] = (TPPointerEvent){type, 1, x, y};
      else emit(in, type, x, y);
    } else if (in->count == 2 && in->mode == PENDING && tp_view_contains(v, x, y)) {
      float cx, cy, distance;
      pair(in, &cx, &cy, &distance);
      in->initial_distance = fmaxf(distance, 1);
      in->initial_zoom = v->zoom;
      in->anchor_x = v->x + (cx - v->left) / v->zoom;
      in->anchor_y = v->y + (cy - v->top) / v->zoom;
      in->pending_count = 0;
      in->mode = GESTURE;
    }
    return;
  }
  if (!finger) return;
  finger->x = x;
  finger->y = y;
  if (type == TP_TOUCH_MOVE && in->mode == GESTURE) {
    float cx, cy, distance;
    if (pair(in, &cx, &cy, &distance)) {
      v->zoom = clamp(in->initial_zoom * distance / in->initial_distance, 1, 4);
      v->x = clamp(in->anchor_x - (cx - v->left) / v->zoom, 0, v->width * (1 - 1 / v->zoom));
      v->y = clamp(in->anchor_y - (cy - v->top) / v->zoom, 0, v->height * (1 - 1 / v->zoom));
      emit(in, TP_VIEW_CHANGED, cx, cy);
    }
  } else if (id == in->primary && in->mode == PENDING) {
    if (in->pending_count == 128) commit_stroke(in);
    if (in->mode == PENDING)
      in->pending[in->pending_count++] = (TPPointerEvent){type, 1, x, y};
    else emit(in, type, x, y);
    if (type == TP_TOUCH_UP) commit_stroke(in);
  } else if (id == in->primary && in->mode == DRAWING) {
    emit(in, type, x, y);
  }
  if (type == TP_TOUCH_UP) {
    finger->used = 0;
    in->count--;
    if (in->mode == GESTURE || id == in->primary) in->mode = BLOCKED;
    if (in->count == 0) { in->mode = IDLE; in->pending_count = 0; }
  }
}
