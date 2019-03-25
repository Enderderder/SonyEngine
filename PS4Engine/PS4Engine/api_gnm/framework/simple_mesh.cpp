/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "simple_mesh.h"
#include <float.h>
#include <stdio.h>
#include <stddef.h>
#include <unordered_map>
#include "../toolkit/dataformat_interpreter.h"
#include "../framework/framework.h"

using namespace sce;

class SimpleMeshVertex
{
public:
	Vector3Unaligned m_position;
	Vector3Unaligned m_normal;
	Vector4Unaligned m_tangent;
	Vector2Unaligned m_texture;
};

Framework::Mesh::Mesh()
: m_vertexBuffer(0)
,m_vertexBufferSize(0)
,m_vertexCount(0)
,m_vertexAttributeCount(0)
,m_indexBuffer(0)
,m_indexBufferSize(0)
,m_indexCount(0)
,m_indexType(sce::Gnm::kIndexSize16)
,m_primitiveType(sce::Gnm::kPrimitiveTypeTriList)
,m_bufferToModel(Matrix4::identity())
{
}

void Framework::Mesh::SetVertexBuffer(Gnmx::GnmxGfxContext &gfxc,Gnm::ShaderStage stage)
{
	SCE_GNM_ASSERT(m_vertexAttributeCount < kMaximumVertexBufferElements);
	gfxc.setVertexBuffers(stage,0,m_vertexAttributeCount,m_buffer);
}

Vector4 Framework::getBufferElement(sce::Gnm::Buffer buffer,uint32_t elementIndex)
{
	SCE_GNM_ASSERT_MSG(elementIndex < buffer.getNumElements(),"can't ask for element %d in a mesh that has only %d",elementIndex,buffer.getNumElements());
	const Gnm::DataFormat dataFormat = buffer.getDataFormat();
	const uint32_t stride = buffer.getStride();
	const uint8_t *src = static_cast<const uint8_t *>(buffer.getBaseAddress()) + elementIndex * stride;
	Gnmx::Toolkit::Reg32 reg32[4];
	Gnmx::Toolkit::dataFormatDecoder(reg32,static_cast<const uint32_t *>(static_cast<const void *>(src)),dataFormat);
	Vector4 value;
	memcpy(&value,reg32,sizeof(value));
	return value;
}

void Framework::setBufferElement(sce::Gnm::Buffer buffer,uint32_t elementIndex,Vector4 value)
{
	SCE_GNM_ASSERT_MSG(elementIndex < buffer.getNumElements(),"can't ask for element %d in a mesh that has only %d",elementIndex,buffer.getNumElements());
	const Gnm::DataFormat dataFormat = buffer.getDataFormat();
	const uint32_t stride = buffer.getStride();
	uint8_t *dest = static_cast<uint8_t *>(buffer.getBaseAddress()) + elementIndex * stride;
	Gnmx::Toolkit::Reg32 reg32[4];
	memcpy(reg32,&value,sizeof(value));
	uint32_t destDwords = 0;
	uint32_t temp[4];
	Gnmx::Toolkit::dataFormatEncoder(temp,&destDwords,reg32,dataFormat);
	memcpy(dest,temp,dataFormat.getBytesPerElement());
}

void Framework::getBufferElementMinMax(Vector4 *min,Vector4 *max,sce::Gnm::Buffer buffer)
{
	*min = *max = getBufferElement(buffer,0);
	for(uint32_t elementIndex = 1; elementIndex < buffer.getNumElements(); ++elementIndex)
	{
		const Vector4 element = getBufferElement(buffer,elementIndex);
		*min = minPerElem(*min,element);
		*max = maxPerElem(*max,element);
	}
}

Vector4 Framework::Mesh::getElement(uint32_t attributeIndex,uint32_t elementIndex) const
{
	SCE_GNM_ASSERT_MSG(attributeIndex < m_vertexAttributeCount,"can't ask for attribute %d in a mesh that has only %d",attributeIndex,m_vertexAttributeCount);
	const Gnm::Buffer buffer = m_buffer[attributeIndex];
	return getBufferElement(buffer,elementIndex);
}

void Framework::Mesh::setElement(uint32_t attributeIndex,uint32_t elementIndex,Vector4 value)
{
	SCE_GNM_ASSERT_MSG(attributeIndex < m_vertexAttributeCount,"can't ask for attribute %d in a mesh that has only %d",attributeIndex,m_vertexAttributeCount);
	const Gnm::Buffer buffer = m_buffer[attributeIndex];
	setBufferElement(buffer,elementIndex,value);
}

void Framework::Mesh::getMinMaxFromAttribute(Vector4 *min,Vector4 *max,uint32_t attributeIndex) const
{
	SCE_GNM_ASSERT_MSG(attributeIndex < m_vertexAttributeCount,"can't ask for attribute %d in a mesh that has only %d",attributeIndex,m_vertexAttributeCount);
	const Gnm::Buffer buffer = m_buffer[attributeIndex];
	getBufferElementMinMax(min,max,buffer);
}

void Framework::Mesh::allocate(const sce::Gnm::DataFormat *dataFormat,uint32_t attributes,uint32_t elements,uint32_t indices,sce::Gnmx::Toolkit::IAllocator* allocator)
{
	m_vertexAttributeCount = attributes;
	m_vertexCount = elements;
	m_indexCount = indices;

	m_vertexBufferSize = 0;
	for(uint32_t attribute = 0; attribute < m_vertexAttributeCount; ++attribute)
	{
		m_vertexBufferSize += dataFormat[attribute].getBytesPerElement() * m_vertexCount;
		m_vertexBufferSize = (m_vertexBufferSize + 3) & ~3;
	}
	m_indexBufferSize = indices * ((m_indexType == sce::Gnm::kIndexSize16) ? sizeof(uint16_t) : sizeof(uint32_t));

	const uint32_t bufferSize = m_vertexBufferSize + m_indexBufferSize;

	m_vertexBuffer = allocator->allocate(sce::Gnm::SizeAlign(bufferSize,sce::Gnm::kAlignmentOfBufferInBytes));
	m_indexBuffer = (uint8_t *)m_vertexBuffer + m_vertexBufferSize;

	unsigned offset = 0;
	for(uint32_t attribute = 0; attribute < m_vertexAttributeCount; ++attribute)
	{
		m_buffer[attribute].initAsVertexBuffer((uint8_t *)m_vertexBuffer + offset,dataFormat[attribute],dataFormat[attribute].getBytesPerElement(),m_vertexCount);
		offset += dataFormat[attribute].getBytesPerElement() * m_vertexCount;
		offset = (offset + 3) & ~3;
	}
}

void Framework::SetMeshVertexBufferFormat( Gnm::Buffer* buffer, Framework::SimpleMesh *destMesh, const MeshVertexBufferElement* element, uint32_t elements )
{
	while( elements-- )
	{
		switch( *element++ )
		{
		case Framework::kPosition:
			buffer->initAsVertexBuffer(static_cast<uint8_t*>(destMesh->m_vertexBuffer) + offsetof(SimpleMeshVertex,m_position), Gnm::kDataFormatR32G32B32Float, sizeof(SimpleMeshVertex), destMesh->m_vertexCount);
			break;
		case Framework::kNormal:
			buffer->initAsVertexBuffer(static_cast<uint8_t*>(destMesh->m_vertexBuffer) + offsetof(SimpleMeshVertex,m_normal), Gnm::kDataFormatR32G32B32Float, sizeof(SimpleMeshVertex), destMesh->m_vertexCount);
			break;
		case Framework::kTangent:
			buffer->initAsVertexBuffer(static_cast<uint8_t*>(destMesh->m_vertexBuffer) + offsetof(SimpleMeshVertex,m_tangent), Gnm::kDataFormatR32G32B32A32Float, sizeof(SimpleMeshVertex), destMesh->m_vertexCount);
			break;
		case Framework::kTexture:
			buffer->initAsVertexBuffer(static_cast<uint8_t*>(destMesh->m_vertexBuffer) + offsetof(SimpleMeshVertex,m_texture), Gnm::kDataFormatR32G32Float, sizeof(SimpleMeshVertex), destMesh->m_vertexCount);
			break;
		}
		buffer->setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
		++buffer;
	}
}

