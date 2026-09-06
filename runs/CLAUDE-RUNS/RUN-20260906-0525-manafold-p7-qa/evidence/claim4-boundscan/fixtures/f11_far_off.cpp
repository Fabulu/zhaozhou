constexpr float kLimaTab[] = {0.f,1.f,2.f,3.f,4.f,5.f,6.f,7.f,8.f,9.f,10.f,11.f,12.f,13.f,14.f};
void apply(int idx) {
  if (idx < 10) {                // WRONG: 15 entries, silently drops 5
    use(kLimaTab[idx]);
  }
}
