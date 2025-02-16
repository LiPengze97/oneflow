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
#ifndef ONEFLOW_IR_INCLUDE_ONEFLOW_PASSES_H_
#define ONEFLOW_IR_INCLUDE_ONEFLOW_PASSES_H_

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Tosa/IR/TosaOps.h"
#include "mlir/Dialect/SCF/SCF.h"
#include "mlir/Dialect/GPU/GPUDialect.h"
#include "mlir/Dialect/LLVMIR/NVVMDialect.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#include "OneFlow/Conversion/OneFlowToTosa.h"
#include "OneFlow/Conversion/SCFToGPU.h"
#include "OneFlow/Transform/BufferHostRegister.h"
#include "OneFlow/Transform/OutlineAndFuse.h"
#ifdef WITH_MLIR_CUDA_CODEGEN
#include "OneFlow/Conversion/PTXToCubin.h"
#endif  // WITH_MLIR_CUDA_CODEGEN

namespace mlir {

#define GEN_PASS_CLASSES
#define GEN_PASS_REGISTRATION
#include "OneFlow/OneFlowPasses.h.inc"

namespace oneflow {

LogicalResult LowerModuleToLLVM(mlir::MLIRContext* context, ModuleOp module);
#ifdef WITH_MLIR_CUDA_CODEGEN
LogicalResult LowerModuleToCUDALLVM(mlir::MLIRContext* context, ModuleOp module);
#endif  // WITH_MLIR_CUDA_CODEGEN
void populateFuserPasses(::mlir::RewritePatternSet& patterns);
void populateFuserForExistingOp(::mlir::RewritePatternSet& patterns);

}  // namespace oneflow

}  // namespace mlir

#endif  // ONEFLOW_IR_INCLUDE_ONEFLOW_PASSES_H_