void Framework::SetMeshVertexBufferFormat( Framework::SimpleMesh *destMesh )
{
	const MeshVertexBufferElement element[] = { kPosition, kNormal, kTangent, kTexture };
	const uint32_t elements = sizeof(element)/sizeof(element[0]);
	SetMeshVertexBufferFormat( destMesh->m_buffer, destMesh, element, elements );
	destMesh->m_vertexAttributeCount = elements;
}


#define BUILD_MESH_OPTIMIZE_FOR_REUSE 14

void Framework::BuildTorusMesh(BuildMeshMode const eMode, Gnmx::Toolkit::Allocators* allocators, const char *name, Framework::SimpleMesh *destMesh, float outerRadius, float innerRadius, uint16_t outerQuads, uint16_t innerQuads, float outerRepeats, float innerRepeats)
{
	SCE_GNM_ASSERT( outerQuads >= 3 );
	SCE_GNM_ASSERT( innerQuads >= 3 );

	const uint32_t outerVertices = outerQuads + 1;
	const uint32_t innerVertices = innerQuads + 1;
	const uint32_t vertices = outerVertices * innerVertices;
	// If we can prime the reuse cache with the first row of vertices, the maximum number of vertices we issue as a strip 
	// before we have to jump back to a parallel strip is (BUILD_MESH_OPTIMIZE_FOR_REUSE - 1).  
	// For a 14 index reuse buffer, the reuse cache index used by each index looks like this:
	//  0--1--2--3--4--5--6--7--8--9-10-11-12 <- Primed with 7 degenerate triangles: {0,0,1}, {2,2,3}, {4,4,5}, {6,6,7}, {8,8,9}, {10,10,11}, {12,12,12} 
	//  |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |
	// 13--0--1--2--3--4--5--6--7--8--9-10-11 <- Note that starting with the second triangle, the index added by each triangle pushes an index out of the cache that was just used for the last time by the previous triangle.
	//  |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |    
	// 12-13--0--1--2--3--4--5--6--7--8--9-10 <- In each row, there are 12 quads or 24 triangles which add 13 new vertices.  The asymptotic reuse rate is 24/13 = ~1.846 prims per vertex.
	//  |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |    
	//  <-----------------------------------> We call this a "stripe" - a stack of parallel rows - a "full stripe" if it contains the number of quads which maximizes reuse, and a "maximal stripe" if it maximizes reuse with cache priming.
	const uint32_t maxQuadsInStripeForReuseWithPrimedCache = (BUILD_MESH_OPTIMIZE_FOR_REUSE - 2);
	// Dispatch draw prevents us from priming the reuse cache, because at any point during a stripe, dispatch draw might
	// cull a series of triangles, leaving the cache unprimed again.  Once the cache becomes unprimed, stripes of this
	// maximal width revert to the reuse rate of a triangle strip, about 1.0 prims per vertex:
	//  0--2--4--6--8-10-12--0--2--4--6--8-10 <- without priming the cache, we get a strip-like pattern in the reuse buffer
	//  |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |
	//  1--3--5--7--9-11-13--1--3--5--7--9-11
	// 12--0--2--4--6--8-10-12--0--2--4--6--8 <- Note that no reuse occurs against the adjacent strip, because the adjacent indices are all overwritten
	//  |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |/ |    In each row, there are now 24 triangles which add 26 new vertices, dropping the reuse rate to ~0.923 prims per vertex.
	// 13--1--3--5--7--9-11-13--1--3--5--7--9
	// The maximum stripe width which works without priming the reuse cache is (BUILD_MESH_OPTIMIZE_FOR_REUSE - 2)/2 + 1 vertices:
	//  0--2--4--6--8-10-12 <- without priming the cache, we get a strip-like pattern in the reuse buffer
	//	|/ |/ |/ |/ |/ |/ |
	//	1--3--5--7--9-11-13
	//  |/ |/ |/ |/ |/ |/ |
	//  0--1--2--3--4--5--6 <- Again, starting with the second triangle, the index added by each triangle pushes an index out of the cache that was just used for the last time by the previous triangle.
	//  |/ |/ |/ |/ |/ |/ |
	//  7--8--9-10-11-12-13 <- In each row, there are 6 quads or 12 triangles which add 7 new vertices.  The asymptotic reuse rate is 12/7 = ~1.714 prims per vertex.
	//  |/ |/ |/ |/ |/ |/ |
	//  <-----------------> A "full stripe" for dispatch draw has (BUILD_MESH_OPTIMIZE_FOR_REUSE - 2)/2 quads.
	const uint32_t innerQuadsFullStripe = 
		(eMode >= kBuildDispatchDrawOptimizedIndices) ?	maxQuadsInStripeForReuseWithPrimedCache/2 :	// dispatch draw prevents priming the reuse cache by culling triangles; optimize by traversing in the widest parallel strips that will achieve 0.5 new vertices for most triangles without priming the reuse cache. 
		(eMode >= kBuildOptimizedIndices) ? maxQuadsInStripeForReuseWithPrimedCache :				// optimize by traversing in the widest parallel stripes that will achieve 0.5 new vertices for most triangles, assuming we can prime the reuse cache.
		innerQuads;																					// just traverse the full innerQuads width of the torus.
	// To stripe the entire torus which has innerQuads x outerQuads quads into N stripes, the last of which may be less than a full stripe width:
	const uint32_t numInnerQuadsFullStripes = innerQuads / innerQuadsFullStripe;
	const uint32_t innerQuadsLastStripe = innerQuads - numInnerQuadsFullStripes*innerQuadsFullStripe;
	// I won't get into the details, here, but it turns out that priming a stripe that is smaller than "maximal" but
	// too large to work without priming takes (stripeWidth - (maximalStripeWidth-1)/2) degenerate triangles.
	// For a reuse cache of 14 indices and stripes of width 12 quads, it takes 7 triangles.  
	// For 11 quads -> 6 tris, 10 quads -> 5 tris, ..., 7 quads -> 2 tris, and for <=6 quads, no priming is necessary.
	const uint32_t trianglesToPrimeFullStripe = (eMode >= kBuildOptimizedIndices && innerQuadsFullStripe > maxQuadsInStripeForReuseWithPrimedCache/2) ? innerQuadsFullStripe - (maxQuadsInStripeForReuseWithPrimedCache -1)/2 : 0;
	const uint32_t trianglesToPrimeLastStripe = (innerQuadsLastStripe > maxQuadsInStripeForReuseWithPrimedCache/2) ? innerQuadsLastStripe - (maxQuadsInStripeForReuseWithPrimedCache - 1)/2 : 0;
	const uint32_t triangles = 2 * outerQuads * innerQuads + trianglesToPrimeFullStripe*numInnerQuadsFullStripes + trianglesToPrimeLastStripe;

	//TODO: Dispatch draw must also break up the index data into blocks for processing by compute, 
	// each of which contains no more than 256 vertices and 512 triangles.
	// Minimizing the number of index blocks is also important, and can be achieved by outputting
	// patches of 256 vertices which minimize the number of vertices shared with other patches.
	// Minimizing index blocks will necessarily trade off against maximizing vertex reuse.
	// Simple striping of width 6 quads results in index blocks with:
	// (7 x 36 + 4) vertices ~= (6 x 35 + [2:3] = [212:213]) quads = [424:426] triangles, with [84:86] vertices shared, 4 shared 4 ways.
	// This has an asymptotic unculled vertex reuse rate of ~1.704.
	//
	// The best repetitive pattern is to output square patches of 16 x 16 vertices with 
	// (16-1) x (16-1) = 225 quads or 450 triangles with 60 vertices shared with adjacent patches, 4 shared 4 ways.
	// This results in ~6% fewer index blocks but reduces the asymptotic unculled vertex reuse rate to ~1.601 prims per vertex (6.4% more vertices),
	// assuming we always continue the last full stripe from the previous block into the next block.
	//
	// As this isn't a multiple of our best 6 quad stripe width, we probably instead want to use exactly
	// 2 stripes (13 x 19 + 9) vertices ~= (12 x 18 + [7:8] = [223:224]) quads = [446:448] triangles, with [62:64] vertices shared, 4 shared 4 ways.
	// This results in ~5% fewer index blocks but reduces the asymptotic unculled vertex reuse rate to ~1.660 prims per vertex (2.6% more vertices),
	// assuming we always continue the last full stripe from the previous block into the next block.
	//
	// When tiling a finite plane, there are patterns better than the best repetitive pattern, due
	// to the fact that plane edge vertices aren't reused and because our repetitive block width
	// may not divide evenly into the plane width.

	if (eMode & 0x1) {		//BuildVertices
		destMesh->m_vertexCount = vertices;
		destMesh->m_vertexStride = sizeof(SimpleMeshVertex);
		destMesh->m_vertexAttributeCount = 4;
		destMesh->m_vertexBufferSize = destMesh->m_vertexCount * destMesh->m_vertexStride;
	}
	if (eMode > kBuildVerticesOnly) {	//BuildIndices
		destMesh->m_indexCount = triangles * 3;
		destMesh->m_indexType = Gnm::kIndexSize16;
		destMesh->m_primitiveType = Gnm::kPrimitiveTypeTriList;
		destMesh->m_indexBufferSize = destMesh->m_indexCount * sizeof(uint16_t);
	}

	if (eMode & 0x1) {		//BuildVertices
		destMesh->m_vertexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(destMesh->m_vertexBufferSize, Gnm::kAlignmentOfBufferInBytes));
		if(0 != name && Gnm::kInvalidOwnerHandle != allocators->m_owner)
		{
			char temp[Framework::kResourceNameLength];
			sprintf(temp, "%s (Vertex)", name);
			Gnm::registerResource(nullptr, allocators->m_owner, destMesh->m_vertexBuffer, destMesh->m_vertexBufferSize, temp, Gnm::kResourceTypeVertexBufferBaseAddress, 0);
		}
		SetMeshVertexBufferFormat( destMesh );
		if (destMesh->m_vertexBuffer == NULL) {
			SCE_GNM_ASSERT(destMesh->m_vertexBuffer != NULL);
			return;
		}

		SimpleMeshVertex *outV = static_cast<SimpleMeshVertex*>(destMesh->m_vertexBuffer);
		const Vector2 textureScale( outerRepeats/(outerVertices-1), innerRepeats/(innerVertices-1) );
		for (uint32_t o=0; o<outerVertices; ++o)
		{
			const float outerTheta = o * 2*M_PI / (outerVertices-1);
			const Matrix4 outerToWorld = Matrix4::rotationZ(outerTheta) * Matrix4::translation(Vector3(outerRadius, 0, 0));
			for (uint32_t i=0; i<innerVertices; ++i)
			{
				const float innerTheta = i * 2*M_PI / (innerVertices-1);
				const Matrix4 innerToOuter = Matrix4::rotationY(innerTheta) * Matrix4::translation(Vector3(innerRadius, 0, 0));
				const Matrix4 localToWorld = outerToWorld * innerToOuter;
				outV->m_position = localToWorld * Vector4(0,0,0,1);
				outV->m_normal   = localToWorld * Vector4(1,0,0,0);
				outV->m_tangent  = localToWorld * Vector4(0,1,0,0);
				outV->m_texture  = mulPerElem(Vector2((float)o, (float)i), textureScale);
				++outV;
			}
		}
		SCE_GNM_ASSERT(outV == static_cast<SimpleMeshVertex*>(destMesh->m_vertexBuffer) + vertices);
	}

	if (eMode > kBuildVerticesOnly) {	//BuildIndices
		destMesh->m_indexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(destMesh->m_indexBufferSize, Gnm::kAlignmentOfBufferInBytes));
		if(0 != name && Gnm::kInvalidOwnerHandle != allocators->m_owner)
		{
			char temp[Framework::kResourceNameLength];
			sprintf(temp, "%s (Index)", name);
			Gnm::registerResource(nullptr, allocators->m_owner, destMesh->m_indexBuffer, destMesh->m_indexBufferSize, temp, Gnm::kResourceTypeIndexBufferBaseAddress, 0);
		}
		if (destMesh->m_indexBuffer == NULL) {
			SCE_GNM_ASSERT(destMesh->m_indexBuffer != NULL);
			return;
		}

		uint16_t *outI = static_cast<uint16_t*>(destMesh->m_indexBuffer);
		uint16_t const numInnerQuadsStripes = numInnerQuadsFullStripes + (innerQuadsLastStripe > 0 ? 1 : 0);
		for (uint16_t iStripe = 0; iStripe < numInnerQuadsStripes; ++iStripe)
		{
			uint16_t const innerVertex0 = iStripe*innerQuadsFullStripe;
			uint16_t const trianglesToPrimeStripe = (iStripe < numInnerQuadsFullStripes ? trianglesToPrimeFullStripe : trianglesToPrimeLastStripe);
			uint16_t const innerQuadsStripe = (iStripe < numInnerQuadsFullStripes ? innerQuadsFullStripe : innerQuadsLastStripe);
			// prime full strip with degenerate triangles that inject { 0, 1, 2, 3, ... } into the reuse cache
			for (uint16_t iPrime = 0; iPrime < trianglesToPrimeStripe; ++iPrime) {
				outI[0] = innerVertex0 + iPrime*2;
				outI[1] = innerVertex0 + iPrime*2;
				outI[2] = innerVertex0 + iPrime*2 + 1;
				outI += 3;
			}
			if (trianglesToPrimeStripe > 0 && outI[-1] > innerVertex0 + innerQuadsFullStripe)
				outI[-1] = innerVertex0 + innerQuadsFullStripe;	// if a full strip has an even number of quads, we have to prime an odd number of vertices, and the last degenerate has only one vertex to prime
			// emit full strip 
			for (uint16_t o=0; o<outerQuads; ++o)
			{
				for (uint16_t i=0; i<innerQuadsStripe; ++i)
				{
					const uint16_t index[4] =
					{
						static_cast<uint16_t>((o+0) * innerVertices + innerVertex0 + (i+0)),
						static_cast<uint16_t>((o+0) * innerVertices + innerVertex0 + (i+1)),
						static_cast<uint16_t>((o+1) * innerVertices + innerVertex0 + (i+0)),
						static_cast<uint16_t>((o+1) * innerVertices + innerVertex0 + (i+1)),
					};
					outI[0] = index[0];
					outI[1] = index[1];
					outI[2] = index[2];
					outI[3] = index[2];
					outI[4] = index[1];
					outI[5] = index[3];
					outI += 6;
				}
			}
		}
		SCE_GNM_ASSERT(outI == static_cast<uint16_t*>(destMesh->m_indexBuffer) + triangles*3);
	}
}

