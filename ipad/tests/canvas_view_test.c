#include "../CanvasView.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void near(float actual, float expected)
{
  assert(fabsf(actual - expected) < 0.01f);
}

static int drain(TPTouchInput *input, int expected_type)
{
  TPPointerEvent event;
  int count = 0;
  while (tp_touch_next(input, &event)) {
    assert(event.type == expected_type);
    count++;
  }
  return count;
}

int main(void)
{
  TPCanvasView view;
  TPTouchInput input;
  TPPointerEvent event;
  tp_view_init(&view, 100, 0, 800, 600);
  view.active = 1;
  tp_touch_reset(&input);

  /* A two-finger gesture must never send a paint down/up, even on release. */
  tp_touch_event(&input, &view, TP_TOUCH_DOWN, 1, 300, 300, 10);
  tp_touch_event(&input, &view, TP_TOUCH_DOWN, 2, 500, 300, 30);
  assert(!tp_touch_next(&input, &event));
  tp_touch_event(&input, &view, TP_TOUCH_MOVE, 1, 200, 300, 40);
  tp_touch_event(&input, &view, TP_TOUCH_MOVE, 2, 600, 300, 40);
  near(view.zoom, 2);
  near(view.x, 150);
  near(view.y, 150);
  assert(drain(&input, TP_VIEW_CHANGED) == 2);

  /* The canvas point under the pinch midpoint stays under that midpoint. */
  float x = 400, y = 300;
  tp_view_to_canvas(&view, &x, &y);
  near(x, 400); near(y, 300);
  tp_view_to_screen(&view, &x, &y);
  near(x, 400); near(y, 300);

  /* Moving both fingers together pans while preserving zoom. */
  tp_touch_event(&input, &view, TP_TOUCH_MOVE, 1, 250, 340, 50);
  tp_touch_event(&input, &view, TP_TOUCH_MOVE, 2, 650, 340, 50);
  near(view.zoom, 2); near(view.x, 125); near(view.y, 130);
  drain(&input, TP_VIEW_CHANGED);
  tp_touch_event(&input, &view, TP_TOUCH_UP, 1, 250, 340, 60);
  tp_touch_event(&input, &view, TP_TOUCH_MOVE, 2, 700, 400, 70);
  tp_touch_event(&input, &view, TP_TOUCH_UP, 2, 700, 400, 80);
  assert(!tp_touch_next(&input, &event));

  /* Fast single-finger taps still produce a complete paint gesture. */
  tp_touch_event(&input, &view, TP_TOUCH_DOWN, 3, 450, 200, 200);
  tp_touch_event(&input, &view, TP_TOUCH_UP, 3, 450, 200, 220);
  assert(tp_touch_next(&input, &event) && event.type == TP_TOUCH_DOWN && event.canvas);
  assert(tp_touch_next(&input, &event) && event.type == TP_TOUCH_UP);
  assert(!tp_touch_next(&input, &event));

  /* Preserve early curved stroke points, instead of replacing them with a line. */
  tp_touch_event(&input, &view, TP_TOUCH_DOWN, 4, 200, 100, 300);
  tp_touch_event(&input, &view, TP_TOUCH_MOVE, 4, 210, 110, 320);
  tp_touch_event(&input, &view, TP_TOUCH_MOVE, 4, 220, 100, 340);
  tp_touch_tick(&input, 420);
  assert(tp_touch_next(&input, &event) && event.type == TP_TOUCH_DOWN);
  assert(tp_touch_next(&input, &event) && event.x == 210 && event.y == 110);
  assert(tp_touch_next(&input, &event) && event.x == 220 && event.y == 100);
  /* A later second finger must not erase an already-started drawing. */
  float zoom = view.zoom;
  tp_touch_event(&input, &view, TP_TOUCH_DOWN, 5, 500, 100, 500);
  tp_touch_event(&input, &view, TP_TOUCH_MOVE, 5, 800, 400, 510);
  near(view.zoom, zoom);
  tp_touch_event(&input, &view, TP_TOUCH_UP, 4, 220, 100, 520);
  assert(drain(&input, TP_TOUCH_UP) == 1);
  tp_touch_event(&input, &view, TP_TOUCH_UP, 5, 800, 400, 530);
  assert(!tp_touch_next(&input, &event));

  /* Toolbar taps have no paint delay or canvas coordinate conversion. */
  tp_touch_event(&input, &view, TP_TOUCH_DOWN, 6, 30, 50, 600);
  assert(tp_touch_next(&input, &event) && event.type == TP_TOUCH_DOWN && !event.canvas);
  tp_touch_event(&input, &view, TP_TOUCH_UP, 6, 30, 50, 610);
  drain(&input, TP_TOUCH_UP);

  /* Zoom and pan limits keep the drawing covering the whole viewport. */
  tp_touch_event(&input, &view, TP_TOUCH_DOWN, 7, 300, 300, 700);
  tp_touch_event(&input, &view, TP_TOUCH_DOWN, 8, 310, 300, 710);
  tp_touch_event(&input, &view, TP_TOUCH_MOVE, 8, 890, 590, 720);
  near(view.zoom, 4);
  assert(view.x >= 0 && view.x <= 600 && view.y >= 0 && view.y <= 450);
  tp_view_fit(&view);
  near(view.zoom, 1); near(view.x, 0); near(view.y, 0);

  /* Dialogs bypass zoom and touch-delay handling. */
  tp_touch_reset(&input);
  view.active = 0;
  tp_touch_event(&input, &view, TP_TOUCH_DOWN, 9, 400, 300, 800);
  assert(tp_touch_next(&input, &event) && !event.canvas);
  puts("PASS: pinch, pan, bounds, inverse coordinates, tap/stroke routing, and dialogs");
  return 0;
}
