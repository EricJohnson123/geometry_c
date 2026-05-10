#include "explain.h"
#include "elements.h"
#include "ui.h"
#include "math_render.h"
#include <stdio.h>
#include <string.h>

static float fsqrt(float s) {
  int i; float r;
  if (s <= 0.0f) return 0.0f;
  r = s / 2.0f;
  for (i = 0; i < 24; i++) r = (r + s / r) / 2.0f;
  return r;
}

void render_math_step(const char* text, int x, int y, bool large,
                      eadk_color_t fg, eadk_color_t bg) {
  draw_str(text, x, y, large, fg, bg);
}

int calc_math_width(const char* text, bool large) {
  int fw = large ? LARGE_FONT_W : SMALL_FONT_W;
  return (int)strlen(text) * fw;
}

int math_steps_total_height(const ExplainStep* steps, int n_steps) {
  int i, total = 0;
  for (i = 0; i < n_steps; i++) {
    const char* next = (i + 1 < n_steps) ? steps[i + 1].text : NULL;
    total += math_expr_height(steps[i].text) + math_step_gap(steps[i].text, next);
  }
  return total;
}

static void efmt(float v, char* out) { fmtnum(v, out); }

static int is_int(float v) {
  float d = v - (float)(int)v;
  if (d < 0.0f) d = -d;
  return d < 0.0005f;
}

static int add_step(ExplainStep* steps, int n, const char* text) {
  if (n < MAX_EXPLAIN_STEPS) {
    strncpy(steps[n].text, text, MAX_EXPLAIN_WIDTH - 1);
    steps[n].text[MAX_EXPLAIN_WIDTH - 1] = '\0';
    steps[n].indent = 0;
    n++;
  }
  return n;
}

/* ── Fraction helpers ──────────────────────────────────────────────────── */

static int gcd_int(int a, int b) {
  int t;
  if (a < 0) a = -a;
  if (b < 0) b = -b;
  while (b) { t = b; b = a % b; a = t; }
  return a ? a : 1;
}

/* Find smallest fraction p/q ≈ v (q ≤ 500).
   Returns 1 on success; p carries the sign, q is always > 0. */
static int to_fraction(float v, int* p_out, int* q_out) {
  int sign = (v < 0.0f) ? -1 : 1;
  float av = (v < 0.0f) ? -v : v;
  int q, p;
  float err;
  for (q = 1; q <= 500; q++) {
    float pf = av * (float)q;
    p = (int)(pf + 0.5f);
    err = pf - (float)p;
    if (err < 0.0f) err = -err;
    if (err < 0.0005f) {
      int g = gcd_int(p, q);
      *p_out = sign * (p / g);
      *q_out = q / g;
      return 1;
    }
  }
  return 0;
}

static int terminates_dec(int q) {
  if (q < 0) q = -q;
  while (q % 2 == 0) q /= 2;
  while (q % 5 == 0) q /= 5;
  return q == 1;
}

/*
 * fmt_frac_markup — formats v as a [F:p|q] markup token when v is a
 * non-terminating fraction, or as a plain decimal otherwise.
 *
 * out      : destination buffer (at least maxlen chars)
 * maxlen   : buffer size
 * returns 1 if the result is a non-terminating fraction (caller may want
 *           to add a "≈ decimal" step), 0 otherwise.
 *
 * For the [F:] token we write the absolute value inside and prepend '-'
 * outside so the renderer keeps things clean.
 */
static int fmt_frac_markup(float v, char* out, int maxlen) {
  int p, q;
  char tmp[32];
  char neg_prefix[2];
  float av = v < 0.0f ? -v : v;

  neg_prefix[0] = (v < 0.0f) ? '-' : '\0';
  neg_prefix[1] = '\0';

  if (to_fraction(av, &p, &q) && q > 1 && !terminates_dec(q)) {
    /* non-terminating rational — use stacked fraction markup */
    snprintf(out, maxlen, "%s[F:%d|%d]", neg_prefix, p, q);
    return 1;
  }
  /* integer or terminating decimal — plain text */
  efmt(v, tmp);
  strncpy(out, tmp, maxlen - 1);
  out[maxlen - 1] = '\0';
  return 0;
}

