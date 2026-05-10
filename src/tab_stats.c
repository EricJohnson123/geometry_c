#include "tab_stats.h"
#include "ui.h"
#include "elements.h"
#include "tab_graph.h"
#include "explain.h"
#include "math_render.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// ─── Math helpers ─────────────────────────────────────────────────────────────
static float fsqrt(float s){
  int i;float r;if(s<=0.0f)return 0.0f;r=s/2.0f;for(i=0;i<24;i++)r=(r+s/r)/2.0f;return r;
}
static float point_dist(float x1,float y1,float x2,float y2){
  float dx=x2-x1,dy=y2-y1;return fsqrt(dx*dx+dy*dy);
}
static float facos_deg(float x){
  float result,y,y3,y5,y7,asin_y,x3,x5,x7,asin_x;
  if(x>1.0f) x=1.0f;
  if(x<-1.0f) x=-1.0f;
  if(x<-0.7f||x>0.7f){
    y=fsqrt((1.0f-x)/2.0f);y3=y*y*y;y5=y3*y*y;y7=y5*y*y;
    asin_y=y+y3/6.0f+3.0f*y5/40.0f+15.0f*y7/336.0f;result=2.0f*asin_y;
  }else{x3=x*x*x;x5=x3*x*x;x7=x5*x*x;
    asin_x=x+x3/6.0f+3.0f*x5/40.0f+15.0f*x7/336.0f;result=1.5707963f-asin_x;}
  return result*57.29578f;
}

// ─── Rational arithmetic ──────────────────────────────────────────────────────
static long gcd(long a,long b){long t;if(a<0)a=-a;if(b<0)b=-b;while(b){t=b;b=a%b;a=t;}return a?a:1;}
static void float_to_frac(float v,long*num,long*den){
  long neg,a,h0=1,h1=0,k0=0,k1=1,h,k,g;float x,rem;
  neg=(v<0.0f)?1:0;if(neg)v=-v;x=v;
  while(1){a=(long)x;h=a*h0+h1;k=a*k0+k1;if(k>1000)break;
    h1=h0;h0=h;k1=k0;k0=k;rem=x-(float)a;if(rem<1e-5f)break;x=1.0f/rem;}
  g=gcd(h0,k0);*num=neg?-(h0/g):(h0/g);*den=k0/g;
}

// ─── Equation builders ────────────────────────────────────────────────────────
static void fmt_frac(long num,long den,char*buf,int cm){
  if(num==0){buf[0]='\0';return;}
  if(cm){if(num==den){buf[0]='\0';return;}if(num==-den){buf[0]='-';buf[1]='\0';return;}}
  if(den==1){snprintf(buf,16,"%ld",num);return;}snprintf(buf,16,"%ld/%ld",num,den);
}
static void build_slope_intercept(float slope,float intercept,char*out,int maxlen){
  long sn,sd,bn,bd,g;char ms[16],bs[16];char sign;float ab;
  float_to_frac(slope,&sn,&sd);fmt_frac(sn,sd,ms,1);
  if(intercept==0.0f){snprintf(out,maxlen,"y=%sx",ms);return;}
  ab=intercept<0.0f?-intercept:intercept;
  float_to_frac(ab,&bn,&bd);g=gcd(bn<0?-bn:bn,bd);bn/=g;bd/=g;
  fmt_frac(bn,bd,bs,0);sign=intercept<0.0f?'-':'+';
  snprintf(out,maxlen,"y=%sx%c%s",ms,sign,bs);
}
static void build_cartesian_from_abc(float fa,float fb,float fc,char*out,int maxlen){
  long na,da,nb,db,nc,dc,scale,g,t,cx,cy,cc;char tmp[48];int pos;float aa,ab2,ac2;
  aa=fa<0.0f?-fa:fa;ab2=fb<0.0f?-fb:fb;ac2=fc<0.0f?-fc:fc;
  float_to_frac(aa,&na,&da);if(fa<0.0f)na=-na;
  float_to_frac(ab2,&nb,&db);if(fb<0.0f)nb=-nb;
  float_to_frac(ac2,&nc,&dc);if(fc<0.0f)nc=-nc;
  g=gcd(da,db);scale=(da/g)*db;g=gcd(scale,dc);scale=(scale/g)*dc;
  cx=na*(scale/da);cy=nb*(scale/db);cc=nc*(scale/dc);
  {long aa2=cx<0?-cx:cx,bb2=cy<0?-cy:cy,cc2=cc<0?-cc:cc;
   g=aa2?aa2:1;if(bb2){t=gcd(g,bb2);g=t;}if(cc2){t=gcd(g,cc2);g=t;}if(g==0)g=1;}
  cx/=g;cy/=g;cc/=g;
  if(cx<0||(cx==0&&cy<0)){cx=-cx;cy=-cy;cc=-cc;}
  tmp[0]='\0';pos=0;
  if(cx==1)pos+=snprintf(tmp+pos,sizeof(tmp)-pos,"x");
  else if(cx==-1)pos+=snprintf(tmp+pos,sizeof(tmp)-pos,"-x");
  else if(cx!=0)pos+=snprintf(tmp+pos,sizeof(tmp)-pos,"%ldx",cx);
  if(cy!=0){if(pos>0){if(cy>0)pos+=snprintf(tmp+pos,sizeof(tmp)-pos,"+");
    else{pos+=snprintf(tmp+pos,sizeof(tmp)-pos,"-");cy=-cy;}}
    if(cy==1)pos+=snprintf(tmp+pos,sizeof(tmp)-pos,"y");
    else pos+=snprintf(tmp+pos,sizeof(tmp)-pos,"%ldy",cy);}
  if(cc!=0){if(pos>0){if(cc>0)pos+=snprintf(tmp+pos,sizeof(tmp)-pos,"+");
    else{pos+=snprintf(tmp+pos,sizeof(tmp)-pos,"-");cc=-cc;}}
    pos+=snprintf(tmp+pos,sizeof(tmp)-pos,"%ld",cc);}
  snprintf(out,maxlen,"%s=0",tmp);
}
static void build_line_through_point_dir(float px,float py,float dx,float dy,
                                         char*si_out,int si_len,char*cart_out,int cart_len){
  char tmp[12];float sl,ic;
  if(dx==0.0f){fmtnum(px,tmp);snprintf(si_out,si_len,"x=%s",tmp);snprintf(cart_out,cart_len,"x=%s",tmp);return;}
  sl=dy/dx;ic=py-sl*px;
  build_slope_intercept(sl,ic,si_out,si_len);
  build_cartesian_from_abc(sl,-1.0f,ic,cart_out,cart_len);
}

// ─── Snapshot ─────────────────────────────────────────────────────────────────
static Element snap_elements[MAX_ELEMENTS];
static int     snap_elem_count=0;
static void refresh_snapshot(void){
  int i;snap_elem_count=elem_count;for(i=0;i<elem_count;i++)snap_elements[i]=elements[i];
}

// ─── Addable slots ────────────────────────────────────────────────────────────
#define MAX_ADDABLE     32
#define STAT_BOTTOM_PAD 10
#define BTN_SZ           14
#define BTN_MARGIN        6
#define BTN_X            (SCREEN_W - BTN_SZ - BTN_MARGIN)

typedef struct{
  ElementType type;
  float x,y,ox;
  bool  si_form;float si_m,si_b;
  int   subkey;
  bool  added;int elem_idx;
  int   row_content_y;
  int   abs_y;           /* scroll-independent y: stat_y + stat_scroll at draw time */
  
  // Explanation support
  bool  has_explanation;
  bool  is_explain_only;  // true = no + button, never commit to canvas
  ExplanationType explain_type;
  float src_x1, src_y1, src_x2, src_y2;
  char  src_label1[4];
  char  src_label2[4];
  float src_x3, src_y3;
  char  src_label3[4];
} Addable;

