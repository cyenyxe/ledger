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

#include "math/linalg/blas/gemm_tt_vector.hpp"

#include "math/linalg/blas/base.hpp"
#include "math/linalg/prototype.hpp"
#include "math/tensor_view.hpp"

namespace fetch {
namespace math {
namespace linalg {

template <typename S>
void Blas<S, Signature(_C <= _alpha, _A, _B, _beta, _C),
          Computes(_C <= _alpha * T(_A) * T(_B) + _beta * _C),
          platform::Parallelisation::VECTORISE>::operator()(Type const             alpha,
                                                            TensorView<Type> const a,
                                                            TensorView<Type> const b,
                                                            Type const             beta,
                                                            TensorView<Type>       c) const
{
  std::size_t i;
  std::size_t j;
  if ((c.height() == 0) ||
      ((c.width() == 0) || (((alpha == static_cast<Type>(0.0)) || (a.height() == 0)) &&
                            (beta == static_cast<Type>(1.0)))))
  {
    return;
  }

  if (alpha == static_cast<Type>(0.0))
  {
    if (beta == static_cast<Type>(0.0))
    {
      for (j = 0; j < c.width(); ++j)
      {
        Type zero{0.0};

        auto          ret_slice = c.data().slice(c.padded_height() * j, c.height());
        memory::Range range(std::size_t(0), std::size_t(c.height()));
        ret_slice.in_parallel().RangedApply(range, [zero](auto &&vw_c_j) {
          vw_c_j = static_cast<std::remove_reference_t<decltype(vw_c_j)>>(zero);
        });
      }
    }
    else
    {
      for (j = 0; j < c.width(); ++j)
      {
        auto ret_slice = c.data().slice(c.padded_height() * j, c.height());
        auto slice_c_j = c.data().slice(c.padded_height() * std::size_t(j), c.padded_height());
        memory::Range range(std::size_t(0), std::size_t(c.height()));
        ret_slice.in_parallel().RangedApplyMultiple(
            range,
            [beta](auto const &vr_c_j, auto &vw_c_j) {
              vw_c_j = static_cast<std::remove_reference_t<decltype(vw_c_j)>>(beta) * vr_c_j;
            },
            slice_c_j);
      }
    }

    return;
  }

  for (j = 0; j < c.width(); ++j)
  {
    for (i = 0; i < c.height(); ++i)
    {
      Type        temp;
      std::size_t l;
      temp = static_cast<Type>(0.0);
      for (l = 0; l < a.height(); ++l)
      {
        temp = temp + a(l, i) * b(j, l);
      }

      if (beta == static_cast<Type>(0.0))
      {
        c(i, j) = alpha * temp;
      }
      else
      {
        c(i, j) = alpha * temp + beta * c(i, j);
      }
    }
  }
}

template class Blas<double, Signature(_C <= _alpha, _A, _B, _beta, _C),
                    Computes(_C <= _alpha * T(_A) * T(_B) + _beta * _C),
                    platform::Parallelisation::VECTORISE>;

template class Blas<float, Signature(_C <= _alpha, _A, _B, _beta, _C),
                    Computes(_C <= _alpha * T(_A) * T(_B) + _beta * _C),
                    platform::Parallelisation::VECTORISE>;

}  // namespace linalg
}  // namespace math
}  // namespace fetch
