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

#include <iostream>
#include <iomanip>
#include <cmath>
#include "linalg.hpp"

IterativeSolver::IterativeSolver()
   : Solver(0, true)
{
   oper = NULL;
   prec = NULL;
   max_iter = 10;
   print_level = -1;
   rel_tol = abs_tol = 0.0;
#ifdef MFEM_USE_MPI
   dot_prod_type = 0;
#endif
}

#ifdef MFEM_USE_MPI
IterativeSolver::IterativeSolver(MPI_Comm _comm)
   : Solver(0, true)
{
   oper = NULL;
   prec = NULL;
   max_iter = 10;
   print_level = -1;
   rel_tol = abs_tol = 0.0;
   dot_prod_type = 1;
   comm = _comm;
}
#endif

double IterativeSolver::Dot(const Vector &x, const Vector &y) const
{
#ifndef MFEM_USE_MPI
   return (x * y);
#else
   if (dot_prod_type == 0)
   {
      return (x * y);
   }
   else
   {
      double local_dot = (x * y);
      double global_dot;

      MPI_Allreduce(&local_dot, &global_dot, 1, MPI_DOUBLE, MPI_SUM, comm);

      return global_dot;
   }
#endif
}

void IterativeSolver::SetPrintLevel(int print_lvl)
{
#ifndef MFEM_USE_MPI
   print_level = print_lvl;
#else
   if (dot_prod_type == 0)
   {
      print_level = print_lvl;
   }
   else
   {
      int rank;
      MPI_Comm_rank(comm, &rank);
      if (rank == 0)
         print_level = print_lvl;
   }
#endif
}

void IterativeSolver::SetPreconditioner(Solver &pr)
{
   prec = &pr;
   prec->iterative_mode = false;
}

void IterativeSolver::SetOperator(const Operator &op)
{
   oper = &op;
   size = op.Size();
   if (prec)
      prec->SetOperator(*oper);
}


void CGSolver::UpdateVectors()
{
   r.SetSize(size);
   d.SetSize(size);
   z.SetSize(size);
}

void CGSolver::Mult(const Vector &b, Vector &x) const
{
   int i;
   double r0, den, nom, nom0, betanom, alpha, beta;

   if (iterative_mode)
   {
      oper->Mult(x, r);
      subtract(b, r, r); // r = b - A x
   }
   else
   {
      r = b;
      x = 0.0;
   }

   if (prec)
   {
      prec->Mult(r, z); // z = B r
      d = z;
   }
   else
   {
      d = r;
   }
   nom0 = nom = Dot(d, r);

   if (print_level == 1)
      cout << "   Iteration : " << setw(3) << 0 << "  (B r, r) = "
           << nom << endl;

   r0 = fmax(nom*rel_tol*rel_tol, abs_tol*abs_tol);
   if (nom <= r0)
   {
      converged = 1;
      final_iter = 0;
      final_norm = sqrt(nom);
      return;
   }

   oper->Mult(d, z);  // z = A d
   den = Dot(z, d);

   if (print_level >= 0 && den < 0.0)
   {
      cout << "Negative denominator in step 0 of PCG: " << den << endl;
   }

   if (den == 0.0)
   {
      converged = 0;
      final_iter = 0;
      final_norm = sqrt(nom);
      return;
   }

   // start iteration
   converged = 0;
   final_iter = max_iter;
   for (i = 1; true; )
   {
      alpha = nom/den;
      add(x,  alpha, d, x);     //  x = x + alpha d
      add(r, -alpha, z, r);     //  r = r - alpha A d

      if (prec)
      {
         prec->Mult(r, z);      //  z = B r
         betanom = Dot(r, z);
      }
      else
      {
         betanom = Dot(r, r);
      }

      if (print_level == 1)
         cout << "   Iteration : " << setw(3) << i << "  (B r, r) = "
              << betanom << endl;

      if (betanom < r0)
      {
         if (print_level == 2)
            cout << "Number of PCG iterations: " << i << endl;
         else
            if (print_level == 3)
               cout << "(B r_0, r_0) = " << nom0 << endl
                    << "(B r_N, r_N) = " << betanom << endl
                    << "Number of PCG iterations: " << i << endl;
         converged = 1;
         final_iter = i;
         break;
      }

      if (++i > max_iter)
         break;

      beta = betanom/nom;
      if (prec)
         add(z, beta, d, d);  //  d = z + beta d
      else
         add(r, beta, d, d);
      oper->Mult(d, z);       //  z = A d
      den = Dot(d, z);
      if (den <= 0.0)
      {
         if (print_level >= 0 && Dot(d, d) > 0.0)
            cout <<"PCG: The operator is not postive definite. (Ad, d) = "
                 << den << endl;
      }
      nom = betanom;
   }
   if (print_level >= 0 && !converged)
   {
      cerr << "PCG: No convergence!" << endl;
      cout << "(B r_0, r_0) = " << nom0 << endl
           << "(B r_N, r_N) = " << betanom << endl
           << "Number of PCG iterations: " << final_iter << endl;
   }
   if (print_level >= 1 || (print_level >= 0 && !converged))
   {
      cout << "Average reduction factor = "
           << pow (betanom/nom0, 0.5/final_iter) << endl;
   }
   final_norm = sqrt(betanom);
}

