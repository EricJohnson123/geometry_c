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
void graph_enter(void) { graph_nav = GRAPH_IDLE; }

bool graph_can_enter_tabbar(void) {
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

// ─── Math helpers ─────────────────────────────────────────────────────────────
static float fsqrt(float s) {
  int i;
  float r;
  if (s <= 0.0f) return 0.0f;
  r = s / 2.0f;
  for (i = 0; i < 24; i++) r = (r + s / r) / 2.0f;
  return r;
}

// Integer square root (for arrowhead, no libm)
static int isqrt(int v) {
  int r = 0, bit = 1 << 14;
  if (v < 0) v = -v;
  while (bit > v) bit >>= 2;
  while (bit > 0) {
    if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
    else r >>= 1;
    bit >>= 2;
  }
  return r;
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
    bool sel    = (graph_nav == GRAPH_SUBTOOL && i == (int)sub_sel);
    bool active = sel || (i == SUB_NAVIGATE && graph_nav == GRAPH_PAN);
    bg = active ? COLOR_SUBTOOL_SELECTED : COLOR_SUBTOOL_BG;
    fg = active ? COLOR_BLACK : COLOR_DARK_GRAY;
    w = (int)strlen(sub_labels[i]) * SMALL_FONT_W + 8;
    fill_rect(x, CONTENT_Y + 1, w, GRAPH_SUBTOOL_H - 2, bg);
    draw_str(sub_labels[i], x + 4, CONTENT_Y + 2, false, fg, bg);
    if (i == SUB_AUTO) {
      fill_circle_clipped(x + w + 5, CONTENT_Y + GRAPH_SUBTOOL_H / 2, 4, COLOR_ORANGE);
      x += w + 14;
    } else {
      x += w + 6;
    }
  }
}

// ─── Graph-plot-area clipped primitives ──────────────────────────────────────
static void gfill(int x, int y, int w, int h, eadk_color_t c) {
  if (w <= 0 || h <= 0) return;
  if (y < GRAPH_PLOT_Y)        { h -= (GRAPH_PLOT_Y - y); y = GRAPH_PLOT_Y; }
  if (y + h > CONTENT_BOTTOM)    h  = CONTENT_BOTTOM - y;
  if (x < 0)                   { w += x; x = 0; }
  if (x + w > SCREEN_W)          w  = SCREEN_W - x;
  if (w > 0 && h > 0) fill_rect(x, y, w, h, c);
}

static void ghline(int x, int y, int w, eadk_color_t c) { gfill(x, y, w, 1, c); }
static void gvline(int x, int y, int h, eadk_color_t c) { gfill(x, y, 1, h, c); }

// gstr clips to plot area so labels never bleed into the subtoolbar
static void gstr(const char* s, int x, int y, eadk_color_t fg, eadk_color_t bg) {
  if (y < GRAPH_PLOT_Y || y + SMALL_FONT_H > CONTENT_BOTTOM) return;
  if (x < 0 || x >= SCREEN_W) return;
  draw_str(s, x, y, false, fg, bg);
}

static void gdot(int cx, int cy, int r, eadk_color_t c) {
  int dy, dx;
  for (dy = -r; dy <= r; dy++) {
    dx = 0;
    while (dx*dx + dy*dy <= r*r) dx++;
    gfill(cx - dx + 1, cy + dy, 2*dx - 2, 1, c);
  }
}

