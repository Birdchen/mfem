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

// This file contains operator-based bilinear form integrators used
// with BilinearFormOperator.

#ifndef MFEM_TENSORFUNC
#define MFEM_TENSORFUNC

#include "dalg.hpp"
#include "fem.hpp"

namespace mfem
{

/**
* Gives the evaluation of the 1d basis functions and their derivative at one point @param x
*/
template <typename Tensor>
void ComputeBasis0d(const FiniteElement *fe, double x,
                     Tensor& shape0d, Tensor& dshape0d)
{
   const TensorBasisElement* tfe(dynamic_cast<const TensorBasisElement*>(fe));
   const Poly_1D::Basis &basis0d = tfe->GetBasis1D();

   const int quads0d = 1;
   const int dofs = fe->GetOrder() + 1;

   // We use Matrix and not Vector because we don't want shape0d and dshape0d to have
   // a different treatment than shape1d and dshape1d
   // shape0d  = Tensor(dofs, quads0d);
   // dshape0d = Tensor(dofs, quads0d);

   Vector u(dofs);
   Vector d(dofs);
   basis0d.Eval(x, u, d);
   for (int i = 0; i < dofs; i++)
   {
      shape0d(i, 0) = u(i);
      dshape0d(i, 0) = d(i);
   }
}

/**
* Gives the evaluation of the 1d basis functions and their derivative at all quadrature points
*/
template <typename Tensor>
void ComputeBasis1d(const FiniteElement *fe, int order, Tensor& shape1d,
                           Tensor& dshape1d, bool backward=false)
{
   const TensorBasisElement* tfe(dynamic_cast<const TensorBasisElement*>(fe));
   const Poly_1D::Basis &basis1d = tfe->GetBasis1D();
   const IntegrationRule &ir1d = IntRules.Get(Geometry::SEGMENT, order);

   const int quads1d = ir1d.GetNPoints();
   const int dofs = fe->GetOrder() + 1;

   // shape1d  = Tensor(dofs, quads1d);
   // dshape1d = Tensor(dofs, quads1d);

   Vector u(dofs);
   Vector d(dofs);
   for (int k = 0; k < quads1d; k++)
   {
      int ind = backward ? quads1d -1 - k : k;
      const IntegrationPoint &ip = ir1d.IntPoint(k);
      basis1d.Eval(ip.x, u, d);
      for (int i = 0; i < dofs; i++)
      {
         shape1d(i, ind) = u(i);
         dshape1d(i, ind) = d(i);
      }
   }
}

/**
* Gives the evaluation of the 1d basis functions at one point @param x
*/
template <typename Tensor>
void ComputeBasis0d(const FiniteElement *fe, double x, Tensor& shape0d)
{
   const TensorBasisElement* tfe(dynamic_cast<const TensorBasisElement*>(fe));
   const Poly_1D::Basis &basis0d = tfe->GetBasis1D();

   const int quads0d = 1;
   const int dofs = fe->GetOrder() + 1;

   // We use Matrix and not Vector because we don't want shape0d and dshape0d to have
   // a different treatment than shape1d and dshape1d
   // Well... that was before, we might want to reconsider this.
   // shape0d  = Tensor(dofs, quads0d);

   Vector u(dofs);
   Vector d(dofs);
   basis0d.Eval(x, u, d);
   for (int i = 0; i < dofs; i++)
   {
      shape0d(i, 0) = u(i);
   }
}

/**
* Gives the evaluation of the 1d basis functions at all quadrature points
*/
template <typename Tensor>
void ComputeBasis1d(const FiniteElement *fe, int order, Tensor& shape1d, bool backward=false)
{
   const TensorBasisElement* tfe(dynamic_cast<const TensorBasisElement*>(fe));
   const Poly_1D::Basis &basis1d = tfe->GetBasis1D();
   const IntegrationRule &ir1d = IntRules.Get(Geometry::SEGMENT, order);

   const int quads1d = ir1d.GetNPoints();
   const int dofs = fe->GetOrder() + 1;

   // shape1d  = Tensor(dofs, quads1d);

   Vector u(dofs);
   Vector d(dofs);
   for (int k = 0; k < quads1d; k++)
   {
      int ind = backward ? quads1d -1 - k : k;
      const IntegrationPoint &ip = ir1d.IntPoint(k);
      basis1d.Eval(ip.x, u, d);
      for (int i = 0; i < dofs; i++)
      {
         shape1d(i, ind) = u(i);
      }
   }
}

/**
*  This class contains the B matrix that contains the evaluation of basis functions at quadrature points.
*/
class TensorBasis
{
protected:
   const int dim;

private:
   typedef Tensor<2> Tensor2d;
   Tensor2d shape1d;

public:
   TensorBasis(const FiniteElementSpace* fes, const int order)
   : dim(fes->GetFE(0)->GetDim()),
     shape1d(fes->GetNDofs1d(),fes->GetNQuads1d(order))
   {
      // Store the 1d shape functions and gradients
      ComputeBasis1d(fes->GetFE(0), order, shape1d);
   }

