constexpr float kFoxtrotTab[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
void apply(int idx) {
  if (idx < 5) {
    int a = 0;
    int b = 1;
    int c = 2;
    int d = 3;
    int e = 4;
    int f = 5;
    int g = 6;
    int h = 7;
    use(kFoxtrotTab[idx] + a + b + c + d + e + f + g + h);
  }
}
