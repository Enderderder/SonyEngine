/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_FLEXIBLEMESH_H_
#define _SCE_GNM_FRAMEWORK_FLEXIBLEMESH_H_

#include <gnmx.h>
#include "../toolkit/allocator.h"
#include "../toolkit/allocators.h"
#include "../toolkit/geommath/geommath.h"

namespace Framework
{
	Vector4 getBufferElement(sce::Gnm::Buffer buffer, uint32_t elementIndex);
	void setBufferElement(sce::Gnm::Buffer buffer, uint32_t elementIndex, Vector4 value);
	void getBufferElementMinMax(Vector4 *min, Vector4 *max, sce::Gnm::Buffer buffer);

	class SimpleMesh;

	class FlexibleMesh
	{
	public:
		enum { kMaximumVertexBufferElements = 16 };

		void *m_vertexBuffer;
		uint32_t m_vertexBufferSize;
		uint32_t m_vertexCount;
		uint32_t m_vertexAttributeCount;
		void *m_indexBuffer;
		uint32_t m_indexBufferSize;
		uint32_t m_indexCount;
		sce::Gnm::IndexSize m_indexType;
		sce::Gnm::PrimitiveType m_primitiveType;
		sce::Gnm::Buffer m_buffer[kMaximumVertexBufferElements];

		Matrix4 m_bufferToModel;
	
		FlexibleMesh();
		void SetVertexBuffer(sce::Gnmx::GnmxGfxContext &gfxc, sce::Gnm::ShaderStage stage);
		Vector4 getElement(uint32_t attributeIndex, uint32_t elementIndex) const;
		void setElement(uint32_t attributeIndex, uint32_t elementIndex, Vector4 value);

		void copyPositionAttribute(const FlexibleMesh *source, uint32_t attribute);
		void copyAttribute(const FlexibleMesh *source, uint32_t attribute);
		void getMinMaxFromAttribute(Vector4 *min, Vector4 *max, uint32_t attribute) const;

		void allocate(const sce::Gnm::DataFormat *dataFormat, uint32_t attributes, uint32_t elements, uint32_t indices, sce::Gnmx::Toolkit::IAllocator* allocator);
		void allocate(const sce::Gnm::DataFormat *dataFormat, uint32_t attributes, uint32_t elements, uint32_t indices, sce::Gnmx::Toolkit::Allocators* allocators, const char *name);
		void compress(const sce::Gnm::DataFormat *dataFormat, const Framework::SimpleMesh *source, sce::Gnmx::Toolkit::IAllocator* allocator);
	};

	/** @brief Saves a Mesh to a text file.
	 * @param mesh A pointer to the mesh to be saved
	 * @param filename The path to the mesh file to save
	 */
	void SaveMesh(const FlexibleMesh* mesh, const char* filename);
}

#endif // _SCE_GNM_FRAMEWORK_MESH_
