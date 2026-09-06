#include "g05_decl.h"
void apply(int idx) {
  if (idx < 5) {                 // WRONG: 6 entries
    int a=0; int b=1; int c=2;
    int d=3; int e=4; int f=5;
    int g=6; int h=7; int i2=8;
    int j=9; int k2=10;
    use(kFoxtrotTab[idx]+a+b+c+d+e+f+g+h+i2+j+k2);
  }
}
