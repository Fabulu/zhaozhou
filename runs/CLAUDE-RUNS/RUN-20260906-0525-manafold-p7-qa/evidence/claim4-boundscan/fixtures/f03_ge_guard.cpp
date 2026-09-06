constexpr float kDeltaTab[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
void apply(int idx) {
  if (idx >= 5) return;          // WRONG: drops entry 5 of 6
  use(kDeltaTab[idx]);
}
