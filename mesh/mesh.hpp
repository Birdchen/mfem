// Copyright (c) 2010,  Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// This file is part of the MFEM library.  See file COPYRIGHT for details.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#ifndef MFEM_MESH
#define MFEM_MESH

// Data type mesh

class FiniteElementSpace;
class GridFunction;

class Mesh
{
protected:
   int Dim;

   int NumOfVertices, NumOfElements, NumOfBdrElements;
   int NumOfEdges, NumOfFaces;

   int State, WantTwoLevelState;

   // 0 = Empty, 1 = Standard (NetGen), 2 = TrueGrid
   int meshgen;

   int c_NumOfVertices, c_NumOfElements, c_NumOfBdrElements;
   int f_NumOfVertices, f_NumOfElements, f_NumOfBdrElements;
   int c_NumOfEdges, c_NumOfFaces;
   int f_NumOfEdges, f_NumOfFaces;

   Array<Element *> elements;
   Array<Vertex> vertices;
   Array<Element *> boundary;
   Array<Element *> faces;

   class FaceInfo { public: int Elem1No, Elem2No, Elem1Inf, Elem2Inf; };
   Array<FaceInfo> faces_info;

   Table *el_to_edge;
   Table *el_to_face;
   Table *el_to_el;
   Array<int> be_to_edge;  // for 2D
   Table *bel_to_edge;     // for 3D
   Array<int> be_to_face;
   mutable Table *face_edge;
   mutable Table *edge_vertex;

   Table *c_el_to_edge, *f_el_to_edge, *c_bel_to_edge, *f_bel_to_edge;
   Array<int> fc_be_to_edge; // swapped with be_to_edge when switching state
   Table *c_el_to_face, *f_el_to_face; // for 3D two-level state
   Array<FaceInfo> fc_faces_info;      // for 3D two-level state

   IsoparametricTransformation Transformation, Transformation2;
   IsoparametricTransformation FaceTransformation;
   FaceElementTransformations FaceElemTr;

   GridFunction *Nodes;
   int own_nodes;

   void Init();

   void InitTables();

   void DeleteTables();

   /** Delete the 'el_to_el', 'face_edge' and 'edge_vertex' tables.
       Usefull in refinement methods to destroy the coarse tables. */
   void DeleteCoarseTables();

   /// Return the length of the segment from node i to node j.
   double GetLength(int i, int j) const;

   /** Compute the Jacobian of the transformation from the perfect
       reference element at the center of the element. */
   void GetElementJacobian(int i, DenseMatrix &J);

   void MarkTetMeshForRefinement();

   STable3D *GetElementToFaceTable (int ret_ftbl = 0);

   /** Red refinement. Element with index i is refined. The default
       red refinement for now is Uniform. */
   void RedRefinement(int i, const DSTable &v_to_v,
                      int *edge1, int *edge2, int *middle)
   { UniformRefinement(i, v_to_v, edge1, edge2, middle); }

   /** Green refinement. Element with index i is refined. The default
       refinement for now is Bisection. */
   void GreenRefinement(int i, const DSTable &v_to_v,
                        int *edge1, int *edge2, int *middle)
   { Bisection(i, v_to_v, edge1, edge2, middle); }

   /** Bisection. Element with index i is bisected. */
   void Bisection(int i, const DSTable &, int *, int *, int *);

   /** Bisection. Element with index i is bisected. */
   void Bisection(int i, const DSTable &, int *);

   /** Uniform Refinement. Element with index i is refined uniformly. */
   void UniformRefinement(int i, const DSTable &, int *, int *, int *);

   /** Averages the vertices with given indexes and save the result in
       vertices[result]. */
   void AverageVertices (int * indexes, int n, int result);

   /// Update the nodes of a curved mesh after refinement
   void UpdateNodes();

   /// Refine quadrilateral mesh.
   void QuadUniformRefinement();

   /// Refine hexahedral mesh.
   void HexUniformRefinement();

   /** Mark the edges in the elements, boundary elements and faces for
       refinement. */
   void MarkEdges();

   void BisectTriTrans (DenseMatrix &pointmat, Triangle *tri,
                        int child);

   void BisectTetTrans (DenseMatrix &pointmat, Tetrahedron *tet,
                        int child);

   int GetFineElemPath (int i, int j);

   int GetBisectionHierarchy (Element *E);

   FiniteElement *GetTransformationFEforElementType (int);

   void GetElementTransformation (int i, IsoparametricTransformation *ElTr);

