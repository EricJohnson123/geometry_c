#ifndef ELEMENTS_H
#define ELEMENTS_H

#include <stdbool.h>

#define MAX_ELEMENTS 12

typedef enum {
  ELEM_POINT = 0,
  ELEM_VECTOR,
  ELEM_LINE,
} ElementType;

typedef struct {
  ElementType type;
  bool        active;     // shown in graph & stats
  char        label[4];   // "A", "B", … for points; "u", "v", … for vectors

  // ELEM_POINT:  position (x, y)
  // ELEM_VECTOR: components (dx, dy) in x, y; origin in ox, oy
  // ELEM_LINE:   ax + by + c = 0  stored as x=a, y=b, ox=c
  float       x;
  float       y;
  float       ox;         // vectors: origin x  |  lines: c coefficient
  float       oy;         // vectors: origin y
  bool        has_origin; // vectors: whether ox/oy were explicitly set

  // Lines only — remember original entry form for display
  bool        si_form;    // true = was entered as y = mx + b
  float       si_m;       // original slope     (si_form only)
  float       si_b;       // original intercept (si_form only)
} Element;

extern Element elements[MAX_ELEMENTS];
extern int     elem_count;

// Utilities
void  ftoa(float v, char* buf, int decimals);
void  fmtnum(float v, char* buf);
float simple_atof(const char* s);

void  make_label(int idx, char* out);
void  make_point_label(int point_idx, char* out);
void  make_vector_label(int vec_idx, char* out);
void  make_line_label(int line_idx, char* out);

void  buf_append(char* buf, char c, int max_len);
void  buf_backspace(char* buf);

int   count_type(ElementType type);

#endif