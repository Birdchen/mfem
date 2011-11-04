# Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at the
# Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights reserved.
# See file COPYRIGHT for details.
#
# This file is part of the MFEM library. For more information and source code
# availability see http://mfem.googlecode.com.
#
# MFEM is free software; you can redistribute it and/or modify it under the
# terms of the GNU Lesser General Public License (as published by the Free
# Software Foundation) version 2.1 dated February 1999.

# Serial compiler
CC         = g++
CCOPTS     =
DEBUG_OPTS = -g -DMFEM_DEBUG
OPTIM_OPTS = -O3
DEPCC      = $(CC)

# Parallel compiler
MPICC      = mpiCC
MPIOPTS    = $(CCOPTS) -I$(HYPRE_DIR)/include

# The HYPRE library (needed to build the parallel version)
HYPRE_DIR  = ../../hypre-2.8.0b-pre/src/hypre

# Internal mfem options
USE_LAPACK   = YES
USE_MEMALLOC = YES

USE_LAPACK_NO  =
USE_LAPACK_YES = -DMFEM_USE_LAPACK
USE_LAPACK_DEF = $(USE_LAPACK_$(USE_LAPACK))

USE_MEMALLOC_NO  =
USE_MEMALLOC_YES = -DMFEM_USE_MEMALLOC
USE_MEMALLOC_DEF = $(USE_MEMALLOC_$(USE_MEMALLOC))

DEFS = $(USE_LAPACK_DEF) $(USE_MEMALLOC_DEF)

CCC = $(CC) $(MODE_OPTS) $(DEFS) $(CCOPTS)

# Compiler and options used for generating deps.mk
DEPCCC = $(DEPCC) $(CCOPTS) $(MODE_OPTS) $(DEFS)

DEFINES = $(subst -D,,$(filter -D%,$(CCC)))

# Source dirs in logical order
DIRS = general linalg mesh fem
SOURCE_FILES = $(foreach dir,$(DIRS),$(wildcard $(dir)/*.cpp))
OBJECT_FILES = $(SOURCE_FILES:.cpp=.o)

.SUFFIXES: .cpp .o
.cpp.o:
	cd $(<D); $(CCC) -c $(<F)

# Serial build
serial: opt

# Parallel build
parallel pdebug: CCC=$(MPICC) $(MODE_OPTS) $(DEFS) -DMFEM_USE_MPI $(MPIOPTS)
parallel: opt
pdebug: debug

lib: libmfem.a mfem_defs.hpp

debug deps_debug: MODE_OPTS = $(DEBUG_OPTS)
debug: lib

opt deps_opt: MODE_OPTS = $(OPTIM_OPTS)
opt: lib

-include deps.mk

libmfem.a: $(OBJECT_FILES)
	ar cruv libmfem.a $(OBJECT_FILES)
	ranlib libmfem.a

mfem_defs.hpp:
	@echo "Generating 'mfem_defs.hpp' ..."
	@echo "// Auto-generated file." > mfem_defs.hpp
	for i in $(DEFINES); do \
		echo "#define "$${i} >> mfem_defs.hpp; done

deps deps_debug deps_opt:
	rm -f deps.mk
	for i in $(SOURCE_FILES:.cpp=); do \
		$(DEPCCC) -MM -MT $${i}.o $${i}.cpp >> deps.mk; done

clean:
	rm -f */*.o */*~ *~ libmfem.a mfem_defs.hpp deps.mk
	cd examples; make clean
