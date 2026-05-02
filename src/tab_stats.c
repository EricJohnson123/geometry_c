#include "tab_stats.h"
#include "ui.h"
#include "elements.h"
#include "tab_graph.h"
#include <stdio.h>
#include <stdbool.h>

// ─── Math helpers (no libm) ───────────────────────────────────────────────────
static float fsqrt(float s) {
  int i;
  float r;
  if (s <= 0.0f) return 0.0f;
  r = s / 2.0f;
  for (i = 0; i < 24; i++) r = (r + s / r) / 2.0f;
  return r;
}

static float point_dist(float x1, float y1, float x2, float y2) {
  float dx = x2 - x1, dy = y2 - y1;
  return fsqrt(dx * dx + dy * dy);
}

// arccos via minimax polynomial (accurate to ~0.01 deg for input in [-1,1])
// Returns angle in degrees.
static float facos_deg(float x) {
  float result, y, y3, y5, y7, asin_y;
  float x3, x5, x7, asin_x;
  if (x >  1.0f) x =  1.0f;
  if (x < -1.0f) x = -1.0f;
  if (x < -0.7f || x > 0.7f) {
    y     = fsqrt((1.0f - x) / 2.0f);
    y3    = y * y * y;
    y5    = y3 * y * y;
    y7    = y5 * y * y;
    asin_y = y + y3 / 6.0f + 3.0f * y5 / 40.0f + 15.0f * y7 / 336.0f;
    result = 2.0f * asin_y;
  } else {
    x3    = x * x * x;
    x5    = x3 * x * x;
    x7    = x5 * x * x;
    asin_x = x + x3 / 6.0f + 3.0f * x5 / 40.0f + 15.0f * x7 / 336.0f;
    result = 1.5707963f - asin_x;
  }
  return result * 57.29578f;
}

// Round float to nearest 0.001, check if whole number
static bool is_whole(float v) {
  long scaled;
  if (v < 0.0f) v = -v;
  scaled = (long)(v * 1000.0f + 0.5f);
  return (scaled % 1000) == 0;
}

// ─── Rational arithmetic ──────────────────────────────────────────────────────

static long gcd(long a, long b) {
  long t;
  if (a < 0) a = -a;
  if (b < 0) b = -b;
  while (b) { t = b; b = a % b; a = t; }
  return a ? a : 1;
}

// Convert float to exact fraction num/den in lowest terms.
// Uses continued fractions, max denominator 1000.
static void float_to_frac(float v, long* num, long* den) {
  long neg, a, h0, h1, k0, k1, h, k, g;
  float x, rem;

  neg = (v < 0.0f) ? 1 : 0;
  if (neg) v = -v;

  h0 = 1; h1 = 0;
  k0 = 0; k1 = 1;
  x = v;

  /* Up to 8 continued-fraction steps, stop when denominator > 1000 */
  while (1) {
    a = (long)x;
    h = a * h0 + h1;
    k = a * k0 + k1;
    if (k > 1000) break;
    h1 = h0; h0 = h;
    k1 = k0; k0 = k;
    rem = x - (float)a;
    if (rem < 1e-5f) break;
    x = 1.0f / rem;
  }

  g = gcd(h0, k0);
  *num = neg ? -(h0 / g) : (h0 / g);
  *den = k0 / g;
}

// ─── Clean equation builders ──────────────────────────────────────────────────

// Write a fraction to buf: "3/4", "-3/4", "2" (if den==1), "" (if num==0).
// If coeff_mode=1, special-case num==den (return "") and num==-den (return "-").
static void fmt_frac(long num, long den, char* buf, int coeff_mode) {
  if (num == 0)            { buf[0] = '\0'; return; }
  if (coeff_mode) {
    if (num ==  den)       { buf[0] = '\0'; return; }
    if (num == -den)       { buf[0] = '-'; buf[1] = '\0'; return; }
  }
  if (den == 1)            { snprintf(buf, 16, "%ld", num); return; }
  snprintf(buf, 16, "%ld/%ld", num, den);
}

