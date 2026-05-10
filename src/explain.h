#ifndef EXPLAIN_H
#define EXPLAIN_H

#include <stdbool.h>
#include "eadk.h"

typedef enum {
  EXPLAIN_NONE = 0,
  EXPLAIN_DISTANCE,
  EXPLAIN_MIDPOINT,
  EXPLAIN_SLOPE,
  EXPLAIN_LINE_SI,
  EXPLAIN_LINE_CART,
  EXPLAIN_VECTOR_MAG,
  EXPLAIN_CENTROID,
} ExplanationType;

#define MAX_EXPLAIN_STEPS  12
#define MAX_EXPLAIN_WIDTH  100

typedef struct {
  char text[MAX_EXPLAIN_WIDTH];
  int  indent;
} ExplainStep;

/* Two-point explanations (share same signature) */
int generate_distance_explanation(float x1, float y1, float x2, float y2,
                                   const char* lbl1, const char* lbl2,
                                   ExplainStep* steps);
int generate_midpoint_explanation(float x1, float y1, float x2, float y2,
                                   const char* lbl1, const char* lbl2,
                                   ExplainStep* steps);
int generate_slope_explanation(float x1, float y1, float x2, float y2,
                                const char* lbl1, const char* lbl2,
                                ExplainStep* steps);
int generate_line_si_explanation(float x1, float y1, float x2, float y2,
                                  const char* lbl1, const char* lbl2,
                                  ExplainStep* steps);
int generate_line_cart_explanation(float x1, float y1, float x2, float y2,
                                    const char* lbl1, const char* lbl2,
                                    ExplainStep* steps);

/* Vector: dx/dy are the components, lbl1+lbl2 name the endpoints */
int generate_vector_mag_explanation(float dx, float dy,
                                     const char* lbl1, const char* lbl2,
                                     ExplainStep* steps);

/* Centroid: three source points */
int generate_centroid_explanation(float x1, float y1, float x2, float y2,
                                   float x3, float y3,
                                   const char* lbl1, const char* lbl2,
                                   const char* lbl3, ExplainStep* steps);

void render_math_step(const char* text, int x, int y,
                      bool large, eadk_color_t fg, eadk_color_t bg);
int  calc_math_width(const char* text, bool large);

/* Sum of (math_expr_height(steps[i].text) + MATH_LINE_GAP) for all steps.
   Defined here because ExplainStep is fully known in this header. */
int math_steps_total_height(const ExplainStep* steps, int n_steps);

#endif