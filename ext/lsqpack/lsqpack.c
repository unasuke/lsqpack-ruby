#include "lsqpack.h"

VALUE rb_mLsqpack;

void
Init_lsqpack(void)
{
  rb_mLsqpack = rb_define_module("Lsqpack");
}
