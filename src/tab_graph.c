#include "tab_graph.h"
#include "ui.h"
#include "elements.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// ─── State ────────────────────────────────────────────────────────────────────
GraphNavState graph_nav      = GRAPH_IDLE;
float graph_x_min            = -10.0f;
float graph_x_max            =  10.0f;
float graph_y_min            =  -6.25f;
float graph_y_max            =   6.25f;
float graph_cursor_x         = 0.0f;
float graph_cursor_y         = 0.0f;
bool  graph_cursor_valid     = false;

typedef enum { SUB_AUTO=0, SUB_AXES, SUB_NAVIGATE, SUB_CALCULATE, SUB_COUNT }
  SubToolItem;
static SubToolItem sub_sel = SUB_AUTO;
static bool show_axes      = true;

// ─── Public API ───────────────────────────────────────────────────────────────
void graph_enter(void) {
  graph_nav = GRAPH_IDLE;
}

bool graph_can_enter_tabbar(void) {
  // Can enter tab bar only from IDLE or SUBTOOL (not while panning)
  return graph_nav == GRAPH_IDLE || graph_nav == GRAPH_SUBTOOL;
}

// ─── Coordinate conversion ────────────────────────────────────────────────────
int world_to_screen_x(float wx) {
  return (int)((wx - graph_x_min) / (graph_x_max - graph_x_min) * SCREEN_W);
}

int world_to_screen_y(float wy) {
  return GRAPH_PLOT_Y + GRAPH_PLOT_H - 1
       - (int)((wy - graph_y_min) / (graph_y_max - graph_y_min) * GRAPH_PLOT_H);
}

// ─── Sub-toolbar ─────────────────────────────────────────────────────────────
static const char* sub_labels[SUB_COUNT] = {
  "Auto", "Axes", "Navigate", "Calculate"
};

static void draw_subtoolbar(void) {
  int i, x, w;
  eadk_color_t bg, fg;
  fill_rect(0, CONTENT_Y, SCREEN_W, GRAPH_SUBTOOL_H, COLOR_SUBTOOL_BG);
  draw_hline(0, CONTENT_Y + GRAPH_SUBTOOL_H - 1, SCREEN_W, COLOR_DARK_GRAY);

  x = 4;
  for (i = 0; i < SUB_COUNT; i++) {
    bool sel = (graph_nav == GRAPH_SUBTOOL && i == (int)sub_sel);
    // Navigate also shows as active (pressed) when in PAN mode
    bool active = sel || (i == SUB_NAVIGATE && graph_nav == GRAPH_PAN);
    bg = active ? COLOR_SUBTOOL_SELECTED : COLOR_SUBTOOL_BG;
    fg = active ? COLOR_BLACK : COLOR_DARK_GRAY;
    w = (int)strlen(sub_labels[i]) * SMALL_FONT_W + 8;
    fill_rect(x, CONTENT_Y + 1, w, GRAPH_SUBTOOL_H - 2, bg);
    draw_str(sub_labels[i], x + 4, CONTENT_Y + 2, false, fg, bg);
    if (i == SUB_AUTO) {
      // Orange dot after Auto
      fill_circle_clipped(x + w + 5, CONTENT_Y + GRAPH_SUBTOOL_H / 2,
                          4, COLOR_ORANGE);
      x += w + 14;
    } else {
      x += w + 6;
    }
  }
}

// ─── Graph-plot-area clipped primitives ──────────────────────────────────────
// These clip to [GRAPH_PLOT_Y, CONTENT_BOTTOM) — strictly below the subtoolbar.

static void gfill(int x, int y, int w, int h, eadk_color_t c) {
  if (w <= 0 || h <= 0) return;
  if (y < GRAPH_PLOT_Y)   { h -= (GRAPH_PLOT_Y - y); y = GRAPH_PLOT_Y; }
  if (y + h > CONTENT_BOTTOM) h = CONTENT_BOTTOM - y;
  if (x < 0)              { w += x; x = 0; }
  if (x + w > SCREEN_W)   w = SCREEN_W - x;
  if (w > 0 && h > 0) fill_rect(x, y, w, h, c);
}