// Build slope-intercept: "y = (7/6)x - 1/6"
static void build_slope_intercept(float slope, float intercept,
                                  char* out, int maxlen) {
  long sn, sd, bn, bd, g;
  char ms[16], bs[16];
  char sign;
  float ab;

  float_to_frac(slope, &sn, &sd);
  fmt_frac(sn, sd, ms, 1);  // coeff mode: 1 -> "", -1 -> "-"

  if (intercept == 0.0f) {
    snprintf(out, maxlen, "y = %sx", ms);
    return;
  }

  // Express intercept as fraction too
  float_to_frac(intercept < 0.0f ? -intercept : intercept, &bn, &bd);
  if (intercept < 0.0f) bn = -bn;

  // If slope and intercept share same denominator, show as fraction
  if (sd == bd) {
    // e.g. slope=7/6, intercept=-1/6 => "y = 7/6 x - 1/6"
    fmt_frac(sn, sd, ms, 1);
    ab = intercept < 0.0f ? -intercept : intercept;
    float_to_frac(ab, &bn, &bd);
    // Reduce intercept fraction
    g = gcd(bn < 0 ? -bn : bn, bd);
    bn /= g; bd /= g;
    fmt_frac(bn, bd, bs, 0);
    sign = intercept < 0.0f ? '-' : '+';
    snprintf(out, maxlen, "y = %sx %c %s", ms, sign, bs);
  } else {
    ab = intercept < 0.0f ? -intercept : intercept;
    float_to_frac(ab, &bn, &bd);
    fmt_frac(bn, bd, bs, 0);
    sign = intercept < 0.0f ? '-' : '+';
    snprintf(out, maxlen, "y = %sx %c %s", ms, sign, bs);
  }
}

// Build cartesian form using exact rational slope: "7x - 6y - 1 = 0"
static void build_cartesian(float slope, float intercept,
                             char* out, int maxlen) {
  long sn, sd, g, scale, cx, cy, cc, t;
  char tmp[48];
  int pos;

  // slope = sn/sd  =>  sn*x - sd*y - sd*intercept = 0
  float_to_frac(slope, &sn, &sd);

  // intercept term: sd * intercept as integer (round to nearest)
  // Use same fraction approach: intercept = in/id
  {
    long in2, id2;
    float ai = intercept < 0.0f ? -intercept : intercept;
    float_to_frac(ai, &in2, &id2);
    if (intercept < 0.0f) in2 = -in2;
    // Bring to common denominator sd:
    // equation: sn/sd * x - y + in2/id2 = 0
    // multiply through by lcm(sd, id2)
    g = gcd(sd, id2);
    scale = (sd / g) * id2;  // lcm
    cx =  sn * (scale / sd);
    cy = -1  * (scale / 1);   // -1 * scale, but we need -scale/1 normalised
    // Actually: multiply (sn/sd)x - y + (in2/id2) = 0 by scale=lcm(sd,id2)
    cx = sn  * (scale / sd);
    cy = -1  * scale;
    cc = in2 * (scale / id2);
  }

  // Reduce all three by GCD
  {
    long aa = cx < 0 ? -cx : cx;
    long bb = cy < 0 ? -cy : cy;
    long cc2 = cc < 0 ? -cc : cc;
    g = gcd(aa, bb);
    if (cc2) { t = gcd(g, cc2); g = t; }
    if (g == 0) g = 1;
  }
  cx /= g; cy /= g; cc /= g;

  // Make leading coefficient positive
  if (cx < 0) { cx = -cx; cy = -cy; cc = -cc; }

  tmp[0] = '\0';
  pos = 0;

  // x term
  if      (cx ==  1) pos += snprintf(tmp+pos, sizeof(tmp)-pos, "x");
  else if (cx == -1) pos += snprintf(tmp+pos, sizeof(tmp)-pos, "-x");
  else if (cx !=  0) pos += snprintf(tmp+pos, sizeof(tmp)-pos, "%ldx", cx);

  // y term
  if (cy != 0) {
    if (pos > 0) {
      if (cy > 0) pos += snprintf(tmp+pos, sizeof(tmp)-pos, " + ");
      else        { pos += snprintf(tmp+pos, sizeof(tmp)-pos, " - "); cy = -cy; }
    }
    if      (cy == 1) pos += snprintf(tmp+pos, sizeof(tmp)-pos, "y");
    else              pos += snprintf(tmp+pos, sizeof(tmp)-pos, "%ldy", cy);
  }

  // constant term
  if (cc != 0) {
    if (pos > 0) {
      if (cc > 0) pos += snprintf(tmp+pos, sizeof(tmp)-pos, " + ");
      else        { pos += snprintf(tmp+pos, sizeof(tmp)-pos, " - "); cc = -cc; }
    }
    pos += snprintf(tmp+pos, sizeof(tmp)-pos, "%ld", cc);
  }

  snprintf(out, maxlen, "%s = 0", tmp);
}

