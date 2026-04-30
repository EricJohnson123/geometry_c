#ifndef TAB_STATS_H
#define TAB_STATS_H

#include "eadk.h"

void draw_stats_tab(void);
void handle_stats_event(eadk_event_t ev);
void stats_reset_scroll(void);  // call when entering the stats tab

#endif