void CG(const Operator &A, const Vector &b, Vector &x,
        int print_iter, int max_num_iter,
        double RTOLERANCE, double ATOLERANCE)
{
   CGSolver cg;
   cg.SetPrintLevel(print_iter);
   cg.SetMaxIter(max_num_iter);
   cg.SetRelTol(sqrt(RTOLERANCE));
   cg.SetAbsTol(sqrt(ATOLERANCE));
   cg.SetOperator(A);
   cg.Mult(b, x);
}

void PCG(const Operator &A, Solver &B, const Vector &b, Vector &x,
         int print_iter, int max_num_iter,
         double RTOLERANCE, double ATOLERANCE)
{
   CGSolver pcg;
   pcg.SetPrintLevel(print_iter);
   pcg.SetMaxIter(max_num_iter);
   pcg.SetRelTol(sqrt(RTOLERANCE));
   pcg.SetAbsTol(sqrt(ATOLERANCE));
   pcg.SetOperator(A);
   pcg.SetPreconditioner(B);
   pcg.Mult(b, x);
}


inline void GeneratePlaneRotation(double &dx, double &dy,
                                  double &cs, double &sn)
{
   if (dy == 0.0)
   {
      cs = 1.0;
      sn = 0.0;
   }
   else if (fabs(dy) > fabs(dx))
   {
      double temp = dx / dy;
      sn = 1.0 / sqrt( 1.0 + temp*temp );
      cs = temp * sn;
   }
   else
   {
      double temp = dy / dx;
      cs = 1.0 / sqrt( 1.0 + temp*temp );
      sn = temp * cs;
   }
}

inline void ApplyPlaneRotation(double &dx, double &dy, double &cs, double &sn)
{
   double temp = cs * dx + sn * dy;
   dy = -sn * dx + cs * dy;
   dx = temp;
}

inline void Update(Vector &x, int k, DenseMatrix &h, Vector &s,
                   Array<Vector*> &v)
{
   Vector y(s);

   // Backsolve:
   for (int i = k; i >= 0; i--)
   {
      y(i) /= h(i,i);
      for (int j = i - 1; j >= 0; j--)
         y(j) -= h(j,i) * y(i);
   }

   for (int j = 0; j <= k; j++)
      x.Add(y(j), *v[j]);
}

void GMRESSolver::Mult(const Vector &b, Vector &x) const
{
   // Generalized Minimum Residual method following the algorithm
   // on p. 20 of the SIAM Templates book.

   int n = size;

   DenseMatrix H(m+1, m);
   Vector s(m+1), cs(m+1), sn(m+1);
   Vector r(n), w(n);

   double resid;
   int i, j, k;

   if (iterative_mode)
      oper->Mult(x, r);
   else
      x = 0.0;

   if (prec)
   {
      if (iterative_mode)
      {
         subtract(b, r, w);
         prec->Mult(w, r);    // r = M (b - A x)
      }
      else
         prec->Mult(b, r);
   }
   else
   {
      if (iterative_mode)
         subtract(b, r, r);
      else
         r = b;
   }
   double beta = Norm(r);  // beta = ||r||

   final_norm = fmax(rel_tol*beta, abs_tol);

   if (beta <= final_norm)
   {
      final_norm = beta;
      final_iter = 0;
      converged = 1;
      return;
   }

   if (print_level >= 0)
      cout << "   Pass : " << setw(2) << 1
           << "   Iteration : " << setw(3) << 0
           << "  ||B r|| = " << beta << endl;

   Array<Vector *> v(m+1);
   v = NULL;

   for (j = 1; j <= max_iter; )
   {
      if (v[0] == NULL)  v[0] = new Vector(n);
      v[0]->Set(1.0/beta, r);
      s = 0.0; s(0) = beta;

      for (i = 0; i < m && j <= max_iter; i++, j++)
      {
         if (prec)
         {
            oper->Mult(*v[i], r);
            prec->Mult(r, w);        // w = M A v[i]
         }
         else
         {
            oper->Mult(*v[i], w);
         }

         for (k = 0; k <= i; k++)
         {
            H(k,i) = Dot(w, *v[k]);  // H(k,i) = w * v[k]
            w.Add(-H(k,i), *v[k]);   // w -= H(k,i) * v[k]
         }

         H(i+1,i) = Norm(w);           // H(i+1,i) = ||w||
         if (v[i+1] == NULL)  v[i+1] = new Vector(n);
         v[i+1]->Set(1.0/H(i+1,i), w); // v[i+1] = w / H(i+1,i)

         for (k = 0; k < i; k++)
            ApplyPlaneRotation(H(k,i), H(k+1,i), cs(k), sn(k));

         GeneratePlaneRotation(H(i,i), H(i+1,i), cs(i), sn(i));
         ApplyPlaneRotation(H(i,i), H(i+1,i), cs(i), sn(i));
         ApplyPlaneRotation(s(i), s(i+1), cs(i), sn(i));

         resid = fabs(s(i+1));
         if (print_level >= 0)
            cout << "   Pass : " << setw(2) << (j-1)/m+1
                 << "   Iteration : " << setw(3) << j
                 << "  ||B r|| = " << resid << endl;

         if (resid <= final_norm)
         {
            Update(x, i, H, s, v);
            final_norm = resid;
            final_iter = j;
            for (i = 0; i <= m; i++)
               delete v[i];
            converged = 1;
            return;
         }
      }

      if (print_level >= 0 && j <= max_iter)
         cout << "Restarting..." << endl;

      Update(x, i-1, H, s, v);

      oper->Mult(x, r);
      if (prec)
      {
         subtract(b, r, w);
         prec->Mult(w, r);    // r = M (b - A x)
      }
      else
      {
         subtract(b, r, r);
      }
      beta = Norm(r);         // beta = ||r||
      if (beta <= final_norm)
      {
         final_norm = beta;
         final_iter = j;
         for (i = 0; i <= m; i++)
            delete v[i];
         converged = 1;
         return;
      }
   }

   final_norm = beta;
   final_iter = max_iter;
   for (i = 0; i <= m; i++)
      delete v[i];
   converged = 0;
}

