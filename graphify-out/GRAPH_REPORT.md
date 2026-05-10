# Graph Report - geometry_c  (2026-05-10)

## Corpus Check
- 16 files · ~22,367 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 167 nodes · 505 edges · 8 communities detected
- Extraction: 65% EXTRACTED · 35% INFERRED · 0% AMBIGUOUS · INFERRED: 179 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 7|Community 7]]

## God Nodes (most connected - your core abstractions)
1. `mark_dirty()` - 28 edges
2. `draw_stats_tab()` - 23 edges
3. `fill_rect_clipped()` - 20 edges
4. `draw_str_clipped()` - 20 edges
5. `fill_rect()` - 19 edges
6. `draw_rect_border()` - 15 edges
7. `draw_hline_clipped()` - 15 edges
8. `handle_input_event()` - 13 edges
9. `draw_grid_and_axes()` - 13 edges
10. `draw_elements()` - 13 edges

## Surprising Connections (you probably didn't know these)
- `render()` --calls--> `is_dirty()`  [INFERRED]
  main.c → ui.c
- `stat_line_plain()` --calls--> `draw_str_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `stat_line_explain()` --calls--> `draw_str_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `stat_sep()` --calls--> `draw_hline_clipped()`  [INFERRED]
  tab_stats.c → ui.c
- `draw_explanation_overlay()` --calls--> `fill_rect()`  [INFERRED]
  tab_stats.c → ui.c

## Communities

### Community 0 - "Community 0"
Cohesion: 0.21
Nodes (31): buf_append(), buf_backspace(), count_type(), make_label(), make_line_label(), make_point_label(), make_vector_label(), simple_atof() (+23 more)

### Community 1 - "Community 1"
Cohesion: 0.19
Nodes (24): fmtnum(), build_cartesian_from_abc(), build_line_through_point_dir(), build_slope_intercept(), commit_addable(), draw_explanation_overlay(), draw_stats_tab(), explain_title() (+16 more)

### Community 2 - "Community 2"
Cohesion: 0.21
Nodes (20): render_math_step(), draw_elements(), draw_graph_tab(), draw_grid_and_axes(), draw_subtoolbar(), gdot(), gdraw_line(), gdraw_line_thick() (+12 more)

### Community 3 - "Community 3"
Cohesion: 0.29
Nodes (19): add_step(), efmt(), fmt_cart_eq(), fmt_coeff(), fmt_frac(), fmt_si_eq(), fsqrt(), gcd_int() (+11 more)

### Community 4 - "Community 4"
Cohesion: 0.23
Nodes (18): ftoa(), draw_scrollbar(), clip_hrect(), draw_circle_clipped(), draw_header(), draw_hline(), draw_line_clipped(), draw_str_centered() (+10 more)

### Community 5 - "Community 5"
Cohesion: 0.17
Nodes (15): eadk_keyboard_key_down(), dispatch(), is_nav_event(), key_to_event(), main(), render(), switch_tab(), auto_fit() (+7 more)

### Community 6 - "Community 6"
Cohesion: 0.39
Nodes (18): draw_dot_menu(), draw_edit_point(), draw_edit_vector(), draw_input_overlay(), draw_input_tab(), draw_line_cart_entry(), draw_line_si_entry(), draw_line_type_picker() (+10 more)

### Community 7 - "Community 7"
Cohesion: 0.83
Nodes (3): draw_math_expr(), math_expr_width(), utf8_seq_len()

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `mark_dirty()` connect `Community 0` to `Community 1`, `Community 4`, `Community 5`?**
  _High betweenness centrality (0.222) - this node is a cross-community bridge._
- **Why does `draw_stats_tab()` connect `Community 1` to `Community 0`, `Community 4`, `Community 5`, `Community 6`?**
  _High betweenness centrality (0.138) - this node is a cross-community bridge._
- **Why does `generate_explanation_steps()` connect `Community 3` to `Community 1`, `Community 5`?**
  _High betweenness centrality (0.109) - this node is a cross-community bridge._
- **Are the 27 inferred relationships involving `mark_dirty()` (e.g. with `commit_addable()` and `draw_stats_tab()`) actually correct?**
  _`mark_dirty()` has 27 INFERRED edges - model-reasoned connections that need verification._
- **Are the 6 inferred relationships involving `draw_stats_tab()` (e.g. with `fill_rect()` and `draw_str_clipped()`) actually correct?**
  _`draw_stats_tab()` has 6 INFERRED edges - model-reasoned connections that need verification._
- **Are the 15 inferred relationships involving `fill_rect_clipped()` (e.g. with `stat_header()` and `draw_input_tab()`) actually correct?**
  _`fill_rect_clipped()` has 15 INFERRED edges - model-reasoned connections that need verification._
- **Are the 18 inferred relationships involving `draw_str_clipped()` (e.g. with `stat_line_plain()` and `stat_line_explain()`) actually correct?**
  _`draw_str_clipped()` has 18 INFERRED edges - model-reasoned connections that need verification._