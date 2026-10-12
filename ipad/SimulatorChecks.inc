/* Compiled only into simulator builds; enabled explicitly by the CI launcher. */
static void ios_test_require(int passed, const char *description)
{
  fprintf(stderr, "%s: %s\n", passed ? "PASS" : "FAIL", description);
  if (!passed) exit(4);
}

static int ios_test_white(int x, int y)
{
  Uint8 r, g, b;
  SDL_GetRGB(getpixels[canvas->format->BytesPerPixel](canvas, x, y), canvas->format, &r, &g, &b);
  return r == 255 && g == 255 && b == 255;
}

static void ios_test_finger(Uint32 type, Sint64 id, float x, float y)
{
  int wx, wy, width, height;
  SDL_RenderLogicalToWindow(renderer, x, y, &wx, &wy);
  SDL_GetWindowSize(window_screen, &width, &height);
  SDL_Event event;
  SDL_zero(event);
  event.type = type;
  event.tfinger.windowID = SDL_GetWindowID(window_screen);
  event.tfinger.touchId = 42;
  event.tfinger.fingerId = id;
  event.tfinger.x = (float)wx / width;
  event.tfinger.y = (float)wy / height;
  SDL_PushEvent(&event);
}

static void ios_test_tap(float x, float y)
{
  ios_test_finger(SDL_FINGERDOWN, 10, x, y);
  ios_test_finger(SDL_FINGERUP, 10, x, y);
}

static void ios_test_tool(int tool)
{
  int slot = toolbar_slot_for_tool(tool);
  ios_test_tap((slot % 2 + 0.5f) * tool_button_w,
               r_tools.y + (slot / 2 + 0.5f) * tool_button_h);
}

static void ios_test_action(int code)
{
  SDL_Event event;
  SDL_zero(event);
  event.type = SDL_USEREVENT;
  event.user.code = code;
  SDL_PushEvent(&event);
}

