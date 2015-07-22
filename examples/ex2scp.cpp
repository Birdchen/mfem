//                       MFEM Example 2 - Parallel Version
//                                        with Static Condensation
//
// Compile with: make ex2p
//
// Sample runs:  mpirun -np 4 ex2scp -m ../data/beam-tri.mesh
//               mpirun -np 4 ex2scp -m ../data/beam-quad.mesh
//               mpirun -np 4 ex2scp -m ../data/beam-tet.mesh
//               mpirun -np 4 ex2scp -m ../data/beam-hex.mesh
//               mpirun -np 4 ex2scp -m ../data/beam-quad-nurbs.mesh
//               mpirun -np 4 ex2scp -m ../data/beam-hex-nurbs.mesh
//
// Description:  This example code solves a simple linear elasticity problem
//               describing a multi-material cantilever beam.
//
//               Specifically, we approximate the weak form of -div(sigma(u))=0
//               where sigma(u)=lambda*div(u)*I+mu*(grad*u+u*grad) is the stress
//               tensor corresponding to displacement field u, and lambda and mu
//               are the material Lame constants. The boundary conditions are
//               u=0 on the fixed part of the boundary with attribute 1, and
//               sigma(u).n=f on the remainder with f being a constant pull down
//               vector on boundary elements with attribute 2, and zero
//               otherwise. The geometry of the domain is assumed to be as
//               follows:
//
//                                 +----------+----------+
//                    boundary --->| material | material |<--- boundary
//                    attribute 1  |    1     |    2     |     attribute 2
//                    (fixed)      +----------+----------+     (pull down)
//
//               The example demonstrates the use of high-order and NURBS vector
//               finite element spaces with the linear elasticity bilinear form,
//               meshes with curved elements, and the definition of piece-wise
//               constant and vector coefficient objects.
//
//               We recommend viewing Example 1 before viewing this example.

#include "mfem.hpp"
#include <fstream>
#include <iostream>

using namespace std;
using namespace mfem;

