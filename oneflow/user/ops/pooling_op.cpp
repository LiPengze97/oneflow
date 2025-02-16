/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "oneflow/core/framework/framework.h"
#include "oneflow/user/kernels/pooling_kernel_util.h"
#include "oneflow/user/kernels/avg_pooling_kernel_util.h"

namespace oneflow {

namespace {

typedef std::function<Maybe<void>(user_op::InferContext* ctx)> TensorDescInferFn;
typedef std::function<Maybe<void>(const user_op::UserOpWrapper& op, user_op::AddOpFn AddOp)>
    GenBackwardOpConfFn;

TensorDescInferFn MaxPoolMakeForwardTensorDescInferFn(const int32_t dim) {
  return [dim](user_op::InferContext* ctx) -> Maybe<void> {
    const Shape* x_shape = ctx->Shape4ArgNameAndIndex("x", 0);
    const std::string& data_format = ctx->Attr<std::string>("data_format");
    const std::vector<int32_t>& padding = ctx->Attr<std::vector<int32_t>>("padding");
    const std::vector<int32_t>& kernel_size = ctx->Attr<std::vector<int32_t>>("kernel_size");
    const std::vector<int32_t>& stride = ctx->Attr<std::vector<int32_t>>("stride");
    const std::vector<int32_t>& dilation = ctx->Attr<std::vector<int32_t>>("dilation");
    const bool return_indices = ctx->Attr<bool>("return_indices");
    const bool ceil_mode = ctx->Attr<bool>("ceil_mode");

    CHECK_EQ_OR_RETURN(kernel_size.size(), dim);
    for (int32_t pool_dim : kernel_size) { CHECK_GT_OR_RETURN(pool_dim, 0); }
    CHECK_EQ_OR_RETURN(stride.size(), dim);
    for (int32_t stride_dim : stride) { CHECK_GT_OR_RETURN(stride_dim, 0); }
    for (int32_t i = 0; i < padding.size(); i++) {
      CHECK_GE_OR_RETURN(kernel_size[i], 2 * padding[i])
          << "pad should be smaller than half of kernel size";
    }

    const MaxPoolingParams3D params_3d(dim, *x_shape, data_format, padding, kernel_size, stride,
                                       dilation, return_indices, ceil_mode);
    user_op::TensorDesc* y_desc = ctx->TensorDesc4ArgNameAndIndex("y", 0);
    *y_desc = *ctx->TensorDesc4ArgNameAndIndex("x", 0);
    *y_desc->mut_shape() = params_3d.GetYShape();

    user_op::TensorDesc* indice_desc = ctx->TensorDesc4ArgNameAndIndex("indice", 0);
    *indice_desc = *ctx->TensorDesc4ArgNameAndIndex("y", 0);
    *indice_desc->mut_shape() = *y_desc->mut_shape();
    DataType* dtype = indice_desc->mut_data_type();
    *dtype = kInt64;
    return Maybe<void>::Ok();
  };
}

TensorDescInferFn AvgPoolMakeForwardTensorDescInferFn(const int32_t dim) {
  return [dim](user_op::InferContext* ctx) -> Maybe<void> {
    const Shape* x_shape = ctx->Shape4ArgNameAndIndex("x", 0);
    const std::string& data_format = ctx->Attr<std::string>("data_format");
    const std::vector<int32_t>& padding = ctx->Attr<std::vector<int32_t>>("padding");
    const std::vector<int32_t>& kernel_size = ctx->Attr<std::vector<int32_t>>("kernel_size");
    const std::vector<int32_t>& stride = ctx->Attr<std::vector<int32_t>>("stride");
    const bool ceil_mode = ctx->Attr<bool>("ceil_mode");
    const bool count_include_pad = ctx->Attr<bool>("count_include_pad");
    const int64_t& divisor_override = ctx->Attr<int64_t>("divisor_override");

    CHECK_EQ_OR_RETURN(kernel_size.size(), dim);
    for (int32_t pool_dim : kernel_size) { CHECK_GT_OR_RETURN(pool_dim, 0); }
    CHECK_EQ_OR_RETURN(stride.size(), dim);
    for (int32_t stride_dim : stride) { CHECK_GT_OR_RETURN(stride_dim, 0); }
    for (int32_t i = 0; i < padding.size(); i++) {
      CHECK_GE_OR_RETURN(kernel_size[i], 2 * padding[i])
          << "pad should be smaller than half of kernel size";
    }

    const AvgPoolingParams3D params_3d(dim, *x_shape, data_format, padding, kernel_size, stride,
                                       ceil_mode, count_include_pad, divisor_override);
    user_op::TensorDesc* y_desc = ctx->TensorDesc4ArgNameAndIndex("y", 0);
    *y_desc = *ctx->TensorDesc4ArgNameAndIndex("x", 0);
    *y_desc->mut_shape() = params_3d.GetYShape();

    return Maybe<void>::Ok();
  };
}

Maybe<void> MaxPoolForwardGetSbpFn(user_op::SbpContext* ctx) {
  const user_op::TensorDesc& tensor = ctx->LogicalTensorDesc4InputArgNameAndIndex("x", 0);
  FOR_RANGE(int64_t, i, 0, std::min(2, (int)tensor.shape().NumAxes() - 2)) {
    ctx->NewBuilder()
        .Split(user_op::OpArg("x", 0), i)
        .Split(user_op::OpArg("y", 0), i)
        .Split(user_op::OpArg("indice", 0), i)
        .Build();
  }
  return Maybe<void>::Ok();
}

Maybe<void> AvgPoolForwardGetSbpFn(user_op::SbpContext* ctx) {
  const user_op::TensorDesc& tensor = ctx->LogicalTensorDesc4InputArgNameAndIndex("x", 0);
  FOR_RANGE(int64_t, i, 0, std::min(2, (int)tensor.shape().NumAxes() - 2)) {
    ctx->NewBuilder().Split(user_op::OpArg("x", 0), i).Split(user_op::OpArg("y", 0), i).Build();
  }
  return Maybe<void>::Ok();
}

Maybe<void> MaxPoolBackwardGetSbpFn(user_op::SbpContext* ctx) {
  const user_op::TensorDesc& tensor = ctx->LogicalTensorDesc4InputArgNameAndIndex("x", 0);
  const std::vector<int32_t>& padding = ctx->Attr<std::vector<int32_t>>("padding");
  FOR_RANGE(int64_t, i, 0, std::min(2, (int)tensor.shape().NumAxes())) {
    ctx->NewBuilder()
        .Split(user_op::OpArg("x", 0), i)
        .Split(user_op::OpArg("y", 0), i)
        .Split(user_op::OpArg("indice", 0), i)
        .Split(user_op::OpArg("dy", 0), i)
        .Split(user_op::OpArg("dx", 0), i)
        .Build();
  }
  return Maybe<void>::Ok();
}

Maybe<void> AvgPoolBackwardGetSbpFn(user_op::SbpContext* ctx) {
  FOR_RANGE(int64_t, i, 0, 2) {
    ctx->NewBuilder()
        .Split(user_op::OpArg("x", 0), i)
        .Split(user_op::OpArg("y", 0), i)
        .Split(user_op::OpArg("dy", 0), i)
        .Split(user_op::OpArg("dx", 0), i)
        .Build();
  }
  return Maybe<void>::Ok();
}

GenBackwardOpConfFn MaxPoolMakeBackwardOpConfFn(const std::string& mode, const int32_t dim) {
  return [mode, dim](const user_op::UserOpWrapper& op, user_op::AddOpFn AddOp) -> Maybe<void> {
    if (op.NeedGenGradTensor4OpInput("x", 0)) {
      user_op::UserOpConfWrapperBuilder builder(op.op_name() + "_grad");
      user_op::UserOpConfWrapper grad_op =
          builder.Op(mode + "pool_" + std::to_string(dim) + "d_grad")
              .Input("x", op.input("x", 0))
              .Input("y", op.output("y", 0))
              .Input("indice", op.output("indice", 0))
              .Input("dy", op.GetGradTensorWithOpOutput("y", 0))
              .Output("dx")
              .Attr("data_format", op.attr<std::string>("data_format"))
              .Attr("padding", op.attr<std::vector<int32_t>>("padding"))
              .Attr("kernel_size", op.attr<std::vector<int32_t>>("kernel_size"))
              .Attr("stride", op.attr<std::vector<int32_t>>("stride"))
              .Attr("dilation", op.attr<std::vector<int32_t>>("dilation"))
              .Attr("return_indices", op.attr<bool>("return_indices"))
              .Attr("ceil_mode", op.attr<bool>("ceil_mode"))
              .Build();
      op.BindGradTensorWithOpInput(grad_op.output("dx", 0), "x", 0);
      AddOp(grad_op);
    }
    return Maybe<void>::Ok();
  };
}

GenBackwardOpConfFn AvgPoolMakeBackwardOpConfFn(const int32_t dim) {
  return [dim](const user_op::UserOpWrapper& op, const user_op::AddOpFn& AddOp) -> Maybe<void> {
    if (op.NeedGenGradTensor4OpInput("x", 0)) {
      user_op::UserOpConfWrapperBuilder builder(op.op_name() + "_grad");
      user_op::UserOpConfWrapper grad_op =
          builder.Op("avgpool_" + std::to_string(dim) + "d_grad")
              .Input("x", op.input("x", 0))
              .Input("y", op.output("y", 0))
              .Input("dy", op.GetGradTensorWithOpOutput("y", 0))
              .Output("dx")
              .Attr("data_format", op.attr<std::string>("data_format"))
              .Attr("padding", op.attr<std::vector<int32_t>>("padding"))
              .Attr("kernel_size", op.attr<std::vector<int32_t>>("kernel_size"))
              .Attr("stride", op.attr<std::vector<int32_t>>("stride"))
              .Attr("ceil_mode", op.attr<bool>("ceil_mode"))
              .Attr("count_include_pad", op.attr<bool>("count_include_pad"))
              .Attr("divisor_override", op.attr<int64_t>("divisor_override"))
              .Build();
      op.BindGradTensorWithOpInput(grad_op.output("dx", 0), "x", 0);
      AddOp(grad_op);
    }
    return Maybe<void>::Ok();
  };
}

Maybe<void> BackwardTensorDescInferFn(user_op::InferContext* ctx) {
  *ctx->TensorDesc4ArgNameAndIndex("dx", 0) = *ctx->TensorDesc4ArgNameAndIndex("x", 0);
  return Maybe<void>::Ok();
}

Maybe<void> FwInferDataType(user_op::InferContext* ctx) {
  *ctx->OutputDType("y", 0) = ctx->InputDType("x", 0);
  return Maybe<void>::Ok();
}

Maybe<void> BwInferDataType(user_op::InferContext* ctx) {
  *ctx->OutputDType("dx", 0) = ctx->InputDType("x", 0);
  return Maybe<void>::Ok();
}
}  // namespace

#define REGISTER_MAXPOOL_FORWARD_OP(name, dim)                        \
  REGISTER_USER_OP(name)                                              \
      .Input("x")                                                     \
      .Output("y")                                                    \
      .Output("indice")                                               \
      .Attr<std::vector<int32_t>>("padding")                          \
      .Attr<std::string>("data_format")                               \
      .Attr<std::vector<int32_t>>("kernel_size")                      \
      .Attr<std::vector<int32_t>>("stride")                           \
      .Attr<std::vector<int32_t>>("dilation")                         \
      .Attr<bool>("return_indices")                                   \
      .Attr<bool>("ceil_mode")                                        \
      .SetTensorDescInferFn(MaxPoolMakeForwardTensorDescInferFn(dim)) \
      .SetGetSbpFn(MaxPoolForwardGetSbpFn)                            \
      .SetDataTypeInferFn(FwInferDataType);

REGISTER_MAXPOOL_FORWARD_OP("maxpool_1d", 1)
REGISTER_MAXPOOL_FORWARD_OP("maxpool_2d", 2)
REGISTER_MAXPOOL_FORWARD_OP("maxpool_3d", 3)

#undef REGISTER_MAXPOOL_FORWARD_OP

#define REGISTER_MAXPOOL_BACKWARD_OP(name)             \
  REGISTER_USER_OP(name)                               \
      .Input("x")                                      \
      .Input("y")                                      \
      .Input("indice")                                 \
      .Input("dy")                                     \
      .Output("dx")                                    \
      .Attr<std::vector<int32_t>>("padding")           \
      .Attr<std::string>("data_format")                \
      .Attr<std::vector<int32_t>>("kernel_size")       \
      .Attr<std::vector<int32_t>>("stride")            \
      .Attr<std::vector<int32_t>>("dilation")          \
      .Attr<bool>("return_indices")                    \
      .Attr<bool>("ceil_mode")                         \
      .SetTensorDescInferFn(BackwardTensorDescInferFn) \
      .SetGetSbpFn(MaxPoolBackwardGetSbpFn)            \
      .SetDataTypeInferFn(BwInferDataType);

REGISTER_MAXPOOL_BACKWARD_OP("maxpool_1d_grad")
REGISTER_MAXPOOL_BACKWARD_OP("maxpool_2d_grad")
REGISTER_MAXPOOL_BACKWARD_OP("maxpool_3d_grad")

#undef REGISTER_MAXPOOL_BACKWARD_OP

REGISTER_USER_OP_GRAD("maxpool_1d").SetGenBackwardOpConfFn(MaxPoolMakeBackwardOpConfFn("max", 1));
REGISTER_USER_OP_GRAD("maxpool_2d").SetGenBackwardOpConfFn(MaxPoolMakeBackwardOpConfFn("max", 2));
REGISTER_USER_OP_GRAD("maxpool_3d").SetGenBackwardOpConfFn(MaxPoolMakeBackwardOpConfFn("max", 3));

#define REGISTER_AVGPOOL_FORWARD_OP(name, ndim)                        \
  REGISTER_USER_OP(name)                                               \
      .Input("x")                                                      \
      .Output("y")                                                     \
      .Attr<std::vector<int32_t>>("padding")                           \
      .Attr<std::string>("data_format")                                \
      .Attr<std::vector<int32_t>>("kernel_size")                       \
      .Attr<std::vector<int32_t>>("stride")                            \
      .Attr<bool>("ceil_mode")                                         \
      .Attr<bool>("count_include_pad")                                 \
      .Attr<int64_t>("divisor_override")                               \
      .SetTensorDescInferFn(AvgPoolMakeForwardTensorDescInferFn(ndim)) \
      .SetGetSbpFn(AvgPoolForwardGetSbpFn)                             \
      .SetDataTypeInferFn(FwInferDataType);

REGISTER_AVGPOOL_FORWARD_OP("avgpool_1d", 1);
REGISTER_AVGPOOL_FORWARD_OP("avgpool_2d", 2);
REGISTER_AVGPOOL_FORWARD_OP("avgpool_3d", 3);

#undef REGISTER_AVGPOOL_FORWARD_OP

#define REGISTER_AVGPOOL_BACKWARD_OP(name)             \
  REGISTER_USER_OP(name)                               \
      .Input("x")                                      \
      .Input("y")                                      \
      .Input("dy")                                     \
      .Output("dx")                                    \
      .Attr<std::vector<int32_t>>("padding")           \
      .Attr<std::string>("data_format")                \
      .Attr<std::vector<int32_t>>("kernel_size")       \
      .Attr<std::vector<int32_t>>("stride")            \
      .Attr<bool>("ceil_mode")                         \
      .Attr<bool>("count_include_pad")                 \
      .Attr<int64_t>("divisor_override")               \
      .SetTensorDescInferFn(BackwardTensorDescInferFn) \
      .SetGetSbpFn(AvgPoolBackwardGetSbpFn)            \
      .SetDataTypeInferFn(BwInferDataType);

REGISTER_AVGPOOL_BACKWARD_OP("avgpool_1d_grad");
REGISTER_AVGPOOL_BACKWARD_OP("avgpool_2d_grad");
REGISTER_AVGPOOL_BACKWARD_OP("avgpool_3d_grad");

#undef REGISTER_AVGPOOL_BACKWARD_OP

REGISTER_USER_OP_GRAD("avgpool_1d").SetGenBackwardOpConfFn(AvgPoolMakeBackwardOpConfFn(1));
REGISTER_USER_OP_GRAD("avgpool_2d").SetGenBackwardOpConfFn(AvgPoolMakeBackwardOpConfFn(2));
REGISTER_USER_OP_GRAD("avgpool_3d").SetGenBackwardOpConfFn(AvgPoolMakeBackwardOpConfFn(3));

}  // namespace oneflow
