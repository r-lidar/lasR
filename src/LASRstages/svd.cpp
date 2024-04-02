#include "svd.h"
#include "omp.h"
#include "error.h"

#include <stdlib.h>   /* abs */
#include <math.h>     /* acos */
#include <cmath>      /* fmod pow */

#include <vector>
#include <iostream>

#include "armadillo/armadillo.h"

#define PUREKNN 0
#define KNNRADIUS 1
#define PURERADIUS 2

LASRsvd::LASRsvd(int k, double r, std::string features)
{
  this->k = k;
  this->r = r;

  record_eigen_values = true;
  record_coefficients = true;

  if (k == 0 && r > 0) mode = PURERADIUS;
  else if (k > 0 && r == 0) mode = PUREKNN;
  else if (k > 0 && r > 0) mode = PURERADIUS;
  else throw "Internal error: invalid argument k or r"; // # nocov

  // Parse feature = "*"
  std::string all = "CEapslocein";
  size_t pos = features.find('*');
  if (pos != std::string::npos) features = all;

  ft_C = false;
  ft_E = false;
  ft_a = false;
  ft_p = false;
  ft_s = false;
  ft_l = false;
  ft_o = false;
  ft_c = false;
  ft_e = false;
  ft_i = false;
  ft_n = false;
  for(const char& c : features)
  {
    switch(c)
    {
      case 'C': ft_C = true; break;
      case 'E': ft_E = true; break;
      case 'a': ft_a = true; break;
      case 'p': ft_p = true; break;
      case 's': ft_s = true; break;
      case 'l': ft_l = true; break;
      case 'o': ft_o = true; break;
      case 'c': ft_c = true; break;
      case 'e': ft_e = true; break;
      case 'i': ft_i = true; break;
      case 'n': ft_n = true; break;
      default: warning("Option '%c' not supported\n", c); break;
    }
  }
}

