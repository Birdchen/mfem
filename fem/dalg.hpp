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

#ifndef MFEM_DUMMYALGEBRA
#define MFEM_DUMMYALGEBRA

#include "../config/config.hpp"
#include "bilininteg.hpp"
#include <vector>
#include <iostream>

using namespace std;
using std::vector;

namespace mfem
{

/**
* A Class to compute the real indice from the multi-indice of a tensor
*/
template <int N, int Dim, typename T, typename... Args>
class TensorInd
{
public:
   static int result(const int* sizes, T first, Args... args)
   {
      return first + sizes[N-1]*TensorInd<N+1,Dim,Args...>::result(sizes,args...);
   }
};
//Terminal case
template <int Dim, typename T, typename... Args>
class TensorInd<Dim,Dim,T,Args...>
{
public:
   static int result(const int* sizes, T first, Args... args)
   {
      return first;
   }
};

/**
* A class to initialize the size of a Tensor
*/
template <int N, int Dim, typename T, typename... Args>
class Init
{
public:
   static int result(int* sizes, T first, Args... args){
      sizes[N-1] = first;
      return first * Init<N+1,Dim,Args...>::result(sizes,args...);
   }
};
//Terminal case
template <int Dim, typename T, typename... Args>
class Init<Dim,Dim,T,Args...>
{
public:
   static int result(int* sizes, T first, Args... args){
      sizes[Dim-1] = first;
      return first;
   }
};

/**
*  A basic generic Tensor class
*/
template<int Dim, typename Scalar=double>
class Tensor
{
protected:
   int capacity;
   Scalar* data;
   bool own_data;
   int sizes[Dim];

public:
   /**
   *  A default constructor
   */
   explicit Tensor()
   : capacity(0), data(NULL), own_data(true)
   {
   }

   /**
   *  A default destructor
   */
   ~Tensor()
   {
      if (own_data)
      {
         delete [] data;
      }
   }

   /**
   *  A constructor to initialize the sizes of a tensor with an array of integers
   */
   Tensor(int* _sizes)
   : own_data(true)
   {
      int nb = 1;
      for (int i = 0; i < Dim; ++i)
      {
         sizes[i] = _sizes[i];
         nb *= sizes[i];
      }
      capacity = nb;
      data = new Scalar[nb];
   }

   /**
   *  A constructor to initialize a tensor from a different size Tensor
   */
   template <int Dim1, typename... Args>
   Tensor(Tensor<Dim1,Scalar>& t, Args... args)
   : own_data(false)
   {
      static_assert(sizeof...(args)==Dim, "Wrong number of arguments");
      // Initialize sizes, and compute the number of values
      long int nb = Init<1,Dim,Args...>::result(sizes,args...);
      capacity = nb;
      data = t.getData();
   }

   /**
   *  A constructor to initialize the sizes of a tensor with a variadic function
   */
   template <typename... Args>
   Tensor(Args... args)
   : own_data(true)
   {
      static_assert(sizeof...(args)==Dim, "Wrong number of arguments");
      // Initialize sizes, and compute the number of values
      long int nb = Init<1,Dim,Args...>::result(sizes,args...);
      capacity = nb;
      data = new Scalar[nb];
   }

   /**
   *  A constructor to initialize a tensor from the Scalar array _data
   */
   template <typename... Args>
   Tensor(Scalar* _data, Args... args)
   : own_data(false)
   {
      static_assert(sizeof...(args)==Dim, "Wrong number of arguments");
      // Initialize sizes, and compute the number of values
      long int nb = Init<1,Dim,Args...>::result(sizes,args...);
      capacity = nb;
      data = _data;
   }

   // Let's write some uggly unsafe code, since C++ does not have const constructor
   template <typename... Args>
   Tensor(const Scalar* _data, Args... args)
   : own_data(false)
   {
      static_assert(sizeof...(args)==Dim, "Wrong number of arguments");
      // Initialize sizes, and compute the number of values
      long int nb = Init<1,Dim,Args...>::result(sizes,args...);
      capacity = nb;
      data = const_cast<Scalar*>(_data);
   }

   // Not interesting, assumes that the compiler will optimize the copy...
   // template <typename... Args>
   // static const Tensor mapTo(const Scalar* data, Args... args)
   // {
   //    return Tensor(const_cast<Scalar*>(data), args);
   // }

