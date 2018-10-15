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

#ifndef MFEM_BACKENDS_OCCA_VECTOR_HPP
#define MFEM_BACKENDS_OCCA_VECTOR_HPP

#include "../../config/config.hpp"
#if defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_OCCA)

#include <occa.hpp>
#include "../base/vector.hpp"
#include "array.hpp"

namespace mfem
{

namespace occa
{

// FIXME: Once XL fixes this code quirk we can remove this #ifdef switch
#ifdef __ibmxl__
class Vector : public Array, public PVector
#else
class Vector : virtual public Array, public PVector
#endif
{
protected:
   //
   // Inherited fields
   //
   // DLayout layout;

   /**
       @name Virtual interface
    */
   ///@{

   virtual PVector *DoVectorClone(bool copy_data, void **buffer,
                                  int buffer_type_id) const;

   virtual void DoDotProduct(const PVector &x, void *result,
                             int result_type_id) const;

   virtual void DoAxpby(const void *a, const PVector &x,
                        const void *b, const PVector &y,
                        int ab_type_id);

   ///@}
   // End: Virtual interface

public:
   Vector(const Engine &e)
      : PArray(*(new Layout(e, 0))), Array(e), PVector(*layout)
   { }

   Vector(Layout &lt)
      : PArray(lt), Array(lt, sizeof(double)), PVector(lt)
   { }

   mfem::Vector Wrap();

   const mfem::Vector Wrap() const;

#if defined(MFEM_USE_MPI)
   bool IsParallel() const { return (OccaEngine().GetComm() != MPI_COMM_NULL); }
#endif
};

} // namespace mfem::occa

} // namespace mfem

#endif // defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_OCCA)

#endif // MFEM_BACKENDS_OCCA_VECTOR_HPP
