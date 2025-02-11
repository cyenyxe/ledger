#pragma once
//------------------------------------------------------------------------------
//
//   Copyright 2018-2019 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include "core/byte_array/decoders.hpp"
#include "core/macros.hpp"
#include "crypto/ecdsa.hpp"
#include "dmlf/networkers/abstract_learner_networker.hpp"
#include "muddle/muddle_interface.hpp"
#include "muddle/rpc/client.hpp"
#include "muddle/rpc/server.hpp"
#include "network/management/network_manager.hpp"
#include "network/peer.hpp"
#include "network/service/protocol.hpp"
#include <memory>

namespace fetch {
namespace dmlf {

enum class MuddleChannel : uint16_t
{
  DEFAULT   = 1,
  MULTIPLEX = 2
};

class MuddleLearnerNetworker : public AbstractLearnerNetworker
{
public:
  using NetworkManager    = fetch::network::NetworkManager;
  using NetworkManagerPtr = std::shared_ptr<NetworkManager>;
  using MuddlePtr         = muddle::MuddlePtr;
  using MuddleEndpoint    = fetch::muddle::MuddleEndpoint;
  using RpcServer         = fetch::muddle::rpc::Server;
  using RpcClient         = fetch::muddle::rpc::Client;
  using Flag              = std::atomic<bool>;
  using Promise           = fetch::service::Promise;
  using Payload           = fetch::muddle::Packet::Payload;
  using Response          = MuddleEndpoint::Response;
  using Server            = fetch::muddle::rpc::Server;
  using CertificatePtr    = muddle::ProverPtr;
  using Uri               = fetch::network::Uri;

  using Mutex = fetch::Mutex;
  using Lock  = std::unique_lock<Mutex>;

  MuddleLearnerNetworker(
      const std::string &cloud_config, std::size_t instance_number,
      const std::shared_ptr<NetworkManager> &netm        = std::shared_ptr<NetworkManager>(),
      MuddleChannel                          channel_tmp = MuddleChannel::DEFAULT);
  ~MuddleLearnerNetworker() override                          = default;
  MuddleLearnerNetworker(const MuddleLearnerNetworker &other) = delete;
  MuddleLearnerNetworker &operator=(const MuddleLearnerNetworker &other)  = delete;
  bool                    operator==(const MuddleLearnerNetworker &other) = delete;
  bool                    operator<(const MuddleLearnerNetworker &other)  = delete;

  void        PushUpdate(const UpdateInterfacePtr &update) override;
  void        PushUpdateType(const std::string &type, const UpdateInterfacePtr &update) override;
  std::size_t GetPeerCount() const override;

  uint64_t RecvBytes(const byte_array::ByteArray &b);

  using Peer     = std::string;
  using Peers    = std::vector<Peer>;
  using PeerUris = std::unordered_set<std::string>;

protected:
  CertificatePtr CreateIdentity();
  CertificatePtr LoadIdentity(const std::string &privkey);

  CertificatePtr ident_;

  class MuddleLearnerNetworkerProtocol : public fetch::service::Protocol
  {
  public:
    enum
    {
      RECV_BYTES,
    };
    explicit MuddleLearnerNetworkerProtocol(MuddleLearnerNetworker &sample);
  };

  std::shared_ptr<NetworkManager>                 netm_;
  MuddlePtr                                       mud_;
  std::shared_ptr<Server>                         server_;
  std::shared_ptr<MuddleLearnerNetworkerProtocol> proto_;

  mutable Mutex mutex_;
  Peers         peers_;

  // TOFIX
  MuddleChannel channel_tmp_;

private:
};

}  // namespace dmlf
}  // namespace fetch