static void ghline(int x, int y, int w, eadk_color_t c) { gfill(x, y, w, 1, c); }
static void gvline(int x, int y, int h, eadk_color_t c) { gfill(x, y, 1, h, c); }

static void gstr(const char* s, int x, int y, eadk_color_t fg, eadk_color_t bg) {
  if (y < GRAPH_PLOT_Y || y + SMALL_FONT_H > CONTENT_BOTTOM) return;
  if (x < 0 || x >= SCREEN_W) return;
  draw_str(s, x, y, false, fg, bg);
}

static void gdot(int cx, int cy, int r, eadk_color_t c) {
  /* filled circle, plot-clipped */
  int dy, dx;
  for (dy = -r; dy <= r; dy++) {
    dx = 0;
    while (dx*dx + dy*dy <= r*r) dx++;
    gfill(cx - dx + 1, cy + dy, 2*dx - 2, 1, c);
  }
}

static void gring(int cx, int cy, int r, eadk_color_t c) {
  /* Bresenham circle outline, plot-clipped */
  int x = r, y = 0, err = 0, px, py, j;
  int pts[8][2];
  while (x >= y) {
    pts[0][0]=cx+x; pts[0][1]=cy+y;
    pts[1][0]=cx-x; pts[1][1]=cy+y;
    pts[2][0]=cx+x; pts[2][1]=cy-y;
    pts[3][0]=cx-x; pts[3][1]=cy-y;
    pts[4][0]=cx+y; pts[4][1]=cy+x;
    pts[5][0]=cx-y; pts[5][1]=cy+x;
    pts[6][0]=cx+y; pts[6][1]=cy-x;
    pts[7][0]=cx-y; pts[7][1]=cy-x;
    for (j = 0; j < 8; j++) {
      px = pts[j][0]; py = pts[j][1];
      if (px>=0 && px<SCREEN_W && py>=GRAPH_PLOT_Y && py<CONTENT_BOTTOM)
        fill_rect(px, py, 1, 1, c);
    }
    y++;
    if (err <= 0) err += 2*y+1;
    else          { x--; err += 2*(y-x)+1; }
  }
}