int GMRES(const Operator &A, Vector &x, const Vector &b, Solver &M,
          int &max_iter, int m, double &tol, double atol, int printit)
{
   GMRESSolver gmres;
   gmres.SetPrintLevel(printit);
   gmres.SetMaxIter(max_iter);
   gmres.SetKDim(m);
   gmres.SetRelTol(sqrt(tol));
   gmres.SetAbsTol(sqrt(atol));
   gmres.SetOperator(A);
   gmres.SetPreconditioner(M);
   gmres.Mult(b, x);
   max_iter = gmres.GetNumIterations();
   tol = gmres.GetFinalNorm()*gmres.GetFinalNorm();
   return gmres.GetConverged();
}

void GMRES(const Operator &A, Solver &B, const Vector &b, Vector &x,
           int print_iter, int max_num_iter, int m, double rtol, double atol)
{
   GMRES(A, x, b, B, max_num_iter, m, rtol, atol, print_iter);
}


void BiCGSTABSolver::UpdateVectors()
{
   p.SetSize(size);
   phat.SetSize(size);
   s.SetSize(size);
   shat.SetSize(size);
   t.SetSize(size);
   v.SetSize(size);
   r.SetSize(size);
   rtilde.SetSize(size);
}

void BiCGSTABSolver::Mult(const Vector &b, Vector &x) const
{
   // BiConjugate Gradient Stabilized method following the algorithm
   // on p. 27 of the SIAM Templates book.

   int i;
   double resid, tol_goal;
   double rho_1, rho_2=1.0, alpha=1.0, beta, omega=1.0;

   if (iterative_mode)
   {
      oper->Mult(x, r);
      subtract(b, r, r); // r = b - A x
   }
   else
   {
      x = 0.0;
      r = b;
   }
   rtilde = r;

   resid = Norm(r);
   if (print_level >= 0)
      cout << "   Iteration : " << setw(3) << 0
           << "   ||r|| = " << resid << endl;

   tol_goal = fmax(resid*rel_tol, abs_tol);

   if (resid <= tol_goal)
   {
      final_norm = resid;
      final_iter = 0;
      converged = 1;
      return;
   }

   for (i = 1; i <= max_iter; i++)
   {
      rho_1 = Dot(rtilde, r);
      if (rho_1 == 0)
      {
         if (print_level >= 0)
            cout << "   Iteration : " << setw(3) << i
                 << "   ||r|| = " << resid << endl;
         final_norm = resid;
         final_iter = i;
         converged = 0;
         return;
      }
      if (i == 1)
         p = r;
      else
      {
         beta = (rho_1/rho_2) * (alpha/omega);
         add(p, -omega, v, p);  //  p = p - omega * v
         add(r, beta, p, p);    //  p = r + beta * p
      }
      if (prec)
         prec->Mult(p, phat);  //  phat = M^{-1} * p
      else
         phat = p;
      oper->Mult(phat, v);     //  v = A * phat
      alpha = rho_1 / Dot(rtilde, v);
      add(r, -alpha, v, s); //  s = r - alpha * v
      resid = Norm(s);
      if (resid < tol_goal)
      {
         x.Add(alpha, phat);  //  x = x + alpha * phat
         if (print_level >= 0)
            cout << "   Iteration : " << setw(3) << i
                 << "   ||s|| = " << resid << endl;
         final_norm = resid;
         final_iter = i;
         converged = 1;
         return;
      }
      if (print_level >= 0)
         cout << "   Iteration : " << setw(3) << i
              << "   ||s|| = " << resid;
      if (prec)
         prec->Mult(s, shat);  //  shat = M^{-1} * s
      else
         shat = s;
      oper->Mult(shat, t);     //  t = A * shat
      omega = Dot(t, s) / Dot(t, t);
      x.Add(alpha, phat);   //  x += alpha * phat
      x.Add(omega, shat);   //  x += omega * shat
      add(s, -omega, t, r); //  r = s - omega * t

      rho_2 = rho_1;
      resid = Norm(r);
      if (print_level >= 0)
         cout << "   ||r|| = " << resid << endl;
      if (resid < tol_goal)
      {
         final_norm = resid;
         final_iter = i;
         converged = 1;
         return;
      }
      if (omega == 0)
      {
         final_norm = resid;
         final_iter = i;
         converged = 0;
         return;
      }
   }

   final_norm = resid;
   final_iter = max_iter;
   converged = 0;
}

