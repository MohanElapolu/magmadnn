/**
 * @file dot.cpp
 * @author Daniel Nichols
 * @version 0.1
 * @date 2019-06-07
 *
 * @copyright Copyright (c) 2019
 */
#include "math/dot.h"

namespace magmadnn {
namespace math {

#define comp_cpu(type) template void dot<CPU, type>(type, bool, const Tensor &, bool, const Tensor &, type, Tensor &);
CALL_FOR_ALL_TYPES(comp_cpu)
#undef comp_cpu

}  // namespace math
}  // namespace magmadnn