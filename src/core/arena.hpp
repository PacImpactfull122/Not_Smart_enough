#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <vector>

namespace opencode {

class Arena {
 public:
  explicit Arena(std::size_t block_size = 64 * 1024) : block_size_(block_size) {}

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

    std::size_t bytes_used() const noexcept {
      std::size_t total = 0;
      for (const auto& block : blocks_) total += block.used;
      return total;
    }

   private:
    struct Block {
      std::vector<std::byte> data;
      std::size_t used = 0;
    };

    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
      if (bytes == 0) bytes = 1;
      if (blocks_.empty()) add_block(bytes);

      for (;;) {
        auto& block = blocks_.back();
        auto addr = reinterpret_cast<std::uintptr_t>(block.data.data() + block.used);
        auto pad = (alignment - addr % alignment) % alignment;
        std::size_t offset = block.used + pad;
        if (offset <= block.data.size() && bytes <= block.data.size() - offset) {
          block.used = offset + bytes;
          return block.data.data() + offset;
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
      blocks_.push_back(Block{std::vector<std::byte>(size), 0});
    }

    std::vector<Block> blocks_;
    std::size_t block_size_;
  };

  Resource resource_;
  std::size_t block_size_;
};

}  // namespace opencode
