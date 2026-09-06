constexpr float kNovemberTab[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
void apply(int idx) {
  const float* p = kNovemberTab;
  if (idx < 5) {
    use(p[idx]);
  }
}
