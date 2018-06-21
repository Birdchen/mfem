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


#ifndef MFEM_DOMAINKERNELS
#define MFEM_DOMAINKERNELS

#include "dalg.hpp"
#include "dgpabilininteg.hpp"
#include "tensorialfunctions.hpp"
#include "partialassemblykernel.hpp"

namespace mfem
{

/////////////////////////////////////////////////
//                                             //
//                                             //
//               DOMAIN KERNEL                 //
//                                             //
//                                             //
/////////////////////////////////////////////////

  //////////////////////////////
 // Available Domain Kernels //
//////////////////////////////
/**
*  simple CPU implementation
*/
template <typename  Equation, PAOp OpName = Equation::OpName>
class DomainMult;

/**
*  A class that implement the BtDB partial assembly Kernel
*/
template <typename Equation>
class DomainMult<Equation,PAOp::BtDB>: private Equation
{
public:
   static const int dimD = 2;
   typedef Tensor<dimD,double> DTensor;
   typedef DenseMatrix Tensor2d;

protected:
   const FiniteElementSpace *fes;
   Tensor2d shape1d;
   DTensor D;

public:
   DomainMult(const FiniteElementSpace* _fes, int order)
   : fes(_fes), shape1d(fes->GetNDofs1d(),fes->GetNQuads1d(order)), D()
   {
      ComputeBasis1d(fes->GetFE(0), order, shape1d);
   }

   void InitD(const int dim, const int quads, const int nb_elts){
      this->D.setSize(quads,nb_elts);
   }

   const DTensor& getD() const
   {
      return D;
   }

   template <typename Args>
   void evalEq(const int dim, const int k, const int e, ElementTransformation * Tr,
               const IntegrationPoint & ip, const Tensor<2>& J, const Args& args)
   {
      double res = 0.0;
      this->evalD(res, Tr, ip, J, args);
      this->D(k,e) = res;
   }

   void evalEq(const int dim, const int k, const int e, ElementTransformation * Tr,
               const IntegrationPoint & ip, const Tensor<2>& J)
   {
      double res = 0.0;
      this->evalD(res, Tr, ip, J);
      this->D(k,e) = res;
   }
protected:
   /**
   *  The domain Kernels for BtDB in 1d,2d and 3d.
   */
   void Mult1d(const Vector &V, Vector &U) const;
   void Mult2d(const Vector &V, Vector &U) const;
   void Mult3d(const Vector &V, Vector &U) const;  

};


/**
*  A class that implement a BtDG partial assembly Kernel
*/
template <typename Equation>
class DomainMult<Equation,PAOp::BtDG>: private Equation
{
public:
   static const int dimD = 3;
   typedef Tensor<dimD,double> DTensor;
   typedef DenseMatrix Tensor2d;

protected:
   const FiniteElementSpace *fes;
   Tensor2d shape1d, dshape1d;
   DTensor D;

public:
   DomainMult(const FiniteElementSpace* _fes, int order)
   : fes(_fes), shape1d(fes->GetNDofs1d(),fes->GetNQuads1d(order)),
   dshape1d(fes->GetNDofs1d(),fes->GetNQuads1d(order)), D()
   {
      ComputeBasis1d(fes->GetFE(0), order, shape1d, dshape1d);
   }

   const DTensor& getD() const
   {
      return D;
   }

   void InitD(const int dim, const int quads, const int nb_elts){
      this->D.setSize(dim,quads,nb_elts);
   }

   template <typename Args>
   void evalEq(const int dim, const int k, const int e, ElementTransformation * Tr,
               const IntegrationPoint & ip, const Tensor<2>& J, const Args& args)
   {
      Tensor<1> res(dim);
      this->evalD(res, Tr, ip, J, args);
      for (int i = 0; i < dim; ++i)
      {
         this->D(i,k,e) = res(i);
      }
   }

   void evalEq(const int dim, const int k, const int e, ElementTransformation * Tr,
               const IntegrationPoint & ip, const Tensor<2>& J)
   {
      Tensor<1> res(dim);
      this->evalD(res, Tr, ip, J);
      for (int i = 0; i < dim; ++i)
      {
         this->D(i,k,e) = res(i);
      }
   }

protected:
   /**
   *  The domain Kernels for BtDG in 1d,2d and 3d.
   */
   void Mult1d(const Vector &V, Vector &U) const;
   void Mult2d(const Vector &V, Vector &U) const;
   void Mult3d(const Vector &V, Vector &U) const;

};