/*
 * fmt_frac_markup_abs — same but for an absolute-value snippet that
 * will be embedded inside a larger expression where the sign has already
 * been handled (e.g. "y = -[F:2|3]x" where the - is written manually).
 */
static int fmt_frac_markup_abs(float v, char* out, int maxlen) {
  float av = v < 0.0f ? -v : v;
  return fmt_frac_markup(av, out, maxlen);
}

/* Build "y = mx + b" using fraction markup for m and b. */
static void fmt_si_eq(float m, float b, char* out, int maxlen) {
  char sm[48], sb[48];
  char result[MAX_EXPLAIN_WIDTH];
  int pos = 0;
  float am = (m < 0.0f) ? -m : m;
  float ab = (b < 0.0f) ? -b : b;

  fmt_frac_markup_abs(am, sm, sizeof(sm));

  if (m < 0.0f)
    pos += snprintf(result + pos, sizeof(result) - pos, "y = -%sx", sm);
  else
    pos += snprintf(result + pos, sizeof(result) - pos, "y = %sx", sm);

  if (b < -0.0005f) {
    fmt_frac_markup_abs(ab, sb, sizeof(sb));
    pos += snprintf(result + pos, sizeof(result) - pos, " - %s", sb);
  } else if (b > 0.0005f) {
    fmt_frac_markup(b, sb, sizeof(sb));
    pos += snprintf(result + pos, sizeof(result) - pos, " + %s", sb);
  }

  strncpy(out, result, maxlen - 1);
  out[maxlen - 1] = '\0';
}

/* Supported unicode (UTF-8) — still used for the ≈ symbol in text */
#define APPROX "\xe2\x89\x88"   /* U+2248 ≈ */
/* ^ = superscript next char, _ = subscript next char (draw_math_expr markup) */
/* √ inside [R:] bodies is drawn by the renderer itself; no need for the
   bare √ UTF-8 sequence in explain.c any more. */

/* ── Distance ───────────────────────────────────────────────────────────── */

