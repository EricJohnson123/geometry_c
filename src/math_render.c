#include "math_render.h"
#include "ui.h"
#include <string.h>

/* ── UTF-8 helper ──────────────────────────────────────────────────────── */
static int utf8_seq_len(unsigned char b) {
  if (b >= 0xF0) return 4;
  if (b >= 0xE0) return 3;
  if (b >= 0xC0) return 2;
  return 1;
}

/* ── Bracket-expression parsers ────────────────────────────────────────── */
/*
 * find_bracket_end(p)
 *   p points to the char AFTER the opening '['.
 *   Returns pointer to the matching ']', or NULL if not found.
 *   Does not handle nesting of '['.
 */
static const char* find_bracket_end(const char* p) {
  while (*p && *p != ']') p++;
  return *p == ']' ? p : NULL;
}

/*
 * parse_frac(p, num_out, den_out)
 *   p points to the char AFTER "[F:".
 *   Fills num_out / den_out with the two sub-strings (NUL-terminated copies).
 *   Returns pointer to the ']' that closes the token, or NULL on error.
 *   Buffers must be at least MATH_SEG_BUF chars.
 */
static const char* parse_frac(const char* p,
                               char* num_out, char* den_out) {
  const char* sep;
  const char* end;
  int len;

  sep = p;
  while (*sep && *sep != '|' && *sep != ']') sep++;
  if (*sep != '|') return NULL;

  end = find_bracket_end(sep + 1);
  if (!end) return NULL;

  len = (int)(sep - p);
  if (len >= MATH_SEG_BUF) len = MATH_SEG_BUF - 1;
  memcpy(num_out, p, (size_t)len);
  num_out[len] = '\0';

  len = (int)(end - (sep + 1));
  if (len >= MATH_SEG_BUF) len = MATH_SEG_BUF - 1;
  memcpy(den_out, sep + 1, (size_t)len);
  den_out[len] = '\0';

  return end;   /* caller should advance past ']' */
}

/*
 * parse_root_body(p, body_out)
 *   p points to the char AFTER "[R:".
 *   Fills body_out with the body string (NUL-terminated copy).
 *   Returns pointer to the ']', or NULL on error.
 */
static const char* parse_root_body(const char* p, char* body_out) {
  const char* end = find_bracket_end(p);
  int len;
  if (!end) return NULL;
  len = (int)(end - p);
  if (len >= MATH_SEG_BUF) len = MATH_SEG_BUF - 1;
  memcpy(body_out, p, (size_t)len);
  body_out[len] = '\0';
  return end;
}

/* ── Width of a plain-markup sub-expression (no [F:] or [R:]) ─────────── */
/*
 * Used internally to measure numerator / denominator strings that may only
 * contain ^/_ markup and plain ASCII.
 */
static int plain_width(const char* expr) {
  const char* p = expr;
  int w = 0;
  int nb;
  while (*p) {
    if (*p == '^' || *p == '_') {
      p++;
      if (*p) { w += SMALL_FONT_W; p++; }
    } else if ((unsigned char)*p >= 0x80) {
      nb = utf8_seq_len((unsigned char)*p);
      w += SMALL_FONT_W;
      p += nb;
    } else {
      w += SMALL_FONT_W;
      p++;
    }
  }
  return w;
}

/* ── Width of a full expression (may contain [F:] and [R:]) ───────────── */
int math_expr_width(const char* expr) {
  const char* p = expr;
  int w = 0;
  int nb;
  char num[MATH_SEG_BUF], den[MATH_SEG_BUF], body[MATH_SEG_BUF];
  const char* end;
  int nw, dw, bar_w;

  while (*p) {
    if (p[0] == '[' && p[1] == 'F' && p[2] == ':') {
      end = parse_frac(p + 3, num, den);
      if (end) {
        nw = plain_width(num);
        dw = plain_width(den);
        bar_w = (nw > dw ? nw : dw) + 4;
        w += bar_w;
        p = end + 1;
      } else {
        w += SMALL_FONT_W; p++;
      }
    } else if (p[0] == '[' && p[1] == 'R' && p[2] == ':') {
      end = parse_root_body(p + 3, body);
      if (end) {
        /* √ glyph + body width */
        w += SMALL_FONT_W + math_expr_width(body);
        p = end + 1;
      } else {
        w += SMALL_FONT_W; p++;
      }
    } else if (*p == '^' || *p == '_') {
      p++;
      if (*p) { w += SMALL_FONT_W; p++; }
    } else if ((unsigned char)*p >= 0x80) {
      nb = utf8_seq_len((unsigned char)*p);
      w += SMALL_FONT_W;
      p += nb;
    } else {
      w += SMALL_FONT_W;
      p++;
    }
  }
  return w;
}