int BiCGSTAB(const Operator &A, Vector &x, const Vector &b, Solver &M,
             int &max_iter, double &tol, double atol, int printit)
{
   BiCGSTABSolver bicgstab;
   bicgstab.SetPrintLevel(printit);
   bicgstab.SetMaxIter(max_iter);
   bicgstab.SetRelTol(sqrt(tol));
   bicgstab.SetAbsTol(sqrt(atol));
   bicgstab.SetOperator(A);
   bicgstab.SetPreconditioner(M);
   bicgstab.Mult(b, x);
   max_iter = bicgstab.GetNumIterations();
   tol = bicgstab.GetFinalNorm()*bicgstab.GetFinalNorm();
   return bicgstab.GetConverged();
}

void BiCGSTAB(const Operator &A, Solver &B, const Vector &b, Vector &x,
              int print_iter, int max_num_iter, double rtol, double atol)
{
   BiCGSTAB(A, x, b, B, max_num_iter, rtol, atol, print_iter);
}


void MINRESSolver::SetOperator(const Operator &op)
{
   IterativeSolver::SetOperator(op);
   v0.SetSize(size);
   v1.SetSize(size);
   w0.SetSize(size);
   w1.SetSize(size);
   q.SetSize(size);
   if (prec)
      u1.SetSize(size);
}

void MINRESSolver::Mult(const Vector &b, Vector &x) const
{
   // Based on the MINRES algorithm on p. 86, Fig. 6.9 in
   // "Iterative Krylov Methods for Large Linear Systems",
   // by Henk A. van der Vorst, 2003.
   // Extended to support an SPD preconditioner.

   int it;
   double beta, eta, gamma0, gamma1, sigma0, sigma1;
   double alpha, delta, rho1, rho2, rho3, norm_goal;
   Vector *z = (prec) ? &u1 : &v1;

   converged = 1;

   if (!iterative_mode)
   {
      v1 = b;
      x = 0.;
   }
   else
   {
      oper->Mult(x, v1);
      subtract(b, v1, v1);
   }

   if (prec)
      prec->Mult(v1, u1);
   eta = beta = sqrt(Dot(*z, v1));
   gamma0 = gamma1 = 1.;
   sigma0 = sigma1 = 0.;

   norm_goal = fmax(rel_tol*eta, abs_tol);

   if (print_level == 1 || print_level == 3)
      cout << "MINRES: iteration " << setw(3) << 0 << ": ||r||_B = "
           << eta << endl;

   if (eta <= norm_goal)
   {
      it = 0;
      goto loop_end;
   }

   for (it = 1; it <= max_iter; it++)
   {
      v1 /= beta;
      if (prec)
         u1 /= beta;
      oper->Mult(*z, q);
      alpha = Dot(*z, q);
      if (it > 1) // (v0 == 0) for (it == 1)
         q.Add(-beta, v0);
      add(q, -alpha, v1, v0);

      delta = gamma1*alpha - gamma0*sigma1*beta;
      rho3 = sigma0*beta;
      rho2 = sigma1*alpha + gamma0*gamma1*beta;
      if (!prec)
      {
         beta = Norm(v0);
      }
      else
      {
         prec->Mult(v0, q);
         beta = sqrt(Dot(v0, q));
      }
      rho1 = hypot(delta, beta);

      if (it == 1)
         w0.Set(1./rho1, *z);  // (w0 == 0) and (w1 == 0)
      else if (it == 2)
         add(1./rho1, *z, -rho2/rho1, w1, w0);  // (w0 == 0)
      else
      {
         add(-rho3/rho1, w0, -rho2/rho1, w1, w0);
         w0.Add(1./rho1, *z);
      }

      gamma0 = gamma1;
      gamma1 = delta/rho1;

      x.Add(gamma1*eta, w0);

      sigma0 = sigma1;
      sigma1 = beta/rho1;

      eta = -sigma1*eta;

      if (print_level == 1)
         cout << "MINRES: iteration " << setw(3) << it << ": ||r||_B = "
              << fabs(eta) << endl;

      if (fabs(eta) <= norm_goal)
         goto loop_end;

      if (prec)
         swap(&u1, &q);
      swap(&v0, &v1);
      swap(&w0, &w1);
   }
   converged = 0;
   it--;

loop_end:
   final_iter = it;
   final_norm = fabs(eta);

   if (print_level == 2)
      cout << "MINRES: number of iterations: " << it << endl;
   else if (print_level == 3)
      cout << "MINRES: iteration " << setw(3) << it << ": ||r||_B = "
           << fabs(eta) << endl;
#if 0
   if (print_level >= 1)
   {
      oper->Mult(x, v1);
      subtract(b, v1, v1);
      if (prec)
         prec->Mult(v1, u1);
      eta = sqrt(Dot(*z, v1));
      cout << "MINRES: iteration " << setw(3) << it << ": ||r||_B = "
           << eta << " (re-computed)" << endl;
   }
#endif
   if (!converged && print_level >= 0)
      cerr << "MINRES: No convergence!" << endl;
}