static void gring(int cx, int cy, int r, eadk_color_t c) {
  int x = r, y = 0, err = 0, px, py, j;
  int pts[8][2];
  while (x >= y) {
    pts[0][0]=cx+x; pts[0][1]=cy+y; pts[1][0]=cx-x; pts[1][1]=cy+y;
    pts[2][0]=cx+x; pts[2][1]=cy-y; pts[3][0]=cx-x; pts[3][1]=cy-y;
    pts[4][0]=cx+y; pts[4][1]=cy+x; pts[5][0]=cx-y; pts[5][1]=cy+x;
    pts[6][0]=cx+y; pts[6][1]=cy-x; pts[7][0]=cx-y; pts[7][1]=cy-x;
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

// Single-pixel Bresenham line, plot-clipped
static void gdraw_line(int x0, int y0, int x1, int y1, eadk_color_t c) {
  int dx, dy, sx, sy, err, e2, x, y;
  dx = x1 > x0 ? x1 - x0 : x0 - x1;
  dy = y1 > y0 ? y1 - y0 : y0 - y1;
  sx = x0 < x1 ? 1 : -1;
  sy = y0 < y1 ? 1 : -1;
  err = (dx > dy ? dx : -dy) / 2;
  x = x0; y = y0;
  while (1) {
    if (x >= 0 && x < SCREEN_W && y >= GRAPH_PLOT_Y && y < CONTENT_BOTTOM)
      gfill(x, y, 1, 1, c);
    if (x == x1 && y == y1) break;
    e2 = err;
    if (e2 > -dx) { err -= dy; x += sx; }
    if (e2 <  dy) { err += dx; y += sy; }
  }
}

// 2-pixel-thick line (for lines/elements)
static void gdraw_line_thick(int x0, int y0, int x1, int y1, eadk_color_t c) {
  int adx, ady, ox, oy;
  adx = x1 > x0 ? x1 - x0 : x0 - x1;
  ady = y1 > y0 ? y1 - y0 : y0 - y1;
  gdraw_line(x0, y0, x1, y1, c);
  if (adx >= ady) { ox = 0; oy = 1; }
  else            { ox = 1; oy = 0; }
  gdraw_line(x0 + ox, y0 + oy, x1 + ox, y1 + oy, c);
}

// ─── Vector arrow helpers (from src) ─────────────────────────────────────────
#define ARROW_LEN  9
#define ARROW_HALF 4

// Filled triangle, plot-clipped
static void gtriangle(int x0, int y0, int x1, int y1, int x2, int y2,
                      eadk_color_t c) {
  int tx, ty, dy02, dy01, dy12, y, xa, xb;
  if (y0 > y1) { tx=x0;x0=x1;x1=tx; ty=y0;y0=y1;y1=ty; }
  if (y0 > y2) { tx=x0;x0=x2;x2=tx; ty=y0;y0=y2;y2=ty; }
  if (y1 > y2) { tx=x1;x1=x2;x2=tx; ty=y1;y1=y2;y2=ty; }
  dy02 = y2-y0; dy01 = y1-y0; dy12 = y2-y1;
  for (y = y0; y <= y2; y++) {
    if (dy02 == 0) xa = x0; else xa = x0 + (x2-x0)*(y-y0)/dy02;
    if (y <= y1) { if (dy01 == 0) xb = x0; else xb = x0 + (x1-x0)*(y-y0)/dy01; }
    else         { if (dy12 == 0) xb = x1; else xb = x1 + (x2-x1)*(y-y1)/dy12; }
    if (xa > xb) { tx=xa; xa=xb; xb=tx; }
    gfill(xa, y, xb-xa+1, 1, c);
  }
}

// Draw vector shaft (2x2 thick Bresenham) + filled arrowhead
static void gdraw_vector(int x0, int y0, int x1, int y1, eadk_color_t c) {
  int dx, dy, len, ax, ay, px, py;
  int tip_x, tip_y, b0x, b0y, b1x, b1y;

  dx = x1 - x0; dy = y1 - y0;
  len = isqrt(dx*dx + dy*dy);
  if (len == 0) return;

  ax = dx * ARROW_LEN / len;
  ay = dy * ARROW_LEN / len;
  px = -dy * ARROW_HALF / len;
  py =  dx * ARROW_HALF / len;

  tip_x = x1; tip_y = y1;
  b0x = tip_x - ax + px; b0y = tip_y - ay + py;
  b1x = tip_x - ax - px; b1y = tip_y - ay - py;

  // Shaft: 2x2 thick Bresenham
  {
    int lx0 = x0, ly0 = y0, lx1 = tip_x - ax, ly1 = tip_y - ay;
    int adx = lx1-lx0; if (adx<0) adx=-adx;
    int ady = ly1-ly0; if (ady<0) ady=-ady;
    int sx = (lx1>=lx0) ? 1 : -1;
    int sy = (ly1>=ly0) ? 1 : -1;
    int er = adx - ady;
    int cx = lx0, cy = ly0;
    while (1) {
      gfill(cx, cy, 2, 2, c);
      if (cx == lx1 && cy == ly1) break;
      { int e2 = 2*er;
        if (e2 > -ady) { er -= ady; cx += sx; }
        if (e2 <  adx) { er += adx; cy += sy; } }
    }
  }

  gtriangle(tip_x, tip_y, b0x, b0y, b1x, b1y, c);
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

  sy0 = world_to_screen_y(0.0f);
  sx0 = world_to_screen_x(0.0f);
  if (show_axes) {
    ghline(0, sy0, SCREEN_W, COLOR_AXIS);
    gvline(sx0, GRAPH_PLOT_Y, GRAPH_PLOT_H, COLOR_AXIS);
  }

  if (show_axes) {
    // X tick labels
    if      (sy0 < GRAPH_PLOT_Y)    ly = GRAPH_PLOT_Y + 2;
    else if (sy0 >= CONTENT_BOTTOM)  ly = CONTENT_BOTTOM - SMALL_FONT_H - 2;
    else                             ly = sy0 + 2;

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

    // Y tick labels
    if      (sx0 < 0)             lx = 2;
    else if (sx0 >= SCREEN_W-22)  lx = SCREEN_W - 22;
    else                          lx = sx0 + 3;

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
  } // end show_axes
}

// ─── Draw elements ────────────────────────────────────────────────────────────
static void draw_elements(void) {
  int i, sx, sy;
  eadk_color_t ac;

  for (i = 0; i < elem_count; i++) {
    Element* e = &elements[i];
    if (!e->active) continue;
    ac = accent_colors[i % NUM_ACCENT_COLORS];

    if (e->type == ELEM_POINT) {
      sx = world_to_screen_x(e->x);
      sy = world_to_screen_y(e->y);
      gdot(sx, sy, 3, ac);
      gring(sx, sy, 5, ac);
      gstr(e->label, sx + 7, sy - SMALL_FONT_H, ac, COLOR_WHITE);

    } else if (e->type == ELEM_VECTOR) {
      // Origin from e->ox, e->oy (0,0 if no origin set)
      int ox2 = world_to_screen_x(e->ox);
      int oy2 = world_to_screen_y(e->oy);
      int tx  = world_to_screen_x(e->ox + e->x);
      int ty  = world_to_screen_y(e->oy + e->y);
      char lbl_str[8];
      int lx2, ly2;
      gdraw_vector(ox2, oy2, tx, ty, ac);
      snprintf(lbl_str, sizeof(lbl_str), "->%s", e->label);
      lx2 = tx + 14;
      ly2 = ty - SMALL_FONT_H - 2;
      if (lx2 + (int)(strlen(lbl_str) * SMALL_FONT_W) > SCREEN_W)
        lx2 = tx - (int)(strlen(lbl_str) * SMALL_FONT_W) - 4;
      if (ly2 < GRAPH_PLOT_Y) ly2 = ty + 4;
      if (ly2 + SMALL_FONT_H > CONTENT_BOTTOM) ly2 = CONTENT_BOTTOM - SMALL_FONT_H - 1;
      gstr(lbl_str, lx2, ly2, ac, COLOR_WHITE);

    } else if (e->type == ELEM_LINE) {
      float a = e->x, b = e->y, c = e->ox;
      int lx0, ly0, lx1, ly1, lmx, lmy;
      if (b == 0.0f && a != 0.0f) {
        float vx = -c / a;
        int lvx = world_to_screen_x(vx);
        gdraw_line_thick(lvx, GRAPH_PLOT_Y, lvx, CONTENT_BOTTOM - 1, ac);
        gstr(e->label, lvx + 7, GRAPH_PLOT_Y + 2, ac, COLOR_WHITE);
      } else if (b != 0.0f) {
        float y_left  = (-a * graph_x_min - c) / b;
        float y_right = (-a * graph_x_max - c) / b;
        lx0 = world_to_screen_x(graph_x_min);
        ly0 = world_to_screen_y(y_left);
        lx1 = world_to_screen_x(graph_x_max);
        ly1 = world_to_screen_y(y_right);
        gdraw_line_thick(lx0, ly0, lx1, ly1, ac);
        lmx = (lx0 + lx1) / 2;
        lmy = (ly0 + ly1) / 2;
        gstr(e->label, lmx + 7, lmy - SMALL_FONT_H, ac, COLOR_WHITE);
      }
    }
  }
}

// ─── Auto-fit ─────────────────────────────────────────────────────────────────
static void auto_fit(void) {
  int count, i, j, ncands;
  float xmin, xmax, ymin, ymax, xpad, ypad;
  float cands[6][2];
  Element* e;
  float a, b, c;
  bool lines_only;

  count = 0;
  xmin = xmax = ymin = ymax = 0.0f;
  lines_only = true;

  for (i = 0; i < elem_count; i++) {
    e = &elements[i];
    if (!e->active) continue;
    if (e->type != ELEM_LINE) lines_only = false;

    ncands = 0;

    if (e->type == ELEM_POINT) {
      cands[0][0] = e->x; cands[0][1] = e->y; ncands = 1;
    } else if (e->type == ELEM_VECTOR) {
      cands[0][0] = e->ox;          cands[0][1] = e->oy;
      cands[1][0] = e->ox + e->x;   cands[1][1] = e->oy + e->y;
      ncands = 2;
    } else if (e->type == ELEM_LINE) {
      a = e->x; b = e->y; c = e->ox;
      cands[0][0] = 0.0f; cands[0][1] = 0.0f; ncands = 1;
      if (b == 0.0f && a != 0.0f) {
        float vx = -c / a;
        cands[ncands][0] = vx; cands[ncands][1] = 0.0f; ncands++;
      } else if (b != 0.0f) {
        cands[ncands][0] = 0.0f; cands[ncands][1] = -c / b; ncands++;
        if (a != 0.0f) {
          cands[ncands][0] = -c / a; cands[ncands][1] = 0.0f; ncands++;
        }
      }
    }

    for (j = 0; j < ncands; j++) {
      if (count == 0) {
        xmin = xmax = cands[j][0];
        ymin = ymax = cands[j][1];
      } else {
        if (cands[j][0] < xmin) xmin = cands[j][0];
        if (cands[j][0] > xmax) xmax = cands[j][0];
        if (cands[j][1] < ymin) ymin = cands[j][1];
        if (cands[j][1] > ymax) ymax = cands[j][1];
      }
      count++;
    }
  }

  if (count == 0) {
    graph_x_min=-10.0f; graph_x_max=10.0f;
    graph_y_min=-6.25f; graph_y_max=6.25f;
    return;
  }

  if (lines_only) {
    if (xmin > 0.0f) xmin = 0.0f;
    if (xmax < 0.0f) xmax = 0.0f;
    if (ymin > 0.0f) ymin = 0.0f;
    if (ymax < 0.0f) ymax = 0.0f;
  }

  xpad = (xmax - xmin) * 0.25f + 2.0f;
  ypad = (ymax - ymin) * 0.25f + 2.0f;
  graph_x_min = xmin - xpad; graph_x_max = xmax + xpad;
  graph_y_min = ymin - ypad; graph_y_max = ymax + ypad;
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
    if (ev == eadk_event_down || ev == eadk_event_ok || ev == eadk_event_exe) {
      graph_nav = GRAPH_SUBTOOL; sub_sel = SUB_AUTO;
      mark_dirty(DIRTY_CONTENT);
    }
    if (ev == eadk_event_back) {
      extern Tab current_tab;
      current_tab = TAB_INPUT;
      mark_dirty(DIRTY_TABS | DIRTY_CONTENT | DIRTY_TOOLBAR);
    }
    break;

  case GRAPH_SUBTOOL:
    switch (ev) {
      case eadk_event_left:
        if (sub_sel > 0) { sub_sel--; mark_dirty(DIRTY_CONTENT); } break;
      case eadk_event_right:
        if (sub_sel < SUB_COUNT-1) { sub_sel++; mark_dirty(DIRTY_CONTENT); } break;
      case eadk_event_down:
        graph_nav = GRAPH_IDLE; mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_back:
        graph_nav = GRAPH_IDLE; mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_ok:
      case eadk_event_exe:
        if (sub_sel == SUB_AUTO) {
          auto_fit(); mark_dirty(DIRTY_CONTENT | DIRTY_TOOLBAR);
        } else if (sub_sel == SUB_AXES) {
          show_axes = !show_axes; mark_dirty(DIRTY_CONTENT);
        } else if (sub_sel == SUB_NAVIGATE) {
          graph_nav = GRAPH_PAN; mark_dirty(DIRTY_CONTENT);
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
        graph_x_min -= dx; graph_x_max -= dx; mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_right:
        graph_x_min += dx; graph_x_max += dx; mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_up:
        graph_y_min += dy; graph_y_max += dy; mark_dirty(DIRTY_CONTENT); break;
      case eadk_event_down:
        graph_y_min -= dy; graph_y_max -= dy; mark_dirty(DIRTY_CONTENT); break;
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
        graph_nav = GRAPH_SUBTOOL; sub_sel = SUB_NAVIGATE;
        mark_dirty(DIRTY_CONTENT); break;
      default: break;
    }
    break;
  }
}