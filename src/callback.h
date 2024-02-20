#ifndef LASCALLBACK_H
#define LASCALLBACK_H

#ifdef USING_R

#include "lasralgorithm.h"
#include <vector>

class LASRcallback : public LASRalgorithm
{
public:
  LASRcallback(double xmin, double ymin, double xmax, double ymax, std::string select, SEXP fun, SEXP args, bool modify, bool drop_buffer);
  bool process(LAS*& las) override;
  SEXP to_R() override;
  std::string get_name() const override { return "callback";  }
  LASRcallback* clone() const override { return new LASRcallback(*this); };
  void merge(const LASRcallback* other);

private:
  bool modify;
  bool drop_buffer;
  std::string select;
  std::vector<std::string> filenames;
  SEXP fun;
  SEXP args;
  SEXP ans;

  enum attributes{X, Y, Z, I, T, RN, NOR, SDF, EoF, CLASS, SYNT, KEYP, WITH, OVER, UD, SA, PSID, R, G, B, NIR, CHAN, BUFF};
};


#else
#pragma message("LASRcallback skipped: cannot be compiled without R")
#endif

#endif
