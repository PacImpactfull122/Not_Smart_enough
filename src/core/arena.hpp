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

  std::size_t cursor() const noexcept { return resource_.cursor(); }

 private:
  class Resource final : public std::pmr::memory_resource {
   public:
    explicit Resource(std::size_t block_size) : block_size_(block_size) {}

    std::size_t blocks() const noexcept { return blocks_.size(); }
    std::size_t cursor() const noexcept { return cursor_; }

   private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
      if (bytes == 0) bytes = 1;
      if (blocks_.empty()) add_block(bytes);

      void* ptr = aligned(blocks_.back().data(), cursor_, alignment);
      auto used = static_cast<std::size_t>(static_cast<std::byte*>(ptr) -
                                          blocks_.back().data());
      if (used + bytes > blocks_.back().size()) {
        add_block(bytes);
        ptr = aligned(blocks_.back().data(), 0, alignment);
        used = static_cast<std::size_t>(static_cast<std::byte*>(ptr) -
                                        blocks_.back().data());
      }

      cursor_ = used + bytes;
      return ptr;
    }

    void do_deallocate(void*, std::size_t, std::size_t) override {}

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
      return this == &other;
    }

    void add_block(std::size_t bytes) {
      std::size_t size = block_size_;
      if (bytes > size) size = bytes;
      blocks_.emplace_back(size);
      cursor_ = 0;
    }

    static void* aligned(std::byte* base, std::size_t offset, std::size_t alignment) {
      auto addr = reinterpret_cast<std::uintptr_t>(base + offset);
      auto pad = (alignment - addr % alignment) % alignment;
      return base + offset + pad;
    }

    std::vector<std::vector<std::byte>> blocks_;
    std::size_t block_size_;
    std::size_t cursor_ = 0;
  };

  Resource resource_;
  std::size_t block_size_;
};

}  // namespace opencode
