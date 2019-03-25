/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_SIMPLE_MESH_H_
#define _SCE_GNM_FRAMEWORK_SIMPLE_MESH_H_

#include <gnmx.h>
#include "../toolkit/allocators.h"
#include "../toolkit/geommath/geommath.h"

namespace Framework
{
	Vector4 getBufferElement(sce::Gnm::Buffer buffer,uint32_t elementIndex);
	void setBufferElement(sce::Gnm::Buffer buffer,uint32_t elementIndex,Vector4 value);
	void getBufferElementMinMax(Vector4 *min,Vector4 *max,sce::Gnm::Buffer buffer);

	class SimpleMesh;

	class Mesh
	{
	public:
		enum { kMaximumVertexBufferElements = 16 };

		void *m_vertexBuffer;
		uint32_t m_vertexBufferSize;
		uint32_t m_vertexCount;
		uint32_t m_vertexAttributeCount;
		uint8_t m_reserved0[4];
		void *m_indexBuffer;
		uint32_t m_indexBufferSize;
		uint32_t m_indexCount;
		sce::Gnm::IndexSize m_indexType;
		sce::Gnm::PrimitiveType m_primitiveType;
		sce::Gnm::Buffer m_buffer[kMaximumVertexBufferElements];

		Matrix4 m_bufferToModel;

		Mesh();
		void SetVertexBuffer(sce::Gnmx::GnmxGfxContext &gfxc,sce::Gnm::ShaderStage stage);
		Vector4 getElement(uint32_t attributeIndex,uint32_t elementIndex) const;
		void setElement(uint32_t attributeIndex,uint32_t elementIndex,Vector4 value);

		void copyPositionAttribute(const Mesh *source,uint32_t attribute);
		void copyAttribute(const Mesh *source,uint32_t attribute);
		void getMinMaxFromAttribute(Vector4 *min,Vector4 *max,uint32_t attribute) const;

		void allocate(const sce::Gnm::DataFormat *dataFormat,uint32_t attributes,uint32_t elements,uint32_t indices,sce::Gnmx::Toolkit::IAllocator* allocator);
		void compress(const sce::Gnm::DataFormat *dataFormat,const Framework::SimpleMesh *source,sce::Gnmx::Toolkit::IAllocator* allocator);
	};

    typedef Mesh FlexibleMesh;

	void tessellate(Mesh *destination,const Mesh *source,sce::Gnmx::Toolkit::IAllocator* allocator);

	class SimpleMesh : public Mesh
	{
	public:
		uint32_t m_vertexStride; // in bytes
		uint32_t m_reserved[3];
	};

	/** @brief Describes a vertex format element for a SimpleMesh
     */
	enum MeshVertexBufferElement
	{
		kPosition,
		kNormal,
		kTangent,
		kTexture
	};

	/** @brief Sets the vertex format for a SimpleMesh from an array of MeshVertexBufferElements
     */
	void SetMeshVertexBufferFormat( sce::Gnm::Buffer* dest, SimpleMesh *destMesh, const MeshVertexBufferElement* element, uint32_t elements );

	/** @brief Sets a standard vertex format that contains all possible MeshVertexBufferElements
     */
	void SetMeshVertexBufferFormat( SimpleMesh *destMesh );

	/** @brief Controls whether Build*Mesh functions generate vertices and/or indices, and what optimizations are performed while generating indices.
	 */
	enum BuildMeshMode {
		kBuildVerticesOnly								=-1,	///< only build SimpleMesh::m_vertexBuffer and fill in vertex related SimpleMesh data; do not fill out any index related SimpleMesh elements
		kBuildUnoptimizedIndices						= 0,	///< only build SimpleMesh::m_indexBuffer with no optimization for reuse cache and fill in index related SimpleMesh data; do not fill out any vertex related SimpleMesh elements
		kBuildVerticesAndUnoptimizedIndices				= 1,	///< build SimpleMesh::m_vertexBuffer and SimpleMesh::m_indexBuffer with no optimization for reuse cache
		kBuildOptimizedIndices							= 2,	///< build SimpleMesh::m_indexBuffer striped to maximize vertex reuse; do not fill out any vertex related SimpleMesh elements
		kBuildVerticesAndOptimizedIndices				= 3,	///< build SimpleMesh::m_vertexBuffer and SimpleMesh::m_indexBuffer striped to maximize vertex reuse
		kBuildDispatchDrawOptimizedIndices				= 4,	///< build SimpleMesh::m_indexBuffer striped to maximize vertex reuse in dispatch draw; do not fill out any vertex related SimpleMesh elements
		kBuildVerticesAndDispatchDrawOptimizedIndices	= 5,	///< build SimpleMesh::m_vertexBuffer and SimpleMesh::m_indexBuffer striped to maximize vertex reuse in dispatch draw
	};

