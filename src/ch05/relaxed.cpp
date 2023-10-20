//
// Created by iphelf on 2023-10-16.
//

#include <algorithm>
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
#include <latch>
#include <sstream>
#include <thread>
#include <vector>

void bad_example() {
  bool success{true};
  int cnt{1'000};
  do {
    std::atomic<bool> x{false};
    std::atomic<bool> y{false};
    std::atomic<int> z{0};
    std::latch latch{2};
    auto set_x_then_y{[&] {
      latch.arrive_and_wait();
      x.store(true, std::memory_order_relaxed);
      y.store(true, std::memory_order_relaxed);
    }};
    auto inc_z_if_y_implies_x{[&] {
      latch.arrive_and_wait();
      while (!y.load(std::memory_order_relaxed))
        ;
      if (x.load(std::memory_order_relaxed)) ++z;
    }};
    std::thread b{inc_z_if_y_implies_x};
    std::thread a{set_x_then_y};
    a.join();
    b.join();
    success = success && z > 0;
  } while (success && cnt-- > 0);
  assert(success);  // which is against the example (Listing 5.5) in book
}

void good_example() {
  const int n_dims{3};
  const int n_writers{n_dims};
  const int n_readers{2};
  const int n_iters{10};

  struct InconsistencyDetected : public std::exception {
    int a[n_dims]{};
    int b[n_dims]{};
    InconsistencyDetected(const int a[n_dims], const int b[n_dims]) {
      for (int i{0}; i < n_dims; ++i) {
        this->a[i] = a[i];
        this->b[i] = b[i];
      }
    }
  };

  struct Observation {
    int x[n_dims]{};
    Observation() {
      for (int i{0}; i < n_dims; ++i) x[i] = 0;
    }
    [[nodiscard]] std::string to_string() const {
      std::ostringstream oss;
      oss << '(';
      for (int i{0}; i < n_dims; ++i) {
        oss << x[i];
        if (i < n_dims - 1) oss << ", ";
      }
      oss << ')';
      return oss.str();
    }
    bool operator<(const Observation& rhs) const {
      bool le{true}, ge{true};
      for (int i{0}; i < n_dims; ++i) {
        le = le && (x[i] <= rhs.x[i]);
        ge = ge && (x[i] >= rhs.x[i]);
      }
      if (ge)
        return false;
      else if (le)
        return true;
      else
        throw InconsistencyDetected{x, rhs.x};
    }
  };

  bool detected_inconsistency{false};
  int n_trials{0};
  try {
    while (true) {
      ++n_trials;
      std::atomic<int> x[n_dims];
      for (auto& xi : x) xi.store(0);

      std::latch latch{n_writers + n_readers};
      std::vector<std::future<std::vector<Observation>>> os_futures;
      for (int wi{0}; wi < n_writers; ++wi)
        os_futures.push_back(std::async([&, wi] {
          std::vector<Observation> os(n_iters);
          latch.arrive_and_wait();
          for (int i{0}; i < n_iters; ++i) {
            for (int di{0}; di < n_dims; ++di)
              os[i].x[di] = x[di].load(std::memory_order_relaxed);
            x[wi].store(i + 1, std::memory_order_relaxed);
            std::this_thread::yield();
          }
          return os;
        }));
      for (int ri{0}; ri < n_readers; ++ri)
        os_futures.push_back(std::async([&] {
          std::vector<Observation> os(n_iters);
          latch.arrive_and_wait();
          for (int i{0}; i < n_iters; ++i) {
            for (int di{0}; di < n_dims; ++di)
              os[i].x[di] = x[di].load(std::memory_order_relaxed);
            std::this_thread::yield();
          }
          return os;
        }));

      std::vector<Observation> observations;
      observations.reserve(os_futures.size() * n_iters);
      for (auto& os_future : os_futures) {
        auto os{os_future.get()};
        // for (const auto& o : os) std::cout << o.to_string() << ' ';
        // std::cout << '\n';
        observations.insert(observations.end(), os.begin(), os.end());
      }

      std::sort(observations.begin(), observations.end());
      // for (const auto& o : observations) std::cout << o.to_string() << ' ';
      // std::cout << '\n';
      bool sorted{std::is_sorted(observations.begin(), observations.end())};
      assert(sorted);
    }
  } catch (InconsistencyDetected& inconsistency) {
    detected_inconsistency = true;
  } catch (...) {
    abort();
  }
  assert(detected_inconsistency);
}

int main() {
  bad_example();
  good_example();
}