int generate_distance_explanation(float x1, float y1, float x2, float y2,
                                   const char* lbl1, const char* lbl2,
                                   ExplainStep* steps) {
  int n = 0;
  int vert, horiz, exact, skip_sq, dx_triv, dy_triv;
  float dx, dy, dx2, dy2, sum, dist, dist_int;
  char sl1[4], sl2[4];
  char sx1[16], sy1[16], sx2[16], sy2[16];
  char sdx[16], sdy[16], sdx2[16], sdy2[16];
  char ssum[16], sresult[16], buf[MAX_EXPLAIN_WIDTH];

  strncpy(sl1, lbl1 ? lbl1 : "A", 3); sl1[3] = '\0';
  strncpy(sl2, lbl2 ? lbl2 : "B", 3); sl2[3] = '\0';

  dx   = x2 - x1;
  dy   = y2 - y1;
  dx2  = dx * dx;
  dy2  = dy * dy;
  sum  = dx2 + dy2;
  dist = fsqrt(sum);

  vert  = (dx > -0.001f && dx < 0.001f);
  horiz = (dy > -0.001f && dy < 0.001f);

  dist_int = (float)(int)(dist + 0.5f);
  exact = (dist_int * dist_int - sum < 0.5f && dist_int * dist_int - sum > -0.5f);
  skip_sq = (is_int(dx) && is_int(dy) && dx*dx < 1000.0f && dy*dy < 1000.0f);

  efmt(x1,sx1); efmt(y1,sy1); efmt(x2,sx2); efmt(y2,sy2);
  efmt(dx,sdx); efmt(dy,sdy); efmt(dx2,sdx2); efmt(dy2,sdy2);
  efmt(sum,ssum); efmt(dist,sresult);

  /* Step 1: general formula using [R:] for the radical */
  if (vert) {
    snprintf(buf, sizeof(buf),
      "d = [R:(y_%s-y_%s)^2]", sl2, sl1);
  } else if (horiz) {
    snprintf(buf, sizeof(buf),
      "d = [R:(x_%s-x_%s)^2]", sl2, sl1);
  } else {
    snprintf(buf, sizeof(buf),
      "d = [R:(x_%s-x_%s)^2+(y_%s-y_%s)^2]",
      sl2, sl1, sl2, sl1);
  }
  n = add_step(steps, n, buf);

  /* Step 2: substitute coordinates */
  if (vert) {
    snprintf(buf, sizeof(buf), "d = [R:(%s-%s)^2]", sy2, sy1);
  } else if (horiz) {
    snprintf(buf, sizeof(buf), "d = [R:(%s-%s)^2]", sx2, sx1);
  } else {
    snprintf(buf, sizeof(buf),
      "d = [R:(%s-%s)^2+(%s-%s)^2]", sx2, sx1, sy2, sy1);
  }
  n = add_step(steps, n, buf);

  /* Step 3: simplify differences */
  dx_triv = (dx >= 0.0f && dx < 10.0f && is_int(dx));
  dy_triv = (dy >= 0.0f && dy < 10.0f && is_int(dy));
  if (vert) {
    snprintf(buf, sizeof(buf), "d = [R:%s^2]", sdy);
    n = add_step(steps, n, buf);
  } else if (horiz) {
    snprintf(buf, sizeof(buf), "d = [R:%s^2]", sdx);
    n = add_step(steps, n, buf);
  } else if (!dx_triv || !dy_triv) {
    snprintf(buf, sizeof(buf), "d = [R:%s^2+%s^2]", sdx, sdy);
    n = add_step(steps, n, buf);
  }

  /* Step 4: evaluate squares */
  if (vert) {
    snprintf(buf, sizeof(buf), "d = [R:%s]", sdy2);
    n = add_step(steps, n, buf);
  } else if (horiz) {
    snprintf(buf, sizeof(buf), "d = [R:%s]", sdx2);
    n = add_step(steps, n, buf);
  } else if (skip_sq) {
    snprintf(buf, sizeof(buf),
      "d = [R:%s+%s] = [R:%s]", sdx2, sdy2, ssum);
    n = add_step(steps, n, buf);
  } else {
    snprintf(buf, sizeof(buf), "d = [R:%s+%s]", sdx2, sdy2);
    n = add_step(steps, n, buf);
    snprintf(buf, sizeof(buf), "d = [R:%s]", ssum);
    n = add_step(steps, n, buf);
  }

  /* Step 5: result */
  if (exact) {
    snprintf(buf, sizeof(buf), "d = %s", sresult);
  } else {
    snprintf(buf, sizeof(buf), "d " APPROX " %s", sresult);
  }
  n = add_step(steps, n, buf);

  return n;
}

/* ── Midpoint ───────────────────────────────────────────────────────────── */