   /// Used in GetFaceElementTransformations (...)
   void GetLocalSegToTriTransformation (IsoparametricTransformation &loc,
                                        int i);
   void GetLocalSegToQuadTransformation (IsoparametricTransformation &loc,
                                         int i);
   /// Used in GetFaceElementTransformations (...)
   void GetLocalTriToTetTransformation (IsoparametricTransformation &loc,
                                        int i);
   /// Used in GetFaceElementTransformations (...)
   void GetLocalQuadToHexTransformation (IsoparametricTransformation &loc,
                                         int i);

   /// Returns the orientation of "test" relative to "base"
   static int GetTriOrientation (const int * base, const int * test);
   /// Returns the orientation of "test" relative to "base"
   static int GetQuadOrientation (const int * base, const int * test);

   static void GetElementArrayEdgeTable(const Array<Element*> &elem_array,
                                        const DSTable &v_to_v,
                                        Table &el_to_edge);

   /** Return vertex to vertex table. The connections stored in the table
       are from smaller to bigger vertex index, i.e. if i<j and (i, j) is
       in the table, then (j, i) is not stored. */
   void GetVertexToVertexTable(DSTable &) const;

   /** Return element to edge table and the indeces for the boundary edges.
       The entries in the table are ordered according to the order of the
       nodes in the elements. For example, if T is the element to edge table
       T(i, 0) gives the index of edge in element i that connects vertex 0
       to vertex 1, etc. Returns the number of the edges. */
   int GetElementToEdgeTable(Table &, Array<int> &);

   /// Used in GenerateFaces()
   void AddSegmentFaceElement (int lf, int gf, int el, int v0, int v1);

   void AddTriangleFaceElement (int lf, int gf, int el,
                                int v0, int v1, int v2);

   void AddQuadFaceElement (int lf, int gf, int el,
                            int v0, int v1, int v2, int v3);

   void FreeElement (Element *E);

   void GenerateFaces();

public:

   enum { NORMAL, TWO_LEVEL_COARSE, TWO_LEVEL_FINE };

   Array<int> attributes;
   Array<int> bdr_attributes;

   Mesh() { Init(); InitTables(); meshgen = 0; Dim = 0; }

   Mesh (int _Dim, int NVert, int NElem, int NBdrElem = 0);
   void AddVertex (double *);
   void AddElement (Element *elem)  { elements[NumOfElements++] = elem; }
   void AddTri (int *vi, int attr = 1);
   void AddTriangle (int *vi, int attr = 1);
   void AddQuad (int *vi, int attr = 1);
   void AddTet (int *vi, int attr = 1);
   void AddHex (int *vi, int attr = 1);
   void AddBdrSegment (int *vi, int attr = 1);
   void AddBdrTriangle (int *vi, int attr = 1);
   void AddBdrQuad (int *vi, int attr = 1);
   void GenerateBoundaryElements();
   void FinalizeTriMesh (int generate_edges = 0, int refine = 0);
   void FinalizeQuadMesh (int generate_edges = 0, int refine = 0);
   void FinalizeTetMesh (int generate_edges = 0, int refine = 0);
   void FinalizeHexMesh (int generate_edges = 0, int refine = 0);

   void SetAttributes();

   /** Creates mesh for the unit square, divided into nx * ny quadrilaterals
       if type = QUADRILATERAL or into 2*nx*ny triangles if type = TRIANGLE.
       If generate_edges = 0 (default) edges are not generated, if 1 edges
       are generated. */
   Mesh(int nx, int ny, Element::Type type, int generate_edges = 0,
        double sx = 1.0, double sy = 1.0);

   /** Creates 1D mesh , divided into n equal intervals. */
   Mesh (int n);

   /** Creates mesh by reading data stream in netgen format. If
       generate_edges = 0 (default) edges are not generated, if 1 edges
       are generated. */
   Mesh ( istream &input, int generate_edges = 0, int refine = 1);

   /** Start creating a mesh by reading data stream in netgen format. If
       generate_edges = 0 (default) edges are not generated, if 1 edges
       are generated. Used with consecutive load calls. */
   Mesh ( istream &input, Vector **data, int nprocessors,
          int currentprocessor, int generate_edges = 0 );

   /** Creating a mesh from array of nprocessors input streams. The meshes
       are read in the array order. The nodes in the consecutive meshes are
       properly shifted. dim is output array of dimension nprocessors and
       gives the number of vertices in the different subdomains. The obtained
       global mesh is nonconforming between the subdomains in the sence that
       the same nodes on the boundary have different indices (from the
       different subdomains. */
   Mesh ( istream ** input, int nprocessors, int * dim);

   /* This is similar to the above mesh constructor, but here the current
      mesh is destroyed and another one created based on the data stream
      again given in netgen format. If generate_edges = 0 (default) edges
      are not generated, if 1 edges are generated. */
   void Load ( istream &input, int generate_edges = 0, int refine = 1);

