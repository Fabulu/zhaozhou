constexpr float kWhiskeyTab[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
constexpr int kWhiskeyN = 6;
void apply() {
  for (int i = 0; i < kWhiskeyN - 1; ++i) {   // WRONG: drops entry 5
    use(kWhiskeyTab[i]);
  }
}
