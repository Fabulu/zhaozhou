constexpr float kXrayTab[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
void apply(int t) {
  use(kXrayTab[t % 5]);          // WRONG: 6 entries
}
