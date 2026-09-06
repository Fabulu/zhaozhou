constexpr float kQuebecTab[8] = {0.f, 1.f, 2.f};
void apply(int idx) {
  if (idx < 3) {
    use(kQuebecTab[idx]);
  }
}
