#include "f06_split_decl.h"
void apply(int idx) {
  if (idx < 5) {
    use(kGolfTab[idx]);
  }
}