   /**
   *  A copy constructor
   */
   Tensor(const Tensor& t)
   :capacity(t.length()), data(new Scalar[capacity]), own_data(true)
   {
      for (int i = 0; i < Dim; ++i)
      {
         sizes[i] = t.size(i);
      }
      const Scalar* data_t = t.getData();
      for (int i = 0; i < capacity; ++i)
      {
         data[i] = data_t[i];
      }
   }

   /**
   *  A copy assignment operator (do not resize Tensors)
   */
   Tensor& operator=(const Tensor& t)
   {
      if (this==&t)
      {
         return *this;
      }
      const int nb = t.length();
      // if (nb>capacity)
      // {
      //    if (own_data)
      //    {
      //       if(data!=NULL) delete [] data;
      //    }
      //    data = new Scalar[nb];
      //    capacity = nb;
      //    own_data = true;
      // }
      for (int i = 0; i < Dim; ++i)
      {
         if(sizes[i] != t.size(i))
         {
            cout << sizes[i] << " | " << t.size(i) << endl;
            mfem_error("The Tensors have different sizes.");
         }
         // sizes[i] = t.size(i);
      }
      for (int i = 0; i < nb; ++i)
      {
         data[i] = t[i];
      }
      return *this;
   }

   /**
   *  Operator += for Tensors of the same size.
   */
   // Tensor& operator+=(const Tensor& t)
   // {
   //    const int nb = t.length();
   //    for (int i = 0; i < Dim; ++i)
   //    {
   //       if(sizes[i] != t.size(i))
   //       {
   //          cout << sizes[i] << " | " << t.size(i) << endl;
   //          mfem_error("The Tensors have different sizes.");
   //       }
   //       // sizes[i] = t.size(i);
   //    }
   //    for (int i = 0; i < nb; ++i)
   //    {
   //       data[i] += t[i];
   //    }
   //    return *this;
   // }

   /**
   *  Sets the size of the tensor, and allocate memory if necessary
   */
   template <typename... Args>
   void setSize(Args... args)
   {
      static_assert(sizeof...(args)==Dim, "Wrong number of arguments");
      // Initialize sizes, and compute the number of values
      long int nb = Init<1,Dim,Args...>::result(sizes,args...);
      if(nb>capacity)
      {
         Scalar* _data = new Scalar[nb];
         for (int i = 0; i < capacity; ++i)
         {
            _data[i] = data[i];
         }
         for (int i = capacity; i < nb; ++i)
         {
            _data[i] = Scalar();
         }
         if (own_data)
         {
            if(data!=NULL) delete [] data;
         }
         data = _data;
         own_data = true;
         capacity = nb;
      }
   }

   /**
   *  A const accessor for the data
   */
   template <typename... Args>
   const Scalar& operator()(Args... args) const
   {
      static_assert(sizeof...(args)==Dim, "Wrong number of arguments");
      return data[ TensorInd<1,Dim,Args...>::result(sizes,args...) ];
   }

   /**
   *  A reference accessor to the data
   */
   template <typename... Args>
   Scalar& operator()(Args... args)
   {
      static_assert(sizeof...(args)==Dim, "Wrong number of arguments");
      return data[ TensorInd<1,Dim,Args...>::result(sizes,args...) ];
   }

   const Scalar& operator[](int i) const
   {
      return data[i];
   }

   Scalar& operator[](int i)
   {
      return data[i];
   }

   void zero()
   {
      for (int i = 0; i < capacity; ++i)
      {
         data[i] = Scalar();
      }
   }

   /**
   *  Returns the size of the i-th dimension #UNSAFE#
   */
   const int size(int i) const
   {
      return sizes[i];
   }

   const int Height() const
   {
      static_assert(Dim==2, "Height() should only be used for second order tensors");
      return sizes[0];
   }

   const int Width() const
   {
      static_assert(Dim==2, "Width() should only be used for second order tensors");
      return sizes[1];
   }

   /**
   *  Returns the length of the Tensor (number of values, may be different from capacity)
   */
   const int length() const
   {
      int res = 1;
      for (int i = 0; i < Dim; ++i)
      {
         res *= sizes[i];
      }
      return res;
   }

   /**
   *  Returns the dimension of the tensor.
   */
   const int dimension() const
   {
      return Dim;
   }

   /**
   *  Returns the Scalar array data (Really unsafe and ugly method)
   *  Mostly exists to remap Tensors, so could be avoided by using more constructors...
   */
   Scalar* getData()
   {
      return data;
   }

