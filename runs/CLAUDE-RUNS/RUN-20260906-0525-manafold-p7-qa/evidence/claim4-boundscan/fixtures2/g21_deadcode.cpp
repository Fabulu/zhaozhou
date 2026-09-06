/* old table, kept for reference:
constexpr float kVictorTab[] = {0.f, 1.f, 2.f, 3.f, 4.f};
*/
constexpr float kVictorTab[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f};
void apply(int idx) {
  if (idx < 4) {                 // WRONG: 8 entries
    use(kVictorTab[idx]);
  }
}
