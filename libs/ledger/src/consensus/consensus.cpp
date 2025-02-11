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

#include "core/random/lcg.hpp"

#include <ctime>
#include <random>
#include <utility>

#include "beacon/block_entropy.hpp"
#include "ledger/consensus/consensus.hpp"

constexpr char const *LOGGING_NAME = "Consensus";

using Consensus       = fetch::ledger::Consensus;
using NextBlockPtr    = Consensus::NextBlockPtr;
using Status          = Consensus::Status;
using StakeManagerPtr = Consensus::StakeManagerPtr;

using fetch::ledger::MainChain;
using fetch::ledger::Block;
using fetch::beacon::BlockEntropy;

namespace {
using DRNG = fetch::random::LinearCongruentialGenerator;

std::size_t SafeDecrement(std::size_t value, std::size_t decrement)
{
  if (decrement >= value)
  {
    return 0;
  }

  return value - decrement;
}

template <typename T>
T DeterministicShuffle(T &container, uint64_t entropy)
{
  DRNG rng(entropy);
  std::shuffle(container.begin(), container.end(), rng);
  return container;
}
}  // namespace

Consensus::Consensus(StakeManagerPtr stake, BeaconServicePtr beacon, MainChain const &chain,
                     Identity mining_identity, uint64_t aeon_period, uint64_t max_committee_size,
                     uint32_t block_interval_ms)
  : stake_{std::move(stake)}
  , beacon_{std::move(beacon)}
  , chain_{chain}
  , mining_identity_{std::move(mining_identity)}
  , mining_address_{Address(mining_identity_)}
  , aeon_period_{aeon_period}
  , max_committee_size_{max_committee_size}
  , block_interval_ms_{block_interval_ms}
{
  assert(stake_);
  FETCH_UNUSED(chain_);
}

// TODO(HUT): probably this is not required any more.
Consensus::CommitteePtr Consensus::GetCommittee(Block const &previous)
{
  // Calculate the last relevant snapshot
  uint64_t const last_snapshot =
      previous.body.block_number - (previous.body.block_number % aeon_period_);

  // Invalid to request a committee too far ahead in time
  assert(committee_history_.find(last_snapshot) != committee_history_.end());

  if (committee_history_.find(last_snapshot) == committee_history_.end())
  {
    FETCH_LOG_INFO(LOGGING_NAME,
                   "No committee history found for block: ", previous.body.block_number, " AKA ",
                   last_snapshot);
    return nullptr;
  }

  // If the last committee was the valid committee, return this. Otherwise, deterministically
  // shuffle the committee using the random beacon
  if (last_snapshot == previous.body.block_number)
  {
    return committee_history_.at(last_snapshot);
  }

  CommitteePtr committee_ptr = committee_history_[last_snapshot];
  assert(!committee_ptr->empty());

  Committee committee_copy = *committee_ptr;

  DeterministicShuffle(committee_copy, previous.body.block_entropy.EntropyAsU64());

  return std::make_shared<Committee>(committee_copy);
}

bool Consensus::ValidMinerForBlock(Block const &previous, Address const &address)
{
  auto const committee = GetCommittee(previous);

  if (!committee || committee->empty())
  {
    FETCH_LOG_WARN(LOGGING_NAME, "Unable to determine committee for block validation");
    return false;
  }

  return std::find_if((*committee).begin(), (*committee).end(),
                      [&address](Identity const &identity) {
                        return address == Address(identity);
                      }) != (*committee).end();
}

Block GetBlockPriorTo(Block const &current, MainChain const &chain)
{
  return *chain.GetBlock(current.body.previous_hash);
}

Block GetBeginningOfAeon(Block const &current, MainChain const &chain)
{
  Block ret = current;

  // Walk back the chain until we see a block specifying an aeon beginning (corner
  // case for true genesis)
  while (!ret.body.block_entropy.IsAeonBeginning() && !(current.body.block_number == 0))
  {
    ret = GetBlockPriorTo(ret, chain);
  }

  return ret;
}