   const Scalar* getData() const
   {
      return data;
   }

   /**
   *  Basic printing method.
   */
   friend std::ostream& operator<<(std::ostream& os, const Tensor& T)
   {
      int nb_elts = 1;
      for (int i = 0; i < T.dimension(); ++i)
      {
          nb_elts *= T.size(i);
      }
      for (int i = 0; i < nb_elts; ++i)
      {
          os << T.data[i] << " ";
          if ((i+1)%T.sizes[0]==0)
          {
             os << "\n";
          }
      }
      os << "\n";    
      return os;
   }
};

typedef Tensor<2,int> IntMatrix;

// template <typename Scalar,int N>
// void zero(Tensor<N,Scalar>& t)
// {
//    for (int i = 0; i < t.length(); ++i)
//    {
//       t[i] = Scalar();
//    }  
// }

template <typename Scalar>
inline void adjugate(const Tensor<2,Scalar>& A, Tensor<2,Scalar>& Adj)
{
   const int dim = A.Height();
   switch(dim){
      case 1:
         Adj(0,0) = A(0,0);
         break;
      case 2:
         Adj(0,0) =  A(1,1); Adj(0,1) = -A(0,1);
         Adj(1,0) = -A(1,0); Adj(1,1) =  A(0,0);
         break;
      case 3:
         Adj(0,0) =  A(1,1)*A(2,2)-A(1,2)*A(2,1);
         Adj(0,1) = -A(0,1)*A(2,2)+A(0,2)*A(2,1);
         Adj(0,2) =  A(0,1)*A(1,2)-A(0,2)*A(1,1);
         //
         Adj(1,0) = -A(1,0)*A(2,2)+A(1,2)*A(2,0);
         Adj(1,1) =  A(0,0)*A(2,2)-A(0,2)*A(2,0);
         Adj(1,2) = -A(0,0)*A(1,2)+A(0,2)*A(1,0);
         //
         Adj(2,0) =  A(1,0)*A(2,1)-A(1,1)*A(2,0);
         Adj(2,1) = -A(0,0)*A(2,1)+A(0,1)*A(2,0);
         Adj(2,2) =  A(0,0)*A(1,1)-A(0,1)*A(1,0);
         break;
      default:
         mfem_error("adjugate not defined for this size");
         break;
   }
}

template <typename Scalar>
inline Scalar det(const Tensor<2,Scalar>& A)
{
   const int dim = A.Height();
   switch(dim){
      case 1:
         return A(0,0);
      case 2:
         return A(0,0)*A(1,1) - A(0,1)*A(1,0);
      case 3:
         return A(0,0)*(A(1,1)*A(2,2)-A(1,2)*A(2,1))
               -A(1,0)*(A(0,1)*A(2,2)-A(0,2)*A(2,1))
               +A(2,0)*(A(0,1)*A(1,2)-A(0,2)*A(1,1));
      default:
         mfem_error("determinant not defined for this size");
         break;
   }
   return Scalar();
}

template <typename Scalar>
inline Scalar norm2sq(const Tensor<1,Scalar>& t)
{
   Scalar res = 0.0;
   for (int i = 0; i < t.size(0); ++i)
   {
      res += t(i)*t(i);
   }
   return res;
}

template <typename Scalar>
inline Scalar dot(const Tensor<1,Scalar>& t1, const Tensor<1,Scalar>& t2)
{
   Scalar res = 0.0;
   MFEM_ASSERT(t1.size(0)==t2.size(0), "Tensor<1> t1 and t2 are of different size");
   for (int i = 0; i < t1.size(0); ++i)
   {
      res += t1(i)*t2(i);
   }
   return res;
}

template <typename Scalar>
inline void calcOrtho(const Tensor<2,Scalar>& J, const int& face_id, Tensor<1>& n)
{
   const int dim = n.length();
   switch(dim)
   {
      case 1:
         n(0) = face_id == 0 ? -J(0,0) : J(0,0);
         break;
      case 2:
         //FIXME: looks strange we access 2D Jacobians in a different way than 3D
         switch(face_id)
         {
            case 0://SOUTH ( 0,-1)
               // n(0) = -J(0,1); n(1) = -J(1,1);
               n(0) = -J(1,0); n(1) = -J(1,1);
               break;
            case 1://EAST  ( 1, 0)
               // n(0) =  J(0,0); n(1) =  J(1,0);
               n(0) =  J(0,0); n(1) =  J(0,1);
               break;
            case 2://NORTH ( 0, 1)
               // n(0) =  J(0,1); n(1) =  J(1,1);
               n(0) =  J(1,0); n(1) =  J(1,1);
               break;
            case 3://WEST  (-1, 0)
               // n(0) = -J(0,0); n(1) = -J(1,0);
               n(0) = -J(0,0); n(1) = -J(0,1);
               break;
         }
         break;
      case 3:
         switch(face_id)
         {
            case 0://BOTTOM ( 0, 0,-1)
               n(0) = -J(0,2); n(1) = -J(1,2); n(2) = -J(2,2);
               break;
            case 1://SOUTH  ( 0,-1, 0)
               n(0) = -J(0,1); n(1) = -J(1,1); n(2) = -J(2,1);
               break;
            case 2://EAST   ( 1, 0, 0)
               n(0) =  J(0,0); n(1) =  J(1,0); n(2) =  J(2,0);
               break;
            case 3://NORTH  ( 0, 1, 0)
               n(0) =  J(0,1); n(1) =  J(1,1); n(2) =  J(2,1);
               break;
            case 4://WEST   (-1, 0, 0)
               n(0) = -J(0,0); n(1) = -J(1,0); n(2) = -J(2,0);
               break;
            case 5://TOP    ( 0, 0, 1)
               n(0) =  J(0,2); n(1) =  J(1,2); n(2) =  J(2,2);
               break;
         }
         break;
   }
}

