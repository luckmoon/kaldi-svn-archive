// nnet/nnet-component.h

// Copyright 2011-2012  Karel Vesely
//                      Johns Hopkins University (author: Daniel Povey)

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.



#ifndef KALDI_NNET_COMPONENT_H_
#define KALDI_NNET_COMPONENT_H_


#include "base/kaldi-common.h"
#include "matrix/matrix-lib.h"
// #include "nnet/nnet-nnet.h"

#include <iostream>

namespace kaldi {


/**
 * Abstract class, basic element of the network,
 * it is a box with defined inputs, outputs,
 * and tranformation functions interface.
 *
 * It is able to propagate and backpropagate
 * exact implementation is to be implemented in descendants.
 *
 */ 

class Component {
 public:
  Component() { }
  
  virtual std::string Type() const = 0; // each type should return a string such as
  // "SigmoidComponent".

  /// Initialize, typically from a line of a config file.  The "args" will
  /// contain any parameters that need to be passed to the Component, e.g.
  /// dimensions.
  virtual void InitFromString(std::string args) = 0; 
  
  virtual bool IsUpdatable() { return false; } // lets us know if the component is updatable. (has trainable
  // parameters).
  
  /// Get size of input vectors
  virtual int32 InputDim() const = 0;
  
  /// Get size of output vectors 
  virtual int32 OutputDim() const = 0;
  
  /// Perform forward pass propagation Input->Output.  Each row is
  /// one frame or training example.
  virtual void Propagate(const MatrixBase<BaseFloat> &in,
                         MatrixBase<BaseFloat> *out) const = 0; 
  
  /// Perform backward pass propagation of the derivative.
  /// Used for nonlinear layers (IsLinear() === true) where we 
  /// need the value at the input in order to compute the derivative.
  /// Crashes unless overridden.  Note: so far it has been more convenient
  /// to use the output of the nonlinearities in computing the derivative,
  /// but this isn't always possible in general.  We'll cross that bridge
  /// when we come to it.
  virtual void Backprop(const MatrixBase<BaseFloat> &in_value,
                        const MatrixBase<BaseFloat> &out_value,
                        const MatrixBase<BaseFloat> &out_deriv,
                        Component *to_update, // may be identical to "this".
                        MatrixBase<BaseFloat> *in_deriv) const = 0;
  
  virtual bool BackpropNeedsInput() const { return true; } // if this returns false,
  // the "in_value" to Backprop may be a dummy variable.
  virtual bool BackpropNeedsOutput() const { return true; } // if this returns false,
  // the "out_value" to Backprop may be a dummy variable.
  
  /// Read component from stream
  static Component* ReadNew(std::istream &is, bool binary);

  /// Copy component (deep copy).
  virtual Component* Copy() const = 0;

  /// Initialize the Component from one line that will contain
  /// first the type, e.g. SigmoidComponent, and then
  /// a number of tokens (typically integers or floats) that will
  /// be used to initialize the component.
  static Component *NewFromString(const std::string &initializer_line);
  
  virtual void Read(std::istream &is, bool binary) = 0; // This Read function
  // requires that the Component has the correct type.
  
  /// Write component to stream
  virtual void Write(std::ostream &os, bool binary) const = 0;

  virtual ~Component() { }

 private:
  KALDI_DISALLOW_COPY_AND_ASSIGN(Component);
};


/**
 * Class UpdatableComponent is a Component which has
 * trainable parameters and contains some global 
 * parameters for stochastic gradient descent
 * (learning rate, L2 regularization constant).
 * This is a base-class for Components with parameters.
 */
class UpdatableComponent : public Component {
 public:
  void Init(BaseFloat learning_rate, BaseFloat l2_penalty) {
    learning_rate_ = learning_rate;
    l2_penalty_ = l2_penalty_;
  }
  UpdatableComponent(BaseFloat learning_rate, BaseFloat l2_penalty) {
    Init(learning_rate, l2_penalty);
  }

  /// Set parameters to zero,
  /// and if treat_as_gradient is true, we'll be treating this as a gradient
  /// so set the learning rate to 1 and l2_penalty to zero and make any other
  /// changes necessary (there's a variable we have to set for the
  /// MixtureProbComponent).
  virtual void SetZero(bool treat_as_gradient) = 0;

