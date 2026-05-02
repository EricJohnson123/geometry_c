#ifndef ELEMENTS_H
#define ELEMENTS_H

#include <stdbool.h>

#define MAX_ELEMENTS 6

typedef enum {
  ELEM_POINT = 0,
  ELEM_VECTOR,
} ElementType;

typedef struct {
  ElementType type;
  bool        active;     // shown in graph & stats
  char        label[4];   // "A", "B", … for points; "u", "v", … for vectors

  // ELEM_POINT: position
  // ELEM_VECTOR: components (dx, dy) stored in x, y
  float       x;
  float       y;

  // ELEM_VECTOR only
  float       ox;         // origin x (default 0)
  float       oy;         // origin y (default 0)
  bool        has_origin; // whether ox/oy were explicitly set
} Element;

extern Element elements[MAX_ELEMENTS];
extern int     elem_count;

// Utilities
void  ftoa(float v, char* buf, int decimals);

// Smart formatter: whole numbers without decimals ("2"), fractions strip
// trailing zeros ("2.5"), max 3 decimal places. Handles negatives.
void  fmtnum(float v, char* buf);

float simple_atof(const char* s);

// make_point_label: fills out with "A", "B", … based on point count
void  make_point_label(int point_idx, char* out);

// make_vector_label: fills out with "u", "v", … based on vector count
void  make_vector_label(int vec_idx, char* out);

void  buf_append(char* buf, char c, int max_len);
void  buf_backspace(char* buf);

#endif