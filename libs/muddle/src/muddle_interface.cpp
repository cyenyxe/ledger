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

#include "muddle.hpp"
#include "muddle_fake.hpp"

#include "crypto/ecdsa.hpp"
#include "muddle/muddle_interface.hpp"
#include "muddle/network_id.hpp"

namespace fetch {
namespace muddle {

MuddlePtr CreateMuddle(NetworkId const &network, ProverPtr certificate,
                       network::NetworkManager const &nm, std::string const &external_address)
{
  // enable all message signing
  return std::make_shared<Muddle>(network, certificate, nm, true, true, external_address);
}

MuddlePtr CreateMuddle(char const network[4], ProverPtr certificate,
                       network::NetworkManager const &nm, std::string const &external_address)
{
  return CreateMuddle(NetworkId{network}, std::move(certificate), nm, external_address);
}

MuddlePtr CreateMuddle(NetworkId const &network, network::NetworkManager const &nm,
                       std::string const &external_address)
{
  ProverPtr certificate = std::make_shared<crypto::ECDSASigner>();
  return CreateMuddle(network, std::move(certificate), nm, external_address);
}

MuddlePtr CreateMuddle(char const network[4], network::NetworkManager const &nm,
                       std::string const &external_address)
{
  return CreateMuddle(NetworkId{network}, nm, external_address);
}

}  // namespace muddle
}  // namespace fetch