int main(int argc, char *argv[])
{
   // 1. Initialize MPI.
   int num_procs, myid;
   MPI_Init(&argc, &argv);
   MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
   MPI_Comm_rank(MPI_COMM_WORLD, &myid);

   // 2. Parse command-line options.
   const char *mesh_file = "../data/beam-tri.mesh";
   int order = 1;
   bool visualization = 1;
   bool byNodes = false;

   OptionsParser args(argc, argv);
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Mesh file to use.");
   args.AddOption(&order, "-o", "--order",
                  "Finite element order (polynomial degree).");
   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",
                  "--no-visualization",
                  "Enable or disable GLVis visualization.");
   args.AddOption(&byNodes, "-bn", "--by-nodes", "-bv",
                  "--by-vdim",
                  "Enable ordering by Nodes as opposed to VDim.");
   args.Parse();
   if (!args.Good())
   {
      if (myid == 0)
      {
         args.PrintUsage(cout);
      }
      MPI_Finalize();
      return 1;
   }
   if (myid == 0)
   {
      args.PrintOptions(cout);
   }

   // 3. Read the (serial) mesh from the given mesh file on all processors.  We
   //    can handle triangular, quadrilateral, tetrahedral, hexahedral, surface
   //    and volume meshes with the same code.
   Mesh *mesh;
   ifstream imesh(mesh_file);
   if (!imesh)
   {
      if (myid == 0)
      {
         cerr << "\nCan not open mesh file: " << mesh_file << '\n' << endl;
      }
      MPI_Finalize();
      return 2;
   }
   mesh = new Mesh(imesh, 1, 1);
   imesh.close();
   int dim = mesh->Dimension();

   if (mesh->attributes.Max() < 2 || mesh->bdr_attributes.Max() < 2)
   {
      if (myid == 0)
         cerr << "\nInput mesh should have at least two materials and "
              << "two boundary attributes! (See schematic in ex2.cpp)\n"
              << endl;
      MPI_Finalize();
      return 3;
   }

   // 4. Select the order of the finite element discretization space. For NURBS
   //    meshes, we increase the order by degree elevation.
   if (mesh->NURBSext && order > mesh->NURBSext->GetOrder())
   {
      mesh->DegreeElevate(order - mesh->NURBSext->GetOrder());
   }

   // 5. Refine the serial mesh on all processors to increase the resolution. In
   //    this example we do 'ref_levels' of uniform refinement. We choose
   //    'ref_levels' to be the largest number that gives a final mesh with no
   //    more than 1,000 elements.
   {
      int ref_levels =
         (int)floor(log(1000./mesh->GetNE())/log(2.)/dim);
      for (int l = 0; l < ref_levels; l++)
      {
         mesh->UniformRefinement();
      }
   }

   // 6. Define a parallel mesh by a partitioning of the serial mesh. Refine
   //    this mesh further in parallel to increase the resolution. Once the
   //    parallel mesh is defined, the serial mesh can be deleted.
   ParMesh *pmesh = new ParMesh(MPI_COMM_WORLD, *mesh);
   delete mesh;
   {
      int par_ref_levels = 1;
      for (int l = 0; l < par_ref_levels; l++)
      {
         pmesh->UniformRefinement();
      }
   }

   // 7. Define a parallel finite element space on the parallel mesh. Here we
   //    use vector finite elements, i.e. dim copies of a scalar finite element
   //    space. We use the ordering by vector dimension (the last argument of
   //    the FiniteElementSpace constructor) which is expected in the systems
   //    version of BoomerAMG preconditioner. For NURBS meshes, we use the
   //    (degree elevated) NURBS space associated with the mesh nodes.
   FiniteElementCollection *fec;
   ParFiniteElementSpace *fespace;
   if (pmesh->NURBSext)
   {
      fec = NULL;
      fespace = (ParFiniteElementSpace *)pmesh->GetNodes()->FESpace();
   }
   else
   {
      fec = new H1_FECollection(order, dim);
      if ( byNodes )
	fespace = new ParFiniteElementSpace(pmesh, fec, dim,
					    Ordering::byNODES, true);
      else
	fespace = new ParFiniteElementSpace(pmesh, fec, dim,
					    Ordering::byVDIM, true);
   }
   HYPRE_Int size = fespace->GlobalTrueVSize();
   HYPRE_Int esize = fespace->GlobalTrueExVSize();
   HYPRE_Int psize = size - esize;
   if (myid == 0)
      cout << "Number of unknowns: " << size 
	   << " (" << esize << " + " << psize << ")" << endl
           << "Assembling: " << flush;

   // 8. Set up the parallel linear form b(.) which corresponds to the
   //    right-hand side of the FEM linear system. In this case, b_i equals the
   //    boundary integral of f*phi_i where f represents a "pull down" force on
   //    the Neumann part of the boundary and phi_i are the basis functions in
   //    the finite element fespace. The force is defined by the object f, which
   //    is a vector of Coefficient objects. The fact that f is non-zero on
   //    boundary attribute 2 is indicated by the use of piece-wise constants
   //    coefficient for its last component.
   VectorArrayCoefficient f(dim);
   for (int i = 0; i < dim-1; i++)
   {
      f.Set(i, new ConstantCoefficient(0.0));
   }
   {
      Vector pull_force(pmesh->bdr_attributes.Max());
      pull_force = 0.0;
      pull_force(1) = -1.0e-2;
      f.Set(dim-1, new PWConstCoefficient(pull_force));
   }

   ParLinearForm *b = new ParLinearForm(fespace);
   b->AddBoundaryIntegrator(new VectorBoundaryLFIntegrator(f));
   if (myid == 0)
   {
      cout << "r.h.s. ... " << flush;
   }
   b->Assemble();

   // 9. Define the solution vector x as a parallel finite element grid function
   //    corresponding to fespace. Initialize x with initial guess of zero,
   //    which satisfies the boundary conditions.
   ParGridFunction x(fespace);
   x = 0.0;

   // 10. Set up the parallel bilinear form a(.,.) on the finite element space
   //     corresponding to the linear elasticity integrator with piece-wise
   //     constants coefficient lambda and mu. The boundary conditions are
   //     implemented by marking only boundary attribute 1 as essential. After
   //     serial/parallel assembly we extract the corresponding parallel matrix.
   Vector lambda(pmesh->attributes.Max());
   lambda = 1.0;
   lambda(0) = lambda(1)*50;
   PWConstCoefficient lambda_func(lambda);
   Vector mu(pmesh->attributes.Max());
   mu = 1.0;
   mu(0) = mu(1)*50;
   PWConstCoefficient mu_func(mu);

   ParBilinearForm *a = new ParBilinearForm(fespace);
   a->AddDomainIntegrator(new ElasticityIntegrator(lambda_func, mu_func));
   if (myid == 0)
   {
      cout << "matrix ... " << flush;
   }
   a->Assemble();
   a->Finalize();

   // 11. Define the parallel (hypre) matrix and vectors representing a(.,.),
   //     b(.) and the finite element approximation.
   HypreParMatrix *A = a->ParallelAssembleReduced();
   HypreParVector *B = a->RHS_R(*b);
   HypreParVector *X = x.ParallelAverage();

   Array<int> ess_bdr(pmesh->bdr_attributes.Max());
   ess_bdr = 0;
   ess_bdr[0] = 1;

   Array<int> ess_bdr_v, dof_list;
   fespace->GetEssentialExVDofs(ess_bdr,ess_bdr_v);

   for (int i = 0; i < ess_bdr_v.Size(); i++)
   {
      if (ess_bdr_v[i]) {
	int loctdof = fespace->GetLocalTExDofNumber(i);
	if ( loctdof >= 0 ) dof_list.Append(loctdof);
      }
   }

   // do the parallel elimination
   Vector * xe = NULL;
   HypreParVector * XE = NULL;

   if ( fespace->GetOrdering() == Ordering::byNODES &&
	fespace->GetVDim() > 1 )
   {
     xe = new Vector(fespace->GetExVSize());
     a->SplitExposedPrivate(x,xe,NULL);

     HypreParVector hxe(MPI_COMM_WORLD,
			fespace->GlobalExVSize(),
			xe->GetData(),
			fespace->GetExDofOffsets());

     XE = new HypreParVector(MPI_COMM_WORLD,fespace->GlobalTrueExVSize(),
			     fespace->GetTrueExDofOffsets());

     fespace->ExDof_TrueExDof_Matrix()->MultTranspose(hxe,*XE);

   }
   else
   {
     XE = new HypreParVector(MPI_COMM_WORLD,fespace->GlobalTrueExVSize(),
			     X->GetData(),fespace->GetTrueExDofOffsets());
   }

   A->EliminateRowsCols(dof_list, *XE, *B);

   if (myid == 0)
   {
      cout << "done." << endl;
   }

   // 12. Define and apply a parallel PCG solver for AX=B with the BoomerAMG
   //     preconditioner from hypre.
   HypreBoomerAMG *amg = new HypreBoomerAMG(*A);
   amg->SetSystemsOptions(dim);
   HyprePCG *pcg = new HyprePCG(*A);
   pcg->SetTol(1e-8);
   pcg->SetMaxIter(1000);
   pcg->SetPrintLevel(2);
   pcg->SetPreconditioner(*amg);
   pcg->Mult(*B, *XE);

   // 13. Extract the parallel grid function corresponding to the finite element
   //     approximation X. This is the local solution on each processor.
   if ( fespace->GetOrdering() == Ordering::byNODES &&
	fespace->GetVDim() > 1 )
   {
     HypreParVector hxe(MPI_COMM_WORLD,
			fespace->GlobalExVSize(),
			xe->GetData(),
			fespace->GetExDofOffsets());

     fespace->ExDof_TrueExDof_Matrix()->Mult(*XE,hxe);

     a->MergeExposedPrivate(&hxe,NULL,x);
   }
   else
   {
     x = *X;
   }
   a->UpdatePrivateDoFs(*b,x);

   delete a;
   delete b;
   if ( xe != NULL ) delete xe;

   // 14. For non-NURBS meshes, make the mesh curved based on the finite element
   //     space. This means that we define the mesh elements through a fespace
   //     based transformation of the reference element.  This allows us to save
   //     the displaced mesh as a curved mesh when using high-order finite
   //     element displacement field. We assume that the initial mesh (read from
   //     the file) is not higher order curved mesh compared to the chosen FE
   //     space.
   if (!pmesh->NURBSext)
   {
      pmesh->SetNodalFESpace(fespace);
   }

   // 15. Save in parallel the displaced mesh and the inverted solution (which
   //     gives the backward displacements to the original grid). This output
   //     can be viewed later using GLVis: "glvis -np <np> -m mesh -g sol".
   {
      GridFunction *nodes = pmesh->GetNodes();
      *nodes += x;
      x *= -1;

      ostringstream mesh_name, sol_name;
      mesh_name << "mesh." << setfill('0') << setw(6) << myid;
      sol_name << "sol_sc." << setfill('0') << setw(6) << myid;

      ofstream mesh_ofs(mesh_name.str().c_str());
      mesh_ofs.precision(8);
      pmesh->Print(mesh_ofs);

      ofstream sol_ofs(sol_name.str().c_str());
      sol_ofs.precision(8);
      x.Save(sol_ofs);
   }

   // 16. Send the above data by socket to a GLVis server.  Use the "n" and "b"
   //     keys in GLVis to visualize the displacements.
   if (visualization)
   {
      char vishost[] = "localhost";
      int  visport   = 19916;
      socketstream sol_sock(vishost, visport);
      sol_sock << "parallel " << num_procs << " " << myid << "\n";
      sol_sock.precision(8);
      sol_sock << "solution\n" << *pmesh << x << flush;
   }

   // 17. Free the used memory.
   delete pcg;
   delete amg;
   delete X;
   delete XE;
   delete B;
   delete A;

   if (fec)
   {
      delete fespace;
      delete fec;
   }
   delete pmesh;

   MPI_Finalize();

   return 0;
}