static Addable addables[MAX_ADDABLE];
static int     addable_count=0;
static int     stat_cursor=-1;
static int     stat_cursor_screen_y=-1;  // actual on-screen Y of cursor row

// ─── Explanation State ────────────────────────────────────────────────────────
static bool showing_explanation = false;
static int  explain_scroll_y = 0;
static int  explain_scroll_x = 0;
static int  explain_target_idx = -1;
static bool cursor_on_help = false;

// Serial counters — only ever increment
static int stat_point_serial=0;
static int stat_vec_serial=0;
static int stat_line_serial=0;

// ─── Persistence ─────────────────────────────────────────────────────────────
typedef struct{ElementType type;float x,y,ox;int subkey;bool added;int elem_idx;} AddPersist;
static AddPersist persist[MAX_ADDABLE];
static int        persist_count=0;

static void persist_save(void){
  int i;persist_count=addable_count;
  for(i=0;i<addable_count;i++){
    persist[i].type    =addables[i].type;persist[i].x=addables[i].x;
    persist[i].y       =addables[i].y;persist[i].ox=addables[i].ox;
    persist[i].subkey  =addables[i].subkey;
    persist[i].added   =addables[i].added;persist[i].elem_idx=addables[i].elem_idx;
  }
}
static bool persist_lookup(int slot,int*elem_idx_out){
  int j;
  if(slot<0||slot>=MAX_ADDABLE){*elem_idx_out=-1;return false;}
  for(j=0;j<persist_count;j++){
    if(persist[j].type  ==addables[slot].type &&
       persist[j].x     ==addables[slot].x    &&
       persist[j].y     ==addables[slot].y    &&
       persist[j].ox    ==addables[slot].ox   &&
       persist[j].subkey==addables[slot].subkey){
      *elem_idx_out=persist[j].elem_idx;
      addables[slot].added    =persist[j].added;
      addables[slot].elem_idx =persist[j].elem_idx;
      return persist[j].added;
    }
  }
  *elem_idx_out=-1;return false;
}

// ─── Row drawing ─────────────────────────────────────────────────────────────
static int stat_y,stat_scroll,stat_total_h;
#define STAT_BASE_Y (CONTENT_Y+4)

static void stat_line_plain(const char*label,const char*value){
  int slot;
  bool is_cur;
  bool should_draw;
  
  stat_total_h+=SMALL_FONT_H+2;
  should_draw = (stat_y+SMALL_FONT_H>CONTENT_Y && stat_y<CONTENT_BOTTOM);
  
  slot = addable_count;
  if (slot < MAX_ADDABLE) {
    addables[slot].type = ELEM_POINT;  // dummy, never committed
    addables[slot].x = 0;
    addables[slot].y = 0;
    addables[slot].added = false;
    addables[slot].elem_idx = -1;
    addables[slot].has_explanation = false;
    addables[slot].is_explain_only = true;  // can't add, no +
    addables[slot].abs_y = stat_y + stat_scroll;
    addable_count++;
  }
  
  /* Track cursor position regardless of visibility */
  if(slot == stat_cursor) stat_cursor_screen_y = stat_y;

  if(!should_draw) {
    stat_y+=SMALL_FONT_H+2;
    return;
  }
  
  is_cur = (slot == stat_cursor);
  
  if(is_cur) draw_str_clipped(">",2,stat_y,false,COLOR_ORANGE,COLOR_WHITE);
  draw_str_clipped(label,14,stat_y,false,COLOR_DARK_GRAY,COLOR_WHITE);
  draw_str_clipped(value,130,stat_y,false,COLOR_BLACK,COLOR_WHITE);
  
  stat_y+=SMALL_FONT_H+2;
}

static void stat_line_explain(const char*label, const char*value,
                              bool has_expl, ExplanationType expl_type,
                              float sx1, float sy1, float sx2, float sy2,
                              const char*lbl1, const char*lbl2) {
  int slot;
  bool is_cur;
  bool should_draw;
  
  stat_total_h += SMALL_FONT_H + 2;
  should_draw = (stat_y + SMALL_FONT_H > CONTENT_Y && stat_y < CONTENT_BOTTOM);
  
  slot = addable_count;
  if (slot < MAX_ADDABLE) {
    addables[slot].type = ELEM_POINT;
    addables[slot].x = 0;
    addables[slot].y = 0;
    addables[slot].added = false;
    addables[slot].elem_idx = -1;
    addables[slot].has_explanation = has_expl;
    addables[slot].is_explain_only = true;
    addables[slot].explain_type = expl_type;
    addables[slot].src_x1 = sx1;
    addables[slot].src_y1 = sy1;
    addables[slot].src_x2 = sx2;
    addables[slot].src_y2 = sy2;
    strncpy(addables[slot].src_label1, lbl1 ? lbl1 : "A", 3);
    addables[slot].src_label1[3] = '\0';
    strncpy(addables[slot].src_label2, lbl2 ? lbl2 : "B", 3);
    addables[slot].src_label2[3] = '\0';
    addables[slot].src_x3 = 0.0f;
    addables[slot].src_y3 = 0.0f;
    addables[slot].src_label3[0] = '\0';
    addables[slot].abs_y = stat_y + stat_scroll;
    addable_count++;
  }

  /* Track cursor position regardless of visibility */
  if(slot == stat_cursor) stat_cursor_screen_y = stat_y;

  if(!should_draw) {
    stat_y += SMALL_FONT_H + 2;
    return;
  }
  
  is_cur = (slot == stat_cursor);
  
  if(is_cur) draw_str_clipped(">",2,stat_y,false,COLOR_ORANGE,COLOR_WHITE);
  draw_str_clipped(label, 14, stat_y, false, COLOR_DARK_GRAY, COLOR_WHITE);
  draw_str_clipped(value, 130, stat_y, false, COLOR_BLACK, COLOR_WHITE);
  
  // Draw ? on the right
  if (has_expl) {
    draw_str_clipped("?", 290, stat_y, false, COLOR_DARK_GRAY, COLOR_WHITE);
  }
  
  stat_y += SMALL_FONT_H + 2;
}