uint64_t Consensus::GetBlockGenerationWeight(Block const &previous, Address const &address)
{
  auto const committee = GetCommittee(previous);

  if (!committee)
  {
    FETCH_LOG_WARN(LOGGING_NAME, "Unable to determine block generation weight");
    return 0;
  }

  std::size_t weight{committee->size()};

  // TODO(EJF): Depending on the committee sizes this would need to be improved
  for (auto const &member : *committee)
  {
    if (address == Address(member))
    {
      break;
    }

    weight = SafeDecrement(weight, 1);
  }

  // Note: weight must always be non zero (indicates failure/not in committee)
  return weight;
}

Consensus::WeightedQual QualWeightedByEntropy(BlockEntropy::Cabinet const &cabinet,
                                              uint64_t                     entropy)
{
  Consensus::WeightedQual ret;
  ret.reserve(cabinet.size());

  for (auto const &i : cabinet)
  {
    ret.emplace_back(i);
  }

  return DeterministicShuffle(ret, entropy);
}

/**
 * Determine whether the proposed block is valid according to consensus timing requirements.
 * Requirements for this are that the miner is a member of qual, and that it is within its rights
 * according to the time slot protocol and its weighting. So it tests:
 *
 * - block made by member of qual
 * -
 */
bool Consensus::ValidBlockTiming(Block const &previous, Block const &proposed) const
{
  FETCH_LOG_DEBUG(LOGGING_NAME, "Should generate block? Prev: ", previous.body.block_number);

  Identity const &identity = proposed.body.miner_id;

  // Have to use the proposed block for this fn in case the block would be a new
  // aeon beginning.
  Block beginning_of_aeon = GetBeginningOfAeon(proposed, chain_);

  BlockEntropy::Cabinet qualified_cabinet = beginning_of_aeon.body.block_entropy.qualified;
  auto                  qualified_cabinet_weighted =
      QualWeightedByEntropy(qualified_cabinet, previous.body.block_entropy.EntropyAsU64());

  if (qualified_cabinet.find(identity.identifier()) == qualified_cabinet.end())
  {
    FETCH_LOG_INFO(LOGGING_NAME, "Miner ", identity.identifier().ToBase64(),
                   " attempted to mine block ", previous.body.block_number + 1,
                   " but was not part of qual:");

    for (auto const &member : qualified_cabinet)
    {
      FETCH_LOG_INFO(LOGGING_NAME, member.ToBase64());
    }

    return false;
  }

  // TODO(HUT): check block has been signed correctly

  // Time slot protocol: within the block period, only the heaviest weighted miner may produce a
  // block, outside this interval, any miner may produce a block.
  uint64_t last_block_timestamp_ms     = previous.body.timestamp * 1000;
  uint64_t proposed_block_timestamp_ms = proposed.body.timestamp * 1000;
  uint64_t time_now_ms =
      GetTime(fetch::moment::GetClock("default", fetch::moment::ClockType::SYSTEM)) * 1000;

  // Blocks must be ahead in time of the previous, and less than or equal to current time or they
  // will be rejected.
  if (proposed_block_timestamp_ms > time_now_ms)
  {
    FETCH_LOG_WARN(LOGGING_NAME,
                   "Found block that appears to be minted ahead in time. This is invalid.");
    return false;
  }

  if (proposed_block_timestamp_ms < last_block_timestamp_ms)
  {
    FETCH_LOG_WARN(
        LOGGING_NAME,
        "Found block that indicates it was minted before the previous. This is invalid.");
    return false;
  }

  assert(!qualified_cabinet_weighted.empty());

  // First qual member can always produce
  if (*qualified_cabinet_weighted.begin() == identity)
  {
    return true;
  }

  // Until the time slot has elapsed, others can not produce
  if (last_block_timestamp_ms + block_interval_ms_ < time_now_ms)
  {
    return true;
  }

  return false;
}