  ///////////////////////////
 // "Volume" contractions //
///////////////////////////


// Would defining those contractions for abstract templated types be better?
 ///////
// 1d
template <typename Scalar>
inline void contract(const Tensor<2,Scalar>& B, const Tensor<1,Scalar>& U, Tensor<1,Scalar>& V)
{
   MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
   for (int j = 0; j < B.size(1); ++j)
   {
      V(j) = Scalar();
      for (int i = 0; i < B.size(0); ++i)
      {
         V(j) += B(i,j) * U(i);
      }
   }
}

// template <typename Scalar>
// inline void contract(const Tensor<2,Scalar>& B, const Tensor<1,Scalar>& U, Tensor<1,Scalar>& V)
// {
//    MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
//    const int B_SIZE = 2;
//    V.zero();
//    for (int i = 0; i < B.size(0); ++i)
//    {
//       for (int j = 0; j < B.size(1)/B_SIZE*B_SIZE; j+=B_SIZE)
//       {
//          V(j)   += B(i,j  ) * U(i);
//          V(j+1) += B(i,j+1) * U(i);
//       }
//       for (int j = B.size(1)/B_SIZE*B_SIZE; j < B.size(1); j++)
//       {
//          V(j) += B(i,j) * U(i);
//       }
//    }
// }

template <typename Scalar>
inline void contractT(const Tensor<2,Scalar>& B, const Tensor<1,Scalar>& U, Tensor<1,Scalar>& V)
{
   MFEM_ASSERT(B.size(1)==U.size(0), "Size mismatch for contraction.");
   for (int j = 0; j < B.size(0); ++j)
   {
      V(j) = Scalar();
      for (int i = 0; i < B.size(1); ++i)
      {
         V(j) += B(j,i) * U(i);
      }
   }
}

// template <typename Scalar>
// inline void contractT(const Tensor<2,Scalar>& B, const Tensor<1,Scalar>& U, Tensor<1,Scalar>& V)
// {
//    MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
//    const int B_SIZE = 2;
//    V.zero();
//    for (int i = 0; i < B.size(1); ++i)
//    {
//       for (int j = 0; j < B.size(0)/B_SIZE*B_SIZE; j+=B_SIZE)
//       {
//          V(j)   += B(j  ,i) * U(i);
//          V(j+1) += B(j+1,i) * U(i);
//       }
//       for (int j = B.size(0)/B_SIZE*B_SIZE; j < B.size(0); j++)
//       {
//          V(j) += B(j,i) * U(i);
//       }
//    }
// }

