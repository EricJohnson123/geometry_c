#ifndef TAB_INPUT_H
#define TAB_INPUT_H

#include "eadk.h"

typedef enum {
  STATE_LIST = 0,
  STATE_TYPE_MENU,
  STATE_ENTER_POINT,
} InputState;

extern InputState input_state;
extern int        selected_row;

void draw_input_tab(void);
void draw_input_overlay(void);
void handle_input_event(eadk_event_t ev);

#endif
