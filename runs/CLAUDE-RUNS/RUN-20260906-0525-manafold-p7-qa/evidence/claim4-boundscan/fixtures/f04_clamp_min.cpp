#include <algorithm>
constexpr float kEchoTab[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
void apply(int idx) {
  int j = std::min(idx, 4);      // WRONG: max valid index is 5
  use(kEchoTab[j]);
}
