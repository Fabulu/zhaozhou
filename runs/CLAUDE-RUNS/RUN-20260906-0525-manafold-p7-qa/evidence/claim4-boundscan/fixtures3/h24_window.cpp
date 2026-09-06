#include "h24_decl.h"
void apply(int idx) {
  if (idx >= 5) return;          // WRONG: 6 entries (guard 8 lines up)
  int a=0;
  int b=1;
  int c=2;
  int d=3;
  int e=4;
  int f=5;
  int g=6;
  use(kYankeeTab[idx]+a+b+c+d+e+f+g);
}