void MINRES(const Operator &A, const Vector &b, Vector &x, int print_it,
            int max_it, double rtol, double atol)
{
   MINRESSolver minres;
   minres.SetPrintLevel(print_it);
   minres.SetMaxIter(max_it);
   minres.SetRelTol(sqrt(rtol));
   minres.SetAbsTol(sqrt(atol));
   minres.SetOperator(A);
   minres.Mult(b, x);
}

void MINRES(const Operator &A, Solver &B, const Vector &b, Vector &x,
            int print_it, int max_it, double rtol, double atol)
{
   MINRESSolver minres;
   minres.SetPrintLevel(print_it);
   minres.SetMaxIter(max_it);
   minres.SetRelTol(sqrt(rtol));
   minres.SetAbsTol(sqrt(atol));
   minres.SetOperator(A);
   minres.SetPreconditioner(B);
   minres.Mult(b, x);
}


void NewtonSolver::Mult(const Vector &b, Vector &x) const
{
   int it;
   double norm, norm_goal;

   r.SetSize(size);
   c.SetSize(size);

   if (!iterative_mode)
      x = 0.0;

   oper->Mult(x, r);
   if (b.Size() == size)
      r -= b;

   norm = Norm(r);
   norm_goal = fmax(rel_tol*norm, abs_tol);

   prec->iterative_mode = false;

   // x_{i+1} = x_i - [DF(x_i)]^{-1} [F(x_i)-b]
   for (it = 0; true; it++)
   {
      if (print_level >= 0)
         cout << "Newton iteration " << setw(2) << it
              << " : ||r|| = " << norm << endl;

      if (norm <= norm_goal)
      {
         converged = 1;
         break;
      }

      if (it >= max_iter)
      {
         converged = 0;
         break;
      }

      prec->SetOperator(oper->GetGradient(x));

      prec->Mult(r, c);  // c = [DF(x_i)]^{-1} [F(x_i)-b]

      x -= c;

      oper->Mult(x, r);
      if (b.Size() == size)
         r -= b;
      norm = Norm(r);
   }

   final_iter = it;
   final_norm = norm;
}


int aGMRES(const Operator &A, Vector &x, const Vector &b,
           const Operator &M, int &max_iter,
           int m_max, int m_min, int m_step, double cf,
           double &tol, double &atol, int printit)
{
   int n = A.Size();

   int m = m_max;

   DenseMatrix H(m+1,m);
   Vector s(m+1), cs(m+1), sn(m+1);
   Vector w(n), av(n);

   double r1, resid;
   int i, j, k;

   M.Mult(b,w);
   double normb = w.Norml2(); // normb = ||M b||
   if (normb == 0.0)
      normb = 1;

   Vector r(n);
   A.Mult(x, r);
   subtract(b,r,w);
   M.Mult(w, r);           // r = M (b - A x)
   double beta = r.Norml2();  // beta = ||r||

   resid = beta / normb;

   if (resid * resid <= tol) {
      tol = resid * resid;
      max_iter = 0;
      return 0;
   }

   if (printit)
      cout << "   Pass : " << setw(2) << 1
           << "   Iteration : " << setw(3) << 0
           << "  (r, r) = " << beta*beta << endl;

   tol *= (normb*normb);
   tol = (atol > tol) ? atol : tol;

   m = m_max;
   Array<Vector *> v(m+1);
   for (i= 0; i<=m; i++) {
      v[i] = new Vector(n);
      (*v[i]) = 0.0;
   }

   j = 1;
   while (j <= max_iter) {
      (*v[0]) = 0.0;
      v[0] -> Add (1.0/beta, r);   // v[0] = r / ||r||
      s = 0.0; s(0) = beta;

      r1 = beta;

      for (i = 0; i < m && j <= max_iter; i++) {
         A.Mult((*v[i]),av);
         M.Mult(av,w);              // w = M A v[i]

         for (k = 0; k <= i; k++) {
            H(k,i) = w * (*v[k]);    // H(k,i) = w * v[k]
            w.Add(-H(k,i), (*v[k])); // w -= H(k,i) * v[k]
         }

         H(i+1,i)  = w.Norml2();     // H(i+1,i) = ||w||
         (*v[i+1]) = 0.0;
         v[i+1] -> Add (1.0/H(i+1,i), w); // v[i+1] = w / H(i+1,i)

         for (k = 0; k < i; k++)
            ApplyPlaneRotation(H(k,i), H(k+1,i), cs(k), sn(k));

         GeneratePlaneRotation(H(i,i), H(i+1,i), cs(i), sn(i));
         ApplyPlaneRotation(H(i,i), H(i+1,i), cs(i), sn(i));
         ApplyPlaneRotation(s(i), s(i+1), cs(i), sn(i));

         resid = fabs(s(i+1));
         if (printit)
            cout << "   Pass : " << setw(2) << j
                 << "   Iteration : " << setw(3) << i+1
                 << "  (r, r) = " << resid*resid << endl;

         if ( resid*resid < tol) {
            Update(x, i, H, s, v);
            tol = resid * resid;
            max_iter = j;
            for (i= 0; i<=m; i++)
               delete v[i];
            return 0;
         }
      }

      if (printit)
         cout << "Restarting..." << endl;

      Update(x, i-1, H, s, v);

      A.Mult(x, r);
      subtract(b,r,w);
      M.Mult(w, r);           // r = M (b - A x)
      beta = r.Norml2();      // beta = ||r||
      if ( resid*resid < tol) {
         tol = resid * resid;
         max_iter = j;
         for (i= 0; i<=m; i++)
            delete v[i];
         return 0;
      }

      if (beta/r1 > cf)
      {
         if (m - m_step >= m_min)
            m -= m_step;
         else
            m = m_max;
      }

      j++;
   }

   tol = resid * resid;
   for (i= 0; i<=m; i++)
      delete v[i];
   return 1;
}