 ///////
// 2d
// template <typename Scalar>
// inline void contract(const Tensor<2,Scalar>& B, const Tensor<2,Scalar>& U, Tensor<2,Scalar>& V)
// {
//    MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
//    for (int j1 = 0; j1 < B.size(1); ++j1)
//    {
//       for (int i2 = 0; i2 < U.size(1); ++i2)
//       {
//          V(i2,j1) = Scalar();
//          for (int i1 = 0; i1 < B.size(0); ++i1)
//          {
//             V(i2,j1) += B(i1,j1) * U(i1,i2);
//          }
//       }
//    }
// }

template <typename Scalar>
inline void contract(const Tensor<2,Scalar>& B, const Tensor<2,Scalar>& U, Tensor<2,Scalar>& V)
{
   MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
   const int B_SIZE = 2;
   for (int j1 = 0; j1 < B.size(1); ++j1)
   {
      for (int i2 = 0; i2 < U.size(1)/B_SIZE*B_SIZE; i2+=B_SIZE)
      {
         V(i2  ,j1) = Scalar();
         V(i2+1,j1) = Scalar();
         // V(i2+2,j1) = Scalar();
         // V(i2+3,j1) = Scalar();
         for (int i1 = 0; i1 < B.size(0); ++i1)
         {
            V(i2  ,j1) += B(i1,j1) * U(i1,i2  );
            V(i2+1,j1) += B(i1,j1) * U(i1,i2+1);
            // V(i2+2,j1) += B(i1,j1) * U(i1,i2+2);
            // V(i2+3,j1) += B(i1,j1) * U(i1,i2+3);
         }
      }
      for (int i2 = U.size(1)/B_SIZE*B_SIZE; i2 < U.size(1); i2++)
      {
         V(i2,j1) = Scalar();
         for (int i1 = 0; i1 < B.size(0); ++i1)
         {
            V(i2,j1) += B(i1,j1) * U(i1,i2);
         }
      }
   }
}

// template <typename Scalar>
// inline void contractT(const Tensor<2,Scalar>& B, const Tensor<2,Scalar>& U, Tensor<2,Scalar>& V)
// {
//    MFEM_ASSERT(B.size(1)==U.size(0), "Size mismatch for contraction.");
//    for (int j1 = 0; j1 < B.size(0); ++j1)
//    {
//       for (int i2 = 0; i2 < U.size(1); ++i2)
//       {
//          V(i2,j1) = Scalar();
//          for (int i1 = 0; i1 < B.size(1); ++i1)
//          {
//             V(i2,j1) += B(j1,i1) * U(i1,i2);
//          }
//       }
//    }
// }

template <typename Scalar>
inline void contractT(const Tensor<2,Scalar>& B, const Tensor<2,Scalar>& U, Tensor<2,Scalar>& V)
{
   MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
   const int B_SIZE = 2;
   for (int j1 = 0; j1 < B.size(1); ++j1)
   {
      for (int i2 = 0; i2 < U.size(1)/B_SIZE*B_SIZE; i2+=B_SIZE)
      {
         V(i2  ,j1) = Scalar();
         V(i2+1,j1) = Scalar();
         // V(i2+2,j1) = Scalar();
         // V(i2+3,j1) = Scalar();
         for (int i1 = 0; i1 < B.size(0); ++i1)
         {
            V(i2  ,j1) += B(j1,i1) * U(i1,i2  );
            V(i2+1,j1) += B(j1,i1) * U(i1,i2+1);
            // V(i2+2,j1) += B(j1,i1) * U(i1,i2+2);
            // V(i2+3,j1) += B(j1,i1) * U(i1,i2+3);
         }
      }
      for (int i2 = U.size(1)/B_SIZE*B_SIZE; i2 < U.size(1); i2++)
      {
         V(i2,j1) = Scalar();
         for (int i1 = 0; i1 < B.size(0); ++i1)
         {
            V(i2,j1) += B(j1,i1) * U(i1,i2);
         }
      }
   }
}

