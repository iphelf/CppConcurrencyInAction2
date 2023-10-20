//
// Created by iphelf on 2023-10-08.
//

#include <atomic>
#include <cassert>

void test_atomic_bool() {
  std::atomic<bool> b{false};
  static_assert(std::atomic<bool>::is_always_lock_free);
  assert(b.is_lock_free());
  {
    b = false;
    const bool expected{b};
    const bool desired{!b};
    bool bad_expectation{!expected};
    // _weak may fail when
    //   1) the holding value does not equal to the expected value
    //   2) no reason
    // _weak succeeds only when
    //   1) the holding value equals to the expected value
    //   2) exchange succeeded
    while (true) {
      bool exchanged{b.compare_exchange_weak(bad_expectation, desired)};
      assert(!exchanged);
      if (bad_expectation == expected) {
        // _weak failed due to unmet expectation instead of no reason
        break;
      }
      // _weak failed due to no reason instead of unmet expectation
    }
    assert(bad_expectation == expected);
  }
  {
    b = false;
    const bool expected{b};
    const bool desired{!b};
    bool bad_expectation{!expected};
    bool exchanged{b.compare_exchange_strong(bad_expectation, desired)};
    assert(!exchanged);
    assert(bad_expectation == expected);
  }
  {
    b = false;
    const bool expected{b};
    const bool desired{!b};
    bool expectation{expected};
    bool exchanged{b.compare_exchange_strong(expectation, desired)};
    assert(exchanged);
    assert(expectation == expected);
    assert(b == desired);
  }
  {
    b = false;
    assert(!b.load());
    b.store(true);
    assert(b);
  }
}

void test_atomic_ptr() {
  int a[3] = {1, 2, 3};
  std::atomic<int *> p{a};
  int *old_p{p.fetch_add(2)};
  assert(old_p == a);
  assert(p == a + 2);
  old_p = p.fetch_sub(2);
  assert(old_p == a + 2);
  assert(p == a);
}

void test_atomic_udt() {
  {
    class UDT {
     public:
      int id{0};
      explicit UDT(int id = 0) : id{id} {}
      //  UDT(const UDT&) {}  // non-trivial
      //  UDT(UDT&&) {}  // non-trivial
      //  ~UDT() {}  // non-trivial
      bool operator==(const UDT &rhs) const { return id == rhs.id; }
      bool operator==(UDT &&rhs) const { return id == rhs.id; }
      bool operator!=(const UDT &rhs) const { return id != rhs.id; }
      bool operator!=(UDT &&rhs) const { return id != rhs.id; }
    };
    std::atomic<UDT> udt;
    // udt is default initialized (standard) except on clang (seems like a BUG)
#ifdef __clang__
    std::atomic_init(&udt, UDT{});
#endif
    UDT expected;
    UDT desired{1};
    assert(expected == udt);
    assert(desired != udt);
    bool exchanged{udt.compare_exchange_strong(expected, desired)};
    assert(exchanged);
    assert(desired == udt);
    exchanged = udt.compare_exchange_strong(expected, desired);
    assert(!exchanged);
    assert(expected == udt);
  }
  {
    struct Int {
      char bytes[4];
    };
    static_assert(std::atomic<Int>::is_always_lock_free);
    struct Large {
      char bytes[256];
    };
    static_assert(!std::atomic<Large>::is_always_lock_free);
  }
}

int main() {
  test_atomic_bool();
  test_atomic_ptr();
  test_atomic_udt();
}
