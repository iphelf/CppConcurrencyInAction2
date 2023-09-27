//
// Created by iphelf on 2023-09-25.
//

#include <cassert>
#include <mutex>

template <typename T>
class Resource;

template <typename T>
class Accessor {
  friend class Resource<T>;
  std::unique_lock<std::mutex> lock;
  Resource<T> *r;

 public:
  void set(T value);
  const T &get();
  Accessor(const Accessor &) = delete;
  Accessor &operator=(const Accessor &) = delete;
  Accessor(Accessor &&) = default;
  Accessor &operator=(Accessor &&) = default;

 private:
  explicit Accessor(Resource<T> *r);
};

template <typename T>
class Resource {
  friend class Accessor<T>;
  std::mutex mutex{};
  T data{};

 public:
  Resource() = default;
  explicit Resource(T data) : data{data} {}
  Accessor<T> lock() {
    Accessor<T> accessor{this};
    return accessor;
  }
};

template <typename T>
Accessor<T>::Accessor(Resource<T> *r) : lock{r->mutex}, r{r} {}

template <typename T>
void Accessor<T>::set(T value) {
  assert(lock.owns_lock());
  r->data = value;
}

template <typename T>
const T &Accessor<T>::get() {
  assert(lock.owns_lock());
  return r->data;
}

int main() {
  Resource r{0};
  {
    auto a{r.lock()};
    assert(a.get() == 0);
    a.set(1);
    assert(a.get() == 1);
  }
  {
    auto a{r.lock()};
    assert(a.get() == 1);
    a.set(2);
    assert(a.get() == 2);
  }
}
