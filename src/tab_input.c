#include "tab_input.h"
#include "ui.h"
#include "elements.h"
#include <string.h>
#include <stdio.h>

// ─── State ────────────────────────────────────────────────────────────────────
InputState input_state = STATE_LIST;
int        selected_row = 0;

static int   type_menu_sel = 0;

// Point entry
static char  input_x_buf[16];
static char  input_y_buf[16];
static int   input_field = 0;

// Vector entry (from src — full origin support)
static char  vec_dx_buf[16];
static char  vec_dy_buf[16];
static char  vec_ox_buf[16];
static char  vec_oy_buf[16];
static int   vec_field      = 0;
static bool  vec_has_origin = false;

// Line entry
typedef enum {
  LINE_SUB_CHOOSE = 0,
  LINE_SUB_SLOPE_INTERCEPT,
  LINE_SUB_CARTESIAN,
} LineSubState;

static LineSubState line_substate  = LINE_SUB_CHOOSE;
static int          line_type_sel  = 0;
static char  line_m_buf[16];
static char  line_b_buf[16];
static int   line_si_field   = 0;
static char  line_a_buf[16];
static char  line_b2_buf[16];
static char  line_c_buf[16];
static int   line_cart_field = 0;

// Dot menu
static int   dot_menu_sel = 0;
static int   dot_menu_row = -1;

// List scroll
static int   list_scroll = 0;

#define ROW_H       36
#define ACCENT_W     4
#define CIRCLE_X    16
#define TEXT_X      30

// Vector form layout constants (from src)
#define VEC_FX  16
#define VEC_FW  288
#define VEC_FH_COLLAPSED  116
#define VEC_FH_EXPANDED   158
#define VEC_FH_MAX        158

// ─── Key → decimal char ───────────────────────────────────────────────────────
static char event_to_decimal(eadk_event_t ev) {
  switch (ev) {
    case eadk_event_zero:  return '0';
    case eadk_event_one:   return '1';
    case eadk_event_two:   return '2';
    case eadk_event_three: return '3';
    case eadk_event_four:  return '4';
    case eadk_event_five:  return '5';
    case eadk_event_six:   return '6';
    case eadk_event_seven: return '7';
    case eadk_event_eight: return '8';
    case eadk_event_nine:  return '9';
    case eadk_event_dot:   return '.';
    case eadk_event_minus: return '-';
    default:               return 0;
  }
}

// ─── Commit helpers ───────────────────────────────────────────────────────────
static void commit_vector(bool with_origin) {
  if (elem_count < MAX_ELEMENTS) {
    int vec_idx = count_type(ELEM_VECTOR);
    Element* e = &elements[elem_count];
    e->type       = ELEM_VECTOR;
    e->active     = true;
    e->si_form    = false;
    e->x          = simple_atof(vec_dx_buf);
    e->y          = simple_atof(vec_dy_buf);
    e->has_origin = with_origin;
    e->ox         = with_origin ? simple_atof(vec_ox_buf) : 0.0f;
    e->oy         = with_origin ? simple_atof(vec_oy_buf) : 0.0f;
    make_vector_label(vec_idx, e->label);
    elem_count++;
    selected_row = elem_count - 1;
  }
  input_state = STATE_LIST;
  mark_dirty(DIRTY_CONTENT);
}

static void commit_line_si(void) {
  float m, b;
  if (elem_count < MAX_ELEMENTS) {
    int line_idx = count_type(ELEM_LINE);
    Element* e = &elements[elem_count];
    m = simple_atof(line_m_buf);
    b = simple_atof(line_b_buf);
    e->type    = ELEM_LINE;
    e->active  = true;
    e->si_form = true;
    e->si_m    = m;
    e->si_b    = b;
    e->x  = m;      // a = m
    e->y  = -1.0f;  // b_coeff = -1
    e->ox = b;      // c = b_intercept
    e->oy = 0.0f;
    e->has_origin = false;
    make_line_label(line_idx, e->label);
    elem_count++;
    selected_row = elem_count - 1;
  }
  input_state = STATE_LIST;
  mark_dirty(DIRTY_CONTENT);
}

static void commit_line_cart(void) {
  if (elem_count < MAX_ELEMENTS) {
    int line_idx = count_type(ELEM_LINE);
    Element* e = &elements[elem_count];
    e->type    = ELEM_LINE;
    e->active  = true;
    e->si_form = false;
    e->si_m    = 0.0f;
    e->si_b    = 0.0f;
    e->x  = simple_atof(line_a_buf);
    e->y  = simple_atof(line_b2_buf);
    e->ox = simple_atof(line_c_buf);
    e->oy = 0.0f;
    e->has_origin = false;
    make_line_label(line_idx, e->label);
    elem_count++;
    selected_row = elem_count - 1;
  }
  input_state = STATE_LIST;
  mark_dirty(DIRTY_CONTENT);
}

