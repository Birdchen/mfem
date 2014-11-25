// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.googlecode.com.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.


#include "../general/array.hpp"
#include "operator.hpp"
#include "blockVector.hpp"
#include "blockOperator.hpp"

namespace mfem
{

BlockOperator::BlockOperator(const Array<int> & offsets):
   Operator(offsets.Last()),
   owns_block(0),
   nRowBlocks(offsets.Size() - 1),
   nColBlocks(offsets.Size() - 1),
   row_offsets(0),
   col_offsets(0),
   op(nRowBlocks, nRowBlocks)
{
   op = static_cast<Operator *>(NULL);
   row_offsets.MakeRef(offsets);
   col_offsets.MakeRef(offsets);
}


BlockOperator::BlockOperator(const Array<int> & row_offsets_, const Array<int> & col_offsets_):
   Operator(row_offsets_.Last()),
   owns_block(0),
   nRowBlocks(row_offsets_.Size()-1),
   nColBlocks(col_offsets_.Size()-1),
   row_offsets(0),
   col_offsets(0),
   op(nRowBlocks, nColBlocks)
{
   op = static_cast<Operator *>(NULL);
   row_offsets.MakeRef(row_offsets_);
   col_offsets.MakeRef(col_offsets_);
}

void BlockOperator::SetDiagonalBlock(int iblock, Operator *op)
{
   SetBlock(iblock, iblock, op);
}


void BlockOperator::SetBlock(int iRow, int iCol, Operator *opt)
{
   op(iRow, iCol) = opt;

   if( row_offsets[iRow+1] - row_offsets[iRow] != opt->Size() )
      mfem_error("BlockOperator::SetBlock Incompatible Row Size\n");

//  Since Operator does not have the method Width we trust that the width is correct.
//      if( col_offsets[iCol+1]-row_offsets[iCol] != opt->Width() )
//              mfem_error("BlockOperator::SetBlock Incompatible Col Size\n");
}

// Operator application
void BlockOperator::Mult (const Vector & x, Vector & y) const
{
   yblock.Update(y.GetData(),row_offsets);
   xblock.Update(x.GetData(),col_offsets);

   y = 0.0;
   for (int iRow(0); iRow < nRowBlocks; ++iRow)
   {
      tmp.SetSize(row_offsets[iRow+1] - row_offsets[iRow]);
      for(int jCol(0); jCol < nColBlocks; ++jCol)
      {
         if(op(iRow,jCol))
         {
            op(iRow,jCol)->Mult(xblock.GetBlock(jCol), tmp);
            yblock.GetBlock(iRow) += tmp;
         }
      }
   }
}

// Action of the transpose operator
void BlockOperator::MultTranspose (const Vector & x, Vector & y) const
{
   y = 0.0;

   xblock.Update(x.GetData(),row_offsets);
   yblock.Update(y.GetData(),col_offsets);

   for (int iRow(0); iRow < nRowBlocks; ++iRow)
   {
      tmp.SetSize(row_offsets[iRow+1] - row_offsets[iRow]);
      for(int jCol(0); jCol < nColBlocks; ++jCol)
      {
         if(op(jCol,iRow))
         {
            op(jCol,iRow)->MultTranspose(xblock.GetBlock(iRow), tmp);
            yblock.GetBlock(jCol) += tmp;
         }
      }
   }

}

BlockOperator::~BlockOperator()
{
   if(owns_block)
      for (int iRow(0); iRow < nRowBlocks; ++iRow)
         for(int jCol(0); jCol < nColBlocks; ++jCol)
            delete op(jCol,iRow);
}

//-----------------------------------------------------------------------
BlockDiagonalPreconditioner::BlockDiagonalPreconditioner(const Array<int> & offsets_):
   Solver(offsets_.Last()),
   owns_block(0),
   nBlocks(offsets_.Size() - 1),
   offsets(0),
   op(nBlocks)

{
   op = static_cast<Operator *>(NULL);
   offsets.MakeRef(offsets_);
}

void BlockDiagonalPreconditioner::SetDiagonalBlock(int iblock, Operator *opt)
{
   if( offsets[iblock+1] - offsets[iblock] != opt->Size() )
      mfem_error("offsets[iblock+1] - offsets[iblock] != size");

   op[iblock] = opt;
}

// Operator application
void BlockDiagonalPreconditioner::Mult (const Vector & x, Vector & y) const
{

   y = 0.0;

   yblock.Update(y.GetData(), offsets);
   xblock.Update(x.GetData(), offsets);

   for(int i(0); i<nBlocks; ++i)
      if(op[i])
         op[i]->Mult(xblock.GetBlock(i), yblock.GetBlock(i));
      else
         yblock.GetBlock(i) = xblock.GetBlock(i);

}

// Action of the transpose operator
void BlockDiagonalPreconditioner::MultTranspose (const Vector & x, Vector & y) const
{

   y = 0.0;

   yblock.Update(y.GetData(), offsets);
   xblock.Update(x.GetData(), offsets);

   for(int i(0); i<nBlocks; ++i)
      if(op[i])
         (op[i])->MultTranspose(xblock.GetBlock(i), yblock.GetBlock(i));
      else
         yblock.GetBlock(i) = xblock.GetBlock(i);
}

BlockDiagonalPreconditioner::~BlockDiagonalPreconditioner()
{
   if(owns_block)
      for(int i(0); i<nBlocks; ++i)
         delete op[i];
}

}