/**
 * Whether the committee should be triggered
 */
bool ShouldTriggerAeon(uint64_t block_number, uint64_t aeon_period)
{
  return (block_number % aeon_period) == 0;
}

/**
 * Trigger a new committee on a trigger point, so long as the exact committee
 * wasn't already triggered. This will allow alternating committees to be triggered for the
 * same block height.
 *
 */
bool Consensus::ShouldTriggerNewCommittee(Block const &block)
{
  bool const trigger_point = ShouldTriggerAeon(block.body.block_number, aeon_period_);

  if (last_triggered_committee_ != block.body.hash && trigger_point)
  {
    last_triggered_committee_ = block.body.hash;
    return true;
  }

  return false;
}

void Consensus::UpdateCurrentBlock(Block const &current)
{
  bool const one_ahead = current.body.block_number == current_block_.body.block_number + 1;

  if (current.body.block_number > current_block_.body.block_number && !one_ahead)
  {
    FETCH_LOG_ERROR(LOGGING_NAME,
                    "Updating the current block more than one block ahead is invalid! current: ",
                    current_block_.body.block_number, " Attempt: ", current.body.block_number);
    return;
  }

  // Don't try to set previous when we see genesis!
  if (current.body.block_number == 0)
  {
    current_block_ = current;
  }
  else
  {
    current_block_     = current;
    previous_block_    = GetBlockPriorTo(current_block_, chain_);
    beginning_of_aeon_ = GetBeginningOfAeon(current_block_, chain_);
  }

  stake_->UpdateCurrentBlock(current_block_);

  if (ShouldTriggerNewCommittee(current_block_))
  {
    CabinetMemberList cabinet_member_list;
    committee_history_[current.body.block_number] = stake_->BuildCommittee(current_block_);

    TrimToSize(committee_history_, HISTORY_LENGTH);

    for (auto const &staker : *committee_history_[current.body.block_number])
    {
      FETCH_LOG_DEBUG(LOGGING_NAME, "Adding staker: ", staker.identifier().ToBase64());
      cabinet_member_list.insert(staker.identifier());
    }

    auto threshold = static_cast<uint32_t>(
        std::ceil(static_cast<double>(cabinet_member_list.size()) * threshold_));

    FETCH_LOG_INFO(LOGGING_NAME, "Block: ", current_block_.body.block_number,
                   " creating new aeon. Periodicity: ", aeon_period_, " threshold: ", threshold,
                   " as double: ", threshold_, " cabinet size: ", cabinet_member_list.size());

    uint64_t last_block_time = current.body.timestamp;
    auto     current_time =
        GetTime(fetch::moment::GetClock("default", fetch::moment::ClockType::SYSTEM));

    if (current.body.block_number == 0)
    {
      last_block_time = default_start_time_;
    }

    FETCH_LOG_INFO(LOGGING_NAME, "Starting DKG with timestamp: ", last_block_time,
                   " current: ", current_time,
                   " diff: ", int64_t(current_time) - int64_t(last_block_time));

    uint64_t block_interval = 1;

    // Safe to call this multiple times
    beacon_->StartNewCabinet(cabinet_member_list, threshold, current_block_.body.block_number + 1,
                             current_block_.body.block_number + aeon_period_,
                             last_block_time + block_interval, current.body.block_entropy);
  }

  beacon_->AbortCabinet(current_block_.body.block_number);
}

