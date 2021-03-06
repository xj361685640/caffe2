/**
 * Copyright (c) 2016-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "caffe2/mkl/mkl_utils.h"
#include "caffe2/operators/expand_squeeze_dims_op.h"

#ifdef CAFFE2_HAS_MKL_DNN

namespace caffe2 {
namespace mkl {

template <typename T>
class MKLSqueezeOp final : public MKLOperator<T> {
 public:
  USE_MKLOPERATOR_FUNCTIONS(T);

  MKLSqueezeOp(const OperatorDef& operator_def, Workspace* ws)
      : MKLOperator<T>(operator_def, ws),
        dims_(OperatorBase::GetRepeatedArgument<int>("dims")) {
    auto originalSize = dims_.size();
    CAFFE_ENFORCE(originalSize > 0, "Parameter `dims` must be provided.");

    std::sort(dims_.begin(), dims_.end());
    dims_.erase(std::unique(dims_.begin(), dims_.end()), dims_.end());
    if (dims_.size() < originalSize) {
      LOG(WARNING) << "Parameter `dims` has repeated dimensions.";
    }
    CAFFE_ENFORCE(dims_.front() >= 0, "Dimension ids must be non-negative.");
  }

  bool RunOnDevice() override {
    const auto& X = Input(0);
    auto* Y = Output(0);

    CAFFE_ENFORCE_GT(
        X.ndim(),
        dims_.back(),
        "Input needs at least ",
        (dims_.back() + 1),
        " dimensions.");
    const auto& new_dims = SqueezeOp<MKLContext>::ComputeDims(X.dims(), dims_);

    bool dims_changed;
    CHECK_INPUT_DIMS(X, dims_changed);
    if (dims_changed) {
      // Temp buffer mainly to convert the input to plain layout before
      // Reshape() if the input has a custom layout.
      buffer_.Reset(X.dims());
    }

    // Always copy to temp buffer to avoid subsequent runs throwing layout
    // mismatch errors for X.
    buffer_.CopyFrom(X);
    Y->Reset(X.dims(), nullptr, dnnResourceNumber, true);
    CAFFE_ENFORCE(dnnLayoutCompare<T>(buffer_.layout(), Y->layout()));
    CAFFE_ENFORCE(Y->ShareFrom(buffer_));
    Y->Reshape(new_dims);
    return true;
  }

 private:
  vector<int> dims_;
  vector<TIndex> cached_input_dims_;
};

} // namespace mkl

REGISTER_MKL_OPERATOR(Squeeze, mkl::MKLSqueezeOp<float>);

} // namespace caffe2

#endif // CAFFE2_HAS_MKL_DNN
