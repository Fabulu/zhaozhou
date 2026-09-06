constexpr float kCharlieRamp[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
void apply() {
  for (int i = 0; i <= 6; ++i) {
    use(kCharlieRamp[i]);
  }
}