int generate_midpoint_explanation(float x1, float y1, float x2, float y2,
                                   const char* lbl1, const char* lbl2,
                                   ExplainStep* steps) {
  int n = 0;
  int sx_triv, sy_triv;
  float sx, sy, mx, my;
  char sl1[4], sl2[4];
  char sx1[16], sy1[16], sx2[16], sy2[16];
  char ssx[16], ssy[16], smx[48], smy[48];
  char buf[MAX_EXPLAIN_WIDTH];

  strncpy(sl1, lbl1 ? lbl1 : "A", 3); sl1[3] = '\0';
  strncpy(sl2, lbl2 ? lbl2 : "B", 3); sl2[3] = '\0';

  sx = x1 + x2; sy = y1 + y2;
  mx = sx / 2.0f; my = sy / 2.0f;

  efmt(x1,sx1); efmt(y1,sy1); efmt(x2,sx2); efmt(y2,sy2);
  efmt(sx,ssx); efmt(sy,ssy);
  /* Use fraction markup for result coords when they are non-integer */
  fmt_frac_markup(mx, smx, sizeof(smx));
  fmt_frac_markup(my, smy, sizeof(smy));

  /* Step 1: formula */
  snprintf(buf, sizeof(buf),
    "M = ([F:x_%s+x_%s|2], [F:y_%s+y_%s|2])", sl1, sl2, sl1, sl2);
  n = add_step(steps, n, buf);

  /* Step 2: substitute */
  snprintf(buf, sizeof(buf),
    "M = ([F:%s+%s|2], [F:%s+%s|2])", sx1, sx2, sy1, sy2);
  n = add_step(steps, n, buf);

  /* Step 3: add — skip if one coord is 0 */
  sx_triv = (sx == x1 || sx == x2);
  sy_triv = (sy == y1 || sy == y2);
  if (!sx_triv || !sy_triv) {
    snprintf(buf, sizeof(buf), "M = ([F:%s|2], [F:%s|2])", ssx, ssy);
    n = add_step(steps, n, buf);
  }

  /* Step 4: result */
  snprintf(buf, sizeof(buf), "M = (%s, %s)", smx, smy);
  n = add_step(steps, n, buf);

  return n;
}

/* ── Slope ──────────────────────────────────────────────────────────────── */

int generate_slope_explanation(float x1, float y1, float x2, float y2,
                                const char* lbl1, const char* lbl2,
                                ExplainStep* steps) {
  int n = 0;
  float dx, dy, slope;
  int dx_triv, dy_triv;
  char sl1[4], sl2[4];
  char sx1[16], sy1[16], sx2[16], sy2[16];
  char sdx[16], sdy[16], sresult[16];
  char buf[MAX_EXPLAIN_WIDTH];

  strncpy(sl1, lbl1 ? lbl1 : "A", 3); sl1[3] = '\0';
  strncpy(sl2, lbl2 ? lbl2 : "B", 3); sl2[3] = '\0';

  dx = x2 - x1;
  dy = y2 - y1;

  if (dx > -0.001f && dx < 0.001f) {
    n = add_step(steps, n, "Slope undefined");
    snprintf(buf, sizeof(buf), "x_%s = x_%s (vertical)", sl1, sl2);
    n = add_step(steps, n, buf);
    return n;
  }

  slope = dy / dx;
  efmt(x1,sx1); efmt(y1,sy1); efmt(x2,sx2); efmt(y2,sy2);
  efmt(dx,sdx); efmt(dy,sdy); efmt(slope,sresult);

  /* Step 1: formula */
  snprintf(buf, sizeof(buf),
    "m = [F:y_%s-y_%s|x_%s-x_%s]", sl2, sl1, sl2, sl1);
  n = add_step(steps, n, buf);

  /* Step 2: substitute */
  snprintf(buf, sizeof(buf),
    "m = [F:%s-%s|%s-%s]", sy2, sy1, sx2, sx1);
  n = add_step(steps, n, buf);

  /* Step 3: simplified differences */
  dx_triv = (dx > -10.0f && dx < 10.0f && is_int(dx));
  dy_triv = (dy > -10.0f && dy < 10.0f && is_int(dy));
  if (!dx_triv || !dy_triv) {
    snprintf(buf, sizeof(buf), "m = [F:%s|%s]", sdy, sdx);
    n = add_step(steps, n, buf);
  }

  /* Step 4: result — fraction form */
  {
    char sm_frac[48];
    int need_approx = fmt_frac_markup(slope, sm_frac, sizeof(sm_frac));
    snprintf(buf, sizeof(buf), "m = %s", sm_frac);
    n = add_step(steps, n, buf);
    if (need_approx) {
      snprintf(buf, sizeof(buf), "m " APPROX " %s", sresult);
      n = add_step(steps, n, buf);
    }
  }

  return n;
}

