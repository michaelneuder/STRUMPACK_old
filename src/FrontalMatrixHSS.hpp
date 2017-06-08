/*
 * STRUMPACK -- STRUctured Matrices PACKage, Copyright (c) 2014, The Regents of
 * the University of California, through Lawrence Berkeley National Laboratory
 * (subject to receipt of any required approvals from the U.S. Dept. of Energy).
 * All rights reserved.
 *
 * If you have questions about your rights to use or distribute this software,
 * please contact Berkeley Lab's Technology Transfer Department at TTD@lbl.gov.
 *
 * NOTICE. This software is owned by the U.S. Department of Energy. As such, the
 * U.S. Government has been granted for itself and others acting on its behalf a
 * paid-up, nonexclusive, irrevocable, worldwide license in the Software to
 * reproduce, prepare derivative works, and perform publicly and display publicly.
 * Beginning five (5) years after the date permission to assert copyright is
 * obtained from the U.S. Department of Energy, and subject to any subsequent five
 * (5) year renewals, the U.S. Government is granted for itself and others acting
 * on its behalf a paid-up, nonexclusive, irrevocable, worldwide license in the
 * Software to reproduce, prepare derivative works, distribute copies to the
 * public, perform publicly and display publicly, and to permit others to do so.
 *
 * Developers: Pieter Ghysels, Francois-Henry Rouet, Xiaoye S. Li.
 *             (Lawrence Berkeley National Lab, Computational Research Division).
 *
 */
#ifndef FRONTAL_MATRIX_HSS_HPP
#define FRONTAL_MATRIX_HSS_HPP

#include <iostream>
#include <algorithm>
#include <memory>
#include "FrontalMatrix.hpp"
#include "CompressedSparseMatrix.hpp"
#include "MatrixReordering.hpp"
#include "HSS/HSSMatrix.hpp"
#include "TaskTimer.hpp"

namespace strumpack {

  // forward declarations
  template<typename scalar_t,typename integer_t> class FrontalMatrixDense;

  template<typename scalar_t,typename integer_t> class FrontalMatrixHSS
    : public FrontalMatrix<scalar_t,integer_t> {
    //using real_t = typename RealType<scalar_t>::value_type
    using F_t = FrontalMatrix<scalar_t,integer_t>;
    using DenseM_t = DenseMatrix<scalar_t>;
    using DenseMW_t = DenseMatrixWrapper<scalar_t>;
  public:
    HSS::HSSMatrix<scalar_t> _H;
    HSS::HSSFactors<scalar_t> _ULV;
    // TODO do not store this here: makes solve not thread safe!!
    std::unique_ptr<HSS::WorkSolve<scalar_t>> _ULVwork;

    /** Schur complement update:
     *    S = F22 - _Theta * Vhat^C * _Phi^C
     **/
    DenseM_t _Theta;
    DenseM_t _Phi;
    DenseM_t _ThetaVhatC_or_VhatCPhiC;
    DenseM_t _DUB01;

    /** these are saved during/after randomized compression and are
    	then later used to sample the Schur complement when
    	compressing the parent front */
    DenseM_t R1;        /* top of the random matrix used to construct
    			   HSS matrix of this front */
    DenseM_t Sr2, Sc2;  /* bottom of the sample matrix used to
    			   construct HSS matrix of this front */
    std::uint32_t _sampled_columns = 0;

    FrontalMatrixHSS(CompressedSparseMatrix<scalar_t,integer_t>* _A,
		     integer_t _sep, integer_t _sep_begin, integer_t _sep_end,
		     integer_t _dim_upd, integer_t* _upd);
    ~FrontalMatrixHSS() {}
    void extend_add_to_dense(FrontalMatrixDense<scalar_t,integer_t>* p, int task_depth);

    void sample_CB(const SPOptions<scalar_t>& opts, DenseM_t& R, DenseM_t& Sr, DenseM_t& Sc,
		   F_t* pa, int task_depth);
    void sample_CB_direct(const DenseM_t& cR, DenseM_t& Sr, DenseM_t& Sc,
     			  const std::vector<std::size_t>& I, int task_depth);
    void release_work_memory();
    void random_sampling(const SPOptions<scalar_t>& opts, DenseM_t& Rr, DenseM_t& Rc,
			 DenseM_t& Sr, DenseM_t& Sc, int etree_level, int task_depth);
    void element_extraction(const std::vector<std::size_t>& I, const std::vector<std::size_t>& J,
			    DenseM_t& B, int task_depth);
    void extract_CB_sub_matrix(const std::vector<std::size_t>& I, const std::vector<std::size_t>& J,
			       DenseM_t& B, int task_depth) const;

    void multifrontal_factorization(const SPOptions<scalar_t>& opts, int etree_level=0, int task_depth=0);
    void forward_multifrontal_solve(scalar_t* b, scalar_t* wmem, int etree_level=0, int task_depth=0);
    void backward_multifrontal_solve(scalar_t* y, scalar_t* wmem, int etree_level=0, int task_depth=0);


    integer_t maximum_rank(int task_depth=0) const;
    void print_rank_statistics(std::ostream &out) const;
    bool isHSS() const { return true; };
    std::string type() const { return "FrontalMatrixHSS"; }

    int random_samples() const { return R1.cols(); };
    void bisection_partitioning(const SPOptions<scalar_t>& opts, integer_t* sorder,
    				bool isroot=true, int task_depth=0);

    void set_HSS_partitioning(const SPOptions<scalar_t>& opts, const HSS::HSSPartitionTree& sep_tree, bool is_root);

  private:
    FrontalMatrixHSS(const FrontalMatrixHSS&) = delete;
    FrontalMatrixHSS& operator=(FrontalMatrixHSS const&) = delete;
    void multifrontal_factorization_node(const SPOptions<scalar_t>& opts, int etree_level, int task_depth);
    void forward_multifrontal_solve_node(scalar_t* b, scalar_t* wmem, int etree_level, int task_depth);
    void backward_multifrontal_solve_node(scalar_t* y, scalar_t* wmem, int etree_level, int task_depth);
    long long node_factor_nonzeros() const;
    long long dense_node_factor_nonzeros() const;

    void split_separator(const SPOptions<scalar_t>& opts, HSS::HSSPartitionTree& hss_tree,
			 integer_t& nr_parts, integer_t part, integer_t count, integer_t* sorder);
    void extract_separator(const SPOptions<scalar_t>& opts, integer_t part, std::vector<idx_t>& xadj,
			   std::vector<idx_t>& adjncy, integer_t* sorder);
  };