void Framework::BuildTorusMesh(BuildMeshMode const eMode, Gnmx::Toolkit::IAllocator* allocator, Framework::SimpleMesh *destMesh, float outerRadius, float innerRadius, uint16_t outerQuads, uint16_t innerQuads, float outerRepeats, float innerRepeats)
{
	Gnmx::Toolkit::Allocators allocators(*allocator, *allocator);
	return BuildTorusMesh(eMode, &allocators, 0, destMesh, outerRadius, innerRadius, outerQuads, innerQuads, outerRepeats, innerRepeats);
}

void Framework::BuildSphereMesh(Gnmx::Toolkit::Allocators* allocators, const char *name, SimpleMesh *destMesh, float radius, long xdiv, long ydiv, float tx, float ty, float tz)
{
	destMesh->m_vertexCount = (xdiv + 1) * (ydiv + 1);

	destMesh->m_vertexStride = sizeof(SimpleMeshVertex);

	destMesh->m_vertexAttributeCount = 4;
	destMesh->m_indexCount = (xdiv * (ydiv - 1) * 2) * 3;
	destMesh->m_indexType = Gnm::kIndexSize16;
	destMesh->m_primitiveType = Gnm::kPrimitiveTypeTriList;

	destMesh->m_vertexBufferSize = destMesh->m_vertexCount * sizeof(SimpleMeshVertex);
	destMesh->m_indexBufferSize = destMesh->m_indexCount * sizeof(uint16_t);

	destMesh->m_vertexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(destMesh->m_vertexBufferSize, Gnm::kAlignmentOfBufferInBytes));
	destMesh->m_indexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(destMesh->m_indexBufferSize, Gnm::kAlignmentOfBufferInBytes));
	if(0 != name && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		char temp[Framework::kResourceNameLength];
		sprintf(temp, "%s (Vertex)", name);
		Gnm::registerResource(nullptr, allocators->m_owner, destMesh->m_vertexBuffer, destMesh->m_vertexBufferSize, temp, Gnm::kResourceTypeVertexBufferBaseAddress, 0);
		sprintf(temp, "%s (Index)", name);
		Gnm::registerResource(nullptr, allocators->m_owner, destMesh->m_indexBuffer, destMesh->m_indexBufferSize, temp, Gnm::kResourceTypeIndexBufferBaseAddress, 0);
	}

	memset(destMesh->m_vertexBuffer, 0, destMesh->m_vertexBufferSize);
	memset(destMesh->m_indexBuffer, 0, destMesh->m_indexBufferSize);

	SetMeshVertexBufferFormat( destMesh );

	// Everything else is just filling in the vertex and index buffer.
	SimpleMeshVertex *outV = static_cast<SimpleMeshVertex*>(destMesh->m_vertexBuffer);
	uint16_t *outI = static_cast<uint16_t*>(destMesh->m_indexBuffer);

	const float gx = 2*M_PI / xdiv;
	const float gy = M_PI / ydiv;

	for (long i = 0; i < xdiv; ++i)
	{
		const float theta = (float)i * gx;
		const float ct = cosf(theta);
		const float st = sinf(theta);

		const long k = i * (ydiv + 1);
		for (long j = 1; j < ydiv; ++j)
		{
			const float phi = (float) j * gy;
			const float sp = sinf(phi);
			const float x = ct * sp;
			const float y = st * sp;
			const float z = cosf(phi);

			outV[k+j].m_position = Vector3(x*radius+tx,y*radius+ty,z*radius+tz);
			outV[k+j].m_normal = Vector3(x,y,z);
			outV[k+j].m_texture = Vector2(theta * 0.1591549430918953f, phi * 0.31830988618379f);
		}
	}

	const long kk = xdiv * (ydiv + 1);
	for (long j = 1; j < ydiv; ++j)
	{
		const float phi = (float)j * gy;
		const float x = sinf(phi);
		const float z = cosf(phi);

		outV[kk+j].m_position = Vector3(x*radius+tx,ty,z*radius+tz);
		outV[kk+j].m_normal = Vector3(x,0,z);
		outV[kk+j].m_texture = Vector2(1,phi * 0.31830988618379f);
	}

	for (long i = 0; i < xdiv; i++)
	{
		const long k1 = i * (ydiv + 1) + 1;
		const long k2 = (i + 1) * (ydiv + 1) + 1;
		const float s = ( outV[k1].m_texture.x + outV[k2].m_texture.x ) * 0.5f;

		outV[k1-1].m_position = Vector3(tx,ty,radius+tz);
		outV[k1-1].m_normal = Vector3(0,0,1);
		outV[k1-1].m_texture = Vector2(s,0);

		outV[k1+ydiv-1].m_position = Vector3(tx,ty,-radius+tz);
		outV[k1+ydiv-1].m_normal = Vector3(0,0,-1);
		outV[k1+ydiv-1].m_texture = Vector2(s,1);
	}

	outV[xdiv*(ydiv+1)].m_position = outV[0].m_position;
	outV[xdiv*(ydiv+1)].m_normal = outV[0].m_normal;
	outV[xdiv*(ydiv+1)].m_texture = outV[0].m_texture;

	outV[xdiv*(ydiv+1)+ydiv].m_position = outV[ydiv].m_position;
	outV[xdiv*(ydiv+1)+ydiv].m_normal = outV[ydiv].m_normal;
	outV[xdiv*(ydiv+1)+ydiv].m_texture = outV[ydiv].m_texture;

	long ii = 0;
	for(long i = 0; i < xdiv; ++i)
	{
		const long k = i * (ydiv + 1);

		outI[ii+0] = (uint16_t) k;
		outI[ii+1] = (uint16_t) (k + 1);
		outI[ii+2] = (uint16_t) (k + ydiv + 2);
		ii += 3;

		for(long j = 1; j < ydiv - 1; ++j)
		{
			outI[ii+0] = (uint16_t) (k + j);
			outI[ii+1] = (uint16_t) (k + j + 1);
			outI[ii+2] = (uint16_t) (k + j + ydiv + 2);
			outI[ii+3] = (uint16_t) (k + j);
			outI[ii+4] = (uint16_t) (k + j + ydiv + 2);
			outI[ii+5] = (uint16_t) (k + j + ydiv + 1);
			ii += 6;
		}

		outI[ii+0] = (uint16_t) (k + ydiv - 1);
		outI[ii+1] = (uint16_t) (k + ydiv);
		outI[ii+2] = (uint16_t) (k + ydiv * 2);
		ii += 3;
	}

	// Double texcoords
	for(uint32_t i = 0; i < destMesh->m_vertexCount; ++i)
	{
		outV[i].m_texture = mulPerElem( ToVector2( outV[i].m_texture ), Vector2( 4.f, 2.f ) );
	}

	// Calculate tangents
	Vector3* tan1 = new Vector3[destMesh->m_vertexCount];
	Vector3* tan2 = new Vector3[destMesh->m_vertexCount];

	memset(tan1, 0, sizeof(Vector3)*destMesh->m_vertexCount);
	memset(tan2, 0, sizeof(Vector3)*destMesh->m_vertexCount);

	for(uint32_t i = 0; i < destMesh->m_indexCount/3; ++i)
	{
		const long i1 = outI[i*3+0];
		const long i2 = outI[i*3+1];
		const long i3 = outI[i*3+2];
		const Vector3 v1 = ToVector3( outV[i1].m_position );
		const Vector3 v2 = ToVector3( outV[i2].m_position );
		const Vector3 v3 = ToVector3( outV[i3].m_position );
		const Vector2 w1 = ToVector2( outV[i1].m_texture );
		const Vector2 w2 = ToVector2( outV[i2].m_texture );
		const Vector2 w3 = ToVector2( outV[i3].m_texture );

		const float x1 = v2[0] - v1[0];
		const float x2 = v3[0] - v1[0];
		const float y1 = v2[1] - v1[1];
		const float y2 = v3[1] - v1[1];
		const float z1 = v2[2] - v1[2];
		const float z2 = v3[2] - v1[2];

		const float s1 = w2[0] - w1[0];
		const float s2 = w3[0] - w1[0];
		const float t1 = w2[1] - w1[1];
		const float t2 = w3[1] - w1[1];

		const float r = 1.f / (s1*t2-s2*t1);
		const Vector3 sdir( (t2*x1-t1*x2)*r, (t2*y1-t1*y2)*r, (t2*z1-t1*z2)*r );
		const Vector3 tdir( (s1*x2-s2*x1)*r, (s1*y2-s2*y1)*r, (s1*z2-s2*z1)*r );

		tan1[i1] = tan1[i1] + sdir;
		tan1[i2] = tan1[i2] + sdir;
		tan1[i3] = tan1[i3] + sdir;
		tan2[i1] = tan2[i1] + tdir;
		tan2[i2] = tan2[i2] + tdir;
		tan2[i3] = tan2[i3] + tdir;
	}
	const long count = destMesh->m_vertexCount;
	for(long i = 0; i < count; ++i)
	{
		const Vector3 n = ToVector3( outV[i].m_normal );
		const Vector3 t = tan1[i];
		const float nDotT = n[0]*t[0] + n[1]*t[1] + n[2]*t[2];
		const Vector3 tan_a( t[0]-n[0]*nDotT, t[1]-n[1]*nDotT, t[2]-n[2]*nDotT );
        const float len = sqrtf(tan_a[0] * tan_a[0] + tan_a[1] * tan_a[1] + tan_a[2] * tan_a[2]);
        if(len > 1e-6f)
        {
		    const float ooLen = 1.f / len;
		    outV[i].m_tangent = Vector4(tan_a[0] * ooLen, tan_a[1] * ooLen, tan_a[2] * ooLen, 1);
        }
	}

	delete[] tan1;
	delete[] tan2;
}