// ─── Grid and axes ────────────────────────────────────────────────────────────
static void draw_grid_and_axes(void) {
  float x_range, y_range, x_step, y_step, wx, wy;
  int sx, sy, sx0, sy0, lw, lx, ly, iv;
  char lbl[12];

  gfill(0, GRAPH_PLOT_Y, SCREEN_W, GRAPH_PLOT_H, COLOR_WHITE);

  x_range = graph_x_max - graph_x_min;
  y_range = graph_y_max - graph_y_min;
  x_step = 1.0f; while (x_range / x_step > 20.0f) x_step *= 2.0f;
  y_step = 1.0f; while (y_range / y_step > 15.0f) y_step *= 2.0f;

  wx = x_step * (float)((int)(graph_x_min / x_step) - 1);
  while (wx <= graph_x_max + x_step) {
    sx = world_to_screen_x(wx);
    gvline(sx, GRAPH_PLOT_Y, GRAPH_PLOT_H, COLOR_GRID);
    wx += x_step;
  }
  wy = y_step * (float)((int)(graph_y_min / y_step) - 1);
  while (wy <= graph_y_max + y_step) {
    sy = world_to_screen_y(wy);
    ghline(0, sy, SCREEN_W, COLOR_GRID);
    wy += y_step;
  }

  // Axis lines
  sy0 = world_to_screen_y(0.0f);
  sx0 = world_to_screen_x(0.0f);
  if (show_axes) {
    ghline(0, sy0, SCREEN_W, COLOR_AXIS);
    gvline(sx0, GRAPH_PLOT_Y, GRAPH_PLOT_H, COLOR_AXIS);
  }

  // X tick labels — clamp row to plot area
  if (sy0 < GRAPH_PLOT_Y)         ly = GRAPH_PLOT_Y + 2;
  else if (sy0 >= CONTENT_BOTTOM) ly = CONTENT_BOTTOM - SMALL_FONT_H - 2;
  else                            ly = sy0 + 2;

  wx = x_step * (float)((int)(graph_x_min / x_step) - 1);
  while (wx <= graph_x_max + x_step) {
    if (wx > graph_x_min - x_step && wx < graph_x_max + x_step) {
      if (wx == 0.0f && sx0 >= 0 && sx0 < SCREEN_W) { wx += x_step; continue; }
      sx = world_to_screen_x(wx);
      iv = (int)wx;
      if ((float)iv == wx) snprintf(lbl, sizeof(lbl), "%d", iv);
      else ftoa(wx, lbl, 1);
      lw = (int)strlen(lbl) * SMALL_FONT_W;
      lx = sx - lw / 2;
      if (lx < 0) lx = 0;
      if (lx + lw > SCREEN_W) lx = SCREEN_W - lw;
      gstr(lbl, lx, ly, COLOR_AXIS_LABEL, COLOR_WHITE);
      if (sy0 >= GRAPH_PLOT_Y && sy0 < CONTENT_BOTTOM)
        gvline(sx, sy0 - 2, 5, COLOR_AXIS);
    }
    wx += x_step;
  }

  // Y tick labels — clamp column to plot area
  if (sx0 < 0)                    lx = 2;
  else if (sx0 >= SCREEN_W - 22)  lx = SCREEN_W - 22;
  else                            lx = sx0 + 3;

  wy = y_step * (float)((int)(graph_y_min / y_step) - 1);
  while (wy <= graph_y_max + y_step) {
    if (wy > graph_y_min - y_step && wy < graph_y_max + y_step) {
      if (wy == 0.0f && sy0 >= GRAPH_PLOT_Y && sy0 < CONTENT_BOTTOM) {
        wy += y_step; continue;
      }
      sy = world_to_screen_y(wy);
      iv = (int)wy;
      if ((float)wy == (float)iv) snprintf(lbl, sizeof(lbl), "%d", iv);
      else ftoa(wy, lbl, 1);
      gstr(lbl, lx, sy - SMALL_FONT_H / 2, COLOR_AXIS_LABEL, COLOR_WHITE);
      if (sx0 >= 0 && sx0 < SCREEN_W)
        ghline(sx0 - 2, sy, 5, COLOR_AXIS);
    }
    wy += y_step;
  }
}

static void draw_elements(void) {
  int i, sx, sy;
  eadk_color_t ac;
  for (i = 0; i < elem_count; i++) {
    Element* e = &elements[i];
    if (!e->active) continue;
    sx = world_to_screen_x(e->x);
    sy = world_to_screen_y(e->y);
    ac = accent_colors[i % NUM_ACCENT_COLORS];
    gdot(sx, sy, 3, ac);
    gring(sx, sy, 5, ac);
    /* Label: only draw if within plot area */
    if (sy - SMALL_FONT_H >= GRAPH_PLOT_Y && sy - SMALL_FONT_H < CONTENT_BOTTOM
        && sx + 7 >= 0 && sx + 7 < SCREEN_W)
      draw_str(e->label, sx + 7, sy - SMALL_FONT_H, false, ac, COLOR_WHITE);
  }
}

// ─── Auto-fit ─────────────────────────────────────────────────────────────────
static void auto_fit(void) {
  int count, i;
  float xmin, xmax, ymin, ymax, xpad, ypad;
  count = 0;
  xmin = xmax = ymin = ymax = 0.0f;
  for (i = 0; i < elem_count; i++) {
    if (!elements[i].active) continue;
    if (count == 0) {
      xmin = xmax = elements[i].x;
      ymin = ymax = elements[i].y;
    } else {
      if (elements[i].x < xmin) xmin = elements[i].x;
      if (elements[i].x > xmax) xmax = elements[i].x;
      if (elements[i].y < ymin) ymin = elements[i].y;
      if (elements[i].y > ymax) ymax = elements[i].y;
    }
    count++;
  }
  if (count == 0) {
    graph_x_min=-10.0f; graph_x_max=10.0f;
    graph_y_min=-6.25f; graph_y_max=6.25f;
    return;
  }
  xpad = (xmax-xmin)*0.2f + 2.0f;
  ypad = (ymax-ymin)*0.2f + 2.0f;
  graph_x_min=xmin-xpad; graph_x_max=xmax+xpad;
  graph_y_min=ymin-ypad; graph_y_max=ymax+ypad;
}

