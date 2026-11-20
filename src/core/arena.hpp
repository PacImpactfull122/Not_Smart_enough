#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <vector>

namespace opencode {

class Arena {
 public:
  explicit Arena(std::size_t block_size = 64 * 1024) : resource_(block_size) {
    if (block_size == 0) throw std::invalid_argument("arena block size must be positive");
  }

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  std::pmr::memory_resource* resource() noexcept { return &resource_; }

  std::size_t blocks() const noexcept { return resource_.blocks(); }

  std::size_t bytes_used() const noexcept { return resource_.bytes_used(); }

 private:
  class Resource final : public std::pmr::memory_resource {
   public:
    explicit Resource(std::size_t block_size) : block_size_(block_size) {}

    std::size_t blocks() const noexcept { return blocks_.size(); }

    std::size_t bytes_used() const noexcept { return used_; }

   private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
      if (bytes == 0) bytes = 1;
      if (blocks_.empty()) add_block(bytes);

      for (;;) {
        auto addr = reinterpret_cast<std::uintptr_t>(blocks_.back().get() + used_);
        auto pad = (alignment - addr % alignment) % alignment;
        std::size_t offset = used_ + pad;
        if (offset <= block_size_ && bytes <= block_size_ - offset) {
          used_ = offset + bytes;
          return blocks_.back().get() + offset;
        }
        add_block(bytes);
      }
    }

    void do_deallocate(void*, std::size_t, std::size_t) override {}

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
      return this == &other;
    }

    void add_block(std::size_t bytes) {
      std::size_t size = block_size_ > bytes ? block_size_ : bytes;
      blocks_.push_back(std::make_unique<std::byte[]>(size));
      used_ = 0;
    }

    std::vector<std::unique_ptr<std::byte[]>> blocks_;
    std::size_t block_size_;
    std::size_t used_ = 0;
  };

  Resource resource_;
};

}  // namespace opencode