bool LASRsvd::process(LAS*& las)
{
  double radius;
  if (mode == PUREKNN) radius = F64_MAX;
  else if (mode == PURERADIUS) { last_error = "Internal error: radius search is not supported yet"; return false; } // # nocov
  else if (mode == KNNRADIUS) radius = r;
  else { last_error = "Internal error: invalid search mode"; return false; } // # nocov

  int nattr_before_geometry = las->header->get_attributes_size();

  if (ft_C)
  {
    las->add_attribute(LAS::FLOAT, "coeff00", "Principal component coefficient");
    las->add_attribute(LAS::FLOAT, "coeff01", "Principal component coefficient");
    las->add_attribute(LAS::FLOAT, "coeff02", "Principal component coefficient");
    las->add_attribute(LAS::FLOAT, "coeff10", "Principal component coefficient");
    las->add_attribute(LAS::FLOAT, "coeff11", "Principal component coefficient");
    las->add_attribute(LAS::FLOAT, "coeff12", "Principal component coefficient");
    las->add_attribute(LAS::FLOAT, "coeff20", "Principal component coefficient");
    las->add_attribute(LAS::FLOAT, "coeff21", "Principal component coefficient");
    las->add_attribute(LAS::FLOAT, "coeff22", "Principal component coefficient");
  }

  if (ft_E)
  {
    las->add_attribute(LAS::FLOAT, "lambda1", "Eigen value 1");
    las->add_attribute(LAS::FLOAT, "lambda2", "Eigen value 2");
    las->add_attribute(LAS::FLOAT, "lambda3", "Eigen value 3");
  }

  if (ft_a) las->add_attribute(LAS::FLOAT, "anisotropy", "anisotropy");
  if (ft_p) las->add_attribute(LAS::FLOAT, "planarity", "planarity");
  if (ft_s) las->add_attribute(LAS::FLOAT, "sphericity", "sphericity");
  if (ft_l) las->add_attribute(LAS::FLOAT, "linearity", "linearity");
  if (ft_o) las->add_attribute(LAS::FLOAT, "omnivariance", "omnivariance");
  if (ft_c) las->add_attribute(LAS::FLOAT, "curvature", "curvature");
  if (ft_e) las->add_attribute(LAS::FLOAT, "eigensum", "sum of eigen values");
  if (ft_i) las->add_attribute(LAS::FLOAT, "angle", "azimutal angle (degree)");
  if (ft_n)
  {
    las->add_attribute(LAS::FLOAT, "normalX", "x component of normal vector");
    las->add_attribute(LAS::FLOAT, "normalY", "y component of normal vector");
    las->add_attribute(LAS::FLOAT, "normalZ", "z component of normal vector");
  }

  progress->reset();
  progress->set_total(las->npoints);
  progress->set_prefix("SVD/PCA");
  progress->show();

  // The next for loop is at the level a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < las->npoints ; i++)
  {
    std::vector<PointLAS> pts;
    double xyz[3];
    las->get_xyz(i, xyz);
    las->knn(xyz, k, radius, pts, &lasfilter);

    arma::mat A(pts.size(), 3);
    arma::mat coeff;  // Principle component matrix
    arma::mat score;
    arma::vec latent; // Eigenvalues in descending order

    for (unsigned int k = 0 ; k < pts.size() ; k++)
    {
      A(k,0) = pts[k].x;
      A(k,1) = pts[k].y;
      A(k,2) = pts[k].z;
    }

    arma::princomp(coeff, score, latent, A);

    double eigen_sum = (latent[0]+latent[1]+latent[2]);
    double eigen_largest = latent[0]; ///eigen_sum; ??
    double eigen_medium = latent[1]; //eigen_sum; ??
    double eigen_smallest = latent[2]; //eigen_sum; ??
    double coeff00 = coeff(0,0);
    double coeff01 = coeff(0,1);
    double coeff02 = coeff(0,2);
    double coeff10 = coeff(1,0);
    double coeff11 = coeff(1,1);
    double coeff12 = coeff(1,2);
    double coeff20 = coeff(2,0);
    double coeff21 = coeff(2,1);
    double coeff22 = coeff(2,2);
    double nx = coeff(0,0);
    double ny = coeff(0,1);
    double nz = coeff(0,2);
    double anisotropy = (eigen_largest-eigen_smallest)/eigen_largest;
    double planarity = (eigen_medium-eigen_smallest)/eigen_largest;
    double sphericity = eigen_smallest/eigen_largest;
    double linearity = (eigen_largest-eigen_medium)/eigen_largest;
    double omnivariance = pow(eigen_largest*eigen_medium*eigen_smallest, 1.0/3.0);
    double curvature = eigen_smallest/eigen_sum;
    double angle = acos(abs(coeff(2,2)));
    angle = fmod(angle, PI);
    angle = angle*180/PI;

    #pragma omp critical
    {
      int attr_index = nattr_before_geometry;
      las->seek(i);

      if (ft_C)
      {
        las->point.set_attribute_as_float(attr_index, coeff00); attr_index++;
        las->point.set_attribute_as_float(attr_index, coeff01); attr_index++;
        las->point.set_attribute_as_float(attr_index, coeff02); attr_index++;
        las->point.set_attribute_as_float(attr_index, coeff10); attr_index++;
        las->point.set_attribute_as_float(attr_index, coeff11); attr_index++;
        las->point.set_attribute_as_float(attr_index, coeff12); attr_index++;
        las->point.set_attribute_as_float(attr_index, coeff20); attr_index++;
        las->point.set_attribute_as_float(attr_index, coeff21); attr_index++;
        las->point.set_attribute_as_float(attr_index, coeff22); attr_index++;
      }

      if (ft_E)
      {
        las->point.set_attribute_as_float(attr_index, eigen_largest); attr_index++;
        las->point.set_attribute_as_float(attr_index, eigen_medium); attr_index++;
        las->point.set_attribute_as_float(attr_index, eigen_smallest); attr_index++;
      }

      if (ft_a) { las->point.set_attribute_as_float(attr_index, anisotropy); attr_index++; }
      if (ft_p) { las->point.set_attribute_as_float(attr_index, planarity); attr_index++; }
      if (ft_s) { las->point.set_attribute_as_float(attr_index, sphericity); attr_index++; }
      if (ft_l) { las->point.set_attribute_as_float(attr_index, linearity); attr_index++; }
      if (ft_o) { las->point.set_attribute_as_float(attr_index, omnivariance); attr_index++; }
      if (ft_c) { las->point.set_attribute_as_float(attr_index, curvature); attr_index++; }
      if (ft_e) { las->point.set_attribute_as_float(attr_index, eigen_sum); attr_index++; }
      if (ft_i) { las->point.set_attribute_as_float(attr_index, angle); attr_index++; }
      if (ft_n)
      {
        las->point.set_attribute_as_float(attr_index, nx); attr_index++;
        las->point.set_attribute_as_float(attr_index, ny); attr_index++;
        las->point.set_attribute_as_float(attr_index, nz); attr_index++;
      }

      las->update_point();

      if (main_thread)
      {
          (*progress)++;
          progress->show();
      }
    }
  }

  progress->done();

  return true;
}