/* ── Line (slope-intercept) ─────────────────────────────────────────────── */

int generate_line_si_explanation(float x1, float y1, float x2, float y2,
                                  const char* lbl1, const char* lbl2,
                                  ExplainStep* steps) {
  int n = 0;
  float dx, dy, slope, intercept;
  int dx_triv, dy_triv;
  int m_approx, b_approx;
  char sl1[4], sl2[4];
  char sx1[16], sy1[16], sx2[16], sy2[16];
  char sm[16], sb[16];
  char sm_frac[48], sb_frac[48];
  char eq[MAX_EXPLAIN_WIDTH];
  char buf[MAX_EXPLAIN_WIDTH];

  strncpy(sl1, lbl1 ? lbl1 : "A", 3); sl1[3] = '\0';
  strncpy(sl2, lbl2 ? lbl2 : "B", 3); sl2[3] = '\0';

  dx = x2 - x1;
  dy = y2 - y1;

  if (dx > -0.001f && dx < 0.001f) {
    efmt(x1, sx1);
    snprintf(buf, sizeof(buf), "x = %s (vertical line)", sx1);
    n = add_step(steps, n, buf);
    return n;
  }

  slope     = dy / dx;
  intercept = y1 - slope * x1;
  efmt(x1,sx1); efmt(y1,sy1); efmt(x2,sx2); efmt(y2,sy2);
  efmt(slope, sm); efmt(intercept, sb);

  m_approx = fmt_frac_markup(slope,     sm_frac, sizeof(sm_frac));
  b_approx = fmt_frac_markup(intercept, sb_frac, sizeof(sb_frac));

  dx_triv = (dx > -10.0f && dx < 10.0f && is_int(dx));
  dy_triv = (dy > -10.0f && dy < 10.0f && is_int(dy));

  /* Step 1: slope formula */
  snprintf(buf, sizeof(buf),
    "m = [F:y_%s-y_%s|x_%s-x_%s]", sl2, sl1, sl2, sl1);
  n = add_step(steps, n, buf);

  /* Step 2: substitute (collapse when trivial) */
  if (dx_triv && dy_triv) {
    snprintf(buf, sizeof(buf),
      "m = [F:%s-%s|%s-%s] = %s", sy2, sy1, sx2, sx1, sm_frac);
  } else {
    snprintf(buf, sizeof(buf),
      "m = [F:%s-%s|%s-%s]", sy2, sy1, sx2, sx1);
  }
  n = add_step(steps, n, buf);

  if (!dx_triv || !dy_triv) {
    snprintf(buf, sizeof(buf), "m = %s", sm_frac);
    n = add_step(steps, n, buf);
  }
  if (m_approx) {
    snprintf(buf, sizeof(buf), "m " APPROX " %s", sm);
    n = add_step(steps, n, buf);
  }

  /* Step 3: find b */
  snprintf(buf, sizeof(buf), "b = y_%s - m*x_%s", sl1, sl1);
  n = add_step(steps, n, buf);

  snprintf(buf, sizeof(buf), "b = %s - %s*%s", sy1, sm_frac, sx1);
  n = add_step(steps, n, buf);

  snprintf(buf, sizeof(buf), "b = %s", sb_frac);
  n = add_step(steps, n, buf);
  if (b_approx) {
    snprintf(buf, sizeof(buf), "b " APPROX " %s", sb);
    n = add_step(steps, n, buf);
  }

  /* Step 4: result equation */
  fmt_si_eq(slope, intercept, eq, sizeof(eq));
  n = add_step(steps, n, eq);

  return n;
}

/* ── Line (Cartesian form) ──────────────────────────────────────────────── */

