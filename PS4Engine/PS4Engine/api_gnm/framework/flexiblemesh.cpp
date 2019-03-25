/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "flexiblemesh.h"
#include <float.h>
#include <stdio.h>
#include <stddef.h>
#include "../toolkit/dataformat_interpreter.h"
#include "../framework/framework.h"

using namespace sce;

Framework::FlexibleMesh::FlexibleMesh()
: m_vertexBuffer(0)
, m_vertexBufferSize(0)
, m_vertexCount(0)
, m_vertexAttributeCount(0)
, m_indexBuffer(0)
, m_indexBufferSize(0)
, m_indexCount(0)
, m_indexType(sce::Gnm::kIndexSize16)
, m_primitiveType(sce::Gnm::kPrimitiveTypeTriList)
, m_bufferToModel(Matrix4::identity())
{
}

void Framework::FlexibleMesh::SetVertexBuffer(sce::Gnmx::GnmxGfxContext &gfxc, Gnm::ShaderStage stage )
{
	SCE_GNM_ASSERT( m_vertexAttributeCount < kMaximumVertexBufferElements );
	gfxc.setVertexBuffers(stage, 0, m_vertexAttributeCount, m_buffer);
}

Vector4 Framework::getBufferElement(sce::Gnm::Buffer buffer, uint32_t elementIndex)
{
	SCE_GNM_ASSERT_MSG(elementIndex < buffer.getNumElements(), "can't ask for element %d in a mesh that has only %d", elementIndex, buffer.getNumElements());
	const Gnm::DataFormat dataFormat = buffer.getDataFormat();
	const uint32_t stride = buffer.getStride();
	const uint8_t *src = static_cast<const uint8_t *>(buffer.getBaseAddress()) + elementIndex * stride;
	Gnmx::Toolkit::Reg32 reg32[4];
	Gnmx::Toolkit::dataFormatDecoder(reg32, static_cast<const uint32_t *>(static_cast<const void *>(src)), dataFormat);
	Vector4 value;
	memcpy(&value, reg32, sizeof(value));
	return value;
}

void Framework::setBufferElement(sce::Gnm::Buffer buffer, uint32_t elementIndex, Vector4 value)
{
	SCE_GNM_ASSERT_MSG(elementIndex < buffer.getNumElements(), "can't ask for element %d in a mesh that has only %d", elementIndex, buffer.getNumElements());
	const Gnm::DataFormat dataFormat = buffer.getDataFormat();
	const uint32_t stride = buffer.getStride();
	uint8_t *dest = static_cast<uint8_t *>(buffer.getBaseAddress()) + elementIndex * stride;
	Gnmx::Toolkit::Reg32 reg32[4];
	memcpy(reg32, &value, sizeof(value));
	uint32_t destDwords = 0;
	uint32_t temp[4];
	Gnmx::Toolkit::dataFormatEncoder(temp, &destDwords, reg32, dataFormat);
	memcpy(dest, temp, dataFormat.getBytesPerElement());
}

void Framework::getBufferElementMinMax(Vector4 *min, Vector4 *max, sce::Gnm::Buffer buffer)
{
	*min = *max = getBufferElement(buffer, 0);
	for(uint32_t elementIndex = 1; elementIndex < buffer.getNumElements(); ++elementIndex)
	{
		const Vector4 element = getBufferElement(buffer, elementIndex);
		*min = minPerElem(*min, element);
		*max = maxPerElem(*max, element);
	}
}

Vector4 Framework::FlexibleMesh::getElement(uint32_t attributeIndex, uint32_t elementIndex) const
{
	SCE_GNM_ASSERT_MSG(attributeIndex < m_vertexAttributeCount, "can't ask for attribute %d in a mesh that has only %d", attributeIndex, m_vertexAttributeCount);
	const Gnm::Buffer buffer = m_buffer[attributeIndex];
	return getBufferElement(buffer, elementIndex);
}

