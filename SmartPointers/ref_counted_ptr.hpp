// ref_counted_ptr.hpp
//
// Minimal reference-counted smart pointer, illustrating the mechanics
// behind std::shared_ptr's ownership counting: a heap-allocated counter
// shared between all owners, decremented on destruction/reassignment, and
// the pointee freed only when the count reaches zero.
//
// This is a minimal approximation, not a real control block: std::shared_ptr
// colocates the object and its control block (strong count, weak count,
// deleter, allocator) in a single allocation when created via
// std::make_shared. This class always performs two separate allocations
// (one for T, one for the count), so it doesn't demonstrate that
// single-allocation optimization.

#ifndef REF_COUNTED_PTR_H_
#define REF_COUNTED_PTR_H_

#include <cstddef>
#include <utility>

namespace mem {

// A single-threaded reference-counted owning pointer with an
// externally allocated reference count.
//
// Not thread-safe: the counter is a plain std::size_t rather than an
// atomic, since the point here is to expose the ref-counting mechanics
// clearly rather than to reproduce std::shared_ptr's thread-safety
// guarantees. A production version would use std::atomic<std::size_t>
// for the counter, plus a real control block to support std::weak_ptr
// and enable_shared_from_this-style aliasing.
template <typename T>
class RefCountedPtr {
 public:
  RefCountedPtr() = default;

  // Note:
  // Guard against allocating a counter for a pointer that doesn't actually own anything:
  // In case the caller passes a nullptr, we don't want to allocate a counter for it. UseCount()
  // will return 0 for a nullptr, operator bool() returns false, Get() returns nullptr, Release()
  // in the destructor sees count_ == nullptr, skips the decrement/delete entirely, and does nothing
  explicit RefCountedPtr(T* ptr) : ptr_(ptr), count_(ptr ? new std::size_t(1) : nullptr) {}

  // COPY-CONSTRUCTOR
  // Copy operations share ownership: both instances point at the same
  // object and the same counter, so the count must be bumped rather than
  // duplicating the underlying resource.
  RefCountedPtr(const RefCountedPtr& other) : ptr_(other.ptr_), count_(other.count_) {
    AddRef();
  }

  // COPY-ASSIGNMENT OPERATOR
  RefCountedPtr& operator=(const RefCountedPtr& other) {
    // Guard against self-assignment before releasing our own reference,
    // otherwise `x = x` would drop the count to zero and free the object
    // out from under itself.
    if (this == &other) {
      return *this;
    }
    Release();
    ptr_ = other.ptr_;
    count_ = other.count_;
    AddRef();
    return *this;
  }

  // MOVE-CONSTRUCTOR
  // Move operations transfer ownership without touching the count: the
  // source gives up its reference entirely, so the total number of
  // owners is unchanged and no increment/decrement is needed. Each
  // std::exchange reads other's member and resets it to nullptr in one
  // expression (not an atomic operation - this class is single-threaded).
  RefCountedPtr(RefCountedPtr&& other) noexcept
      : ptr_(std::exchange(other.ptr_, nullptr)),
        count_(std::exchange(other.count_, nullptr)) {}

  // MOVE-ASSIGNMENT OPERATOR
  RefCountedPtr& operator=(RefCountedPtr&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    Release();
    ptr_ = std::exchange(other.ptr_, nullptr);
    count_ = std::exchange(other.count_, nullptr);
    return *this;
  }

  // Destructors are implicitly noexcept(true) by default in C++ (since C++11)
  ~RefCountedPtr() { Release(); }

  // Accessors
  T* Get() const noexcept { return ptr_; }
  T& operator*() const noexcept { return *ptr_; }
  T* operator->() const noexcept { return ptr_; }
  explicit operator bool() const noexcept { return ptr_ != nullptr; }

  // Exposed for demonstration/debugging only; a real API would not need
  // to leak the count to callers.
  std::size_t UseCount() const noexcept { return count_ ? *count_ : 0; }

 private:
  void AddRef() noexcept {
    if (count_) {
      ++(*count_);
    }
  }

  // Frees the pointee and the counter once the last owner releases it.
  // Left as a private helper (rather than inlined into the dtor/assign
  // operators) so copy-assign and move-assign can both reuse it without
  // duplicating the zero-check.
  void Release() noexcept{
    if (count_ && --(*count_) == 0) {
      delete ptr_;
      delete count_;
    }
    ptr_ = nullptr;
    count_ = nullptr;
  }

  T* ptr_ = nullptr;
  std::size_t* count_ = nullptr;
};

// Convenience factory analogous to std::make_shared: pairs construction
// with ownership and avoids exposing a raw pointer at the call site.
// Unlike std::make_shared, this performs two separate allocations
// (one for T, one for the count) rather than one combined allocation.
// Args&& + std::forward pattern:
// lets MakeRefCounted pass its arguments through to T's constructor without
// losing information about whether each argument was an lvalue or an rvalue.
// (temporary arguments remain rvalues, named variables remain lvalues)
template <typename T, typename... Args>
RefCountedPtr<T> MakeRefCounted(Args&&... args) {
  return RefCountedPtr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace mem

#endif  // REF_COUNTED_PTR_H_