static void fmt_coeff(float v, char* out) {
  float av = v < 0.0f ? -v : v;
  if (av > 0.9995f && av < 1.0005f) {
    out[0] = '\0';
  } else {
    efmt(av, out);
  }
}

static void fmt_cart_eq(float a, float b, float c, char* out, int maxlen) {
  char ca[16], cb[16], cc[16];
  char result[MAX_EXPLAIN_WIDTH];
  int pos = 0;

  fmt_coeff(a, ca);
  fmt_coeff(b, cb);

  if (a < 0.0f)
    pos += snprintf(result + pos, sizeof(result) - pos, "-%sx", ca);
  else
    pos += snprintf(result + pos, sizeof(result) - pos, "%sx", ca);

  if (b < 0.0f)
    pos += snprintf(result + pos, sizeof(result) - pos, " - %sy", cb);
  else if (b > 0.0f)
    pos += snprintf(result + pos, sizeof(result) - pos, " + %sy", cb);

  if (c < -0.0005f) {
    float ac = -c; efmt(ac, cc);
    pos += snprintf(result + pos, sizeof(result) - pos, " - %s", cc);
  } else if (c > 0.0005f) {
    efmt(c, cc);
    pos += snprintf(result + pos, sizeof(result) - pos, " + %s", cc);
  }

  snprintf(result + pos, sizeof(result) - pos, " = 0");
  strncpy(out, result, maxlen - 1);
  out[maxlen - 1] = '\0';
}

int generate_line_cart_explanation(float x1, float y1, float x2, float y2,
                                    const char* lbl1, const char* lbl2,
                                    ExplainStep* steps) {
  int n = 0;
  float dx, dy, a, b, c, ax1, by1, sum_ab;
  int triv_diff, triv_prod;
  char sl1[4], sl2[4];
  char sx1[16], sy1[16], sx2[16], sy2[16];
  char sdx[16], sdy[16], sa[16], sb_s[16], sc[16];
  char sax1[16], sby1[16], ssum[16];
  char sa_frac[48], sb_frac[48], sc_frac[48];
  int a_approx, b_approx, c_approx;
  char eq[MAX_EXPLAIN_WIDTH], buf[MAX_EXPLAIN_WIDTH];

  strncpy(sl1, lbl1 ? lbl1 : "A", 3); sl1[3] = '\0';
  strncpy(sl2, lbl2 ? lbl2 : "B", 3); sl2[3] = '\0';

  dx  = x2 - x1;
  dy  = y2 - y1;
  a   = dy;
  b   = -dx;
  c   = -(a * x1 + b * y1);

  efmt(x1,sx1); efmt(y1,sy1); efmt(x2,sx2); efmt(y2,sy2);
  efmt(dx,sdx); efmt(dy,sdy);
  efmt(a, sa); efmt(b, sb_s); efmt(c, sc);

  a_approx = fmt_frac_markup(a, sa_frac, sizeof(sa_frac));
  b_approx = fmt_frac_markup(b, sb_frac, sizeof(sb_frac));
  c_approx = fmt_frac_markup(c, sc_frac, sizeof(sc_frac));

  ax1    = a * x1;
  by1    = b * y1;
  sum_ab = ax1 + by1;
  efmt(ax1,   sax1);
  efmt(by1,   sby1);
  efmt(sum_ab, ssum);

  triv_diff = (is_int(dx) && is_int(dy) &&
               dx > -20.0f && dx < 20.0f &&
               dy > -20.0f && dy < 20.0f);

  triv_prod = (is_int(ax1) && is_int(by1) &&
               ax1 > -100.0f && ax1 < 100.0f &&
               by1 > -100.0f && by1 < 100.0f);

  /* Step 1: direction vector formula */
  snprintf(buf, sizeof(buf),
    "->%s%s = (x_%s-x_%s, y_%s-y_%s)", sl1, sl2, sl2, sl1, sl2, sl1);
  n = add_step(steps, n, buf);

  /* Step 2: substitute */
  if (triv_diff) {
    snprintf(buf, sizeof(buf),
      "->%s%s = (%s-%s, %s-%s) = (%s, %s)",
      sl1, sl2, sx2, sx1, sy2, sy1, sdx, sdy);
  } else {
    snprintf(buf, sizeof(buf),
      "->%s%s = (%s-%s, %s-%s)", sl1, sl2, sx2, sx1, sy2, sy1);
    n = add_step(steps, n, buf);
    snprintf(buf, sizeof(buf), "->%s%s = (%s, %s)", sl1, sl2, sdx, sdy);
  }
  n = add_step(steps, n, buf);

  /* Step 3: derive a and b */
  snprintf(buf, sizeof(buf), "a = dy = %s", sa_frac);
  n = add_step(steps, n, buf);
  if (a_approx) { snprintf(buf, sizeof(buf), "a " APPROX " %s", sa); n = add_step(steps, n, buf); }
  snprintf(buf, sizeof(buf), "b = -dx = %s", sb_frac);
  n = add_step(steps, n, buf);
  if (b_approx) { snprintf(buf, sizeof(buf), "b " APPROX " %s", sb_s); n = add_step(steps, n, buf); }

  /* Step 4: use point to find c */
  snprintf(buf, sizeof(buf), "a*x_%s + b*y_%s + c = 0", sl1, sl1);
  n = add_step(steps, n, buf);

  /* Step 5: substitute */
  if (triv_prod) {
    snprintf(buf, sizeof(buf), "%s + %s + c = 0", sax1, sby1);
    n = add_step(steps, n, buf);
  } else {
    snprintf(buf, sizeof(buf), "%s*%s + %s*%s + c = 0", sa_frac, sx1, sb_frac, sy1);
    n = add_step(steps, n, buf);
    snprintf(buf, sizeof(buf), "%s + c = 0", ssum);
    n = add_step(steps, n, buf);
  }

  /* Step 6: c */
  snprintf(buf, sizeof(buf), "c = %s", sc_frac);
  n = add_step(steps, n, buf);
  if (c_approx) { snprintf(buf, sizeof(buf), "c " APPROX " %s", sc); n = add_step(steps, n, buf); }

  /* Step 7: final equation */
  fmt_cart_eq(a, b, c, eq, sizeof(eq));
  n = add_step(steps, n, eq);

  return n;
}