   /* Continue loading a mesh - started by the constructor that takes as
      argument number of processors. */
   void Load ( istream &input, Vector **data, int nprocessors,
               int currentprocessor, int generate_edges = 0);

   void SetNodalFESpace(FiniteElementSpace *nfes);
   void SetNodalGridFunction(GridFunction *nodes);
   const FiniteElementSpace *GetNodalFESpace();

   /// Truegrid or NetGen?
   inline int MeshGenerator() { return meshgen; }

   /// Returns number of vertices.
   inline int GetNV() const {return NumOfVertices;}

   /// Returns number of elements.
   inline int GetNE() const {return NumOfElements;}

   /// Returns number of boundary elements.
   inline int GetNBE() const {return NumOfBdrElements;}

   /// Return the number of edges.
   inline int GetNEdges () const { return NumOfEdges;}

   inline int GetNFaces () const { return NumOfFaces; };

   /// Equals 1 + num_holes - num_loops
   inline int  EulerNumber () const
   { return NumOfVertices - NumOfEdges + NumOfFaces - NumOfElements; };
   /// Equals 1 - num_holes
   inline int  EulerNumber2D () const
   { return NumOfVertices - NumOfEdges + NumOfElements; };

   int Dimension() const { return Dim; };

   /// Return pointer to vertex i's coordinates
   const double *GetVertex(int i) const { return vertices[i](); }
   double *GetVertex(int i) { return vertices[i](); }

   const Element *GetElement (int i) const { return elements[i]; };

   Element *GetElement (int i) { return elements[i]; };

   const Element *GetBdrElement (int i) const { return boundary[i]; };

   const Element *GetFace (int i) const { return faces[i]; };

   int GetFaceBaseGeometry(int i) const;

   int GetElementBaseGeometry(int i) const
   { return elements[i]->GetGeometryType(); };

   int GetBdrElementBaseGeometry(int i) const
   { return boundary[i]->GetGeometryType(); };

   /// Returns the indices of the dofs of element i.
   void GetElementVertices ( int i, Array<int> &dofs ) const
   { elements[i] -> GetVertices (dofs); }

   /// Returns the indices of the dofs of boundary element i.
   void GetBdrElementVertices ( int i, Array<int> &dofs ) const
   { boundary[i]->GetVertices( dofs ); }

   /// Return the indices and the orientations of all edges of element i.
   void GetElementEdges (int i, Array<int> &, Array<int> &) const;

   /// Return the indices and the orientations of all edges of bdr element i.
   void GetBdrElementEdges(int i, Array<int> &, Array<int> &) const;

   /// Return the indices and the orientations of all edges of face i.
   void GetFaceEdges(int i, Array<int> &, Array<int> &) const;

   /// Returns the indices of the vertices of face i.
   void GetFaceVertices (int i, Array<int> &vert) const
   { faces[i] -> GetVertices (vert); }

   /// Returns the indices of the vertices of edge i.
   void GetEdgeVertices (int i, Array<int> &vert) const;

   /// Returns the face-to-edge Table (3D)
   Table *GetFaceEdgeTable() const;

   /// Returns the edge-to-vertex Table (3D)
   Table *GetEdgeVertexTable() const;

   /// Return the indices and the orientations of all faces of element i.
   void GetElementFaces (int i, Array<int> &, Array<int> &) const;

   /// Return the index and the orientation of the face of bdr element i. (3D)
   void GetBdrElementFace (int i, int *, int *) const;

   /** Return the edge index of boundary element i. (2D)
       return the face index of boundary element i. (3D) */
   int GetBdrElementEdgeIndex (int i) const;

   /// Returns the type of element i.
   int GetElementType (int i) const;

   /// Returns the type of boundary element i.
   int GetBdrElementType (int i) const;

   /* Return point matrix of element i of dimension Dim X #dofs, where for
      every degree of freedom we give its coordinates in space of dimension
      Dim. */
   void GetPointMatrix( int i, DenseMatrix &pointmat ) const;

   /* Return point matrix of boundary element i of dimension Dim X #dofs,
      where for every degree of freedom we give its coordinates in space
      of dimension Dim. */
   void GetBdrPointMatrix( int i, DenseMatrix &pointmat ) const;

   /// Returns the transformation defining the i-th element
   ElementTransformation * GetElementTransformation (int i);

   /** Return the transformation defining the i-th element assuming
       the position of the vertices/nodes are given by 'nodes'. */
   void GetElementTransformation (int i, const Vector &nodes,
                                  IsoparametricTransformation *ElTr);

   /// Returns the transformation defining the i-th boundary element
   ElementTransformation * GetBdrElementTransformation(int i);