	/** @brief Creates a SimpleMesh in the shape of a torus.
	 * @param eMode Controls whether vertices and/or indices are generated, and what optimizations are performed while generating indices.
	 * @param destMesh The mesh to receive the torus shape
	 * @param outerRadius The outer radius of the torus - the distance from its center to the center of its cross-section
	 * @param innerRadius The inner radius of the torus - the distance from the center of its cross-section to its surface
	 * @param outerQuads The number of quadrilaterals around the outer radius	 
	 * @param innerQuads The number of quadrilaterals around the inner radius
	 * @param outerRepeats The number of times that UV coordinates repeat around the outer radius
	 * @param innerRepeats The number of times that UV coordinates repeat around the inner radius
	 */
	void BuildTorusMesh(BuildMeshMode const eMode, sce::Gnmx::Toolkit::IAllocator* allocator, SimpleMesh *destMesh, float outerRadius, float innerRadius, uint16_t outerQuads, uint16_t innerQuads, float outerRepeats, float innerRepeats);

	/** @brief Creates a SimpleMesh in the shape of a torus.
	 * @param eMode Controls whether vertices and/or indices are generated, and what optimizations are performed while generating indices.
	 * @param destMesh The mesh to receive the torus shape
	 * @param outerRadius The outer radius of the torus - the distance from its center to the center of its cross-section
	 * @param innerRadius The inner radius of the torus - the distance from the center of its cross-section to its surface
	 * @param outerQuads The number of quadrilaterals around the outer radius	 
	 * @param innerQuads The number of quadrilaterals around the inner radius
	 * @param outerRepeats The number of times that UV coordinates repeat around the outer radius
	 * @param innerRepeats The number of times that UV coordinates repeat around the inner radius
	 */
	void BuildTorusMesh(BuildMeshMode const eMode, sce::Gnmx::Toolkit::Allocators* allocators, const char *name, SimpleMesh *destMesh, float outerRadius, float innerRadius, uint16_t outerQuads, uint16_t innerQuads, float outerRepeats, float innerRepeats);

	/** @brief Legacy version of BuildTorusMesh.
	 */
	static inline void BuildTorusMesh(sce::Gnmx::Toolkit::IAllocator* allocator, SimpleMesh *destMesh, float outerRadius, float innerRadius, uint16_t outerQuads, uint16_t innerQuads, float outerRepeats, float innerRepeats)
	{
		BuildTorusMesh(kBuildVerticesAndUnoptimizedIndices, allocator, destMesh, outerRadius, innerRadius, outerQuads, innerQuads, outerRepeats, innerRepeats);
	}

	/** @brief Legacy version of BuildTorusMesh.
	 */
	static inline void BuildTorusMesh(sce::Gnmx::Toolkit::Allocators* allocators, const char *name, SimpleMesh *destMesh, float outerRadius, float innerRadius, uint16_t outerQuads, uint16_t innerQuads, float outerRepeats, float innerRepeats)
	{
		BuildTorusMesh(kBuildVerticesAndUnoptimizedIndices, allocators, name, destMesh, outerRadius, innerRadius, outerQuads, innerQuads, outerRepeats, innerRepeats);
	}

	/** @brief Creates a SimpleMesh in the shape of a sphere.
	 * @param destMesh The mesh to receive the sphere shape.
	 * @param radius The radius of the sphere
	 * @param xdiv The number of X axis subdivisions
	 * @param ydiv The number of Y axis subdivisions
	 * @param tx The X coordinate of the sphere's center
	 * @param ty The Y coordinate of the sphere's center
	 * @param tz The Z coordinate of the sphere's center
	 */
	void BuildSphereMesh(sce::Gnmx::Toolkit::IAllocator* allocator, SimpleMesh *destMesh, float radius, long xdiv, long ydiv, float tx=0.0f, float ty=0.0f, float tz=0.0f);

