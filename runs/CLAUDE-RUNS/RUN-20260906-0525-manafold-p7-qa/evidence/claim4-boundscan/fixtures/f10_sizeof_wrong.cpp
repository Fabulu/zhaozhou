constexpr float kKiloTab[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
constexpr int kKiloCount = sizeof(kKiloTab) / sizeof(kKiloTab[0]) - 1;  // WRONG
void apply() {
  for (int i = 0; i < kKiloCount; ++i) {
    use(kKiloTab[i]);
  }
}