      //////////////////////////////////////
     ///                                ///
    ///                                ///
   /// IMPLEMENTATION OF THE KERNELS  ///
  ///                                ///
 ///                                ///
//////////////////////////////////////

template<typename Equation>
void DomainMult<Equation,PAOp::BtDB>::Mult1d(const Vector &V, Vector &U) const
{
   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();
   const int quads = quads1d;

   Vector Q(quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); e++)
   {
      const Vector Vmat(V.GetData() + offset, dofs1d);
      Vector Umat(U.GetData() + offset, dofs1d);

      // Q_k1 = dshape_j1_k1 * V_i1
      shape1d.MultTranspose(Vmat, Q);

      double *data_q = Q.GetData();
      // const double *data_d = D.GetElmtData(e);
      for (int k = 0; k < quads; ++k) { data_q[k] *= D(k,e); }

      // Q_k1 = dshape_j1_k1 * Q_k1
      shape1d.AddMult(Q, Umat);
   }
}

template<typename Equation>
void DomainMult<Equation,PAOp::BtDB>::Mult2d(const Vector &V, Vector &U) const
{
   const FiniteElement *fe = fes->GetFE(0);
   const int dofs   = fe->GetDof();

   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();
   const int quads  = quads1d*quads1d;

   DenseMatrix QQ(quads1d, quads1d);
   DenseMatrix DQ(dofs1d, quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); e++)
   {
      const DenseMatrix Vmat(V.GetData() + offset, dofs1d, dofs1d);
      DenseMatrix Umat(U.GetData() + offset, dofs1d, dofs1d);

      // DQ_j2_k1   = E_j1_j2  * shape_j1_k1 -- contract in x direction
      // QQ_0_k1_k2 = DQ_j2_k1 * shape_j2_k2  -- contract in y direction
      MultAtB(Vmat, shape1d, DQ);
      MultAtB(DQ, shape1d, QQ);

      // QQ_c_k1_k2 = Dmat_c_d_k1_k2 * QQ_d_k1_k2
      // NOTE: (k1, k2) = k -- 1d index over tensor product of quad points
      double *data_qq = QQ.GetData();
      // const double *data_d = D.GetElmtData(e);
      for (int k = 0; k < quads; ++k) { data_qq[k] *= D(k,e); }

      // DQ_i2_k1   = shape_i2_k2  * QQ_0_k1_k2
      // U_i1_i2   += dshape_i1_k1 * DQ_i2_k1
      MultABt(shape1d, QQ, DQ);
      AddMultABt(shape1d, DQ, Umat);

      // increment offset
      offset += dofs;
   }
}

template<typename Equation>
void DomainMult<Equation,PAOp::BtDB>::Mult3d(const Vector &V, Vector &U) const
{
   const FiniteElement *fe = fes->GetFE(0);
   const int dofs   = fe->GetDof();

   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();
   const int quads  = quads1d * quads1d * quads1d;

   Vector Q(quads1d);
   DenseMatrix QQ(quads1d, quads1d);
   DenseTensor QQQ(quads1d, quads1d, quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); e++)
   {
      const DenseTensor Vmat(V.GetData() + offset, dofs1d, dofs1d, dofs1d);
      DenseTensor Umat(U.GetData() + offset, dofs1d, dofs1d, dofs1d);

      // QQQ_k1_k2_k3 = shape_j1_k1 * shape_j2_k2  * shape_j3_k3  * Vmat_j1_j2_j3
      QQQ = 0.;
      for (int j3 = 0; j3 < dofs1d; ++j3)
      {
         QQ = 0.;
         for (int j2 = 0; j2 < dofs1d; ++j2)
         {
            Q = 0.;
            for (int j1 = 0; j1 < dofs1d; ++j1)
            {
               for (int k1 = 0; k1 < quads1d; ++k1)
               {
                  Q(k1) += Vmat(j1, j2, j3) * shape1d(j1, k1);
               }
            }
            for (int k2 = 0; k2 < quads1d; ++k2)
               for (int k1 = 0; k1 < quads1d; ++k1)
               {
                  QQ(k1, k2) += Q(k1) * shape1d(j2, k2);
               }
         }
         for (int k3 = 0; k3 < quads1d; ++k3)
            for (int k2 = 0; k2 < quads1d; ++k2)
               for (int k1 = 0; k1 < quads1d; ++k1)
               {
                  QQQ(k1, k2, k3) += QQ(k1, k2) * shape1d(j3, k3);
               }
      }

      // QQQ_k1_k2_k3 = Dmat_k1_k2_k3 * QQQ_k1_k2_k3
      // NOTE: (k1, k2, k3) = q -- 1d quad point index
      double *data_qqq = QQQ.GetData(0);
      // const double *data_d = D.GetElmtData(e);
      for (int k = 0; k < quads; ++k) { data_qqq[k] *= D(k,e); }

      // Apply transpose of the first operator that takes V -> QQQ -- QQQ -> U
      for (int k3 = 0; k3 < quads1d; ++k3)
      {
         QQ = 0.;
         for (int k2 = 0; k2 < quads1d; ++k2)
         {
            Q = 0.;
            for (int k1 = 0; k1 < quads1d; ++k1)
            {
               for (int i1 = 0; i1 < dofs1d; ++i1)
               {
                  Q(i1) += QQQ(k1, k2, k3) * shape1d(i1, k1);
               }
            }
            for (int i2 = 0; i2 < dofs1d; ++i2)
               for (int i1 = 0; i1 < dofs1d; ++i1)
               {
                  QQ(i1, i2) += Q(i1) * shape1d(i2, k2);
               }
         }
         for (int i3 = 0; i3 < dofs1d; ++i3)
            for (int i2 = 0; i2 < dofs1d; ++i2)
               for (int i1 = 0; i1 < dofs1d; ++i1)
               {
                  Umat(i1, i2, i3) += shape1d(i3, k3) * QQ(i1, i2);
               }
      }

      // increment offset
      offset += dofs;
   }  
}

