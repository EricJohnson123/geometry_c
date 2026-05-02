#include "ui.h"
#include <string.h>

// ─── Globals ──────────────────────────────────────────────────────────────────
const eadk_color_t accent_colors[6] = {
  0xF800, // red        (keep)
  0x001F, // blue       (keep)
  0x3666, // slate gray-green  (replaces aggressive green)
  0x8C51, // dusty rose / mauve (replaces aggressive orange)
  0x602F, // muted indigo      (replaces aggressive purple)
  0x4CD3, // steel blue-gray   (replaces aggressive cyan)
};

uint8_t   dirty    = DIRTY_ALL;
Tab       current_tab = TAB_INPUT;
AppFocus  app_focus   = FOCUS_CONTENT;
int       tab_hover   = 0;

void mark_dirty(uint8_t flags) { dirty |= flags; }
bool is_dirty(uint8_t flag)    { return (dirty & flag) != 0; }

// ─── Unclipped primitives ─────────────────────────────────────────────────────
void fill_rect(int x, int y, int w, int h, eadk_color_t c) {
  if (w <= 0 || h <= 0) return;
  eadk_rect_t r = {(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h};
  eadk_display_push_rect_uniform(r, c);
}

void draw_hline(int x, int y, int w, eadk_color_t c) { fill_rect(x, y, w, 1, c); }
void draw_vline(int x, int y, int h, eadk_color_t c) { fill_rect(x, y, 1, h, c); }

void draw_rect_border(int x, int y, int w, int h, eadk_color_t c) {
  draw_hline(x, y,     w, c);
  draw_hline(x, y+h-1, w, c);
  draw_vline(x,   y, h, c);
  draw_vline(x+w-1, y, h, c);
}

void draw_str(const char* s, int x, int y, bool large, eadk_color_t fg, eadk_color_t bg) {
  eadk_point_t p = {(uint16_t)x, (uint16_t)y};
  eadk_display_draw_string(s, p, large, fg, bg);
}

void draw_str_centered(const char* s, int cx, int y, bool large,
                       eadk_color_t fg, eadk_color_t bg) {
  int fw = large ? LARGE_FONT_W : SMALL_FONT_W;
  int len = (int)strlen(s);
  draw_str(s, cx - (len * fw) / 2, y, large, fg, bg);
}

// ─── Clipped primitives ───────────────────────────────────────────────────────
// Content zone: x in [0, SCREEN_W), y in [CONTENT_Y, CONTENT_BOTTOM)

static void clip_hrect(int* x, int* y, int* w, int* h) {
  // Clip to content zone
  if (*y < CONTENT_Y)   { *h -= (CONTENT_Y - *y); *y = CONTENT_Y; }
  if (*y + *h > CONTENT_BOTTOM) { *h = CONTENT_BOTTOM - *y; }
  if (*x < 0)           { *w += *x; *x = 0; }
  if (*x + *w > SCREEN_W) { *w = SCREEN_W - *x; }
}

void fill_rect_clipped(int x, int y, int w, int h, eadk_color_t c) {
  clip_hrect(&x, &y, &w, &h);
  if (w > 0 && h > 0) fill_rect(x, y, w, h, c);
}

void draw_hline_clipped(int x, int y, int w, eadk_color_t c) {
  fill_rect_clipped(x, y, w, 1, c);
}

void draw_vline_clipped(int x, int y, int h, eadk_color_t c) {
  int w = 1;
  clip_hrect(&x, &y, &w, &h);
  if (w > 0 && h > 0) fill_rect(x, y, w, h, c);
}

void draw_str_clipped(const char* s, int x, int y, bool large,
                      eadk_color_t fg, eadk_color_t bg) {
  if (y < CONTENT_Y || y + (large ? LARGE_FONT_H : SMALL_FONT_H) > CONTENT_BOTTOM) return;
  if (x < 0 || x >= SCREEN_W) return;
  draw_str(s, x, y, large, fg, bg);
}

void draw_circle_clipped(int cx, int cy, int r, eadk_color_t c) {
  // Bresenham outline, each pixel clipped
  int x = r, y = 0, err = 0;
  while (x >= y) {
    // 8 symmetry points
    int pts[8][2] = {
      {cx+x, cy+y}, {cx-x, cy+y}, {cx+x, cy-y}, {cx-x, cy-y},
      {cx+y, cy+x}, {cx-y, cy+x}, {cx+y, cy-x}, {cx-y, cy-x}
    };
    for (int i = 0; i < 8; i++) {
      int px = pts[i][0], py = pts[i][1];
      if (px >= 0 && px < SCREEN_W && py >= CONTENT_Y && py < CONTENT_BOTTOM)
        fill_rect(px, py, 1, 1, c);
    }
    y++;
    if (err <= 0) { err += 2*y + 1; }
    else          { x--; err += 2*(y - x) + 1; }
  }
}

void fill_circle_clipped(int cx, int cy, int r, eadk_color_t c) {
  for (int dy = -r; dy <= r; dy++) {
    int dx = 0;
    while (dx*dx + dy*dy <= r*r) dx++;
    fill_rect_clipped(cx - dx + 1, cy + dy, 2*dx - 2, 1, c);
  }
}

static int iabs(int v) { return v < 0 ? -v : v; }

void draw_line_clipped(int x0, int y0, int x1, int y1, eadk_color_t c) {
  int dx = iabs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -iabs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (1) {
    if (x0 >= 0 && x0 < SCREEN_W && y0 >= CONTENT_Y && y0 < CONTENT_BOTTOM)
      fill_rect(x0, y0, 1, 1, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// ─── Header ───────────────────────────────────────────────────────────────────
void draw_header(void) {
  fill_rect(0, 0, SCREEN_W, HEADER_H, COLOR_ORANGE);
  draw_str_centered("GEOMETRY", SCREEN_W / 2, 2, false, COLOR_WHITE, COLOR_ORANGE);
}

// ─── Tab bar ─────────────────────────────────────────────────────────────────
static const char* tab_names[3] = {"Input", "Graph", "Stats"};

void draw_tabs(void) {
  int i, tw, this_w, tx;
  eadk_color_t bg, fg;
  tw = SCREEN_W / 3;
  for (i = 0; i < 3; i++) {
    this_w = (i == 2) ? SCREEN_W - tw * 2 : tw;
    tx = i * tw;
    if (app_focus == FOCUS_TABBAR && i == tab_hover) {
      // Hovered: white background, dark text — cursor is on this tab
      bg = COLOR_WHITE;
      fg = COLOR_TAB_TEXT_ACTIVE;
    } else if (i == (int)current_tab) {
      // Active tab (content shown): light blue-gray
      bg = COLOR_TAB_ACTIVE;
      fg = COLOR_TAB_TEXT_ACTIVE;
    } else {
      // Inactive tab: dark gray
      bg = COLOR_TAB_INACTIVE;
      fg = COLOR_TAB_TEXT_INACTIVE;
    }
    fill_rect(tx, HEADER_H, this_w, TAB_H, bg);
    draw_str_centered(tab_names[i], tx + this_w / 2, HEADER_H + 4, false, fg, bg);
    if (i > 0) draw_vline(tx, HEADER_H, TAB_H, COLOR_BLACK);
  }
}

// ─── Toolbars ─────────────────────────────────────────────────────────────────
static void draw_toolbar_button(int x, int y, int w, int h,
                                const char* label, bool pressed) {
  eadk_color_t bg = pressed ? COLOR_LIGHT_GRAY : COLOR_TOOLBAR_BG;
  fill_rect(x, y, w, h, bg);
  draw_rect_border(x, y, w, h, COLOR_DARK_GRAY);
  draw_str_centered(label, x + w / 2, y + (h - SMALL_FONT_H) / 2, false,
                    COLOR_BLACK, bg);
}

void draw_toolbar_input(void) {
  int ty = SCREEN_H - TOOLBAR_H;
  fill_rect(0, ty, SCREEN_W, TOOLBAR_H, COLOR_TOOLBAR_BG);
  draw_hline(0, ty, SCREEN_W, COLOR_DARK_GRAY);
  draw_toolbar_button(20,  ty + 4, 120, 20, "Plot graph",    false);
  draw_toolbar_button(180, ty + 4, 120, 20, "Display stats", false);
}

void draw_toolbar_graph(float cx, float cy, bool cursor_valid) {
  int ty = SCREEN_H - TOOLBAR_H;
  fill_rect(0, ty, SCREEN_W, TOOLBAR_H, COLOR_TOOLBAR_BG);
  draw_hline(0, ty, SCREEN_W, COLOR_DARK_GRAY);
  if (cursor_valid) {
    // Show x= and y= like Numworks graph view
    char xbuf[24], ybuf[24];
    // simple ftoa inline (avoid dependency on elements.h here)
    extern void ftoa(float v, char* buf, int dec);
    char xs[12], ys[12];
    ftoa(cx, xs, 3); ftoa(cy, ys, 3);
    // build strings
    xbuf[0] = 'x'; xbuf[1] = '=';
    int i = 0; while (xs[i]) { xbuf[2+i] = xs[i]; i++; } xbuf[2+i] = '\0';
    ybuf[0] = 'y'; ybuf[1] = '=';
    i = 0; while (ys[i]) { ybuf[2+i] = ys[i]; i++; } ybuf[2+i] = '\0';
    draw_str_centered(xbuf,  80,  ty + (TOOLBAR_H - SMALL_FONT_H) / 2,
                      false, COLOR_DARK_GRAY, COLOR_TOOLBAR_BG);
    draw_str_centered(ybuf, 240,  ty + (TOOLBAR_H - SMALL_FONT_H) / 2,
                      false, COLOR_DARK_GRAY, COLOR_TOOLBAR_BG);
  }
}

void draw_toolbar_stats(void) {
  int ty = SCREEN_H - TOOLBAR_H;
  fill_rect(0, ty, SCREEN_W, TOOLBAR_H, COLOR_TOOLBAR_BG);
  draw_hline(0, ty, SCREEN_W, COLOR_DARK_GRAY);
}