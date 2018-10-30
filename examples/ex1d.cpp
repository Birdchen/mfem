
#include <mfem.hpp>
#include <fstream>
#include <iostream>

using namespace std;
using namespace mfem;

int main(int argc, char *argv[])
{
   // 1. Parse command-line options.
   const char *spec = "cpu";
   const char *mesh_file = "../data/star.mesh";
   int order = 1;
   int level = -1;
   int max_iter = 2000;
   bool static_cond = false;
   bool visualization = 1;

   OptionsParser args(argc, argv);
   args.AddOption(&spec, "-s", "--spec",
                  "Compute resource specification.");
   args.AddOption(&mesh_file, "-m", "--mesh",
                  "Mesh file to use.");
   args.AddOption(&order, "-o", "--order",
                  "Finite element order (polynomial degree) or -1 for"
                  " isoparametric space.");
   args.AddOption(&level, "-l", "--level", "Refinement level");
   args.AddOption(&max_iter, "-mi", "--max-iter",
                  "Maximum number of CG iterations");
   args.AddOption(&static_cond, "-sc", "--static-condensation", "-no-sc",
                  "--no-static-condensation", "Enable static condensation.");
   args.AddOption(&visualization, "-vis", "--visualization", "-no-vis",
                  "--no-visualization",
                  "Enable or disable GLVis visualization.");
   args.Parse();
   if (!args.Good())
   {
      args.PrintUsage(cout);
      return 1;
   }
   args.PrintOptions(cout);

#ifdef MFEM_USE_BACKENDS
   /// Engine *engine = EngineDepot.Select(spec);

   string occa_spec("mode: 'Serial'");
   //string occa_spec("mode: 'CUDA', device_id: 0");
   // string occa_spec("mode: 'OpenMP', threads: 4");
   // string occa_spec("mode: 'OpenCL', device_id: 0, platform_id: 0");

   // The following flag affects only 'Serial' and 'OpenMP' modes.
   // In 'CUDA' mode, '-O3' affects only host code.
   // In 'OpenCL' mode, adding '-O3' breaks compilation.
   // occa_spec += ", kernel: { compiler_flags: '-O3' }";

   SharedPtr<Engine> engine(new mfem::occa::Engine(occa_spec));
#endif

   // 2. Read the mesh from the given mesh file. We can handle triangular,
   //    quadrilateral, tetrahedral, hexahedral, surface and volume meshes with
   //    the same code.
   Mesh *mesh = new Mesh(mesh_file, 1, 1);
#ifdef MFEM_USE_BACKENDS
   mesh->SetEngine(*engine);
#endif
   int dim = mesh->Dimension();

   // 3. Refine the mesh to increase the resolution. In this example we do
   //    'ref_levels' of uniform refinement. We choose 'ref_levels' to be the
   //    largest number that gives a final mesh with no more than 50,000
   //    elements.
   {
      int ref_levels = level>=0 ? level :
                       (int)floor(log(50000./mesh->GetNE())/log(2.)/dim);
      for (int l = 0; l < ref_levels; l++)
      {
         mesh->UniformRefinement();
      }
   }

   // 4. Define a finite element space on the mesh. Here we use continuous
   //    Lagrange finite elements of the specified order. If order < 1, we
   //    instead use an isoparametric/isogeometric space.
   FiniteElementCollection *fec;
   if (order > 0)
   {
      fec = new H1_FECollection(order, dim);
   }
   else if (mesh->GetNodes())
   {
      fec = mesh->GetNodes()->OwnFEC();
      cout << "Using isoparametric FEs: " << fec->Name() << endl;
   }
   else
   {
      fec = new H1_FECollection(order = 1, dim);
   }
   FiniteElementSpace *fespace = new FiniteElementSpace(mesh, fec);
   cout << "Number of finite element unknowns: "
        << fespace->GetTrueVSize() << endl;

   // 5. Determine the list of true (i.e. conforming) essential boundary dofs.
   //    In this example, the boundary conditions are defined by marking all
   //    the boundary attributes from the mesh as essential (Dirichlet) and
   //    converting them to a list of true dofs.
   Array<int> ess_tdof_list;
   if (mesh->bdr_attributes.Size())
   {
      Array<int> ess_bdr(mesh->bdr_attributes.Max());
      ess_bdr = 1;
      fespace->GetEssentialTrueDofs(ess_bdr, ess_tdof_list);
   }

   // 6. Set up the linear form b(.) which corresponds to the right-hand side of
   //    the FEM linear system, which in this case is (1,phi_i) where phi_i are
   //    the basis functions in the finite element fespace.
   LinearForm *b = new LinearForm(fespace);
   ConstantCoefficient one(1.0);
   b->AddDomainIntegrator(new DomainLFIntegrator(one));
   b->Assemble();

   // 7. Define the solution vector x as a finite element grid function
   //    corresponding to fespace. Initialize x with initial guess of zero,
   //    which satisfies the boundary conditions.
   GridFunction x(fespace);
   x.Fill(0.0);

   // 8. Set up the bilinear form a(.,.) on the finite element space
   //    corresponding to the Laplacian operator -Delta, by adding the Diffusion
   //    domain integrator.
   BilinearForm *a = new BilinearForm(fespace);
   a->AddDomainIntegrator(new DiffusionIntegrator(one));

   // 9. Assemble the bilinear form and the corresponding linear system,
   //    applying any necessary transformations such as: eliminating boundary
   //    conditions, applying conforming constraints for non-conforming AMR,
   //    static condensation, etc.
   if (static_cond) { a->EnableStaticCondensation(); }

   tic_toc.Clear();
   tic_toc.Start();
   a->Assemble();

   OperatorHandle A(Operator::ANY_TYPE);
   Vector B, X;
   a->FormLinearSystem(ess_tdof_list, x, *b, A, X, B);
   
   double my_rt = tic_toc.RealTime();
   cout << "\nTotal BilinearForm time:    " << my_rt << " sec.";
   cout << "\n\"DOFs/sec\" in assembly: "
        << 1e-6*A.Ptr()->Height()/my_rt << " million.\n"
        << endl;

   cout << "Size of linear system: " << A.Ptr()->Height() << endl;

   CGSolver *cg;
   cg = new CGSolver;
   cg->SetRelTol(1e-6);
   cg->SetMaxIter(max_iter);
   cg->SetPrintLevel(3);
   cg->SetOperator(*A.Ptr());

   tic_toc.Clear();
   tic_toc.Start();
#ifndef MFEM_USE_SUITESPARSE
   cg->Mult(B, X);
#else
   // 10. If MFEM was compiled with SuiteSparse, use UMFPACK to solve the system.
   UMFPackSolver umf_solver;
   umf_solver.Control[UMFPACK_ORDERING] = UMFPACK_ORDERING_METIS;
   umf_solver.SetOperator(A.Ptr());
   umf_solver.Mult(B, X);
#endif

   my_rt = tic_toc.RealTime();
   cout << "\nTotal CG time:    " << my_rt << " sec." << endl;
   cout << "Time per CG step: "
        << my_rt / cg->GetNumIterations() << " sec." << endl;
   cout << "\n\"DOFs/sec\" in CG: "
        << 1e-6*A.Ptr()->Height()*cg->GetNumIterations()/my_rt << " million.\n"
        << endl;

   delete cg;

   // 11. Recover the solution as a finite element grid function.
   a->RecoverFEMSolution(X, *b, x);
   x.Pull();

   // 12. Save the refined mesh and the solution. This output can be viewed later
   //     using GLVis: "glvis -m refined.mesh -g sol.gf".
   ofstream mesh_ofs("refined.mesh");
   mesh_ofs.precision(8);
   mesh->Print(mesh_ofs);
   ofstream sol_ofs("sol.gf");
   sol_ofs.precision(8);
   x.Save(sol_ofs);

   // 13. Send the solution by socket to a GLVis server.
   if (visualization)
   {
      char vishost[] = "localhost";
      int  visport   = 19916;
      socketstream sol_sock(vishost, visport);
      sol_sock.precision(8);
      sol_sock << "solution\n" << *mesh << x << flush;
   }

   // 14. Free the used memory.
   delete a;
   delete b;
   delete fespace;
   if (order > 0) { delete fec; }
   delete mesh;

   return 0;
}
