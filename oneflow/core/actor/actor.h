#ifndef ONEFLOW_CORE_ACTOR_ACTOR_H_
#define ONEFLOW_CORE_ACTOR_ACTOR_H_

#include "oneflow/core/actor/actor_message_bus.h"
#include "oneflow/core/device/cpu_device_context.h"
#include "oneflow/core/device/cuda_device_context.h"
#include "oneflow/core/device/cuda_stream_handle.h"
#include "oneflow/core/job/task.pb.h"
#include "oneflow/core/kernel/kernel_context.h"
#include "oneflow/core/kernel/kernel_manager.h"
#include "oneflow/core/persistence/snapshot_manager.h"
#include "oneflow/core/register/local_register_wrapper.h"
#include "oneflow/core/register/register_manager.h"
#include "oneflow/core/register/remote_register_wrapper.h"
#include "oneflow/core/thread/thread_context.h"

namespace oneflow {

class Actor {
 public:
  OF_DISALLOW_COPY_AND_MOVE(Actor);
  virtual ~Actor() = default;

  virtual void Init(const TaskProto&, const ThreadCtx&) = 0;
  // 1: success, and actor finish
  // 0: success, and actor not finish
  int ProcessMsg(const ActorMsg& msg) { return (this->*msg_handler_)(msg); }

  int64_t actor_id() const { return actor_id_; }
  int64_t GetMachineId() const {
    return IDMgr::Singleton()->MachineId4ActorId(actor_id_);
  }
  int64_t GetThrdLocId() const {
    return IDMgr::Singleton()->ThrdLocId4ActorId(actor_id_);
  }

 protected:
  struct ExecKernel {
    const Kernel* kernel;
    HashMap<std::string, int64_t> bn_in_op2regst_desc_id;
  };

  Actor() = default;
  int64_t RegstDescId4Name(const std::string& name) const;

  std::unique_ptr<DeviceCtx>& mut_device_ctx() { return device_ctx_; }
  KernelCtx GenDefaultKernelCtx() const;

  void set_num_of_not_eord(int val) { num_of_not_eord_ = val; }
  int& mut_num_of_read_empty() { return num_of_read_empty_; }

  // Msg Handler
  using MsgHandler = int (Actor::*)(const ActorMsg&);
  MsgHandler msg_handler() { return msg_handler_; }
  void set_msg_handler(MsgHandler val) { msg_handler_ = val; }
#define OF_SET_MSG_HANDLER(val)                                   \
  do {                                                            \
    LOG(INFO) << "actor " << actor_id() << " switch to " << #val; \
    set_msg_handler(static_cast<MsgHandler>(val));                \
  } while (0)

  // Common Handlers
  virtual int HandlerNormal(const ActorMsg& msg) = 0;
  virtual int HandlerWaitUntilNoReadableRegst(const ActorMsg& msg) = 0;
  int HandlerWaitUntilReadingCntEqualZero(const ActorMsg& msg);

  // Act
  void ActUntilFail();
  virtual void Act() = 0;
  virtual bool IsReadReady() = 0;
  virtual void ProcessEord();
  // Async Do on KernelCtx
  void AsyncLaunchKernel(
      const KernelCtx&,
      std::function<std::shared_ptr<RegstWrapper>(int64_t)> Regst4RegstDescId);
  void AsyncSendReadableRegstMsg(std::function<void(Regst*)> RegstPreProcess,
                                 std::function<bool(int64_t)> IsAllowedActor);
  void AsyncSendReadableRegstMsg(std::function<void(Regst*)> RegstPreProcess);
  void AsyncSendReadableRegstMsg(std::function<bool(int64_t)> IsAllowedActor);
  void AsyncSendReadableRegstMsg();
  void AsyncSendEORDMsgToSubscribers(int64_t regst_desc_id);
  void AsyncSendEORDMsgForAllProducedRegstDesc();
  void AsyncSendRegstMsgToProducer(const std::shared_ptr<RegstWrapper>&);
  void AsyncDo(std::function<void()>);

  // Status of Produced Registers
  int TryUpdtStateAsProducedRegst(Regst* regst);
  Regst* GetCurWriteableRegst(int64_t regst_desc_id);
  Regst* GetCurWriteableRegst(const std::string& name);
  void ForEachCurWriteableRegst(std::function<void(Regst*)> func);
  void SetReadOnlyForRegstDescId(int64_t regst_desc_id);
  int64_t total_reading_cnt() const { return total_reading_cnt_; }
  int64_t expected_piece_id() const { return expected_piece_id_; }

 private:
  bool IsWriteReady();

  int64_t actor_id_;
  KernelLaunchFunc launch_func_;
  std::vector<ExecKernel> exec_kernel_vec_;
  HashMap<int64_t, std::vector<std::unique_ptr<Regst>>>
      produced_regsts_;  // <regst_desc_id, regst>
  HashMap<std::string, int64_t> name2regst_desc_id_;

  std::unique_ptr<DeviceCtx> device_ctx_;

  MsgHandler msg_handler_;

  // Status of Produced Registers
  int64_t expected_piece_id_;
  HashMap<int64_t, std::queue<Regst*>>
      writeable_produced_regst_;  // <regst_desc_id, regst>
  int64_t writeable_produced_regst_desc_num_;
  HashMap<Regst*, int64_t> produced_regst2reading_cnt_;
  int64_t total_reading_cnt_;
  int num_of_not_eord_;
  int num_of_read_empty_;
};

}  // namespace oneflow

#endif  // ONEFLOW_CORE_ACTOR_ACTOR_H_
