#pragma once

#include <cstddef>
#include <memory_resource>

namespace opencode {

class Arena {
 public:
  explicit Arena(std::size_t block_size = 64 * 1024) : block_size_(block_size) {}

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  std::pmr::memory_resource* resource() noexcept { return &resource_; }

  std::size_t allocated() const noexcept { return resource_.allocated(); }

 private:
  class Resource final : public std::pmr::memory_resource {
   public:
    explicit Resource(std::size_t block_size) : block_size_(block_size) {}

    ~Resource() override { release(); }

    std::size_t allocated() const noexcept { return allocated_; }

   private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
      void* ptr = ::operator new(bytes, std::align_val_t(alignment));
      allocated_ += bytes;
      return ptr;
    }

    void do_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) override {
      ::operator delete(ptr, bytes, std::align_val_t(alignment));
      deallocated_ += bytes;
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
      return this == &other;
    }

    void release() noexcept {}

    std::size_t block_size_;
    std::size_t allocated_ = 0;
    std::size_t deallocated_ = 0;
  };

  Resource resource_;
  std::size_t block_size_;
};

}  // namespace opencode