void Framework::FlexibleMesh::setElement(uint32_t attributeIndex, uint32_t elementIndex, Vector4 value)
{
	SCE_GNM_ASSERT_MSG(attributeIndex < m_vertexAttributeCount, "can't ask for attribute %d in a mesh that has only %d", attributeIndex, m_vertexAttributeCount);
	const Gnm::Buffer buffer = m_buffer[attributeIndex];
	setBufferElement(buffer, elementIndex, value);
}

void Framework::FlexibleMesh::getMinMaxFromAttribute(Vector4 *min, Vector4 *max, uint32_t attributeIndex) const
{
	SCE_GNM_ASSERT_MSG(attributeIndex < m_vertexAttributeCount, "can't ask for attribute %d in a mesh that has only %d", attributeIndex, m_vertexAttributeCount);
	const Gnm::Buffer buffer = m_buffer[attributeIndex];
	getBufferElementMinMax(min, max, buffer);
}

void Framework::FlexibleMesh::allocate(const sce::Gnm::DataFormat *dataFormat, uint32_t attributes, uint32_t elements, uint32_t indices, sce::Gnmx::Toolkit::Allocators* allocators, const char *name)
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

	m_vertexBuffer = allocators->m_garlic.allocate(sce::Gnm::SizeAlign(bufferSize, sce::Gnm::kAlignmentOfBufferInBytes));
	m_indexBuffer = (uint8_t *)m_vertexBuffer + m_vertexBufferSize;
	if(0 != name && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		char temp[Framework::kResourceNameLength];
		sprintf(temp, "%s (Vertex)", name);
		Gnm::registerResource(nullptr, allocators->m_owner, m_vertexBuffer, m_vertexBufferSize, temp, Gnm::kResourceTypeVertexBufferBaseAddress, 0);
		sprintf(temp, "%s (Index)", name);
		Gnm::registerResource(nullptr, allocators->m_owner, m_indexBuffer, m_indexBufferSize, temp, Gnm::kResourceTypeIndexBufferBaseAddress, 0);
	}

	unsigned offset = 0;
	for(uint32_t attribute = 0; attribute < m_vertexAttributeCount; ++attribute)
	{
		m_buffer[attribute].initAsVertexBuffer((uint8_t *)m_vertexBuffer + offset, dataFormat[attribute], dataFormat[attribute].getBytesPerElement(), m_vertexCount);
		offset += dataFormat[attribute].getBytesPerElement() * m_vertexCount;
		offset = (offset + 3) & ~3;
	}
}

void Framework::FlexibleMesh::allocate(const sce::Gnm::DataFormat *dataFormat, uint32_t attributes, uint32_t elements, uint32_t indices, sce::Gnmx::Toolkit::IAllocator* allocator)
{
	Gnmx::Toolkit::Allocators allocators(*allocator, *allocator);
	return allocate(dataFormat, attributes, elements, indices, &allocators, 0);
}