/* ── Height helpers ────────────────────────────────────────────────────── */

/*
 * Height of the "slot" occupied by a plain (non-2D) token: the sup zone
 * plus the large-font body plus room for a subscript.
 * = MATH_SUP_OFFSET + LARGE_FONT_H = 6 + 18 = 24
 */
#define PLAIN_SLOT_H   (MATH_SUP_OFFSET + LARGE_FONT_H)

/*
 * Height of a fraction node:
 *   numerator   : SMALL_FONT_H = 14
 *   vinculum gap: 3
 *   denominator : SMALL_FONT_H = 14
 *   total       : 31
 * The fraction is vertically centred on the main baseline, so when it
 * appears inline with plain text the containing line is taller than
 * PLAIN_SLOT_H.  We treat frac_height as an absolute slot height that
 * replaces the plain slot.
 */
#define FRAC_H   (SMALL_FONT_H + 3 + SMALL_FONT_H)   /* 31 */

/*
 * Height of a square-root node:
 *   overbar        : 2
 *   body slot      : max(PLAIN_SLOT_H, body_height)
 *   (we add the 2px overbar on top of whatever the body needs)
 */
static int root_height(const char* body) {
  int bh = math_expr_height(body);
  return 2 + bh;
}

int math_expr_height(const char* expr) {
  const char* p = expr;
  int h = PLAIN_SLOT_H;   /* start with the minimum */
  char num[MATH_SEG_BUF], den[MATH_SEG_BUF], body[MATH_SEG_BUF];
  const char* end;
  int node_h;

  while (*p) {
    if (p[0] == '[' && p[1] == 'F' && p[2] == ':') {
      end = parse_frac(p + 3, num, den);
      if (end) {
        node_h = FRAC_H;
        if (node_h > h) h = node_h;
        p = end + 1;
      } else { p++; }
    } else if (p[0] == '[' && p[1] == 'R' && p[2] == ':') {
      end = parse_root_body(p + 3, body);
      if (end) {
        node_h = root_height(body);
        if (node_h > h) h = node_h;
        p = end + 1;
      } else { p++; }
    } else if (*p == '^' || *p == '_') {
      p++; if (*p) p++;
    } else if ((unsigned char)*p >= 0x80) {
      p += utf8_seq_len((unsigned char)*p);
    } else {
      p++;
    }
  }
  return h;
}


/* ── Markup scanners ───────────────────────────────────────────────────── */

/* Returns 1 if expr contains at least one '^' (superscript) token. */
static int has_superscript(const char* expr) {
  const char* p = expr;
  while (*p) {
    if (*p == '^') return 1;
    /* skip bracket expressions so we don't false-positive on their content */
    if (p[0] == '[' && (p[1] == 'F' || p[1] == 'R') && p[2] == ':') {
      const char* e = find_bracket_end(p + 3);
      if (e) { p = e + 1; continue; }
    }
    p++;
  }
  return 0;
}

/* Returns 1 if expr contains at least one '_' (subscript) token. */
static int has_subscript(const char* expr) {
  const char* p = expr;
  while (*p) {
    if (*p == '_') return 1;
    if (p[0] == '[' && (p[1] == 'F' || p[1] == 'R') && p[2] == ':') {
      const char* e = find_bracket_end(p + 3);
      if (e) { p = e + 1; continue; }
    }
    p++;
  }
  return 0;
}

int math_expr_has_2d(const char* expr) {
  const char* p = expr;
  while (*p) {
    if (p[0] == '[' && (p[1] == 'F' || p[1] == 'R') && p[2] == ':')
      return 1;
    p++;
  }
  return 0;
}

int math_step_gap(const char* this_expr, const char* next_expr) {
  if (math_expr_has_2d(this_expr)) return MATH_LINE_GAP_2D;
  if (next_expr && math_expr_has_2d(next_expr)) return MATH_LINE_GAP_2D;
  return MATH_LINE_GAP;
}


/* ── Plain sub-expression renderer ────────────────────────────────────── */
/*
 * draw_plain_expr — renders a string that may contain ^/_ and UTF-8 but
 * NO [F:] or [R:] nodes.  Used for numerators / denominators.
 *
 *   cx       — current pixel x (advanced in-place)
 *   y_main   — top of the main (large) font row
 *   sup_push — extra pixels added to y_sup (pushes superscripts down)
 *   fg / bg  — colours
 */
