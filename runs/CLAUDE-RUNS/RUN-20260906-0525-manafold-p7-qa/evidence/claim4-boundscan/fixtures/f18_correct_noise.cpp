constexpr float kSierraTab[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
void apply(int idx) {
  int mask = 1 << 5;             // unrelated shift
  if (idx < 6) {                 // CORRECT
    use(kSierraTab[idx] + mask);
  }
}
