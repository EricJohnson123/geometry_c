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
static int   input_field = 0;  // 0=x, 1=y

// Vector entry
// Fields: 0=dx, 1=dy, 2=button, 3=ox, 4=oy
static char  vec_dx_buf[16];
static char  vec_dy_buf[16];
static char  vec_ox_buf[16];
static char  vec_oy_buf[16];
static int   vec_field      = 0;
static bool  vec_has_origin = false;

#define ROW_H       36
#define ACCENT_W     4
#define CIRCLE_X    16
#define TEXT_X      30

static int list_scroll = 0;  // pixel scroll offset for the element list

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

// ─── Helper: count elements of a given type ───────────────────────────────────
static int count_type(ElementType t) {
  int i, n = 0;
  for (i = 0; i < elem_count; i++)
    if (elements[i].type == t) n++;
  return n;
}

// ─── Commit a vector ──────────────────────────────────────────────────────────
static void commit_vector(bool with_origin) {
  if (elem_count < MAX_ELEMENTS) {
    int vec_idx = count_type(ELEM_VECTOR);
    Element* e = &elements[elem_count];
    e->type       = ELEM_VECTOR;
    e->active     = true;
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

// ─── Draw: element list ───────────────────────────────────────────────────────
void draw_input_tab(void) {
  int i;
  int total_rows = elem_count + 1;          // elements + "Add" row
  int total_h    = total_rows * ROW_H;
  int needs_scroll = (total_h > CONTENT_H);
  int bar_w = needs_scroll ? 4 : 0;        // scrollbar width reserved on right

  fill_rect_clipped(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_WHITE);

  for (i = 0; i < elem_count; i++) {
    Element* e = &elements[i];
    int ry = CONTENT_Y + i * ROW_H - list_scroll;
    // Skip rows fully outside content area
    if (ry + ROW_H <= CONTENT_Y) continue;
    if (ry >= CONTENT_BOTTOM)   break;

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
      char main_label[32], sx[12], sy[12], sub_label[16];
      ftoa(e->x, sx, 2);
      ftoa(e->y, sy, 2);
      snprintf(main_label, sizeof(main_label), "(%s, %s)", sx, sy);
      draw_str_clipped(main_label, TEXT_X, ry + 4, true, COLOR_BLACK, row_bg);
      snprintf(sub_label, sizeof(sub_label), "Point %s", e->label);
      draw_str_clipped(sub_label, TEXT_X, ry + 22, false, COLOR_DARK_GRAY, row_bg);

    } else if (e->type == ELEM_VECTOR) {
      char main_label[32], sdx[12], sdy[12], sub_label[40];
      ftoa(e->x, sdx, 2);
      ftoa(e->y, sdy, 2);
      snprintf(main_label, sizeof(main_label), "(%s, %s)", sdx, sdy);
      draw_str_clipped(main_label, TEXT_X, ry + 4, true, COLOR_BLACK, row_bg);
      if (e->has_origin) {
        char sox[10], soy[10];
        ftoa(e->ox, sox, 2);
        ftoa(e->oy, soy, 2);
        snprintf(sub_label, sizeof(sub_label), "->%s  at (%s,%s)",
                 e->label, sox, soy);
      } else {
        snprintf(sub_label, sizeof(sub_label), "->%s", e->label);
      }
      draw_str_clipped(sub_label, TEXT_X, ry + 22, false, COLOR_DARK_GRAY, row_bg);
    }

    draw_str_clipped("...", SCREEN_W - 34 - bar_w, ry + 11,
                     false, COLOR_DARK_GRAY, row_bg);
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
    thumb_y = CONTENT_Y + list_scroll * (CONTENT_H - thumb_h) /
              (total_h - CONTENT_H);
    fill_rect_clipped(tx, thumb_y, bar_w, thumb_h, COLOR_DARK_GRAY);
  }
}

// ─── Draw: type selection popup ───────────────────────────────────────────────
static const char* type_names[] = {"Point", "Vector"};
#define NUM_TYPES 2

static void draw_type_menu(void) {
  int i;
  int mx = 20, my = CONTENT_Y + 8;
  int mw = 200, item_h = 28;
  int mh = 20 + NUM_TYPES * item_h;

  fill_rect_clipped(mx, my, mw, mh, COLOR_WHITE);
  draw_rect_border(mx, my, mw, mh, COLOR_DARK_GRAY);
  fill_rect_clipped(mx + 1, my + 1, mw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("Add element", mx + 6, my + 2, false,
                   COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(mx, my + 17, mw, COLOR_DARK_GRAY);

  for (i = 0; i < NUM_TYPES; i++) {
    int iy = my + 18 + i * item_h;
    bool sel = (i == type_menu_sel);
    eadk_color_t bg = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
    fill_rect_clipped(mx + 1, iy, mw - 2, item_h, bg);
    if (sel)
      draw_str_clipped(">", mx + 4, iy + 7, false, COLOR_ORANGE, bg);
    draw_str_clipped(type_names[i], mx + 16, iy + 7, false, COLOR_BLACK, bg);
    if (i < NUM_TYPES - 1)
      draw_hline_clipped(mx, iy + item_h - 1, mw, COLOR_LIGHT_GRAY);
  }
}

// ─── Draw: point entry form ───────────────────────────────────────────────────
static void draw_point_entry(void) {
  int fx = 20, fy = CONTENT_Y + 10;
  int fw = 280, fh = 100;

  fill_rect_clipped(fx, fy, fw, fh, COLOR_WHITE);
  draw_rect_border(fx, fy, fw, fh, COLOR_DARK_GRAY);
  fill_rect_clipped(fx + 1, fy + 1, fw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("New Point", fx + 6, fy + 2, false,
                   COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(fx, fy + 17, fw, COLOR_DARK_GRAY);

  {
    bool sel = (input_field == 0);
    eadk_color_t bg = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
    eadk_color_t border = sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
    char buf[18];
    fill_rect_clipped(fx + 8, fy + 22, fw - 16, 24, bg);
    draw_rect_border(fx + 8, fy + 22, fw - 16, 24, border);
    draw_str_clipped("x =", fx + 12, fy + 27, false, COLOR_DARK_GRAY, bg);
    snprintf(buf, sizeof(buf), "%s%s", input_x_buf, sel ? "|" : "");
    draw_str_clipped(buf, fx + 48, fy + 27, false, COLOR_BLACK, bg);
  }
  {
    bool sel = (input_field == 1);
    eadk_color_t bg = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
    eadk_color_t border = sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
    char buf[18];
    fill_rect_clipped(fx + 8, fy + 52, fw - 16, 24, bg);
    draw_rect_border(fx + 8, fy + 52, fw - 16, 24, border);
    draw_str_clipped("y =", fx + 12, fy + 57, false, COLOR_DARK_GRAY, bg);
    snprintf(buf, sizeof(buf), "%s%s", input_y_buf, sel ? "|" : "");
    draw_str_clipped(buf, fx + 48, fy + 57, false, COLOR_BLACK, bg);
  }
  draw_str_clipped("EXE: next  Back: cancel", fx + 8, fy + 82,
                   false, COLOR_DARK_GRAY, COLOR_WHITE);
}

// ─── Draw: vector entry form ──────────────────────────────────────────────────
// Collapsed layout (vec_has_origin=false):
//   ┌─────────────────────────────────────────┐
//   │ New Vector                              │
//   ├─────────────────────────────────────────┤
//   │  dx = [                               ] │
//   │  dy = [                               ] │
//   │                                         │
//   │   [      save      ] [  start point  ]  │
//   └─────────────────────────────────────────┘
//
// Expanded layout (vec_has_origin=true):
//   ┌─────────────────────────────────────────┐
//   │ New Vector                              │
//   ├─────────────────────────────────────────┤
//   │  dx = [                               ] │
//   │  dy = [                               ] │
//   │  ox = [                               ] │
//   │  oy = [                               ] │
//   │                                         │
//   │            [        save        ]       │
//   └─────────────────────────────────────────┘
//
// Field numbers:
//   Collapsed:  0=dx, 1=dy, 2=save-btn, 3=start-point-btn
//   Expanded:   0=dx, 1=dy, 2=ox, 3=oy, 4=save-btn

#define VEC_FX  16
#define VEC_FW  288
#define VEC_FH_COLLAPSED  116
#define VEC_FH_EXPANDED   158
#define VEC_FH_MAX        158   // always erase this much to avoid ghost box

static void draw_vector_entry(void) {
  int fx = VEC_FX;
  int fy = CONTENT_Y + 8;
  int fw = VEC_FW;
  int fh = vec_has_origin ? VEC_FH_EXPANDED : VEC_FH_COLLAPSED;
  char buf[18];
  bool sel;
  eadk_color_t bg, border;

  // Always erase the max possible height first to kill ghost boxes
  fill_rect_clipped(fx, fy, fw, VEC_FH_MAX, COLOR_WHITE);

  // Box
  fill_rect_clipped(fx, fy, fw, fh, COLOR_WHITE);
  draw_rect_border(fx, fy, fw, fh, COLOR_DARK_GRAY);

  // Title bar
  fill_rect_clipped(fx + 1, fy + 1, fw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("New Vector", fx + 6, fy + 2, false,
                   COLOR_WHITE, COLOR_TAB_INACTIVE);
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
    // Two side-by-side buttons on one row
    // [save] on left (field 2), [start point] on right (field 3)
    int by = fy + 82;
    int bw = (fw - 28) / 2;  // ~128px each with 8px gap and margins

    // [save] button (field 2)
    sel    = (vec_field == 2);
    bg     = sel ? COLOR_ROW_SELECTED : COLOR_ADD_ROW_BG;
    border = sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
    fill_rect_clipped(fx + 8,          by, bw, 22, bg);
    draw_rect_border(fx + 8,          by, bw, 22, border);
    draw_str_clipped("save", fx + 8 + (bw - 4*SMALL_FONT_W)/2, by + 4,
                     false, COLOR_BLACK, bg);

    // [start point] button (field 3)
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

// ─── Combined draw entry point ────────────────────────────────────────────────
void draw_input_overlay(void) {
  if (input_state == STATE_TYPE_MENU)    draw_type_menu();
  if (input_state == STATE_ENTER_POINT)  draw_point_entry();
  if (input_state == STATE_ENTER_VECTOR) draw_vector_entry();
}

// ─── Event handlers ───────────────────────────────────────────────────────────
static void handle_list(eadk_event_t ev) {
  int total = elem_count + 1;
  int total_h, max_scroll, row_top, row_bot;

  switch (ev) {
    case eadk_event_down:
      if (selected_row < total - 1) {
        selected_row++;
        // Scroll down if selected row bottom is below visible area
        total_h   = total * ROW_H;
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
        // Scroll up if selected row top is above visible area
        row_top = selected_row * ROW_H - list_scroll;
        if (row_top < 0) {
          list_scroll += row_top;
          if (list_scroll < 0) list_scroll = 0;
        }
        mark_dirty(DIRTY_CONTENT);
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
      if (type_menu_sel > 0)             { type_menu_sel--; mark_dirty(DIRTY_OVERLAY); }
      break;
    case eadk_event_back:
      input_state = STATE_LIST;
      mark_dirty(DIRTY_CONTENT);
      break;
    case eadk_event_right:   // right arrow confirms, same as EXE
    case eadk_event_ok:
    case eadk_event_exe:
      if (type_menu_sel == 0) {
        input_x_buf[0] = '\0';
        input_y_buf[0] = '\0';
        input_field = 0;
        input_state = STATE_ENTER_POINT;
      } else {
        vec_dx_buf[0] = '\0';
        vec_dy_buf[0] = '\0';
        vec_ox_buf[0] = '\0';
        vec_oy_buf[0] = '\0';
        vec_field = 0;
        vec_has_origin = false;
        input_state = STATE_ENTER_VECTOR;
      }
      mark_dirty(DIRTY_OVERLAY);
      break;
    default: break;
  }
}

static void handle_point_entry(eadk_event_t ev) {
  char* cur = (input_field == 0) ? input_x_buf : input_y_buf;

  if (ev == eadk_event_back) {
    if (strlen(cur) > 0)       { buf_backspace(cur); mark_dirty(DIRTY_OVERLAY); }
    else if (input_field == 1) { input_field = 0; mark_dirty(DIRTY_OVERLAY); }
    else                       { input_state = STATE_TYPE_MENU; mark_dirty(DIRTY_OVERLAY); }
    return;
  }
  if (ev == eadk_event_down && input_field == 0) {
    input_field = 1; mark_dirty(DIRTY_OVERLAY); return;
  }
  if (ev == eadk_event_up && input_field == 1) {
    input_field = 0; mark_dirty(DIRTY_OVERLAY); return;
  }
  if (ev == eadk_event_ok || ev == eadk_event_exe) {
    if (input_field == 0) {
      input_field = 1; mark_dirty(DIRTY_OVERLAY);
    } else {
      if (elem_count < MAX_ELEMENTS) {
        int point_idx = count_type(ELEM_POINT);
        Element* e = &elements[elem_count];
        e->type       = ELEM_POINT;
        e->active     = true;
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
  {
    char c = event_to_decimal(ev);
    if (c) { buf_append(cur, c, 15); mark_dirty(DIRTY_OVERLAY); }
  }
}

// ─── Vector entry handler ─────────────────────────────────────────────────────
// Collapsed fields: 0=dx, 1=dy, 2=save-btn, 3=start-point-btn
// Expanded fields:  0=dx, 1=dy, 2=ox,       3=oy,            4=save-btn
//
// EXE on save (field 2 collapsed / field 4 expanded) → commit
// EXE on start-point (field 3 collapsed) → expand, jump to ox (field 2)
// Back on dx (field 0, empty) → type menu
// Back on any other field (empty) → previous field
// Back on ox (field 2 expanded, empty) → collapse, land on start-point (field 3)
// Down/Up navigate through fields

static void handle_vector_entry(eadk_event_t ev) {
  char* cur = NULL;
  if      (vec_field == 0) cur = vec_dx_buf;
  else if (vec_field == 1) cur = vec_dy_buf;
  else if (vec_has_origin && vec_field == 2) cur = vec_ox_buf;
  else if (vec_has_origin && vec_field == 3) cur = vec_oy_buf;
  // fields 2/3 (collapsed) and 4 (expanded) are buttons — no buffer

  // ── Back ─────────────────────────────────────────────────────────────────
  if (ev == eadk_event_back) {
    if (cur && strlen(cur) > 0) {
      buf_backspace(cur); mark_dirty(DIRTY_OVERLAY); return;
    }
    if (!vec_has_origin) {
      // Collapsed
      switch (vec_field) {
        case 0: input_state = STATE_TYPE_MENU; mark_dirty(DIRTY_OVERLAY); break;
        case 1: vec_field = 0; mark_dirty(DIRTY_OVERLAY); break;
        case 2: vec_field = 1; mark_dirty(DIRTY_OVERLAY); break;
        case 3: vec_field = 2; mark_dirty(DIRTY_OVERLAY); break;
        default: break;
      }
    } else {
      // Expanded
      switch (vec_field) {
        case 0: input_state = STATE_TYPE_MENU; mark_dirty(DIRTY_OVERLAY); break;
        case 1: vec_field = 0; mark_dirty(DIRTY_OVERLAY); break;
        case 2:
          // Back on ox (empty) → collapse, land on start-point btn (field 3)
          vec_ox_buf[0] = '\0';
          vec_oy_buf[0] = '\0';
          vec_has_origin = false;
          vec_field = 3; mark_dirty(DIRTY_OVERLAY); break;
        case 3: vec_field = 2; mark_dirty(DIRTY_OVERLAY); break;
        case 4: vec_field = 3; mark_dirty(DIRTY_OVERLAY); break;
        default: break;
      }
    }
    return;
  }

  // ── Left / Right (button row navigation in collapsed state) ─────────────────
  if (ev == eadk_event_left || ev == eadk_event_right) {
    if (!vec_has_origin) {
      if (vec_field == 2) { vec_field = 3; mark_dirty(DIRTY_OVERLAY); }
      else if (vec_field == 3) { vec_field = 2; mark_dirty(DIRTY_OVERLAY); }
    }
    return;
  }
  if (ev == eadk_event_down) {
    if (!vec_has_origin) {
      if (vec_field == 0) vec_field = 1;
      else if (vec_field == 1) vec_field = 2;
      else if (vec_field == 2) vec_field = 3;
      // field 3 = last, no wrap
    } else {
      if (vec_field == 0) vec_field = 1;
      else if (vec_field == 1) vec_field = 2;
      else if (vec_field == 2) vec_field = 3;
      else if (vec_field == 3) vec_field = 4;
      // field 4 = last
    }
    mark_dirty(DIRTY_OVERLAY); return;
  }

  // ── Up ───────────────────────────────────────────────────────────────────
  if (ev == eadk_event_up) {
    if (!vec_has_origin) {
      if (vec_field == 3) vec_field = 2;
      else if (vec_field == 2) vec_field = 1;
      else if (vec_field == 1) vec_field = 0;
      // field 0 = first
    } else {
      if (vec_field == 4) vec_field = 3;
      else if (vec_field == 3) vec_field = 2;
      else if (vec_field == 2) vec_field = 1;
      else if (vec_field == 1) vec_field = 0;
    }
    mark_dirty(DIRTY_OVERLAY); return;
  }

  // ── EXE / OK ─────────────────────────────────────────────────────────────
  if (ev == eadk_event_ok || ev == eadk_event_exe) {
    if (!vec_has_origin) {
      switch (vec_field) {
        case 0: vec_field = 1; mark_dirty(DIRTY_OVERLAY); break;
        case 1: vec_field = 2; mark_dirty(DIRTY_OVERLAY); break;
        case 2:
          // save button → commit without origin
          commit_vector(false); break;
        case 3:
          // start point button → expand, jump to ox
          vec_has_origin = true;
          vec_field = 2; mark_dirty(DIRTY_OVERLAY); break;
        default: break;
      }
    } else {
      switch (vec_field) {
        case 0: vec_field = 1; mark_dirty(DIRTY_OVERLAY); break;
        case 1: vec_field = 2; mark_dirty(DIRTY_OVERLAY); break;
        case 2: vec_field = 3; mark_dirty(DIRTY_OVERLAY); break;
        case 3: vec_field = 4; mark_dirty(DIRTY_OVERLAY); break;
        case 4:
          // save button → commit with origin
          commit_vector(true); break;
        default: break;
      }
    }
    return;
  }

  // ── Digits / dot / minus (text fields only) ───────────────────────────────
  {
    bool is_text_field = (vec_field == 0 || vec_field == 1 ||
                          (vec_has_origin && (vec_field == 2 || vec_field == 3)));
    if (is_text_field) {
      char c = event_to_decimal(ev);
      if (c && cur) { buf_append(cur, c, 15); mark_dirty(DIRTY_OVERLAY); }
    }
  }
}

void handle_input_event(eadk_event_t ev) {
  switch (input_state) {
    case STATE_LIST:         handle_list(ev);          break;
    case STATE_TYPE_MENU:    handle_type_menu(ev);     break;
    case STATE_ENTER_POINT:  handle_point_entry(ev);   break;
    case STATE_ENTER_VECTOR: handle_vector_entry(ev);  break;
  }
}

void input_reset_scroll(void) { list_scroll = 0; }