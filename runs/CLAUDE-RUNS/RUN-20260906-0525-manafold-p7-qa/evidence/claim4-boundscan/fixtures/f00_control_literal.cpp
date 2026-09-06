constexpr float kAlphaPresets[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
void apply(int idx) {
  if (idx < 5) {
    use(kAlphaPresets[idx]);
  }
}