// Preconditioned stationary linear iteration
void SLI(const Operator &A, const Operator &B, const Vector &b, Vector &x,
         int print_iter, int max_num_iter, double RTOLERANCE, double ATOLERANCE)
{
   int i, dim = x.Size();
   double r0, nom, nomold = 1, nom0, cf;
   Vector r(dim),  z(dim);

   r0 = -1.0;

   for(i = 1; i < max_num_iter ; i++) {
      A.Mult(x, r);         //    r = A x
      subtract(b, r, r);    //    r = b  - A x
      B.Mult(r, z);         //    z = B r

      nom = z * r;

      if (r0 == -1.0) {
         nom0 = nom;
         r0 = nom * RTOLERANCE;
         if (r0 < ATOLERANCE) r0 = ATOLERANCE;
      }

      cf = sqrt(nom/nomold);
      if (print_iter == 1) {
         cout << "   Iteration : " << setw(3) << i << "  (B r, r) = "
              << nom;
         if (i > 1)
            cout << "\tConv. rate: " << cf;
         cout << endl;
      }
      nomold = nom;

      if (nom < r0) {
         if (print_iter == 2)
            cout << "Number of iterations: " << i << endl
                 << "Conv. rate: " << cf << endl;
         else
            if (print_iter == 3)
               cout << "(B r_0, r_0) = " << nom0 << endl
                    << "(B r_N, r_N) = " << nom << endl
                    << "Number of iterations: " << i << endl;
         break;
      }

      add(x, 1.0, z, x);  //  x = x + B (b - A x)
   }

   if (i == max_num_iter)
   {
      cerr << "No convergence!" << endl;
      cout << "(B r_0, r_0) = " << nom0 << endl
           << "(B r_N, r_N) = " << nom << endl
           << "Number of iterations: " << i << endl;
   }
}