   /** Returns (a poiter to a structure containing) the following data:
       1) Elem1No - the index of the first element that contains this face
       this is the element that has the same outward unit normal
       vector as the face;
       2) Elem2No - the index of the second element that contains this face
       this element has outward unit normal vector as the face multiplied
       with -1;
       3) Elem1, Elem2 - pointers to the ElementTransformation's of the
       first and the second element respectively;
       4) Face - pointer to the ElementTransformation of the face;
       5) Loc1, Loc2 - IntegrationPointTransformation's mapping the
       face coordinate system to the element coordinate system
       (both in their reference elements). Used to transform
       IntegrationPoints from face to element.
       6) FaceGeom - the base geometry for the face. */
   FaceElementTransformations *GetFaceElementTransformations (int FaceNo);

   FaceElementTransformations *GetInteriorFaceTransformations (int FaceNo)
   { if (faces_info[FaceNo].Elem2No < 0) return NULL;
   return GetFaceElementTransformations (FaceNo); };

   FaceElementTransformations *GetBdrFaceTransformations (int BdrElemNo);

   void GetFaceElements (int Face, int *Elem1, int *Elem2);
   void GetFaceInfos (int Face, int *Inf1, int *Inf2);

   /// Check the orientation of the elements
   void CheckElementOrientation ();
   /// Check the orientation of the boundary elements
   void CheckBdrElementOrientation ();

   /// Return the attribute of element i.
   int GetAttribute(int i) const { return elements[i]->GetAttribute();}

   /// Return the attribute of boundary element i.
   int GetBdrAttribute(int i) const { return boundary[i]->GetAttribute(); }

   const Table &ElementToElementTable();

   const Table &ElementToFaceTable() const;

   const Table &ElementToEdgeTable() const;

   ///  The returned Table must be destroyed by the caller
   Table *GetVertexToElementTable();

   void CheckPartitioning (int *partitioning);

   void CheckDisplacements(const Vector &displacements, double &tmax);
   void MoveVertices(const Vector &displacements);
   void GetVertices(Vector &vert_coord) const;
   void SetVertices(const Vector &vert_coord);

   // Node operations for curved mesh.
   // They call the corresponding '...Vertices' method if the
   // mesh is not curved (i.e. Nodes == NULL).
   void MoveNodes(const Vector &displacements);
   void GetNodes(Vector &node_coord) const;
   void SetNodes(const Vector &node_coord);

   // use the provided GridFunction as Nodes
   void NewNodes(GridFunction &nodes);

   /// Refine the marked elements.
   void LocalRefinement(const Array<int> &marked_el, int type = 3);

   void UniformRefinement();

   /** Sets or clears the flag that indicates that 'LocalRefinement (...)'
       should put the mesh in two-level state. */
   void UseTwoLevelState (int use)
   {
      if (!use && State != Mesh::NORMAL)
         SetState (Mesh::NORMAL);
      WantTwoLevelState = use;
   }

   /// Change the mesh state to NORMAL, TWO_LEVEL_COARSE, TWO_LEVEL_FINE
   void SetState (int s);

   /** For a given coarse element i returns the number of
       subelements it is divided into. */
   int GetNumFineElems (int i);

   /// For a given coarse element i returns its refinement type
   int GetRefinementType (int i);

   /** For a given coarse element i and index j of one of its subelements
       return the global index of the fine element in the fine mesh. */
   int GetFineElem (int i, int j);

   /** For a given coarse element i and index j of one of its subelements
       return the transformation that transforms the fine element into the
       coordinate system of the coarse element. Clear, isn't it? :-) */
   ElementTransformation * GetFineElemTrans (int i, int j);

   /** Print the mesh in file given by f_name in netgen format. If
       f_name == NULL (default) the mesh is printed on the terminal. */
   void Print(ostream &out = cout) const;

   void GetElementColoring(Array<int> &colors, int el0 = 0);

   /** Prints the mesh with bdr elements given by the boundary of
       the subdomains, so that the boundary of subdomain i has bdr
       attribute i. */
   void PrintWithPartitioning (int *partitioning,
                               ostream &out) const;

   void PrintElementsWithPartitioning (int *partitioning,
                                       ostream &out,
                                       int interior_faces = 0);

   void ScaleSubdomains (double sf);
   void ScaleElements (double sf);

   /** Get the size of the i-th element relative to the perfect
       reference element. */
   double GetElementSize(int i, int type = 0);

   double GetElementSize(int i, const Vector &dir);

   double GetElementVolume(int i);

   void PrintCharacteristics (Vector *Vh = NULL, Vector *Vk = NULL);

   /// Destroys mesh.
   ~Mesh();
};

#endif
