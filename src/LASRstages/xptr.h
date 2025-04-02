#ifndef XPOINTER_H
#define XPOINTER_H

#ifdef USING_R

#include "Stage.h"

class LASRreaderxptr : public Stage
{
public:
  LASRreaderxptr(PointCloud* las) { this->las = las; };
  bool process(Header*& header) override { header = this->las->header; return true; };
  bool process(PointCloud*& las) override { las = this->las; return true; };
  std::string get_name() const override { return "reader_externalptr";  }
  bool use_rcapi() const override { return true; };
  void merge(const Stage* other) override { const LASRreaderxptr* o = dynamic_cast<const LASRreaderxptr*>(other); las = o->las; };

  LASRreaderxptr* clone() const override { return new LASRreaderxptr(*this); };

private:
  PointCloud* las;
};

class LASRxptr : public Stage
{
public:
  LASRxptr() { las = nullptr; };
  bool process(PointCloud*& las) override;
  SEXP to_R() override;
  std::string get_name() const override { return "xpointer";  }
  bool use_rcapi() const override { return true; };
  void merge(const Stage* other) override;

  LASRxptr* clone() const override { return new LASRxptr(*this); };

private:
  PointCloud* las;
};


#else
#pragma message("LASRxpointer skipped: cannot be compiled without R")
#endif

#endif
