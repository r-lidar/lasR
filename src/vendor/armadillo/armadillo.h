// SPDX-License-Identifier: Apache-2.0
//
// Copyright 2008-2016 Conrad Sanderson (http://conradsanderson.id.au)
// Copyright 2008-2016 National ICT Australia (NICTA)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ------------------------------------------------------------------------


#ifndef ARMA_INCLUDES
#define ARMA_INCLUDES

// NOTE: functions that are designed to be user accessible are described in the documentation (docs.html).
// NOTE: all other functions and classes (ie. not explicitly described in the documentation)
// NOTE: are considered as internal implementation details, and may be changed or removed without notice.

#include "config.hpp"
#include "compiler_check.hpp"

#include <cstdlib>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <climits>
#include <cstdint>
#include <cmath>
#include <ctime>

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <new>
#include <limits>
#include <algorithm>
#include <complex>
#include <vector>
#include <utility>
#include <map>
#include <initializer_list>
#include <random>
#include <functional>
#include <chrono>
#include <atomic>

#if !defined(ARMA_DONT_USE_STD_MUTEX)
  #include <mutex>
#endif

// #if defined(ARMA_HAVE_CXX17)
//   #include <charconv>
//   #include <system_error>
// #endif

#if ( defined(__unix__) || defined(__unix) || defined(_POSIX_C_SOURCE) || (defined(__APPLE__) && defined(__MACH__)) ) && !defined(_WIN32)
  #include <unistd.h>
#endif

#if defined(ARMA_USE_TBB_ALLOC)
  #if defined(__has_include)
    #if __has_include(<tbb/scalable_allocator.h>)
      #include <tbb/scalable_allocator.h>
    #else
      #undef ARMA_USE_TBB_ALLOC
      #pragma message ("WARNING: use of TBB alloc disabled; tbb/scalable_allocator.h header not found")
    #endif
  #else
    #include <tbb/scalable_allocator.h>
  #endif
#endif

#if defined(ARMA_USE_MKL_ALLOC)
  #if defined(__has_include)
    #if __has_include(<mkl_service.h>)
      #include <mkl_service.h>
    #else
      #undef ARMA_USE_MKL_ALLOC
      #pragma message ("WARNING: use of MKL alloc disabled; mkl_service.h header not found")
    #endif
  #else
    #include <mkl_service.h>
  #endif
#endif


#include "compiler_setup.hpp"


#if defined(ARMA_USE_OPENMP)
  #if defined(__has_include)
    #if __has_include(<omp.h>)
      #include <omp.h>
    #else
      #undef ARMA_USE_OPENMP
      #pragma message ("WARNING: use of OpenMP disabled; omp.h header not found")
    #endif
  #else
    #include <omp.h>
  #endif
#endif


#include "include_hdf5.hpp"
#include "include_superlu.hpp"


