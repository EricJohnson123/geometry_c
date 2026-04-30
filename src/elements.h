#ifndef ELEMENTS_H
#define ELEMENTS_H

#include <stdbool.h>

#define MAX_ELEMENTS 12

typedef enum {
  ELEM_POINT = 0,
} ElementType;

typedef struct {
  ElementType type;
  bool        active;   // shown in graph & stats
  char        label[4]; // "A", "B", …
  float       x;
  float       y;
} Element;

extern Element elements[MAX_ELEMENTS];
extern int     elem_count;

// Utilities
void  ftoa(float v, char* buf, int decimals);

// Smart formatter: whole numbers without decimals ("2"), fractions strip
// trailing zeros ("2.5"), max 3 decimal places. Handles negatives.
void  fmtnum(float v, char* buf);

float simple_atof(const char* s);
void  make_label(int idx, char* out);

void  buf_append(char* buf, char c, int max_len);
void  buf_backspace(char* buf);

#endif