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

#include "core/byte_array/const_byte_array.hpp"
#include "core/byte_array/decoders.hpp"
#include "core/byte_array/encoders.hpp"
#include "ledger/chain/address.hpp"

#include "vm/vm.hpp"

namespace fetch {
namespace vm {

class Address : public Object
{
public:
  static constexpr std::size_t RAW_BYTES_SIZE = 32;

  using Buffer = std::vector<uint8_t>;

  static Ptr<Address> Constructor(VM *vm, TypeId type_id)
  {
    return Ptr<Address>{new Address{vm, type_id}};
  }

  static Ptr<Address> ConstructorFromString(VM *vm, TypeId type_id, Ptr<String> const &address)
  {
    return Ptr<Address>{new Address{vm, type_id, address}};
  }

  static Ptr<String> ToString(VM * /*vm*/, Ptr<Address> const &address)
  {
    return address->AsString();
  }

  // Construction / Destruction
  Address(VM *vm, TypeId type_id, Ptr<String> const &address = Ptr<String>{},
          bool signed_tx = false)
    : Object(vm, type_id)
    , signed_tx_{signed_tx}
  {
    if (address && !ledger::Address::Parse(address->str.c_str(), address_))
    {
      vm->RuntimeError("Unable to parse address");
    }
  }

  ~Address() override = default;

  bool HasSignedTx() const
  {
    return signed_tx_;
  }

  void SetSignedTx(bool set = true)
  {
    signed_tx_ = set;
  }

  Ptr<String> AsString()
  {
    return Ptr<String>{new String{vm_, std::string{address_.display()}}};
  }

  std::vector<uint8_t> ToBytes() const
  {
    auto const &address = address_.address();
    auto const *start   = address.pointer();
    auto const *end     = start + address.size();

    return {start, end};
  }

  void FromBytes(std::vector<uint8_t> &&data)
  {
    if (RAW_BYTES_SIZE != data.size())
    {
      vm_->RuntimeError("Invalid address format");
      return;
    }

    // update the value
    address_ = ledger::Address({data.data(), data.size()});
  }

  ledger::Address const &address() const
  {
    return address_;
  }

  Address &operator=(ledger::Address const &address)
  {
    address_ = address;
    return *this;
  }

  bool operator==(ledger::Address const &other) const
  {
    return address_ == other;
  }

  bool SerializeTo(MsgPackSerializer &buffer) override
  {
    buffer << address_.address();
    return true;
  }

  bool DeserializeFrom(MsgPackSerializer &buffer) override
  {
    fetch::byte_array::ConstByteArray raw_address{};
    buffer >> raw_address;
    address_ = ledger::Address{raw_address};
    return true;
  }

  bool IsEqual(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override
  {
    Ptr<Address> lhs = lhso;
    Ptr<Address> rhs = rhso;
    return lhs->address_ == rhs->address_;
  }

  bool IsNotEqual(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override
  {
    Ptr<Address> lhs = lhso;
    Ptr<Address> rhs = rhso;
    return lhs->address_ != rhs->address_;
  }

  bool IsLessThan(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override
  {
    Ptr<Address> lhs = lhso;
    Ptr<Address> rhs = rhso;
    return lhs->address_ < rhs->address_;
  }

  bool IsLessThanOrEqual(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override
  {
    Ptr<Address> lhs = lhso;
    Ptr<Address> rhs = rhso;
    return lhs->address_ <= rhs->address_;
  }

  bool IsGreaterThan(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override
  {
    Ptr<Address> lhs = lhso;
    Ptr<Address> rhs = rhso;
    return lhs->address_ > rhs->address_;
  }

  bool IsGreaterThanOrEqual(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override
  {
    Ptr<Address> lhs = lhso;
    Ptr<Address> rhs = rhso;
    return lhs->address_ >= rhs->address_;
  }

  bool ToJSON(vm::JSONVariant &variant) override
  {
    variant = address_.display();
    return true;
  }

  bool FromJSON(vm::JSONVariant const &obj) override
  {
    if (!ledger::Address::Parse(obj.template As<byte_array::ConstByteArray>(), address_))
    {
      vm_->RuntimeError("Unable to parse address during JSON deserialization of " + GetTypeName() +
                        ".");
    }
    return true;
  }

private:
  ledger::Address address_;
  bool            signed_tx_{false};
};

}  // namespace vm
}  // namespace fetch
