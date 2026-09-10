// Vzhao_field_progcache.h (compat shim) -- lets tests/field/field_progcache_directed.cpp,
// the REAL-PROGRAM suite written for zhao_field_progcache, run UNCHANGED against
// the scanned candidate zhao_field_progdir, whose port list is identical.
//
// Only the target that verilates zhao_field_progdir puts this directory on its
// include path, so the real Vzhao_field_progcache.h is never shadowed for the
// oracle's own test. Same source, two tops: the suite's 123 checks and its
// random lane against zref::field::ProgCache become the candidate's for free.
#pragma once
#include "Vzhao_field_progdir.h"
using Vzhao_field_progcache = Vzhao_field_progdir;