static void stat_line_add(const char*label, const char*value,
                          ElementType type, float x, float y, float ox,
                          bool si_form, float si_m, float si_b, int subkey,
                          bool has_expl, ExplanationType expl_type,
                          float sx1, float sy1, float sx2, float sy2,
                          const char*lbl1, const char*lbl2) {
  int slot,dummy;
  bool is_cur,added;
  bool should_draw;
  
  stat_total_h+=SMALL_FONT_H+2;
  should_draw = (stat_y+SMALL_FONT_H>CONTENT_Y && stat_y<CONTENT_BOTTOM);
  
  slot=addable_count;
  if(slot<MAX_ADDABLE){
    addables[slot].type         =type;
    addables[slot].x            =x;addables[slot].y=y;addables[slot].ox=ox;
    addables[slot].si_form      =si_form;addables[slot].si_m=si_m;addables[slot].si_b=si_b;
    addables[slot].subkey       =subkey;
    addables[slot].row_content_y=stat_y-CONTENT_Y;
    addables[slot].added        =false;addables[slot].elem_idx=-1;
    addables[slot].has_explanation = has_expl;
    addables[slot].is_explain_only = false;
    addables[slot].explain_type = expl_type;
    addables[slot].src_x1 = sx1;
    addables[slot].src_y1 = sy1;
    addables[slot].src_x2 = sx2;
    addables[slot].src_y2 = sy2;
    strncpy(addables[slot].src_label1, lbl1 ? lbl1 : "A", 3);
    addables[slot].src_label1[3] = '\0';
    strncpy(addables[slot].src_label2, lbl2 ? lbl2 : "B", 3);
    addables[slot].src_label2[3] = '\0';
    addables[slot].src_x3 = 0.0f;
    addables[slot].src_y3 = 0.0f;
    addables[slot].src_label3[0] = '\0';
    addables[slot].abs_y = stat_y + stat_scroll;
    addable_count++;
    added=persist_lookup(slot,&dummy);
    addables[slot].added = added;
    addables[slot].elem_idx = (added ? dummy : -1);
  }else{added=false;}

  /* Track cursor position regardless of visibility */
  if(slot == stat_cursor && slot < MAX_ADDABLE) stat_cursor_screen_y = stat_y;

  if(!should_draw) {
    stat_y+=SMALL_FONT_H+2;
    return;
  }

  is_cur=(slot==stat_cursor&&slot<MAX_ADDABLE);
  
  if(is_cur)draw_str_clipped(">",2,stat_y,false,COLOR_ORANGE,COLOR_WHITE);
  draw_str_clipped(label,14,stat_y,false,COLOR_DARK_GRAY,COLOR_WHITE);
  draw_str_clipped(value,130,stat_y,false,COLOR_BLACK,COLOR_WHITE);

  // Draw symbols on right: +/- and ?
  int sym_x = 280;
  if(added){
    draw_str_clipped("-",sym_x,stat_y,false,COLOR_DARK_GRAY,COLOR_WHITE);
  }else{
    draw_str_clipped("+",sym_x,stat_y,false,COLOR_DARK_GRAY,COLOR_WHITE);
  }
  if(has_expl){
    draw_str_clipped("?",sym_x+10,stat_y,false,COLOR_DARK_GRAY,COLOR_WHITE);
  }
  
  stat_y+=SMALL_FONT_H+2;
}

static void stat_sep(void){
  stat_total_h+=5;
  if(stat_y>=CONTENT_BOTTOM){stat_y+=5;return;}
  if(stat_y+4>=CONTENT_Y)draw_hline_clipped(10,stat_y+1,SCREEN_W-22,COLOR_LIGHT_GRAY);
  stat_y+=5;
}
static void stat_header(const char*text){
  int hw;stat_total_h+=SMALL_FONT_H+3;
  if(stat_y>=CONTENT_BOTTOM){stat_y+=SMALL_FONT_H+3;return;}
  if(stat_y+SMALL_FONT_H>=CONTENT_Y){
    hw=(int)strlen(text)*SMALL_FONT_W+12;
    fill_rect_clipped(10,stat_y-1,hw,SMALL_FONT_H+2,COLOR_ADD_ROW_BG);
    draw_str_clipped(text,14,stat_y,false,COLOR_DARK_GRAY,COLOR_ADD_ROW_BG);
  }
  stat_y+=SMALL_FONT_H+3;
}
/* Set third source point for centroid explanation — call after stat_line_add */
static void set_explain_extra(float x3, float y3, const char* lbl3) {
  int last;
  if (addable_count <= 0) return;
  last = addable_count - 1;
  addables[last].src_x3 = x3;
  addables[last].src_y3 = y3;
  strncpy(addables[last].src_label3, lbl3 ? lbl3 : "", 3);
  addables[last].src_label3[3] = '\0';
}

/* Dispatch to the right generator and return step count */
static int generate_explanation_steps(Addable* a, ExplainStep* steps) {
  switch (a->explain_type) {
    case EXPLAIN_DISTANCE:
      return generate_distance_explanation(a->src_x1, a->src_y1, a->src_x2, a->src_y2,
                                           a->src_label1, a->src_label2, steps);
    case EXPLAIN_MIDPOINT:
      return generate_midpoint_explanation(a->src_x1, a->src_y1, a->src_x2, a->src_y2,
                                           a->src_label1, a->src_label2, steps);
    case EXPLAIN_SLOPE:
      return generate_slope_explanation(a->src_x1, a->src_y1, a->src_x2, a->src_y2,
                                        a->src_label1, a->src_label2, steps);
    case EXPLAIN_LINE_SI:
      return generate_line_si_explanation(a->src_x1, a->src_y1, a->src_x2, a->src_y2,
                                          a->src_label1, a->src_label2, steps);
    case EXPLAIN_LINE_CART:
      return generate_line_cart_explanation(a->src_x1, a->src_y1, a->src_x2, a->src_y2,
                                            a->src_label1, a->src_label2, steps);
    case EXPLAIN_VECTOR_MAG:
      return generate_vector_mag_explanation(a->src_x1, a->src_y1,
                                             a->src_label1, a->src_label2, steps);
    case EXPLAIN_CENTROID:
      return generate_centroid_explanation(a->src_x1, a->src_y1, a->src_x2, a->src_y2,
                                           a->src_x3, a->src_y3,
                                           a->src_label1, a->src_label2, a->src_label3,
                                           steps);
    default: return 0;
  }
}

static const char* explain_title(ExplanationType t) {
  switch (t) {
    case EXPLAIN_DISTANCE:   return "Distance formula";
    case EXPLAIN_MIDPOINT:   return "Midpoint formula";
    case EXPLAIN_SLOPE:      return "Slope formula";
    case EXPLAIN_LINE_SI:    return "Line equation";
    case EXPLAIN_LINE_CART:  return "Cartesian form";
    case EXPLAIN_VECTOR_MAG: return "Vector magnitude";
    case EXPLAIN_CENTROID:   return "Centroid formula";
    default:                 return "Explanation";
  }
}

static void draw_scrollbar(void){
  int track_h,thumb_h,thumb_y,tx;
  if(stat_total_h<=CONTENT_H)return;
  tx=SCREEN_W-4;track_h=CONTENT_H;
  thumb_h=track_h*CONTENT_H/stat_total_h;if(thumb_h<8)thumb_h=8;
  thumb_y=CONTENT_Y+(track_h-thumb_h)*stat_scroll/(stat_total_h-CONTENT_H);
  fill_rect(tx,CONTENT_Y,3,track_h,COLOR_LIGHT_GRAY);
  fill_rect(tx,thumb_y,3,thumb_h,COLOR_DARK_GRAY);
}

