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

#include "vm/common.hpp"
#include "vm/object.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace fetch {
namespace vm {

class VM;

template <typename T>
struct Array;

struct String : public Object
{
  String()           = delete;
  ~String() override = default;

  String(VM *vm, std::string str__, bool is_literal__ = false);

  int32_t                 Length() const;
  int32_t                 SizeInBytes() const;
  void                    Trim();
  int32_t                 Find(Ptr<String> const &substring) const;
  Ptr<String>             Substring(int32_t start_index, int32_t end_index);
  void                    Reverse();
  Ptr<Array<Ptr<String>>> Split(Ptr<String> const &separator) const;

  std::size_t GetHashCode() override;
  bool        IsEqual(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override;
  bool        IsNotEqual(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override;
  bool        IsLessThan(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override;
  bool        IsLessThanOrEqual(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override;
  bool        IsGreaterThan(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override;
  bool        IsGreaterThanOrEqual(Ptr<Object> const &lhso, Ptr<Object> const &rhso) override;
  void        Add(Ptr<Object> &lhso, Ptr<Object> &rhso) override;

  bool SerializeTo(MsgPackSerializer &buffer) override;
  bool DeserializeFrom(MsgPackSerializer &buffer) override;

  std::string str;
  bool        is_literal;
  int32_t     length;
};

}  // namespace vm
}  // namespace fetch
