#include "eadk.h"
#include "ui.h"
#include "elements.h"
#include "tab_input.h"
#include "tab_graph.h"
#include "tab_stats.h"
#include <stdbool.h>
#include <stdint.h>

// ─── App metadata ─────────────────────────────────────────────────────────────
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Geometry";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

// ─── Key-repeat parameters ────────────────────────────────────────────────────
// Only navigation keys get repeat; typing keys (digits, OK, Back…) fire once.
#define REPEAT_DELAY_MS   350u   // hold time before repeat starts
#define REPEAT_RATE_MS     80u   // interval between repeated firings
#define POLL_MS            16u   // main loop poll interval (~60 fps)

// Keys that participate in repeat
static bool is_repeatable(eadk_key_t k) {
  return k == eadk_key_left  || k == eadk_key_right ||
         k == eadk_key_up    || k == eadk_key_down  ||
         k == eadk_key_plus  || k == eadk_key_minus;
}

// Map a key to its event equivalent (for the repeatable subset)
static eadk_event_t key_to_event(eadk_key_t k) {
  switch (k) {
    case eadk_key_left:  return eadk_event_left;
    case eadk_key_right: return eadk_event_right;
    case eadk_key_up:    return eadk_event_up;
    case eadk_key_down:  return eadk_event_down;
    case eadk_key_plus:  return eadk_event_plus;
    case eadk_key_minus: return eadk_event_minus;
    default:             return (eadk_event_t)0xFFFF;
  }
}

// ─── Repeatable key list ──────────────────────────────────────────────────────
static const eadk_key_t repeat_keys[] = {
  eadk_key_left, eadk_key_right,
  eadk_key_up,   eadk_key_down,
  eadk_key_plus, eadk_key_minus
};
#define NUM_REPEAT_KEYS 6

// Per-key repeat state
typedef struct {
  bool     held;
  uint64_t press_time;    // millis when key first went down
  uint64_t last_fire;     // millis of last action fired
} KeyRepeat;

static KeyRepeat kr[NUM_REPEAT_KEYS];

// ─── Snapshot of previous frame ───────────────────────────────────────────────
static Tab        prev_tab         = TAB_INPUT;
static InputState prev_input_state = STATE_LIST;

// ─── Render ───────────────────────────────────────────────────────────────────
static void render(void) {
  bool tab_changed, overlay_closed, overlay_was_open, overlay_is_open;
  if (!dirty) return;

  tab_changed    = (prev_tab != current_tab);
  overlay_closed = (prev_input_state != STATE_LIST && input_state == STATE_LIST);

  if (tab_changed || overlay_closed)
    dirty |= DIRTY_TABS | DIRTY_CONTENT | DIRTY_TOOLBAR;

  overlay_was_open = (prev_input_state != STATE_LIST);
  overlay_is_open  = (input_state      != STATE_LIST);
  if (overlay_was_open && overlay_is_open && prev_input_state != input_state)
    dirty |= DIRTY_CONTENT;

  if (is_dirty(DIRTY_HEADER)) draw_header();
  if (is_dirty(DIRTY_TABS))   draw_tabs();

  if (is_dirty(DIRTY_CONTENT)) {
    switch (current_tab) {
      case TAB_INPUT: draw_input_tab();  break;
      case TAB_GRAPH: draw_graph_tab();  break;
      case TAB_STATS: draw_stats_tab();  break;
    }
  }

  if (is_dirty(DIRTY_OVERLAY) && current_tab == TAB_INPUT)
    draw_input_overlay();

  prev_tab         = current_tab;
  prev_input_state = input_state;
  dirty = 0;
}

// ─── Switch to a tab ──────────────────────────────────────────────────────────
static void switch_tab(Tab t) {
  current_tab = t;
  if (t == TAB_INPUT) input_reset_scroll();
  if (t == TAB_GRAPH) graph_enter();
  if (t == TAB_STATS) stats_reset_scroll();
  mark_dirty(DIRTY_TABS | DIRTY_CONTENT | DIRTY_TOOLBAR);
}

