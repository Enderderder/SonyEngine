/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "embedded_shader.h"
#include <string.h>
#include <gnmx/shader_parser.h>
#include <algorithm>
using namespace sce;

void Gnmx::Toolkit::EmbeddedPsShader::initializeWithAllocators(Allocators *allocators)
{
	Gnmx::ShaderInfo shaderInfo;
	Gnmx::parseShader(&shaderInfo, m_source);

	void *shaderBinary = allocators->m_garlic.allocate(shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
	void *shaderHeader = allocators->m_onion.allocate(shaderInfo.m_psShader->computeSize(), Gnm::kAlignmentOfBufferInBytes);

	memcpy(shaderBinary, shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);
	memcpy(shaderHeader, shaderInfo.m_psShader, shaderInfo.m_psShader->computeSize());

	m_shader = static_cast<Gnmx::PsShader*>(shaderHeader);
	m_shader->patchShaderGpuAddress(shaderBinary);

	if(0 != m_name && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		Gnm::registerResource(nullptr, allocators->m_owner, m_shader->getBaseAddress(), shaderInfo.m_gpuShaderCodeSize, m_name, Gnm::kResourceTypeShaderBaseAddress, 0);
	}
	Gnmx::generateInputOffsetsCache(&m_offsetsTable, Gnm::kShaderStagePs, m_shader);
}

void Gnmx::Toolkit::EmbeddedCsShader::initializeWithAllocators(Allocators *allocators)
{
	Gnmx::ShaderInfo shaderInfo;
	Gnmx::parseShader(&shaderInfo, m_source);

	void *shaderBinary = allocators->m_garlic.allocate(shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
	void *shaderHeader = allocators->m_onion.allocate(shaderInfo.m_csShader->computeSize(), Gnm::kAlignmentOfBufferInBytes);

	memcpy(shaderBinary, shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);
	memcpy(shaderHeader, shaderInfo.m_csShader, shaderInfo.m_csShader->computeSize());

	m_shader = static_cast<Gnmx::CsShader*>(shaderHeader);
	m_shader->patchShaderGpuAddress(shaderBinary);

	if(0 != m_name && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		Gnm::registerResource(nullptr, allocators->m_owner, m_shader->getBaseAddress(), shaderInfo.m_gpuShaderCodeSize, m_name, Gnm::kResourceTypeShaderBaseAddress, 0);
	}
	Gnmx::generateInputOffsetsCache(&m_offsetsTable, Gnm::kShaderStageCs, m_shader);
}

void Gnmx::Toolkit::EmbeddedVsShader::initializeWithAllocators(Allocators *allocators)
{
	Gnmx::ShaderInfo shaderInfo;
	Gnmx::parseShader(&shaderInfo, m_source);

	void *shaderBinary = allocators->m_garlic.allocate(shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
	void *shaderHeader = allocators->m_onion.allocate(shaderInfo.m_vsShader->computeSize(), Gnm::kAlignmentOfBufferInBytes);

	memcpy(shaderBinary, shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);
	memcpy(shaderHeader, shaderInfo.m_vsShader, shaderInfo.m_vsShader->computeSize());

	m_shader = static_cast<Gnmx::VsShader*>(shaderHeader);
	m_shader->patchShaderGpuAddress(shaderBinary);

	if(0 != m_name && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		Gnm::registerResource(nullptr, allocators->m_owner, m_shader->getBaseAddress(), shaderInfo.m_gpuShaderCodeSize, m_name, Gnm::kResourceTypeShaderBaseAddress, 0);
	}
	Gnmx::generateInputOffsetsCache(&m_offsetsTable, Gnm::kShaderStageVs, m_shader);
}

void Gnmx::Toolkit::EmbeddedEsShader::initializeWithAllocators(Allocators *allocators)
{
	Gnmx::ShaderInfo shaderInfo;
	Gnmx::parseShader(&shaderInfo, m_source);

	void *shaderBinary = allocators->m_garlic.allocate(shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
	void *shaderHeader = allocators->m_onion.allocate(shaderInfo.m_esShader->computeSize(), Gnm::kAlignmentOfBufferInBytes);

	memcpy(shaderBinary, shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);
	memcpy(shaderHeader, shaderInfo.m_esShader, shaderInfo.m_esShader->computeSize());

	m_shader = static_cast<Gnmx::EsShader*>(shaderHeader);
	m_shader->patchShaderGpuAddress(shaderBinary);

	if(0 != m_name && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		Gnm::registerResource(nullptr, allocators->m_owner, m_shader->getBaseAddress(), shaderInfo.m_gpuShaderCodeSize, m_name, Gnm::kResourceTypeShaderBaseAddress, 0);
	}
	Gnmx::generateInputOffsetsCache(&m_offsetsTable, Gnm::kShaderStageEs, m_shader);
}

void Gnmx::Toolkit::EmbeddedGsShader::initializeWithAllocators(Allocators *allocators)
{
	Gnmx::ShaderInfo gsShaderInfo;
	Gnmx::ShaderInfo vsShaderInfo;
	Gnmx::parseGsShader(&gsShaderInfo, &vsShaderInfo, m_source);

	void *gsShaderBinary = allocators->m_garlic.allocate(gsShaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
	void *vsShaderBinary = allocators->m_garlic.allocate(vsShaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
	void *gsShaderHeader = allocators->m_onion.allocate(gsShaderInfo.m_gsShader->computeSize(), Gnm::kAlignmentOfBufferInBytes);

	memcpy(gsShaderBinary, gsShaderInfo.m_gpuShaderCode, gsShaderInfo.m_gpuShaderCodeSize);
	memcpy(vsShaderBinary, vsShaderInfo.m_gpuShaderCode, vsShaderInfo.m_gpuShaderCodeSize);
	memcpy(gsShaderHeader, gsShaderInfo.m_gsShader, gsShaderInfo.m_gsShader->computeSize());

	m_shader = static_cast<Gnmx::GsShader*>(gsShaderHeader);
	m_shader->patchShaderGpuAddresses(gsShaderBinary, vsShaderBinary);

	if(m_gsName != 0 && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		Gnm::registerResource(nullptr, allocators->m_owner, m_shader->getBaseAddress(),                  gsShaderInfo.m_gpuShaderCodeSize, m_gsName, Gnm::kResourceTypeShaderBaseAddress, 0);
	}
	if(m_vsName != 0 && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		Gnm::registerResource(nullptr, allocators->m_owner, m_shader->getCopyShader()->getBaseAddress(), vsShaderInfo.m_gpuShaderCodeSize, m_vsName, Gnm::kResourceTypeShaderBaseAddress, 0);
	}
	Gnmx::generateInputOffsetsCache(&m_offsetsTable, Gnm::kShaderStageGs, m_shader);
}

void Gnmx::Toolkit::EmbeddedLsShader::initializeWithAllocators(Allocators *allocators)
{
	Gnmx::ShaderInfo shaderInfo;
	Gnmx::parseShader(&shaderInfo, m_source);

	void *shaderBinary = allocators->m_garlic.allocate(shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
	void *shaderHeader = allocators->m_onion.allocate(shaderInfo.m_lsShader->computeSize(), Gnm::kAlignmentOfBufferInBytes);

	memcpy(shaderBinary, shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);
	memcpy(shaderHeader, shaderInfo.m_lsShader, shaderInfo.m_lsShader->computeSize());

	m_shader = static_cast<Gnmx::LsShader*>(shaderHeader);
	m_shader->patchShaderGpuAddress(shaderBinary);

	if(m_name != 0 && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		Gnm::registerResource(nullptr, allocators->m_owner, m_shader->getBaseAddress(), shaderInfo.m_gpuShaderCodeSize, m_name, Gnm::kResourceTypeShaderBaseAddress, 0);
	}
	Gnmx::generateInputOffsetsCache(&m_offsetsTable, Gnm::kShaderStageLs, m_shader);
}

void Gnmx::Toolkit::EmbeddedHsShader::initializeWithAllocators(Allocators *allocators)
{
	Gnmx::ShaderInfo shaderInfo;
	Gnmx::parseShader(&shaderInfo, m_source);

	void *shaderBinary = allocators->m_garlic.allocate(shaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
	void *shaderHeader = allocators->m_onion.allocate(shaderInfo.m_hsShader->computeSize(), Gnm::kAlignmentOfBufferInBytes);

	memcpy(shaderBinary, shaderInfo.m_gpuShaderCode, shaderInfo.m_gpuShaderCodeSize);
	memcpy(shaderHeader, shaderInfo.m_hsShader, shaderInfo.m_hsShader->computeSize());

	m_shader = static_cast<Gnmx::HsShader*>(shaderHeader);
	m_shader->patchShaderGpuAddress(shaderBinary);

	if(m_name != 0 && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		Gnm::registerResource(nullptr, allocators->m_owner, m_shader->getBaseAddress(), shaderInfo.m_gpuShaderCodeSize, m_name, Gnm::kResourceTypeShaderBaseAddress, 0);
	}
	Gnmx::generateInputOffsetsCache(&m_offsetsTable, Gnm::kShaderStageHs, m_shader);
}

void Gnmx::Toolkit::EmbeddedCsVsShader::initializeWithAllocators(Allocators *allocators)
{
	Gnmx::ShaderInfo csShaderInfo;
	Gnmx::ShaderInfo csvsShaderInfo;
	Gnmx::parseCsVsShader(&csvsShaderInfo, &csShaderInfo, m_source);

	void *vsShaderBinary = allocators->m_garlic.allocate(csvsShaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
	void *csShaderBinary = allocators->m_garlic.allocate(csShaderInfo.m_gpuShaderCodeSize, Gnm::kAlignmentOfShaderInBytes);
	void *csvsShaderHeader = allocators->m_onion.allocate(csvsShaderInfo.m_csvsShader->computeSize(), Gnm::kAlignmentOfBufferInBytes);

	memcpy(vsShaderBinary, csvsShaderInfo.m_gpuShaderCode, csvsShaderInfo.m_gpuShaderCodeSize);
	memcpy(csShaderBinary, csShaderInfo.m_gpuShaderCode, csShaderInfo.m_gpuShaderCodeSize);
	memcpy(csvsShaderHeader, csvsShaderInfo.m_csvsShader, csvsShaderInfo.m_csvsShader->computeSize());

	m_shader = static_cast<Gnmx::CsVsShader*>(csvsShaderHeader);
	m_shader->patchShaderGpuAddresses(vsShaderBinary, csShaderBinary);

	if(m_csName != 0 && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		Gnm::registerResource(nullptr, allocators->m_owner, m_shader->getComputeShader()->getBaseAddress(), csShaderInfo.m_gpuShaderCodeSize, m_csName, Gnm::kResourceTypeShaderBaseAddress, 0);
	}
	if(m_vsName != 0 && Gnm::kInvalidOwnerHandle != allocators->m_owner)
	{
		Gnm::registerResource(nullptr, allocators->m_owner, m_shader->getVertexShader()->getBaseAddress(), csvsShaderInfo.m_gpuShaderCodeSize, m_vsName, Gnm::kResourceTypeShaderBaseAddress, 0);
	}
	generateInputOffsetsCacheForDispatchDraw(&m_offsetsTableCs, &m_offsetsTableVs, m_shader);
}

void Gnmx::Toolkit::EmbeddedShaders::initializeWithAllocators(Allocators *allocators)
{
	for(uint32_t i = 0; i < m_embeddedCsShaders; ++i)
		m_embeddedCsShader[i]->initializeWithAllocators(allocators);
	for(uint32_t i = 0; i < m_embeddedPsShaders; ++i)
		m_embeddedPsShader[i]->initializeWithAllocators(allocators);
	for(uint32_t i = 0; i < m_embeddedVsShaders; ++i)
		m_embeddedVsShader[i]->initializeWithAllocators(allocators);
	for(uint32_t i = 0; i < m_embeddedEsShaders; ++i)
		m_embeddedEsShader[i]->initializeWithAllocators(allocators);
	for(uint32_t i = 0; i < m_embeddedGsShaders; ++i)
		m_embeddedGsShader[i]->initializeWithAllocators(allocators);
	for(uint32_t i = 0; i < m_embeddedLsShaders; ++i)
		m_embeddedLsShader[i]->initializeWithAllocators(allocators);
	for(uint32_t i = 0; i < m_embeddedHsShaders; ++i)
		m_embeddedHsShader[i]->initializeWithAllocators(allocators);
	for(uint32_t i = 0; i < m_embeddedCsVsShaders; ++i)
		m_embeddedCsVsShader[i]->initializeWithAllocators(allocators);
}