   const Tensor2d& getB() const
   {
      return shape1d;
   }
};

/**
*  This class contains the matrices that contains the evaluation of basis functions at quadrature points on faces.
*/
class FaceTensorBasis: public TensorBasis
{
private:
   typedef Tensor<2> Tensor2d;
   Tensor2d shape0d0, shape0d1;

public:
   FaceTensorBasis(const FiniteElementSpace* fes, const int order)
   : TensorBasis(fes,order),
     shape0d0(fes->GetNDofs1d(),fes->GetNQuads1d(order)),
     shape0d1(fes->GetNDofs1d(),fes->GetNQuads1d(order))
   {
      // Store the two 0d shape functions and gradients
      // in x = 0.0
      ComputeBasis0d(fes->GetFE(0), 0.0  , shape0d0);
      // in x = 1.0
      ComputeBasis0d(fes->GetFE(0), 1.0  , shape0d1);
   }

   const Tensor2d& getB0d(const int face_id) const
   {
      switch(dim)
      {
         case 1:
         switch(face_id)
         {
            case 0://WEST
               return shape0d0;
            case 1://EAST
               return shape0d1;
         }
         case 2:
         switch(face_id)
         {
            case 0://SOUTH
               return shape0d0;
            case 1://EAST
               return shape0d1;
            case 2://NORTH
               return shape0d1;
            case 3://WEST
               return shape0d0;
         }
         case 3:
         switch(face_id)
         {
            case 0://BOTTOM
               return shape0d0;
            case 1://SOUTH
               return shape0d0;
            case 2://EAST
               return shape0d1;
            case 3://NORTH
               return shape0d1;
            case 4://WEST
               return shape0d0;
            case 5://TOP
               return shape0d1;
         }
      }
   }

   const Tensor2d& getB0dTrial(const int face_id) const
   {
      switch(dim)
      {
         case 1:
         switch(face_id)
         {
            case 0://WEST
               return shape0d1;
            case 1://EAST
               return shape0d0;
         }
         case 2:
         switch(face_id)
         {
            case 0://SOUTH
               return shape0d1;
            case 1://EAST
               return shape0d0;
            case 2://NORTH
               return shape0d0;
            case 3://WEST
               return shape0d1;
         }
         case 3:
         switch(face_id)
         {
            case 0://BOTTOM
               return shape0d1;
            case 1://SOUTH
               return shape0d1;
            case 2://EAST
               return shape0d0;
            case 3://NORTH
               return shape0d0;
            case 4://WEST
               return shape0d1;
            case 5://TOP
               return shape0d0;
         }
      }
   }