 ///////
// 3d
// template <typename Scalar>
// inline void contract(const Tensor<2,Scalar>& B, const Tensor<3,Scalar>& U, Tensor<3,Scalar>& V)
// {
//    MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
//    for (int j1 = 0; j1 < B.size(1); ++j1)
//    {
//       for (int i3 = 0; i3 < U.size(2); ++i3)
//       {
//          for (int i2 = 0; i2 < U.size(1); ++i2)
//          {
//             V(i2,i3,j1) = Scalar();
//             for (int i1 = 0; i1 < B.size(0); ++i1)
//             {
//                V(i2,i3,j1) += B(i1,j1) * U(i1,i2,i3);
//             }
//          }
//       }
//    }
// }

// template <typename Scalar>
// inline void contract(const Tensor<2,Scalar>& B, const Tensor<3,Scalar>& U, Tensor<3,Scalar>& V)
// {
//    MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
//    const int B_SIZE = 2;
//    for (int j1 = 0; j1 < B.size(1); ++j1)
//    {
//       for (int i3 = 0; i3 < U.size(2); ++i3)
//       {
//          for (int i2 = 0; i2 < U.size(1); i2++)
//          {
//             Scalar tmp[2] = {Scalar(),Scalar()};
//             // Scalar tmp2 = Scalar();
//             for (int i1 = 0; i1 < B.size(0)/B_SIZE*B_SIZE; i1+=B_SIZE)
//             {
//                tmp[0] += B(i1  ,j1) * U(i1  ,i2,i3);
//                tmp[1] += B(i1+1,j1) * U(i1+1,i2,i3);
//             }
//             tmp[0] += tmp[1];
//             for (int i1 = B.size(0)/B_SIZE*B_SIZE; i1 < B.size(0); i1++)
//             {
//                 tmp[0] += B(i1,j1) * U(i1,i2,i3);
//             }
//             V(i2,i3,j1) = tmp[0];
//          }
//       }
//    }
// }

template <typename Scalar>
inline void contract(const Tensor<2,Scalar>& B, const Tensor<3,Scalar>& U, Tensor<3,Scalar>& V)
{
   MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
   const int B_SIZE = 2;
   for (int j1 = 0; j1 < B.size(1); ++j1)
   {
      for (int i3 = 0; i3 < U.size(2); ++i3)
      {
         for (int i2 = 0; i2 < U.size(1)/B_SIZE*B_SIZE; i2+=B_SIZE)
         {
            V(i2,i3,j1)   = Scalar();
            V(i2+1,i3,j1) = Scalar();
            for (int i1 = 0; i1 < B.size(0); ++i1)
            {
               V(i2,i3,j1)   += B(i1,j1) * U(i1,i2,i3);
               V(i2+1,i3,j1) += B(i1,j1) * U(i1,i2+1,i3);
            }
         }
         for (int i2 = U.size(1)/B_SIZE*B_SIZE; i2 < U.size(1); i2++)
         {
            V(i2,i3,j1)   = Scalar();
            for (int i1 = 0; i1 < B.size(0); ++i1)
            {
               V(i2,i3,j1)   += B(i1,j1) * U(i1,i2,i3);
            }
         }
      }
   }
}

// template <typename Scalar>
// inline void contractT(const Tensor<2,Scalar>& B, const Tensor<3,Scalar>& U, Tensor<3,Scalar>& V)
// {
//    MFEM_ASSERT(B.size(1)==U.size(0), "Size mismatch for contraction.");
//    for (int j1 = 0; j1 < B.size(0); ++j1)
//    {
//       for (int i3 = 0; i3 < U.size(2); ++i3)
//       {
//          for (int i2 = 0; i2 < U.size(1); ++i2)
//          {
//             V(i2,i3,j1) = Scalar();
//             for (int i1 = 0; i1 < B.size(1); ++i1)
//             {
//                V(i2,i3,j1) += B(j1,i1) * U(i1,i2,i3);
//             }
//          }
//       }
//    }
// }

template <typename Scalar>
inline void contractT(const Tensor<2,Scalar>& B, const Tensor<3,Scalar>& U, Tensor<3,Scalar>& V)
{
   MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
   const int B_SIZE = 2;
   for (int j1 = 0; j1 < B.size(1); ++j1)
   {
      for (int i3 = 0; i3 < U.size(2); ++i3)
      {
         for (int i2 = 0; i2 < U.size(1)/B_SIZE*B_SIZE; i2+=B_SIZE)
         {
            V(i2,i3,j1)   = Scalar();
            V(i2+1,i3,j1) = Scalar();
            for (int i1 = 0; i1 < B.size(0); ++i1)
            {
               V(i2,i3,j1)   += B(j1,i1) * U(i1,i2,i3);
               V(i2+1,i3,j1) += B(j1,i1) * U(i1,i2+1,i3);
            }
         }
         for (int i2 = U.size(1)/B_SIZE*B_SIZE; i2 < U.size(1); i2++)
         {
            V(i2,i3,j1)   = Scalar();
            for (int i1 = 0; i1 < B.size(0); ++i1)
            {
               V(i2,i3,j1)   += B(j1,i1) * U(i1,i2,i3);
            }
         }
      }
   }
}