// ─── Row-based text output ────────────────────────────────────────────────────
static int stat_y;        // current draw cursor (absolute screen y)
static int stat_scroll;   // pixel offset scrolled (0 = top)
static int stat_total_h;  // total content height accumulated (for scrollbar)

// Base y of the stats content area (before scroll)
#define STAT_BASE_Y  (CONTENT_Y + 4)

static void stat_line(const char* label, const char* value) {
  // Always advance total height tracker
  stat_total_h += SMALL_FONT_H + 1;
  if (stat_y + SMALL_FONT_H <= CONTENT_Y) { stat_y += SMALL_FONT_H + 1; return; }
  if (stat_y >= CONTENT_BOTTOM)           { stat_y += SMALL_FONT_H + 1; return; }
  draw_str_clipped(label, 10,  stat_y, false, COLOR_DARK_GRAY, COLOR_WHITE);
  draw_str_clipped(value, 150, stat_y, false, COLOR_BLACK,     COLOR_WHITE);
  stat_y += SMALL_FONT_H + 1;
}

static void stat_sep(void) {
  stat_total_h += 5;
  if (stat_y >= CONTENT_BOTTOM) { stat_y += 5; return; }
  if (stat_y + 4 >= CONTENT_Y)
    draw_hline_clipped(10, stat_y + 1, SCREEN_W - 22, COLOR_LIGHT_GRAY);
  stat_y += 5;
}

static void stat_header(const char* text) {
  int hw;
  stat_total_h += SMALL_FONT_H + 3;
  if (stat_y >= CONTENT_BOTTOM) { stat_y += SMALL_FONT_H + 3; return; }
  if (stat_y + SMALL_FONT_H >= CONTENT_Y) {
    hw = (int)strlen(text) * SMALL_FONT_W + 12;
    fill_rect_clipped(10, stat_y - 1, hw, SMALL_FONT_H + 2, COLOR_ADD_ROW_BG);
    draw_str_clipped(text, 14, stat_y, false, COLOR_DARK_GRAY, COLOR_ADD_ROW_BG);
  }
  stat_y += SMALL_FONT_H + 3;
}

// Draw a scrollbar on the right edge
static void draw_scrollbar(void) {
  int track_h, thumb_h, thumb_y, tx;
  if (stat_total_h <= CONTENT_H) return;  // no scroll needed
  tx      = SCREEN_W - 4;
  track_h = CONTENT_H;
  thumb_h = track_h * CONTENT_H / stat_total_h;
  if (thumb_h < 8) thumb_h = 8;
  thumb_y = CONTENT_Y + (track_h - thumb_h) * stat_scroll / (stat_total_h - CONTENT_H);
  fill_rect(tx, CONTENT_Y, 3, track_h, COLOR_LIGHT_GRAY);
  fill_rect(tx, thumb_y,   3, thumb_h, COLOR_DARK_GRAY);
}

