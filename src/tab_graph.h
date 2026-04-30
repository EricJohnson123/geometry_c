#ifndef TAB_GRAPH_H
#define TAB_GRAPH_H

#include "eadk.h"
#include <stdbool.h>

// Three graph nav states:
//   IDLE     — just entered graph, arrows do nothing
//   SUBTOOL  — cursor is in the subtoolbar row
//   PAN      — Navigate mode: arrows pan, +/- zoom, Back exits
typedef enum {
  GRAPH_IDLE = 0,
  GRAPH_SUBTOOL,
  GRAPH_PAN,
} GraphNavState;

extern GraphNavState graph_nav;

// View range
extern float graph_x_min, graph_x_max;
extern float graph_y_min, graph_y_max;

// Cursor (world coords)
extern float graph_cursor_x, graph_cursor_y;
extern bool  graph_cursor_valid;

void draw_graph_tab(void);
void handle_graph_event(eadk_event_t ev);

// Called by main.c when entering graph tab
void graph_enter(void);

// Returns true when up-arrow from graph content should enter the tab bar
bool graph_can_enter_tabbar(void);

// Coordinate conversion
int world_to_screen_x(float wx);
int world_to_screen_y(float wy);

#endif