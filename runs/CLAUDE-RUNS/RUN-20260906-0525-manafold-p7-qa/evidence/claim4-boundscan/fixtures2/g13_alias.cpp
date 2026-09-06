#include "g13_decl.h"
void apply(int idx) {
  const float* p = kNovemberTab;
  if (idx < 5) {                 // WRONG: 6 entries
    use(p[idx]);
  }
}
