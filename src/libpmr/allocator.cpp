
#include <algorithm>  // std::swap

#include "libpmr/allocator.h"

LIBPMR_NAMESPACE_BEG_

allocator::holder_base &allocator::get_holder() noexcept {
  return *::LIBIMP_::byte_cast<holder_base>(holder_.data());
}

allocator::holder_base const &allocator::get_holder() const noexcept {
  return *::LIBIMP_::byte_cast<holder_base const>(holder_.data());
}

allocator::allocator() noexcept {
  ::LIBIMP_::construct<holder_null>(holder_.data());
}

allocator::~allocator() noexcept {
  ::LIBIMP_::destroy(&get_holder());
}

allocator::allocator(allocator &&other) noexcept
  : allocator(other) /*copy*/ {
  ::LIBIMP_::construct<holder_null>(other.holder_.data());
}

allocator &allocator::operator=(allocator &&other) & noexcept {
  if (this == &other) return *this;
  this->holder_ = other.holder_;
  ::LIBIMP_::construct<holder_null>(other.holder_.data());
  return *this;
}

void allocator::swap(allocator &other) noexcept {
  std::swap(this->holder_, other.holder_);
}

bool allocator::valid() const noexcept {
  return get_holder().valid();
}

allocator::operator bool() const noexcept {
  return valid();
}

void *allocator::alloc(std::size_t s) {
  if (!valid()) return nullptr;
  return get_holder().alloc(s);
}

void allocator::free(void *p, std::size_t s) {
  if (!valid()) return;
  get_holder().free(p, s);
}

LIBPMR_NAMESPACE_END_