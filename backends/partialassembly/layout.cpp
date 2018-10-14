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
#if defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_PA)

#include "layout.hpp"
#include "../../general/array.hpp"

namespace mfem
{

namespace pa
{

void Layout::Resize(std::size_t new_size)
{
   size = new_size;
}

void Layout::Resize(const Array<std::size_t> &offsets)
{
   MFEM_ASSERT(offsets.Size() == 2,
               "multiple workers are not supported yet");
   size = offsets.Last();
}

} // namespace mfem::pa

} // namespace mfem

#endif // defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_PA)
