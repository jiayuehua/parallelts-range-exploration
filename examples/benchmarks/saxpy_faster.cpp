#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <tuple>
#include <vector>

#include <gstorm.h>
#include <CL/sycl.hpp>
#include <range/v3/all.hpp>

#include "experimental.h"
#include "my_zip.h"

template <typename T>
struct SaxpyOperator {
  constexpr SaxpyOperator(T a) : a(a){};
  const T a;
  T operator()(T x, T y) const { return a * x + y; }
};

int main(int argc, char* argv[]) {
  const size_t base_size = 1024 * 1024;

  size_t multiplier = 16;
  if (argc > 1) {
    std::stringstream{argv[1]} >> multiplier;
  }

  std::cout << "Size: " << multiplier << "\n";
  const auto vsize = base_size * multiplier;
  const auto iterations = 100;

  std::default_random_engine generator;
  std::uniform_real_distribution<float> distribution(0.0, 10.0);

  auto generate_float = [&generator, &distribution]() {
    return distribution(generator);
  };
  auto x = static_cast<float*>(aligned_alloc(4096, vsize * sizeof(float)));
  auto y = static_cast<float*>(aligned_alloc(4096, vsize * sizeof(float)));
  auto z = static_cast<float*>(aligned_alloc(4096, vsize * sizeof(float)));

  for (auto i = 0u; i < vsize; ++i) {
    x[i] = generate_float();
    y[i] = generate_float();
  }
  // Input to the SYCL device
  // std::vector<float> x(vsize);
  // std::vector<float> y(vsize);

  // ranges::generate(x, generate_float);
  // ranges::generate(y, generate_float);

  const float a = generate_float();

  // std::vector<float> z(vsize);
  std::vector<double> times{};

  auto q = cl::sycl::queue{};
  std::cout << "Using device: "
            << q.get_device().get_info<cl::sycl::info::device::name>()
            << ", from: "
            << q.get_device()
                   .get_platform()
                   .get_info<cl::sycl::info::platform::name>()
            << "\n";

  for (auto i = 0; i < iterations; ++i) {
    auto start = std::chrono::system_clock::now();
    {
      gstorm::sycl_exec exec(q);

      // auto gpu_x = std::experimental::copy(exec, x);
      // auto gpu_y = std::experimental::copy(exec, y);
      // auto gpu_z = std::experimental::copy(exec, z);
      auto gpu_x = gstorm::range::gvector<std::vector<float>>(x, vsize);
      auto gpu_y = gstorm::range::gvector<std::vector<float>>(y, vsize);
      auto gpu_z = gstorm::range::gvector<std::vector<float>>(z, vsize);

      exec.registerGVector(&gpu_x);
      exec.registerGVector(&gpu_y);
      exec.registerGVector(&gpu_z);

      std::experimental::transform(exec, gpu_x, gpu_y, gpu_z,
                                   SaxpyOperator<float>{a});
    }
    auto end = std::chrono::system_clock::now();

    auto time_taken =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    times.push_back(time_taken.count() / 1000.0);
    std::cout << "\r" << (i + 1) << "/" << iterations << std::flush;
  }
  std::cout << "\n";

  ranges::sort(times);
  std::cout << "Median time: " << times[iterations / 2] << " ms\n";

  std::vector<float> expected(vsize);
  for (auto i = 0u; i < vsize; ++i) {
    expected[i] = a * x[i] + y[i];
  }

  for (auto i = 0u; i < vsize; ++i) {
    if (z[i] != expected[i]) {
      std::cout << "Mismatch between expected and actual result!\n";
      break;
    }
  }

  free(x);
  free(y);
  free(z);
}