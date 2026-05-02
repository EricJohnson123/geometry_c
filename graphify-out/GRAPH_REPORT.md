# Graph Report - geometry_c  (2026-05-02)

## Corpus Check
- 12 files · ~12,473 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 99 nodes · 257 edges · 6 communities detected
- Extraction: 68% EXTRACTED · 32% INFERRED · 0% AMBIGUOUS · INFERRED: 83 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]

## God Nodes (most connected - your core abstractions)
1. `fill_rect()` - 17 edges
2. `draw_stats_tab()` - 14 edges
3. `draw_grid_and_axes()` - 13 edges
4. `render()` - 12 edges
5. `fill_rect_clipped()` - 12 edges
6. `main()` - 11 edges
7. `mark_dirty()` - 11 edges
8. `draw_str_clipped()` - 11 edges
9. `draw_elements()` - 11 edges
10. `handle_point_entry()` - 9 edges

## Surprising Connections (you probably didn't know these)
- `draw_stats_tab()` --calls--> `fmtnum()`  [INFERRED]
  tab_stats.c → elements.c
- `stat_line()` --calls--> `draw_str_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `stat_sep()` --calls--> `draw_hline_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `stat_header()` --calls--> `fill_rect_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `stat_header()` --calls--> `draw_str_clipped()`  [INFERRED]
  tab_stats.c → ui.c

## Communities

### Community 0 - "Community 0"
Cohesion: 0.17
Nodes (17): eadk_keyboard_key_down(), dispatch(), is_nav_event(), key_to_event(), main(), switch_tab(), auto_fit(), graph_can_enter_tabbar() (+9 more)

### Community 1 - "Community 1"
Cohesion: 0.28
Nodes (18): render(), clip_hrect(), draw_circle_clipped(), draw_header(), draw_hline(), draw_line_clipped(), draw_rect_border(), draw_str_centered() (+10 more)

### Community 2 - "Community 2"
Cohesion: 0.29
Nodes (16): draw_elements(), draw_graph_tab(), draw_grid_and_axes(), draw_subtoolbar(), gdot(), gdraw_vector(), gfill(), ghline() (+8 more)

### Community 3 - "Community 3"
Cohesion: 0.3
Nodes (13): build_cartesian(), build_slope_intercept(), draw_scrollbar(), draw_stats_tab(), facos_deg(), float_to_frac(), fmt_frac(), fsqrt() (+5 more)

### Community 4 - "Community 4"
Cohesion: 0.26
Nodes (12): buf_append(), buf_backspace(), fmtnum(), ftoa(), make_point_label(), make_vector_label(), simple_atof(), commit_vector() (+4 more)

### Community 5 - "Community 5"
Cohesion: 0.51
Nodes (9): draw_input_overlay(), draw_input_tab(), draw_point_entry(), draw_type_menu(), draw_vector_entry(), draw_hline_clipped(), draw_str_clipped(), fill_circle_clipped() (+1 more)

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `mark_dirty()` connect `Community 0` to `Community 1`, `Community 4`?**
  _High betweenness centrality (0.152) - this node is a cross-community bridge._
- **Why does `draw_stats_tab()` connect `Community 3` to `Community 1`, `Community 4`, `Community 5`?**
  _High betweenness centrality (0.126) - this node is a cross-community bridge._
- **Why does `render()` connect `Community 1` to `Community 0`, `Community 2`, `Community 3`, `Community 5`?**
  _High betweenness centrality (0.108) - this node is a cross-community bridge._
- **Are the 4 inferred relationships involving `fill_rect()` (e.g. with `draw_scrollbar()` and `draw_subtoolbar()`) actually correct?**
  _`fill_rect()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `draw_stats_tab()` (e.g. with `fill_rect_clipped()` and `draw_str_clipped()`) actually correct?**
  _`draw_stats_tab()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `draw_grid_and_axes()` (e.g. with `ftoa()` and `fill_rect_clipped()`) actually correct?**
  _`draw_grid_and_axes()` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Are the 10 inferred relationships involving `render()` (e.g. with `is_dirty()` and `draw_header()`) actually correct?**
  _`render()` has 10 INFERRED edges - model-reasoned connections that need verification._