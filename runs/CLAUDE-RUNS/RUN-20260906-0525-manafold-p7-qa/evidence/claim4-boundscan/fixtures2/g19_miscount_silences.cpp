constexpr Vec kTangoTab[] = {mk(0,1), mk(2,3), mk(4,5), mk(6,7)};
void apply(int idx) {
  if (idx < 3) {                 // WRONG: 4 entries, drops the last
    use(kTangoTab[idx]);
  }
}
