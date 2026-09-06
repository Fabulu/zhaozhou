constexpr const char* kPapaTab[] = {"a,b,c", "d,e", "f"};
void apply(int idx) {
  if (idx < 2) {                 // WRONG: 3 entries
    use(kPapaTab[idx]);
  }
}
