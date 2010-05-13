// Copyright (c) 2010,  Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// This file is part of the MFEM library.  See file COPYRIGHT for details.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#ifndef MFEM_GEOM
#define MFEM_GEOM

/** Types of domains for integration rules and reference finite elements:
    Geometry::POINT    - a point
    Geometry::SEGMENT  - the interval [0,1]
    Geometry::TRIANGLE - triangle with verteces (0,0), (1,0), (0,1)
    Geometry::SQUARE   - the unit square (0,1)x(0,1)
    Geometry::TETRAHEDRON - w/ vert. (0,0,0),(1,0,0),(0,1,0),(0,0,1)
    Geometry::CUBE - the unit cube                                    */
class Geometry
{
public:
   enum Type { POINT, SEGMENT, TRIANGLE, SQUARE, TETRAHEDRON, CUBE };

   static const int NumGeom = 6;

   static const int NumBdrArray[];

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

   void GetPerfPointMat (int GeomType, DenseMatrix &pm);
   void JacToPerfJac(int GeomType, const DenseMatrix &J,
                     DenseMatrix &PJ) const;

   int NumBdr (int GeomType) { return NumBdrArray[GeomType]; }
};

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
   RefinedGeometry *RGeom[Geometry::NumGeom];
   IntegrationRule *IntPts[Geometry::NumGeom];
public:
   GeometryRefiner();

   RefinedGeometry *Refine (int Geom, int Times, int ETimes = 1);
   const IntegrationRule *RefineInterior(int Geom, int Times);

   ~GeometryRefiner();
};

extern GeometryRefiner GlobGeometryRefiner;

#endif