template<typename Equation>
void DomainMult<Equation,PAOp::BtDG>::Mult1d(const Vector &V, Vector &U) const
{
   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();
   const int quads = quads1d;

   Vector Q(quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); e++)
   {
      const Vector Vmat(V.GetData() + offset, dofs1d);
      Vector Umat(U.GetData() + offset, dofs1d);

      // Q_k1 = dshape_j1_k1 * V_i1
      dshape1d.MultTranspose(Vmat, Q);

      double *data_q = Q.GetData();
      // const double *data_d = D.GetElmtData(e);
      for (int k = 0; k < quads; ++k)
      {
         data_q[k] *= D(0,k,e);
      }

      // Q_k1 = dshape_j1_k1 * Q_k1
      shape1d.AddMult(Q, Umat);
   }   
}

template<typename Equation>
void DomainMult<Equation,PAOp::BtDG>::Mult2d(const Vector &V, Vector &U) const
{
   const int dim = 2;

   const FiniteElement *fe = fes->GetFE(0);
   const int dofs   = fe->GetDof();

   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();
   const int quads  = quads1d * quads1d;

   DenseTensor QQ(quads1d, quads1d, dim);
   DenseMatrix DQ(dofs1d, quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); e++)
   {
      const DenseMatrix Vmat(V.GetData() + offset, dofs1d, dofs1d);
      DenseMatrix Umat(U.GetData() + offset, dofs1d, dofs1d);

      // DQ_j2_k1   = E_j1_j2  * dshape_j1_k1 -- contract in x direction
      // QQ_0_k1_k2 = DQ_j2_k1 * shape_j2_k2  -- contract in y direction
      MultAtB(Vmat, dshape1d, DQ);
      MultAtB(DQ, shape1d, QQ(0));

      // DQ_j2_k1   = E_j1_j2  * shape_j1_k1  -- contract in x direction
      // QQ_1_k1_k2 = DQ_j2_k1 * dshape_j2_k2 -- contract in y direction
      MultAtB(Vmat, shape1d, DQ);
      MultAtB(DQ, dshape1d, QQ(1));

      // QQ_c_k1_k2 = Dmat_c_d_k1_k2 * QQ_d_k1_k2
      // NOTE: (k1, k2) = k -- 1d index over tensor product of quad points
      double *data_qq = QQ(0).GetData();
      for (int k = 0; k < quads; ++k)
      {
         // int ind0[] = {0,k,e};
         const double D0 = D(0,k,e);
         // int ind1[] = {1,k,e};
         const double D1 = D(1,k,e);

         const double q0 = data_qq[0*quads + k];
         const double q1 = data_qq[1*quads + k];

         data_qq[0*quads + k] = D0 * q0 + D1 * q1;
      }

      // DQ_i2_k1   = shape_i2_k2  * QQ_0_k1_k2
      // U_i1_i2   += dshape_i1_k1 * DQ_i2_k1
      MultABt(shape1d, QQ(0), DQ);
      AddMultABt(shape1d, DQ, Umat);

      // increment offset
      offset += dofs;
   }
}