//! \namespace arma namespace for Armadillo classes and functions
namespace arma
  {

  // preliminaries

  #include "arma_forward.hpp"
  #include "arma_static_check.hpp"
  #include "typedef_elem.hpp"
  #include "typedef_elem_check.hpp"
  #include "typedef_mat.hpp"
  #include "arma_str.hpp"
  #include "arma_version.hpp"
  #include "arma_config.hpp"
  #include "traits.hpp"
  #include "promote_type.hpp"
  #include "upgrade_val.hpp"
  #include "restrictors.hpp"
  #include "access.hpp"
  #include "span.hpp"
  #include "distr_param.hpp"
  #include "constants.hpp"
  #include "constants_old.hpp"
  #include "mp_misc.hpp"
  #include "arma_rel_comparators.hpp"
  #include "fill.hpp"

  #if defined(ARMA_RNG_ALT)
    #include ARMA_INCFILE_WRAP(ARMA_RNG_ALT)
  #else
    #include "arma_rng_cxx03.hpp"
  #endif

  #include "arma_rng.hpp"


  //
  // class prototypes

  #include "Base_bones.hpp"
  #include "BaseCube_bones.hpp"
  #include "SpBase_bones.hpp"

  #include "def_blas.hpp"
  #include "def_atlas.hpp"
  #include "def_lapack.hpp"
  #include "def_arpack.hpp"
  #include "def_superlu.hpp"
  #include "def_fftw3.hpp"

  #include "translate_blas.hpp"
  #include "translate_atlas.hpp"
  #include "translate_lapack.hpp"
  #include "translate_arpack.hpp"
  #include "translate_superlu.hpp"
  #include "translate_fftw3.hpp"

  #include "cond_rel_bones.hpp"
  #include "arrayops_bones.hpp"
  #include "podarray_bones.hpp"
  #include "auxlib_bones.hpp"
  #include "sp_auxlib_bones.hpp"

  #include "injector_bones.hpp"

  #include "Mat_bones.hpp"
  #include "Col_bones.hpp"
  #include "Row_bones.hpp"
  #include "Cube_bones.hpp"
  #include "xvec_htrans_bones.hpp"
  #include "xtrans_mat_bones.hpp"
  #include "SizeMat_bones.hpp"
  #include "SizeCube_bones.hpp"

  #include "SpValProxy_bones.hpp"
  #include "SpMat_bones.hpp"
  #include "SpCol_bones.hpp"
  #include "SpRow_bones.hpp"
  #include "SpSubview_bones.hpp"
  #include "SpSubview_col_list_bones.hpp"
  #include "spdiagview_bones.hpp"
  #include "MapMat_bones.hpp"

  #include "typedef_mat_fixed.hpp"

  #include "field_bones.hpp"
  #include "subview_bones.hpp"
  #include "subview_elem1_bones.hpp"
  #include "subview_elem2_bones.hpp"
  #include "subview_field_bones.hpp"
  #include "subview_cube_bones.hpp"
  #include "diagview_bones.hpp"
  #include "subview_each_bones.hpp"
  #include "subview_cube_each_bones.hpp"
  #include "subview_cube_slices_bones.hpp"

  #include "hdf5_name.hpp"
  #include "csv_name.hpp"
  #include "diskio_bones.hpp"
  #include "wall_clock_bones.hpp"
  #include "running_stat_bones.hpp"
  #include "running_stat_vec_bones.hpp"

  #include "Op_bones.hpp"
  #include "CubeToMatOp_bones.hpp"
  #include "OpCube_bones.hpp"
  #include "SpOp_bones.hpp"
  #include "SpToDOp_bones.hpp"

  #include "eOp_bones.hpp"
  #include "eOpCube_bones.hpp"

  #include "mtOp_bones.hpp"
  #include "mtOpCube_bones.hpp"
  #include "mtSpOp_bones.hpp"

  #include "Glue_bones.hpp"
  #include "eGlue_bones.hpp"
  #include "mtGlue_bones.hpp"
  #include "SpGlue_bones.hpp"
  #include "mtSpGlue_bones.hpp"
  #include "SpToDGlue_bones.hpp"

  #include "GlueCube_bones.hpp"
  #include "eGlueCube_bones.hpp"
  #include "mtGlueCube_bones.hpp"

  #include "eop_core_bones.hpp"
  #include "eglue_core_bones.hpp"

  #include "Gen_bones.hpp"
  #include "GenCube_bones.hpp"

  #include "op_diagmat_bones.hpp"
  #include "op_diagvec_bones.hpp"
  #include "op_dot_bones.hpp"
  #include "op_det_bones.hpp"
  #include "op_log_det_bones.hpp"
  #include "op_inv_gen_bones.hpp"
  #include "op_inv_spd_bones.hpp"
  #include "op_htrans_bones.hpp"
  #include "op_max_bones.hpp"
  #include "op_min_bones.hpp"
  #include "op_index_max_bones.hpp"
  #include "op_index_min_bones.hpp"
  #include "op_mean_bones.hpp"
  #include "op_median_bones.hpp"
  #include "op_sort_bones.hpp"
  #include "op_sort_index_bones.hpp"
  #include "op_sum_bones.hpp"
  #include "op_stddev_bones.hpp"
  #include "op_strans_bones.hpp"
  #include "op_var_bones.hpp"
  #include "op_repmat_bones.hpp"
  #include "op_repelem_bones.hpp"
  #include "op_reshape_bones.hpp"
  #include "op_vectorise_bones.hpp"
  #include "op_resize_bones.hpp"
  #include "op_cov_bones.hpp"
  #include "op_cor_bones.hpp"
  #include "op_shift_bones.hpp"
  #include "op_shuffle_bones.hpp"
  #include "op_prod_bones.hpp"
  #include "op_pinv_bones.hpp"
  #include "op_dotext_bones.hpp"
  #include "op_flip_bones.hpp"
  #include "op_reverse_bones.hpp"
  #include "op_princomp_bones.hpp"
  #include "op_misc_bones.hpp"
  #include "op_orth_null_bones.hpp"
  #include "op_relational_bones.hpp"
  #include "op_find_bones.hpp"
  #include "op_find_unique_bones.hpp"
  #include "op_chol_bones.hpp"
  #include "op_cx_scalar_bones.hpp"
  #include "op_trimat_bones.hpp"
  #include "op_cumsum_bones.hpp"
  #include "op_cumprod_bones.hpp"
  #include "op_symmat_bones.hpp"
  #include "op_hist_bones.hpp"
  #include "op_unique_bones.hpp"
  #include "op_toeplitz_bones.hpp"
  #include "op_fft_bones.hpp"
  #include "op_any_bones.hpp"
  #include "op_all_bones.hpp"
  #include "op_normalise_bones.hpp"
  #include "op_clamp_bones.hpp"
  #include "op_expmat_bones.hpp"
  #include "op_nonzeros_bones.hpp"
  #include "op_diff_bones.hpp"
  #include "op_norm_bones.hpp"
  #include "op_vecnorm_bones.hpp"
  #include "op_norm2est_bones.hpp"
  #include "op_sqrtmat_bones.hpp"
  #include "op_logmat_bones.hpp"
  #include "op_range_bones.hpp"
  #include "op_chi2rnd_bones.hpp"
  #include "op_wishrnd_bones.hpp"
  #include "op_roots_bones.hpp"
  #include "op_cond_bones.hpp"
  #include "op_rcond_bones.hpp"
  #include "op_sp_plus_bones.hpp"
  #include "op_sp_minus_bones.hpp"
  #include "op_powmat_bones.hpp"
  #include "op_rank_bones.hpp"
  #include "op_row_as_mat_bones.hpp"
  #include "op_col_as_mat_bones.hpp"

  #include "glue_times_bones.hpp"
  #include "glue_times_misc_bones.hpp"
  #include "glue_mixed_bones.hpp"
  #include "glue_cov_bones.hpp"
  #include "glue_cor_bones.hpp"
  #include "glue_kron_bones.hpp"
  #include "glue_cross_bones.hpp"
  #include "glue_join_bones.hpp"
  #include "glue_relational_bones.hpp"
  #include "glue_solve_bones.hpp"
  #include "glue_conv_bones.hpp"
  #include "glue_toeplitz_bones.hpp"
  #include "glue_hist_bones.hpp"
  #include "glue_histc_bones.hpp"
  #include "glue_max_bones.hpp"
  #include "glue_min_bones.hpp"
  #include "glue_trapz_bones.hpp"
  #include "glue_atan2_bones.hpp"
  #include "glue_hypot_bones.hpp"
  #include "glue_polyfit_bones.hpp"
  #include "glue_polyval_bones.hpp"
  #include "glue_intersect_bones.hpp"
  #include "glue_affmul_bones.hpp"
  #include "glue_mvnrnd_bones.hpp"
  #include "glue_quantile_bones.hpp"
  #include "glue_powext_bones.hpp"

  #include "gmm_misc_bones.hpp"
  #include "gmm_diag_bones.hpp"
  #include "gmm_full_bones.hpp"

  #include "spop_max_bones.hpp"
  #include "spop_min_bones.hpp"
  #include "spop_sum_bones.hpp"
  #include "spop_strans_bones.hpp"
  #include "spop_htrans_bones.hpp"
  #include "spop_misc_bones.hpp"
  #include "spop_diagmat_bones.hpp"
  #include "spop_mean_bones.hpp"
  #include "spop_var_bones.hpp"
  #include "spop_trimat_bones.hpp"
  #include "spop_symmat_bones.hpp"
  #include "spop_normalise_bones.hpp"
  #include "spop_reverse_bones.hpp"
  #include "spop_repmat_bones.hpp"
  #include "spop_vectorise_bones.hpp"
  #include "spop_norm_bones.hpp"
  #include "spop_vecnorm_bones.hpp"
  #include "spop_shift_bones.hpp"

  #include "spglue_plus_bones.hpp"
  #include "spglue_minus_bones.hpp"
  #include "spglue_schur_bones.hpp"
  #include "spglue_times_bones.hpp"
  #include "spglue_join_bones.hpp"
  #include "spglue_kron_bones.hpp"
  #include "spglue_min_bones.hpp"
  #include "spglue_max_bones.hpp"
  #include "spglue_merge_bones.hpp"
  #include "spglue_relational_bones.hpp"

  #include "spsolve_factoriser_bones.hpp"

  #if defined(ARMA_USE_NEWARP)
    #include "newarp_EigsSelect.hpp"
    #include "newarp_DenseGenMatProd_bones.hpp"
    #include "newarp_SparseGenMatProd_bones.hpp"
    #include "newarp_SparseGenRealShiftSolve_bones.hpp"
    #include "newarp_DoubleShiftQR_bones.hpp"
    #include "newarp_GenEigsSolver_bones.hpp"
    #include "newarp_SymEigsSolver_bones.hpp"
    #include "newarp_SymEigsShiftSolver_bones.hpp"
    #include "newarp_TridiagEigen_bones.hpp"
    #include "newarp_UpperHessenbergEigen_bones.hpp"
    #include "newarp_UpperHessenbergQR_bones.hpp"
  #endif


  //
  // low-level debugging and memory handling functions

  #include "debug.hpp"
  #include "memory.hpp"

  //
  // wrappers for various cmath functions

  #include "arma_cmath.hpp"

  //
  // classes that underlay metaprogramming

  #include "unwrap.hpp"
  #include "unwrap_cube.hpp"
  #include "unwrap_spmat.hpp"

  #include "Proxy.hpp"
  #include "ProxyCube.hpp"
  #include "SpProxy.hpp"

  #include "diagmat_proxy.hpp"

  #include "strip.hpp"

  #include "eop_aux.hpp"

  //
  // ostream

  #include "arma_ostream_bones.hpp"
  #include "arma_ostream_meat.hpp"

  //
  // n_unique, which is used by some sparse operators

  #include "fn_n_unique.hpp"

  //
  // operators

  #include "operator_plus.hpp"
  #include "operator_minus.hpp"
  #include "operator_times.hpp"
  #include "operator_schur.hpp"
  #include "operator_div.hpp"
  #include "operator_relational.hpp"

  #include "operator_cube_plus.hpp"
  #include "operator_cube_minus.hpp"
  #include "operator_cube_times.hpp"
  #include "operator_cube_schur.hpp"
  #include "operator_cube_div.hpp"
  #include "operator_cube_relational.hpp"

  #include "operator_ostream.hpp"

  //
  // user accessible functions

  // the order of the fn_*.hpp include files matters,
  // as some files require functionality given in preceding files

  #include "fn_conv_to.hpp"
  #include "fn_max.hpp"
  #include "fn_min.hpp"
  #include "fn_index_max.hpp"
  #include "fn_index_min.hpp"
  #include "fn_accu.hpp"
  #include "fn_sum.hpp"
  #include "fn_diagmat.hpp"
  #include "fn_diagvec.hpp"
  #include "fn_inv.hpp"
  #include "fn_inv_sympd.hpp"
  #include "fn_trace.hpp"
  #include "fn_trans.hpp"
  #include "fn_det.hpp"
  #include "fn_log_det.hpp"
  #include "fn_eig_gen.hpp"
  #include "fn_eig_sym.hpp"
  #include "fn_eig_pair.hpp"
  #include "fn_lu.hpp"
  #include "fn_zeros.hpp"
  #include "fn_ones.hpp"
  #include "fn_eye.hpp"
  #include "fn_misc.hpp"
  #include "fn_orth_null.hpp"
  #include "fn_regspace.hpp"
  #include "fn_find.hpp"
  #include "fn_find_unique.hpp"
  #include "fn_elem.hpp"
  #include "fn_approx_equal.hpp"
  #include "fn_norm.hpp"
  #include "fn_vecnorm.hpp"
  #include "fn_dot.hpp"
  #include "fn_randu.hpp"
  #include "fn_randn.hpp"
  #include "fn_trig.hpp"
  #include "fn_mean.hpp"
  #include "fn_median.hpp"
  #include "fn_stddev.hpp"
  #include "fn_var.hpp"
  #include "fn_sort.hpp"
  #include "fn_sort_index.hpp"
  #include "fn_strans.hpp"
  #include "fn_chol.hpp"
  #include "fn_qr.hpp"
  #include "fn_svd.hpp"
  #include "fn_solve.hpp"
  #include "fn_repmat.hpp"
  #include "fn_repelem.hpp"
  #include "fn_reshape.hpp"
  #include "fn_vectorise.hpp"
  #include "fn_resize.hpp"
  #include "fn_cov.hpp"
  #include "fn_cor.hpp"
  #include "fn_shift.hpp"
  #include "fn_shuffle.hpp"
  #include "fn_prod.hpp"
  #include "fn_eps.hpp"
  #include "fn_pinv.hpp"
  #include "fn_rank.hpp"
  #include "fn_kron.hpp"
  #include "fn_flip.hpp"
  #include "fn_reverse.hpp"
  #include "fn_as_scalar.hpp"
  #include "fn_princomp.hpp"
  #include "fn_cross.hpp"
  #include "fn_join.hpp"
  #include "fn_conv.hpp"
  #include "fn_trunc_exp.hpp"
  #include "fn_trunc_log.hpp"
  #include "fn_toeplitz.hpp"
  #include "fn_trimat.hpp"
  #include "fn_trimat_ind.hpp"
  #include "fn_cumsum.hpp"
  #include "fn_cumprod.hpp"
  #include "fn_symmat.hpp"
  #include "fn_sylvester.hpp"
  #include "fn_hist.hpp"
  #include "fn_histc.hpp"
  #include "fn_unique.hpp"
  #include "fn_fft.hpp"
  #include "fn_fft2.hpp"
  #include "fn_any.hpp"
  #include "fn_all.hpp"
  #include "fn_size.hpp"
  #include "fn_numel.hpp"
  #include "fn_inplace_strans.hpp"
  #include "fn_inplace_trans.hpp"
  #include "fn_randi.hpp"
  #include "fn_randg.hpp"
  #include "fn_cond_rcond.hpp"
  #include "fn_normalise.hpp"
  #include "fn_clamp.hpp"
  #include "fn_expmat.hpp"
  #include "fn_nonzeros.hpp"
  #include "fn_interp1.hpp"
  #include "fn_interp2.hpp"
  #include "fn_qz.hpp"
  #include "fn_diff.hpp"
  #include "fn_hess.hpp"
  #include "fn_schur.hpp"
  #include "fn_kmeans.hpp"
  #include "fn_sqrtmat.hpp"
  #include "fn_logmat.hpp"
  #include "fn_trapz.hpp"
  #include "fn_range.hpp"
  #include "fn_polyfit.hpp"
  #include "fn_polyval.hpp"
  #include "fn_intersect.hpp"
  #include "fn_normpdf.hpp"
  #include "fn_log_normpdf.hpp"
  #include "fn_normcdf.hpp"
  #include "fn_mvnrnd.hpp"
  #include "fn_chi2rnd.hpp"
  #include "fn_wishrnd.hpp"
  #include "fn_roots.hpp"
  #include "fn_randperm.hpp"
  #include "fn_quantile.hpp"
  #include "fn_powmat.hpp"
  #include "fn_powext.hpp"
  #include "fn_diags_spdiags.hpp"

  #include "fn_speye.hpp"
  #include "fn_spones.hpp"
  #include "fn_sprandn.hpp"
  #include "fn_sprandu.hpp"
  #include "fn_eigs_sym.hpp"
  #include "fn_eigs_gen.hpp"
  #include "fn_spsolve.hpp"
  #include "fn_svds.hpp"

  //
  // misc stuff

  #include "hdf5_misc.hpp"
  #include "fft_engine_kissfft.hpp"
  #include "fft_engine_fftw3.hpp"
  #include "band_helper.hpp"
  #include "sym_helper.hpp"
  #include "trimat_helper.hpp"

  //
  // classes implementing various forms of dense matrix multiplication

  #include "mul_gemv.hpp"
  #include "mul_gemm.hpp"
  #include "mul_gemm_mixed.hpp"
  #include "mul_syrk.hpp"
  #include "mul_herk.hpp"

  //
  // class meat

  #include "Op_meat.hpp"
  #include "CubeToMatOp_meat.hpp"
  #include "OpCube_meat.hpp"
  #include "SpOp_meat.hpp"
  #include "SpToDOp_meat.hpp"

  #include "mtOp_meat.hpp"
  #include "mtOpCube_meat.hpp"
  #include "mtSpOp_meat.hpp"

  #include "Glue_meat.hpp"
  #include "GlueCube_meat.hpp"
  #include "SpGlue_meat.hpp"
  #include "mtSpGlue_meat.hpp"
  #include "SpToDGlue_meat.hpp"

  #include "eOp_meat.hpp"
  #include "eOpCube_meat.hpp"

  #include "eGlue_meat.hpp"
  #include "eGlueCube_meat.hpp"

  #include "mtGlue_meat.hpp"
  #include "mtGlueCube_meat.hpp"

  #include "Base_meat.hpp"
  #include "BaseCube_meat.hpp"
  #include "SpBase_meat.hpp"

  #include "Gen_meat.hpp"
  #include "GenCube_meat.hpp"

  #include "eop_core_meat.hpp"
  #include "eglue_core_meat.hpp"

  #include "cond_rel_meat.hpp"
  #include "arrayops_meat.hpp"
  #include "podarray_meat.hpp"
  #include "auxlib_meat.hpp"
  #include "sp_auxlib_meat.hpp"

  #include "injector_meat.hpp"

  #include "Mat_meat.hpp"
  #include "Col_meat.hpp"
  #include "Row_meat.hpp"
  #include "Cube_meat.hpp"
  #include "xvec_htrans_meat.hpp"
  #include "xtrans_mat_meat.hpp"
  #include "SizeMat_meat.hpp"
  #include "SizeCube_meat.hpp"

  #include "field_meat.hpp"
  #include "subview_meat.hpp"
  #include "subview_elem1_meat.hpp"
  #include "subview_elem2_meat.hpp"
  #include "subview_field_meat.hpp"
  #include "subview_cube_meat.hpp"
  #include "diagview_meat.hpp"
  #include "subview_each_meat.hpp"
  #include "subview_cube_each_meat.hpp"
  #include "subview_cube_slices_meat.hpp"

  #include "SpValProxy_meat.hpp"
  #include "SpMat_meat.hpp"
  #include "SpMat_iterators_meat.hpp"
  #include "SpCol_meat.hpp"
  #include "SpRow_meat.hpp"
  #include "SpSubview_meat.hpp"
  #include "SpSubview_iterators_meat.hpp"
  #include "SpSubview_col_list_meat.hpp"
  #include "spdiagview_meat.hpp"
  #include "MapMat_meat.hpp"

  #include "diskio_meat.hpp"
  #include "wall_clock_meat.hpp"
  #include "running_stat_meat.hpp"
  #include "running_stat_vec_meat.hpp"

  #include "op_diagmat_meat.hpp"
  #include "op_diagvec_meat.hpp"
  #include "op_dot_meat.hpp"
  #include "op_det_meat.hpp"
  #include "op_log_det_meat.hpp"
  #include "op_inv_gen_meat.hpp"
  #include "op_inv_spd_meat.hpp"
  #include "op_htrans_meat.hpp"
  #include "op_max_meat.hpp"
  #include "op_index_max_meat.hpp"
  #include "op_index_min_meat.hpp"
  #include "op_min_meat.hpp"
  #include "op_mean_meat.hpp"
  #include "op_median_meat.hpp"
  #include "op_sort_meat.hpp"
  #include "op_sort_index_meat.hpp"
  #include "op_sum_meat.hpp"
  #include "op_stddev_meat.hpp"
  #include "op_strans_meat.hpp"
  #include "op_var_meat.hpp"
  #include "op_repmat_meat.hpp"
  #include "op_repelem_meat.hpp"
  #include "op_reshape_meat.hpp"
  #include "op_vectorise_meat.hpp"
  #include "op_resize_meat.hpp"
  #include "op_cov_meat.hpp"
  #include "op_cor_meat.hpp"
  #include "op_shift_meat.hpp"
  #include "op_shuffle_meat.hpp"
  #include "op_prod_meat.hpp"
  #include "op_pinv_meat.hpp"
  #include "op_dotext_meat.hpp"
  #include "op_flip_meat.hpp"
  #include "op_reverse_meat.hpp"
  #include "op_princomp_meat.hpp"
  #include "op_misc_meat.hpp"
  #include "op_orth_null_meat.hpp"
  #include "op_relational_meat.hpp"
  #include "op_find_meat.hpp"
  #include "op_find_unique_meat.hpp"
  #include "op_chol_meat.hpp"
  #include "op_cx_scalar_meat.hpp"
  #include "op_trimat_meat.hpp"
  #include "op_cumsum_meat.hpp"
  #include "op_cumprod_meat.hpp"
  #include "op_symmat_meat.hpp"
  #include "op_hist_meat.hpp"
  #include "op_unique_meat.hpp"
  #include "op_toeplitz_meat.hpp"
  #include "op_fft_meat.hpp"
  #include "op_any_meat.hpp"
  #include "op_all_meat.hpp"
  #include "op_normalise_meat.hpp"
  #include "op_clamp_meat.hpp"
  #include "op_expmat_meat.hpp"
  #include "op_nonzeros_meat.hpp"
  #include "op_diff_meat.hpp"
  #include "op_norm_meat.hpp"
  #include "op_vecnorm_meat.hpp"
  #include "op_norm2est_meat.hpp"
  #include "op_sqrtmat_meat.hpp"
  #include "op_logmat_meat.hpp"
  #include "op_range_meat.hpp"
  #include "op_chi2rnd_meat.hpp"
  #include "op_wishrnd_meat.hpp"
  #include "op_roots_meat.hpp"
  #include "op_cond_meat.hpp"
  #include "op_rcond_meat.hpp"
  #include "op_sp_plus_meat.hpp"
  #include "op_sp_minus_meat.hpp"
  #include "op_powmat_meat.hpp"
  #include "op_rank_meat.hpp"
  #include "op_row_as_mat_meat.hpp"
  #include "op_col_as_mat_meat.hpp"

  #include "glue_times_meat.hpp"
  #include "glue_times_misc_meat.hpp"
  #include "glue_mixed_meat.hpp"
  #include "glue_cov_meat.hpp"
  #include "glue_cor_meat.hpp"
  #include "glue_kron_meat.hpp"
  #include "glue_cross_meat.hpp"
  #include "glue_join_meat.hpp"
  #include "glue_relational_meat.hpp"
  #include "glue_solve_meat.hpp"
  #include "glue_conv_meat.hpp"
  #include "glue_toeplitz_meat.hpp"
  #include "glue_hist_meat.hpp"
  #include "glue_histc_meat.hpp"
  #include "glue_max_meat.hpp"
  #include "glue_min_meat.hpp"
  #include "glue_trapz_meat.hpp"
  #include "glue_atan2_meat.hpp"
  #include "glue_hypot_meat.hpp"
  #include "glue_polyfit_meat.hpp"
  #include "glue_polyval_meat.hpp"
  #include "glue_intersect_meat.hpp"
  #include "glue_affmul_meat.hpp"
  #include "glue_mvnrnd_meat.hpp"
  #include "glue_quantile_meat.hpp"
  #include "glue_powext_meat.hpp"

  #include "gmm_misc_meat.hpp"
  #include "gmm_diag_meat.hpp"
  #include "gmm_full_meat.hpp"

  #include "spop_max_meat.hpp"
  #include "spop_min_meat.hpp"
  #include "spop_sum_meat.hpp"
  #include "spop_strans_meat.hpp"
  #include "spop_htrans_meat.hpp"
  #include "spop_misc_meat.hpp"
  #include "spop_diagmat_meat.hpp"
  #include "spop_mean_meat.hpp"
  #include "spop_var_meat.hpp"
  #include "spop_trimat_meat.hpp"
  #include "spop_symmat_meat.hpp"
  #include "spop_normalise_meat.hpp"
  #include "spop_reverse_meat.hpp"
  #include "spop_repmat_meat.hpp"
  #include "spop_vectorise_meat.hpp"
  #include "spop_norm_meat.hpp"
  #include "spop_vecnorm_meat.hpp"
  #include "spop_shift_meat.hpp"

  #include "spglue_plus_meat.hpp"
  #include "spglue_minus_meat.hpp"
  #include "spglue_schur_meat.hpp"
  #include "spglue_times_meat.hpp"
  #include "spglue_join_meat.hpp"
  #include "spglue_kron_meat.hpp"
  #include "spglue_min_meat.hpp"
  #include "spglue_max_meat.hpp"
  #include "spglue_merge_meat.hpp"
  #include "spglue_relational_meat.hpp"

  #include "spsolve_factoriser_meat.hpp"

  #if defined(ARMA_USE_NEWARP)
    #include "newarp_cx_attrib.hpp"
    #include "newarp_SortEigenvalue.hpp"
    #include "newarp_DenseGenMatProd_meat.hpp"
    #include "newarp_SparseGenMatProd_meat.hpp"
    #include "newarp_SparseGenRealShiftSolve_meat.hpp"
    #include "newarp_DoubleShiftQR_meat.hpp"
    #include "newarp_GenEigsSolver_meat.hpp"
    #include "newarp_SymEigsSolver_meat.hpp"
    #include "newarp_SymEigsShiftSolver_meat.hpp"
    #include "newarp_TridiagEigen_meat.hpp"
    #include "newarp_UpperHessenbergEigen_meat.hpp"
    #include "newarp_UpperHessenbergQR_meat.hpp"
  #endif
  }



#include "compiler_setup_post.hpp"

#endif
