#include "h24b_decl.h"
void apply(int idx) {
  if (idx < 5) {                 // WRONG: 6 entries (index 9 lines below)
    int a=0;
    int b=1;
    int c=2;
    int d=3;
    int e=4;
    int f=5;
    int g=6;
    int h=7;
    use(kZuluTab[idx]+a+b+c+d+e+f+g+h);
  }
}