/* ── Vector magnitude ───────────────────────────────────────────────────── */

int generate_vector_mag_explanation(float dx, float dy,
                                     const char* lbl1, const char* lbl2,
                                     ExplainStep* steps) {
  int n = 0;
  int exact, skip_sq;
  float dx2, dy2, sum, mag, mag_int;
  char sl1[4], sl2[4];
  char sdx[16], sdy[16], sdx2[16], sdy2[16], ssum[16], sresult[16];
  char buf[MAX_EXPLAIN_WIDTH];

  strncpy(sl1, lbl1 ? lbl1 : "A", 3); sl1[3] = '\0';
  strncpy(sl2, lbl2 ? lbl2 : "B", 3); sl2[3] = '\0';

  dx2  = dx * dx;
  dy2  = dy * dy;
  sum  = dx2 + dy2;
  mag  = fsqrt(sum);

  mag_int = (float)(int)(mag + 0.5f);
  exact   = (mag_int * mag_int - sum < 0.5f && mag_int * mag_int - sum > -0.5f);
  skip_sq = (is_int(dx) && is_int(dy) && dx2 < 1000.0f && dy2 < 1000.0f);

  efmt(dx,sdx); efmt(dy,sdy); efmt(dx2,sdx2); efmt(dy2,sdy2);
  efmt(sum,ssum); efmt(mag,sresult);

  /* Step 1: formula */
  snprintf(buf, sizeof(buf),
    "||%s%s|| = [R:dx^2+dy^2]", sl1, sl2);
  n = add_step(steps, n, buf);

  /* Step 2: substitute */
  snprintf(buf, sizeof(buf),
    "||%s%s|| = [R:%s^2+%s^2]", sl1, sl2, sdx, sdy);
  n = add_step(steps, n, buf);

  /* Step 3: evaluate squares */
  if (skip_sq) {
    snprintf(buf, sizeof(buf),
      "||%s%s|| = [R:%s+%s] = [R:%s]", sl1, sl2, sdx2, sdy2, ssum);
    n = add_step(steps, n, buf);
  } else {
    snprintf(buf, sizeof(buf),
      "||%s%s|| = [R:%s+%s]", sl1, sl2, sdx2, sdy2);
    n = add_step(steps, n, buf);
    snprintf(buf, sizeof(buf),
      "||%s%s|| = [R:%s]", sl1, sl2, ssum);
    n = add_step(steps, n, buf);
  }

  /* Step 4: result */
  if (exact) {
    snprintf(buf, sizeof(buf), "||%s%s|| = %s", sl1, sl2, sresult);
  } else {
    snprintf(buf, sizeof(buf), "||%s%s|| " APPROX " %s", sl1, sl2, sresult);
  }
  n = add_step(steps, n, buf);

  return n;
}

