#include "base/kaldi-math.h"

#include "cudamatrix/cu-common.h"
#include "cudamatrix/cu-rand.h"
#include "cudamatrix/cu-randkernels.h"

namespace kaldi {

#if 0
// explicit template class intantiation : 
// templeted methods from this file will get compiled-in
template class CuRand<BaseFloat>;
//


template<typename Real> void CuRand<Real>::SeedGpu(MatrixIndexT state_size) {
  if(NULL != host_) delete[] host_; 
  host_ = new uint32[state_size]; 
  host_size_ = state_size;

  SeedBuffer(&z1_, state_size);
  SeedBuffer(&z2_, state_size);
  SeedBuffer(&z3_, state_size);
  SeedBuffer(&z4_, state_size);
  state_size_ = state_size;

  delete[] host_;
  host_size_ = 0;
}



template<typename Real> void CuRand<Real>::SeedBuffer(uint32* *tgt, MatrixIndexT state_size) {
  // generate random state
  for(MatrixIndexT i=0; i<host_size_; i++) {
    host_[i] = RandInt(128, RAND_MAX);
  }
  #if HAVE_CUDA==1
  // push it to the GPU
  if (CuDevice::Instantiate().Enabled()) {
    int32 state_size_in_bytes = state_size*sizeof(uint32);
    // resize the GPU buffer
    if (state_size_ != state_size) {
      cudaFree(*tgt);
      cudaMalloc((void**)tgt, state_size_in_bytes);
    }
    // copy the values
    cudaMemcpy(*tgt, host_, state_size_in_bytes, cudaMemcpyHostToDevice);
  } else
  #endif
  {// use back-off host buffer
    if (state_size_ != state_size) {
      delete[] (*tgt);
      *tgt = new uint32[state_size];
    }
    int32 state_size_in_bytes = state_size*sizeof(uint32);
    memcpy(*tgt, host_, state_size_in_bytes);
  }
}



template<> void CuRand<float>::RandUniform(CuMatrix<float> *tgt) {
  #if HAVE_CUDA==1 
  if (CuDevice::Instantiate().Enabled()) { 
    Timer tim;

    int32 tgt_size = tgt->NumRows()*tgt->Stride();
    if (tgt_size != state_size_) SeedGpu(tgt_size);

    dim3 dimBlock(CUBLOCK, CUBLOCK);
    dim3 dimGrid(n_blocks(tgt->NumCols(), CUBLOCK), n_blocks(tgt->NumRows(), CUBLOCK));

    cudaF_rand(dimGrid, dimBlock, tgt->Data(), z1_, z2_, z3_, z4_, tgt->Dim());
    cuSafeCall(cudaGetLastError());
  
    CuDevice::Instantiate().AccuProfile(__func__, tim.Elapsed());
  } else
  #endif
  {
    for(int32 r=0; r<tgt->NumRows(); r++) {
      for(int32 c=0; c<tgt->NumCols(); c++) {
        tgt->Mat()(r, c) = kaldi::RandUniform();
      }
    }
  }
}



template<> void CuRand<float>::RandGaussian(CuMatrix<float> *tgt) {
  #if HAVE_CUDA==1 
  if (CuDevice::Instantiate().Enabled()) { 
    Timer tim;

    int32 tgt_size = tgt->NumRows()*tgt->Stride();
    if (tgt_size != state_size_) SeedGpu(tgt_size);

    dim3 dimBlock(CUBLOCK, CUBLOCK);
    dim3 dimGrid(n_blocks(tgt->NumCols(), CUBLOCK), n_blocks(tgt->NumRows(), CUBLOCK));

    cudaF_gauss_rand(dimGrid, dimBlock, tgt->Data(), z1_, z2_, z3_, z4_, tgt->Dim());
    cuSafeCall(cudaGetLastError());
  
    CuDevice::Instantiate().AccuProfile(__func__, tim.Elapsed());
  } else
  #endif
  {
    for(int32 r=0; r<tgt->NumRows(); r++) {
      for(int32 c=0; c<tgt->NumCols(); c++) {
        tgt->Mat()(r, c) = RandGauss();
      }
    }
  }
}



template<> void CuRand<float>::BinarizeProbs(const CuMatrix<float> &probs, CuMatrix<float> *states) {
  #if HAVE_CUDA==1 
  if (CuDevice::Instantiate().Enabled()) { 
    Timer tim;

    // possible no-op's...
    int32 tgt_size = probs.NumRows()*probs.Stride();
    if (tgt_size != state_size_) SeedGpu(tgt_size);

    states->Resize(probs.NumRows(), probs.NumCols());
    tmp.Resize(probs.NumRows(), probs.NumCols());

    RandUniform(&tmp);

    dim3 dimBlock(CUBLOCK, CUBLOCK);
    dim3 dimGrid(n_blocks(states->NumCols(), CUBLOCK), n_blocks(states->NumRows(), CUBLOCK));

    cudaF_binarize_probs(dimGrid, dimBlock, states->Data(), probs.Data(), tmp.Data(), states->Dim());
    cuSafeCall(cudaGetLastError());
  
    CuDevice::Instantiate().AccuProfile(__func__, tim.Elapsed());
  } else
  #endif
  {
    for(int32 r=0; r<states->NumRows(); r++) {
      for(int32 c=0; c<states->NumCols(); c++) {
        states->Mat()(r, c) = ((kaldi::RandUniform() < probs.Mat()(r, c))? 1 : 0 );
      }
    }
  }
}



template<> void CuRand<float>::AddGaussNoise(CuMatrix<float> *tgt, float gscale) {
  tmp.Resize(tgt->NumRows(), tgt->NumCols());
  RandGaussian(&tmp);
  tgt->AddMat(gscale, tmp, 1.0);
}

#endif

} // namespace
