constexpr float kMikeTab[] = {0.f, 1.f};
void apply(int idx) {
  if (idx < 1) {                 // WRONG: 2 entries
    use(kMikeTab[idx]);
  }
}