  UpdatableComponent(): learning_rate_(0.0), l2_penalty_(0.0) { }
  
  virtual ~UpdatableComponent() { }

  /// Check if contains trainable parameters 
  bool IsUpdatable() const { return true; }

  /// Here, "other" is a component of the same specific type.  This
  /// function computes the dot product in parameters, and is computed while
  /// automatically adjusting learning rates; typically, one of the two will
  /// actually contain the gradient.
  virtual BaseFloat DotProduct(const UpdatableComponent &other) const = 0;

  /// We introduce a new virtual function that only applies to
  /// class UpdatableComponent.  This is used in testing.
  virtual void PerturbParams(BaseFloat stddev) = 0;
  
  /// Sets the learning rate of gradient descent
  void SetLearningRate(BaseFloat lrate) {  learning_rate_ = lrate; }
  /// Gets the learning rate of gradient descent
  BaseFloat LearningRate() { return learning_rate_; }
  /// Sets L2 penalty (weight decay)
  void SetL2Penalty(BaseFloat l2) { l2_penalty_ = l2;  }
  /// Gets L2 penalty (weight decay)
  BaseFloat L2Penalty() { return l2_penalty_; }
 protected:
  BaseFloat learning_rate_; ///< learning rate (0.0..0.01)
  BaseFloat l2_penalty_; ///< L2 regularization constant (0.0..1e-4)
  BaseFloat OldWeight(int32 num_frames) const;
};

/// This kind of Component is a base-class for things like
/// sigmoid and softmax.
class NonlinearComponent: public Component {
 public:
  virtual void Init(int32 dim) { dim_ = dim; }
  NonlinearComponent(int32 dim): dim_(dim) { }
  NonlinearComponent(): dim_(0) { } // e.g. prior to Read().
  
  virtual int32 InputDim() const { return dim_; }
  virtual int32 OutputDim() const { return dim_; }
  
  /// We implement InitFromString at this level.
  virtual void InitFromString(std::string args);
  
  /// We implement Read at this level as it just needs the Type().
  virtual void Read(std::istream &is, bool binary);
  
  /// Write component to stream.
  virtual void Write(std::ostream &os, bool binary) const;
  
 protected:
  int32 dim_;
};

class SigmoidComponent: public NonlinearComponent {
 public:
  SigmoidComponent(int32 dim): NonlinearComponent(dim) { }
  SigmoidComponent() { }
  virtual std::string Type() const { return "SigmoidComponent"; }
  virtual bool BackpropNeedsInput() { return false; }
  virtual bool BackpropNeedsOutput() { return true; }
  virtual Component* Copy() const { return new SigmoidComponent(dim_); }
  virtual void Propagate(const MatrixBase<BaseFloat> &in,
                         MatrixBase<BaseFloat> *out) const; 
  virtual void Backprop(const MatrixBase<BaseFloat> &in_value,
                        const MatrixBase<BaseFloat> &out_value,
                        const MatrixBase<BaseFloat> &out_deriv,
                        Component *to_update, // may be identical to "this".
                        MatrixBase<BaseFloat> *in_deriv) const;
 private:
  KALDI_DISALLOW_COPY_AND_ASSIGN(SigmoidComponent);
};

class TanhComponent: public NonlinearComponent {
 public:
  TanhComponent(int32 dim): NonlinearComponent(dim) { }
  TanhComponent() { }
  virtual std::string Type() const { return "TanhComponent"; }
  virtual Component* Copy() const { return new TanhComponent(dim_); }
  virtual bool BackpropNeedsInput() { return false; }
  virtual bool BackpropNeedsOutput() { return true; }
  virtual void Propagate(const MatrixBase<BaseFloat> &in,
                         MatrixBase<BaseFloat> *out) const; 
  virtual void Backprop(const MatrixBase<BaseFloat> &in_value,
                        const MatrixBase<BaseFloat> &out_value,
                        const MatrixBase<BaseFloat> &out_deriv,
                        Component *to_update, // may be identical to "this".
                        MatrixBase<BaseFloat> *in_deriv) const;
 private:
  KALDI_DISALLOW_COPY_AND_ASSIGN(TanhComponent);
};

class SoftmaxComponent: public NonlinearComponent {
 public:
  SoftmaxComponent(int32 dim): NonlinearComponent(dim) { }
  SoftmaxComponent() { }
  virtual std::string Type() const { return "softmaxComponent"; }  // Make it lower case
  // because each type of Component needs a different first letter.
  virtual Component* Copy() const { return new SoftmaxComponent(dim_); }
  virtual bool BackpropNeedsInput() const { return false; }
  virtual bool BackpropNeedsOutput() const { return true; }
  virtual void Propagate(const MatrixBase<BaseFloat> &in,
                         MatrixBase<BaseFloat> *out) const; 
  virtual void Backprop(const MatrixBase<BaseFloat> &in_value,
                        const MatrixBase<BaseFloat> &out_value,
                        const MatrixBase<BaseFloat> &out_deriv,
                        Component *to_update, // may be identical to "this".
                        MatrixBase<BaseFloat> *in_deriv) const;
 private:
  KALDI_DISALLOW_COPY_AND_ASSIGN(SoftmaxComponent);
};

// Affine means a linear function plus an offset.
class AffineComponent: public UpdatableComponent {
 public:
  virtual int32 InputDim() const { return linear_params_.NumCols(); }
  virtual int32 OutputDim() const { return linear_params_.NumRows(); }
  virtual void Init(BaseFloat learning_rate, BaseFloat l2_penalty,
                    int32 input_dim, int32 output_dim,
                    BaseFloat param_stddev);