// ─── Draw ─────────────────────────────────────────────────────────────────────
void draw_graph_tab(void) {
  draw_subtoolbar();
  draw_grid_and_axes();
  draw_elements();
}

// ─── Event handler ────────────────────────────────────────────────────────────
void handle_graph_event(eadk_event_t ev) {
  float dx, dy, cx, cy, hw, hh;

  switch (graph_nav) {

  case GRAPH_IDLE:
    // In IDLE: only up-arrow does anything (handled by main.c for tab bar),
    // and down-arrow enters the subtoolbar.
    if (ev == eadk_event_down || ev == eadk_event_ok || ev == eadk_event_exe) {
      graph_nav = GRAPH_SUBTOOL;
      sub_sel   = SUB_AUTO;
      mark_dirty(DIRTY_CONTENT);
    }
    // back from idle exits to input
    if (ev == eadk_event_back) {
      extern Tab current_tab;
      current_tab = TAB_INPUT;
      mark_dirty(DIRTY_TABS | DIRTY_CONTENT | DIRTY_TOOLBAR);
    }
    break;

  case GRAPH_SUBTOOL:
    switch (ev) {
      case eadk_event_left:
        if (sub_sel > 0) { sub_sel--; mark_dirty(DIRTY_CONTENT); }
        break;
      case eadk_event_right:
        if (sub_sel < SUB_COUNT-1) { sub_sel++; mark_dirty(DIRTY_CONTENT); }
        break;
      case eadk_event_down:
        // Leave subtoolbar back to idle (arrows do nothing in idle)
        graph_nav = GRAPH_IDLE;
        mark_dirty(DIRTY_CONTENT);
        break;
      case eadk_event_back:
        graph_nav = GRAPH_IDLE;
        mark_dirty(DIRTY_CONTENT);
        break;
      case eadk_event_ok:
      case eadk_event_exe:
        if (sub_sel == SUB_AUTO) {
          auto_fit();
          mark_dirty(DIRTY_CONTENT | DIRTY_TOOLBAR);
        } else if (sub_sel == SUB_AXES) {
          show_axes = !show_axes;
          mark_dirty(DIRTY_CONTENT);
        } else if (sub_sel == SUB_NAVIGATE) {
          graph_nav = GRAPH_PAN;
          mark_dirty(DIRTY_CONTENT);
        }
        break;
      default: break;
    }
    break;

  case GRAPH_PAN:
    dx = (graph_x_max - graph_x_min) * 0.15f;
    dy = (graph_y_max - graph_y_min) * 0.15f;
    switch (ev) {
      case eadk_event_left:
        graph_x_min -= dx; graph_x_max -= dx;
        mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_right:
        graph_x_min += dx; graph_x_max += dx;
        mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_up:
        graph_y_min += dy; graph_y_max += dy;
        mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_down:
        graph_y_min -= dy; graph_y_max -= dy;
        mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_plus:
        cx=(graph_x_min+graph_x_max)/2.0f; cy=(graph_y_min+graph_y_max)/2.0f;
        hw=(graph_x_max-graph_x_min)*0.4f; hh=(graph_y_max-graph_y_min)*0.4f;
        graph_x_min=cx-hw; graph_x_max=cx+hw;
        graph_y_min=cy-hh; graph_y_max=cy+hh;
        mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_minus:
        cx=(graph_x_min+graph_x_max)/2.0f; cy=(graph_y_min+graph_y_max)/2.0f;
        hw=(graph_x_max-graph_x_min)*0.6f; hh=(graph_y_max-graph_y_min)*0.6f;
        graph_x_min=cx-hw; graph_x_max=cx+hw;
        graph_y_min=cy-hh; graph_y_max=cy+hh;
        mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_back:
        // Exit pan mode → back to subtoolbar on Navigate
        graph_nav = GRAPH_SUBTOOL;
        sub_sel   = SUB_NAVIGATE;
        mark_dirty(DIRTY_CONTENT); break;
      default: break;
    }
    break;
  }
}