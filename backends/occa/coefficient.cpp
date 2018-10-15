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
#if defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_OCCA)

#include "coefficient.hpp"
#include "bilininteg.hpp"

namespace mfem
{

namespace occa
{

//---[ Parameter ]------------
OccaParameter::~OccaParameter() {}

void OccaParameter::Setup(OccaIntegrator &integ,
                          ::occa::properties &props) {}

::occa::kernelArg OccaParameter::KernelArgs()
{
   return ::occa::kernelArg();
}
//====================================


//---[ Include Parameter ]------------
OccaIncludeParameter::OccaIncludeParameter(const std::string &filename_) :
   filename(filename_) {}

OccaParameter* OccaIncludeParameter::Clone()
{
   return new OccaIncludeParameter(filename);
}

void OccaIncludeParameter::Setup(OccaIntegrator &integ,
                                 ::occa::properties &props)
{
   props["headers"].asArray() += "#include " + filename;
}
//====================================


//---[ Source Parameter ]------------
OccaSourceParameter::OccaSourceParameter(const std::string &source_) :
   source(source_) {}

OccaParameter* OccaSourceParameter::Clone()
{
   return new OccaSourceParameter(source);
}

void OccaSourceParameter::Setup(OccaIntegrator &integ,
                                ::occa::properties &props)
{
   props["headers"].asArray() += source;
}
//====================================

//---[ Vector Parameter ]-------
OccaVectorParameter::OccaVectorParameter(const std::string &name_,
                                         Vector &v_,
                                         const bool useRestrict_) :
   name(name_),
   v(v_),
   useRestrict(useRestrict_),
   attr("") {}

OccaVectorParameter::OccaVectorParameter(const std::string &name_,
                                         Vector &v_,
                                         const std::string &attr_,
                                         const bool useRestrict_) :
   name(name_),
   v(v_),
   useRestrict(useRestrict_),
   attr(attr_) {}

OccaParameter* OccaVectorParameter::Clone()
{
   return new OccaVectorParameter(name, v, attr, useRestrict);
}

void OccaVectorParameter::Setup(OccaIntegrator &integ,
                                ::occa::properties &props)
{
   std::string &args = (props["defines/COEFF_ARGS"]
                        .asString()
                        .string());
   args += "const double *";
   if (useRestrict)
   {
      args += " restrict ";
   }
   args += name;
   if (attr.size())
   {
      args += ' ';
      args += attr;
   }
   args += ",\n";
}

::occa::kernelArg OccaVectorParameter::KernelArgs()
{
   return ::occa::kernelArg(v.OccaMem());
}
//====================================


//---[ GridFunction Parameter ]-------
OccaGridFunctionParameter::OccaGridFunctionParameter(const std::string &name_,
                                                     const Engine &e,
                                                     mfem::GridFunction &gf_,
                                                     const bool useRestrict_)
   : name(name_),
     gf(gf_),
     gfQuad(e),
     useRestrict(useRestrict_) {}

OccaParameter* OccaGridFunctionParameter::Clone()
{
   OccaGridFunctionParameter *param =
      new OccaGridFunctionParameter(name, gfQuad.OccaEngine(), gf, useRestrict);
   param->gfQuad.MakeRef(gfQuad);
   return param;
}

void OccaGridFunctionParameter::Setup(OccaIntegrator &integ,
                                      ::occa::properties &props)
{

   std::string &args = (props["defines/COEFF_ARGS"]
                        .asString()
                        .string());
   if (useRestrict)
   {
      args += "@restrict ";
   }
   args += "const double *";
   args += name;
   args += " @dim(NUM_QUAD, numElements),\n";

   FiniteElementSpace &f = gf.FESpace()->Get_PFESpace()->As<FiniteElementSpace>();
   ToQuad(integ.GetIntegrationRule(), f, gf.Get_PVector()->As<Vector>(), gfQuad);
}

::occa::kernelArg OccaGridFunctionParameter::KernelArgs()
{
   return gfQuad.OccaMem();
}
//====================================


//---[ Coefficient ]------------------
OccaCoefficient::OccaCoefficient(const Engine &e, const double value) :
   engine(&e),
   integ(NULL),
   name("COEFF")
{
   coeffValue = value;
}

OccaCoefficient::OccaCoefficient(const Engine &e, mfem::GridFunction &gf,
                                 const bool useRestrict) :
   engine(&e),
   integ(NULL),
   name("COEFF")
{
   coeffValue = "(u(q, e))";
   AddGridFunction("u", gf, useRestrict);
}

OccaCoefficient::OccaCoefficient(const Engine &e, const std::string &source) :
   engine(&e),
   integ(NULL),
   name("COEFF")
{
   coeffValue = source;
}

OccaCoefficient::OccaCoefficient(const Engine &e, const char *source) :
   engine(&e),
   integ(NULL),
   name("COEFF")
{
   coeffValue = source;
}

OccaCoefficient::OccaCoefficient(const OccaCoefficient &coeff) :
   engine(coeff.engine),
   integ(NULL),
   name(coeff.name),
   coeffValue(coeff.coeffValue)
{

   const int paramCount = (int) coeff.params.size();
   for (int i = 0; i < paramCount; ++i)
   {
      params.push_back(coeff.params[i]->Clone());
   }
}

OccaCoefficient::~OccaCoefficient()
{
   const int paramCount = (int) params.size();
   for (int i = 0; i < paramCount; ++i)
   {
      delete params[i];
   }
}

OccaCoefficient& OccaCoefficient::SetName(const std::string &name_)
{
   name = name_;
   return *this;
}

void OccaCoefficient::Setup(OccaIntegrator &integ_,
                            ::occa::properties &props_)
{
   integ = &integ_;

   const int paramCount = (int) params.size();
   props_["defines"][name + "_ARGS"] = "";
   for (int i = 0; i < paramCount; ++i)
   {
      params[i]->Setup(integ_, props_);
   }
   props_["defines"][name] = coeffValue;

   props = props_;
}

OccaCoefficient& OccaCoefficient::Add(OccaParameter *param)
{
   params.push_back(param);
   return *this;
}

OccaCoefficient& OccaCoefficient::IncludeHeader(const std::string &filename)
{
   return Add(new OccaIncludeParameter(filename));
}

OccaCoefficient& OccaCoefficient::IncludeSource(const std::string &source)
{
   return Add(new OccaSourceParameter(source));
}

OccaCoefficient& OccaCoefficient::AddVector(const std::string &name_,
                                            Vector &v,
                                            const bool useRestrict)
{
   return Add(new OccaVectorParameter(name_, v, useRestrict));
}

OccaCoefficient& OccaCoefficient::AddVector(const std::string &name_,
                                            Vector &v,
                                            const std::string &attr,
                                            const bool useRestrict)
{
   return Add(new OccaVectorParameter(name_, v, attr, useRestrict));
}

OccaCoefficient& OccaCoefficient::AddGridFunction(const std::string &name_,
                                                  mfem::GridFunction &gf,
                                                  const bool useRestrict)
{
   MFEM_ASSERT(engine->CheckVector(gf.Get_PVector()) &&
               engine->CheckFESpace(gf.FESpace()->Get_PFESpace()),
               "invalid device GridFunction");
   return Add(new OccaGridFunctionParameter(name_, *engine, gf, useRestrict));
}

bool OccaCoefficient::IsConstant()
{
   return coeffValue.isNumber();
}

double OccaCoefficient::GetConstantValue()
{
   if (!IsConstant())
   {
      mfem_error("OccaCoefficient is not constant");
   }
   return coeffValue.number();
}

Vector OccaCoefficient::Eval()
{
   if (integ == NULL)
   {
      mfem_error("OccaCoefficient requires a Setup() call before Eval()");
   }

   mfem::FiniteElementSpace &fespace = integ->GetTrialFESpace();
   const mfem::IntegrationRule &ir   = integ->GetIntegrationRule();

   const int elements = fespace.GetNE();
   const int numQuad  = ir.GetNPoints();

   Vector quadCoeff(*(new Layout(OccaEngine(), numQuad * elements)));
   Eval(quadCoeff);
   return quadCoeff;
}

void OccaCoefficient::Eval(Vector &quadCoeff)
{
   const std::string &okl_path = OccaEngine().GetOklPath();
   static ::occa::kernelBuilder builder =
      ::occa::kernelBuilder::fromFile(okl_path + "coefficient.okl",
                                      "CoefficientEval");

   if (integ == NULL)
   {
      mfem_error("OccaCoefficient requires a Setup() call before Eval()");
   }

   const int elements = integ->GetTrialFESpace().GetNE();

   ::occa::properties kernelProps = props;
   if (name != "COEFF")
   {
      kernelProps["defines/COEFF"]      = name;
      kernelProps["defines/COEFF_ARGS"] = name + "_ARGS";
   }

   ::occa::kernel evalKernel = builder.build(GetDevice(), kernelProps);
   evalKernel(elements, *this, quadCoeff.OccaMem());
}

OccaCoefficient::operator ::occa::kernelArg ()
{
   ::occa::kernelArg kArg;
   const int paramCount = (int) params.size();
   for (int i = 0; i < paramCount; ++i)
   {
      kArg.add(params[i]->KernelArgs());
   }
   return kArg;
}

} // namespace mfem::occa

} // namespace mfem

#endif // defined(MFEM_USE_BACKENDS) && defined(MFEM_USE_OCCA)