// ─── Draw Explanation Overlay ─────────────────────────────────────────────────
static void draw_explanation_overlay(void) {
  ExplainStep steps[MAX_EXPLAIN_STEPS];
  int num_steps = 0;
  int i, y_line, total_height, step_h, cursor_y;
  Addable* a;
  const char* title;

  /* Layout constants for the overlay */
#define OVL_HEADER_H  18
#define OVL_FOOTER_H  16
#define OVL_CONTENT_Y OVL_HEADER_H
#define OVL_CONTENT_BOTTOM (SCREEN_H - OVL_FOOTER_H)
#define OVL_FIRST_STEP_Y   (OVL_CONTENT_Y + 4)

  if (explain_target_idx < 0 || explain_target_idx >= addable_count) return;
  a = &addables[explain_target_idx];
  if (a->explain_type == EXPLAIN_NONE) return;

  num_steps = generate_explanation_steps(a, steps);
  if (num_steps == 0) return;

  title = explain_title(a->explain_type);
  total_height = math_steps_total_height(steps, num_steps);

  /* Clear full screen */
  fill_rect(0, 0, SCREEN_W, SCREEN_H, COLOR_WHITE);

  /* Draw steps — variable height per step */
  cursor_y = OVL_FIRST_STEP_Y - explain_scroll_y;
  for (i = 0; i < num_steps; i++) {
    const char* next_text = (i + 1 < num_steps) ? steps[i + 1].text : NULL;
    step_h = math_expr_height(steps[i].text);
    y_line = cursor_y;
    /* Skip steps fully above the visible area */
    if (y_line + step_h >= OVL_CONTENT_Y &&
        y_line         <  OVL_CONTENT_BOTTOM) {
      draw_math_expr(steps[i].text, 8, y_line, COLOR_BLACK, COLOR_WHITE);
    }
    cursor_y += step_h + math_step_gap(steps[i].text, next_text);
    if (cursor_y >= OVL_CONTENT_BOTTOM) break;
  }

  /* Header drawn over steps */
  fill_rect(0, 0, SCREEN_W, OVL_HEADER_H, COLOR_ORANGE);
  draw_str(title, 6, 2, false, COLOR_WHITE, COLOR_ORANGE);
  draw_str("Back", SCREEN_W - 32, 2, false, COLOR_WHITE, COLOR_ORANGE);

  /* Footer drawn over steps */
  fill_rect(0, SCREEN_H - OVL_FOOTER_H, SCREEN_W, OVL_FOOTER_H, COLOR_LIGHT_GRAY);
  draw_str("Back: close", 6, SCREEN_H - OVL_FOOTER_H + 2, false,
           COLOR_DARK_GRAY, COLOR_LIGHT_GRAY);

  /* Scrollbar */
  {
    int visible_h = OVL_CONTENT_BOTTOM - OVL_CONTENT_Y;
    if (total_height > visible_h) {
      int track_h = visible_h;
      int thumb_h = track_h * visible_h / total_height;
      int thumb_y;
      if (thumb_h < 8) thumb_h = 8;
      thumb_y = OVL_CONTENT_Y +
                (track_h - thumb_h) * explain_scroll_y /
                (total_height - visible_h);
      fill_rect(SCREEN_W - 4, OVL_CONTENT_Y, 3, track_h, COLOR_LIGHT_GRAY);
      fill_rect(SCREEN_W - 4, thumb_y,        3, thumb_h, COLOR_DARK_GRAY);
    }
  }

#undef OVL_HEADER_H
#undef OVL_FOOTER_H
#undef OVL_CONTENT_Y
#undef OVL_CONTENT_BOTTOM
#undef OVL_FIRST_STEP_Y
}

// ─── Commit / remove ──────────────────────────────────────────────────────────
static void safe_point_label(char*out){
  int i;char tmp[4];
  while(1){
    memset(tmp, 0, sizeof(tmp));
    make_point_label(stat_point_serial%26,tmp);stat_point_serial++;
    bool col=false;for(i=0;i<elem_count;i++){if(strcmp(elements[i].label,tmp)==0){col=true;break;}}
    if(!col){out[0]=tmp[0];out[1]=tmp[1];out[2]=tmp[2];out[3]=tmp[3];return;}
  }
}
static void safe_vec_label(char*out){
  int i;char tmp[4];
  while(1){
    memset(tmp, 0, sizeof(tmp));
    make_vector_label(stat_vec_serial%26,tmp);stat_vec_serial++;
    bool col=false;for(i=0;i<elem_count;i++){if(strcmp(elements[i].label,tmp)==0){col=true;break;}}
    if(!col){out[0]=tmp[0];out[1]=tmp[1];out[2]=tmp[2];out[3]=tmp[3];return;}
  }
}
static void safe_line_label(char*out){
  int i;char tmp[4];
  while(1){
    memset(tmp, 0, sizeof(tmp));
    make_line_label(stat_line_serial%26,tmp);stat_line_serial++;
    bool col=false;for(i=0;i<elem_count;i++){if(strcmp(elements[i].label,tmp)==0){col=true;break;}}
    if(!col){out[0]=tmp[0];out[1]=tmp[1];out[2]=tmp[2];out[3]=tmp[3];return;}
  }
}

static void commit_addable(int idx){
  int k;Addable*add;Element*e;
  if(idx<0||idx>=addable_count)return;
  add=&addables[idx];
  
  // Skip if this is an explanation-only entry
  if(add->is_explain_only) return;
  
  if(add->added){
    int del=add->elem_idx;
    if(del<0||del>=elem_count){add->added=false;add->elem_idx=-1;persist_save();return;}
    for(k=del;k<elem_count-1;k++)elements[k]=elements[k+1];
    elem_count--;add->added=false;add->elem_idx=-1;
    for(k=0;k<addable_count;k++)
      if(addables[k].added&&addables[k].elem_idx>del)addables[k].elem_idx--;
    persist_save();return;
  }
  if(elem_count>=MAX_ELEMENTS)return;
  e=&elements[elem_count];
  e->type=add->type;e->active=true;
  e->si_form=add->si_form;e->si_m=add->si_m;e->si_b=add->si_b;
  e->oy=0.0f;e->has_origin=false;
  e->x=add->x;e->y=add->y;
  e->ox=(add->type==ELEM_LINE)?add->ox:0.0f;
  if(add->type==ELEM_POINT)      safe_point_label(e->label);
  else if(add->type==ELEM_VECTOR)safe_vec_label(e->label);
  else if(add->type==ELEM_LINE)  safe_line_label(e->label);
  add->elem_idx=elem_count;elem_count++;add->added=true;
  persist_save();mark_dirty(DIRTY_CONTENT);
}

// ─── Vector stats block ───────────────────────────────────────────────────────
static void show_vector_stats(Element*v){
  char buf[64],v1[16],v2[16];float mag=fsqrt(v->x*v->x+v->y*v->y);
  fmtnum(v->x,v1);fmtnum(v->y,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);stat_line_plain("Components",buf);
  fmtnum(mag,v1);stat_line_plain("Length",v1);
  if(v->has_origin){
    fmtnum(v->ox,v1);fmtnum(v->oy,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);stat_line_plain("Origin",buf);
    fmtnum(v->ox+v->x,v1);fmtnum(v->oy+v->y,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);stat_line_plain("Tip",buf);
  }
}