  virtual void InitFromString(std::string args);
  
  AffineComponent() { } // use Init to really initialize.
  virtual std::string Type() const { return "AffineComponent"; }
  virtual bool BackpropNeedsInput() const { return true; }
  virtual bool BackpropNeedsOutput() const { return false; }
  virtual void Propagate(const MatrixBase<BaseFloat> &in,
                         MatrixBase<BaseFloat> *out) const; 
  virtual void Backprop(const MatrixBase<BaseFloat> &in_value,
                        const MatrixBase<BaseFloat> &out_value, // dummy
                        const MatrixBase<BaseFloat> &out_deriv,
                        Component *to_update, // may be identical to "this".
                        MatrixBase<BaseFloat> *in_deriv) const;
  virtual void SetZero(bool treat_as_gradient);
  virtual void Read(std::istream &is, bool binary);
  virtual void Write(std::ostream &os, bool binary) const;
  virtual BaseFloat DotProduct(const UpdatableComponent &other) const;
  virtual Component* Copy() const;
  virtual void PerturbParams(BaseFloat stddev);
 private:
  KALDI_DISALLOW_COPY_AND_ASSIGN(AffineComponent);
  Matrix<BaseFloat> linear_params_;
  Vector<BaseFloat> bias_params_;
};


// Affine means a linear function plus an offset.  "Block" means
// here that we support a number of equal-sized blocks of parameters,
// in the linear part, so e.g. 2 x 500 would mean 2 blocks of 500 each.
class BlockAffineComponent: public UpdatableComponent {
 public:
  virtual int32 InputDim() const { return linear_params_.NumCols() * num_blocks_; }
  virtual int32 OutputDim() const { return linear_params_.NumRows(); }
  // Note: num_blocks must divide input_dim.
  virtual void Init(BaseFloat learning_rate, BaseFloat l2_penalty,
                    int32 input_dim, int32 output_dim,
                    BaseFloat param_stddev, int32 num_blocks);

  virtual void InitFromString(std::string args);
  
