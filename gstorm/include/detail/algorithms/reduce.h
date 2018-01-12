//
// Created by mhaidl on 23/08/16.
//

#pragma once

#include <range/v3/all.hpp>
#include <iostream>
#include <CL/sycl.hpp>

namespace gstorm {
namespace gpu {
namespace algorithm {

template <int x, typename... Ts>
class ReduceKernel{};

template<typename InRng, typename T, typename BinaryFunc, typename value_type = T>
auto reduce(InRng &in, T init, BinaryFunc func,
            cl::sycl::buffer<value_type, 1>& out, size_t thread_count,
            cl::sycl::handler &cgh) {

  size_t distance = ranges::v3::distance(in);
  cl::sycl::nd_range<1> config{thread_count, 128ul};

  const auto inIt = in.begin();
  {
    auto outAcc = out.template get_access<cl::sycl::access::mode::discard_write>(cgh);

    // FIXME: this reduction is super slow
    cgh.parallel_for< class ReduceKernel<1, BinaryFunc> >(config, [=](cl::sycl::nd_item<1> id) {
      auto gid = id.get_global(0);

      auto start = inIt + gid;

      value_type sum = value_type();

      for (size_t i = 0; i < distance; i += thread_count)
        sum = func(sum, *(start + i));

      outAcc[gid] = sum;
    });
  }
}
}
}
}

