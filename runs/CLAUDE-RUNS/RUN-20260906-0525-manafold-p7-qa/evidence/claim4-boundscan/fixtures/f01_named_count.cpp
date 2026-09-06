constexpr float kBravoPresets[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
constexpr int kBravoCount = 5;   // WRONG: table has 6
void apply(int idx) {
  for (int i = 0; i < kBravoCount; ++i) {
    use(kBravoPresets[i]);
  }
}