// TODO(HUT): put block number confirmation/check here (?)
NextBlockPtr Consensus::GenerateNextBlock()
{
  NextBlockPtr ret;

  // Number of block we want to generate
  uint64_t const block_number = current_block_.body.block_number + 1;

  ret = std::make_unique<Block>();

  // Note, it is important to do this here so the block when passed to ValidBlockTiming
  // is well formed
  ret->body.previous_hash = current_block_.body.hash;
  ret->body.block_number  = block_number;
  ret->body.miner         = mining_address_;
  ret->body.miner_id      = mining_identity_;
  ret->body.timestamp =
      GetTime(fetch::moment::GetClock("default", fetch::moment::ClockType::SYSTEM));
  ret->weight = GetBlockGenerationWeight(current_block_, mining_address_);

  // Try to get entropy for the block we are generating - is allowed to fail if we request too
  // early
  if (EntropyGeneratorInterface::Status::OK !=
      beacon_->GenerateEntropy(block_number, ret->body.block_entropy))
  {
    return {};
  }

  // Note here the previous block's entropy determines miner selection
  if (!ValidBlockTiming(current_block_, *ret))
  {
    return {};
  }

  return ret;
}

Status Consensus::ValidBlock(Block const &current) const
{
  Status ret = Status::YES;

  // TODO(HUT): more thorough checks for genesis needed
  if (current.body.block_number == 0)
  {
    return Status::YES;
  }

  auto const block_preceeding = GetBlockPriorTo(current, chain_);

  BlockEntropy::Cabinet        qualified_cabinet;
  BlockEntropy::GroupPublicKey group_pub_key;

  // If this block would be the start of a new aeon, need to check the stakers for the prev. block
  if (ShouldTriggerAeon(block_preceeding.body.block_number, aeon_period_))
  {
    auto const &block_entropy = current.body.block_entropy;

    if (!block_entropy.IsAeonBeginning())
    {
      FETCH_LOG_WARN(LOGGING_NAME,
                     "Found block that didn't create a new aeon when it should have!");
      return Status::NO;
    }

    // Check that the members of qual have all signed correctly

    // Check that the members of qual meet threshold requirements

    qualified_cabinet = block_entropy.qualified;
    group_pub_key     = block_entropy.group_public_key;
  }
  else
  {
    auto beginning_of_aeon = GetBeginningOfAeon(current, chain_);
    qualified_cabinet      = beginning_of_aeon.body.block_entropy.qualified;
    group_pub_key          = beginning_of_aeon.body.block_entropy.group_public_key;
  }

  //// TODO(HUT): check signatures are an unbroken chain.
  //// Determine that the entropy is correct (a signature of the previous signature)
  // if(!dkg::BeaconManager::Verify(group_pub_key,
  // block_preceeding.body.block_entropy.group_signature.getStr(),
  // current.body.block_entropy.group_signature.getStr()))
  //{
  //  FETCH_LOG_WARN(LOGGING_NAME, "Found block whose entropy isn't a signature of the previous!");
  //  return Status::NO;
  //}

  // Perform the time checks (also qual adherence). Note, this check should be last, as the checking
  // logic relies on a well formed block.
  if (!ValidBlockTiming(block_preceeding, current))
  {
    FETCH_LOG_WARN(LOGGING_NAME, "Found block with bad timings!");
    return Status::NO;
  }

  return ret;
}

void Consensus::Reset(StakeSnapshot const &snapshot)
{
  committee_history_[0] = stake_->Reset(snapshot);

  if (committee_history_.find(0) == committee_history_.end())
  {
    FETCH_LOG_INFO(LOGGING_NAME, "No committee history found for block when resetting.");
  }
}

void Consensus::Refresh()
{}

void Consensus::SetThreshold(double threshold)
{
  threshold_ = threshold;
  FETCH_LOG_INFO(LOGGING_NAME, "Set threshold to ", threshold_);
}

void Consensus::SetCommitteeSize(uint64_t size)
{
  max_committee_size_ = size;
  stake_->SetCommitteeSize(max_committee_size_);
}

StakeManagerPtr Consensus::stake()
{
  return stake_;
}

void Consensus::SetDefaultStartTime(uint64_t default_start_time)
{
  default_start_time_ = default_start_time;
}