  /////////////////////////
 // "Face" contractions //
/////////////////////////

 ///////
// 1d
template <typename Scalar>
inline void contractX(const Tensor<1,Scalar>& B, const Tensor<1,Scalar>& U, Scalar& V)
{
   MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
   V = Scalar();
   for (int i = 0; i < B.size(0); ++i)
   {
      V += B(i) * U(i);
   }   
}

template <typename Scalar>
inline void contractTX(const Tensor<1,Scalar>& B, const Scalar& U, Tensor<1,Scalar>& V)
{
   for (int i = 0; i < B.size(0); ++i)
   {
      V(i) = B(i) * U;
   }   
}

 ///////
// 2d
template <typename Scalar>
inline void contractX(const Tensor<2,Scalar>& B, const Tensor<2,Scalar>& U, Tensor<1,Scalar>& V)
{
   MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
   for (int i2 = 0; i2 < U.size(1); ++i2)
   {
      V(i2) = Scalar();
      for (int i1 = 0; i1 < B.size(0); ++i1)
      {
         V(i2) += B(i1,0) * U(i1,i2);
      }
   }
}

template <typename Scalar>
inline void contractTX(const Tensor<2,Scalar>& B, const Tensor<1,Scalar>& U, Tensor<2,Scalar>& V)
{
   for (int i2 = 0; i2 < U.size(0); ++i2)
   {
      for (int i1 = 0; i1 < B.size(0); ++i1)
      {
         V(i1,i2) += B(i1,0) * U(i2);
      }
   }
}

// template <typename Scalar>
// inline void contractY(const Tensor<2,Scalar>& B, const Tensor<2,Scalar>& U, Tensor<1,Scalar>& V)
// {
//    MFEM_ASSERT(B.size(0)==U.size(1), "Size mismatch for contraction.");
//    V.zero();
//    for (int i2 = 0; i2 < U.size(1); ++i2)
//    {
//       for (int i1 = 0; i1 < B.size(0); ++i1)
//       {
//          // V(i1) += B(i2,0) * U(i1,i2);
//          V(i1) += B[i2] * U(i1,i2);
//       }
//    }
// }

template <typename Scalar>
inline void contractY(const Tensor<2,Scalar>& B, const Tensor<2,Scalar>& U, Tensor<1,Scalar>& V)
{
   MFEM_ASSERT(B.size(0)==U.size(1), "Size mismatch for contraction.");
   
   for (int i1 = 0; i1 < B.size(0); ++i1)
   {
      V(i1) = Scalar();
      for (int i2 = 0; i2 < U.size(1); ++i2)
      {
         // V(i1) += B(i2,0) * U(i1,i2);
         V(i1) += B[i2] * U(i1,i2);
      }
   }
}

template <typename Scalar>
inline void contractTY(const Tensor<2,Scalar>& B, const Tensor<1,Scalar>& U, Tensor<2,Scalar>& V)
{
   for (int i2 = 0; i2 < B.size(0); ++i2)
   {
      for (int i1 = 0; i1 < U.size(0); ++i1)
      {
         // V(i1,i2) += B(i2,0) * U(i1);
         V(i1,i2) += B[i2] * U(i1);
      }
   }
}

