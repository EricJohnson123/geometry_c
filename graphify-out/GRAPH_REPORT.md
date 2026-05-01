# Graph Report - geometry_c  (2026-04-30)

## Corpus Check
- 12 files · ~9,213 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 90 nodes · 225 edges · 6 communities detected
- Extraction: 68% EXTRACTED · 32% INFERRED · 0% AMBIGUOUS · INFERRED: 72 edges (avg confidence: 0.8)
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
2. `draw_stats_tab()` - 13 edges
3. `draw_grid_and_axes()` - 13 edges
4. `render()` - 12 edges
5. `main()` - 11 edges
6. `fill_rect_clipped()` - 11 edges
7. `draw_str_clipped()` - 10 edges
8. `draw_elements()` - 10 edges
9. `mark_dirty()` - 9 edges
10. `dispatch()` - 8 edges

## Surprising Connections (you probably didn't know these)
- `draw_stats_tab()` --calls--> `fmtnum()`  [INFERRED]
  tab_stats.c → elements.c
- `draw_stats_tab()` --calls--> `render()`  [INFERRED]
  tab_stats.c → main.c
- `stats_reset_scroll()` --calls--> `switch_tab()`  [INFERRED]
  tab_stats.c → main.c
- `render()` --calls--> `is_dirty()`  [INFERRED]
  main.c → ui.c
- `render()` --calls--> `draw_header()`  [INFERRED]
  main.c → ui.c

## Communities

### Community 0 - "Community 0"
Cohesion: 0.19
Nodes (15): eadk_keyboard_key_down(), dispatch(), is_nav_event(), key_to_event(), main(), render(), switch_tab(), auto_fit() (+7 more)

### Community 1 - "Community 1"
Cohesion: 0.25
Nodes (17): ftoa(), draw_scrollbar(), clip_hrect(), draw_circle_clipped(), draw_header(), draw_hline(), draw_line_clipped(), draw_str_centered() (+9 more)

### Community 2 - "Community 2"
Cohesion: 0.34
Nodes (13): draw_elements(), draw_graph_tab(), draw_grid_and_axes(), draw_subtoolbar(), gdot(), gfill(), ghline(), gring() (+5 more)

### Community 3 - "Community 3"
Cohesion: 0.32
Nodes (13): draw_input_overlay(), draw_input_tab(), draw_point_entry(), draw_type_menu(), draw_stats_tab(), stat_header(), stat_line(), stat_sep() (+5 more)

### Community 4 - "Community 4"
Cohesion: 0.27
Nodes (10): buf_append(), buf_backspace(), fmtnum(), make_label(), simple_atof(), event_to_decimal(), handle_input_event(), handle_list() (+2 more)

### Community 5 - "Community 5"
Cohesion: 0.38
Nodes (8): build_cartesian(), build_slope_intercept(), facos_deg(), float_to_frac(), fmt_frac(), fsqrt(), gcd(), point_dist()

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `draw_stats_tab()` connect `Community 3` to `Community 0`, `Community 1`, `Community 4`, `Community 5`?**
  _High betweenness centrality (0.135) - this node is a cross-community bridge._
- **Why does `mark_dirty()` connect `Community 0` to `Community 1`, `Community 4`?**
  _High betweenness centrality (0.128) - this node is a cross-community bridge._
- **Why does `render()` connect `Community 0` to `Community 1`, `Community 2`, `Community 3`?**
  _High betweenness centrality (0.124) - this node is a cross-community bridge._
- **Are the 4 inferred relationships involving `fill_rect()` (e.g. with `draw_scrollbar()` and `draw_subtoolbar()`) actually correct?**
  _`fill_rect()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **Are the 4 inferred relationships involving `draw_stats_tab()` (e.g. with `fill_rect_clipped()` and `draw_str_clipped()`) actually correct?**
  _`draw_stats_tab()` has 4 INFERRED edges - model-reasoned connections that need verification._
- **Are the 5 inferred relationships involving `draw_grid_and_axes()` (e.g. with `ftoa()` and `fill_rect_clipped()`) actually correct?**
  _`draw_grid_and_axes()` has 5 INFERRED edges - model-reasoned connections that need verification._
- **Are the 10 inferred relationships involving `render()` (e.g. with `is_dirty()` and `draw_header()`) actually correct?**
  _`render()` has 10 INFERRED edges - model-reasoned connections that need verification._