void Framework::SaveMesh(const Framework::FlexibleMesh* mesh, const char* filename)
{
	FILE* file;
	file = fopen(filename, "wb");
	fprintf(file, "[\n");
	fprintf(file, "  {\n");
	fprintf(file, "    \"vertexAttributeCount\": %d,\n", mesh->m_vertexAttributeCount);
	fprintf(file, "    \"vertexCount\": %d,\n", mesh->m_vertexCount);
	fprintf(file, "    \"primitiveType\": %d,\n", mesh->m_primitiveType);
	fprintf(file, "    \"indexCount\": %d,\n", mesh->m_indexCount);
	fprintf(file, "    \"indexType\": %d\n", mesh->m_indexType);
	fprintf(file, "  },\n");
	fprintf(file, "  [");
	for(uint32_t index = 0; index < mesh->m_indexCount; ++index)
	{
		if(index % 16 == 0)
			fprintf(file, "\n    ");
		if(mesh->m_indexType == Gnm::kIndexSize16)
			fprintf(file, "%d", ((const uint16_t *)mesh->m_indexBuffer)[index]);
		else
			fprintf(file, "%d", ((const uint32_t *)mesh->m_indexBuffer)[index]);
		if(index < mesh->m_indexCount - 1)
			fprintf(file, ", ");
	}
	fprintf(file, "\n  ],\n");
	for(uint32_t attribute = 0; attribute < mesh->m_vertexAttributeCount; ++attribute)
	{
		const Gnm::Buffer buffer = mesh->m_buffer[attribute];
		const Gnm::DataFormat dataFormat = buffer.getDataFormat();
		fprintf(file, "  [\n");
		fprintf(file, "    {\n");
		fprintf(file, "      \"surfaceFormat\": %d,\n", dataFormat.getSurfaceFormat());
		fprintf(file, "      \"bufferChannelType\": %d,\n", dataFormat.getBufferChannelType());
		fprintf(file, "      \"channelX\": %d,\n", dataFormat.m_bits.m_channelX);
		fprintf(file, "      \"channelY\": %d,\n", dataFormat.m_bits.m_channelY);
		fprintf(file, "      \"channelZ\": %d,\n", dataFormat.m_bits.m_channelZ);
		fprintf(file, "      \"channelW\": %d\n", dataFormat.m_bits.m_channelW);
		fprintf(file, "    },\n");
		fprintf(file, "    [");
		for(uint32_t element = 0; element < mesh->m_vertexCount; ++element)
		{
			if(element % 4 == 0)
				fprintf(file, "\n      ");
			Vector4 value = mesh->getElement(attribute, element);
			fprintf(file, "[%f, %f, %f, %f]", (float)value.getX(), (float)value.getY(), (float)value.getZ(), (float)value.getW());
			if(element < mesh->m_vertexCount - 1)
				fprintf(file, ", ");
		}
		fprintf(file, "\n");
		fprintf(file, "    ]\n");
		fprintf(file, "  ]");
		if(attribute < mesh->m_vertexAttributeCount - 1 )
			fprintf(file, ", ");
		fprintf(file, "\n");
	}
	fprintf(file, "]\n");
	fclose(file);
}

void Framework::FlexibleMesh::copyPositionAttribute(const FlexibleMesh *source, uint32_t attribute)
{
	SCE_GNM_ASSERT_MSG(m_vertexCount == source->m_vertexCount, "This mesh and the mesh it's copying from must have the same number of vertices.");
	Vector4 mini, maxi;
	source->getMinMaxFromAttribute(&mini, &maxi, 0);
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
		Vector4 value = source->getElement(attribute, element);
		value = value * invScale + invTranslation;
		setElement(attribute, element, value);
	}
}

void Framework::FlexibleMesh::copyAttribute(const FlexibleMesh *source, uint32_t attribute)
{
	SCE_GNM_ASSERT_MSG(m_vertexCount == source->m_vertexCount, "This mesh and the mesh it's copying from must have the same number of vertices.");
	for(uint32_t element = 0; element < source->m_vertexCount; ++element)
	{
		Vector4 value = source->getElement(attribute, element);
		setElement(attribute, element, value);
	}
}

#include "simple_mesh.h"

void Framework::FlexibleMesh::compress(const sce::Gnm::DataFormat *dataFormat, const Framework::SimpleMesh *source, sce::Gnmx::Toolkit::IAllocator* allocator)
{
	m_primitiveType = source->m_primitiveType;
	m_indexType = source->m_indexType;
	allocate(dataFormat, source->m_vertexAttributeCount, source->m_vertexCount, source->m_indexCount, allocator);
	memcpy(m_indexBuffer, source->m_indexBuffer, m_indexBufferSize);
	copyPositionAttribute(source, 0);
	for(uint32_t attribute = 1; attribute < m_vertexAttributeCount; ++attribute)
		copyAttribute(source, attribute);
}