  BlockAffineComponent() { } // use Init to really initialize.
  virtual std::string Type() const { return "BlockAffineComponent"; }
  virtual bool BackpropNeedsInput() const { return true; }
  virtual bool BackpropNeedsOutput() const { return false; }
  virtual void Propagate(const MatrixBase<BaseFloat> &in,
                         MatrixBase<BaseFloat> *out) const; 
  virtual void Backprop(const MatrixBase<BaseFloat> &in_value,
                        const MatrixBase<BaseFloat> &out_value,
                        const MatrixBase<BaseFloat> &out_deriv,
                        Component *to_update, // may be identical to "this".
                        MatrixBase<BaseFloat> *in_deriv) const;
  virtual void SetZero(bool treat_as_gradient);
  virtual void Read(std::istream &is, bool binary);
  virtual void Write(std::ostream &os, bool binary) const;
  virtual BaseFloat DotProduct(const UpdatableComponent &other) const;
  virtual Component* Copy() const;
  virtual void PerturbParams(BaseFloat stddev);
 private:
  KALDI_DISALLOW_COPY_AND_ASSIGN(BlockAffineComponent);
  // The matrix linear_parms_ has a block structure, with num_blocks_ blocks fo
  // equal size.  The blocks are stored in linear_params_ as
  // [ M
  //   N
  //   O ] but we actually treat it as the matrix:
  // [ M 0 0
  //   0 N 0
  //   0 0 O ]
  Matrix<BaseFloat> linear_params_;
  Vector<BaseFloat> bias_params_;
  int32 num_blocks_;
};


// MixtureProbComponent is a linear transform, but it's kind of a special case.
// It's used to transform probabilities while retaining the sum-to-one
// constraint (after the softmax), so we require nonnegative
// elements that sum to one for each column.  In addition, this component
// implements a linear transformation that's a block matrix... not quite
// block diagonal, because the component matrices aren't necessarily square.
// They start off square, but as we mix up, they may get non-square.
class MixtureProbComponent: public UpdatableComponent {
 public:
  virtual int32 InputDim() const { return input_dim_; }
  virtual int32 OutputDim() const { return output_dim_; }
  virtual void Init(BaseFloat learning_rate, BaseFloat l2_penalty,
                    BaseFloat diag_element,
                    const std::vector<int32> &sizes);
  virtual void InitFromString(std::string args);  
  MixtureProbComponent() { }
  virtual void SetZero(bool treat_as_gradient);
  virtual std::string Type() const { return "MixtureComponent"; }
  virtual bool BackpropNeedsInput() const { return true; }
  virtual bool BackpropNeedsOutput() const { return false; }
  virtual void Propagate(const MatrixBase<BaseFloat> &in,
                         MatrixBase<BaseFloat> *out) const;
  // Note: in_value and out_value are both dummy variables.
  virtual void Backprop(const MatrixBase<BaseFloat> &in_value,
                        const MatrixBase<BaseFloat> &out_value,
                        const MatrixBase<BaseFloat> &out_deriv,
                        Component *to_update, // may be identical to "this".
                        MatrixBase<BaseFloat> *in_deriv) const;
  virtual Component* Copy() const;
  
  virtual void Read(std::istream &is, bool binary);
  virtual void Write(std::ostream &os, bool binary) const;
  virtual BaseFloat DotProduct(const UpdatableComponent &other) const;
  virtual void PerturbParams(BaseFloat stddev);
 private:
  KALDI_DISALLOW_COPY_AND_ASSIGN(MixtureProbComponent);
  std::vector<Matrix<BaseFloat> > params_;
  int32 input_dim_;
  int32 output_dim_;
  bool is_gradient_; // true if we're treating this as just a store for the gradient.
};

// PermuteComponent does a random permutation of the dimensions.
// Useful in conjunction with block-diagonal transforms.
class PermuteComponent: public Component {
 public:
  virtual void Init(int32 dim);
  PermuteComponent(int32 dim) { Init(dim); }
  PermuteComponent() { } // e.g. prior to Read() or Init()
  
  virtual int32 InputDim() const { return reorder_.size(); }
  virtual int32 OutputDim() const { return reorder_.size(); }
  virtual Component *Copy() const;

  virtual void InitFromString(std::string args);
  virtual void Read(std::istream &is, bool binary);
  virtual void Write(std::ostream &os, bool binary) const;
  virtual std::string Type() const { return "MixtureComponent"; }
  virtual bool BackpropNeedsInput() const { return false; }
  virtual bool BackpropNeedsOutput() const { return false; }
  virtual void Propagate(const MatrixBase<BaseFloat> &in,
                         MatrixBase<BaseFloat> *out) const; 
  virtual void Backprop(const MatrixBase<BaseFloat> &in_value, // dummy
                        const MatrixBase<BaseFloat> &out_value, // dummy
                        const MatrixBase<BaseFloat> &out_deriv, 
                        Component *to_update, // dummy
                        MatrixBase<BaseFloat> *in_deriv) const;
  
 private:
  KALDI_DISALLOW_COPY_AND_ASSIGN(PermuteComponent);
  std::vector<int32> reorder_; // This class sends input dimension i to
  // output dimension reorder_[i].
};


} // namespace kaldi


#endif
