#include "svd.h"
#include "openmp.h"
#include "error.h"

#include <stdlib.h>   /* abs */
#include <math.h>     /* acos */
#include <cmath>      /* fmod pow */

#include <vector>
#include <iostream>
#include <chrono>

#include "Eigen/Dense"

#define PUREKNN 0
#define KNNRADIUS 1
#define PURERADIUS 2

#ifndef PI
#define PI 3.14159265358979323846
#endif

LASRsvd::LASRsvd()
{
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
}

bool LASRsvd::set_parameters(const nlohmann::json& stage)
{
  k = stage.at("k");
  r = stage.value("r", 0.0);
  std::string features = stage.value("features", "");

  record_eigen_values = true;
  record_coefficients = true;

  if (k == 0 && r > 0) mode = PURERADIUS;
  else if (k > 0 && r == 0) mode = PUREKNN;
  else if (k > 0 && r > 0) mode = KNNRADIUS;
  else
  {
    last_error = "internal error: invalid argument k or r"; // # nocov
    return false;
  }

  // Parse feature = "*"
  std::string all = "CEapslocein";
  size_t pos = features.find('*');
  if (pos != std::string::npos) features = all;

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

  return true;
}

bool LASRsvd::process(PointCloud*& las)
{
  std::vector<Attribute> attributes;
  std::vector<AttributeAccessor> writers;

  if (ft_C)
  {
    attributes.push_back(Attribute("coeff00", AttributeType::FLOAT, 1, 0, "Principal component coefficient"));
    attributes.push_back(Attribute("coeff01", AttributeType::FLOAT, 1, 0, "Principal component coefficient"));
    attributes.push_back(Attribute("coeff02", AttributeType::FLOAT, 1, 0, "Principal component coefficient"));
    attributes.push_back(Attribute("coeff10", AttributeType::FLOAT, 1, 0, "Principal component coefficient"));
    attributes.push_back(Attribute("coeff11", AttributeType::FLOAT, 1, 0, "Principal component coefficient"));
    attributes.push_back(Attribute("coeff12", AttributeType::FLOAT, 1, 0, "Principal component coefficient"));
    attributes.push_back(Attribute("coeff20", AttributeType::FLOAT, 1, 0, "Principal component coefficient"));
    attributes.push_back(Attribute("coeff21", AttributeType::FLOAT, 1, 0, "Principal component coefficient"));
    attributes.push_back(Attribute("coeff22", AttributeType::FLOAT, 1, 0, "Principal component coefficient"));
  }

  if (ft_E)
  {
    attributes.push_back(Attribute("lambda1", AttributeType::FLOAT, 1, 0, "Eigen value 1"));
    attributes.push_back(Attribute("lambda2", AttributeType::FLOAT, 1, 0, "Eigen value 2"));
    attributes.push_back(Attribute("lambda3", AttributeType::FLOAT, 1, 0, "Eigen value 3"));
  }

  if (ft_a) attributes.push_back(Attribute("anisotropy", AttributeType::FLOAT, 1, 0, "anisotropy"));
  if (ft_p) attributes.push_back(Attribute("planarity", AttributeType::FLOAT, 1, 0, "planarity"));
  if (ft_s) attributes.push_back(Attribute("sphericity", AttributeType::FLOAT, 1, 0, "sphericity"));
  if (ft_l) attributes.push_back(Attribute("linearity", AttributeType::FLOAT, 1, 0, "linearity"));
  if (ft_o) attributes.push_back(Attribute("omnivariance", AttributeType::FLOAT, 1, 0, "omnivariance"));
  if (ft_c) attributes.push_back(Attribute("curvature", AttributeType::FLOAT, 1, 0, "curvature"));
  if (ft_e) attributes.push_back(Attribute("eigensum", AttributeType::FLOAT, 1, 0, "sum of eigen values"));
  if (ft_i) attributes.push_back(Attribute("inclination", AttributeType::FLOAT, 1, 0, "angle with vertical (degree)"));
  if (ft_n)
  {
    attributes.push_back(Attribute("normalX", AttributeType::FLOAT, 1, 0, "x component of normal vector"));
    attributes.push_back(Attribute("normalY", AttributeType::FLOAT, 1, 0, "y component of normal vector"));
    attributes.push_back(Attribute("normalZ", AttributeType::FLOAT, 1, 0, "z component of normal vector"));
  }

  if (!las->add_attributes(attributes))
  {
    return false;
  };

  AttributeAccessor set_coeff00("coeff00");
  AttributeAccessor set_coeff01("coeff01");
  AttributeAccessor set_coeff02("coeff02");
  AttributeAccessor set_coeff10("coeff10");
  AttributeAccessor set_coeff11("coeff11");
  AttributeAccessor set_coeff12("coeff12");
  AttributeAccessor set_coeff20("coeff20");
  AttributeAccessor set_coeff21("coeff21");
  AttributeAccessor set_coeff22("coeff22");

  AttributeAccessor set_lambda1("lambda1");
  AttributeAccessor set_lambda2("lambda2");
  AttributeAccessor set_lambda3("lambda3");

  AttributeAccessor set_anisotropy("anisotropy");
  AttributeAccessor set_planarity("planarity");
  AttributeAccessor set_sphericity("sphericity");
  AttributeAccessor set_linearity("linearity");
  AttributeAccessor set_omnivariance("omnivariance");
  AttributeAccessor set_curvature("curvature");
  AttributeAccessor set_eigensum("eigensum");
  AttributeAccessor set_angle("inclination");

  AttributeAccessor set_normalX("normalX");
  AttributeAccessor set_normalY("normalY");
  AttributeAccessor set_normalZ("normalZ");


  // The next for loop is at the level a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  if (verbose) print("  Building KDtree spatial index\n");
  auto start = std::chrono::high_resolution_clock::now();

  las->build_kdtree();

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;

  if (verbose) print("  KDtree built in %.2lf seconds\n", elapsed.count());

  progress->reset();
  progress->set_total(las->npoints);
  progress->set_prefix("SVD/PCA");
  progress->set_ncpu(ncpu);
  progress->show();

  #pragma omp parallel for num_threads(ncpu)
  for (size_t i = 0 ; i < las->npoints ; i++)
  {
    if (progress->interrupted()) continue;

    Point p;
    p.set_schema(&las->header->schema);

    if (!las->get_point(i, &p)) continue;

    std::vector<Point> pts;
    switch (mode)
    {
      case PURERADIUS: { las->query_sphere(p, r, pts, nullptr); break; }
      case PUREKNN:    { las->knn(p, k, pts, nullptr); break; }
      case KNNRADIUS:  { las->rknn(p, k, r, pts, nullptr); break; }
      default: { break; }
    }

    Eigen::MatrixXd A(pts.size(), 3);
    Eigen::MatrixXd coeff; // Principal component matrix
    Eigen::VectorXd latent; // Eigenvalues in descending order

    // Fill the matrix A with points
    for (size_t k = 0; k < pts.size(); ++k)
    {
      A(k, 0) = pts[k].get_x();
      A(k, 1) = pts[k].get_y();
      A(k, 2) = pts[k].get_z();
    }

    // Compute the mean
    Eigen::VectorXd mean = A.colwise().mean();
    Eigen::MatrixXd centered = A.rowwise() - mean.transpose();

    // Compute the covariance matrix
    Eigen::MatrixXd covariance = (centered.transpose() * centered) / double(pts.size() - 1);

    // Perform eigen decomposition
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eigensolver(covariance);
    coeff = eigensolver.eigenvectors().rowwise().reverse(); // Eigen vectors are sorted by increasing eigenvalue, so reverse to get descending order
    latent = eigensolver.eigenvalues().reverse(); // Eigen values are sorted by increasing order, so reverse to get descending order

    double eigen_sum = (latent[0]+latent[1]+latent[2]);
    double eigen_largest = latent[0]; // /eigen_sum; ??
    double eigen_medium = latent[1]; // /eigen_sum; ??
    double eigen_smallest = latent[2]; // /eigen_sum; ??
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
      las->seek(i);

      if (ft_C)
      {
        set_coeff00(&las->point, coeff00);
        set_coeff01(&las->point, coeff01);
        set_coeff02(&las->point, coeff02);
        set_coeff10(&las->point, coeff10);
        set_coeff11(&las->point, coeff11);
        set_coeff12(&las->point, coeff12);
        set_coeff20(&las->point, coeff20);
        set_coeff21(&las->point, coeff21);
        set_coeff22(&las->point, coeff22);
      }

      if (ft_E)
      {
        set_lambda1(&las->point, eigen_largest);
        set_lambda2(&las->point, eigen_medium);
        set_lambda3(&las->point, eigen_smallest);
      }

      if (ft_a) { set_anisotropy(&las->point, anisotropy); }
      if (ft_p) { set_planarity(&las->point, planarity); }
      if (ft_s) { set_sphericity(&las->point, sphericity); }
      if (ft_l) { set_linearity(&las->point, linearity); }
      if (ft_o) { set_omnivariance(&las->point, omnivariance); }
      if (ft_c) { set_curvature(&las->point, curvature); }
      if (ft_e) { set_eigensum(&las->point, eigen_sum); }
      if (ft_i) { set_angle(&las->point, angle); }
      if (ft_n)
      {
        set_normalX(&las->point, nx);
        set_normalY(&las->point, ny);
        set_normalZ(&las->point, nz);
      }

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