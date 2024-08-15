#ifndef LASCALLBACK_H
#define LASCALLBACK_H

#ifdef USING_R

#include "Stage.h"
#include <vector>

class LASRcallback : public Stage
{
public:
  LASRcallback();
  bool process(LAS*& las) override;
  SEXP to_R() override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "callback";  }
  bool use_rcapi() const override { return true; };

  // multi-threading
  LASRcallback* clone() const override { return new LASRcallback(*this); };
  void merge(const Stage* other) override;
  void sort(const std::vector<int>& order) override;

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
