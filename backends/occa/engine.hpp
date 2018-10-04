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

#ifndef MFEM_BACKENDS_OCCA_ENGINE_HPP
#define MFEM_BACKENDS_OCCA_ENGINE_HPP

#include "../../config/config.hpp"
#if defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_OCCA)

#include "../base/backend.hpp"
#include <occa.hpp>

namespace mfem
{

namespace occa
{

class Engine : public mfem::Engine
{
protected:
   //
   // Inherited fields
   //
   // mfem::Backend *backend;
#ifdef MFEM_USE_MPI
   // MPI_Comm comm;
#endif
   // int num_mem_res;
   // int num_workers;
   // MemoryResource **memory_resources;
   // double *workers_weights;
   // int *workers_mem_res;

   static bool fileOpenerRegistered;
   /// An array of OCCA devices. Currently only a single device is supported.
   ::occa::device *device;
   std::string okl_path;
   bool force_cuda_aware_mpi;

   void Init(const std::string &engine_spec);

public:
   /// TODO: doxygen
   Engine(const std::string &engine_spec);

#ifdef MFEM_USE_MPI
   /// TODO: doxygen
   Engine(MPI_Comm comm, const std::string &engine_spec);
#endif

   /// TODO: doxygen
   virtual ~Engine() { delete [] device; }

   /**
       @name OCCA specific interface, used by other objects in the OCCA backend
    */
   ///@{

   /// Get the associated OCCA device.
   ::occa::device GetDevice(int idx = 0) const { return device[idx]; }

   /// TODO: doxygen
   const std::string &GetOklPath() const { return okl_path; }

   /// OCCA device memory allocation.
   ::occa::memory Alloc(std::size_t bytes) const
   { return GetDevice().malloc(bytes); }

   /// Two mfem::occa::Engine%s are equal if they use the same OCCA device.
   bool operator==(const Engine &other) const
   { return GetDevice() == other.GetDevice(); }

   /// TODO: doxygen
   bool CheckEngine(const mfem::Engine *e) const;

   /// TODO: doxygen
   bool CheckLayout(const PLayout *layout) const;

   /// TODO: doxygen
   bool CheckArray(const PArray *array) const;

   /// TODO: doxygen
   bool CheckVector(const PVector *vector) const;

   /// TODO: doxygen
   bool CheckFESpace(const PFiniteElementSpace *fes) const;

#ifdef MFEM_USE_MPI
   void SetForceCudaAwareMPI(bool force = true)
   { force_cuda_aware_mpi = force; }

   bool GetForceCudaAwareMPI() const { return force_cuda_aware_mpi; }
#endif

   ///@}
   // End: OCCA specific interface

   /**
       @name Virtual interface: finite element data structures and algorithms
    */
   ///@{

   virtual DLayout MakeLayout(std::size_t size) const;
   virtual DLayout MakeLayout(const mfem::Array<std::size_t> &offsets) const;

   virtual DArray MakeArray(PLayout &layout, std::size_t item_size) const;

   virtual DVector MakeVector(PLayout &layout,
                              int type_id = ScalarId<double>::value) const;

   virtual DFiniteElementSpace MakeFESpace(mfem::FiniteElementSpace &
                                           fespace) const;

   virtual DBilinearForm MakeBilinearForm(mfem::BilinearForm &bf) const;

   /// FIXME - What will the actual parameters be?
   virtual void AssembleLinearForm(LinearForm &l_form) const;

   /// FIXME - What will the actual parameters be?
   virtual mfem::Operator *MakeOperator(const MixedBilinearForm &mbl_form) const;

   /// FIXME - What will the actual parameters be?
   virtual mfem::Operator *MakeOperator(const NonlinearForm &nl_form) const;

   ///@}
   // End: Virtual interface
};

} // namespace mfem::occa

} // namespace mfem

#endif // defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_OCCA)

#endif // MFEM_BACKENDS_OCCA_ENGINE_HPP