// ─── Draw: element list ───────────────────────────────────────────────────────
void draw_input_tab(void) {
  int i;
  int total_rows = elem_count + 1;
  int total_h    = total_rows * ROW_H;
  int needs_scroll = (total_h > CONTENT_H);
  int bar_w = needs_scroll ? 4 : 0;

  fill_rect_clipped(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_WHITE);

  for (i = 0; i < elem_count; i++) {
    Element* e = &elements[i];
    int ry = CONTENT_Y + i * ROW_H - list_scroll;
    if (ry + ROW_H <= CONTENT_Y) continue;
    if (ry >= CONTENT_BOTTOM)    break;

    bool sel = (input_state == STATE_LIST && selected_row == i);
    eadk_color_t row_bg = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
    eadk_color_t ac = accent_colors[i % NUM_ACCENT_COLORS];

    fill_rect_clipped(0, ry, SCREEN_W - bar_w, ROW_H, row_bg);
    fill_rect_clipped(0, ry, ACCENT_W, ROW_H, ac);

    if (e->active)
      fill_circle_clipped(CIRCLE_X, ry + ROW_H / 2, 5, ac);
    else
      draw_circle_clipped(CIRCLE_X, ry + ROW_H / 2, 5, COLOR_DARK_GRAY);

    if (e->type == ELEM_POINT) {
      char main_label[32], sub_label[16], sx[12], sy[12];
      ftoa(e->x, sx, 2);
      ftoa(e->y, sy, 2);
      snprintf(main_label, sizeof(main_label), "(%s, %s)", sx, sy);
      snprintf(sub_label,  sizeof(sub_label),  "Point %s", e->label);
      draw_str_clipped(main_label, TEXT_X, ry + 4,  true,  COLOR_BLACK,     row_bg);
      draw_str_clipped(sub_label,  TEXT_X, ry + 22, false, COLOR_DARK_GRAY, row_bg);

    } else if (e->type == ELEM_VECTOR) {
      char main_label[32], sub_label[40], sdx[12], sdy[12];
      ftoa(e->x, sdx, 2);
      ftoa(e->y, sdy, 2);
      snprintf(main_label, sizeof(main_label), "(%s, %s)", sdx, sdy);
      draw_str_clipped(main_label, TEXT_X, ry + 4, true, COLOR_BLACK, row_bg);
      if (e->has_origin) {
        char sox[10], soy[10];
        ftoa(e->ox, sox, 2);
        ftoa(e->oy, soy, 2);
        snprintf(sub_label, sizeof(sub_label), "->%s  at (%s,%s)", e->label, sox, soy);
      } else {
        snprintf(sub_label, sizeof(sub_label), "->%s", e->label);
      }
      draw_str_clipped(sub_label, TEXT_X, ry + 22, false, COLOR_DARK_GRAY, row_bg);

    } else if (e->type == ELEM_LINE) {
      char main_label[32], sub_label[16], sa[12], sb[12], sc[12];
      int pos;
      if (e->si_form) {
        fmtnum(e->si_m, sa);
        fmtnum(e->si_b, sb);
        if (e->si_b == 0.0f)
          snprintf(main_label, sizeof(main_label), "y=%sx", sa);
        else if (e->si_b > 0.0f)
          snprintf(main_label, sizeof(main_label), "y=%sx+%s", sa, sb);
        else
          snprintf(main_label, sizeof(main_label), "y=%sx%s", sa, sb);
      } else {
        fmtnum(e->x,  sa);
        fmtnum(e->y,  sb);
        fmtnum(e->ox, sc);
        pos = 0;
        if (e->x != 0.0f)
          pos += snprintf(main_label+pos, sizeof(main_label)-pos, "%sx", sa);
        if (e->y != 0.0f) {
          if (pos > 0 && e->y > 0.0f)
            pos += snprintf(main_label+pos, sizeof(main_label)-pos, "+%sy", sb);
          else
            pos += snprintf(main_label+pos, sizeof(main_label)-pos, "%sy", sb);
        }
        if (e->ox != 0.0f) {
          if (pos > 0 && e->ox > 0.0f)
            pos += snprintf(main_label+pos, sizeof(main_label)-pos, "+%s", sc);
          else
            pos += snprintf(main_label+pos, sizeof(main_label)-pos, "%s", sc);
        }
        snprintf(main_label+pos, sizeof(main_label)-pos, "=0");
      }
      snprintf(sub_label, sizeof(sub_label), "Line %s", e->label);
      draw_str_clipped(main_label, TEXT_X, ry + 4,  true,  COLOR_BLACK,     row_bg);
      draw_str_clipped(sub_label,  TEXT_X, ry + 22, false, COLOR_DARK_GRAY, row_bg);
    }

    draw_str_clipped("...", SCREEN_W - 34 - bar_w, ry + 11, false, COLOR_DARK_GRAY, row_bg);
    draw_hline_clipped(0, ry + ROW_H - 1, SCREEN_W - bar_w, COLOR_LIGHT_GRAY);
  }

  // "Add an element" row
  {
    int ary = CONTENT_Y + elem_count * ROW_H - list_scroll;
    if (ary < CONTENT_BOTTOM && ary + ROW_H > CONTENT_Y) {
      bool sel = (input_state == STATE_LIST && selected_row == elem_count);
      eadk_color_t bg = sel ? COLOR_ROW_SELECTED : COLOR_ADD_ROW_BG;
      fill_rect_clipped(0, ary, SCREEN_W - bar_w, ROW_H, bg);
      draw_str_clipped("Add an element", 14, ary + (ROW_H - SMALL_FONT_H) / 2,
                       false, COLOR_BLACK, bg);
      draw_hline_clipped(0, ary + ROW_H - 1, SCREEN_W - bar_w, COLOR_LIGHT_GRAY);
    }
  }

  // Scrollbar
  if (needs_scroll) {
    int thumb_h, thumb_y;
    int tx = SCREEN_W - bar_w;
    fill_rect_clipped(tx, CONTENT_Y, bar_w, CONTENT_H, COLOR_LIGHT_GRAY);
    thumb_h = CONTENT_H * CONTENT_H / total_h;
    if (thumb_h < 8) thumb_h = 8;
    thumb_y = CONTENT_Y + list_scroll * (CONTENT_H - thumb_h) / (total_h - CONTENT_H);
    fill_rect_clipped(tx, thumb_y, bar_w, thumb_h, COLOR_DARK_GRAY);
  }
}

