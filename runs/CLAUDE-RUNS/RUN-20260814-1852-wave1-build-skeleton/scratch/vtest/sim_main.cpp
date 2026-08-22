#include "Vcounter.h"
#include "verilated.h"
#include <cstdio>
int main(int argc, char** argv) {
  Verilated::commandArgs(argc, argv);
  Vcounter* top = new Vcounter;
  top->rst_n = 0; top->clk = 0; top->eval();
  top->rst_n = 1;
  for (int i = 0; i < 5; i++) { top->clk = 0; top->eval(); top->clk = 1; top->eval(); }
  printf("count=%u\n", (unsigned)top->count);
  delete top; return 0;
}

double sc_time_stamp(){return 0;}