  template<typename scalar_t,typename integer_t>
  FrontalMatrixHSS<scalar_t,integer_t>::FrontalMatrixHSS
  (CompressedSparseMatrix<scalar_t,integer_t>* _A, integer_t _sep,
   integer_t _sep_begin, integer_t _sep_end, integer_t _dim_upd, integer_t* _upd)
    : FrontalMatrix<scalar_t,integer_t>(_A, NULL, NULL, _sep, _sep_begin, _sep_end, _dim_upd, _upd) {
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::release_work_memory() {
    _ThetaVhatC_or_VhatCPhiC.clear();
    _H.delete_trailing_block();
    R1.clear();
    Sr2.clear();
    Sc2.clear();
    _DUB01.clear();
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::extend_add_to_dense
  (FrontalMatrixDense<scalar_t,integer_t>* p, int task_depth) {
    const std::size_t pdsep = p->dim_sep;
    const std::size_t dupd = this->dim_upd;
    std::size_t upd2sep;
    auto I = this->upd_to_parent(p, upd2sep);

    auto F22 = _H.child(1)->dense();
    if (_Theta.cols() < _Phi.cols()) // S = F22 - _Theta * _ThetaVhatC_or_VhatCPhiC
      gemm(Trans::N, Trans::N, scalar_t(-1.), _Theta, _ThetaVhatC_or_VhatCPhiC,
	   scalar_t(1.), F22, task_depth);
    else // S = F22 - _ThetaVhatC_or_VhatCPhiC * _Phi'
      gemm(Trans::N, Trans::C, scalar_t(-1.), _ThetaVhatC_or_VhatCPhiC, _Phi,
	   scalar_t(1.), F22, task_depth);

    //#pragma omp taskloop default(shared) grainsize(64) if(task_depth < params::task_recursion_cutoff_level)
    for (std::size_t c=0; c<dupd; c++) {
      std::size_t pc = I[c];
      if (pc < pdsep) {
	for (std::size_t r=0; r<upd2sep; r++) p->F11(I[r],pc) += F22(r,c);
	for (std::size_t r=upd2sep; r<dupd; r++) p->F21(I[r]-pdsep,pc) += F22(r,c);
      } else {
	for (std::size_t r=0; r<upd2sep; r++) p->F12(I[r],pc-pdsep) += F22(r,c);
	for (std::size_t r=upd2sep; r<dupd; r++) p->F22(I[r]-pdsep,pc-pdsep) += F22(r,c);
      }
    }
    STRUMPACK_FLOPS((is_complex<scalar_t>()?2:1)*static_cast<long long int>(dupd*dupd));
    release_work_memory();
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::extract_CB_sub_matrix
  (const std::vector<std::size_t>& I, const std::vector<std::size_t>& J, DenseM_t& B, int task_depth) const {
    std::vector<std::size_t> lJ, oJ;
    this->find_upd_indices(J, lJ, oJ);
    if (lJ.empty()) return;
    std::vector<std::size_t> lI, oI;
    this->find_upd_indices(I, lI, oI);
    if (lI.empty()) return;

    auto M = _H.child(1)->extract(lI, lJ);
    for (std::size_t j=0; j<lJ.size(); j++)
      for (std::size_t i=0; i<lI.size(); i++)
	B(oI[i], oJ[j]) += M(i, j);

    if (_Theta.cols() < _Phi.cols()) { // S = F22 - _Theta * _ThetaVhatC_or_VhatCPhiC
      auto r = _Theta.cols();
      DenseM_t r_theta(lI.size(), r);
      DenseM_t c_vhatphiC(r, lJ.size());
      for (std::size_t i=0; i<lI.size(); i++) copy(1, r, _Theta, lI[i], 0, r_theta, i, 0);
      for (std::size_t j=0; j<lJ.size(); j++) copy(r, 1, _ThetaVhatC_or_VhatCPhiC, 0, lJ[j], c_vhatphiC, 0, j);
      gemm(Trans::N, Trans::N, scalar_t(-1.), r_theta, c_vhatphiC, scalar_t(0.), M, task_depth);
      for (std::size_t j=0; j<lJ.size(); j++)
	for (std::size_t i=0; i<lI.size(); i++)
	  B(oI[i], oJ[j]) += M(i, j);
    } else { // S = F22 - _ThetaVhatC_or_VhatCPhiC * _Phi'
      auto r = _Phi.cols();
      DenseM_t r_thetavhat(lI.size(), r);
      DenseM_t r_phi(lJ.size(), r);
      for (std::size_t i=0; i<lI.size(); i++) copy(1, r, _ThetaVhatC_or_VhatCPhiC, lI[i], 0, r_thetavhat, i, 0);
      for (std::size_t j=0; j<lJ.size(); j++) copy(1, r, _Phi, lJ[j], 0, r_phi, j, 0);
      gemm(Trans::N, Trans::C, scalar_t(-1.), r_thetavhat, r_phi, scalar_t(0.), M, task_depth);
      for (std::size_t j=0; j<lJ.size(); j++)
	for (std::size_t i=0; i<lI.size(); i++)
	  B(oI[i], oJ[j]) += M(i, j);
    }
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::sample_CB
  (const SPOptions<scalar_t>& opts, DenseM_t& R, DenseM_t& Sr, DenseM_t& Sc, F_t* pa, int task_depth) {
    if (!this->dim_upd) return;
    auto I = this->upd_to_parent(pa);
    auto cR = R.extract_rows(I);
    auto dchild = R1.cols();
    auto dall = R.cols();
    if (dchild > 0 && opts.indirect_sampling()) {
      DenseM_t cSr, cSc;
      DenseMW_t cRd0(cR.rows(), dchild, cR, 0, 0);
      _H.Schur_product_indirect(_ULV, _DUB01, R1, cRd0, Sr2, Sc2, cSr, cSc);
      DenseMW_t(Sr.rows(), dchild, Sr, 0, 0).scatter_rows_add(I, cSr);
      DenseMW_t(Sc.rows(), dchild, Sc, 0, 0).scatter_rows_add(I, cSc);
      R1.clear();
      Sr2.clear();
      Sc2.clear();
      if (dall > dchild) {
    	DenseMW_t Srdd(Sr.rows(), dall-dchild, Sr, 0, dchild);
    	DenseMW_t Scdd(Sc.rows(), dall-dchild, Sc, 0, dchild);
    	DenseMW_t cRdd(cR.rows(), dall-dchild, cR, 0, dchild);
    	sample_CB_direct(cRdd, Srdd, Scdd, I, task_depth);
      }
    } else sample_CB_direct(cR, Sr, Sc, I, task_depth);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::sample_CB_direct
  (const DenseM_t& cR, DenseM_t& Sr, DenseM_t& Sc, const std::vector<std::size_t>& I, int task_depth) {
#if 0
    auto cSr = _H.child(1)->apply(cR);
    auto cSc = _H.child(1)->applyC(cR);
    if (_Theta.cols() < _Phi.cols()) {
      // TODO 2 tasks?
      DenseM_t tmp(_ThetaVhatC_or_VhatCPhiC.rows(), cR.cols());
      gemm(Trans::N, Trans::N, scalar_t(1.), _ThetaVhatC_or_VhatCPhiC, cR, scalar_t(0.), tmp, task_depth);
      gemm(Trans::N, Trans::N, scalar_t(-1.), _Theta, tmp, scalar_t(1.), cSr, task_depth);
      tmp = DenseM_t(_Theta.cols(), cR.cols());
      gemm(Trans::C, Trans::N, scalar_t(1.), _Theta, cR, scalar_t(0.), tmp, task_depth);
      gemm(Trans::C, Trans::N, scalar_t(-1.), _ThetaVhatC_or_VhatCPhiC, tmp, scalar_t(1.), cSc, task_depth);
    } else {
      DenseM_t tmp(_Phi.cols(), cR.cols());
      gemm(Trans::C, Trans::N, scalar_t(1.), _Phi, cR, scalar_t(0.), tmp, task_depth);
      gemm(Trans::N, Trans::N, scalar_t(-1.), _ThetaVhatC_or_VhatCPhiC, tmp, scalar_t(1.), cSr, task_depth);
      tmp = DenseM_t(_ThetaVhatC_or_VhatCPhiC.cols(), cR.cols());
      gemm(Trans::C, Trans::N, scalar_t(1.), _ThetaVhatC_or_VhatCPhiC, cR, scalar_t(0.), tmp, task_depth);
      gemm(Trans::N, Trans::N, scalar_t(-1.), _Phi, tmp, scalar_t(1.), cSc, task_depth);
    }
#else
    DenseM_t cSr(cR.rows(), cR.cols());
    DenseM_t cSc(cR.rows(), cR.cols());
    _H.Schur_product_direct(_ULV, _Theta, _DUB01, _Phi, _ThetaVhatC_or_VhatCPhiC, cR, cSr, cSc);
#endif
    Sr.scatter_rows_add(I, cSr);
    Sc.scatter_rows_add(I, cSc);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::multifrontal_factorization
  (const SPOptions<scalar_t>& opts, int etree_level, int task_depth) {
    if (task_depth == 0)
#pragma omp parallel if(!omp_in_parallel())
#pragma omp single
      multifrontal_factorization_node(opts, etree_level, task_depth);
    else multifrontal_factorization_node(opts, etree_level, task_depth);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::random_sampling
  (const SPOptions<scalar_t>& opts, DenseM_t& Rr, DenseM_t& Rc,
   DenseM_t& Sr, DenseM_t& Sc, int etree_level, int task_depth) {
    auto f0 = params::flops;
    Sr.zero();
    Sc.zero();

    if (opts.indirect_sampling()) {
      auto rgen = random::make_random_generator<real_t>
	(opts.HSS_options().random_engine(), opts.HSS_options().random_distribution());
      auto dd = opts.HSS_options().dd();
      auto d0 = opts.HSS_options().d0();
      auto d = Rr.cols();
      auto m = Rr.rows();
      if (d0 % dd == 0) {
	for (std::uint32_t c=0; c<d; c+=dd) {
	  std::uint32_t r = 0, cs = c + _sampled_columns;
	  for (; r<this->dim_sep; r++) {
	    rgen->seed(std::uint32_t(r+this->sep_begin), cs);
	    for (std::uint32_t cc=c; cc<c+dd; cc++) Rr(r,cc) = Rc(r,cc) = rgen->get();
	  }
	  for (; r<m; r++) {
	    rgen->seed(std::uint32_t(this->upd[r-this->dim_sep]), cs);
	    for (std::uint32_t cc=c; cc<c+dd; cc++) Rr(r,cc) = Rc(r,cc) = rgen->get();
	  }
	}
      } else {
	for (std::uint32_t c=0; c<d; c++) {
	  std::uint32_t r = 0, cs = c + _sampled_columns;
	  for (; r<this->dim_sep; r++) Rr(r,c) = Rc(r,c) = rgen->get(r+this->sep_begin, cs);
	  for (; r<m; r++) Rr(r,c) = Rc(r,c) = rgen->get(this->upd[r-this->dim_sep], cs);
	}
      }
      STRUMPACK_FLOPS(rgen->flops_per_prng()*d*m);
      params::random_flops += params::flops - f0;
    }

    f0 = params::flops;
    this->A->front_multiply(this->sep_begin, this->sep_end, this->upd, this->dim_upd, Rr, Sr, Sc);
    params::sparse_sample_flops += params::flops - f0;
    auto f1 = params::flops;
    if (this->lchild) this->lchild->sample_CB(opts, Rr, Sr, Sc, this, task_depth);
    if (this->rchild) this->rchild->sample_CB(opts, Rr, Sr, Sc, this, task_depth);
    params::CB_sample_flops += params::flops - f1;

    if (opts.indirect_sampling() && etree_level != 0) {
      auto dold = R1.cols();
      auto dd = Rr.cols();
      auto dnew = dold + dd;
      R1.resize(this->dim_sep, dnew);
      Sr2.resize(this->dim_upd, dnew);
      Sc2.resize(this->dim_upd, dnew);
      copy(this->dim_sep, dd, Rr, 0, 0, R1, 0, dold);
      copy(this->dim_upd, dd, Sr, this->dim_sep, 0, Sr2, 0, dold);
      copy(this->dim_upd, dd, Sc, this->dim_sep, 0, Sc2, 0, dold);
    }
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::element_extraction
  (const std::vector<std::size_t>& I, const std::vector<std::size_t>& J, DenseM_t& B, int task_depth) {
    std::vector<std::size_t> gI, gJ;
    gI.reserve(I.size());
    gJ.reserve(J.size());
    for (auto i : I) gI.push_back((i < this->dim_sep) ? i+this->sep_begin : this->upd[i-this->dim_sep]);
    for (auto j : J) gJ.push_back((j < this->dim_sep) ? j+this->sep_begin : this->upd[j-this->dim_sep]);
    this->A->extract_separator(this->sep_end, gI, gJ, B, task_depth);
    if (this->lchild) this->lchild->extract_CB_sub_matrix(gI, gJ, B, task_depth);
    if (this->rchild) this->rchild->extract_CB_sub_matrix(gI, gJ, B, task_depth);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::multifrontal_factorization_node
  (const SPOptions<scalar_t>& opts, int etree_level, int task_depth) {
    bool tasked = task_depth < params::task_recursion_cutoff_level;
    if (tasked) {
      if (this->lchild)
#pragma omp task default(shared) final(task_depth >= params::task_recursion_cutoff_level-1) mergeable
	this->lchild->multifrontal_factorization(opts, etree_level+1, task_depth+1);
      if (this->rchild)
#pragma omp task default(shared) final(task_depth >= params::task_recursion_cutoff_level-1) mergeable
	this->rchild->multifrontal_factorization(opts, etree_level+1, task_depth+1);
#pragma omp taskwait
    } else {
      if (this->lchild) this->lchild->multifrontal_factorization(opts, etree_level+1, task_depth);
      if (this->rchild) this->rchild->multifrontal_factorization(opts, etree_level+1, task_depth);
    }
    _H.set_openmp_task_depth(task_depth);
    auto mult = [&](DenseM_t& Rr, DenseM_t& Rc, DenseM_t& Sr, DenseM_t& Sc) {
      auto f0 = params::flops;
      random_sampling(opts, Rr, Rc, Sr, Sc, etree_level, task_depth);
      params::sample_flops += params::flops - f0;
      if (_sampled_columns == 0) params::initial_sample_flops += params::flops - f0;
      _sampled_columns += Rr.cols();
    };
    auto elem = [&](const std::vector<std::size_t>& I, const std::vector<std::size_t>& J, DenseM_t& B) {
      auto f0 = params::flops;
      element_extraction(I, J, B, task_depth);
      params::extraction_flops += params::flops - f0;
    };
    auto f0 = params::flops;
    auto HSSopts = opts.HSS_options();
    int child_samples = 0;
    if (this->lchild) child_samples = this->lchild->random_samples();
    if (this->rchild) child_samples = std::max(child_samples, this->rchild->random_samples());
    HSSopts.set_d0(std::max(child_samples - HSSopts.dd(), HSSopts.d0()));
    if (opts.indirect_sampling())
      HSSopts.set_user_defined_random(true);
    _H.compress(mult, elem, HSSopts);
    params::compression_flops += params::flops - f0;
    if (this->lchild) this->lchild->release_work_memory();
    if (this->rchild) this->rchild->release_work_memory();
    if (this->dim_sep) {
      if (etree_level > 0) {
	auto f0 = params::flops;
	_ULV = _H.partial_factor();
	params::ULV_factor_flops += params::flops - f0;
	auto f1 = params::flops;
	_H.Schur_update(_ULV, _Theta, _DUB01, _Phi);
	DenseM_t& Vhat = _ULV.Vhat();
	if (_Theta.cols() < _Phi.cols()) {
	  _ThetaVhatC_or_VhatCPhiC = DenseM_t(Vhat.cols(), _Phi.rows());
	  gemm(Trans::C, Trans::C, scalar_t(1.), Vhat, _Phi, scalar_t(0.), _ThetaVhatC_or_VhatCPhiC, task_depth);
	} else {
	  _ThetaVhatC_or_VhatCPhiC = DenseM_t(_Theta.rows(), Vhat.rows());
	  gemm(Trans::N, Trans::C, scalar_t(1.), _Theta, Vhat, scalar_t(0.), _ThetaVhatC_or_VhatCPhiC, task_depth);
	}
	params::schur_flops += params::flops - f1;
      } else {
	auto f0 = params::flops;
	_ULV = _H.factor();
	params::ULV_factor_flops += params::flops - f0;
      }
    }
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::forward_multifrontal_solve
  (scalar_t* b, scalar_t* wmem, int etree_level, int task_depth) {
    if (task_depth == 0)
#pragma omp parallel if(!omp_in_parallel())
#pragma omp single
      forward_multifrontal_solve_node(b, wmem, etree_level, task_depth);
    else forward_multifrontal_solve_node(b, wmem, etree_level, task_depth);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::forward_multifrontal_solve_node
  (scalar_t* b, scalar_t* wmem, int etree_level, int task_depth) {
    bool tasked = task_depth<params::task_recursion_cutoff_level;
    if (tasked) {
      if (this->lchild)
#pragma omp task untied default(shared) final(task_depth >= params::task_recursion_cutoff_level-1) mergeable
	this->lchild->forward_multifrontal_solve(b, wmem, etree_level+1, task_depth+1);
      if (this->rchild)
#pragma omp task untied default(shared) final(task_depth >= params::task_recursion_cutoff_level-1) mergeable
	this->rchild->forward_multifrontal_solve(b, wmem, etree_level+1, task_depth+1);
#pragma omp taskwait
    } else {
      if (this->lchild) this->lchild->forward_multifrontal_solve(b, wmem, etree_level+1, task_depth);
      if (this->rchild) this->rchild->forward_multifrontal_solve(b, wmem, etree_level+1, task_depth);
    }
    this->look_left(b, wmem);
    if (etree_level) {
      if (_Theta.cols() && _Phi.cols()) {
	DenseMW_t rhs(this->dim_sep, 1, b+this->sep_begin, this->A->size());
	_ULVwork = std::unique_ptr<HSS::WorkSolve<scalar_t>>(new HSS::WorkSolve<scalar_t>());
	_H.child(0)->forward_solve(_ULV, *_ULVwork, rhs, true);
	if (this->dim_upd) {
	  DenseMW_t tmp(_Theta.rows(), _ULVwork->reduced_rhs.cols(), wmem+this->p_wmem, this->dim_upd);
	  gemm(Trans::N, Trans::N, scalar_t(-1.), _Theta, _ULVwork->reduced_rhs, scalar_t(1.), tmp, task_depth);
	}
	_ULVwork->reduced_rhs.clear();
      }
    } else {
      DenseMW_t rhs(this->dim_sep, 1, b+this->sep_begin, this->A->size());
      _ULVwork = std::unique_ptr<HSS::WorkSolve<scalar_t>>(new HSS::WorkSolve<scalar_t>());
      _H.forward_solve(_ULV, *_ULVwork, rhs, false);
    }
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::backward_multifrontal_solve
  (scalar_t* b, scalar_t* wmem, int etree_level, int task_depth) {
    if (task_depth == 0)
#pragma omp parallel if(!omp_in_parallel())
#pragma omp single
      backward_multifrontal_solve_node(b, wmem, etree_level, task_depth);
    else backward_multifrontal_solve_node(b, wmem, etree_level, task_depth);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::backward_multifrontal_solve_node
  (scalar_t* y, scalar_t* wmem, int etree_level, int task_depth) {
    bool tasked = task_depth<params::task_recursion_cutoff_level;
    if (etree_level) {
      if (_Phi.cols() && _Theta.cols()) {
	if (this->dim_upd) {
	  DenseMW_t tmp(_Phi.rows(), 1, wmem+this->p_wmem, _Phi.rows());
	  gemm(Trans::C, Trans::N, scalar_t(-1.), _Phi, tmp, scalar_t(1.), _ULVwork->x, task_depth);
	}
	DenseMW_t rhs(this->dim_sep, 1, y+this->sep_begin, this->A->size());
	_H.child(0)->backward_solve(_ULV, *_ULVwork, rhs);
	_ULVwork.reset();
      }
    } else {
      DenseMW_t rhs(this->dim_sep, 1, y+this->sep_begin, this->A->size());
      _H.backward_solve(_ULV, *_ULVwork, rhs);
    }
    this->look_right(y, wmem);
    if (tasked) {
      if (this->lchild)
#pragma omp task untied default(shared) final(task_depth >= params::task_recursion_cutoff_level-1) mergeable
	this->lchild->backward_multifrontal_solve(y, wmem, etree_level+1, task_depth+1);
      if (this->rchild)
#pragma omp task untied default(shared) final(task_depth >= params::task_recursion_cutoff_level-1) mergeable
	this->rchild->backward_multifrontal_solve(y, wmem, etree_level+1, task_depth+1);
#pragma omp taskwait
    } else {
      if (this->lchild) this->lchild->backward_multifrontal_solve(y, wmem, etree_level+1, task_depth);
      if (this->rchild) this->rchild->backward_multifrontal_solve(y, wmem, etree_level+1, task_depth);
    }
  }

  template<typename scalar_t,typename integer_t> integer_t
  FrontalMatrixHSS<scalar_t,integer_t>::maximum_rank(int task_depth) const {
    integer_t r = _H.rank(), rl = 0, rr = 0;
    if (this->lchild)
#pragma omp task untied default(shared) final(task_depth >= params::task_recursion_cutoff_level-1) mergeable
      rl = this->lchild->maximum_rank(task_depth+1);
    if (this->rchild)
#pragma omp task untied default(shared) final(task_depth >= params::task_recursion_cutoff_level-1) mergeable
      rr = this->rchild->maximum_rank(task_depth+1);
#pragma omp taskwait
    return std::max(r, std::max(rl, rr));
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::print_rank_statistics(std::ostream &out) const {
    if (this->lchild) this->lchild->print_rank_statistics(out);
    if (this->rchild) this->rchild->print_rank_statistics(out);
    out << "# HSSMatrix " << _H.rows() << "x" << _H.cols() << std::endl;
    _H.print_info(out);
  }

  template<typename scalar_t,typename integer_t> long long
  FrontalMatrixHSS<scalar_t,integer_t>::node_factor_nonzeros() const {
    return _H.nonzeros() + _ULV.nonzeros() + _Theta.nonzeros()
      + _Phi.nonzeros() + _ThetaVhatC_or_VhatCPhiC.nonzeros();
  }

  template<typename scalar_t,typename integer_t> long long
  FrontalMatrixHSS<scalar_t,integer_t>::dense_node_factor_nonzeros() const {
    return this->dim_blk*this->dim_blk-this->dim_upd*this->dim_upd;
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::set_HSS_partitioning
  (const SPOptions<scalar_t>& opts, const HSS::HSSPartitionTree& sep_tree, bool is_root) {
    assert(sep_tree.size == this->dim_sep);
    if (is_root)
      _H = HSS::HSSMatrix<scalar_t>(sep_tree, opts.HSS_options());
    else {
      HSS::HSSPartitionTree hss_tree(this->dim_blk);
      hss_tree.c.reserve(2);
      hss_tree.c.push_back(sep_tree);
      hss_tree.c.emplace_back(this->dim_upd);
      hss_tree.c.back().refine(opts.HSS_options().leaf_size());
      _H = HSS::HSSMatrix<scalar_t>(hss_tree, opts.HSS_options());
    }
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::bisection_partitioning
  (const SPOptions<scalar_t>& opts, integer_t* sorder, bool isroot, int task_depth) {
    if (this->lchild)
#pragma omp task default(shared) if(task_depth < params::task_recursion_cutoff_level)
      this->lchild->bisection_partitioning(opts, sorder, false, task_depth+1);
    if (this->rchild)
#pragma omp task default(shared) if(task_depth < params::task_recursion_cutoff_level)
      this->rchild->bisection_partitioning(opts, sorder, false, task_depth+1);
#pragma omp taskwait

    std::cout << "TODO FrontalMatrixHSS::bisection_partitioning" << std::endl;

    for (integer_t i=this->sep_begin; i<this->sep_end; i++) sorder[i] = -i;
    HSS::HSSPartitionTree sep_tree(this->dim_sep);
    sep_tree.refine(opts.HSS_options().leaf_size());

    // TODO this still needs to work when the sparse matrix is a
    // ProportionallyMappedSparseMatrix!!!
    // if (this->dim_sep >= 2 * opts.HSS_options().leaf_size()) {
    //   integer_t nrparts = 0;
    //   split_separator(opts, sep_tree, nrparts, 0, 1, sorder);
    //   auto count = this->sep_begin;
    //   for (integer_t part=0; part<nrparts; part++)
    //   	for (integer_t i=this->sep_begin; i<this->sep_end; i++)
    //  	  if (sorder[i] == part) sorder[i] = -count++;
    // } else for (integer_t i=this->sep_begin; i<this->sep_end; i++) sorder[i] = -i;

    if (isroot)
      _H = HSS::HSSMatrix<scalar_t>(sep_tree, opts.HSS_options());
    else {
      HSS::HSSPartitionTree hss_tree(this->dim_blk);
      hss_tree.c.reserve(2);
      hss_tree.c.push_back(sep_tree);
      hss_tree.c.emplace_back(this->dim_upd);
      hss_tree.c.back().refine(opts.HSS_options().leaf_size());
      _H = HSS::HSSMatrix<scalar_t>(hss_tree, opts.HSS_options());
    }
  }


  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::split_separator
  (const SPOptions<scalar_t>& opts, HSS::HSSPartitionTree& hss_tree, integer_t& nr_parts,
   integer_t part, integer_t count, integer_t* sorder) {
    std::vector<idx_t> xadj, adjncy;
    extract_separator(opts, part, xadj, adjncy, sorder);
    idx_t ncon = 1, edge_cut = 0, two = 2, nvtxs=xadj.size()-1;
    auto partitioning = new idx_t[nvtxs];
    int info = METIS_PartGraphRecursive(&nvtxs, &ncon, xadj.data(), adjncy.data(), NULL, NULL, NULL,
					&two, NULL, NULL, NULL, &edge_cut, partitioning);
    if (info != METIS_OK) {
      std::cerr << "METIS_PartGraphRecursive for separator reordering returned: " << info << std::endl;
      exit(1);
    }
    hss_tree.c.resize(2);
    for (integer_t i=this->sep_begin, j=0; i<this->sep_end; i++)
      if (sorder[i] == part) {
	auto p = partitioning[j++];
	sorder[i] = -count - p;
	hss_tree.c[p].size++;
      }
    delete[] partitioning;
    for (integer_t p=0; p<2; p++)
      if (hss_tree.c[p].size >= 2 * opts.HSS_options().leaf_size())
	split_separator(opts, hss_tree.c[p], nr_parts, -count-p, count+2, sorder);
      else std::replace(sorder+this->sep_begin, sorder+this->sep_end, -count-p, nr_parts++);
  }

  template<typename scalar_t,typename integer_t> void
  FrontalMatrixHSS<scalar_t,integer_t>::extract_separator
  (const SPOptions<scalar_t>& opts, integer_t part, std::vector<idx_t>& xadj,
   std::vector<idx_t>& adjncy, integer_t* sorder) {
    assert(opts.separator_ordering_level() == 0 || opts.separator_ordering_level() == 1);
    auto mark = new bool[this->dim_sep];
    auto ind_to_part = new integer_t[this->dim_sep];
    integer_t nvtxs = 0;
    for (integer_t r=0; r<this->dim_sep; r++)
      ind_to_part[r] = (sorder[r+this->sep_begin] == part) ? nvtxs++ : -1;
    xadj.reserve(nvtxs+1);
    adjncy.reserve(5*nvtxs);
    for (integer_t i=this->sep_begin, e=0; i<this->sep_end; i++) {
      if (sorder[i] == part) {
	xadj.push_back(e);
	std::fill(mark, mark+this->dim_sep, false);
	for (integer_t j=this->A->get_ptr()[i]; j<this->A->get_ptr()[i+1]; j++) {
	  auto c = this->A->get_ind()[j];
	  if (c == i) continue;
	  auto lc = c - this->sep_begin;
	  if (lc >= 0 && lc < this->dim_sep && sorder[c]==part && !mark[lc]) {
	    mark[lc] = true;
	    adjncy.push_back(ind_to_part[lc]);
	    e++;
	  } else {
	    if (opts.separator_ordering_level() > 0) {
	      for (integer_t k=this->A->get_ptr()[c]; k<this->A->get_ptr()[c+1]; k++) {
	   	auto cc = this->A->get_ind()[k];
	   	auto lcc = cc - this->sep_begin;
	   	if (cc!=i && lcc >= 0 && lcc < this->dim_sep && sorder[cc]==part && !mark[lcc]) {
	   	  mark[lcc] = true;
	   	  adjncy.push_back(ind_to_part[lcc]);
		  e++;
	   	}
	      }
	    }
	  }
	}
      }
    }
    xadj.push_back(adjncy.size());
    delete[] mark;
    delete[] ind_to_part;
  }

} // end namespace strumpack

#endif
