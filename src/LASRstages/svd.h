#ifndef SVD_H
#define SVD_H

#include "Stage.h"

class LASRsvd : public Stage
{
public:
  LASRsvd();
  bool process(PointCloud*& las) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "svd"; }
  bool is_parallelized() const override { return true; }
  LASRsvd* clone() const override { return new LASRsvd(*this); };

private:
  int mode;
  int k;
  double r;

  bool always_up;
  bool record_eigen_values;
  bool record_coefficients;

  bool ft_C;
  bool ft_E;
  bool ft_a;
  bool ft_p;
  bool ft_s;
  bool ft_l;
  bool ft_o;
  bool ft_c;
  bool ft_e;
  bool ft_i;
  bool ft_n;
};

#endif
