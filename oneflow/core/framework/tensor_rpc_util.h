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
#ifndef ONEFLOW_CORE_FRAMEWORK_TENSOR_RPC_UTIL_H_
#define ONEFLOW_CORE_FRAMEWORK_TENSOR_RPC_UTIL_H_

#include "oneflow/core/framework/transport_util.h"
#include "oneflow/core/framework/tensor.h"
#include "oneflow/core/common/optional.h"
#include "oneflow/core/common/decorator.h"
#include "oneflow/core/rpc/include/global_process_ctx.h"

namespace oneflow {

namespace private_details {

class CheckConsistencyAsyncTransportCtx;

int64_t* MutThreadLocalTensorMetaCheckDepth();

Maybe<CheckConsistencyAsyncTransportCtx> LaunchTensorMetaConsistencyCheck(
    const one::Tensor& tensor);

Maybe<void> BuzyWaitAndCheck(std::shared_ptr<CheckConsistencyAsyncTransportCtx>& ctx);

Maybe<void> RunCallback(const std::shared_ptr<one::Tensor>& tensor,
                        const std::function<Maybe<void>()>& Callback);

}  // namespace private_details

inline bool IsConsistentTensorMetaCheckDisabled() {
  return *private_details::MutThreadLocalTensorMetaCheckDepth() > 1;
}

template<typename... Args>
struct CheckConsistentTensorMeta;

template<typename RetT, typename... Args>
struct CheckConsistentTensorMeta<RetT, const std::shared_ptr<one::Tensor>&, Args...> {
  static_assert(is_maybe<RetT>::value, "returned value type must be Maybe<T>.");
  template<RetT (*func)(const std::shared_ptr<one::Tensor>&, Args...)>
  static RetT Call(const std::shared_ptr<one::Tensor>& tensor, Args... args) {
    std::shared_ptr<private_details::CheckConsistencyAsyncTransportCtx> ctx;
    int64_t* depth = private_details::MutThreadLocalTensorMetaCheckDepth();
    if (*depth == 0) { ctx = JUST(private_details::LaunchTensorMetaConsistencyCheck(*tensor)); }
    ++*depth;
    RetT ret = func(tensor, args...);
    --*depth;
    // Always synchronize consistent tensor meta even if `func` failed.
    if (*depth == 0) { JUST(private_details::BuzyWaitAndCheck(ctx)); }
    return ret;
  }
};

struct DisableCheckConsistentTensorMetaScope final {
  DisableCheckConsistentTensorMetaScope() {
    ++*private_details::MutThreadLocalTensorMetaCheckDepth();
  }
  ~DisableCheckConsistentTensorMetaScope() {
    --*private_details::MutThreadLocalTensorMetaCheckDepth();
  }
};

static constexpr auto* WithConsistencyChecked =
    DECORATE(&private_details::RunCallback, CheckConsistentTensorMeta);

}  // namespace oneflow

#endif  // ONEFLOW_CORE_FRAMEWORK_TENSOR_RPC_UTIL_H_
