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

#ifndef MFEM_GEOM
#define MFEM_GEOM

#include "../config/config.hpp"
#include "../linalg/densemat.hpp"
#include "intrules.hpp"

namespace mfem
{

/** Types of domains for integration rules and reference finite elements:
    Geometry::POINT    - a point
    Geometry::SEGMENT  - the interval [0,1]
    Geometry::TRIANGLE - triangle with vertices (0,0), (1,0), (0,1)
    Geometry::SQUARE   - the unit square (0,1)x(0,1)
    Geometry::TETRAHEDRON - w/ vert. (0,0,0),(1,0,0),(0,1,0),(0,0,1)
    Geometry::CUBE - the unit cube                                    */
class Geometry
{
public:
   enum Type { POINT, SEGMENT, TRIANGLE, SQUARE, TETRAHEDRON, CUBE };

   static const int NumGeom = 6;
   static const int NumBdrArray[];
   static const char *Name[NumGeom];
   static const double Volume[NumGeom];

   // Strcture that holds constants describing the Geometries.
   // Currently it contains just the space dimension.
   template <Type Geom> struct Constants;

private:
   IntegrationRule *GeomVert[NumGeom];
   IntegrationPoint GeomCenter[NumGeom];
   DenseMatrix *PerfGeomToGeomJac[NumGeom];

public:

   Geometry();

   ~Geometry();

   const IntegrationRule *GetVertices(int GeomType);
   const IntegrationPoint &GetCenter(int GeomType)
   { return GeomCenter[GeomType]; }

   DenseMatrix *GetPerfGeomToGeomJac(int GeomType)
   { return PerfGeomToGeomJac[GeomType]; }
   void GetPerfPointMat(int GeomType, DenseMatrix &pm);
   void JacToPerfJac(int GeomType, const DenseMatrix &J,
                     DenseMatrix &PJ) const;

   int NumBdr (int GeomType) { return NumBdrArray[GeomType]; }
};

template <> struct Geometry::Constants<Geometry::POINT>
{ static const int Dimension = 0; };
template <> struct Geometry::Constants<Geometry::SEGMENT>
{ static const int Dimension = 1; };
template <> struct Geometry::Constants<Geometry::TRIANGLE>
{ static const int Dimension = 2; };
template <> struct Geometry::Constants<Geometry::SQUARE>
{ static const int Dimension = 2; };
template <> struct Geometry::Constants<Geometry::TETRAHEDRON>
{ static const int Dimension = 3; };
template <> struct Geometry::Constants<Geometry::CUBE>
{ static const int Dimension = 3; };

extern Geometry Geometries;

class RefinedGeometry
{
public:
   int Times, ETimes;
   IntegrationRule RefPts;
   Array<int> RefGeoms, RefEdges;

   RefinedGeometry (int NPts, int NRefG, int NRefE) :
      RefPts (NPts), RefGeoms (NRefG), RefEdges (NRefE) { }
};

class GeometryRefiner
{
private:
   int type; // 0 - uniform points, otherwise - poly1d.ClosedPoints
   RefinedGeometry *RGeom[Geometry::NumGeom];
   IntegrationRule *IntPts[Geometry::NumGeom];
public:
   GeometryRefiner();

   void SetType(const int t) { type = t; }
   RefinedGeometry *Refine (int Geom, int Times, int ETimes = 1);
   const IntegrationRule *RefineInterior(int Geom, int Times);

   ~GeometryRefiner();
};

extern GeometryRefiner GlobGeometryRefiner;

}

#endif