// ─── Draw: type selection popup ───────────────────────────────────────────────
static const char* type_names[] = {"Point", "Vector", "Line"};
#define NUM_TYPES 3

static void draw_type_menu(void) {
  int i, iy;
  bool sel;
  eadk_color_t bg;
  int mx = 20, my = CONTENT_Y + 8;
  int mw = 200, item_h = 28;
  int mh = 20 + NUM_TYPES * item_h;

  fill_rect_clipped(mx, my, mw, mh, COLOR_WHITE);
  draw_rect_border(mx, my, mw, mh, COLOR_DARK_GRAY);
  fill_rect_clipped(mx + 1, my + 1, mw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("Add element", mx + 6, my + 2, false, COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(mx, my + 17, mw, COLOR_DARK_GRAY);

  for (i = 0; i < NUM_TYPES; i++) {
    iy  = my + 18 + i * item_h;
    sel = (i == type_menu_sel);
    bg  = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
    fill_rect_clipped(mx + 1, iy, mw - 2, item_h, bg);
    if (sel) draw_str_clipped(">", mx + 4, iy + 7, false, COLOR_ORANGE, bg);
    draw_str_clipped(type_names[i], mx + 16, iy + 7, false, COLOR_BLACK, bg);
    if (i < NUM_TYPES - 1)
      draw_hline_clipped(mx, iy + item_h - 1, mw, COLOR_LIGHT_GRAY);
  }
}

// ─── Draw: point entry form ───────────────────────────────────────────────────
static void draw_point_entry(void) {
  bool x_sel, y_sel;
  eadk_color_t x_bg, x_border, y_bg, y_border;
  char xbuf[18], ybuf[18];
  int fx = 20, fy = CONTENT_Y + 10, fw = 280, fh = 100;

  fill_rect_clipped(fx, fy, fw, fh, COLOR_WHITE);
  draw_rect_border(fx, fy, fw, fh, COLOR_DARK_GRAY);
  fill_rect_clipped(fx + 1, fy + 1, fw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("New Point", fx + 6, fy + 2, false, COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(fx, fy + 17, fw, COLOR_DARK_GRAY);

  x_sel = (input_field == 0);
  x_bg = x_sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  x_border = x_sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 22, fw - 16, 24, x_bg);
  draw_rect_border(fx + 8, fy + 22, fw - 16, 24, x_border);
  draw_str_clipped("x =", fx + 12, fy + 27, false, COLOR_DARK_GRAY, x_bg);
  snprintf(xbuf, sizeof(xbuf), "%s%s", input_x_buf, x_sel ? "|" : "");
  draw_str_clipped(xbuf, fx + 48, fy + 27, false, COLOR_BLACK, x_bg);

  y_sel = (input_field == 1);
  y_bg = y_sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  y_border = y_sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 52, fw - 16, 24, y_bg);
  draw_rect_border(fx + 8, fy + 52, fw - 16, 24, y_border);
  draw_str_clipped("y =", fx + 12, fy + 57, false, COLOR_DARK_GRAY, y_bg);
  snprintf(ybuf, sizeof(ybuf), "%s%s", input_y_buf, y_sel ? "|" : "");
  draw_str_clipped(ybuf, fx + 48, fy + 57, false, COLOR_BLACK, y_bg);

  draw_str_clipped("EXE: next  Back: cancel", fx + 8, fy + 82,
                   false, COLOR_DARK_GRAY, COLOR_WHITE);
}

// ─── Draw: vector entry form (full src version with origin) ───────────────────
// Collapsed (vec_has_origin=false): dx, dy, [save] [origin]
// Expanded  (vec_has_origin=true):  dx, dy, ox, oy, [save]
static void draw_vector_entry(void) {
  bool sel;
  eadk_color_t bg, border;
  char buf[18];
  int fx = VEC_FX;
  int fy = CONTENT_Y + 8;
  int fw = VEC_FW;
  int fh = vec_has_origin ? VEC_FH_EXPANDED : VEC_FH_COLLAPSED;

  // Erase max height first to prevent ghost boxes
  fill_rect_clipped(fx, fy, fw, VEC_FH_MAX, COLOR_WHITE);

  fill_rect_clipped(fx, fy, fw, fh, COLOR_WHITE);
  draw_rect_border(fx, fy, fw, fh, COLOR_DARK_GRAY);
  fill_rect_clipped(fx + 1, fy + 1, fw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("New Vector", fx + 6, fy + 2, false, COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(fx, fy + 17, fw, COLOR_DARK_GRAY);

  // dx (field 0)
  sel    = (vec_field == 0);
  bg     = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  border = sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 22, fw - 16, 24, bg);
  draw_rect_border(fx + 8, fy + 22, fw - 16, 24, border);
  draw_str_clipped("dx =", fx + 12, fy + 27, false, COLOR_DARK_GRAY, bg);
  snprintf(buf, sizeof(buf), "%s%s", vec_dx_buf, sel ? "|" : "");
  draw_str_clipped(buf, fx + 58, fy + 27, false, COLOR_BLACK, bg);

  // dy (field 1)
  sel    = (vec_field == 1);
  bg     = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  border = sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 50, fw - 16, 24, bg);
  draw_rect_border(fx + 8, fy + 50, fw - 16, 24, border);
  draw_str_clipped("dy =", fx + 12, fy + 55, false, COLOR_DARK_GRAY, bg);
  snprintf(buf, sizeof(buf), "%s%s", vec_dy_buf, sel ? "|" : "");
  draw_str_clipped(buf, fx + 58, fy + 55, false, COLOR_BLACK, bg);

  if (!vec_has_origin) {
    // Two buttons: [save] (field 2)  [origin] (field 3)
    int by = fy + 82;
    int bw = (fw - 28) / 2;

    sel    = (vec_field == 2);
    bg     = sel ? COLOR_ROW_SELECTED : COLOR_ADD_ROW_BG;
    border = sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
    fill_rect_clipped(fx + 8, by, bw, 22, bg);
    draw_rect_border(fx + 8, by, bw, 22, border);
    draw_str_clipped("save", fx + 8 + (bw - 4*SMALL_FONT_W)/2, by + 4,
                     false, COLOR_BLACK, bg);

    sel    = (vec_field == 3);
    bg     = sel ? COLOR_ROW_SELECTED : COLOR_ADD_ROW_BG;
    border = sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
    fill_rect_clipped(fx + 8 + bw + 8, by, bw, 22, bg);
    draw_rect_border(fx + 8 + bw + 8, by, bw, 22, border);
    draw_str_clipped("origin", fx + 8 + bw + 8 + (bw - 6*SMALL_FONT_W)/2,
                     by + 4, false, COLOR_BLACK, bg);
  } else {
    // ox (field 2)
    sel    = (vec_field == 2);
    bg     = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
    border = sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
    fill_rect_clipped(fx + 8, fy + 78, fw - 16, 24, bg);
    draw_rect_border(fx + 8, fy + 78, fw - 16, 24, border);
    draw_str_clipped("ox =", fx + 12, fy + 83, false, COLOR_DARK_GRAY, bg);
    snprintf(buf, sizeof(buf), "%s%s", vec_ox_buf, sel ? "|" : "");
    draw_str_clipped(buf, fx + 58, fy + 83, false, COLOR_BLACK, bg);

    // oy (field 3)
    sel    = (vec_field == 3);
    bg     = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
    border = sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
    fill_rect_clipped(fx + 8, fy + 106, fw - 16, 24, bg);
    draw_rect_border(fx + 8, fy + 106, fw - 16, 24, border);
    draw_str_clipped("oy =", fx + 12, fy + 111, false, COLOR_DARK_GRAY, bg);
    snprintf(buf, sizeof(buf), "%s%s", vec_oy_buf, sel ? "|" : "");
    draw_str_clipped(buf, fx + 58, fy + 111, false, COLOR_BLACK, bg);

    // [save] button (field 4) — centered
    {
      int by = fy + 134;
      int bw = 120;
      int bx = fx + (fw - bw) / 2;
      sel    = (vec_field == 4);
      bg     = sel ? COLOR_ROW_SELECTED : COLOR_ADD_ROW_BG;
      border = sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
      fill_rect_clipped(bx, by, bw, 22, bg);
      draw_rect_border(bx, by, bw, 22, border);
      draw_str_clipped("save", bx + (bw - 4*SMALL_FONT_W)/2, by + 4,
                       false, COLOR_BLACK, bg);
    }
  }
}

// ─── Draw: line type picker ───────────────────────────────────────────────────
static void draw_line_type_picker(void) {
  bool sel;
  eadk_color_t bg;
  int iy;
  int mx = 20, my = CONTENT_Y + 8, mw = 220, item_h = 28;
  int mh = 20 + 2 * item_h;

  fill_rect_clipped(mx, my, mw, mh, COLOR_WHITE);
  draw_rect_border(mx, my, mw, mh, COLOR_DARK_GRAY);
  fill_rect_clipped(mx + 1, my + 1, mw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("Line form", mx + 6, my + 2, false, COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(mx, my + 17, mw, COLOR_DARK_GRAY);

  iy = my + 18;
  sel = (line_type_sel == 0);
  bg  = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  fill_rect_clipped(mx + 1, iy, mw - 2, item_h, bg);
  if (sel) draw_str_clipped(">", mx + 4, iy + 7, false, COLOR_ORANGE, bg);
  draw_str_clipped("y = mx + b", mx + 16, iy + 7, false, COLOR_BLACK, bg);

  iy += item_h;
  sel = (line_type_sel == 1);
  bg  = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  fill_rect_clipped(mx + 1, iy, mw - 2, item_h, bg);
  if (sel) draw_str_clipped(">", mx + 4, iy + 7, false, COLOR_ORANGE, bg);
  draw_str_clipped("ax+by+c=0", mx + 16, iy + 7, false, COLOR_BLACK, bg);
}

// ─── Draw: line slope-intercept entry ────────────────────────────────────────
static void draw_line_si_entry(void) {
  bool m_sel, b_sel;
  eadk_color_t m_bg, m_border, b_bg, b_border;
  char mbuf[18], bbuf[18];
  int fx = 20, fy = CONTENT_Y + 10, fw = 280, fh = 110;

  fill_rect_clipped(fx, fy, fw, fh, COLOR_WHITE);
  draw_rect_border(fx, fy, fw, fh, COLOR_DARK_GRAY);
  fill_rect_clipped(fx + 1, fy + 1, fw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("New Line", fx + 6, fy + 2, false, COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(fx, fy + 17, fw, COLOR_DARK_GRAY);

  m_sel = (line_si_field == 0);
  m_bg = m_sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  m_border = m_sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 22, fw - 16, 24, m_bg);
  draw_rect_border(fx + 8, fy + 22, fw - 16, 24, m_border);
  draw_str_clipped("m =", fx + 12, fy + 27, false, COLOR_DARK_GRAY, m_bg);
  snprintf(mbuf, sizeof(mbuf), "%s%s", line_m_buf, m_sel ? "|" : "");
  draw_str_clipped(mbuf, fx + 48, fy + 27, false, COLOR_BLACK, m_bg);

  b_sel = (line_si_field == 1);
  b_bg = b_sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  b_border = b_sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 52, fw - 16, 24, b_bg);
  draw_rect_border(fx + 8, fy + 52, fw - 16, 24, b_border);
  draw_str_clipped("b =", fx + 12, fy + 57, false, COLOR_DARK_GRAY, b_bg);
  snprintf(bbuf, sizeof(bbuf), "%s%s", line_b_buf, b_sel ? "|" : "");
  draw_str_clipped(bbuf, fx + 48, fy + 57, false, COLOR_BLACK, b_bg);

  draw_str_clipped("EXE: next  Back: cancel", fx + 8, fy + 82,
                   false, COLOR_DARK_GRAY, COLOR_WHITE);
}

// ─── Draw: line Cartesian entry ───────────────────────────────────────────────
static void draw_line_cart_entry(void) {
  bool a_sel, b_sel, c_sel;
  eadk_color_t a_bg, a_border, b_bg, b_border, c_bg, c_border;
  char abuf[18], b2buf[18], cbuf[18];
  int fx = 20, fy = CONTENT_Y + 8, fw = 280, fh = 140;

  fill_rect_clipped(fx, fy, fw, fh, COLOR_WHITE);
  draw_rect_border(fx, fy, fw, fh, COLOR_DARK_GRAY);
  fill_rect_clipped(fx + 1, fy + 1, fw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("New Line", fx + 6, fy + 2, false, COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(fx, fy + 17, fw, COLOR_DARK_GRAY);

  a_sel = (line_cart_field == 0);
  a_bg = a_sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  a_border = a_sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 22, fw - 16, 24, a_bg);
  draw_rect_border(fx + 8, fy + 22, fw - 16, 24, a_border);
  draw_str_clipped("a =", fx + 12, fy + 27, false, COLOR_DARK_GRAY, a_bg);
  snprintf(abuf, sizeof(abuf), "%s%s", line_a_buf, a_sel ? "|" : "");
  draw_str_clipped(abuf, fx + 48, fy + 27, false, COLOR_BLACK, a_bg);

  b_sel = (line_cart_field == 1);
  b_bg = b_sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  b_border = b_sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 52, fw - 16, 24, b_bg);
  draw_rect_border(fx + 8, fy + 52, fw - 16, 24, b_border);
  draw_str_clipped("b =", fx + 12, fy + 57, false, COLOR_DARK_GRAY, b_bg);
  snprintf(b2buf, sizeof(b2buf), "%s%s", line_b2_buf, b_sel ? "|" : "");
  draw_str_clipped(b2buf, fx + 48, fy + 57, false, COLOR_BLACK, b_bg);

  c_sel = (line_cart_field == 2);
  c_bg = c_sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  c_border = c_sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 82, fw - 16, 24, c_bg);
  draw_rect_border(fx + 8, fy + 82, fw - 16, 24, c_border);
  draw_str_clipped("c =", fx + 12, fy + 87, false, COLOR_DARK_GRAY, c_bg);
  snprintf(cbuf, sizeof(cbuf), "%s%s", line_c_buf, c_sel ? "|" : "");
  draw_str_clipped(cbuf, fx + 48, fy + 87, false, COLOR_BLACK, c_bg);

  draw_str_clipped("EXE: next  Back: cancel", fx + 8, fy + 112,
                   false, COLOR_DARK_GRAY, COLOR_WHITE);
}

// ─── Draw: dot menu ───────────────────────────────────────────────────────────
static void draw_dot_menu(void) {
  bool sel;
  eadk_color_t bg;
  int iy;
  int mx = SCREEN_W - 108;
  int my = CONTENT_Y + dot_menu_row * ROW_H + 4;
  int mw = 100, item_h = 28, mh = 20 + item_h;

  fill_rect_clipped(mx, my, mw, mh, COLOR_WHITE);
  draw_rect_border(mx, my, mw, mh, COLOR_DARK_GRAY);
  fill_rect_clipped(mx + 1, my + 1, mw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("Menu", mx + 6, my + 2, false, COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(mx, my + 17, mw, COLOR_DARK_GRAY);

  iy  = my + 18;
  sel = (dot_menu_sel == 0);
  bg  = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  fill_rect_clipped(mx + 1, iy, mw - 2, item_h, bg);
  if (sel) draw_str_clipped(">", mx + 4, iy + 7, false, COLOR_ORANGE, bg);
  draw_str_clipped("Delete", mx + 16, iy + 7, false, COLOR_BLACK, bg);
}

// ─── Combined overlay draw ────────────────────────────────────────────────────
void draw_input_overlay(void) {
  if (input_state == STATE_TYPE_MENU)    draw_type_menu();
  if (input_state == STATE_ENTER_POINT)  draw_point_entry();
  if (input_state == STATE_ENTER_VECTOR) draw_vector_entry();
  if (input_state == STATE_ENTER_LINE) {
    switch (line_substate) {
      case LINE_SUB_CHOOSE:          draw_line_type_picker(); break;
      case LINE_SUB_SLOPE_INTERCEPT: draw_line_si_entry();    break;
      case LINE_SUB_CARTESIAN:       draw_line_cart_entry();  break;
    }
  }
  if (input_state == STATE_DOT_MENU) draw_dot_menu();
}

// ─── Event handlers ───────────────────────────────────────────────────────────
static void handle_list(eadk_event_t ev) {
  int total   = elem_count + 1;
  int total_h = total * ROW_H;
  int max_scroll, row_top, row_bot;

  switch (ev) {
    case eadk_event_down:
      if (selected_row < total - 1) {
        selected_row++;
        max_scroll = total_h - CONTENT_H;
        if (max_scroll < 0) max_scroll = 0;
        row_bot = (selected_row + 1) * ROW_H - list_scroll;
        if (row_bot > CONTENT_H) {
          list_scroll += row_bot - CONTENT_H;
          if (list_scroll > max_scroll) list_scroll = max_scroll;
        }
        mark_dirty(DIRTY_CONTENT);
      }
      break;
    case eadk_event_up:
      if (selected_row > 0) {
        selected_row--;
        row_top = selected_row * ROW_H - list_scroll;
        if (row_top < 0) {
          list_scroll += row_top;
          if (list_scroll < 0) list_scroll = 0;
        }
        mark_dirty(DIRTY_CONTENT);
      }
      break;
    case eadk_event_right:
      if (selected_row < elem_count) {
        dot_menu_row = selected_row;
        dot_menu_sel = 0;
        input_state  = STATE_DOT_MENU;
        mark_dirty(DIRTY_OVERLAY);
      }
      break;
    case eadk_event_ok:
    case eadk_event_exe:
      if (selected_row == elem_count) {
        type_menu_sel = 0;
        input_state = STATE_TYPE_MENU;
        mark_dirty(DIRTY_OVERLAY);
      } else {
        elements[selected_row].active = !elements[selected_row].active;
        mark_dirty(DIRTY_CONTENT);
      }
      break;
    default: break;
  }
}

static void handle_type_menu(eadk_event_t ev) {
  switch (ev) {
    case eadk_event_down:
      if (type_menu_sel < NUM_TYPES - 1) { type_menu_sel++; mark_dirty(DIRTY_OVERLAY); }
      break;
    case eadk_event_up:
      if (type_menu_sel > 0) { type_menu_sel--; mark_dirty(DIRTY_OVERLAY); }
      break;
    case eadk_event_back:
      input_state = STATE_LIST;
      mark_dirty(DIRTY_CONTENT);
      break;
    case eadk_event_right:
    case eadk_event_ok:
    case eadk_event_exe:
      if (type_menu_sel == 0) {
        input_x_buf[0] = '\0'; input_y_buf[0] = '\0';
        input_field = 0;
        input_state = STATE_ENTER_POINT;
      } else if (type_menu_sel == 1) {
        vec_dx_buf[0] = '\0'; vec_dy_buf[0] = '\0';
        vec_ox_buf[0] = '\0'; vec_oy_buf[0] = '\0';
        vec_field = 0; vec_has_origin = false;
        input_state = STATE_ENTER_VECTOR;
      } else if (type_menu_sel == 2) {
        line_substate = LINE_SUB_CHOOSE;
        line_type_sel = 0;
        input_state = STATE_ENTER_LINE;
      }
      mark_dirty(DIRTY_OVERLAY);
      break;
    default: break;
  }
}

static void handle_point_entry(eadk_event_t ev) {
  char* cur = (input_field == 0) ? input_x_buf : input_y_buf;
  char c;

  if (ev == eadk_event_back) {
    if (strlen(cur) > 0)       { buf_backspace(cur); mark_dirty(DIRTY_OVERLAY); }
    else if (input_field == 1) { input_field = 0; mark_dirty(DIRTY_OVERLAY); }
    else                       { input_state = STATE_TYPE_MENU; mark_dirty(DIRTY_OVERLAY); }
    return;
  }
  if (ev == eadk_event_down && input_field == 0) { input_field = 1; mark_dirty(DIRTY_OVERLAY); return; }
  if (ev == eadk_event_up   && input_field == 1) { input_field = 0; mark_dirty(DIRTY_OVERLAY); return; }
  if (ev == eadk_event_ok || ev == eadk_event_exe) {
    if (input_field == 0) { input_field = 1; mark_dirty(DIRTY_OVERLAY); }
    else {
      if (elem_count < MAX_ELEMENTS) {
        int point_idx = count_type(ELEM_POINT);
        Element* e = &elements[elem_count];
        e->type       = ELEM_POINT;
        e->active     = true;
        e->si_form    = false;
        e->x          = simple_atof(input_x_buf);
        e->y          = simple_atof(input_y_buf);
        e->ox         = 0.0f;
        e->oy         = 0.0f;
        e->has_origin = false;
        make_point_label(point_idx, e->label);
        elem_count++;
        selected_row = elem_count - 1;
      }
      input_state = STATE_LIST;
      mark_dirty(DIRTY_CONTENT);
    }
    return;
  }
  c = event_to_decimal(ev);
  if (c) { buf_append(cur, c, 15); mark_dirty(DIRTY_OVERLAY); }
}

// ─── Vector entry handler (full src version) ──────────────────────────────────
// Collapsed fields: 0=dx, 1=dy, 2=save-btn, 3=origin-btn
// Expanded  fields: 0=dx, 1=dy, 2=ox,       3=oy,          4=save-btn
static void handle_vector_entry(eadk_event_t ev) {
  char* cur = NULL;
  char c;

  if      (vec_field == 0) cur = vec_dx_buf;
  else if (vec_field == 1) cur = vec_dy_buf;
  else if (vec_has_origin && vec_field == 2) cur = vec_ox_buf;
  else if (vec_has_origin && vec_field == 3) cur = vec_oy_buf;

  // Back
  if (ev == eadk_event_back) {
    if (cur && strlen(cur) > 0) { buf_backspace(cur); mark_dirty(DIRTY_OVERLAY); return; }
    if (!vec_has_origin) {
      switch (vec_field) {
        case 0: input_state = STATE_TYPE_MENU; mark_dirty(DIRTY_OVERLAY); break;
        case 1: vec_field = 0; mark_dirty(DIRTY_OVERLAY); break;
        case 2: vec_field = 1; mark_dirty(DIRTY_OVERLAY); break;
        case 3: vec_field = 2; mark_dirty(DIRTY_OVERLAY); break;
        default: break;
      }
    } else {
      switch (vec_field) {
        case 0: input_state = STATE_TYPE_MENU; mark_dirty(DIRTY_OVERLAY); break;
        case 1: vec_field = 0; mark_dirty(DIRTY_OVERLAY); break;
        case 2:
          vec_ox_buf[0] = '\0'; vec_oy_buf[0] = '\0';
          vec_has_origin = false; vec_field = 3; mark_dirty(DIRTY_OVERLAY); break;
        case 3: vec_field = 2; mark_dirty(DIRTY_OVERLAY); break;
        case 4: vec_field = 3; mark_dirty(DIRTY_OVERLAY); break;
        default: break;
      }
    }
    return;
  }

  // Left/Right in button row (collapsed)
  if (ev == eadk_event_left || ev == eadk_event_right) {
    if (!vec_has_origin) {
      if      (vec_field == 2) { vec_field = 3; mark_dirty(DIRTY_OVERLAY); }
      else if (vec_field == 3) { vec_field = 2; mark_dirty(DIRTY_OVERLAY); }
    }
    return;
  }

  // Down
  if (ev == eadk_event_down) {
    if (!vec_has_origin) {
      if      (vec_field == 0) vec_field = 1;
      else if (vec_field == 1) vec_field = 2;
      else if (vec_field == 2) vec_field = 3;
    } else {
      if      (vec_field == 0) vec_field = 1;
      else if (vec_field == 1) vec_field = 2;
      else if (vec_field == 2) vec_field = 3;
      else if (vec_field == 3) vec_field = 4;
    }
    mark_dirty(DIRTY_OVERLAY); return;
  }

  // Up
  if (ev == eadk_event_up) {
    if (!vec_has_origin) {
      if      (vec_field == 3) vec_field = 2;
      else if (vec_field == 2) vec_field = 1;
      else if (vec_field == 1) vec_field = 0;
    } else {
      if      (vec_field == 4) vec_field = 3;
      else if (vec_field == 3) vec_field = 2;
      else if (vec_field == 2) vec_field = 1;
      else if (vec_field == 1) vec_field = 0;
    }
    mark_dirty(DIRTY_OVERLAY); return;
  }

  // EXE / OK
  if (ev == eadk_event_ok || ev == eadk_event_exe) {
    if (!vec_has_origin) {
      switch (vec_field) {
        case 0: vec_field = 1; mark_dirty(DIRTY_OVERLAY); break;
        case 1: vec_field = 2; mark_dirty(DIRTY_OVERLAY); break;
        case 2: commit_vector(false); break;
        case 3:
          vec_has_origin = true; vec_field = 2; mark_dirty(DIRTY_OVERLAY); break;
        default: break;
      }
    } else {
      switch (vec_field) {
        case 0: vec_field = 1; mark_dirty(DIRTY_OVERLAY); break;
        case 1: vec_field = 2; mark_dirty(DIRTY_OVERLAY); break;
        case 2: vec_field = 3; mark_dirty(DIRTY_OVERLAY); break;
        case 3: vec_field = 4; mark_dirty(DIRTY_OVERLAY); break;
        case 4: commit_vector(true); break;
        default: break;
      }
    }
    return;
  }

  // Digits
  {
    bool is_text = (vec_field == 0 || vec_field == 1 ||
                    (vec_has_origin && (vec_field == 2 || vec_field == 3)));
    if (is_text) {
      c = event_to_decimal(ev);
      if (c && cur) { buf_append(cur, c, 15); mark_dirty(DIRTY_OVERLAY); }
    }
  }
}

static void handle_line_choose(eadk_event_t ev) {
  switch (ev) {
    case eadk_event_down: if (line_type_sel < 1) { line_type_sel++; mark_dirty(DIRTY_OVERLAY); } break;
    case eadk_event_up:   if (line_type_sel > 0) { line_type_sel--; mark_dirty(DIRTY_OVERLAY); } break;
    case eadk_event_back: input_state = STATE_TYPE_MENU; mark_dirty(DIRTY_OVERLAY); break;
    case eadk_event_ok:
    case eadk_event_exe:
      if (line_type_sel == 0) {
        line_substate = LINE_SUB_SLOPE_INTERCEPT; line_si_field = 0;
        line_m_buf[0] = '\0'; line_b_buf[0] = '\0';
      } else {
        line_substate = LINE_SUB_CARTESIAN; line_cart_field = 0;
        line_a_buf[0] = '\0'; line_b2_buf[0] = '\0'; line_c_buf[0] = '\0';
      }
      mark_dirty(DIRTY_OVERLAY); break;
    default: break;
  }
}

static void handle_line_si(eadk_event_t ev) {
  char* cur = (line_si_field == 0) ? line_m_buf : line_b_buf;
  char c;
  if (ev == eadk_event_back) {
    if (strlen(cur) > 0)         { buf_backspace(cur); mark_dirty(DIRTY_OVERLAY); }
    else if (line_si_field == 1) { line_si_field = 0; mark_dirty(DIRTY_OVERLAY); }
    else                         { line_substate = LINE_SUB_CHOOSE; mark_dirty(DIRTY_OVERLAY); }
    return;
  }
  if (ev == eadk_event_down && line_si_field == 0) { line_si_field = 1; mark_dirty(DIRTY_OVERLAY); return; }
  if (ev == eadk_event_up   && line_si_field == 1) { line_si_field = 0; mark_dirty(DIRTY_OVERLAY); return; }
  if (ev == eadk_event_ok || ev == eadk_event_exe) {
    if (line_si_field == 0) { line_si_field = 1; mark_dirty(DIRTY_OVERLAY); }
    else                    { commit_line_si(); }
    return;
  }
  c = event_to_decimal(ev);
  if (c) { buf_append(cur, c, 15); mark_dirty(DIRTY_OVERLAY); }
}

static void handle_line_cart(eadk_event_t ev) {
  char* cur;
  char c;
  if      (line_cart_field == 0) cur = line_a_buf;
  else if (line_cart_field == 1) cur = line_b2_buf;
  else if (line_cart_field == 2) cur = line_c_buf;
  else                           cur = line_a_buf;
  if (ev == eadk_event_back) {
    if (strlen(cur) > 0)          { buf_backspace(cur); mark_dirty(DIRTY_OVERLAY); }
    else if (line_cart_field > 0) { line_cart_field--; mark_dirty(DIRTY_OVERLAY); }
    else                          { line_substate = LINE_SUB_CHOOSE; mark_dirty(DIRTY_OVERLAY); }
    return;
  }
  if (ev == eadk_event_down && line_cart_field < 2) { line_cart_field++; mark_dirty(DIRTY_OVERLAY); return; }
  if (ev == eadk_event_up   && line_cart_field > 0) { line_cart_field--; mark_dirty(DIRTY_OVERLAY); return; }
  if (ev == eadk_event_ok || ev == eadk_event_exe) {
    if (line_cart_field < 2) { line_cart_field++; mark_dirty(DIRTY_OVERLAY); }
    else                     { commit_line_cart(); }
    return;
  }
  c = event_to_decimal(ev);
  if (c) { buf_append(cur, c, 15); mark_dirty(DIRTY_OVERLAY); }
}

static void handle_line_entry(eadk_event_t ev) {
  switch (line_substate) {
    case LINE_SUB_CHOOSE:          handle_line_choose(ev); break;
    case LINE_SUB_SLOPE_INTERCEPT: handle_line_si(ev);     break;
    case LINE_SUB_CARTESIAN:       handle_line_cart(ev);   break;
  }
}

static void handle_dot_menu(eadk_event_t ev) {
  int i;
  switch (ev) {
    case eadk_event_back:
      input_state = STATE_LIST; mark_dirty(DIRTY_CONTENT); break;
    case eadk_event_up:
      if (dot_menu_sel > 0) { dot_menu_sel--; mark_dirty(DIRTY_OVERLAY); } break;
    case eadk_event_down:
      if (dot_menu_sel < 0) { dot_menu_sel++; mark_dirty(DIRTY_OVERLAY); } break;
    case eadk_event_ok:
    case eadk_event_exe:
      if (dot_menu_sel == 0) {
        for (i = dot_menu_row; i < elem_count - 1; i++)
          elements[i] = elements[i + 1];
        elem_count--;
        if (selected_row >= elem_count)
          selected_row = elem_count > 0 ? elem_count - 1 : 0;
        input_state = STATE_LIST;
        mark_dirty(DIRTY_CONTENT);
      }
      break;
    default: break;
  }
}

void handle_input_event(eadk_event_t ev) {
  switch (input_state) {
    case STATE_LIST:         handle_list(ev);          break;
    case STATE_TYPE_MENU:    handle_type_menu(ev);     break;
    case STATE_ENTER_POINT:  handle_point_entry(ev);   break;
    case STATE_ENTER_VECTOR: handle_vector_entry(ev);  break;
    case STATE_ENTER_LINE:   handle_line_entry(ev);    break;
    case STATE_DOT_MENU:     handle_dot_menu(ev);      break;
  }
}

void input_reset_scroll(void) { list_scroll = 0; selected_row = 0; }