   const Tensor2d& getB0dTest(const int face_id) const
   {
      switch(dim)
      {
         case 1:
         switch(face_id)
         {
            case 0://WEST
               return shape0d0;
            case 1://EAST
               return shape0d1;
         }
         case 2:
         switch(face_id)
         {
            case 0://SOUTH
               return shape0d0;
            case 1://EAST
               return shape0d1;
            case 2://NORTH
               return shape0d1;
            case 3://WEST
               return shape0d0;
         }
         case 3:
         switch(face_id)
         {
            case 0://BOTTOM
               return shape0d0;
            case 1://SOUTH
               return shape0d0;
            case 2://EAST
               return shape0d1;
            case 3://NORTH
               return shape0d1;
            case 4://WEST
               return shape0d0;
            case 5://TOP
               return shape0d1;
         }
      }
   }
};

/**
*  this function goes from the L-vector to E-vector ordered in Lexicographical order.
*/
inline void ScatterDofs(const FiniteElementSpace* mfes, const Table& eldof, const Array<int>& dof_map,
                  const GridFunction* nodes, const int dofs, const int dim, const int e,
                  Tensor<2>& LexPointMat)
{
   if (dof_map.Size()==0)
   {
      if(mfes->GetOrdering()==Ordering::byVDIM)
      {
         for (int i = 0; i < dofs; ++i)
         {
            for (int j = 0; j < dim; ++j)
            {
               LexPointMat(j,i) = (*nodes)( ( e*dofs + i )*dim + j );
            }
         }
      }else{
         for (int i = 0; i < dofs; ++i)
         {
            for (int j = 0; j < dim; ++j)
            {
               LexPointMat(j,i) = (*nodes)( ( e*dofs + i ) + j*mfes->GetNDofs() );
            }
         }
      }
   }else{
      if(mfes->GetOrdering()==Ordering::byVDIM)
      {
         for (int i = 0; i < dofs; ++i)
         {
            const int pivot = dof_map[i];
            for (int j = 0; j < dim; ++j)
            {
               LexPointMat(j,i) = (*nodes)(eldof.GetJ()[ e*dofs + pivot ]*dim + j);
            }
         }
      }else{
         for (int i = 0; i < dofs; ++i)
         {
            const int pivot = dof_map[i];
            for (int j = 0; j < dim; ++j)
            {
               LexPointMat(j,i) = (*nodes)(eldof.GetJ()[ e*dofs + pivot ] + j*mfes->GetNDofs());
            }
         }
      }
   }   
}

/**
*  This function evaluates the Jacobain in 1D using a partial assembly approach.
*/
void EvalJacobians1D(const FiniteElementSpace* fes, const int order, Tensor<1>& J)
{
   const int dim = 1;

   const Mesh* mesh = fes->GetMesh();
   const FiniteElementSpace* mfes = mesh->GetNodalFESpace();
   const Table& eldof = mfes->GetElementToDofTable();
   const FiniteElement* fe = mfes->GetFE(0);
   const TensorBasisElement* tfe = dynamic_cast<const TensorBasisElement*>(fe);
   const Array<int>& dof_map = tfe->GetDofMap();
   const GridFunction* nodes = mesh->GetNodes();
   Tensor<2> shape1d(mfes->GetNDofs1d(),mfes->GetNQuads1d(order)), dshape1d(mfes->GetNDofs1d(),mfes->GetNQuads1d(order));
   ComputeBasis1d( fe, order, shape1d, dshape1d );

   const int NE = fes->GetNE();

   const int quads1d = shape1d.Width();
   const int dofs1d = shape1d.Height();
   const int dofs = dofs1d;

   Tensor<2> Jac(J.getData(),quads1d,NE);
   Jac.zero();
   Tensor<2> LexPointMat(dim,dofs);

   Tensor<1> T0(LexPointMat.getData(),dofs1d);
   for (int e = 0; e < NE; ++e)
   {
      ScatterDofs(mfes, eldof, dof_map, nodes, dofs, dim, e, LexPointMat);
      // Computing the Jacobian with the tensor product structure
      for (int j1 = 0; j1 < quads1d; ++j1)
      {
         for (int i1 = 0; i1 < dofs1d; ++i1)
         {
            for (int d = 0; d < dim; ++d)
            {
               Jac(j1,e) += T0(i1) * dshape1d(i1,j1);
            }
         }
      }
   }
}

/**
*  This function evaluates the Jacobain in 2D using a partial assembly approach.
*/
void EvalJacobians2D(const FiniteElementSpace* fes, const int order, Tensor<1>& J)
{
   const int dim = 2;

   const Mesh* mesh = fes->GetMesh();
   const FiniteElementSpace* mfes = mesh->GetNodalFESpace();
   const Table& eldof = mfes->GetElementToDofTable();
   const FiniteElement* fe = mfes->GetFE(0);
   const TensorBasisElement* tfe = dynamic_cast<const TensorBasisElement*>(fe);
   const Array<int>& dof_map = tfe->GetDofMap();
   const GridFunction* nodes = mesh->GetNodes();
   Tensor<2> shape1d(mfes->GetNDofs1d(),mfes->GetNQuads1d(order)), dshape1d(mfes->GetNDofs1d(),mfes->GetNQuads1d(order));
   ComputeBasis1d( fe, order, shape1d, dshape1d );

   const int NE = mfes->GetNE();

   const int quads1d = shape1d.Width();
   const int dofs1d  = shape1d.Height();
   const int dofs = dofs1d * dofs1d;


   Tensor<5> Jac(J.getData(),dim,dim,quads1d,quads1d,NE);
   Jac.zero();
   Tensor<2> LexPointMat(dim,dofs);

   Tensor<3> T0(LexPointMat.getData(),dim,dofs1d,dofs1d);
   Tensor<2> T1b(dim,quads1d), T1d(dim,quads1d);
   for (int e = 0; e < NE; ++e)
   {
      ScatterDofs(mfes, eldof, dof_map, nodes, dofs, dim, e, LexPointMat);
      // Computing the Jacobian with the tensor product structure
      for (int i2 = 0; i2 < dofs1d; ++i2)
      {
         T1b.zero();
         T1d.zero();
         for (int j1 = 0; j1 < quads1d; ++j1)
         {
            for (int i1 = 0; i1 < dofs1d; ++i1)
            {
               for (int d = 0; d < dim; ++d)
               {
                  T1b(d,j1) += T0(d,i1,i2) *  shape1d(i1,j1);
                  T1d(d,j1) += T0(d,i1,i2) * dshape1d(i1,j1);
               }
            }
         }
         for (int j2 = 0; j2 < quads1d; ++j2)
         {
            for (int j1 = 0; j1 < quads1d; ++j1)
            {
               for (int d = 0; d < dim; ++d)
               {
                  Jac(d,0,j1,j2,e) += T1d(d,j1) *  shape1d(i2,j2);
                  Jac(d,1,j1,j2,e) += T1b(d,j1) * dshape1d(i2,j2);
               }
            }
         }
      }
   }
}

/**
*  This function evaluates the Jacobain in 3D using a partial assembly approach.
*/
void EvalJacobians3D(const FiniteElementSpace* fes, const int order, Tensor<1>& J)
{
   const int dim = 3;

   const Mesh* mesh = fes->GetMesh();
   const FiniteElementSpace* mfes = mesh->GetNodalFESpace();
   const Table& eldof = mfes->GetElementToDofTable();
   const FiniteElement* fe = mfes->GetFE(0);
   const TensorBasisElement* tfe = dynamic_cast<const TensorBasisElement*>(fe);
   const Array<int>& dof_map = tfe->GetDofMap();
   const GridFunction* nodes = mesh->GetNodes();
   Tensor<2> shape1d(mfes->GetNDofs1d(),mfes->GetNQuads1d(order)), dshape1d(mfes->GetNDofs1d(),mfes->GetNQuads1d(order));
   ComputeBasis1d( fe, order, shape1d, dshape1d );

   const int NE = fes->GetNE();

   const int quads1d = shape1d.Width();
   const int dofs1d = shape1d.Height();
   const int dofs = dofs1d * dofs1d * dofs1d;

   Tensor<6> Jac(J.getData(),dim,dim,quads1d,quads1d,quads1d,NE);
   Jac.zero();
   Tensor<2> LexPointMat(dim,dofs);

   Tensor<4> T0(LexPointMat.getData(),dim,dofs1d,dofs1d,dofs1d);
   Tensor<2> T1b(dim,quads1d), T1d(dim,quads1d);
   Tensor<3> T2bb(dim,quads1d,quads1d), T2db(dim,quads1d,quads1d), T2bd(dim,quads1d,quads1d);
   for (int e = 0; e < NE; ++e)
   {
      ScatterDofs(mfes, eldof, dof_map, nodes, dofs, dim, e, LexPointMat);
      // Computing the Jacobian with the tensor product structure
      for (int i3 = 0; i3 < dofs1d; ++i3)
      {
         T2bb.zero();
         T2db.zero();
         T2bd.zero();
         for (int i2 = 0; i2 < dofs1d; ++i2)
         {
            T1b.zero();
            T1d.zero();
            for (int j1 = 0; j1 < quads1d; ++j1)
            {
               for (int i1 = 0; i1 < dofs1d; ++i1)
               {
                  for (int d = 0; d < dim; ++d)
                  {
                     T1b(d,j1) += T0(d,i1,i2,i3) *  shape1d(i1,j1);
                     T1d(d,j1) += T0(d,i1,i2,i3) * dshape1d(i1,j1);
                  }
               }
            }
            for (int j2 = 0; j2 < quads1d; ++j2)
            {
               for (int j1 = 0; j1 < quads1d; ++j1)
               {
                  for (int d = 0; d < dim; ++d)
                  {
                     T2bb(d,j1,j2) += T1b(d,j1) *  shape1d(i2,j2);
                     T2bd(d,j1,j2) += T1b(d,j1) * dshape1d(i2,j2);
                     T2db(d,j1,j2) += T1d(d,j1) *  shape1d(i2,j2);
                  }
               }
            }
         }
         for (int j3 = 0; j3 < quads1d; ++j3)
         {
            for (int j2 = 0; j2 < quads1d; ++j2)
            {
               for (int j1 = 0; j1 < quads1d; ++j1)
               {
                  for (int d = 0; d < dim; ++d)
                  {
                     Jac(d,0,j1,j2,j3,e) += T2db(d,j1,j2) *  shape1d(i3,j3);
                     Jac(d,1,j1,j2,j3,e) += T2bd(d,j1,j2) *  shape1d(i3,j3);
                     Jac(d,2,j1,j2,j3,e) += T2bb(d,j1,j2) * dshape1d(i3,j3);
                  }
               }
            }
         }
      }
   }
}

/**
*  This function returns the Jacobain using a partial assembly approach.
*/
void EvalJacobians( const int dim, const FiniteElementSpace* fes, const int order,
                     Tensor<1>& J )
{
   switch(dim)
   {
      case 1:
         EvalJacobians1D( fes, order, J );
         break;
      case 2:
         EvalJacobians2D( fes, order, J );
         break;
      case 3:
         EvalJacobians3D( fes, order, J );
         break;
      default:
         mfem_error("This orientation does not exist in 3D");
   }
}

/**
*  This function returns the diagonal of a matrix represented by a partial assembly operator in 1D.
*/
template <int Dim, typename Op>
void GetDiag1d(const FiniteElementSpace& fes, const int order, const Op& op, Tensor<Dim>& diag)
{
   const int dofs1d = fes.GetNDofs1d();
   const int quads1d = fes.GetNQuads1d(order);
   const int nb_elts = fes.GetNE();
   Tensor<2> diagT(diag, dofs1d, nb_elts);
   auto Dlin = op.getD();
   Tensor<2> D(Dlin, quads1d, nb_elts);
   Tensor<2> shape1d(dofs1d,quads1d);
   ComputeBasis1d( fes.GetFE(0), order, shape1d);

   diagT.zero();
   for (int e = 0; e < nb_elts; ++e)
   {
      for (int j1 = 0; j1 < quads1d; ++j1)
      {
         for (int i1 = 0; i1 < dofs1d; ++i1)
         {
               diagT(i1,e) += D(j1,e) * shape1d(i1,j1) * shape1d(i1,j1);
         }
      }
   }
}

/**
*  This function returns the diagonal of a matrix represented by a partial assembly operator in 2D.
*/
template <int Dim, typename Op>
void GetDiag2d(const FiniteElementSpace& fes, const int order, const Op& op, Tensor<Dim>& diag)
{
   const int dofs1d = fes.GetNDofs1d();
   const int quads1d = fes.GetNQuads1d(order);
   const int nb_elts = fes.GetNE();
   Tensor<3> diagT(diag, dofs1d, dofs1d, nb_elts);
   auto Dlin = op.getD();
   Tensor<3> D(Dlin, quads1d, quads1d, nb_elts);
   Tensor<2> shape1d(dofs1d,quads1d);
   ComputeBasis1d( fes.GetFE(0), order, shape1d);

   Tensor<2> T1(dofs1d,quads1d);

   diagT.zero();
   for (int e = 0; e < nb_elts; ++e)
   {
      T1.zero();
      for (int j2 = 0; j2 < quads1d; ++j2)
      {
         for (int j1 = 0; j1 < quads1d; ++j1)
         {
            for (int i1 = 0; i1 < dofs1d; ++i1)
            {
                  T1(i1,j2) += D(j1,j2,e) * shape1d(i1,j1) * shape1d(i1,j1);
            }
         }
      }
      for (int j2 = 0; j2 < quads1d; ++j2)
      {
         for (int i2 = 0; i2 < dofs1d; ++i2)
         {
            for (int i1 = 0; i1 < dofs1d; ++i1)
            {
                  diagT(i1,i2,e) += T1(i1,j2) * shape1d(i2,j2) * shape1d(i2,j2);
            }
         }
      }
   }
}


/**
*  This function returns the diagonal of a matrix represented by a partial assembly operator in 3D.
*/
template <int Dim, typename Op>
void GetDiag3d(const FiniteElementSpace& fes, const int order, const Op& op, Tensor<Dim>& diag)
{
   const int dofs1d = fes.GetNDofs1d();
   const int quads1d = fes.GetNQuads1d(order);
   const int nb_elts = fes.GetNE();
   Tensor<4> diagT(diag, dofs1d, dofs1d, dofs1d, nb_elts);
   auto Dlin = op.getD();
   Tensor<4> D(Dlin, quads1d, quads1d, quads1d, nb_elts);
   Tensor<2> shape1d(dofs1d,quads1d);
   ComputeBasis1d( fes.GetFE(0), order, shape1d);

   Tensor<3> T1(dofs1d,quads1d,quads1d),T2(dofs1d,dofs1d,quads1d);

   diagT.zero();
   for (int e = 0; e < nb_elts; ++e)
   {
      T1.zero();
      for (int j3 = 0; j3 < quads1d; ++j3)
      {
         for (int j2 = 0; j2 < quads1d; ++j2)
         {
            for (int j1 = 0; j1 < quads1d; ++j1)
            {
               for (int i1 = 0; i1 < dofs1d; ++i1)
               {
                     T1(i1,j2,j3) += D(j1,j2,j3,e) * shape1d(i1,j1) * shape1d(i1,j1);
               }
            }
         }
      }
      T2.zero();
      for (int j3 = 0; j3 < quads1d; ++j3)
      {
         for (int j2 = 0; j2 < quads1d; ++j2)
         {
            for (int i2 = 0; i2 < dofs1d; ++i2)
            {
               for (int i1 = 0; i1 < dofs1d; ++i1)
               {
                     T2(i1,i2,j3) += T1(i1,j2,j3) * shape1d(i2,j2) * shape1d(i2,j2);
               }
            }
         }
      }
      for (int j3 = 0; j3 < quads1d; ++j3)
      {
         for (int i3 = 0; i3 < dofs1d; ++i3)
         {
            for (int i2 = 0; i2 < dofs1d; ++i2)
            {
               for (int i1 = 0; i1 < dofs1d; ++i1)
               {
                     diagT(i1,i2,i3,e) += T2(i1,i2,j3) * shape1d(i3,j3) * shape1d(i3,j3);
               }
            }
         }
      }
   }
}

/**
*  This function returns the diagonal of a matrix represented by a partial assembly operator.
*/
template <int Dim, typename Op>
void GetDiag(const FiniteElementSpace& fes, const int order, const Op& op, Tensor<Dim>& diag)
{
   switch(fes.GetFE(0)->GetDim())
   {
      case 1:
         GetDiag1d(fes, order, op, diag);
         break;
      case 2:
         GetDiag2d(fes, order, op, diag);
         break;
      case 3:
         GetDiag3d(fes, order, op, diag);
         break;
   }
}

}

#endif