static void draw_plain_expr(const char* expr, int* cx, int y_main,
                             int sup_push, eadk_color_t fg, eadk_color_t bg) {
  const char* p = expr;
  int y_sup = y_main - MATH_SUP_OFFSET + sup_push;
  int y_sub = y_main + LARGE_FONT_H - SMALL_FONT_H + 2;
  char seg[MATH_SEG_BUF];
  int seg_len = 0;
  char buf[5];
  int nb, k;

#define FLUSH_PLAIN() do { \
  if (seg_len > 0) { \
    seg[seg_len] = '\0'; \
    draw_str(seg, *cx, y_main, true, fg, bg); \
    *cx += seg_len * SMALL_FONT_W; \
    seg_len = 0; \
  } \
} while (0)

  while (*p) {
    if (*p == '^') {
      FLUSH_PLAIN();
      p++;
      if (*p) {
        buf[0] = *p++; buf[1] = '\0';
        draw_str(buf, *cx, y_sup, false, fg, bg);
        *cx += SMALL_FONT_W;
      }
    } else if (*p == '_') {
      FLUSH_PLAIN();
      p++;
      if (*p) {
        buf[0] = *p++; buf[1] = '\0';
        draw_str(buf, *cx, y_sub, false, fg, bg);
        *cx += SMALL_FONT_W;
      }
    } else if ((unsigned char)*p >= 0x80) {
      FLUSH_PLAIN();
      nb = utf8_seq_len((unsigned char)*p);
      if (nb > 4) nb = 4;
      for (k = 0; k < nb; k++) buf[k] = p[k];
      buf[nb] = '\0';
      draw_str(buf, *cx, y_main, true, fg, bg);
      *cx += SMALL_FONT_W;
      p += nb;
    } else {
      if (seg_len < MATH_SEG_BUF - 1) seg[seg_len++] = *p;
      p++;
    }
  }
  FLUSH_PLAIN();
#undef FLUSH_PLAIN
}

/* ── Full expression renderer (internal) ───────────────────────────────── */
/*
 * sup_push: extra pixels added to y_sup for all superscripts in this call.
 * Used when rendering inside a [R:] body so exponents don't touch the bar.
 */
static void draw_math_expr_ex(const char* expr, int x, int y_line,
                               eadk_color_t fg, eadk_color_t bg, int sup_push);