void Framework::BuildSphereMesh(Gnmx::Toolkit::IAllocator* allocator, SimpleMesh *destMesh, float radius, long xdiv, long ydiv, float tx, float ty, float tz)
{
	Gnmx::Toolkit::Allocators allocators(*allocator, *allocator);
	return BuildSphereMesh(&allocators, 0, destMesh, radius, xdiv, ydiv, tx, ty, tz);
}

void Framework::BuildQuadMesh(Gnmx::Toolkit::Allocators* allocators, const char *name, Framework::SimpleMesh *destMesh, float size)
{
	destMesh->m_vertexCount = 4;

	destMesh->m_vertexStride = sizeof(SimpleMeshVertex);

	destMesh->m_vertexAttributeCount = 4;
	destMesh->m_indexCount = 6;
	destMesh->m_indexType = Gnm::kIndexSize16;
	destMesh->m_primitiveType = Gnm::kPrimitiveTypeTriList;

	destMesh->m_vertexBufferSize = destMesh->m_vertexCount * sizeof(SimpleMeshVertex);
	destMesh->m_indexBufferSize = destMesh->m_indexCount * sizeof(uint16_t);

	destMesh->m_vertexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(destMesh->m_vertexBufferSize, Gnm::kAlignmentOfBufferInBytes));
	destMesh->m_indexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(destMesh->m_indexBufferSize, Gnm::kAlignmentOfBufferInBytes));
	if(0 != name && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		char temp[Framework::kResourceNameLength];
		sprintf(temp, "%s (Vertex)", name);
		Gnm::registerResource(nullptr, allocators->m_owner, destMesh->m_vertexBuffer, destMesh->m_vertexBufferSize, temp, Gnm::kResourceTypeVertexBufferBaseAddress, 0);
		sprintf(temp, "%s (Index)", name);
		Gnm::registerResource(nullptr, allocators->m_owner, destMesh->m_indexBuffer, destMesh->m_indexBufferSize, temp, Gnm::kResourceTypeIndexBufferBaseAddress, 0);
	}

	memset(destMesh->m_vertexBuffer, 0, destMesh->m_vertexBufferSize);
	memset(destMesh->m_indexBuffer, 0, destMesh->m_indexBufferSize);

	SetMeshVertexBufferFormat( destMesh );

	// Everything else is just filling in the vertex and index buffer.
	SimpleMeshVertex *outV = static_cast<SimpleMeshVertex*>(destMesh->m_vertexBuffer);
	uint16_t *outI = static_cast<uint16_t*>(destMesh->m_indexBuffer);

	size *= 0.5f;
	const SimpleMeshVertex verts[4] =
	{
		{ {-size, -size, 0}, {0,0,1}, {1,0,0,1}, {0,1} },
		{ { size, -size, 0}, {0,0,1}, {1,0,0,1}, {1,1} },
		{ {-size,  size, 0}, {0,0,1}, {1,0,0,1}, {0,0} },
		{ { size,  size, 0}, {0,0,1}, {1,0,0,1}, {1,0} },
	};
	memcpy(outV, verts, 4*sizeof(SimpleMeshVertex));

	outI[0] = 0;
	outI[1] = 1;
	outI[2] = 2;
	outI[3] = 1;
	outI[4] = 3;
	outI[5] = 2;
}

