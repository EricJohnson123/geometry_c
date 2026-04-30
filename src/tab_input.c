#include "tab_input.h"
#include "ui.h"
#include "elements.h"
#include <string.h>
#include <stdio.h>

// ─── State ────────────────────────────────────────────────────────────────────
InputState input_state = STATE_LIST;
int        selected_row = 0;

static int   type_menu_sel = 0;
static char  input_x_buf[16];
static char  input_y_buf[16];
static int   input_field = 0;  // 0 = x, 1 = y

#define ROW_H       36
#define ACCENT_W     4
#define CIRCLE_X    16
#define TEXT_X      30

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

// ─── Draw: element list ───────────────────────────────────────────────────────
void draw_input_tab(void) {
  // Background
  fill_rect_clipped(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_WHITE);

  int max_rows = CONTENT_H / ROW_H;

  for (int i = 0; i < elem_count && i < max_rows; i++) {
    Element* e = &elements[i];
    int ry = CONTENT_Y + i * ROW_H;
    bool sel = (input_state == STATE_LIST && selected_row == i);
    eadk_color_t row_bg = sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
    eadk_color_t ac = accent_colors[i % NUM_ACCENT_COLORS];

    // Row background
    fill_rect_clipped(0, ry, SCREEN_W, ROW_H, row_bg);

    // Accent bar
    fill_rect_clipped(0, ry, ACCENT_W, ROW_H, ac);

    // Toggle circle
    if (e->active)
      fill_circle_clipped(CIRCLE_X, ry + ROW_H / 2, 5, ac);
    else
      draw_circle_clipped(CIRCLE_X, ry + ROW_H / 2, 5, COLOR_DARK_GRAY);

    // Main label "(x, y)"
    char main_label[32];
    char sx[12], sy[12];
    ftoa(e->x, sx, 2);
    ftoa(e->y, sy, 2);
    snprintf(main_label, sizeof(main_label), "(%s, %s)", sx, sy);
    draw_str_clipped(main_label, TEXT_X, ry + 4, true, COLOR_BLACK, row_bg);

    // Sub label "Point A"
    char sub_label[12];
    snprintf(sub_label, sizeof(sub_label), "Point %s", e->label);
    draw_str_clipped(sub_label, TEXT_X, ry + 22, false, COLOR_DARK_GRAY, row_bg);

    // "..." menu hint
    draw_str_clipped("...", SCREEN_W - 34, ry + 11, false, COLOR_DARK_GRAY, row_bg);

    // Row separator
    draw_hline_clipped(0, ry + ROW_H - 1, SCREEN_W, COLOR_LIGHT_GRAY);
  }

  // "Add an element" row
  int add_idx = elem_count;
  int ary = CONTENT_Y + add_idx * ROW_H;
  if (ary + ROW_H <= CONTENT_BOTTOM) {
    bool sel = (input_state == STATE_LIST && selected_row == add_idx);
    eadk_color_t bg = sel ? COLOR_ROW_SELECTED : COLOR_ADD_ROW_BG;
    fill_rect_clipped(0, ary, SCREEN_W, ROW_H, bg);
    draw_str_clipped("Add an element", 14, ary + (ROW_H - SMALL_FONT_H) / 2,
                     false, COLOR_BLACK, bg);
    draw_hline_clipped(0, ary + ROW_H - 1, SCREEN_W, COLOR_LIGHT_GRAY);
  }
}

// ─── Draw: type selection popup ───────────────────────────────────────────────
static const char* type_names[] = {"Point"};
#define NUM_TYPES 1