// ─── Draw ─────────────────────────────────────────────────────────────────────
void draw_stats_tab(void) {
  int ai[MAX_ELEMENTS], vi[MAX_ELEMENTS], acount, vcount, i, j;
  char buf[64], v1[16], v2[16], lbl[16];
  Element *a, *b, *c;
  float d, mx, my, slope, intercept, abx, aby;
  float ab, bc, ca, area, ang_a, ang_b, ang_c;
  float ax, ay, bx, by, cx2, cy2, dx2, dy2, dot_a, dot_b;
  float gx, gy;
  bool collinear, is_right, eq_ab_bc, eq_bc_ca, eq_ab_ca;
  const char* tri_type;

  fill_rect_clipped(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_WHITE);
  stat_total_h = 0;
  stat_y = STAT_BASE_Y - stat_scroll;

  // Collect active points and active vectors separately
  acount = 0;
  vcount = 0;
  for (i = 0; i < elem_count; i++) {
    if (!elements[i].active) continue;
    if (elements[i].type == ELEM_POINT)  ai[acount++] = i;
    if (elements[i].type == ELEM_VECTOR) vi[vcount++] = i;
  }

  if (acount == 0 && vcount == 0) {
    draw_str_clipped("No active elements.", 10, stat_y,
                     false, COLOR_DARK_GRAY, COLOR_WHITE);
    stat_y += SMALL_FONT_H + 4;
    draw_str_clipped("Press OK on an element in Input tab.", 10, stat_y,
                     false, COLOR_DARK_GRAY, COLOR_WHITE);
    draw_scrollbar(); return;
  }

  // ══════════════════════════════════════════════════════════════════════════
  // POINTS SECTION (only shown if there are active points)
  // ══════════════════════════════════════════════════════════════════════════

  if (acount == 0) {
    // No points — skip to vectors below
    goto vectors_section;
  }

  // ── 1 point ──────────────────────────────────────────────────────────────
  if (acount == 1) {
    a = &elements[ai[0]];
    snprintf(buf, sizeof(buf), "Point %s", a->label);
    stat_header(buf);
    fmtnum(a->x, v1); stat_line("x", v1);
    fmtnum(a->y, v2); stat_line("y", v2);
    if (vcount == 0) {
      stat_sep();
      draw_str_clipped("Select 2+ points for more stats.", 10, stat_y,
                       false, COLOR_DARK_GRAY, COLOR_WHITE);
    }
    goto vectors_section;
  }

  // ── 2 points ─────────────────────────────────────────────────────────────
  if (acount == 2) {
    a = &elements[ai[0]];
    b = &elements[ai[1]];

    snprintf(buf, sizeof(buf), "Points %s & %s", a->label, b->label);
    stat_header(buf);

    // Distance
    d = point_dist(a->x, a->y, b->x, b->y);
    fmtnum(d, v1);
    snprintf(lbl, sizeof(lbl), "|%s%s|", a->label, b->label);
    stat_line(lbl, v1);

    // Midpoint
    mx = (a->x + b->x) / 2.0f;
    my = (a->y + b->y) / 2.0f;
    fmtnum(mx, v1); fmtnum(my, v2);
    snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
    stat_line("Midpoint", buf);

    stat_sep();
    stat_header("Line AB");

    if (b->x != a->x) {
      slope     = (b->y - a->y) / (b->x - a->x);
      intercept = a->y - slope * a->x;
      build_slope_intercept(slope, intercept, buf, sizeof(buf));
      stat_line("Eq", buf);
      build_cartesian(slope, intercept, buf, sizeof(buf));
      stat_line("Cart.", buf);
    } else {
      fmtnum(a->x, v1);
      snprintf(buf, sizeof(buf), "x = %s", v1);
      stat_line("Eq", buf);
    }

    stat_sep();
    stat_header("Vectors");

    abx = b->x - a->x;
    aby = b->y - a->y;
    fmtnum(abx, v1); fmtnum(aby, v2);
    snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
    snprintf(lbl, sizeof(lbl), "%s%s", a->label, b->label);
    stat_line(lbl, buf);

    fmtnum(-abx, v1); fmtnum(-aby, v2);
    snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
    snprintf(lbl, sizeof(lbl), "%s%s", b->label, a->label);
    stat_line(lbl, buf);

    draw_scrollbar(); goto vectors_section;
  }

  // ── 3 points ─────────────────────────────────────────────────────────────
  if (acount == 3) {
    a = &elements[ai[0]];
    b = &elements[ai[1]];
    c = &elements[ai[2]];

    ab = point_dist(a->x, a->y, b->x, b->y);
    bc = point_dist(b->x, b->y, c->x, c->y);
    ca = point_dist(c->x, c->y, a->x, a->y);

    area = (a->x*(b->y - c->y) + b->x*(c->y - a->y) + c->x*(a->y - b->y)) / 2.0f;
    if (area < 0.0f) area = -area;
    collinear = (area < 0.005f);

    ang_a = 0.0f; ang_b = 0.0f; ang_c = 0.0f;
    if (!collinear) {
      ax = b->x - a->x; ay = b->y - a->y;  // AB
      bx = c->x - a->x; by = c->y - a->y;  // AC
      dot_a = ax*bx + ay*by;
      ang_a = facos_deg(dot_a / (ab * ca));

      cx2 = a->x - b->x; cy2 = a->y - b->y;  // BA
      dx2 = c->x - b->x; dy2 = c->y - b->y;  // BC
      dot_b = cx2*dx2 + cy2*dy2;
      ang_b = facos_deg(dot_b / (ab * bc));

      ang_c = 180.0f - ang_a - ang_b;
      if (ang_c < 0.0f) ang_c = 0.0f;
    }

    is_right  = (!collinear) &&
                ((ang_a > 89.5f && ang_a < 90.5f) ||
                 (ang_b > 89.5f && ang_b < 90.5f) ||
                 (ang_c > 89.5f && ang_c < 90.5f));
    eq_ab_bc  = (!collinear) && (ab - bc < 0.01f && ab - bc > -0.01f);
    eq_bc_ca  = (!collinear) && (bc - ca < 0.01f && bc - ca > -0.01f);
    eq_ab_ca  = (!collinear) && (ab - ca < 0.01f && ab - ca > -0.01f);

    if      (collinear)                                   tri_type = "-";
    else if (eq_ab_bc && eq_bc_ca)                        tri_type = "Equilateral";
    else if (is_right && (eq_ab_bc||eq_bc_ca||eq_ab_ca))  tri_type = "Right isosceles";
    else if (is_right)                                    tri_type = "Right triangle";
    else if (eq_ab_bc || eq_bc_ca || eq_ab_ca)            tri_type = "Isosceles";
    else                                                  tri_type = "Scalene";

    snprintf(buf, sizeof(buf), "Triangle %s%s%s", a->label, b->label, c->label);
    stat_header(buf);

    fmtnum(ab, v1);
    snprintf(buf, sizeof(buf), "%s%s", a->label, b->label);
    stat_line(buf, v1);
    fmtnum(bc, v1);
    snprintf(buf, sizeof(buf), "%s%s", b->label, c->label);
    stat_line(buf, v1);
    fmtnum(ca, v1);
    snprintf(buf, sizeof(buf), "%s%s", c->label, a->label);
    stat_line(buf, v1);

    stat_sep();
    stat_header("Angles");
    if (!collinear) {
      fmtnum(ang_a, v1);
      snprintf(buf, sizeof(buf), "%s deg", v1);
      snprintf(lbl, sizeof(lbl), "at %s", a->label);
      stat_line(lbl, buf);

      fmtnum(ang_b, v1);
      snprintf(buf, sizeof(buf), "%s deg", v1);
      snprintf(lbl, sizeof(lbl), "at %s", b->label);
      stat_line(lbl, buf);

      fmtnum(ang_c, v1);
      snprintf(buf, sizeof(buf), "%s deg", v1);
      snprintf(lbl, sizeof(lbl), "at %s", c->label);
      stat_line(lbl, buf);
    } else {
      stat_line("Angles", "N/A (collinear)");
    }

    stat_sep();
    stat_header("Area & Center");
    if (!collinear) {
      fmtnum(area, v1);
      stat_line("Area", v1);
    } else {
      stat_line("Area", "0 (collinear)");
    }
    gx = (a->x + b->x + c->x) / 3.0f;
    gy = (a->y + b->y + c->y) / 3.0f;
    fmtnum(gx, v1); fmtnum(gy, v2);
    snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
    stat_line("Centroid G", buf);

    stat_sep();
    stat_header("Classification");
    stat_line("Collinear?", collinear ? "Yes" : "No");
    if (!collinear) {
      stat_line("Type", tri_type);
      // Extra detail lines
      if (is_right)   stat_line("Has right angle?", "Yes");
      if (eq_ab_bc || eq_bc_ca || eq_ab_ca) stat_line("Has equal sides?", "Yes");
    }

    draw_scrollbar(); goto vectors_section;
  }

  // ── 4+ points ────────────────────────────────────────────────────────────
  snprintf(buf, sizeof(buf), "%d active points", acount);
  stat_header(buf);

  gx = 0.0f; gy = 0.0f;
  for (i = 0; i < acount; i++) {
    gx += elements[ai[i]].x;
    gy += elements[ai[i]].y;
  }
  gx /= (float)acount;
  gy /= (float)acount;
  fmtnum(gx, v1); fmtnum(gy, v2);
  snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
  stat_line("Centroid G", buf);

  stat_sep();
  draw_str_clipped("Select 2 or 3 pts for full stats.", 10, stat_y,
                   false, COLOR_DARK_GRAY, COLOR_WHITE);

  // ══════════════════════════════════════════════════════════════════════════
  // VECTORS SECTION (only shown if there are active vectors)
  // ══════════════════════════════════════════════════════════════════════════
vectors_section:
  if (vcount == 0) {
    draw_scrollbar();
    return;
  }

  // Separator between sections if we had points
  if (acount > 0) stat_sep();

  if (vcount == 1) {
    Element* va = &elements[vi[0]];
    float mag;
    float vdx = va->x, vdy = va->y;
    snprintf(buf, sizeof(buf), "Vector ->%s", va->label);
    stat_header(buf);
    fmtnum(vdx, v1); fmtnum(vdy, v2);
    snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
    stat_line("Components", buf);
    // Magnitude: sqrt(dx^2 + dy^2)
    mag = fsqrt(vdx * vdx + vdy * vdy);
    fmtnum(mag, v1);
    stat_line("Length |v|", v1);
    if (va->has_origin) {
      fmtnum(va->ox, v1); fmtnum(va->oy, v2);
      snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
      stat_line("Origin", buf);
    }

  } else if (vcount == 2) {
    Element* va = &elements[vi[0]];
    Element* vb = &elements[vi[1]];
    float mag_a, mag_b, det;
    float vdx_a = va->x, vdy_a = va->y;
    float vdx_b = vb->x, vdy_b = vb->y;

    snprintf(buf, sizeof(buf), "Vector ->%s", va->label);
    stat_header(buf);
    fmtnum(vdx_a, v1); fmtnum(vdy_a, v2);
    snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
    stat_line("Components", buf);
    mag_a = fsqrt(vdx_a * vdx_a + vdy_a * vdy_a);
    fmtnum(mag_a, v1);
    stat_line("Length |v|", v1);
    if (va->has_origin) {
      fmtnum(va->ox, v1); fmtnum(va->oy, v2);
      snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
      stat_line("Origin", buf);
    }

    stat_sep();

    snprintf(buf, sizeof(buf), "Vector ->%s", vb->label);
    stat_header(buf);
    fmtnum(vdx_b, v1); fmtnum(vdy_b, v2);
    snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
    stat_line("Components", buf);
    mag_b = fsqrt(vdx_b * vdx_b + vdy_b * vdy_b);
    fmtnum(mag_b, v1);
    stat_line("Length |v|", v1);
    if (vb->has_origin) {
      fmtnum(vb->ox, v1); fmtnum(vb->oy, v2);
      snprintf(buf, sizeof(buf), "(%s, %s)", v1, v2);
      stat_line("Origin", buf);
    }

    stat_sep();
    stat_header("Relations");
    // Determinant: dx_a*dy_b - dy_a*dx_b
    det = vdx_a * vdy_b - vdy_a * vdx_b;
    fmtnum(det, v1);
    stat_line("Det(u,v)", v1);
    if (det > -0.001f && det < 0.001f)
      stat_line("Parallel?", "Yes (det=0)");

  } else {
    // 3+ vectors: just list each with components and length
    int j;
    Element* vj;
    float vmag;
    snprintf(buf, sizeof(buf), "%d active vectors", vcount);
    stat_header(buf);
    for (j = 0; j < vcount; j++) {
      vj = &elements[vi[j]];
      vmag = fsqrt(vj->x * vj->x + vj->y * vj->y);
      fmtnum(vj->x, v1); fmtnum(vj->y, v2);
      snprintf(lbl, sizeof(lbl), "->%s", vj->label);
      snprintf(buf, sizeof(buf), "(%s,%s)", v1, v2);
      stat_line(lbl, buf);
      fmtnum(vmag, v1);
      snprintf(lbl, sizeof(lbl), "|->%s|", vj->label);
      stat_line(lbl, v1);
      if (j < vcount - 1) stat_sep();
    }
    stat_sep();
    draw_str_clipped("Select 2 vectors for det.", 10, stat_y,
                     false, COLOR_DARK_GRAY, COLOR_WHITE);
  }

  draw_scrollbar();
}

void stats_reset_scroll(void) { stat_scroll = 0; }
void handle_stats_event(eadk_event_t ev) {
  int max_scroll;
  switch (ev) {
    case eadk_event_down:
      max_scroll = stat_total_h - CONTENT_H;
      if (max_scroll > 0 && stat_scroll < max_scroll) {
        stat_scroll += 16;
        if (stat_scroll > max_scroll) stat_scroll = max_scroll;
        mark_dirty(DIRTY_CONTENT);
      }
      break;
    case eadk_event_up:
      if (stat_scroll > 0) {
        stat_scroll -= 16;
        if (stat_scroll < 0) stat_scroll = 0;
        mark_dirty(DIRTY_CONTENT);
      }
      break;
    case eadk_event_left:
    case eadk_event_back:
      stat_scroll = 0;  // reset scroll when leaving
      current_tab = TAB_GRAPH;
      mark_dirty(DIRTY_TABS | DIRTY_CONTENT | DIRTY_TOOLBAR);
      break;
    default: break;
  }
}