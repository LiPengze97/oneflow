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

#include <glog/logging.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <type_traits>
#include "oneflow/api/cpp/env.h"
#include "oneflow/core/common/just.h"
#include "oneflow/core/common/multi_client.h"
#include "oneflow/core/common/optional.h"
#include "oneflow/core/framework/shut_down_util.h"
#include "oneflow/core/job/cluster_instruction.h"
#include "oneflow/core/job/env.pb.h"
#include "oneflow/core/job/env_global_objects_scope.h"
#include "oneflow/core/control/ctrl_bootstrap.h"
#include "oneflow/core/rpc/include/base.h"
#include "oneflow/core/vm/vm_util.h"
#include "oneflow/core/thread/thread_consistent_id.h"

namespace oneflow_api {

namespace of = oneflow;

namespace {  // for inltialize

inline bool IsEnvInited() { return of::Global<of::EnvGlobalObjectsScope>::Get() != nullptr; }

bool HasEnvVar(const std::string& key) {
  const char* value = getenv(key.c_str());
  return value != nullptr;
}

std::string GetEnvVar(const std::string& key, const std::string& default_value) {
  const char* value = getenv(key.c_str());
  if (value == nullptr) { return default_value; }
  return std::string(value);
}

int64_t GetEnvVar(const std::string& key, int64_t default_value) {
  const char* value = getenv(key.c_str());
  if (value == nullptr) { return default_value; }
  return std::atoll(value);
}

int32_t FindFreePort(const std::string& addr) {
#ifdef __linux__
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  CHECK_GE(sock, 0) << "fail to find a free port.";
  int optval = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  std::mt19937 rng;
  rng.seed(std::random_device()());
  std::uniform_int_distribution<std::mt19937::result_type> dist(1, 1000);

  int count = 0;
  int num_attempts = 200;
  do {
    int port = 5000 + dist(rng);
    struct sockaddr_in sockaddr {};
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    sockaddr.sin_addr.s_addr = inet_addr(addr.c_str());
    int error = bind(sock, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
    if (error == 0) { return port; }
    ++count;
  } while (count < num_attempts);
  CHECK_NE(count, num_attempts) << "fail to find a free port.";
#endif  // __linux__
  return -1;
}

void CompleteEnvProto(of::EnvProto& env_proto) {
  auto bootstrap_conf = env_proto.mutable_ctrl_bootstrap_conf();
  auto master_addr = bootstrap_conf->mutable_master_addr();
  const std::string addr = GetEnvVar("MASTER_ADDR", "127.0.0.1");
  master_addr->set_host(addr);
  master_addr->set_port(GetEnvVar("MASTER_PORT", FindFreePort(addr)));
  bootstrap_conf->set_world_size(GetEnvVar("WORLD_SIZE", 1));
  bootstrap_conf->set_rank(GetEnvVar("RANK", 0));

  auto cpp_logging_conf = env_proto.mutable_cpp_logging_conf();
  if (HasEnvVar("GLOG_log_dir")) { cpp_logging_conf->set_log_dir(GetEnvVar("GLOG_log_dir", "")); }
  if (HasEnvVar("GLOG_logtostderr")) {
    cpp_logging_conf->set_logtostderr(GetEnvVar("GLOG_logtostderr", -1));
  }
  if (HasEnvVar("GLOG_logbuflevel")) {
    cpp_logging_conf->set_logbuflevel(GetEnvVar("GLOG_logbuflevel", -1));
  }
}

of::Maybe<void> initEnv() {
  of::EnvProto env_proto;
  CompleteEnvProto(env_proto);
  of::Global<of::EnvGlobalObjectsScope>::SetAllocated(new of::EnvGlobalObjectsScope());
  JUST(of::Global<of::EnvGlobalObjectsScope>::Get()->Init(env_proto));
  return of::Maybe<void>::Ok();
}

}  // namespace

void initialize() {
  of::SetIsMultiClient(true).GetOrThrow();
  if (!IsEnvInited()) { initEnv().GetOrThrow(); }
  of::SetShuttingDown(false);
}

void release() {
  if (IsEnvInited()) {
    // sync multi_client
    of::vm::ClusterSync().GetOrThrow();
    // destory env
    if (of::IsMultiClient().GetOrThrow()) {
      OF_ENV_BARRIER();
    } else {
      of::ClusterInstruction::MasterSendHalt();
    }
    of::Global<of::EnvGlobalObjectsScope>::Delete();
  }
  // TODO close session
  of::SetShuttingDown();
  of::ResetThisThreadUniqueConsistentId().GetOrThrow();
}

}  // namespace oneflow_api
