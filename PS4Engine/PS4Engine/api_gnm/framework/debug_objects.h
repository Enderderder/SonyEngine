/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_DEBUG_OBJECTS_H_
#define _SCE_GNM_FRAMEWORK_DEBUG_OBJECTS_H_

#include "../toolkit/allocators.h"
#include "../toolkit/geommath/geommath.h"
#include <gnmx.h>
using namespace sce;

namespace Framework {

class Mesh;

struct DebugVertex
{
	Vector3Unaligned m_position;
	Vector4Unaligned m_color;
};

struct DebugObject
{
	DebugVertex *m_vertex;
	uint32_t m_vertices;
	uint32_t m_reserved;
};

struct DebugConstants
{
	Matrix4 m_modelViewProjection;
	Vector4 m_color;
};

struct DebugObjects
{
	Mesh *m_sphereMesh;
	Mesh *m_coneMesh;
	Mesh *m_cylinderMesh;
	enum { kDebugTriangles = 10240 };
	enum { kDebugVertices = kDebugTriangles * 3 };
	enum { kDebugObjects = 1024 };
	enum { kDebugMeshes = 1024 };
	DebugVertex    *m_debugVertex; // vertex buffers - to be located in GARLIC memory
	uint64_t        m_debugVertices;
	DebugObject     m_debugObject[kDebugObjects];
	uint64_t	    m_debugObjects;
	DebugConstants *m_debugConstants; // constant buffers - to be located in ONION memory
	Mesh           *m_debugMesh[kDebugMeshes];
	uint64_t	    m_debugMeshes;
	DebugConstants *m_debugMeshConstants;
	void Init(sce::Gnmx::Toolkit::IAllocator *allocator,Mesh *sphereMesh,Mesh *coneMesh,Mesh *cylinderMesh);
	void Init(sce::Gnmx::Toolkit::Allocators *allocators,const char *name,Mesh *sphereMesh,Mesh *coneMesh,Mesh *cylinderMesh);
	void BeginDebugMesh();
	void AddDebugTriangle(const DebugVertex& a,const DebugVertex& b,const DebugVertex& c);
	void AddDebugQuad(const DebugVertex& a,const DebugVertex& b,const DebugVertex& c,const DebugVertex& d);
	void EndDebugMesh(const Vector4 &color,const Matrix4 &modelViewProjection);

	void AddDebugTriangle(const Vector4 &color,const Vector3 &p0,const Vector3 &p1,const Vector3 &p2);
	void AddDebugQuad(const Vector4 &color,const Vector3 &p0,const Vector3 &p1,const Vector3 &p2,const Vector3 &p3);

	void AddDebugShape(Mesh *mesh,const Vector4 &color,const Matrix4 &modelViewProjection);
	void AddDebugShape(Mesh *mesh,const Vector3 &a,const Vector3 &b,float radius,const Vector4 &color,const Matrix4 &viewProjection);
	void AddDebugSphere(const Vector3 &position,float radius,const Vector4 &color,const Matrix4 &viewProjection);
	void AddDebugCylinder(const Vector3 &a,const Vector3 &b,float radius,const Vector4 &color,const Matrix4 &viewProjection);
	void AddDebugCone(const Vector3 &a,const Vector3 &b,float radius,const Vector4 &color,const Matrix4 &viewProjection);
	void AddDebugArrow(const Vector3 &a,const Vector3 &b,float lineRadius,float arrowRadius,float arrowLength,const Vector4 &color,const Matrix4 &viewProjection);
	void Clear();
	void RenderObject(sce::Gnmx::GnmxGfxContext &gfxc,uint32_t objectIndex);
	void RenderMesh(sce::Gnmx::GnmxGfxContext &gfxc,uint32_t meshIndex);
	void Render(sce::Gnmx::GnmxGfxContext &gfxc,const Matrix4 &viewProjectionMatrix);

	static void initializeWithAllocators(Gnmx::Toolkit::Allocators *allocators);
};

void BuildDebugConeMesh(Gnmx::Toolkit::IAllocator *allocator, Mesh *destMesh,float /*radius*/,float /*height*/,unsigned div);
void BuildDebugCylinderMesh(Gnmx::Toolkit::IAllocator *allocator, Mesh *destMesh,float /*radius*/,float /*height*/,unsigned div);
void BuildDebugSphereMesh(Gnmx::Toolkit::IAllocator *allocator, Mesh *destMesh,float /*radius*/,int xdiv,int ydiv);
 
}

#endif