void Framework::BuildQuadMesh(Gnmx::Toolkit::IAllocator* allocator, Framework::SimpleMesh *destMesh, float size)
{
	Gnmx::Toolkit::Allocators allocators(*allocator, *allocator);
	return BuildQuadMesh(&allocators, 0, destMesh, size);
}

void Framework::BuildCubeMesh(Gnmx::Toolkit::Allocators* allocators, const char *name, SimpleMesh *destMesh, float side)
{
	destMesh->m_vertexCount = 24;

	destMesh->m_vertexStride = sizeof(SimpleMeshVertex);

	destMesh->m_vertexAttributeCount = 4;
	destMesh->m_indexCount = 36;
	destMesh->m_indexType = Gnm::kIndexSize16;
	destMesh->m_primitiveType = Gnm::kPrimitiveTypeTriList;

	destMesh->m_vertexBufferSize = destMesh->m_vertexCount * sizeof(SimpleMeshVertex);
	destMesh->m_indexBufferSize = destMesh->m_indexCount * sizeof(uint16_t);

	destMesh->m_vertexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(destMesh->m_vertexBufferSize, Gnm::kAlignmentOfBufferInBytes));
	destMesh->m_indexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(destMesh->m_indexBufferSize, Gnm::kAlignmentOfBufferInBytes));
	if(0 != name && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		char temp[Framework::kResourceNameLength];
		sprintf(temp, "%s (Vertex)", name);
		Gnm::registerResource(nullptr, allocators->m_owner, destMesh->m_vertexBuffer, destMesh->m_vertexBufferSize, temp, Gnm::kResourceTypeVertexBufferBaseAddress, 0);
		sprintf(temp, "%s (Index)", name);
		Gnm::registerResource(nullptr, allocators->m_owner, destMesh->m_indexBuffer, destMesh->m_indexBufferSize, temp, Gnm::kResourceTypeIndexBufferBaseAddress, 0);
	}

	memset(destMesh->m_vertexBuffer, 0, destMesh->m_vertexBufferSize);
	memset(destMesh->m_indexBuffer, 0, destMesh->m_indexBufferSize);

	SetMeshVertexBufferFormat( destMesh );

	// Everything else is just filling in the vertex and index buffer.
	SimpleMeshVertex *outV = static_cast<SimpleMeshVertex*>(destMesh->m_vertexBuffer);
	uint16_t *outI = static_cast<uint16_t*>(destMesh->m_indexBuffer);

	const float halfSide = side*0.5f;
	const SimpleMeshVertex verts[] = {
		{{-halfSide, -halfSide, -halfSide}, { -1.0000000, 0.00000000, 0.00000000 }, { 0.00000000, 0.00000000, 1.0000000, 1.0000000 }, { 0, 0 } }, // 0
		{{-halfSide, -halfSide, -halfSide}, { 0.00000000, -1.0000000, 0.00000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 0, 0 } }, // 1
		{{-halfSide, -halfSide, -halfSide}, { 0.00000000, 0.00000000, -1.0000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 1, 0 } }, // 2
		{{-halfSide, -halfSide, halfSide}, { -1.0000000, 0.00000000, 0.00000000 }, { 0.00000000, 0.00000000, 1.0000000, 1.0000000 }, { 0, 1 } }, // 3
		{{-halfSide, -halfSide, halfSide}, { 0.00000000, -1.0000000, 0.00000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 1, 0 } }, // 4
		{{-halfSide, -halfSide, halfSide}, { 0.00000000, 0.00000000, 1.0000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 0, 0 } }, // 5
		{{-halfSide, halfSide, -halfSide}, { -1.0000000, 0.00000000, 0.00000000 }, { 0.00000000, 0.00000000, 1.0000000, 1.0000000 }, { 1, 0 } }, // 6
		{{-halfSide, halfSide, -halfSide}, { 0.00000000, 0.00000000, -1.0000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 0, 0 } }, // 7
		{{-halfSide, halfSide, -halfSide}, { 0.00000000, 1.0000000, 0.00000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 1, 0 } }, // 8
		{{-halfSide, halfSide, halfSide}, { -1.0000000, 0.00000000, 0.00000000 }, { 0.00000000, 0.00000000, 1.0000000, 1.0000000 }, { 1, 1 } }, // 9
		{{-halfSide, halfSide, halfSide}, { 0.00000000, 0.00000000, 1.0000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 1, 0 } }, // 10
		{{-halfSide, halfSide, halfSide}, { 0.00000000, 1.0000000, 0.00000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 0, 0 } }, // 11
		{{halfSide, -halfSide, -halfSide}, { 0.00000000, -1.0000000, 0.00000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 0, 1 } }, // 12
		{{halfSide, -halfSide, -halfSide}, { 0.00000000, 0.00000000, -1.0000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 1, 1 } }, // 13
		{{halfSide, -halfSide, -halfSide}, { 1.0000000, 0.00000000, 0.00000000 }, { 0.00000000, 0.00000000, -1.0000000, 1.0000000 }, { 0, 1 } }, // 14
		{{halfSide, -halfSide, halfSide}, { 0.00000000, -1.0000000, 0.00000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 1, 1 } }, // 15
		{{halfSide, -halfSide, halfSide}, { 0.00000000, 0.00000000, 1.0000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 0, 1 } }, // 16
		{{halfSide, -halfSide, halfSide}, { 1.0000000, 0.00000000, 0.00000000 }, { 0.00000000, 0.00000000, -1.0000000, 1.0000000 }, { 0, 0 } }, // 17
		{{halfSide, halfSide, -halfSide}, { 0.00000000, 0.00000000, -1.0000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 0, 1 } }, // 18
		{{halfSide, halfSide, -halfSide}, { 0.00000000, 1.0000000, 0.00000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 1, 1 } }, // 19
		{{halfSide, halfSide, -halfSide}, { 1.0000000, 0.00000000, 0.00000000 }, { 0.00000000, 0.00000000, -1.0000000, 1.0000000 }, { 1, 1 } }, // 20
		{{halfSide, halfSide, halfSide}, { 0.00000000, 0.00000000, 1.0000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 1, 1 } }, // 21
		{{halfSide, halfSide, halfSide}, { 0.00000000, 1.0000000, 0.00000000 }, { 1.0000000, 0.00000000, 0.00000000, 1.0000000 }, { 0, 1 } }, // 22
		{{halfSide, halfSide, halfSide}, { 1.0000000, 0.00000000, 0.00000000 }, { 0.00000000, 0.00000000, -1.0000000, 1.0000000 }, { 1, 0 } }, // 23
	};
	memcpy(outV, verts, sizeof(verts));

	const uint16_t indices[] = {
		5, 16, 10,		10, 16, 21,
		11, 22, 8,		8, 22, 19,
		7, 18, 2,		2, 18, 13,
		1, 12, 4,		4, 12, 15,
		17, 14, 23,		23, 14, 20,
		0, 3, 6,		6, 3, 9,
	};
	memcpy(outI, indices, sizeof(indices));
}

