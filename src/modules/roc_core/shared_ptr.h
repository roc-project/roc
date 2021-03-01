/*
 * Copyright (c) 2015 Roc authors
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

//! @file roc_core/shared_ptr.h
//! @brief Shared ownership intrusive pointer.

#ifndef ROC_CORE_SHARED_PTR_H_
#define ROC_CORE_SHARED_PTR_H_

#include "roc_core/ownership.h"
#include "roc_core/panic.h"
#include "roc_core/stddefs.h"

namespace roc {
namespace core {

//! Shared ownership intrusive pointer.
//!
//! @tparam T defines pointee type. It may be const.
//! @tparam Ownership defines methods to increase and decrease the reference counter
//! embedded into object. If RefCntOwnership is used, T should inherit RefCnt.
template <class T, template <class TT> class Ownership = RefCntOwnership>
class SharedPtr {
public:
    //! Create empty shared pointer.
    //! @remarks
    //!  This overload is a bit faster than SharedPtr(NULL).
    SharedPtr()
        : ptr_(NULL) {
    }

    //! Create shared pointer from raw pointer.
    //! @remarks
    //!  This overload hits SharedPtr(NULL) and SharedPtr(T).
    SharedPtr(T* ptr)
        : ptr_(ptr) {
        acquire_();
    }

    //! Create shared pointer from shared pointer of the same type.
    //! @remarks
    //!  This is a copy constructor.
    SharedPtr(const SharedPtr& other)
        : ptr_(other.ptr_) {
        acquire_();
    }

    //! Create shared pointer from shared pointer of another type.
    //! @remarks
    //!  - This overload hits SharedPtr(SharedPtr<ConvertibleToT>).
    //!  - This doesn't work as a copy constructor since it's template.
    template <class TT>
    SharedPtr(const SharedPtr<TT, Ownership>& other)
        : ptr_(other.get()) {
        acquire_();
    }

    //! Destroy shared pointer.
    ~SharedPtr() {
        release_();
    }

    //! Reset shared pointer and attach it to another object.
    SharedPtr& operator=(const SharedPtr& other) {
        reset(other.ptr_);
        return *this;
    }

    //! Reset shared pointer and attach it to another object.
    void reset(T* ptr = NULL) {
        if (ptr != ptr_) {
            release_();
            ptr_ = ptr;
            acquire_();
        }
    }

    //! Get underlying pointer.
    T* get() const {
        return ptr_;
    }

    //! Get underlying pointer.
    T* operator->() const {
        if (ptr_ == NULL) {
            roc_panic("shared ptr: attempting to dereference null shared pointer");
        }
        return ptr_;
    }

    //! Get underlying reference.
    T& operator*() const {
        if (ptr_ == NULL) {
            roc_panic("shared ptr: attempting to dereference null shared pointer");
        }
        return *ptr_;
    }

    //! Convert to bool.
    operator const struct unspecified_bool *() const {
        return (const unspecified_bool*)ptr_;
    }

private:
    void acquire_() {
        if (ptr_ != NULL) {
            Ownership<T>::acquire(*ptr_);
        }
    }

    void release_() {
        if (ptr_ != NULL) {
            Ownership<T>::release(*ptr_);
        }
    }

    T* ptr_;
};

//! Equality check.
template <class T1, class T2>
inline bool operator==(const SharedPtr<T1>& a, const SharedPtr<T2>& b) {
    return (a.get() == b.get());
}

} // namespace core
} // namespace roc

#endif // ROC_CORE_SHARED_PTR_H_
