/**
 * \file libconcur/queue.h
 * \author mutouyun (orz@orzz.org)
 * \brief Define concurrent queue.
 * \date 2022-11-19
 */
#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>

#include "libimp/construct.h"
#include "libimp/detect_plat.h"
#include "libimp/aligned.h"

#include "libpmr/allocator.h"
#include "libpmr/memory_resource.h"

#include "libconcur/def.h"
#include "libconcur/concurrent.h"

LIBCONCUR_NAMESPACE_BEG_

template <typename T, typename PRelationT = relation::multi
                    , typename CRelationT = relation::multi>
class queue {
public:
  using producer_relation_t = PRelationT;
  using consumer_relation_t = CRelationT;
  using model_type = prod_cons<trans::unicast, producer_relation_t, consumer_relation_t>;
  using value_type = T;
  using size_type  = std::int64_t;

private:
  struct data {
    model_type model_;
    typename concur::traits<model_type>::header header_;
    ::LIBIMP::aligned<element<value_type>> elements_start_;

    template <typename U>
    data(U &&model) noexcept
      : header_(std::forward<U>(model)) {
      auto elements = this->elements();
      typename decltype(elements)::size_type i = 0;
      LIBIMP_TRY {
        for (; i < elements.size(); ++i) {
          (void)::LIBIMP::construct<element<value_type>>(&elements[i]);
        }
      } LIBIMP_CATCH(...) {
        for (decltype(i) k = 0; k < i; ++k) {
          (void)::LIBIMP::destroy<element<value_type>>(&elements[k]);
        }
      }
    }

    ~data() noexcept {
      for (auto &elem : this->elements()) {
        (void)::LIBIMP::destroy<element<value_type>>(&elem);
      }
    }

    static std::size_t size_of(index_t circ_size) noexcept {
      return sizeof(struct data) + ( (circ_size - 1) * sizeof(element<value_type>) );
    }

    std::size_t byte_size() const noexcept {
      return size_of(header_.circ_size);
    }

    /// \brief element<value_type> elements[0];
    ::LIBIMP::span<element<value_type>> elements() noexcept {
      return {elements_start_.ptr(), header_.circ_size};
    }
  };

  data *init(index_t circ_size) noexcept {
    if (!data_allocator_) {
      return nullptr;
    }
    LIBIMP_TRY {
      auto data_ptr = data_allocator_.alloc(data::size_of(circ_size));
      if (data_ptr == nullptr) {
        return nullptr;
      }
      return ::LIBIMP::construct<data>(data_ptr, circ_size);
    } LIBIMP_CATCH(...) {
      return nullptr;
    }
  }

  ::LIBPMR::allocator data_allocator_;
  std::atomic<size_type> size_;
  data *data_;
  typename concur::traits<model_type>::context context_;

public:
  queue(queue const &) = delete;
  queue(queue &&) = delete;
  queue &operator=(queue const &) = delete;
  queue &operator=(queue &&) = delete;

  ~queue() noexcept {
    if (valid()) {
      auto sz = data_->byte_size();
      (void)::LIBIMP::destroy<data>(data_);
      data_allocator_.dealloc(data_, sz);
    }
  }

  template <typename MR, ::LIBPMR::verify_memory_resource<MR> = true>
  explicit queue(index_t circ_size, MR *memory_resource) noexcept
    : data_allocator_(memory_resource)
    , data_(init(circ_size)) {}

  template <typename MR, ::LIBPMR::verify_memory_resource<MR> = true>
  explicit queue(MR *memory_resource) noexcept
    : queue(default_circle_buffer_size, memory_resource) {}

  explicit queue(index_t circ_size) noexcept
    : queue(circ_size, ::LIBPMR::new_delete_resource::get()) {}

  queue() noexcept
    : queue(default_circle_buffer_size) {}

  bool valid() const noexcept {
    return (data_ != nullptr) && data_allocator_.valid();
  }

  explicit operator bool() const noexcept {
    return valid();
  }

  size_type approx_size() const noexcept {
    return size_.load(std::memory_order_relaxed);
  }

  bool empty() const noexcept {
    return !valid() || (approx_size() == 0);
  }

  template <typename U>
  bool push(U &&value) noexcept {
    if (!valid()) return false;
    if (!data_->model_.enqueue(data_->elements(), data_->header_, context_, std::forward<U>(value))) {
      return false;
    }
    size_.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  bool pop(value_type &value) noexcept {
    if (!valid()) return false;
    if (!data_->model_.dequeue(data_->elements(), data_->header_, context_, value)) {
      return false;
    }
    size_.fetch_sub(1, std::memory_order_relaxed);
    return true;
  }
};

LIBCONCUR_NAMESPACE_END_
