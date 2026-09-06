namespace a {
constexpr float kUniformTab[] = {0.f, 1.f, 2.f, 3.f, 4.f, 5.f};
}
void applyA(int idx) {
  if (idx < 5) {                 // WRONG: a::kUniformTab has 6
    use(a::kUniformTab[idx]);
  }
}
