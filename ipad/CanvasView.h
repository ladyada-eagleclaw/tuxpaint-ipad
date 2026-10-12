/* Canvas-only zoom and touch routing. No UIKit or SDL dependency, for host tests. */
#ifndef TP_CANVAS_VIEW_H
#define TP_CANVAS_VIEW_H
#include <stdint.h>

typedef struct {
  float left, top, width, height;
  float zoom, x, y;
  int active;
} TPCanvasView;

enum { TP_TOUCH_DOWN, TP_TOUCH_MOVE, TP_TOUCH_UP, TP_VIEW_CHANGED };
typedef struct {
  int type, canvas;
  float x, y;
} TPPointerEvent;
typedef struct {
  int used;
  int64_t id;
  float x, y;
} TPFinger;
typedef struct {
  TPFinger fingers[8];
  int count, mode, canvas;
  int64_t primary;
  uint32_t started;
  float anchor_x, anchor_y, initial_zoom, initial_distance;
  TPPointerEvent pending[128], output[512];
  int pending_count, read, write;
} TPTouchInput;

void tp_view_init(TPCanvasView *view, float left, float top, float width, float height);
void tp_view_fit(TPCanvasView *view);
int tp_view_contains(const TPCanvasView *view, float x, float y);
void tp_view_to_canvas(const TPCanvasView *view, float *x, float *y);
void tp_view_to_screen(const TPCanvasView *view, float *x, float *y);
void tp_touch_reset(TPTouchInput *input);
void tp_touch_event(TPTouchInput *input, TPCanvasView *view, int type,
                    int64_t id, float x, float y, uint32_t time);
void tp_touch_tick(TPTouchInput *input, uint32_t time);
int tp_touch_next(TPTouchInput *input, TPPointerEvent *event);
#endif