// ─── Dispatch one logical event ───────────────────────────────────────────────
static void dispatch(eadk_event_t ev) {
  bool can_enter_tabbar;

  if (app_focus == FOCUS_TABBAR) {
    switch (ev) {
      case eadk_event_left:
        if (tab_hover > 0) { tab_hover--; mark_dirty(DIRTY_TABS); }
        break;
      case eadk_event_right:
        if (tab_hover < 2) { tab_hover++; mark_dirty(DIRTY_TABS); }
        break;
      case eadk_event_ok:
      case eadk_event_exe:
        app_focus = FOCUS_CONTENT;
        switch_tab((Tab)tab_hover);
        break;
      case eadk_event_down:
      case eadk_event_back:
        tab_hover = (int)current_tab;
        app_focus = FOCUS_CONTENT;
        mark_dirty(DIRTY_TABS);
        break;
      default: break;
    }
    return;
  }

  // Check if up-arrow should lift into the tab bar
  can_enter_tabbar = false;
  if (ev == eadk_event_up) {
    if (current_tab == TAB_INPUT && input_state == STATE_LIST
        && selected_row == 0)
      can_enter_tabbar = true;
    if (current_tab == TAB_GRAPH && graph_can_enter_tabbar())
      can_enter_tabbar = true;
    // Stats: up scrolls, not tab bar
  }

  if (can_enter_tabbar) {
    tab_hover = (int)current_tab;
    app_focus = FOCUS_TABBAR;
    mark_dirty(DIRTY_TABS);
    return;
  }

  // Normal content dispatch
  switch (current_tab) {
    case TAB_INPUT: handle_input_event(ev);  break;
    case TAB_GRAPH: handle_graph_event(ev);  break;
    case TAB_STATS: handle_stats_event(ev);  break;
  }
}

// ─── Non-repeatable key detection (edge-triggered via event queue) ────────────
// We still use eadk_event_get with timeout=0 for non-navigation keys so that
// alpha/digit input, OK, Back, EXE etc. are handled cleanly without polling.

// Returns true if the event is a navigation key handled by the repeat engine.
static bool is_nav_event(eadk_event_t ev) {
  return ev == eadk_event_left  || ev == eadk_event_right ||
         ev == eadk_event_up    || ev == eadk_event_down  ||
         ev == eadk_event_plus  || ev == eadk_event_minus;
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main(void) {
  int i;
  uint64_t now;
  eadk_keyboard_state_t ks;
  eadk_event_t ev;
  int32_t zero = 0;

  eadk_display_push_rect_uniform(eadk_screen_rect, COLOR_WHITE);
  dirty = DIRTY_ALL;
  render();

  // Initialise repeat state
  for (i = 0; i < NUM_REPEAT_KEYS; i++) {
    kr[i].held       = false;
    kr[i].press_time = 0;
    kr[i].last_fire  = 0;
  }

  while (1) {
    now = eadk_timing_millis();
    ks  = eadk_keyboard_scan();

    // ── 1. Drain the event queue for non-navigation events ────────────────
    // timeout=0 means return immediately if nothing queued.
    ev = eadk_event_get(&zero);
    if (ev != 0xFFFF && !is_nav_event(ev)) {
      dispatch(ev);
    }

    // ── 2. Key-repeat engine for navigation keys ──────────────────────────
    for (i = 0; i < NUM_REPEAT_KEYS; i++) {
      bool down = eadk_keyboard_key_down(ks, repeat_keys[i]);

      if (down && !kr[i].held) {
        // Fresh press: fire immediately
        kr[i].held       = true;
        kr[i].press_time = now;
        kr[i].last_fire  = now;
        dispatch(key_to_event(repeat_keys[i]));

      } else if (down && kr[i].held) {
        // Key held: fire again after initial delay, then at repeat rate
        uint64_t held_ms = now - kr[i].press_time;
        uint64_t since_last = now - kr[i].last_fire;
        if (held_ms >= REPEAT_DELAY_MS && since_last >= REPEAT_RATE_MS) {
          kr[i].last_fire = now;
          dispatch(key_to_event(repeat_keys[i]));
        }

      } else if (!down && kr[i].held) {
        // Key released
        kr[i].held = false;
      }
    }

    // ── 3. Render whatever changed, then sleep until next poll ────────────
    render();
    eadk_timing_msleep(POLL_MS);
  }

  return 0;
}