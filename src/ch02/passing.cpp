//
// Created by iphelf on 2023-09-22.
//

#include <cassert>
#include <thread>
#include <vector>

void test_copy_elision() {
  static int last_id{-1};
  static std::vector<int> records;
  struct Resource {
    int id;
    Resource() : id{++last_id} { records.push_back(id); }
    Resource(const Resource &) : Resource() {}
    Resource &operator=(const Resource &) = delete;
    Resource(Resource &&rhs) noexcept : id{rhs.id} { rhs.id = -1; }
    Resource &operator=(Resource &&) = delete;
    ~Resource() {
      if (id != -1) records.push_back(-id);
    }
  };
  auto create_resource{[] { return Resource{}; }};
  auto pass_by_value{[](auto v) { return v; }};
  auto pass_by_clr([](const auto &v) -> const auto & { return v; });
  auto pass_by_r([](auto &&v) { return v; });
  auto take_by_value{[](auto v) {}};
  auto take_by_clr{[](const auto &v) {}};
  auto take_by_r{[](auto &&v) {}};
  {
    last_id = -1;
    records.clear();
    take_by_value(pass_by_value(create_resource()));
    assert(records == (std::vector{0, 0}));
  }
  {
    last_id = -1;
    records.clear();
    take_by_value(pass_by_value(pass_by_r(create_resource())));
    assert(records == (std::vector{0, 0}));
  }
  {
    last_id = -1;
    records.clear();
    take_by_value(pass_by_clr(create_resource()));
    assert(records == (std::vector{0, 1, -1, 0}));
  }
  {
    last_id = -1;
    records.clear();
    take_by_r(pass_by_clr(create_resource()));
    assert(records == (std::vector{0, 0}));
  }
  {
    last_id = -1;
    records.clear();
    { auto value{pass_by_value(pass_by_value(create_resource()))}; }
    assert(records == (std::vector{0, 0}));
  }
  {
    last_id = -1;
    records.clear();
    {
      auto value{pass_by_value(pass_by_value(create_resource()))};
      take_by_value(value);
    }
    assert(records == (std::vector{0, 1, -1, 0}));
  }
  {
    last_id = -1;
    records.clear();
    {
      auto value{pass_by_value(pass_by_value(create_resource()))};
      take_by_value(std::move(value));
    }
    assert(records == (std::vector{0, 0}));
  }
  {
    last_id = -1;
    records.clear();
    {
      auto value{pass_by_value(pass_by_value(create_resource()))};
      take_by_clr(value);
      take_by_r(value);
    }
    assert(records == (std::vector{0, 0}));
  }
}

int main() {
  {
    static int last_id{-1};
    static std::vector<int> records;
    struct Resource {
      int id;
      Resource() : id{++last_id} { records.push_back(id); }
      Resource(const Resource &) : Resource() {}
      Resource &operator=(const Resource &) = delete;
      Resource(Resource &&rhs) noexcept : id{rhs.id} { rhs.id = -1; }
      Resource &operator=(Resource &&) = delete;
      ~Resource() {
        if (id != -1) records.push_back(-id);
      }
    };
    {
      last_id = -1;
      records.clear();
      std::thread{[](const Resource &) {}, Resource{}}.join();
      assert(records == (std::vector{0, 0}));
    }
    {
      last_id = -1;
      records.clear();
      {
        Resource resource;
        std::thread{[](const Resource &) {}, resource}.join();
      }
      assert(records == (std::vector{0, 1, -1, 0}));
    }
  }
  {
    bool modified{false};
    std::thread{[](bool &modified) { modified = true; }, std::ref(modified)}
        .join();
    assert(modified);
  }
  {
    static bool modified{false};
    struct Proxy {
#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-convert-member-functions-to-static"
      void modify() { modified = true; }
#pragma clang diagnostic pop
      static void static_reset() { modified = false; }
    };
    std::thread(&Proxy::modify, static_cast<Proxy *>(nullptr)).join();
    assert(modified);
    std::thread{&Proxy::static_reset}.join();
    assert(!modified);
  }
  {
    static std::unique_ptr<bool> static_modified{nullptr};
    auto instance{std::make_unique<bool>(false)};
    std::thread{[](std::unique_ptr<bool> &&modified) {
                  *modified = true;
                  static_modified = std::move(modified);
                },
                std::move(instance)}
        .join();
    assert(*static_modified);
    assert(!instance);
  }
  {
    bool modified{false};
    auto create_modifier_thread{
        [&modified] { return std::thread{[&modified] { modified = true; }}; }};
    auto join_modifier_thread{[](std::thread thread) { thread.join(); }};
    join_modifier_thread(create_modifier_thread());
    assert(modified);
  }
  test_copy_elision();
}