static void ios_simulator_checks(void)
{
  static int phase, painted_x, painted_y;
  const char *mode = SDL_getenv("TUXPAINT_TEST_FEATURES");
  if (!mode || !*mode || phase == 100) return;
  if (phase == 0 && (strcmp(mode, "share-only") == 0 || strcmp(mode, "photos-only") == 0))
  {
    ios_export_current(strcmp(mode, "photos-only") == 0 ? IOS_ACTION_PHOTOS : IOS_ACTION_SHARE);
    phase = 100;
    return;
  }
  float cx = r_canvas.x + r_canvas.w * 0.5f;
  float cy = r_canvas.y + r_canvas.h * 0.5f;
  if (phase == 0)
  {
    if (SDL_GetTicks() < 1000) return;
    ios_test_finger(SDL_FINGERDOWN, 1, cx - 50, cy);
    ios_test_finger(SDL_FINGERDOWN, 2, cx + 50, cy);
    ios_test_finger(SDL_FINGERMOTION, 1, cx - 100, cy);
    ios_test_finger(SDL_FINGERMOTION, 2, cx + 100, cy);
    ios_test_finger(SDL_FINGERUP, 1, cx - 100, cy);
    ios_test_finger(SDL_FINGERUP, 2, cx + 100, cy);
  }
  else if (phase == 1)
  {
    fprintf(stderr, "Integrated view: zoom=%f, origin=%f,%f\n", ios_view.zoom, ios_view.x, ios_view.y);
    ios_test_require(fabsf(ios_view.zoom - 2) < 0.04f, "SDL touch events pinch the canvas to 2x");
    ios_test_require(ios_test_white(canvas->w / 2, canvas->h / 2), "Pinch leaves no paint mark");
    float x = r_canvas.x + r_canvas.w * 0.3f, y = r_canvas.y + r_canvas.h * 0.3f;
    float px = x, py = y;
    tp_view_to_canvas(&ios_view, &px, &py);
    painted_x = px - r_canvas.x;
    painted_y = py - r_canvas.y;
    ios_test_tap(x, y);
  }
  else if (phase == 2)
  {
    ios_test_require(!ios_test_white(painted_x, painted_y), "Touch paints at the inverse-mapped canvas position");
    ios_test_require(ios_test_white(canvas->w * 0.3f, canvas->h * 0.3f), "Zoomed touch does not paint at the untransformed position");
    float sx = r_canvas.x + painted_x, sy = r_canvas.y + painted_y;
    tp_view_to_screen(&ios_view, &sx, &sy);
    char *sample_path = get_fname("zoom-sample.txt", DIR_SAVE);
    FILE *sample = fopen(sample_path, "w");
    ios_test_require(sample != NULL, "Write visible-pixel checkpoint");
    fprintf(sample, "%f %f %d %d\n", sx, sy, WINDOW_WIDTH, WINDOW_HEIGHT);
    fclose(sample);
    free(sample_path);
    char *ready = get_fname("zoom-ready.txt", DIR_SAVE);
    FILE *marker = fopen(ready, "w");
    ios_test_require(marker != NULL, "Notify simulator screenshot checkpoint");
    fclose(marker);
    free(ready);
  }
  else if (phase == 3)
  {
    char *captured = get_fname("zoom-captured.txt", DIR_SAVE);
    int exists = access(captured, F_OK) == 0;
    free(captured);
    if (!exists) return;
    if (ios_recovery_pending) return;
    char *recovery = get_fname("saved/" AUTOSAVED_NAME FNAME_EXTENSION, DIR_SAVE);
    SDL_Surface *checkpoint = IMG_Load(recovery);
    free(recovery);
    ios_test_require(checkpoint != NULL, "Idle pause creates a recovery drawing");
    Uint8 r, g, b;
    SDL_GetRGB(getpixels[checkpoint->format->BytesPerPixel](checkpoint, painted_x, painted_y),
                checkpoint->format, &r, &g, &b);
    ios_test_require(r != 255 || g != 255 || b != 255, "Idle recovery includes the latest stroke");
    SDL_FreeSurface(checkpoint);
    ios_test_tool(TOOL_UNDO);
  }
  else if (phase == 4)
  {
    ios_test_require(ios_test_white(painted_x, painted_y), "Undo removes the zoomed stroke");
    ios_test_tool(TOOL_REDO);
  }
  else if (phase == 5)
  {
    ios_test_require(!ios_test_white(painted_x, painted_y), "Redo restores the zoomed stroke");
    ios_test_require(ios_write_share_image(), "Export writes a PNG while zoomed");
    SDL_Surface *exported = IMG_Load(apple_share_path());
    ios_test_require(exported && exported->w == canvas->w && exported->h == canvas->h,
                      "Shared PNG contains the full-size drawing");
    SDL_FreeSurface(exported);
    int previous_prompt = promptless_save;
    promptless_save = SAVE_OVER_ALWAYS;
    ios_test_require(do_save(cur_tool, 1, 0), "Explicit Save succeeds");
    promptless_save = previous_prompt;
    SDL_FillRect(canvas, NULL, SDL_MapRGB(canvas->format, 255, 255, 255));
    load_current();
    ios_test_require(!ios_test_white(painted_x, painted_y), "Explicitly saved drawing reloads");
    update_canvas(0, 0, canvas->w, canvas->h);
    SDL_Event escape;
    SDL_zero(escape);
    escape.type = SDL_KEYDOWN;
    escape.key.keysym.sym = SDLK_ESCAPE;
    SDL_PushEvent(&escape);
    ios_test_require(do_open() == 0, "Open gallery can be cancelled while zoomed");
    ios_test_require(ios_view.active && ios_view.zoom > 1.9f, "Leaving a dialog restores the canvas view");
    if (!ios_kids_mode) ios_test_action(IOS_ACTION_KIDS);
  }
  else if (phase == 6)
  {
    ios_test_require(TOOLBAR_TOOL_COUNT == 10 && gd_tools.rows == 5 && apple_kids_mode(),
                      "Kids Mode has ten tools in five rows and persists its setting");
    ios_test_tool(TOOL_ERASER);
  }
  else if (phase == 7)
  {
    ios_test_require(cur_tool == TOOL_ERASER, "Kids Mode touch selects the mapped tool");
    ios_test_tool(TOOL_BRUSH);
    ios_test_action(IOS_ACTION_FIT);
  }
  else if (phase == 8)
  {
    ios_test_require(cur_tool == TOOL_BRUSH && ios_view.zoom == 1, "Brush and Fit Drawing work in Kids Mode");
    ios_checkpoint();
    if (strcmp(mode, "share") == 0) ios_export_current(IOS_ACTION_SHARE);
    char *result_path = get_fname("features-verified.txt", DIR_SAVE);
    FILE *result = fopen(result_path, "w");
    ios_test_require(result != NULL, "Write simulator result");
    fprintf(result, "PASS: pinch, mapped painting, undo/redo, full PNG export, Save/reload, gallery, Kids Mode, Fit\n");
    fclose(result);
    free(result_path);
    phase = 100;
    return;
  }
  phase++;
}
