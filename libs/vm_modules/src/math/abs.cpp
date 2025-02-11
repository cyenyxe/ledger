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

#include "math/meta/math_type_traits.hpp"
#include "math/standard_functions/abs.hpp"
#include "vm/module.hpp"
#include "vm/vm.hpp"
#include "vm_modules/math/abs.hpp"

#include <cstdint>
#include <cstdlib>

using namespace fetch::vm;

namespace fetch {
namespace vm_modules {
namespace math {

namespace {

/**
 * method for taking the absolute of a value
 */
template <typename T>
fetch::math::meta::IfIsMath<T, T> Abs(VM * /*vm*/, T const &a)
{
  T x;
  fetch::math::Abs(a, x);
  return x;
}

template <typename T, typename R = void>
using IfIsNormalSignedInteger = meta::EnableIf<meta::IsSignedInteger<T> && sizeof(T) >= 4, R>;

template <typename T, typename R = void>
using IfIsSmallSignedInteger = meta::EnableIf<meta::IsSignedInteger<T> && sizeof(T) < 4, R>;

template <typename T>
meta::EnableIf<sizeof(T) < 4, int32_t> ToAtLeastInt(T const &value)
{
  return static_cast<int32_t>(value);
}

template <typename T>
IfIsSmallSignedInteger<T, T> IntegerAbs(VM * /*vm*/, T const &value)
{
  return static_cast<T>(std::abs(value));
}

template <typename T>
IfIsNormalSignedInteger<T, T> IntegerAbs(VM * /*vm*/, T const &value)
{
  return std::abs(value);
}

template <typename T>
meta::IfIsUnsignedInteger<T, T> IntegerAbs(VM * /*vm*/, T const &value)
{
  return value;
}

}  // namespace

void BindAbs(Module &module)
{
  module.CreateFreeFunction("abs", &IntegerAbs<int8_t>);
  module.CreateFreeFunction("abs", &IntegerAbs<int16_t>);
  module.CreateFreeFunction("abs", &IntegerAbs<int32_t>);
  module.CreateFreeFunction("abs", &IntegerAbs<int64_t>);

  module.CreateFreeFunction("abs", &IntegerAbs<uint8_t>);
  module.CreateFreeFunction("abs", &IntegerAbs<uint16_t>);
  module.CreateFreeFunction("abs", &IntegerAbs<uint32_t>);
  module.CreateFreeFunction("abs", &IntegerAbs<uint64_t>);

  module.CreateFreeFunction("abs", &Abs<float_t>);
  module.CreateFreeFunction("abs", &Abs<double_t>);

  module.CreateFreeFunction("abs", &Abs<fixed_point::fp32_t>);
  module.CreateFreeFunction("abs", &Abs<fixed_point::fp64_t>);
}

}  // namespace math
}  // namespace vm_modules
}  // namespace fetch
