# Graph Report - geometry_c  (2026-04-30)

## Corpus Check
- 12 files · ~8,519 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 81 nodes · 196 edges · 6 communities detected
- Extraction: 68% EXTRACTED · 32% INFERRED · 0% AMBIGUOUS · INFERRED: 63 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]

## God Nodes (most connected - your core abstractions)
1. `fill_rect()` - 15 edges
2. `draw_stats_tab()` - 13 edges
3. `render()` - 12 edges
4. `fill_rect_clipped()` - 11 edges
5. `draw_str_clipped()` - 10 edges
6. `mark_dirty()` - 9 edges
7. `draw_grid_and_axes()` - 9 edges
8. `main()` - 8 edges
9. `draw_input_tab()` - 8 edges
10. `handle_point_entry()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `draw_stats_tab()` --calls--> `fmtnum()`  [INFERRED]
  tab_stats.c → elements.c
- `switch_tab()` --calls--> `stats_reset_scroll()`  [INFERRED]
  main.c → tab_stats.c
- `stat_sep()` --calls--> `draw_hline_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `stat_header()` --calls--> `fill_rect_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `draw_scrollbar()` --calls--> `fill_rect()`  [INFERRED]
  tab_stats.c → ui.c

## Communities

### Community 0 - "Community 0"
Cohesion: 0.26
Nodes (19): render(), clip_hrect(), draw_circle_clipped(), draw_header(), draw_hline(), draw_line_clipped(), draw_rect_border(), draw_str() (+11 more)

### Community 1 - "Community 1"
Cohesion: 0.25
Nodes (15): build_cartesian(), build_slope_intercept(), draw_scrollbar(), draw_stats_tab(), facos_deg(), float_to_frac(), fmt_frac(), fsqrt() (+7 more)

### Community 2 - "Community 2"
Cohesion: 0.24
Nodes (11): buf_append(), buf_backspace(), fmtnum(), ftoa(), make_label(), simple_atof(), event_to_decimal(), handle_input_event() (+3 more)

### Community 3 - "Community 3"
Cohesion: 0.33
Nodes (9): handle_tabbar(), main(), switch_tab(), auto_fit(), graph_can_enter_tabbar(), graph_enter(), handle_graph_event(), handle_stats_event() (+1 more)

### Community 4 - "Community 4"
Cohesion: 0.52
Nodes (7): draw_input_overlay(), draw_input_tab(), draw_point_entry(), draw_type_menu(), draw_hline_clipped(), fill_circle_clipped(), fill_rect_clipped()

### Community 5 - "Community 5"
Cohesion: 0.62
Nodes (6): draw_elements(), draw_graph_tab(), draw_grid_and_axes(), draw_subtoolbar(), world_to_screen_x(), world_to_screen_y()

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `draw_stats_tab()` connect `Community 1` to `Community 0`, `Community 2`, `Community 4`?**
  _High betweenness centrality (0.144) - this node is a cross-community bridge._
- **Why does `mark_dirty()` connect `Community 3` to `Community 0`, `Community 2`?**
  _High betweenness centrality (0.140) - this node is a cross-community bridge._
- **Why does `render()` connect `Community 0` to `Community 1`, `Community 3`, `Community 4`, `Community 5`?**
  _High betweenness centrality (0.108) - this node is a cross-community bridge._
- **Are the 2 inferred relationships involving `fill_rect()` (e.g. with `draw_scrollbar()` and `draw_subtoolbar()`) actually correct?**
  _`fill_rect()` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `draw_stats_tab()` (e.g. with `fill_rect_clipped()` and `draw_str_clipped()`) actually correct?**
  _`draw_stats_tab()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **Are the 10 inferred relationships involving `render()` (e.g. with `is_dirty()` and `draw_header()`) actually correct?**
  _`render()` has 10 INFERRED edges - model-reasoned connections that need verification._
- **Are the 6 inferred relationships involving `fill_rect_clipped()` (e.g. with `stat_header()` and `draw_stats_tab()`) actually correct?**
  _`fill_rect_clipped()` has 6 INFERRED edges - model-reasoned connections that need verification._