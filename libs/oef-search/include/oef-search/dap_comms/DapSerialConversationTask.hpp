#pragma once

#include <memory>
#include <queue>
#include <utility>

#include "core/logging.hpp"
#include "oef-base/threading/StateMachineTask.hpp"

#include "mt-search/comms/src/cpp/OefSearchEndpoint.hpp"
#include "mt-search/dap_comms/src/cpp/DapConversationTask.hpp"
#include "oef-base/conversation/OutboundConversations.hpp"

template <typename IN_PROTO, typename OUT_PROTO, typename MIDDLE_PROTO>
class DapSerialConversationTask
  : public StateMachineTask<DapSerialConversationTask<IN_PROTO, OUT_PROTO, MIDDLE_PROTO>>,
    std::enable_shared_from_this<DapSerialConversationTask<IN_PROTO, OUT_PROTO, MIDDLE_PROTO>>
{
public:
  struct PipeDataType
  {
    std::string                   dap_name;
    std::string                   path;
    std::shared_ptr<MIDDLE_PROTO> proto;
  };

  using StateResult      = typename StateMachineTask<DapSerialConversationTask>::Result;
  using EntryPoint       = typename StateMachineTask<DapSerialConversationTask>::EntryPoint;
  using ProtoPipeBuilder = std::function<std::shared_ptr<IN_PROTO>(std::shared_ptr<OUT_PROTO>,
                                                                   std::shared_ptr<MIDDLE_PROTO>)>;
  using ErrorHandler =
      std::function<void(const std::string &, const std::string &, const std::string &)>;

  static constexpr char const *LOGGING_NAME = "DapSerialConversationTask";

  DapSerialConversationTask(uint32_t msg_id, std::shared_ptr<OutboundConversations> outbounds,
                            std::shared_ptr<OefSearchEndpoint> endpoint, )
    : StateMachineTask<DapSerialConversationTask>(
          this, {&DapSerialConversationTask::progress, &DapSerialConversationTask::progress})
    , msg_id_{msg_id}
    , outbounds(std::move(outbounds))
    , endpoint(std::move(endpoint))

  {
    FETCH_LOG_INFO(LOGGING_NAME, "Task created.");
  }

  virtual ~DapSerialConversationTask()
  {
    FETCH_LOG_INFO(LOGGING_NAME, "Task gone.");
  }

  void setPipeBuilder(ProtoPipeBuilder func)
  {
    protoPipeBuilder = func;
  }

  void initPipe(std::shared_ptr<OUT_PROTO> init = nullptr)
  {
    if (init)
    {
      last_output = init;
    }
    else
    {
      last_output = std::make_shared<OUT_PROTO>();
    }
  }

  void add(PipeDataType pipeElement)
  {
    pipe_.push(std::move(pipeElement));
  }

  StateResult progress()
  {
    if (!last_output || !protoPipeBuilder)
    {
      FETCH_LOG_ERROR(LOGGING_NAME, "No last output or pipe builder set");
      return StateResult(0, ERRORED);
    }

    if (pipe_.size() == 0)
    {
      return StateResult(0, COMPLETE);
    }

    auto                                     this_sp = this->shared_from_this();
    std::weak_ptr<DapSerialConversationTask> this_wp = this_sp;

    auto data  = pipe_.front();
    auto input = protoPipeBuilder(last_output, data.proto);

    auto dapTask = std::make_shared<DapConversationTask<IN_PROTO, OUT_PROTO>>(
        data.dap_name, data.path, msg_id_, input, outbounds, endpoint);

    dapTask->messageHandler = [this_wp](std::shared_ptr<OUT_PROTO> response) {
      auto sp = this_wp.lock();
      if (sp)
      {
        sp->last_output = std::move(response);
      }
      else
      {
        FETCH_LOG_ERROR(LOGGING_NAME, "No shared pointer to DapSerialConversationTask");
      }
    };

    dapTask->errorHandler = [this_wp](const std::string &dap_name, const std::string &path) {
      auto sp = this_wp.lock();
      if (sp)
      {
        std::queue<PipeDataType> empty;
        std::swap(sp->pipe_, empty);
        sp->last_output = nullptr;
        if (sp->errorHandler)
        {
          sp->errorHandler(dap_name, path, "");
        }
      }
      else
      {
        FETCH_LOG_ERROR(LOGGING_NAME, "No shared pointer to DapSerialConversationTask");
      }
    };

    dapTask->submit();

    pipe_.pop();

    if (dapTask->makeNotification()
            .Then([this_wp]() {
              auto sp = this_wp.lock();
              if (sp)
              {
                sp->makeRunnable();
              }
            })
            .Waiting())
    {
      FETCH_LOG_INFO(LOGGING_NAME, "Sleeping");
      return StateResult(1, DEFER);
    }

    FETCH_LOG_INFO(LOGGING_NAME, "NOT Sleeping");
    return StateResult(1, COMPLETE);
  }

  std::shared_ptr<OUT_PROTO> getOutput()
  {
    return last_output;
  }

public:
  ErrorHandler errorHandler;

protected:
  uint32_t                               msg_id_;
  std::shared_ptr<OutboundConversations> outbounds;
  std::shared_ptr<OefSearchEndpoint>     endpoint;
  ProtoPipeBuilder                       protoPipeBuilder;

  std::shared_ptr<OUT_PROTO> last_output;
  std::queue<PipeDataType>   pipe_;

private:
  DapSerialConversationTask(const DapSerialConversationTask &other) = delete;  // { copy(other); }
  DapSerialConversationTask &operator=(const DapSerialConversationTask &other) =
      delete;  // { copy(other); return *this; }
  bool operator==(const DapSerialConversationTask &other) =
      delete;  // const { return compare(other)==0; }
  bool operator<(const DapSerialConversationTask &other) =
      delete;  // const { return compare(other)==-1; }
};