void Framework::BuildCubeMesh(Gnmx::Toolkit::IAllocator* allocator, SimpleMesh *destMesh, float side)
{
	Gnmx::Toolkit::Allocators allocators(*allocator, *allocator);
	return BuildCubeMesh(&allocators, 0, destMesh, side);
}

float Framework::ComputeMeshSpecificBumpScale(const SimpleMesh *srcMesh)
{
	float fMeshSpecificBumpScale = 1.0f;
	// sanity check
	if(srcMesh->m_primitiveType==Gnm::kPrimitiveTypeTriList && sizeof(SimpleMeshVertex)==srcMesh->m_vertexStride)
	{
		const int nr_triangles = srcMesh->m_indexCount/3;

		const SimpleMeshVertex * pfVerts = static_cast<const SimpleMeshVertex*>(static_cast<const void*>(srcMesh->m_vertexBuffer));
		const void * pIndices = static_cast<const void*>(srcMesh->m_indexBuffer);
		bool bIs32Bit = srcMesh->m_indexType==Gnm::kIndexSize32;

		int iNrContributions = 0;
		double dAreaRatioSum = 0;
		for(int t=0; t<nr_triangles; t++)
		{
			int i0 = bIs32Bit ? ((int *) pIndices)[t*3+0] : ((uint16_t *) pIndices)[t*3+0];
			int i1 = bIs32Bit ? ((int *) pIndices)[t*3+1] : ((uint16_t *) pIndices)[t*3+1];
			int i2 = bIs32Bit ? ((int *) pIndices)[t*3+2] : ((uint16_t *) pIndices)[t*3+2];

			// assuming position is first and a float3
			const SimpleMeshVertex &v0 = pfVerts[i0];
			const SimpleMeshVertex &v1 = pfVerts[i1];
			const SimpleMeshVertex &v2 = pfVerts[i2];

			float dPx0 = v1.m_position.x-v0.m_position.x;
			float dPy0 = v1.m_position.y-v0.m_position.y;
			float dPz0 = v1.m_position.z-v0.m_position.z;

			float dPx1 = v2.m_position.x-v0.m_position.x;
			float dPy1 = v2.m_position.y-v0.m_position.y;
			float dPz1 = v2.m_position.z-v0.m_position.z;

			float nx = dPy0*dPz1 - dPz0*dPy1;
			float ny = dPz0*dPx1 - dPx0*dPz1;
			float nz = dPx0*dPy1 - dPy0*dPx1;

			float fSurfAreaX2 = sqrtf(nx*nx+ny*ny+nz*nz);

			float dTx0 = v1.m_texture.x-v0.m_texture.x;
			float dTy0 = v1.m_texture.y-v0.m_texture.y;

			float dTx1 = v2.m_texture.x-v0.m_texture.x;
			float dTy1 = v2.m_texture.y-v0.m_texture.y;

			float fNormTexAreaX2 = fabsf( dTx0*dTy1-dTy0*dTx1 );

			if(fNormTexAreaX2>FLT_EPSILON)
			{
				dAreaRatioSum += ((double) (fSurfAreaX2/fNormTexAreaX2));
				++iNrContributions;
			}
		}

		float fAverageRatio = iNrContributions>0 ? ((float) (dAreaRatioSum/iNrContributions)) : 1.0f;
		fMeshSpecificBumpScale = sqrtf(fAverageRatio);
	}

	return fMeshSpecificBumpScale;
}