static void draw_math_expr_ex(const char* expr, int x, int y_line,
                               eadk_color_t fg, eadk_color_t bg, int sup_push) {
  int line_h = math_expr_height(expr);
  int y_main = y_line + (line_h - LARGE_FONT_H) / 2;
  int y_sup  = y_main - MATH_SUP_OFFSET + sup_push;
  int y_sub  = y_main + LARGE_FONT_H - SMALL_FONT_H + 2;

  const char* p = expr;
  int cx = x;

  char seg[MATH_SEG_BUF];
  int  seg_len = 0;
  char buf[5];
  int  nb, k;

  char num[MATH_SEG_BUF], den[MATH_SEG_BUF], body[MATH_SEG_BUF];
  const char* end;
  int nw, dw, bar_w, body_w;

#define FLUSH_SEG() do { \
  if (seg_len > 0) { \
    seg[seg_len] = '\0'; \
    draw_str(seg, cx, y_main, true, fg, bg); \
    cx += seg_len * SMALL_FONT_W; \
    seg_len = 0; \
  } \
} while (0)

  while (*p) {

    /* ── Fraction ──────────────────────────────────────────────────── */
    if (p[0] == '[' && p[1] == 'F' && p[2] == ':') {
      FLUSH_SEG();
      end = parse_frac(p + 3, num, den);
      if (!end) {
        if (seg_len < MATH_SEG_BUF - 1) seg[seg_len++] = *p;
        p++; continue;
      }

      nw = plain_width(num);
      dw = plain_width(den);
      bar_w = (nw > dw ? nw : dw) + 4;

      /*
       * Vinculum position.
       * Default: centre of the FRAC_H slot relative to y_line.
       * bar_y = y_line + FRAC_H/2 - 1
       *
       * If the numerator contains a subscript, its text (SMALL_FONT_H tall)
       * starts at num_y_main + LARGE_FONT_H - SMALL_FONT_H + 2 and ends
       * SMALL_FONT_H pixels later.  We need that bottom edge to clear bar_y.
       * Shift the numerator up (and thus effectively bar_y stays, but we
       * compute num_y_main so subscripts clear it).
       *
       * Required:  num_y_main + LARGE_FONT_H + 2 <= bar_y
       *   =>  num_y_main <= bar_y - LARGE_FONT_H - 2
       */
      {
        int bar_y    = y_line + FRAC_H / 2 - 1;
        int num_y_main;
        int num_x, tmp_cx;
        int den_y, den_x;

        /* Default numerator main-font y */
        num_y_main = y_line + (FRAC_H / 2 - 2 - SMALL_FONT_H);

        /* If numerator has subscripts, pull it up so they clear the bar */
        if (has_subscript(num)) {
          int max_num_y = bar_y - LARGE_FONT_H - 2;
          if (num_y_main > max_num_y) num_y_main = max_num_y;
        }

        /* Draw numerator */
        num_x  = cx + (bar_w - nw) / 2;
        tmp_cx = num_x;
        draw_plain_expr(num, &tmp_cx, num_y_main, 0, fg, bg);

        /* Vinculum */
        fill_rect(cx, bar_y, bar_w, 1, fg);

        /* Denominator */
        den_y  = bar_y + 2;
        den_x  = cx + (bar_w - dw) / 2;
        tmp_cx = den_x;
        draw_plain_expr(den, &tmp_cx, den_y, 0, fg, bg);
      }

      cx += bar_w;
      p = end + 1;
      continue;
    }

    /* ── Square root ───────────────────────────────────────────────── */
    if (p[0] == '[' && p[1] == 'R' && p[2] == ':') {
      FLUSH_SEG();
      end = parse_root_body(p + 3, body);
      if (!end) {
        if (seg_len < MATH_SEG_BUF - 1) seg[seg_len++] = *p;
        p++; continue;
      }

      body_w = math_expr_width(body);

      {
        int SUP_PUSH_ROOT = 3;
        int bar_y, body_y;
        int body_h;
        char sq[4];

        body_h = math_expr_height(body);

        if (has_superscript(body)) {
          bar_y  = y_line;
          body_y = bar_y + 2;
        } else {
          bar_y = y_main;
          /*
           * Without the correction the recursive call computes its own
           * y_main = body_y + (body_h - LARGE_FONT_H) / 2, which pushes
           * the numbers down by the sup-zone slack.  Solve for body_y such
           * that the recursive y_main == bar_y + 2 (flush under the bar):
           *   body_y + (body_h - LARGE_FONT_H) / 2 = bar_y + 2
           *   body_y = bar_y + 2 - (body_h - LARGE_FONT_H) / 2
           */
          body_y = bar_y + 2 - (body_h - LARGE_FONT_H) / 2;
        }

        sq[0] = (char)0xE2; sq[1] = (char)0x88; sq[2] = (char)0x9A; sq[3] = '\0';
        draw_str(sq, cx, bar_y, true, fg, bg);
        cx += SMALL_FONT_W;

        fill_rect(cx, bar_y, body_w, 1, fg);
        draw_math_expr_ex(body, cx, body_y, fg, bg, SUP_PUSH_ROOT);
      }
      cx += body_w;

      p = end + 1;
      continue;
    }

    /* ── Superscript ───────────────────────────────────────────────── */
    if (*p == '^') {
      FLUSH_SEG();
      p++;
      if (*p) {
        buf[0] = *p++; buf[1] = '\0';
        draw_str(buf, cx, y_sup, false, fg, bg);
        cx += SMALL_FONT_W;
      }
      continue;
    }

    /* ── Subscript ─────────────────────────────────────────────────── */
    if (*p == '_') {
      FLUSH_SEG();
      p++;
      if (*p) {
        buf[0] = *p++; buf[1] = '\0';
        draw_str(buf, cx, y_sub, false, fg, bg);
        cx += SMALL_FONT_W;
      }
      continue;
    }

    /* ── UTF-8 multi-byte ──────────────────────────────────────────── */
    if ((unsigned char)*p >= 0x80) {
      FLUSH_SEG();
      nb = utf8_seq_len((unsigned char)*p);
      if (nb > 4) nb = 4;
      for (k = 0; k < nb; k++) buf[k] = p[k];
      buf[nb] = '\0';
      draw_str(buf, cx, y_main, true, fg, bg);
      cx += SMALL_FONT_W;
      p += nb;
      continue;
    }

    /* ── Plain ASCII ───────────────────────────────────────────────── */
    if (seg_len < MATH_SEG_BUF - 1) seg[seg_len++] = *p;
    p++;
  }

  FLUSH_SEG();
#undef FLUSH_SEG
}

void draw_math_expr(const char* expr, int x, int y_line,
                    eadk_color_t fg, eadk_color_t bg) {
  draw_math_expr_ex(expr, x, y_line, fg, bg, 0);
}