/* ── Centroid ───────────────────────────────────────────────────────────── */

int generate_centroid_explanation(float x1, float y1, float x2, float y2,
                                   float x3, float y3,
                                   const char* lbl1, const char* lbl2,
                                   const char* lbl3, ExplainStep* steps) {
  int n = 0;
  int sx_triv, sy_triv;
  float sx, sy, gx, gy;
  char sl1[4], sl2[4], sl3[4];
  char sx1[16], sy1[16], sx2[16], sy2[16], sx3[16], sy3[16];
  char ssx[16], ssy[16], sgx[48], sgy[48];
  char buf[MAX_EXPLAIN_WIDTH];

  strncpy(sl1, lbl1 ? lbl1 : "A", 3); sl1[3] = '\0';
  strncpy(sl2, lbl2 ? lbl2 : "B", 3); sl2[3] = '\0';
  strncpy(sl3, lbl3 ? lbl3 : "C", 3); sl3[3] = '\0';

  sx = x1 + x2 + x3;
  sy = y1 + y2 + y3;
  gx = sx / 3.0f;
  gy = sy / 3.0f;

  efmt(x1,sx1); efmt(y1,sy1); efmt(x2,sx2); efmt(y2,sy2);
  efmt(x3,sx3); efmt(y3,sy3);
  efmt(sx,ssx); efmt(sy,ssy);
  fmt_frac_markup(gx, sgx, sizeof(sgx));
  fmt_frac_markup(gy, sgy, sizeof(sgy));

  /* Step 1: formula */
  snprintf(buf, sizeof(buf),
    "G = ([F:x_%s+x_%s+x_%s|3], [F:y_%s+y_%s+y_%s|3])",
    sl1, sl2, sl3, sl1, sl2, sl3);
  n = add_step(steps, n, buf);

  /* Step 2: substitute */
  snprintf(buf, sizeof(buf),
    "G = ([F:%s+%s+%s|3], [F:%s+%s+%s|3])",
    sx1, sx2, sx3, sy1, sy2, sy3);
  n = add_step(steps, n, buf);

  /* Step 3: sum */
  sx_triv = (is_int(sx) && sx >= 0.0f && sx < 30.0f);
  sy_triv = (is_int(sy) && sy >= 0.0f && sy < 30.0f);
  if (!sx_triv || !sy_triv) {
    snprintf(buf, sizeof(buf), "G = ([F:%s|3], [F:%s|3])", ssx, ssy);
    n = add_step(steps, n, buf);
  }

  /* Step 4: result */
  snprintf(buf, sizeof(buf), "G = (%s, %s)", sgx, sgy);
  n = add_step(steps, n, buf);

  return n;
}