void SLBQPOptimizer::Mult(const Vector& xt, Vector& x) const
{
   int size = xt.Size();

   // Set some algorithm-specific constants and temporaries.
   int nclip   = 0;
   double l    = 0;
   double llow = 0;
   double lupp = 0;
   double lnew = 0;
   double dl   = 2;
   double r    = 0;
   double rlow = 0;
   double rupp = 0;
   double s    = 0;

   // *** Start bracketing phase of SLBQP ***

   // Solve QP with fixed Lagrange multiplier:
   add(l, w, 1.0, xt, x);
   x.median(lo, hi);
   // Compute residual:
   r = Dot(w,x)-a;
   // Increment counter:
   nclip++;

   if (r < 0)
   {
      llow = l;  rlow = r;  l = l + dl;

      // Solve QP with fixed Lagrange multiplier:
      add(l, w, 1.0, xt, x);
      x.median(lo, hi);
      // Compute residual:
      r = Dot(w,x)-a;
      // Increment counter:
      nclip++;

      while ((r < 0) && (nclip < max_iter))
      {
         llow = l;
         s = rlow/r - 1.0;
         if (s < 0.1) { s = 0.1; }
         dl = dl + dl/s;
         l = l + dl;

         // Solve QP with fixed Lagrange multiplier:
         add(l, w, 1.0, xt, x);
         x.median(lo, hi);
         // Compute residual:
         r = Dot(w,x)-a;
         // Increment counter:
         nclip++;
      }

      lupp = l;  rupp = r;
   }
   else
   {
      lupp = l;  rupp = r;  l = l - dl;

      // Solve QP with fixed Lagrange multiplier:
      add(l, w, 1.0, xt, x);
      x.median(lo, hi);
      // Compute residual:
      r = Dot(w,x)-a;
      // Increment counter:
      nclip++;

      while ((r > 0) && (nclip < max_iter))
      {
         lupp = l;
         s = rupp/r - 1.0;
         if (s < 0.1) { s = 0.1; }
         dl = dl + dl/s;
         l = l - dl;

         // Solve QP with fixed Lagrange multiplier:
         add(l, w, 1.0, xt, x);
         x.median(lo, hi);
         // Compute residual:
         r = Dot(w,x)-a;
         // Increment counter:
         nclip++;
      }

      llow = l;  rlow = r;
   }

   // *** Stop bracketing phase of SLBQP ***


   // *** Start secant phase of SLBQP ***

   s = 1.0 - rlow/rupp;  dl = dl/s;  l = lupp - dl;

   // Solve QP with fixed Lagrange multiplier:
   add(l, w, 1.0, xt, x);
   x.median(lo, hi);
   // Compute residual:
   r = Dot(w,x)-a;
   // Increment counter:
   nclip++;

   while ( ((r > abs_tol) || (-r > abs_tol)) && (nclip < max_iter) )
   {
      if (r > 0)
      {
         if (s <= 2.0)
         {
            lupp = l;  rupp = r;  s = 1.0 - rlow/rupp;
            dl = (lupp - llow)/s;  l = lupp - dl;
         }
         else
         {
            s = rupp/r - 1.0;
            if (s < 0.1) { s = 0.1; }
            dl = (lupp - l)/s;
            lnew = 0.75*llow + 0.25*l;
            if (lnew < l-dl) { lnew = l-dl; }
            lupp = l;  rupp = r;  l = lnew;
            s = (lupp - llow)/(lupp - l);
         }

      }
      else
      {
         if (s >= 2.0)
         {
            llow = l;  rlow = r;  s = 1.0 - rlow/rupp;
            dl = (lupp - llow)/s;  l = lupp - dl;
         }
         else
         {
            s = rlow/r - 1.0;
            if (s < 0.1) { s = 0.1; }
            dl = (l - llow)/s;
            lnew = 0.75*lupp + 0.25*l;
            if (lnew < l+dl) { lnew = l+dl; }
            llow = l;  rlow = r; l = lnew;
            s = (lupp - llow)/(lupp - l);
         }
      }

      // Solve QP with fixed Lagrange multiplier:
      add(l, w, 1.0, xt, x);
      x.median(lo, hi);
      // Compute residual:
      r = Dot(w,x)-a;
      // Increment counter:
      nclip++;
   }

   // *** Stop secant phase of SLBQP ***

   int print_level_now = print_level;
   if ((r > abs_tol) || (-r > abs_tol))
   {
      cerr << "SLBQP not converged!" << endl;
      print_level_now = 1;
   }

   if (print_level_now > 0)
   {
      cout << "SLBQP iterations: " << nclip << endl;
      cout << "SLBQP lamba = " << l << endl;
      cout << "SLBQP residual = " << r << endl;
   }
}

void SLBQP(Vector &x, const Vector &xt,
           const Vector &lo, const Vector &hi,
           const Vector &w, double a,
           int max_iter, double abs_tol)
{
   SLBQPOptimizer slbqp;
   slbqp.SetMaxIter(max_iter);
   slbqp.SetAbsTol(abs_tol);
   slbqp.SetBounds(lo,hi);
   slbqp.SetLinearConstraint(w, a);
   slbqp.Mult(xt,x);
}

#ifdef MFEM_USE_MPI
void SLBQP(MPI_Comm comm,
           Vector &x, const Vector &xt,
           const Vector &lo, const Vector &hi,
           const Vector &w, double a,
           int max_iter, double abs_tol)
{
   SLBQPOptimizer slbqp(comm);
   slbqp.SetMaxIter(max_iter);
   slbqp.SetAbsTol(abs_tol);
   slbqp.SetBounds(lo,hi);
   slbqp.SetLinearConstraint(w, a);
   slbqp.Mult(xt,x);
}
#endif

#ifdef MFEM_USE_SUITESPARSE

void UMFPackSolver::Init()
{
   mat = NULL;
   Numeric = NULL;
   AI = AJ = NULL;
   if (!use_long_ints)
      umfpack_di_defaults(Control);
   else
      umfpack_dl_defaults(Control);
}

