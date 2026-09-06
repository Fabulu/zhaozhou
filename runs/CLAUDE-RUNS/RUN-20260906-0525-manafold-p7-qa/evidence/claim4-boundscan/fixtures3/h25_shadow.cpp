constexpr float kAlfaShadow[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
void apply(int idx) {
  if (idx < 5) {                 // WRONG: 6 entries
    use(kAlfaShadow[idx]);
  }
}
#if 0
constexpr float kAlfaShadow[] = {0.f, 1.f, 2.f, 3.f, 4.f};
#endif