	/** @brief Creates a SimpleMesh in the shape of a sphere.
	 * @param destMesh The mesh to receive the sphere shape.
	 * @param radius The radius of the sphere
	 * @param xdiv The number of X axis subdivisions
	 * @param ydiv The number of Y axis subdivisions
	 * @param tx The X coordinate of the sphere's center
	 * @param ty The Y coordinate of the sphere's center
	 * @param tz The Z coordinate of the sphere's center
	 */
	void BuildSphereMesh(sce::Gnmx::Toolkit::Allocators* allocators, const char *name, SimpleMesh *destMesh, float radius, long xdiv, long ydiv, float tx=0.0f, float ty=0.0f, float tz=0.0f);

	/** @brief Creates a SimpleMesh in the shape of a quadrilateral.
	 * @param destMesh The mesh to receive the quadrilateral shape
	 * @param size Half the length of a quadrilateral edge
	 */
	void BuildQuadMesh(sce::Gnmx::Toolkit::IAllocator* allocator, SimpleMesh *destMesh, float size);

	/** @brief Creates a SimpleMesh in the shape of a quadrilateral.
	 * @param destMesh The mesh to receive the quadrilateral shape
	 * @param size Half the length of a quadrilateral edge
	 */
	void BuildQuadMesh(sce::Gnmx::Toolkit::Allocators* allocators, const char *name, SimpleMesh *destMesh, float size);

	/** @brief Creates a SimpleMesh in the shape of a cube.
	 * @param destMesh The mesh to receive the cube shape
	 * @param side The length of one side of the cube
	 */
	void BuildCubeMesh(sce::Gnmx::Toolkit::IAllocator* allocator, SimpleMesh *destMesh, float side);

	/** @brief Creates a SimpleMesh in the shape of a cube.
	 * @param destMesh The mesh to receive the cube shape
	 * @param side The length of one side of the cube
	 */
	void BuildCubeMesh(sce::Gnmx::Toolkit::Allocators* allocators, const char *name, SimpleMesh *destMesh, float side);

	/** @brief Computes a height map scale by analyzing a mesh's contents
	 */
	float ComputeMeshSpecificBumpScale(const SimpleMesh *srcMesh);

	/** @brief Saves a SimpleMesh to a binary file.
	 * @param simplemesh A pointer to the mesh to be saved
	 * @param filename The path to the SimpleMesh file to save
	 */
	void SaveSimpleMesh( const SimpleMesh* simplemesh, const char* filename );

	/** @brief Loads a SimpleMesh from a binary file (created with SaveSimpleMesh).  The vertex data will
	 *         be stored in shared VRAM, and must be freed by the caller when the SimpleMesh is no longer required.
	 * @param simpleMesh If the load is successful, the new SimpleMesh object will be written here.
	 * @param filename The path to the SimpleMesh file to load
	 */
	void LoadSimpleMesh(sce::Gnmx::Toolkit::IAllocator* allocator, SimpleMesh* simpleMesh, const char* filename);

	/** @brief Loads a SimpleMesh from a binary file (created with SaveSimpleMesh).  The vertex data will
	 *         be stored in shared VRAM, and must be freed by the caller when the SimpleMesh is no longer required.
	 * @param simpleMesh If the load is successful, the new SimpleMesh object will be written here.
	 * @param filename The path to the SimpleMesh file to load
	 */
	void LoadSimpleMesh(sce::Gnmx::Toolkit::Allocators* allocators, SimpleMesh* simpleMesh, const char* filename);

	/** @brief Scales the vertex positions of a SimpleMesh, without affecting any other vertex attributes.
	 * @param simpleMesh This is the SimpleMesh object to scale
	 * @param scale This is the scale to apply to the vertex positions of the SimpleMesh.
	 */
	void scaleSimpleMesh(SimpleMesh* simpleMesh, float scale);
}

#endif // _SCE_GNM_FRAMEWORK_SIMPLE_MESH_
