#include "elements.h"
#include <string.h>
#include <stdbool.h>

Element elements[MAX_ELEMENTS];
int     elem_count = 0;

void ftoa(float v, char* buf, int decimals) {
  if (v < 0.0f) { *buf++ = '-'; v = -v; }
  int ipart = (int)v;
  float fpart = v - (float)ipart;
  if (ipart == 0) {
    *buf++ = '0';
  } else {
    char tmp[12]; int ti = 0;
    int ip = ipart;
    while (ip > 0) { tmp[ti++] = '0' + (ip % 10); ip /= 10; }
    for (int i = ti - 1; i >= 0; i--) *buf++ = tmp[i];
  }
  if (decimals > 0) {
    *buf++ = '.';
    for (int i = 0; i < decimals; i++) {
      fpart *= 10.0f;
      int d = (int)fpart;
      *buf++ = '0' + d;
      fpart -= (float)d;
    }
  }
  *buf = '\0';
}

void fmtnum(float v, char* buf) {
  long scaled, ipart, fpart, ip;
  char tmp[12], fdig[4];
  int ti, i, last;

  if (v < 0.0f) { *buf++ = '-'; v = -v; }

  scaled = (long)(v * 1000.0f + 0.5f);
  ipart  = scaled / 1000;
  fpart  = scaled % 1000;

  if (ipart == 0) {
    *buf++ = '0';
  } else {
    ti = 0; ip = ipart;
    while (ip > 0) { tmp[ti++] = '0' + (int)(ip % 10); ip /= 10; }
    for (i = ti - 1; i >= 0; i--) *buf++ = tmp[i];
  }

  if (fpart != 0) {
    *buf++ = '.';
    fdig[0] = '0' + (int)((fpart / 100) % 10);
    fdig[1] = '0' + (int)((fpart /  10) % 10);
    fdig[2] = '0' + (int)( fpart        % 10);
    fdig[3] = '\0';
    last = 2;
    while (last > 0 && fdig[last] == '0') last--;
    for (i = 0; i <= last; i++) *buf++ = fdig[i];
  }

  *buf = '\0';
}

float simple_atof(const char* s) {
  float result = 0.0f, frac = 0.0f, fdiv = 1.0f;
  bool neg = false, in_frac = false;
  if (*s == '-') { neg = true; s++; }
  while (*s) {
    if (*s == '.') { in_frac = true; }
    else if (*s >= '0' && *s <= '9') {
      if (!in_frac) result = result * 10.0f + (float)(*s - '0');
      else          { fdiv *= 10.0f; frac += (float)(*s - '0') / fdiv; }
    }
    s++;
  }
  result += frac;
  return neg ? -result : result;
}

void make_label(int idx, char* out) {
  out[0] = 'A' + (char)(idx % 26);
  out[1] = '\0';
}

void buf_append(char* buf, char c, int max_len) {
  int l = (int)strlen(buf);
  if (l < max_len - 1) { buf[l] = c; buf[l + 1] = '\0'; }
}

void buf_backspace(char* buf) {
  int l = (int)strlen(buf);
  if (l > 0) buf[l - 1] = '\0';
}