void UMFPackSolver::SetOperator(const Operator &op)
{
   int *Ap, *Ai;
   void *Symbolic;
   double *Ax;

   if (Numeric)
      if (!use_long_ints)
         umfpack_di_free_numeric(&Numeric);
      else
         umfpack_dl_free_numeric(&Numeric);

   mat = const_cast<SparseMatrix *>(dynamic_cast<const SparseMatrix *>(&op));
   if (mat == NULL)
      mfem_error("UMFPackSolver::SetOperator : not a SparseMatrix!");

   // UMFPack requires that the column-indices in mat corresponding to each
   // row be sorted.
   // Generally, this will modify the ordering of the entries of mat.
   mat->SortColumnIndices();

   size = mat->Size();
   Ap = mat->GetI();
   Ai = mat->GetJ();
   Ax = mat->GetData();

   if (!use_long_ints)
   {
      int status = umfpack_di_symbolic(size, size, Ap, Ai, Ax, &Symbolic,
                                       Control, Info);
      if (status < 0)
      {
         umfpack_di_report_info(Control, Info);
         umfpack_di_report_status(Control, status);
         mfem_error("UMFPackSolver::SetOperator :"
                    " umfpack_di_symbolic() failed!");
      }

      status = umfpack_di_numeric(Ap, Ai, Ax, Symbolic, &Numeric,
                                  Control, Info);
      if (status < 0)
      {
         umfpack_di_report_info(Control, Info);
         umfpack_di_report_status(Control, status);
         mfem_error("UMFPackSolver::SetOperator :"
                    " umfpack_di_numeric() failed!");
      }
      umfpack_di_free_symbolic(&Symbolic);
   }
   else
   {
      SuiteSparse_long status;

      delete [] AJ;
      delete [] AI;
      AI = new SuiteSparse_long[size + 1];
      AJ = new SuiteSparse_long[Ap[size]];
      for (int i = 0; i <= size; i++)
         AI[i] = (SuiteSparse_long)(Ap[i]);
      for (int i = 0; i <= Ap[size]; i++)
         AJ[i] = (SuiteSparse_long)(Ai[i]);

      status = umfpack_dl_symbolic(size, size, AI, AJ, Ax, &Symbolic,
                                   Control, Info);
      if (status < 0)
      {
         umfpack_dl_report_info(Control, Info);
         umfpack_dl_report_status(Control, status);
         mfem_error("UMFPackSolver::SetOperator :"
                    " umfpack_dl_symbolic() failed!");
      }

      status = umfpack_dl_numeric(AI, AJ, Ax, Symbolic, &Numeric,
                                  Control, Info);
      if (status < 0)
      {
         umfpack_dl_report_info(Control, Info);
         umfpack_dl_report_status(Control, status);
         mfem_error("UMFPackSolver::SetOperator :"
                    " umfpack_dl_numeric() failed!");
      }
      umfpack_dl_free_symbolic(&Symbolic);
   }
}

void UMFPackSolver::Mult(const Vector &b, Vector &x) const
{
   if (mat == NULL)
      mfem_error("UMFPackSolver::Mult : matrix is not set!"
                 " Call SetOperator first!");

   if (!use_long_ints)
   {
      int status =
         umfpack_di_solve(UMFPACK_At, mat->GetI(), mat->GetJ(),
                          mat->GetData(), x, b, Numeric, Control, Info);
      umfpack_di_report_info(Control, Info);
      if (status < 0)
      {
         umfpack_di_report_status(Control, status);
         mfem_error("UMFPackSolver::Mult : umfpack_di_solve() failed!");
      }
   }
   else
   {
      SuiteSparse_long status =
         umfpack_dl_solve(UMFPACK_At, AI, AJ, mat->GetData(), x, b,
                          Numeric, Control, Info);
      umfpack_dl_report_info(Control, Info);
      if (status < 0)
      {
         umfpack_dl_report_status(Control, status);
         mfem_error("UMFPackSolver::Mult : umfpack_dl_solve() failed!");
      }
   }
}

void UMFPackSolver::MultTranspose(const Vector &b, Vector &x) const
{
   if (mat == NULL)
      mfem_error("UMFPackSolver::MultTranspose : matrix is not set!"
                 " Call SetOperator first!");

   if (!use_long_ints)
   {
      int status =
         umfpack_di_solve(UMFPACK_A, mat->GetI(), mat->GetJ(),
                          mat->GetData(), x, b, Numeric, Control, Info);
      umfpack_di_report_info(Control, Info);
      if (status < 0)
      {
         umfpack_di_report_status(Control, status);
         mfem_error("UMFPackSolver::MultTranspose :"
                    " umfpack_di_solve() failed!");
      }
   }
   else
   {
      SuiteSparse_long status =
         umfpack_dl_solve(UMFPACK_A, AI, AJ, mat->GetData(), x, b,
                          Numeric, Control, Info);
      umfpack_dl_report_info(Control, Info);
      if (status < 0)
      {
         umfpack_dl_report_status(Control, status);
         mfem_error("UMFPackSolver::MultTranspose :"
                    " umfpack_dl_solve() failed!");
      }
   }
}

UMFPackSolver::~UMFPackSolver()
{
   delete [] AJ;
   delete [] AI;
   if (Numeric)
      if (!use_long_ints)
         umfpack_di_free_numeric(&Numeric);
      else
         umfpack_dl_free_numeric(&Numeric);
}

#endif // MFEM_USE_SUITESPARSE

