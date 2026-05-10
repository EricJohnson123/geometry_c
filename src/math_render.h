#ifndef MATH_RENDER_H
#define MATH_RENDER_H

#include <stdbool.h>
#include "eadk.h"

/* ── Font dimensions ───────────────────────────────────────────────────── */
/* Do NOT redefine here — ui.h is the authoritative source and is always
 * included first.  The actual NumWorks values are:
 *   SMALL_FONT_W=10  SMALL_FONT_H=14
 *   LARGE_FONT_W=16  LARGE_FONT_H=20                                      */

/* ── Math expression line layout ───────────────────────────────────────── */
/*
 *  y_line + 0  ── superscript top   (small font, 14px tall)
 *  y_line + 6  ── main baseline top (large font, 18px tall)
 *  y_line + 16 ── subscript top     (small font, 14px tall)
 *  y_line + 24 ── main baseline bottom
 *  y_line + 30 ── subscript bottom
 */
#define MATH_SUP_OFFSET   6
#define MATH_SUB_OFFSET  16
#define MATH_LINE_GAP        4   /* gap after a plain-text step */
#define MATH_LINE_GAP_2D    10   /* gap after/before a step that contains [F:] or [R:] */

/*
 * MATH_LINE_H is kept for the common "plain text only" case where every
 * step fits in one font-line height.  Steps that contain [F:] or [R:]
 * nodes may be taller — use math_expr_height() to find the true height of
 * a step, and math_step_total_height() to sum a whole array.
 */
#define MATH_LINE_H      34   /* sup(6) + large_font(18) + sub_extra(6) + gap(4) */

/* Internal segment buffer */
#define MATH_SEG_BUF    128

/* ── Markup notation ───────────────────────────────────────────────────── */
/*
 *  ^x        next single char is superscript (small font, shifted up)
 *  _x        next single char is subscript   (small font, shifted down)
 *  [F:n|d]   fraction — n is the numerator text, d the denominator text.
 *            '|' separates them; ']' closes the whole token.
 *            n and d may contain ^/_ markup and plain ASCII only.
 *  [R:body]  square root — draws the √ glyph then an overbar across body,
 *            then renders body.  body may itself contain [F:n|d].
 *
 *  '|' is reserved as fraction separator inside [F:n|d] — do not use it
 *  elsewhere inside a bracket expression.
 *  ']' closes a bracket expression — do not use it literally inside one.
 *
 *  All other chars: large font at the main baseline.
 *  UTF-8 multi-byte sequences (√ ≈ →) are treated as a single large-font char.
 */

/* ── API ───────────────────────────────────────────────────────────────── */

/*
 * Pixel width of `expr` as it would be rendered.
 * Used to centre or right-align expressions.
 */
int math_expr_width(const char* expr);

/*
 * Returns 1 if expr contains any [F:] or [R:] node (a "2D" expression),
 * 0 otherwise.  Used to decide whether extra spacing is needed.
 */
int math_expr_has_2d(const char* expr);

/*
 * Gap to insert between step[i] and step[i+1].
 * Returns MATH_LINE_GAP_2D if either step is 2D, MATH_LINE_GAP otherwise.
 * Pass next_expr = NULL for the gap after the last step.
 */
int math_step_gap(const char* this_expr, const char* next_expr);

/*
 * Pixel height of the content of `expr` (without the inter-step gap).
 * Plain text returns MATH_SUP_OFFSET + LARGE_FONT_H = 24.
 * Fractions / roots may be taller.
 * Add MATH_LINE_GAP yourself when stacking steps.
 */
int math_expr_height(const char* expr);

/*
 * Render `expr` at pixel position (x, y_line).
 *   x       — left edge of the expression
 *   y_line  — top of the superscript zone (the true typographic top of the line)
 *   fg / bg — foreground / background colour
 *
 * The caller must clear the background rectangle before calling if needed.
 * The function does NOT clear beyond the expression itself.
 */
void draw_math_expr(const char* expr, int x, int y_line,
                    eadk_color_t fg, eadk_color_t bg);

#endif