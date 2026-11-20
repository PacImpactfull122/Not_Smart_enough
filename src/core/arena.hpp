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

    std::size_t bytes_used() const noexcept { return total_used_; }

   private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
      if (bytes == 0) bytes = 1;
      if (blocks_.empty()) add_block(bytes, alignment);

      for (;;) {
        void* start = blocks_.back().get() + used_;
        void* cursor = start;
        std::size_t space = block_size_ + alignment_margin_ - used_;
        void* aligned = std::align(alignment, bytes, cursor, space);
        if (aligned != nullptr) {
          std::size_t padding = static_cast<std::byte*>(aligned) -
                                static_cast<std::byte*>(start);
          std::size_t consumed = padding + bytes;
          used_ += consumed;
          total_used_ += consumed;
          return aligned;
        }
        add_block(bytes, alignment);
      }
    }

    void do_deallocate(void*, std::size_t, std::size_t) override {}

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
      return this == &other;
    }

    void add_block(std::size_t bytes, std::size_t alignment) {
      std::size_t size = block_size_ > bytes ? block_size_ : bytes;
      if (alignment > alignof(std::max_align_t)) size += alignment;
      blocks_.push_back(std::make_unique<std::byte[]>(size));
      used_ = 0;
    }

    std::vector<std::unique_ptr<std::byte[]>> blocks_;
    std::size_t block_size_;
    std::size_t used_ = 0;
    std::size_t total_used_ = 0;
  };

  Resource resource_;
};

}  // namespace opencode
