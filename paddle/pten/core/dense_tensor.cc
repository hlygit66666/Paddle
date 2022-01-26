/* Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/pten/core/dense_tensor.h"

// See Note [ Why still include the fluid headers? ]
#include "paddle/pten/common/bfloat16.h"
#include "paddle/pten/common/complex.h"
#include "paddle/pten/common/float16.h"

#include "paddle/pten/api/lib/utils/storage.h"
#include "paddle/pten/core/convert_utils.h"

namespace pten {

DenseTensor::DenseTensor(Allocator* a, const DenseTensorMeta& meta)
    : meta_(meta), holder_(a->Allocate(SizeOf(dtype()) * numel())) {}

DenseTensor::DenseTensor(Allocator* a, DenseTensorMeta&& meta)
    : meta_(std::move(meta)), holder_(a->Allocate(SizeOf(dtype()) * numel())) {}

DenseTensor::DenseTensor(const std::shared_ptr<pten::Allocation>& holder,
                         const DenseTensorMeta& meta)
    : meta_(meta), holder_(holder) {}

DenseTensor::DenseTensor(const DenseTensor& other) : meta_(other.meta()) {
  holder_ = other.holder_;

#ifdef PADDLE_WITH_MKLDNN
  format_ = other.format_;
#endif
}

DenseTensor& DenseTensor::operator=(const DenseTensor& other) {
  meta_ = other.meta();
  holder_ = other.holder_;
#ifdef PADDLE_WITH_MKLDNN
  format_ = other.format_;
#endif
  return *this;
}

DenseTensor& DenseTensor::operator=(DenseTensor&& other) {
  meta_ = std::move(other.meta_);
  std::swap(holder_, other.holder_);
  return *this;
}

int64_t DenseTensor::numel() const {
  if (meta_.is_scalar) {
    return 1;
  }
  return product(meta_.dims);
}

bool DenseTensor::IsSharedWith(const DenseTensor& b) const {
  return holder_ && holder_ == b.Holder();
}

template <typename T>
const T* DenseTensor::data() const {
  check_memory_size();
  PADDLE_ENFORCE(
      (dtype() == paddle::experimental::CppTypeToDataType<T>::Type()),
      paddle::platform::errors::InvalidArgument(
          "The type of data we are trying to retrieve does not match the "
          "type of data currently contained in the container."));
  return static_cast<const T*>(data());
}

template <typename T>
T* DenseTensor::data() {
  check_memory_size();
  PADDLE_ENFORCE(
      (dtype() == paddle::experimental::CppTypeToDataType<T>::Type()),
      paddle::platform::errors::InvalidArgument(
          "The type of data we are trying to retrieve does not match the "
          "type of data currently contained in the container."));
  return static_cast<T*>(data());
}

void* DenseTensor::data() {
  check_memory_size();
  PADDLE_ENFORCE_NOT_NULL(
      holder_,
      paddle::platform::errors::PreconditionNotMet(
          "The storage must be valid when call the data function."));
  return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(holder_->ptr()) +
                                 meta_.offset);
}

const void* DenseTensor::data() const {
  check_memory_size();
  PADDLE_ENFORCE_NOT_NULL(
      holder_,
      paddle::platform::errors::PreconditionNotMet(
          "The storage must be valid when call the data function."));
  return reinterpret_cast<const void*>(
      reinterpret_cast<uintptr_t>(holder_->ptr()) + meta_.offset);
}

void DenseTensor::set_meta(DenseTensorMeta&& meta) {
  PADDLE_ENFORCE(!meta_.valid(),
                 paddle::platform::errors::InvalidArgument(
                     "Only when the original attribute of Tensor is "
                     "incomplete, can it be reset."));
  meta_ = std::move(meta);
}

void DenseTensor::set_meta(const DenseTensorMeta& meta) {
  PADDLE_ENFORCE(
      meta.valid(),
      paddle::platform::errors::InvalidArgument(
          "Input meta is invalid, please check the meta attribute."));
  meta_.dims = meta.dims;
  meta_.dtype = meta.dtype;
  meta_.is_scalar = meta.is_scalar;
  meta_.layout = meta.layout;
  meta_.lod = meta.lod;
  meta_.offset = meta.offset;
}

/* @jim19930609: This interface will be further modified util we finalized the
   design for Allocator - Allocation
   For now, we have to temporarily accommodate two independent use cases:
   1. Designed behaviour: DenseTensor constructed with its underlying storage_
   initialized
   2. Legacy behaviour(fluid): DenseTensor constructed using default
   constructor, where
                               storage_ won't be initialized until the first
   call to mutable_data(place)
   */
void DenseTensor::ResizeAndAllocate(const DDim& dims) {
  meta_.dims = dims;
  if (holder_ != nullptr && place().GetType() != AllocationType::UNDEFINED) {
    mutable_data(place());
  }
}

void DenseTensor::ResetLoD(const LoD& lod) { meta_.lod = lod; }

#define DATA_MEMBER_FUNC_INSTANTIATION(dtype)      \
  template const dtype* DenseTensor::data() const; \
  template dtype* DenseTensor::data();

DATA_MEMBER_FUNC_INSTANTIATION(bool);
DATA_MEMBER_FUNC_INSTANTIATION(int8_t);
DATA_MEMBER_FUNC_INSTANTIATION(uint8_t);
DATA_MEMBER_FUNC_INSTANTIATION(int16_t);
DATA_MEMBER_FUNC_INSTANTIATION(uint16_t);
DATA_MEMBER_FUNC_INSTANTIATION(int32_t);
DATA_MEMBER_FUNC_INSTANTIATION(uint32_t);
DATA_MEMBER_FUNC_INSTANTIATION(int64_t);
DATA_MEMBER_FUNC_INSTANTIATION(uint64_t);
DATA_MEMBER_FUNC_INSTANTIATION(::paddle::platform::bfloat16);
DATA_MEMBER_FUNC_INSTANTIATION(::paddle::platform::float16);
DATA_MEMBER_FUNC_INSTANTIATION(float);
DATA_MEMBER_FUNC_INSTANTIATION(double);
DATA_MEMBER_FUNC_INSTANTIATION(::paddle::experimental::complex64);
DATA_MEMBER_FUNC_INSTANTIATION(::paddle::experimental::complex128);

#undef DATA_MEMBER_FUNC_INSTANTIATION

}  // namespace pten