template<typename Equation>
void DomainMult<Equation,PAOp::BtDG>::Mult3d(const Vector &V, Vector &U) const
{
   const int dim = 3;

   const FiniteElement *fe = fes->GetFE(0);
   const int dofs   = fe->GetDof();

   const int dofs1d = shape1d.Height();
   const int quads1d = shape1d.Width();
   const int quads  = quads1d * quads1d * quads1d;

   DenseMatrix Q(quads1d, dim);
   DenseTensor QQ(quads1d, quads1d, dim);

   Array<double> QQQmem(quads1d * quads1d * quads1d * dim);
   double *data_qqq = QQQmem.GetData();
   DenseTensor QQQ0(data_qqq + 0*quads, quads1d, quads1d, quads1d);
   DenseTensor QQQ1(data_qqq + 1*quads, quads1d, quads1d, quads1d);
   DenseTensor QQQ2(data_qqq + 2*quads, quads1d, quads1d, quads1d);

   int offset = 0;
   for (int e = 0; e < fes->GetNE(); e++)
   {
      const DenseTensor Vmat(V.GetData() + offset, dofs1d, dofs1d, dofs1d);
      DenseTensor Umat(U.GetData() + offset, dofs1d, dofs1d, dofs1d);

      // QQQ_0_k1_k2_k3 = dshape_j1_k1 * shape_j2_k2  * shape_j3_k3  * Vmat_j1_j2_j3
      // QQQ_1_k1_k2_k3 = shape_j1_k1  * dshape_j2_k2 * shape_j3_k3  * Vmat_j1_j2_j3
      // QQQ_2_k1_k2_k3 = shape_j1_k1  * shape_j2_k2  * dshape_j3_k3 * Vmat_j1_j2_j3
      QQQ0 = 0.; QQQ1 = 0.; QQQ2 = 0.;
      for (int j3 = 0; j3 < dofs1d; ++j3)
      {
         QQ = 0.;
         for (int j2 = 0; j2 < dofs1d; ++j2)
         {
            Q = 0.;
            for (int j1 = 0; j1 < dofs1d; ++j1)
            {
               for (int k1 = 0; k1 < quads1d; ++k1)
               {
                  Q(k1, 0) += Vmat(j1, j2, j3) * dshape1d(j1, k1);
                  Q(k1, 1) += Vmat(j1, j2, j3) * shape1d(j1, k1);
               }
            }
            for (int k2 = 0; k2 < quads1d; ++k2)
               for (int k1 = 0; k1 < quads1d; ++k1)
               {
                  QQ(k1, k2, 0) += Q(k1, 0) * shape1d(j2, k2);
                  QQ(k1, k2, 1) += Q(k1, 1) * dshape1d(j2, k2);
                  QQ(k1, k2, 2) += Q(k1, 1) * shape1d(j2, k2);
               }
         }
         for (int k3 = 0; k3 < quads1d; ++k3)
            for (int k2 = 0; k2 < quads1d; ++k2)
               for (int k1 = 0; k1 < quads1d; ++k1)
               {
                  QQQ0(k1, k2, k3) += QQ(k1, k2, 0) * shape1d(j3, k3);
                  QQQ1(k1, k2, k3) += QQ(k1, k2, 1) * shape1d(j3, k3);
                  QQQ2(k1, k2, k3) += QQ(k1, k2, 2) * dshape1d(j3, k3);
               }
      }

      // QQQ_c_k1_k2_k3 = Dmat_c_d_k1_k2_k3 * QQQ_d_k1_k2_k3
      // NOTE: (k1, k2, k3) = q -- 1d quad point index
      // const double *data_d = D.GetElmtData(e);
      for (int k = 0; k < quads; ++k)
      {
         // int ind0[] = {0,k,e};
         const double D0 = D(0,k,e);
         // int ind1[] = {1,k,e};
         const double D1 = D(1,k,e);
         // int ind2[] = {2,k,e};
         const double D2 = D(2,k,e);

         const double q0 = data_qqq[0*quads + k];
         const double q1 = data_qqq[1*quads + k];
         const double q2 = data_qqq[2*quads + k];

         data_qqq[0*quads + k] = D0 * q0 + D1 * q1 + D2 * q2;
      }

      // Apply transpose of the first operator that takes V -> QQQd -- QQQd -> U
      for (int k3 = 0; k3 < quads1d; ++k3)
      {
         QQ = 0.;
         for (int k2 = 0; k2 < quads1d; ++k2)
         {
            Q = 0.;
            for (int k1 = 0; k1 < quads1d; ++k1)
            {
               for (int i1 = 0; i1 < dofs1d; ++i1)
               {
                  Q(i1, 0) += QQQ0(k1, k2, k3) * shape1d(i1, k1);
               }
            }
            for (int i2 = 0; i2 < dofs1d; ++i2)
               for (int i1 = 0; i1 < dofs1d; ++i1)
               {
                  QQ(i1, i2, 0) += Q(i1, 0) * shape1d(i2, k2);
               }
         }
         for (int i3 = 0; i3 < dofs1d; ++i3)
            for (int i2 = 0; i2 < dofs1d; ++i2)
               for (int i1 = 0; i1 < dofs1d; ++i1)
               {
                  Umat(i1, i2, i3) += QQ(i1, i2, 0) * shape1d(i3, k3);
               }
      }

      // increment offset
      offset += dofs;
   }
}

}

#endif //MFEM_DOMAINKERNELS