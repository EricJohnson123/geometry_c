#ifndef UI_H
#define UI_H

#include "eadk.h"
#include <stdbool.h>
#include <stdint.h>

// ─── Screen layout ────────────────────────────────────────────────────────────
#define SCREEN_W          320
#define SCREEN_H          240
#define HEADER_H           18
#define TAB_H              22
#define TOOLBAR_H          28
#define CONTENT_Y          (HEADER_H + TAB_H)
#define CONTENT_H          (SCREEN_H - HEADER_H - TAB_H - TOOLBAR_H)
#define CONTENT_BOTTOM     (CONTENT_Y + CONTENT_H)
// Graph sub-toolbar sits at top of content area
#define GRAPH_SUBTOOL_H    18
#define GRAPH_PLOT_Y       (CONTENT_Y + GRAPH_SUBTOOL_H)
#define GRAPH_PLOT_H       (CONTENT_H - GRAPH_SUBTOOL_H)
// Font metrics
#define SMALL_FONT_W       10
#define SMALL_FONT_H       14
#define LARGE_FONT_W       16
#define LARGE_FONT_H       20

// ─── Colors (RGB565) ─────────────────────────────────────────────────────────
#define COLOR_ORANGE             ((eadk_color_t)0xFDA7)
#define COLOR_WHITE              ((eadk_color_t)0xFFFF)
#define COLOR_BLACK              ((eadk_color_t)0x0000)
// Active tab: light blue-gray; inactive: dark gray  (matches screenshots)
#define COLOR_TAB_ACTIVE         ((eadk_color_t)0xCE79)
#define COLOR_TAB_INACTIVE       ((eadk_color_t)0x5AEB)
#define COLOR_TAB_TEXT_ACTIVE    ((eadk_color_t)0x2945)
#define COLOR_TAB_TEXT_INACTIVE  ((eadk_color_t)0xCE79)
#define COLOR_CONTENT_BG         ((eadk_color_t)0xEF7D)  // very light gray
#define COLOR_ROW_SELECTED       ((eadk_color_t)0xE73C)
#define COLOR_ADD_ROW_BG         ((eadk_color_t)0xDEFB)
#define COLOR_DARK_GRAY          ((eadk_color_t)0x4A49)
#define COLOR_LIGHT_GRAY         ((eadk_color_t)0x9CF3)
#define COLOR_GRID               ((eadk_color_t)0xEF7D)
#define COLOR_AXIS               ((eadk_color_t)0x0000)
#define COLOR_AXIS_LABEL         ((eadk_color_t)0x6B4D)
#define COLOR_TOOLBAR_BG         ((eadk_color_t)0xDEFB)
#define COLOR_SUBTOOL_BG         ((eadk_color_t)0xEF7D)
#define COLOR_SUBTOOL_SELECTED   ((eadk_color_t)0xCE79)

// Element accent colors (cycling)
extern const eadk_color_t accent_colors[6];
#define NUM_ACCENT_COLORS 6

// ─── Dirty flags ──────────────────────────────────────────────────────────────
#define DIRTY_HEADER   (1u << 0)
#define DIRTY_TABS     (1u << 1)
#define DIRTY_CONTENT  (1u << 2)
#define DIRTY_TOOLBAR  (1u << 3)
#define DIRTY_OVERLAY  (1u << 4)
#define DIRTY_ALL      (0x1Fu)

extern uint8_t dirty;
void  mark_dirty(uint8_t flags);
bool  is_dirty(uint8_t flag);

// ─── Draw primitives (unclipped — for chrome) ─────────────────────────────────
void fill_rect(int x, int y, int w, int h, eadk_color_t c);
void draw_hline(int x, int y, int w, eadk_color_t c);
void draw_vline(int x, int y, int h, eadk_color_t c);
void draw_rect_border(int x, int y, int w, int h, eadk_color_t c);
void draw_str(const char* s, int x, int y, bool large, eadk_color_t fg, eadk_color_t bg);
void draw_str_centered(const char* s, int cx, int y, bool large, eadk_color_t fg, eadk_color_t bg);

// ─── Clipped draw primitives (content area only) ─────────────────────────────
// Clips to [0, SCREEN_W) × [CONTENT_Y, CONTENT_BOTTOM)
void fill_rect_clipped(int x, int y, int w, int h, eadk_color_t c);
void draw_hline_clipped(int x, int y, int w, eadk_color_t c);
void draw_vline_clipped(int x, int y, int h, eadk_color_t c);
void draw_str_clipped(const char* s, int x, int y, bool large, eadk_color_t fg, eadk_color_t bg);
void draw_circle_clipped(int cx, int cy, int r, eadk_color_t c);
void fill_circle_clipped(int cx, int cy, int r, eadk_color_t c);
void draw_line_clipped(int x0, int y0, int x1, int y1, eadk_color_t c);

// ─── Chrome ───────────────────────────────────────────────────────────────────
typedef enum { TAB_INPUT = 0, TAB_GRAPH = 1, TAB_STATS = 2 } Tab;
extern Tab current_tab;

// Global focus: are we navigating the tab bar, or inside content?
typedef enum { FOCUS_CONTENT = 0, FOCUS_TABBAR } AppFocus;
extern AppFocus app_focus;
extern int      tab_hover;   // which tab is highlighted when FOCUS_TABBAR

void draw_header(void);
void draw_tabs(void);        // uses current_tab, app_focus, tab_hover
void draw_toolbar_input(void);
void draw_toolbar_graph(float cx, float cy, bool cursor_valid);
void draw_toolbar_stats(void);

#endif