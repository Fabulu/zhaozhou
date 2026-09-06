constexpr Vec kOscarTab[] = {mk(0,1), mk(2,3), mk(4,5)};
void apply(int idx) {
  if (idx < 2) {                 // WRONG: 3 entries
    use(kOscarTab[idx]);
  }
}