void Framework::SaveSimpleMesh( const SimpleMesh* simpleMesh, const char* filename )
{
	FILE* file;
	file = fopen( filename, "wb" );
	fwrite(&simpleMesh->m_vertexCount, 1, sizeof(simpleMesh->m_vertexCount), file);
	fwrite(&simpleMesh->m_vertexStride, 1, sizeof(simpleMesh->m_vertexStride), file);
	fwrite(&simpleMesh->m_vertexAttributeCount, 1, sizeof(simpleMesh->m_vertexAttributeCount), file);
	fwrite(&simpleMesh->m_indexCount, 1, sizeof(simpleMesh->m_indexCount), file);
	fwrite(&simpleMesh->m_indexType, 1, sizeof(simpleMesh->m_indexType), file);
	fwrite(&simpleMesh->m_primitiveType, 1, sizeof(simpleMesh->m_primitiveType), file);
	fwrite(&simpleMesh->m_indexBufferSize, 1, sizeof(simpleMesh->m_indexBufferSize), file);
	fwrite(&simpleMesh->m_vertexBufferSize, 1, sizeof(simpleMesh->m_vertexBufferSize), file);
	fwrite(simpleMesh->m_indexBuffer, simpleMesh->m_indexBufferSize, 1, file);
	fwrite(simpleMesh->m_vertexBuffer, simpleMesh->m_vertexBufferSize, 1, file);
	fclose(file);
}

void Framework::LoadSimpleMesh(Gnmx::Toolkit::Allocators* allocators, SimpleMesh* simpleMesh, const char* filename)
{
	FILE* file;
	file = fopen( filename, "rb" );
	fread(&simpleMesh->m_vertexCount, 1, sizeof(simpleMesh->m_vertexCount), file);
	fread(&simpleMesh->m_vertexStride, 1, sizeof(simpleMesh->m_vertexStride), file);
	fread(&simpleMesh->m_vertexAttributeCount, 1, sizeof(simpleMesh->m_vertexAttributeCount), file);
	fread(&simpleMesh->m_indexCount, 1, sizeof(simpleMesh->m_indexCount), file);
	fread(&simpleMesh->m_indexType, 1, sizeof(simpleMesh->m_indexType), file);
	fread(&simpleMesh->m_primitiveType, 1, sizeof(simpleMesh->m_primitiveType), file);
	fread(&simpleMesh->m_indexBufferSize, 1, sizeof(simpleMesh->m_indexBufferSize), file);
	fread(&simpleMesh->m_vertexBufferSize, 1, sizeof(simpleMesh->m_vertexBufferSize), file);

	simpleMesh->m_indexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(Gnm::SizeAlign(simpleMesh->m_indexBufferSize,Gnm::kAlignmentOfBufferInBytes)));
	simpleMesh->m_vertexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(Gnm::SizeAlign(simpleMesh->m_vertexBufferSize,Gnm::kAlignmentOfBufferInBytes)));
	if(Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		char temp[Framework::kResourceNameLength];
		sprintf(temp, "%s (Vertex)", filename);
		Gnm::registerResource(nullptr, allocators->m_owner, simpleMesh->m_vertexBuffer, simpleMesh->m_vertexBufferSize, temp, Gnm::kResourceTypeVertexBufferBaseAddress, 0);
		sprintf(temp, "%s (Index)", filename);
		Gnm::registerResource(nullptr, allocators->m_owner, simpleMesh->m_indexBuffer, simpleMesh->m_indexBufferSize, temp, Gnm::kResourceTypeIndexBufferBaseAddress, 0);
	}
	if ( simpleMesh->m_indexBuffer )
	{
		fread(simpleMesh->m_indexBuffer, simpleMesh->m_indexBufferSize, 1, file );
	}
	if ( simpleMesh->m_vertexBuffer )
	{
		fread(simpleMesh->m_vertexBuffer, simpleMesh->m_vertexBufferSize, 1, file );
	}
	fclose(file);
	Framework::SetMeshVertexBufferFormat( simpleMesh );
}

void Framework::LoadSimpleMesh(Gnmx::Toolkit::IAllocator* allocator, SimpleMesh* simpleMesh, const char* filename)
{
	Gnmx::Toolkit::Allocators allocators(*allocator, *allocator);
	return LoadSimpleMesh(&allocators, simpleMesh, filename);
}

void Framework::scaleSimpleMesh(SimpleMesh* simpleMesh, float scale)
{
	SimpleMeshVertex* pvVerts = static_cast<SimpleMeshVertex*>(simpleMesh->m_vertexBuffer);
	const uint32_t iNrVerts = simpleMesh->m_vertexCount;
	for(uint32_t i=0; i<iNrVerts; ++i)
	{
		pvVerts[i].m_position = ToVector3(pvVerts[i].m_position) * scale;
	}
}

void Framework::Mesh::copyPositionAttribute(const Mesh *source,uint32_t attribute)
{
	SCE_GNM_ASSERT_MSG(m_vertexCount == source->m_vertexCount,"This mesh and the mesh it's copying from must have the same number of vertices.");
	Vector4 mini,maxi;
	source->getMinMaxFromAttribute(&mini,&maxi,0);
	const Vector4 range = maxi - mini;
	const float x = range.getX();
	const float y = range.getY();
	const float z = range.getZ();
	const float w = range.getW();
	float scale = x;
	if(y > scale)
		scale = y;
	if(z > scale)
		scale = z;
	if(w > scale)
		scale = w;
	m_bufferToModel = Matrix4::translation(mini.getXYZ()) * Matrix4::scale(Vector3(scale));
	const float invScale = 1.f / scale;
	const Vector4 invTranslation = -mini * invScale;
	for(uint32_t element = 0; element < source->m_vertexCount; ++element)
	{
		Vector4 value = source->getElement(attribute,element);
		value = value * invScale + invTranslation;
		setElement(attribute,element,value);
	}
}

void Framework::Mesh::copyAttribute(const Mesh *source,uint32_t attribute)
{
	SCE_GNM_ASSERT_MSG(m_vertexCount == source->m_vertexCount,"This mesh and the mesh it's copying from must have the same number of vertices.");
	for(uint32_t element = 0; element < source->m_vertexCount; ++element)
	{
		Vector4 value = source->getElement(attribute,element);
		setElement(attribute,element,value);
	}
}

void Framework::Mesh::compress(const sce::Gnm::DataFormat *dataFormat,const Framework::SimpleMesh *source,sce::Gnmx::Toolkit::IAllocator* allocator)
{
	m_primitiveType = source->m_primitiveType;
	m_indexType = source->m_indexType;
	allocate(dataFormat,source->m_vertexAttributeCount,source->m_vertexCount,source->m_indexCount,allocator);
	memcpy(m_indexBuffer,source->m_indexBuffer,m_indexBufferSize);
	copyPositionAttribute(source,0);
	for(uint32_t attribute = 1; attribute < m_vertexAttributeCount; ++attribute)
		copyAttribute(source,attribute);
}