 ///////
// 3d
template <typename Scalar>
inline void contractX(const Tensor<1,Scalar>& B, const Tensor<3,Scalar>& U, Tensor<2,Scalar>& V)
{
   MFEM_ASSERT(B.size(0)==U.size(0), "Size mismatch for contraction.");
   for (int i3 = 0; i3 < U.size(2); ++i3)
   {
      for (int i2 = 0; i2 < U.size(1); ++i2)
      {
         V(i2,i3) = Scalar();
         for (int i1 = 0; i1 < B.size(0); ++i1)
         {
            V(i2,i3) += B(i1) * U(i1,i2,i3);
         }
      }
   }
}

template <typename Scalar>
inline void contractTX(const Tensor<1,Scalar>& B, const Tensor<2,Scalar>& U, Tensor<3,Scalar>& V)
{
   for (int i3 = 0; i3 < U.size(1); ++i3)
   {
      for (int i2 = 0; i2 < U.size(0); ++i2)
      {
         for (int i1 = 0; i1 < B.size(0); ++i1)
         {
            V(i1,i2,i3) = B(i1) * U(i2,i3);
         }
      }
   }
}

template <typename Scalar>
inline void contractY(const Tensor<1,Scalar>& B, const Tensor<3,Scalar>& U, Tensor<2,Scalar>& V)
{
   MFEM_ASSERT(B.size(0)==U.size(1), "Size mismatch for contraction.");
   V.zero();
   for (int i3 = 0; i3 < U.size(2); ++i3)
   {
      for (int i2 = 0; i2 < U.size(1); ++i2)
      {
         for (int i1 = 0; i1 < B.size(0); ++i1)
         {
            V(i1,i3) += B(i2) * U(i1,i2,i3);
         }
      }
   }
}

template <typename Scalar>
inline void contractTY(const Tensor<1,Scalar>& B, const Tensor<2,Scalar>& U, Tensor<3,Scalar>& V)
{
   for (int i3 = 0; i3 < U.size(1); ++i3)
   {
      for (int i2 = 0; i2 < B.size(0); ++i2)
      {
         for (int i1 = 0; i1 < U.size(0); ++i1)
         {
            V(i1,i2,i3) = B(i2) * U(i1,i3);
         }
      }
   }
}

template <typename Scalar>
inline void contractZ(const Tensor<1,Scalar>& B, const Tensor<3,Scalar>& U, Tensor<2,Scalar>& V)
{
   MFEM_ASSERT(B.size(0)==U.size(2), "Size mismatch for contraction.");
   V.zero();
   for (int i3 = 0; i3 < U.size(2); ++i3)
   {
      for (int i2 = 0; i2 < U.size(1); ++i2)
      {
         for (int i1 = 0; i1 < B.size(0); ++i1)
         {
            V(i1,i2) += B(i3) * U(i1,i2,i3);
         }
      }
   }
}

template <typename Scalar>
inline void contractTZ(const Tensor<1,Scalar>& B, const Tensor<2,Scalar>& U, Tensor<3,Scalar>& V)
{
   for (int i3 = 0; i3 < B.size(0); ++i3)
   {
      for (int i2 = 0; i2 < U.size(1); ++i2)
      {
         for (int i1 = 0; i1 < U.size(0); ++i1)
         {
            V(i1,i2,i3) = B(i3) * U(i1,i2);
         }
      }
   }
}
  ///////////////////////////////////////
 //  Coefficient-wise multiplication  //
///////////////////////////////////////

template <int N, typename Scalar>
inline void cWiseMult(const Tensor<N,Scalar>& D, const Tensor<N,Scalar>& U, Tensor<N,Scalar>& V)
{
   MFEM_ASSERT(D.length()==U.length() && U.length()==V.length(),"The Tensors do not contain the same number of elements.")
   for (int i = 0; i < U.length(); ++i)
   {
      V[i] = D[i]*U[i];
   }
}

template <typename Scalar>
inline void cWiseMult(const Tensor<3,Scalar>& D,
   const Tensor<2,Scalar>& BGT, const Tensor<2,Scalar>& GBT,
   Tensor<2,Scalar>& DGT)
{
   for (int i2 = 0; i2 < D.size(2); ++i2)
   {
      for (int i1 = 0; i1 < D.size(1); ++i1)
      {
         DGT(i1,i2) = D(0,i1,i2) * BGT(i1,i2)
                    + D(1,i1,i2) * GBT(i1,i2);
      }
   }
}

template <typename Scalar>
inline void cWiseMult(const Tensor<4,Scalar>& D,
   const Tensor<3,Scalar>& BBGT, const Tensor<3,Scalar>& BGBT, const Tensor<3,Scalar>& GBBT,
   Tensor<3,Scalar>& DGT)
{
   for (int i3 = 0; i3 < D.size(3); ++i3)
   {
      for (int i2 = 0; i2 < D.size(2); ++i2)
      {
         for (int i1 = 0; i1 < D.size(1); ++i1)
         {
            DGT(i1,i2,i3) = D(0,i1,i2,i3) * BBGT(i1,i2,i3)
                          + D(1,i1,i2,i3) * BGBT(i1,i2,i3)
                          + D(2,i1,i2,i3) * GBBT(i1,i2,i3);
         }
      }
   }
}

}

#endif //MFEM_DUMMYALGEBRA