// ─── Draw ─────────────────────────────────────────────────────────────────────
void draw_stats_tab(void){
  // If showing explanation, just draw the overlay and return
  if (showing_explanation) {
    draw_explanation_overlay();
    return;
  }
  

  
  int pi[MAX_ELEMENTS],pcount,vi[MAX_ELEMENTS],vcount,li[MAX_ELEMENTS],lcount;
  int acount,i,j;
  char buf[64],v1[16],v2[16],lbl[32],si_buf[48],cart_buf[48];
  Element*a,*b,*c;
  float d,mx,my,slope,intercept,abx,aby;
  float ab,bc,ca,area,ang_a,ang_b,ang_c;
  float ax,ay,bx,by,cx2,cy2,dx2,dy2,dot_a,dot_b,gx,gy;
  bool collinear,is_right,eq_ab_bc,eq_bc_ca,eq_ab_ca;
  const char*tri_type;

  persist_save();
  addable_count=0;
  stat_cursor_screen_y=-1;
  for(i=0;i<MAX_ADDABLE;i++){addables[i].added=false;addables[i].elem_idx=-1;addables[i].abs_y=0;}

  // Clear entire content area first
  fill_rect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_WHITE);
  
  stat_total_h=0;stat_y=STAT_BASE_Y-stat_scroll;

  pcount=0;vcount=0;lcount=0;
  for(i=0;i<snap_elem_count;i++){
    if(!snap_elements[i].active)continue;
    if     (snap_elements[i].type==ELEM_POINT) pi[pcount++]=i;
    else if(snap_elements[i].type==ELEM_VECTOR)vi[vcount++]=i;
    else if(snap_elements[i].type==ELEM_LINE)  li[lcount++]=i;
  }
  acount=pcount+vcount+lcount;
  #define SE(idx) snap_elements[idx]

  if(acount==0){
    draw_str_clipped("No active elements.",10,stat_y,false,COLOR_DARK_GRAY,COLOR_WHITE);
    stat_y+=SMALL_FONT_H+4;
    draw_str_clipped("Press Ans to refresh.",10,stat_y,false,COLOR_DARK_GRAY,COLOR_WHITE);
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  // ── 1 point ──────────────────────────────────────────────────────────────
  if(pcount==1&&vcount==0&&lcount==0){
    a=&SE(pi[0]);snprintf(buf,sizeof(buf),"Point %s",a->label);stat_header(buf);
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  // ── 2 points ─────────────────────────────────────────────────────────────
  if(pcount==2&&vcount==0&&lcount==0){
    a=&SE(pi[0]);b=&SE(pi[1]);
    snprintf(buf,sizeof(buf),"Points %s & %s",a->label,b->label);stat_header(buf);
    d=point_dist(a->x,a->y,b->x,b->y);fmtnum(d,v1);
    snprintf(lbl,sizeof(lbl),"|%s%s|",a->label,b->label);
    stat_line_explain(lbl, v1, true, EXPLAIN_DISTANCE, a->x, a->y, b->x, b->y, a->label, b->label);
    mx=(a->x+b->x)/2.0f;my=(a->y+b->y)/2.0f;
    fmtnum(mx,v1);fmtnum(my,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
    stat_line_add("Midpoint",buf,ELEM_POINT,mx,my,0,false,0,0, 0,
                  true, EXPLAIN_MIDPOINT, a->x, a->y, b->x, b->y, a->label, b->label);
    stat_sep();
    snprintf(buf,sizeof(buf),"Line %s%s",a->label,b->label);stat_header(buf);
    if(b->x!=a->x){
      slope=(b->y-a->y)/(b->x-a->x);intercept=a->y-slope*a->x;
      fmtnum(slope,v1);
      stat_line_explain("Slope",v1, true, EXPLAIN_SLOPE, a->x,a->y,b->x,b->y, a->label,b->label);
      build_slope_intercept(slope,intercept,buf,sizeof(buf));
      stat_line_add("SI form",buf, ELEM_LINE,slope,-1.0f,intercept,true,slope,intercept, 0,
                    true, EXPLAIN_LINE_SI, a->x, a->y, b->x, b->y, a->label, b->label);
      build_cartesian_from_abc(slope,-1.0f,intercept,buf,sizeof(buf));
      stat_line_add("Cart.",buf,   ELEM_LINE,slope,-1.0f,intercept,false,0,0,            1,
                    true, EXPLAIN_LINE_CART, a->x, a->y, b->x, b->y, a->label, b->label);
      fmtnum(intercept,v1);stat_line_plain("Y-intercept",v1);
      if(slope!=0.0f){fmtnum(-intercept/slope,v1);stat_line_plain("X-intercept",v1);}
    }else{
      fmtnum(a->x,v1);snprintf(buf,sizeof(buf),"x=%s",v1);stat_line_plain("Eq.",buf);
      stat_line_plain("Slope","undefined");
    }
    stat_sep();stat_header("Vectors");
    abx=b->x-a->x;aby=b->y-a->y;
    fmtnum(abx,v1);fmtnum(aby,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
    snprintf(lbl,sizeof(lbl),"->%s%s",a->label,b->label);
    stat_line_add(lbl,buf,ELEM_VECTOR,abx,aby,0,false,0,0, 0,
                  false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
    fmtnum(-abx,v1);fmtnum(-aby,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
    snprintf(lbl,sizeof(lbl),"->%s%s",b->label,a->label);
    stat_line_add(lbl,buf,ELEM_VECTOR,-abx,-aby,0,false,0,0, 1,
                  false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
    fmtnum(d,v1);
    stat_line_explain("Length",v1, true, EXPLAIN_VECTOR_MAG,
                      abx, aby, 0, 0, a->label, b->label);
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  // ── 3 points ─────────────────────────────────────────────────────────────
  if(pcount==3&&vcount==0&&lcount==0){
    float hx=0,hy=0,det_h,ox3=0,oy3=0,cr=0;
    a=&SE(pi[0]);b=&SE(pi[1]);c=&SE(pi[2]);
    ab=point_dist(a->x,a->y,b->x,b->y);bc=point_dist(b->x,b->y,c->x,c->y);ca=point_dist(c->x,c->y,a->x,a->y);
    area=(a->x*(b->y-c->y)+b->x*(c->y-a->y)+c->x*(a->y-b->y))/2.0f;if(area<0.0f)area=-area;
    collinear=(area<0.005f);ang_a=0.0f;ang_b=0.0f;ang_c=0.0f;
    if(!collinear){
      ax=b->x-a->x;ay=b->y-a->y;bx=c->x-a->x;by=c->y-a->y;
      dot_a=ax*bx+ay*by;ang_a=facos_deg(dot_a/(ab*ca));
      cx2=a->x-b->x;cy2=a->y-b->y;dx2=c->x-b->x;dy2=c->y-b->y;
      dot_b=cx2*dx2+cy2*dy2;ang_b=facos_deg(dot_b/(ab*bc));
      ang_c=180.0f-ang_a-ang_b;if(ang_c<0.0f)ang_c=0.0f;
    }
    is_right=(!collinear)&&((ang_a>89.5f&&ang_a<90.5f)||(ang_b>89.5f&&ang_b<90.5f)||(ang_c>89.5f&&ang_c<90.5f));
    eq_ab_bc=(!collinear)&&(ab-bc<0.01f&&ab-bc>-0.01f);
    eq_bc_ca=(!collinear)&&(bc-ca<0.01f&&bc-ca>-0.01f);
    eq_ab_ca=(!collinear)&&(ab-ca<0.01f&&ab-ca>-0.01f);
    if(collinear)tri_type="-";
    else if(eq_ab_bc&&eq_bc_ca)tri_type="Equilateral";
    else if(is_right&&(eq_ab_bc||eq_bc_ca||eq_ab_ca))tri_type="Right isosceles";
    else if(is_right)tri_type="Right triangle";
    else if(eq_ab_bc||eq_bc_ca||eq_ab_ca)tri_type="Isosceles";
    else tri_type="Scalene";
    snprintf(buf,sizeof(buf),"Triangle %s%s%s",a->label,b->label,c->label);stat_header(buf);
    fmtnum(ab,v1);snprintf(lbl,sizeof(lbl),"|%s%s|",a->label,b->label);stat_line_plain(lbl,v1);
    fmtnum(bc,v1);snprintf(lbl,sizeof(lbl),"|%s%s|",b->label,c->label);stat_line_plain(lbl,v1);
    fmtnum(ca,v1);snprintf(lbl,sizeof(lbl),"|%s%s|",c->label,a->label);stat_line_plain(lbl,v1);
    stat_line_plain("Collinear?",collinear?"Yes":"No");
    if(!collinear)stat_line_plain("Type",tri_type);
    stat_sep();stat_header("Angles");
    if(!collinear){
      fmtnum(ang_a,v1);snprintf(buf,sizeof(buf),"%s deg",v1);snprintf(lbl,sizeof(lbl),"at %s",a->label);stat_line_plain(lbl,buf);
      fmtnum(ang_b,v1);snprintf(buf,sizeof(buf),"%s deg",v1);snprintf(lbl,sizeof(lbl),"at %s",b->label);stat_line_plain(lbl,buf);
      fmtnum(ang_c,v1);snprintf(buf,sizeof(buf),"%s deg",v1);snprintf(lbl,sizeof(lbl),"at %s",c->label);stat_line_plain(lbl,buf);
    }else stat_line_plain("Angles","N/A (collinear)");
    stat_sep();stat_header("Area & Centers");
    if(!collinear){fmtnum(area,v1);stat_line_plain("Area",v1);}else stat_line_plain("Area","0 (collinear)");
    gx=(a->x+b->x+c->x)/3.0f;gy=(a->y+b->y+c->y)/3.0f;
    fmtnum(gx,v1);fmtnum(gy,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
    stat_line_add("Centroid G",buf,ELEM_POINT,gx,gy,0,false,0,0, 0,
                  true, EXPLAIN_CENTROID, a->x, a->y, b->x, b->y, a->label, b->label);
    set_explain_extra(c->x, c->y, c->label);
    if(!collinear){
      {float a11=c->x-b->x,a12=c->y-b->y,r1=a11*a->x+a12*a->y;
       float a21=c->x-a->x,a22=c->y-a->y,r2=a21*b->x+a22*b->y;
       det_h=a11*a22-a12*a21;
       if(det_h>0.0001f||det_h<-0.0001f){
         hx=(r1*a22-r2*a12)/det_h;hy=(a11*r2-a21*r1)/det_h;
         fmtnum(hx,v1);fmtnum(hy,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
         stat_line_add("Orthocenter H",buf,ELEM_POINT,hx,hy,0,false,0,0, 1,
                       false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);}};
      {float mabx=(a->x+b->x)/2.0f,maby=(a->y+b->y)/2.0f;
       float mbcx=(b->x+c->x)/2.0f,mbcy=(b->y+c->y)/2.0f;
       float a11=b->x-a->x,a12=b->y-a->y,r1=a11*mabx+a12*maby;
       float a21=c->x-b->x,a22=c->y-b->y,r2=a21*mbcx+a22*mbcy;
       float det_c=a11*a22-a12*a21;
       if(det_c>0.0001f||det_c<-0.0001f){
         ox3=(r1*a22-r2*a12)/det_c;oy3=(a11*r2-a21*r1)/det_c;
         cr=point_dist(ox3,oy3,a->x,a->y);
         fmtnum(ox3,v1);fmtnum(oy3,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
         stat_line_add("Circumcenter O",buf,ELEM_POINT,ox3,oy3,0,false,0,0, 2,
                       false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
         fmtnum(cr,v1);stat_line_plain("Circumradius R",v1);}}
    }
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  // ── 4+ points ────────────────────────────────────────────────────────────
  if(pcount>=4&&vcount==0&&lcount==0){
    snprintf(buf,sizeof(buf),"%d active points",pcount);stat_header(buf);
    gx=0.0f;gy=0.0f;for(i=0;i<pcount;i++){gx+=SE(pi[i]).x;gy+=SE(pi[i]).y;}
    gx/=(float)pcount;gy/=(float)pcount;
    fmtnum(gx,v1);fmtnum(gy,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
    stat_line_add("Centroid G",buf,ELEM_POINT,gx,gy,0,false,0,0, 0,
                  false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
    stat_sep();draw_str_clipped("Select 2 or 3 pts for full stats.",10,stat_y,false,COLOR_DARK_GRAY,COLOR_WHITE);
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  // ── Vectors only ─────────────────────────────────────────────────────────
  if(vcount>0&&pcount==0&&lcount==0){
    if(vcount==1){
      Element*va=&SE(vi[0]);
      snprintf(buf,sizeof(buf),"Vector ->%s",va->label);stat_header(buf);show_vector_stats(va);
    }else if(vcount==2){
      Element*va=&SE(vi[0]);Element*vb=&SE(vi[1]);
      float det,dot_vv,mag_a,mag_b,ang_vv,sx,sy;
      snprintf(buf,sizeof(buf),"Vector ->%s",va->label);stat_header(buf);show_vector_stats(va);
      stat_sep();snprintf(buf,sizeof(buf),"Vector ->%s",vb->label);stat_header(buf);show_vector_stats(vb);
      stat_sep();stat_header("Relations");
      det=va->x*vb->y-va->y*vb->x;fmtnum(det,v1);stat_line_plain("Det(u,v)",v1);
      stat_line_plain("Parallel?",(det>-0.001f&&det<0.001f)?"Yes":"No");
      mag_a=fsqrt(va->x*va->x+va->y*va->y);mag_b=fsqrt(vb->x*vb->x+vb->y*vb->y);
      if(mag_a>0.001f&&mag_b>0.001f){
        dot_vv=va->x*vb->x+va->y*vb->y;ang_vv=facos_deg(dot_vv/(mag_a*mag_b));
        fmtnum(ang_vv,v1);snprintf(buf,sizeof(buf),"%s deg",v1);stat_line_plain("Angle",buf);
        fmtnum(dot_vv,v1);stat_line_plain("Dot product",v1);
      }
      sx=va->x+vb->x;sy=va->y+vb->y;
      fmtnum(sx,v1);fmtnum(sy,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
      stat_line_add("Sum u+v",buf,ELEM_VECTOR,sx,sy,0,false,0,0, 0,
                    false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
      fmtnum(fsqrt(sx*sx+sy*sy),v1);stat_line_plain("|u+v|",v1);
    }else{
      float sx=0.0f,sy=0.0f;
      snprintf(buf,sizeof(buf),"%d active vectors",vcount);stat_header(buf);
      for(j=0;j<vcount;j++){
        Element*vj=&SE(vi[j]);float vmag=fsqrt(vj->x*vj->x+vj->y*vj->y);
        fmtnum(vj->x,v1);fmtnum(vj->y,v2);snprintf(lbl,sizeof(lbl),"->%s",vj->label);
        snprintf(buf,sizeof(buf),"(%s,%s)",v1,v2);stat_line_plain(lbl,buf);
        fmtnum(vmag,v1);snprintf(lbl,sizeof(lbl),"|->%s|",vj->label);stat_line_plain(lbl,v1);
        sx+=vj->x;sy+=vj->y;if(j<vcount-1)stat_sep();
      }
      stat_sep();stat_header("Sum");
      fmtnum(sx,v1);fmtnum(sy,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
      stat_line_add("Sum",buf,ELEM_VECTOR,sx,sy,0,false,0,0, 0,
                    false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
      fmtnum(fsqrt(sx*sx+sy*sy),v1);stat_line_plain("|Sum|",v1);
    }
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  // ── Lines only ────────────────────────────────────────────────────────────
  if(lcount==1&&pcount==0&&vcount==0){
    float la,lb,lc;
    a=&SE(li[0]);snprintf(buf,sizeof(buf),"Line %s",a->label);stat_header(buf);
    la=a->x;lb=a->y;lc=a->ox;
    if(lb!=0.0f){
      float m=-la/lb,bint=-lc/lb;
      build_slope_intercept(m,bint,buf,sizeof(buf));
      stat_line_add("SI form",buf,ELEM_LINE,la,lb,lc,true,m,bint, 0,
                    false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
      build_cartesian_from_abc(la,lb,lc,buf,sizeof(buf));
      stat_line_add("Cart.",buf,  ELEM_LINE,la,lb,lc,false,0,0,   1,
                    false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
      fmtnum(m,v1);stat_line_plain("Slope",v1);
      fmtnum(bint,v1);stat_line_plain("Y-intercept",v1);
      if(la!=0.0f){fmtnum(-lc/la,v1);stat_line_plain("X-intercept",v1);}
    }else if(la!=0.0f){
      fmtnum(-lc/la,v1);snprintf(buf,sizeof(buf),"x=%s",v1);
      stat_line_plain("Eq.",buf);stat_line_plain("Slope","undefined");
    }
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  if(lcount==2&&pcount==0&&vcount==0){
    float a1,b1,c1,a2,b2,c2,det,ix,iy2,angle,adot,len1,len2,same,same2;
    a=&SE(li[0]);b=&SE(li[1]);
    a1=a->x;b1=a->y;c1=a->ox;a2=b->x;b2=b->y;c2=b->ox;
    snprintf(buf,sizeof(buf),"Lines %s & %s",a->label,b->label);stat_header(buf);
    det=a1*b2-a2*b1;
    if(det>-0.001f&&det<0.001f){
      stat_line_plain("Parallel?","Yes");
      same=a1*c2-a2*c1;same2=b1*c2-b2*c1;
      stat_line_plain("Same line?",(same>-0.001f&&same<0.001f&&same2>-0.001f&&same2<0.001f)?"Yes":"No");
      {float norm=fsqrt(a1*a1+b1*b1),scale=fsqrt(a2*a2+b2*b2),dp;
       if(norm>0.001f&&scale>0.001f){dp=(c1/norm)-(c2/scale);if(dp<0)dp=-dp;fmtnum(dp,v1);stat_line_plain("Distance",v1);}}
    }else{
      stat_line_plain("Parallel?","No");
      ix=(b1*c2-b2*c1)/det;iy2=(a2*c1-a1*c2)/det;
      fmtnum(ix,v1);fmtnum(iy2,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
      stat_line_add("Intersection",buf,ELEM_POINT,ix,iy2,0,false,0,0, 0,
                    false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
      len1=fsqrt(a1*a1+b1*b1);len2=fsqrt(a2*a2+b2*b2);
      if(len1>0.001f&&len2>0.001f){
        adot=((-b1)*(-b2)+a1*a2)/(len1*len2);angle=facos_deg(adot);if(angle>90.0f)angle=180.0f-angle;
        fmtnum(angle,v1);snprintf(buf,sizeof(buf),"%s deg",v1);stat_line_plain("Angle",buf);
      }
    }
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  // ── Points + Lines ────────────────────────────────────────────────────────
  if(pcount>=1&&lcount>=1&&vcount==0){
    float la,lb,lc;int sk;
    for(j=0;j<lcount;j++){
      Element*line=&SE(li[j]);la=line->x;lb=line->y;lc=line->ox;
      snprintf(buf,sizeof(buf),"Line %s",line->label);stat_header(buf);
      sk=0;
      for(i=0;i<pcount;i++){
        Element*pt=&SE(pi[i]);
        float dot=la*pt->x+lb*pt->y+lc,norm=fsqrt(la*la+lb*lb);
        bool on=(dot>-0.001f&&dot<0.001f);
        snprintf(lbl,sizeof(lbl),"Pt %s on line?",pt->label);stat_line_plain(lbl,on?"Yes":"No");
        if(!on&&norm>0.001f){
          float dist=(dot<0.0f?-dot:dot)/norm;
          float fx=pt->x-la*dot/(la*la+lb*lb),fy=pt->y-lb*dot/(la*la+lb*lb);
          fmtnum(dist,v1);snprintf(lbl,sizeof(lbl),"Dist %s",pt->label);stat_line_plain(lbl,v1);
          fmtnum(fx,v1);fmtnum(fy,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
          snprintf(lbl,sizeof(lbl),"Foot %s",pt->label);
          stat_line_add(lbl,buf,ELEM_POINT,fx,fy,0,false,0,0, sk++,
                        false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
        }
      }
      if(pcount==2){
        float abx2=SE(pi[1]).x-SE(pi[0]).x,aby2=SE(pi[1]).y-SE(pi[0]).y;
        float pc=la*abx2+lb*aby2;
        stat_line_plain("AB parallel?",(pc>-0.001f&&pc<0.001f)?"Yes":"No");
      }
      if(j<lcount-1)stat_sep();
    }
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  // ── Points + Vectors ─────────────────────────────────────────────────────
  if(pcount>=1&&vcount>=1&&lcount==0){
    int sk;
    for(i=0;i<vcount;i++){
      Element*vec=&SE(vi[i]);sk=0;
      for(j=0;j<pcount;j++){
        Element*pt=&SE(pi[j]);
        float tx=pt->x+vec->x,ty=pt->y+vec->y;
        snprintf(buf,sizeof(buf),"Pt %s + Vec ->%s",pt->label,vec->label);stat_header(buf);
        fmtnum(tx,v1);fmtnum(ty,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
        stat_line_add("Image point",buf,ELEM_POINT,tx,ty,0,false,0,0, sk++,
                      false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
        build_line_through_point_dir(pt->x,pt->y,vec->x,vec->y,si_buf,sizeof(si_buf),cart_buf,sizeof(cart_buf));
        if(vec->x!=0.0f){
          float sl=vec->y/vec->x,ic=pt->y-sl*pt->x;
          stat_line_add("SI form", si_buf, ELEM_LINE,sl,-1.0f,ic,true,sl,ic,  sk++,
                        false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
          stat_line_add("Cart.",   cart_buf,ELEM_LINE,sl,-1.0f,ic,false,0,0,  sk++,
                        false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
        }else stat_line_plain("Eq.",si_buf);
        if(i<vcount-1||j<pcount-1)stat_sep();
      }
    }
    stat_sep();stat_header("Vector details");
    for(i=0;i<vcount;i++){
      Element*vec=&SE(vi[i]);
      snprintf(buf,sizeof(buf),"->%s",vec->label);stat_header(buf);show_vector_stats(vec);
      if(i<vcount-1)stat_sep();
    }
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  // ── Vectors + Lines ───────────────────────────────────────────────────────
  if(vcount>=1&&lcount>=1&&pcount==0){
    int sk;
    for(i=0;i<vcount;i++){
      Element*vec=&SE(vi[i]);sk=0;
      for(j=0;j<lcount;j++){
        Element*line=&SE(li[j]);
        float la=line->x,lb=line->y,lc=line->ox;
        float dot_par=la*vec->x+lb*vec->y;
        bool par=(dot_par>-0.001f&&dot_par<0.001f);
        snprintf(buf,sizeof(buf),"Vec ->%s & Line %s",vec->label,line->label);stat_header(buf);
        stat_line_plain("Parallel?",par?"Yes":"No");
        if(!par){
          float t_val=-(la*vec->ox+lb*vec->oy+lc)/dot_par;
          float ix=vec->ox+t_val*vec->x,iy=vec->oy+t_val*vec->y;
          fmtnum(ix,v1);fmtnum(iy,v2);snprintf(buf,sizeof(buf),"(%s, %s)",v1,v2);
          stat_line_add("Intersection",buf,ELEM_POINT,ix,iy,0,false,0,0, sk++,
                        false, EXPLAIN_NONE, 0, 0, 0, 0, NULL, NULL);
          float vlen=fsqrt(vec->x*vec->x+vec->y*vec->y),llen=fsqrt(la*la+lb*lb);
          if(vlen>0.001f&&llen>0.001f){
            float dot_ang=vec->x*(-lb)+vec->y*la;
            float ang=facos_deg(dot_ang<0?-dot_ang/(vlen*llen):dot_ang/(vlen*llen));
            fmtnum(ang,v1);snprintf(buf,sizeof(buf),"%s deg",v1);stat_line_plain("Angle with line",buf);
          }
        }
        if(i<vcount-1||j<lcount-1)stat_sep();
      }
    }
    stat_total_h+=STAT_BOTTOM_PAD;draw_scrollbar();return;
  }

  // ── Fallback ─────────────────────────────────────────────────────────────
  snprintf(buf,sizeof(buf),"%d elements selected",acount);stat_header(buf);
  stat_line_plain("Points",pcount>0?"yes":"no");
  stat_line_plain("Vectors",vcount>0?"yes":"no");
  stat_line_plain("Lines",lcount>0?"yes":"no");
  stat_sep();draw_str_clipped("Complex combination.",10,stat_y,false,COLOR_DARK_GRAY,COLOR_WHITE);
  stat_total_h+=STAT_BOTTOM_PAD;
  
  /* Keep cursor in view (safety net for edge cases) */
  if(stat_cursor >= 0 && stat_cursor < addable_count && stat_cursor_screen_y >= 0) {
    int max_s = stat_total_h > CONTENT_H ? stat_total_h - CONTENT_H : 0;
    if(stat_cursor_screen_y < CONTENT_Y) {
      stat_scroll += stat_cursor_screen_y - CONTENT_Y;
      if(stat_scroll < 0) stat_scroll = 0;
      mark_dirty(DIRTY_CONTENT);
    } else if(stat_cursor_screen_y + SMALL_FONT_H + 2 > CONTENT_BOTTOM) {
      stat_scroll += (stat_cursor_screen_y + SMALL_FONT_H + 2) - CONTENT_BOTTOM;
      if(stat_scroll > max_s) stat_scroll = max_s;
      mark_dirty(DIRTY_CONTENT);
    }
  }
  
  draw_scrollbar();
  #undef SE
}

// ─── Event handler ────────────────────────────────────────────────────────────
void handle_stats_event(eadk_event_t ev){
  int max_scroll;
  
  // If showing explanation, handle separately
  if (showing_explanation) {
    switch(ev) {
      case eadk_event_down: {
        ExplainStep steps[MAX_EXPLAIN_STEPS];
        int num_steps = 0;
        int total_h, visible_h, max_scroll;
        if (explain_target_idx >= 0 && explain_target_idx < addable_count)
          num_steps = generate_explanation_steps(&addables[explain_target_idx], steps);
        total_h   = math_steps_total_height(steps, num_steps);
        visible_h = SCREEN_H - 18 - 16;   /* header + footer */
        max_scroll = total_h - visible_h;
        if (max_scroll > 0 && explain_scroll_y < max_scroll) {
          explain_scroll_y += 16;
          if (explain_scroll_y > max_scroll) explain_scroll_y = max_scroll;
          mark_dirty(DIRTY_CONTENT);
        }
        break;
      }
      case eadk_event_up:
        if (explain_scroll_y > 0) {
          explain_scroll_y -= 16;
          if (explain_scroll_y < 0) explain_scroll_y = 0;
          mark_dirty(DIRTY_CONTENT);
        }
        break;
      case eadk_event_right:
      case eadk_event_left:
        /* no horizontal scroll needed — text is left-aligned and clipped */
        break;
      case eadk_event_back:
        showing_explanation = false;
        explain_scroll_y = 0;
        explain_scroll_x = 0;
        mark_dirty(DIRTY_HEADER | DIRTY_CONTENT | DIRTY_TABS);
        break;
      default:
        break;
    }
    return;
  }
  

    // Normal stats navigation (not in explanation mode)
  switch(ev){
    case eadk_event_down:
      if(stat_cursor < addable_count - 1) {
        int max_s;
        stat_cursor++;
        /* Proactively scroll to keep cursor visible using abs_y from last draw */
        if(stat_cursor < addable_count) {
          int ay = addables[stat_cursor].abs_y;
          max_s = stat_total_h > CONTENT_H ? stat_total_h - CONTENT_H : 0;
          if(ay - stat_scroll + SMALL_FONT_H + 2 > CONTENT_BOTTOM) {
            stat_scroll = ay + SMALL_FONT_H + 2 - CONTENT_BOTTOM;
            if(stat_scroll > max_s) stat_scroll = max_s;
          }
        }
        mark_dirty(DIRTY_CONTENT);
      }
      break;
    case eadk_event_up:
      if(stat_cursor > 0) {
        stat_cursor--;
        /* Proactively scroll to keep cursor visible using abs_y from last draw */
        if(stat_cursor >= 0) {
          int ay = addables[stat_cursor].abs_y;
          if(ay - stat_scroll < CONTENT_Y) {
            stat_scroll = ay - CONTENT_Y;
            if(stat_scroll < 0) stat_scroll = 0;
          }
        }
        mark_dirty(DIRTY_CONTENT);
      }
      break;

    case eadk_event_left:
      // Do nothing
      break;
      
    case eadk_event_right:
      // First Right press: start at first stat
      if(stat_cursor == -1) {
        stat_cursor = 0;
        mark_dirty(DIRTY_CONTENT);
      }
      // Otherwise do nothing
      break;

    case eadk_event_ok:
    case eadk_event_exe:
      if (addable_count > 0 && stat_cursor >= 0 && stat_cursor < addable_count) {
        // If stat has explanation, show it
        if (addables[stat_cursor].has_explanation) {
          explain_target_idx = stat_cursor;
          explain_scroll_y = 0;
          explain_scroll_x = 0;
          showing_explanation = true;
          mark_dirty(DIRTY_CONTENT);
        }
      }
      break;

    case eadk_event_ans:{
      int i;refresh_snapshot();stat_scroll=0;stat_cursor=-1;
      cursor_on_help=false;persist_count=0;
      for(i=0;i<MAX_ADDABLE;i++){addables[i].added=false;addables[i].elem_idx=-1;addables[i].abs_y=0;}
      mark_dirty(DIRTY_CONTENT);break;
    }

    case eadk_event_back:
      stat_scroll=0;stat_cursor=-1;cursor_on_help=false;
      current_tab=TAB_GRAPH;
      mark_dirty(DIRTY_TABS|DIRTY_CONTENT|DIRTY_TOOLBAR);break;

    case eadk_event_plus:
      // Add current stat
      if(addable_count > 0 && stat_cursor >= 0 && stat_cursor < addable_count) {
        if(!addables[stat_cursor].is_explain_only) {
          commit_addable(stat_cursor);
          mark_dirty(DIRTY_CONTENT);
        }
      }
      break;

    case eadk_event_minus:
      if(addable_count > 0 && stat_cursor >= 0 && stat_cursor < addable_count) {
        if(!addables[stat_cursor].is_explain_only && addables[stat_cursor].added) {
          commit_addable(stat_cursor);  // commit_addable removes when added==true
          mark_dirty(DIRTY_CONTENT);
        }
      }
      break;

    default:break;
  }
}

// ─── Public API ───────────────────────────────────────────────────────────────
void stats_reset_scroll(void){
  int i;
  stat_scroll=0;
  stat_cursor=-1;
  cursor_on_help=false;
  showing_explanation=false;
  explain_scroll_y=0;
  explain_scroll_x=0;
  persist_count=0;
  for(i=0;i<MAX_ADDABLE;i++){addables[i].added=false;addables[i].elem_idx=-1;addables[i].abs_y=0;}
  refresh_snapshot();
  mark_dirty(DIRTY_HEADER | DIRTY_CONTENT | DIRTY_TABS);
}