// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#include "../../config/config.hpp"
#if defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_OCCA)

#include "array.hpp"

namespace mfem
{

namespace occa
{

PArray *Array::DoClone(bool copy_data, void **buffer,
                       std::size_t item_size) const
{
   Array *new_array = new Array(OccaLayout(), item_size);
   if (copy_data)
   {
      new_array->slice.copyFrom(slice);
   }
   if (buffer)
   {
      *buffer = new_array->GetBuffer();
   }
   return new_array;
}

int Array::DoResize(PLayout &new_layout, void **buffer,
                    std::size_t item_size)
{
   MFEM_ASSERT(dynamic_cast<Layout *>(&new_layout) != NULL,
               "new_layout is not an OCCA Layout");
   Layout *lt = static_cast<Layout *>(&new_layout);
   int err = OccaResize(lt, item_size);
   if (!err && buffer)
   {
      *buffer = GetBuffer();
   }
   return err;
}

void *Array::DoPullData(void *buffer, std::size_t item_size)
{
   // called only when Size() != 0

   if (!slice.getDevice().hasSeparateMemorySpace())
   {
      return slice.ptr();
   }
   if (buffer)
   {
      slice.copyTo(buffer);
   }
   return buffer;
}

void Array::DoFill(const void *value_ptr, std::size_t item_size)
{
   // called only when Size() != 0

   switch (item_size)
   {
      case sizeof(int8_t):
         OccaFill(*(const int8_t *)value_ptr);
         break;
      case sizeof(int16_t):
         OccaFill(*(const int16_t *)value_ptr);
         break;
      case sizeof(int32_t):
         OccaFill(*(const int32_t *)value_ptr);
         break;
      // case sizeof(int64_t):
      //    OccaFill(*(const int64_t *)value_ptr);
      //    break;
      case sizeof(double):
         OccaFill(*(const double *)value_ptr);
         break;
      // case sizeof(::occa::double2):
      //    OccaFill(*(const ::occa::double2 *)value_ptr);
      //    break;
      default:
         MFEM_ABORT("item_size = " << item_size << " is not supported");
   }
}

void Array::DoPushData(const void *src_buffer, std::size_t item_size)
{
   // called only when Size() != 0

   if (slice.getDevice().hasSeparateMemorySpace() || slice.ptr() != src_buffer)
   {
      slice.copyFrom(src_buffer);
   }
}

void Array::DoAssign(const PArray &src, std::size_t item_size)
{
   // called only when Size() != 0

   // Note: static_cast can not be used here since PArray is a virtual base
   //       class.
   const Array *source = dynamic_cast<const Array *>(&src);
   MFEM_ASSERT(source != NULL, "invalid source Array type");
   OccaAssign(*source);
}

} // namespace mfem::occa

} // namespace mfem

#endif // defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_OCCA)
