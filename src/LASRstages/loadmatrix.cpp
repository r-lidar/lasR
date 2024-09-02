#include "loadmatrix.h"

bool LASRloadmatrix::set_parameters(const nlohmann::json& stage)
{
  std::vector<double> M = get_vector<double>(stage["matrix"]);

  if (M.size() != 16)
  {
    last_error = "the matrix should have 16 elements";
    return false;
  }

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      matrix[i][j] = M[i * 4 + j];
    }
  }

  // Check if it is a valid composed matrix
  // --------------------------------------
  auto R = matrix;

  // Calculate R^T * R
  double identity[3][3] = {0};  // Should be close to identity if R is a valid rotation matrix

  // Multiply R^T * R
  for (int i = 0; i < 3; ++i)
  {
    for (int j = 0; j < 3; ++j)
    {
      identity[i][j] = 0;
      for (int k = 0; k < 3; ++k)
      {
        identity[i][j] += R[k][i] * R[k][j];
      }
    }
  }

  // Check if the result is close to the identity matrix
  double tolerance = 1e-6;
  for (int i = 0; i < 3; ++i)
  {
    for (int j = 0; j < 3; ++j)
    {
      if (i == j && std::abs(identity[i][j] - 1.0) > tolerance)
      {
        last_error = "the matrix is not orthogonal";
        return false;  // Diagonal elements should be 1
      }
      if (i != j && std::abs(identity[i][j]) > tolerance)
      {
        last_error = "the matrix is not orthogonal";
        return false;  // Off-diagonal elements should be 0
      }
    }
  }

  // Check if the determinant is close to 1
  double determinant =
    R[0][0] * (R[1][1] * R[2][2] - R[1][2] * R[2][1]) -
    R[0][1] * (R[1][0] * R[2][2] - R[1][2] * R[2][0]) +
    R[0][2] * (R[1][0] * R[2][1] - R[1][1] * R[2][0]);

  if (std::abs(determinant - 1.0) > tolerance)
  {
    last_error = "the matrix is not valid. The determinant of the rotationnal part is not 1";
    return false;  // Determinant should be 1
  }

  return true;
}