static void draw_type_menu(void) {
  int mx = 20, my = CONTENT_Y + 8;
  int mw = 200, item_h = 28;
  int mh = 20 + NUM_TYPES * item_h;

  fill_rect_clipped(mx, my, mw, mh, COLOR_WHITE);
  draw_rect_border(mx, my, mw, mh, COLOR_DARK_GRAY);

  // Header
  fill_rect_clipped(mx + 1, my + 1, mw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("Add element", mx + 6, my + 2, false,
                   COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(mx, my + 17, mw, COLOR_DARK_GRAY);

  for (int i = 0; i < NUM_TYPES; i++) {
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

  // Title bar
  fill_rect_clipped(fx + 1, fy + 1, fw - 2, 16, COLOR_TAB_INACTIVE);
  draw_str_clipped("New Point", fx + 6, fy + 2, false,
                   COLOR_WHITE, COLOR_TAB_INACTIVE);
  draw_hline_clipped(fx, fy + 17, fw, COLOR_DARK_GRAY);

  // x field
  bool x_sel = (input_field == 0);
  eadk_color_t x_bg = x_sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  eadk_color_t x_border = x_sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 22, fw - 16, 24, x_bg);
  draw_rect_border(fx + 8, fy + 22, fw - 16, 24, x_border);
  draw_str_clipped("x =", fx + 12, fy + 27, false, COLOR_DARK_GRAY, x_bg);
  char xbuf[18];
  snprintf(xbuf, sizeof(xbuf), "%s%s", input_x_buf, x_sel ? "|" : "");
  draw_str_clipped(xbuf, fx + 48, fy + 27, false, COLOR_BLACK, x_bg);

  // y field
  bool y_sel = (input_field == 1);
  eadk_color_t y_bg = y_sel ? COLOR_ROW_SELECTED : COLOR_WHITE;
  eadk_color_t y_border = y_sel ? COLOR_ORANGE : COLOR_DARK_GRAY;
  fill_rect_clipped(fx + 8, fy + 52, fw - 16, 24, y_bg);
  draw_rect_border(fx + 8, fy + 52, fw - 16, 24, y_border);
  draw_str_clipped("y =", fx + 12, fy + 57, false, COLOR_DARK_GRAY, y_bg);
  char ybuf[18];
  snprintf(ybuf, sizeof(ybuf), "%s%s", input_y_buf, y_sel ? "|" : "");
  draw_str_clipped(ybuf, fx + 48, fy + 57, false, COLOR_BLACK, y_bg);

  // Hint
  draw_str_clipped("EXE: next  Back: cancel", fx + 8, fy + 82,
                   false, COLOR_DARK_GRAY, COLOR_WHITE);
}

// ─── Combined draw entry point ────────────────────────────────────────────────
void draw_input_overlay(void) {
  if (input_state == STATE_TYPE_MENU)   draw_type_menu();
  if (input_state == STATE_ENTER_POINT) draw_point_entry();
}

// ─── Event handlers ───────────────────────────────────────────────────────────
static void handle_list(eadk_event_t ev) {
  int total = elem_count + 1;
  switch (ev) {
    case eadk_event_down:
      if (selected_row < total - 1) { selected_row++; mark_dirty(DIRTY_CONTENT); }
      break;
    case eadk_event_up:
      if (selected_row > 0)         { selected_row--; mark_dirty(DIRTY_CONTENT); }
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
    case eadk_event_ok:
    case eadk_event_exe:
      input_x_buf[0] = '\0';
      input_y_buf[0] = '\0';
      input_field = 0;
      input_state = STATE_ENTER_POINT;
      mark_dirty(DIRTY_OVERLAY);
      break;
    default: break;
  }
}

static void handle_point_entry(eadk_event_t ev) {
  char* cur = (input_field == 0) ? input_x_buf : input_y_buf;

  if (ev == eadk_event_back) {
    if (strlen(cur) > 0)    { buf_backspace(cur); mark_dirty(DIRTY_OVERLAY); }
    else if (input_field == 1) { input_field = 0; mark_dirty(DIRTY_OVERLAY); }
    else                    { input_state = STATE_TYPE_MENU; mark_dirty(DIRTY_OVERLAY); }
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
      // Commit point
      if (elem_count < MAX_ELEMENTS) {
        Element* e = &elements[elem_count];
        e->type   = ELEM_POINT;
        e->active = true;
        e->x      = simple_atof(input_x_buf);
        e->y      = simple_atof(input_y_buf);
        make_label(elem_count, e->label);
        elem_count++;
        selected_row = elem_count - 1;
      }
      input_state = STATE_LIST;
      mark_dirty(DIRTY_CONTENT);
    }
    return;
  }
  char c = event_to_decimal(ev);
  if (c) { buf_append(cur, c, 15); mark_dirty(DIRTY_OVERLAY); }
}

void handle_input_event(eadk_event_t ev) {
  switch (input_state) {
    case STATE_LIST:        handle_list(ev);          break;
    case STATE_TYPE_MENU:   handle_type_menu(ev);     break;
    case STATE_ENTER_POINT: handle_point_entry(ev);   break;
  }
}
