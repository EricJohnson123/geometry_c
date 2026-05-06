# Graph Report - geometry_c  (2026-05-06)

## Corpus Check
- 12 files · ~14,385 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 124 nodes · 361 edges · 6 communities detected
- Extraction: 64% EXTRACTED · 36% INFERRED · 0% AMBIGUOUS · INFERRED: 131 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]

## God Nodes (most connected - your core abstractions)
1. `draw_stats_tab()` - 18 edges
2. `mark_dirty()` - 18 edges
3. `fill_rect()` - 17 edges
4. `fill_rect_clipped()` - 17 edges
5. `draw_str_clipped()` - 16 edges
6. `draw_grid_and_axes()` - 13 edges
7. `draw_elements()` - 13 edges
8. `render()` - 12 edges
9. `draw_rect_border()` - 12 edges
10. `draw_hline_clipped()` - 12 edges

## Surprising Connections (you probably didn't know these)
- `stat_line_plain()` --calls--> `draw_str_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `stat_line_add()` --calls--> `draw_str_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `stat_line_add()` --calls--> `fill_rect_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `stat_line_add()` --calls--> `draw_rect_border()`  [INFERRED]
  tab_stats.c → ui.c
- `stat_sep()` --calls--> `draw_hline_clipped()`  [INFERRED]
  tab_stats.c → ui.c

## Communities

### Community 0 - "Community 0"
Cohesion: 0.24
Nodes (23): buf_append(), buf_backspace(), count_type(), make_label(), make_line_label(), make_point_label(), make_vector_label(), simple_atof() (+15 more)

### Community 1 - "Community 1"
Cohesion: 0.2
Nodes (23): fmtnum(), build_cartesian_from_abc(), build_line_through_point_dir(), build_slope_intercept(), commit_addable(), draw_scrollbar(), draw_stats_tab(), facos_deg() (+15 more)

### Community 2 - "Community 2"
Cohesion: 0.26
Nodes (19): render(), draw_subtoolbar(), clip_hrect(), draw_circle_clipped(), draw_header(), draw_hline(), draw_line_clipped(), draw_str() (+11 more)

### Community 3 - "Community 3"
Cohesion: 0.25
Nodes (17): ftoa(), draw_elements(), draw_graph_tab(), draw_grid_and_axes(), gdot(), gdraw_line(), gdraw_line_thick(), gdraw_vector() (+9 more)

### Community 4 - "Community 4"
Cohesion: 0.18
Nodes (14): eadk_keyboard_key_down(), dispatch(), is_nav_event(), key_to_event(), main(), switch_tab(), auto_fit(), graph_can_enter_tabbar() (+6 more)

### Community 5 - "Community 5"
Cohesion: 0.45
Nodes (14): draw_dot_menu(), draw_input_overlay(), draw_input_tab(), draw_line_cart_entry(), draw_line_si_entry(), draw_line_type_picker(), draw_point_entry(), draw_type_menu() (+6 more)

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `mark_dirty()` connect `Community 0` to `Community 1`, `Community 2`, `Community 4`?**
  _High betweenness centrality (0.194) - this node is a cross-community bridge._
- **Why does `draw_stats_tab()` connect `Community 1` to `Community 2`, `Community 5`?**
  _High betweenness centrality (0.103) - this node is a cross-community bridge._
- **Why does `draw_str_clipped()` connect `Community 5` to `Community 1`, `Community 2`, `Community 3`?**
  _High betweenness centrality (0.085) - this node is a cross-community bridge._
- **Are the 4 inferred relationships involving `draw_stats_tab()` (e.g. with `fill_rect_clipped()` and `draw_str_clipped()`) actually correct?**
  _`draw_stats_tab()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **Are the 17 inferred relationships involving `mark_dirty()` (e.g. with `commit_addable()` and `handle_stats_event()`) actually correct?**
  _`mark_dirty()` has 17 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `fill_rect()` (e.g. with `draw_scrollbar()` and `draw_subtoolbar()`) actually correct?**
  _`fill_rect()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **Are the 12 inferred relationships involving `fill_rect_clipped()` (e.g. with `stat_line_add()` and `stat_header()`) actually correct?**
  _`fill_rect_clipped()` has 12 INFERRED edges - model-reasoned connections that need verification._