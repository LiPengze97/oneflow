#ifndef ONEFLOW_CORE_OPERATOR_RESIZE_RESHAPE_OP_H_
#define ONEFLOW_CORE_OPERATOR_RESIZE_RESHAPE_OP_H_

#include "oneflow/core/operator/operator.h"

namespace oneflow {

class ResizeReshapeOp final : public Operator {
 public:
  OF_DISALLOW_COPY_AND_MOVE(ResizeReshapeOp);
  ResizeReshapeOp() = default;
  ~ResizeReshapeOp() override = default;

  void InitFromOpConf() override;
  const PbMessage &GetCustomizedConf() const override;
  bool NeedInBlobWhenBackward() const override { return false; }
  bool NeedOutBlobWhenBackward() const override { return false; }

  void InferBlobDescs(std::function<BlobDesc *(const std::string &)> GetBlobDesc4BnInOp,
                      const ParallelContext *parallel_ctx) const override;

 private:
  bool IsInputBlobAllowedModelSplit(const std::string &ibn) const override;
  void InferOutputBlobModelSplitAxis(
      std::function<int32_t *(const std::string &)> ModelSplitAxis4BnInOp,
      std::function<int32_t(const std::string &)> ShapeNumAxes4BnInOp,
      const ParallelContext *parallel_context) const override;
};

}  // namespace oneflow

#endif  // ONEFLOW_CORE_OPERATOR_RESIZE_RESHAPE_OP_H_