unsigned makeKey(unsigned a,unsigned b)
{
	if(a > b)
		std::swap(a,b);
	return (a << 16) | b;
}

class Vertex
{
public:
	Vector4 m_vector[4];
};

void Framework::tessellate(Mesh *destination,const Mesh *source,sce::Gnmx::Toolkit::IAllocator* allocator)
{
	std::unordered_map<unsigned,Vertex> edgeToMidpoint;
	const unsigned oldIndices = source->m_indexCount;
	const unsigned oldTriangles = oldIndices / 3;
	const unsigned oldVertices = source->m_vertexCount;
	for(auto t = 0U; t < oldTriangles; t++)
	{
		unsigned i0,i1,i2;
		if(source->m_indexType == Gnm::kIndexSize16)
		{
			i0 = ((uint16_t *)source->m_indexBuffer)[t * 3 + 0];
			i1 = ((uint16_t *)source->m_indexBuffer)[t * 3 + 1];
			i2 = ((uint16_t *)source->m_indexBuffer)[t * 3 + 2];
		} else
		{
			i0 = ((uint32_t *)source->m_indexBuffer)[t * 3 + 0];
			i1 = ((uint32_t *)source->m_indexBuffer)[t * 3 + 1];
			i2 = ((uint32_t *)source->m_indexBuffer)[t * 3 + 2];
		}
		Vertex v[3];
		for(unsigned a = kPosition; a <= kTexture; ++a)
		{
			const Vector4 p0 = source->getElement(a,i0);
			const Vector4 p1 = source->getElement(a,i1);
			const Vector4 p2 = source->getElement(a,i2);
			v[0].m_vector[a] = (p0 + p1) * 0.5f;
			v[1].m_vector[a] = (p1 + p2) * 0.5f;
			v[2].m_vector[a] = (p2 + p0) * 0.5f;
		}
		edgeToMidpoint.insert({makeKey(i0,i1),v[0]});
		edgeToMidpoint.insert({makeKey(i1,i2),v[1]});
		edgeToMidpoint.insert({makeKey(i2,i0),v[2]});
	}

	const unsigned newIndices = oldIndices * 4;
	const unsigned newVertices = source->m_vertexCount + edgeToMidpoint.size();
	const unsigned newIndexBufferSize = newIndices * ((source->m_indexType == Gnm::kIndexSize16) ? 2 : 4);

	Gnm::Buffer buffer[4];
	for(unsigned a = kPosition; a <= kTexture; ++a)
	{
		const unsigned newVertexBufferSize = newVertices * source->m_buffer[a].getDataFormat().getBytesPerElement();
		void *vertexBuffer = allocator->allocate(newVertexBufferSize,Gnm::kAlignmentOfBufferInBytes);
		buffer[a].initAsDataBuffer(vertexBuffer,source->m_buffer[a].getDataFormat(),newVertices);
		for(unsigned i = 0; i < oldVertices; ++i)
			setBufferElement(buffer[a],i,getBufferElement(source->m_buffer[a],i));
	}
	std::unordered_map<unsigned,unsigned> edgeToNewVertexIndex;
	unsigned nextVertexIndex = source->m_vertexCount;
	for(auto e : edgeToMidpoint)
	{
		edgeToNewVertexIndex.insert({e.first,nextVertexIndex});
		for(unsigned a = kPosition; a <= kTexture; ++a)
			setBufferElement(buffer[a],nextVertexIndex,e.second.m_vector[a]);
		++nextVertexIndex;
	}
	destination->m_vertexCount = nextVertexIndex;

	void *indexBuffer = allocator->allocate(newIndexBufferSize,Gnm::kAlignmentOfBufferInBytes);
	for(auto t = 0U; t < oldTriangles; t++)
	{
		if(source->m_indexType == Gnm::kIndexSize16)
		{
			const uint16_t i0 = ((uint16_t *)source->m_indexBuffer)[t * 3 + 0];
			const uint16_t i1 = ((uint16_t *)source->m_indexBuffer)[t * 3 + 1];
			const uint16_t i2 = ((uint16_t *)source->m_indexBuffer)[t * 3 + 2];
			const uint16_t i[] =
			{
				i0,
				i1,
				i2,
				(uint16_t)edgeToNewVertexIndex.find(makeKey(i0,i1))->second,
				(uint16_t)edgeToNewVertexIndex.find(makeKey(i1,i2))->second,
				(uint16_t)edgeToNewVertexIndex.find(makeKey(i2,i0))->second,
			};
			((uint16_t *)indexBuffer)[t * 12 +  0] = i[0];
			((uint16_t *)indexBuffer)[t * 12 +  1] = i[3];
			((uint16_t *)indexBuffer)[t * 12 +  2] = i[5];

			((uint16_t *)indexBuffer)[t * 12 +  3] = i[5];
			((uint16_t *)indexBuffer)[t * 12 +  4] = i[4];
			((uint16_t *)indexBuffer)[t * 12 +  5] = i[2];

			((uint16_t *)indexBuffer)[t * 12 +  6] = i[3];
			((uint16_t *)indexBuffer)[t * 12 +  7] = i[4];
			((uint16_t *)indexBuffer)[t * 12 +  8] = i[5];

			((uint16_t *)indexBuffer)[t * 12 +  9] = i[3];
			((uint16_t *)indexBuffer)[t * 12 + 10] = i[1];
			((uint16_t *)indexBuffer)[t * 12 + 11] = i[4];
		} else
		{
			const uint32_t i0 = ((uint32_t *)source->m_indexBuffer)[t * 3 + 0];
			const uint32_t i1 = ((uint32_t *)source->m_indexBuffer)[t * 3 + 1];
			const uint32_t i2 = ((uint32_t *)source->m_indexBuffer)[t * 3 + 2];
			const uint32_t i[] =
			{
				i0,
				i1,
				i2,
				(uint32_t)edgeToNewVertexIndex.find(makeKey(i0,i1))->second,
				(uint32_t)edgeToNewVertexIndex.find(makeKey(i1,i2))->second,
				(uint32_t)edgeToNewVertexIndex.find(makeKey(i2,i0))->second,
			};
			((uint32_t *)indexBuffer)[t * 12 +  0] = i[0];
			((uint32_t *)indexBuffer)[t * 12 +  1] = i[3];
			((uint32_t *)indexBuffer)[t * 12 +  2] = i[5];

			((uint32_t *)indexBuffer)[t * 12 +  3] = i[5];
			((uint32_t *)indexBuffer)[t * 12 +  4] = i[4];
			((uint32_t *)indexBuffer)[t * 12 +  5] = i[2];

			((uint32_t *)indexBuffer)[t * 12 +  6] = i[3];
			((uint32_t *)indexBuffer)[t * 12 +  7] = i[4];
			((uint32_t *)indexBuffer)[t * 12 +  8] = i[5];

			((uint32_t *)indexBuffer)[t * 12 +  9] = i[3];
			((uint32_t *)indexBuffer)[t * 12 + 10] = i[1];
			((uint32_t *)indexBuffer)[t * 12 + 11] = i[4];
		}
	}

	for(unsigned a = kPosition; a <= kTexture; ++a)
		destination->m_buffer[a] = buffer[a];

	destination->m_indexBuffer = indexBuffer;
	destination->m_vertexBuffer = destination->m_buffer[kPosition].getBaseAddress();
	destination->m_indexCount = newIndices;
	destination->m_indexBufferSize = newIndexBufferSize;
	destination->m_vertexBufferSize = destination->m_buffer[kPosition].getSize();
	destination->m_vertexAttributeCount = source->m_vertexAttributeCount;
	destination->m_indexType = source->m_indexType;
}

