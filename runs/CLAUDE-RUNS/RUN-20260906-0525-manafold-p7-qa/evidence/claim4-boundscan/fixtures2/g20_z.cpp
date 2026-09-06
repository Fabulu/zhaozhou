namespace z {
constexpr float kUniformTab[] = {0.f, 1.f, 2.f};
}
void applyZ(int idx) {
  if (idx < 3) { use(z::kUniformTab[idx]); }
}
