#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <vector>

namespace opencode {

class Arena {
 public:
  explicit Arena(std::size_t block_size = 64 * 1024) : resource_(block_size) {}

  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  std::pmr::memory_resource* resource() noexcept { return &resource_; }

  std::size_t blocks() const noexcept { return resource_.blocks(); }

  std::size_t bytes_used() const noexcept { return resource_.bytes_used(); }

 private:
  class Resource final : public std::pmr::memory_resource {
   public:
    explicit Resource(std::size_t block_size) : block_size_(block_size) {
      if (block_size == 0) throw std::invalid_argument("arena block size must be positive");
    }

    std::size_t blocks() const noexcept { return blocks_.size(); }

    std::size_t bytes_used() const noexcept { return total_used_; }

   private:
    struct Block {
      std::unique_ptr<std::byte[]> data;
      std::size_t size;
      std::size_t used = 0;
    };

    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
      if (bytes == 0) bytes = 1;
      if (blocks_.empty()) add_block(bytes, alignment);

      for (;;) {
        auto& block = blocks_.back();
        void* start = block.data.get() + block.used;
        void* cursor = start;
        std::size_t space = block.size - block.used;
        void* aligned = std::align(alignment, bytes, cursor, space);
        if (aligned != nullptr) {
          // std::align garante bytes <= space, logo padding + bytes <= block.size.
          std::size_t padding = static_cast<std::byte*>(aligned) -
                                static_cast<std::byte*>(start);
          std::size_t consumed = padding + bytes;
          if (consumed > std::numeric_limits<std::size_t>::max() - total_used_) {
            throw std::overflow_error("arena total usage overflow");
          }
          block.used += consumed;
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
      if (alignment > alignof(std::max_align_t)) {
        if (size > std::numeric_limits<std::size_t>::max() - alignment) {
          throw std::overflow_error("arena block size overflow");
        }
        size += alignment;
      }
      blocks_.push_back(Block{std::make_unique<std::byte[]>(size), size, 0});
    }

    std::vector<Block> blocks_;
    std::size_t block_size_;
    std::size_t total_used_ = 0;
  };

  Resource resource_;
};

}  // namespace opencode
