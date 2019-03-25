/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "framework.h"
#include "../toolkit/embedded_shader.h"
#include <unistd.h>
#include <sceerror.h>
#include <gnm.h>
using namespace sce;

#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
#include <razor_gpu_thread_trace.h>
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE

void Framework::waitForGraphicsWrites(sce::Gnmx::GnmxGfxContext *gfxc)
{
	gfxc->waitForGraphicsWrites
	(
		0x00000000, 0xFFFFFFFF, 
		Gnm::kWaitTargetSlotAll, 
		Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
		Gnm::kExtendedCacheActionFlushAndInvalidateCbCache | Gnm::kExtendedCacheActionFlushAndInvalidateDbCache,
		Gnm::kStallCommandBufferParserDisable
	);
}

void Framework::waitForGraphicsWritesToRenderTarget(sce::Gnmx::GnmxGfxContext *gfxc, sce::Gnm::RenderTarget *renderTarget, unsigned waitTargetSlot)
{
	const unsigned baseSlice = renderTarget->getBaseArraySliceIndex();
	const unsigned slices = renderTarget->getLastArraySliceIndex() - baseSlice + 1;
	const unsigned sliceSizeIn256Bytes = renderTarget->getSliceSizeInBytes() >> 8;
	gfxc->waitForGraphicsWrites
	(
		renderTarget->getBaseAddress256ByteBlocks() + baseSlice * sliceSizeIn256Bytes, 
		slices * sliceSizeIn256Bytes, 
		waitTargetSlot,
		Gnm::kCacheActionNone, 
		Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, 
		Gnm::kStallCommandBufferParserDisable
	);
}

void Framework::waitForGraphicsWritesToDepthRenderTarget(sce::Gnmx::GnmxGfxContext *gfxc, sce::Gnm::DepthRenderTarget *renderTarget, unsigned waitTargetSlot)
{
	const unsigned baseSlice = renderTarget->getBaseArraySliceIndex();
	const unsigned slices = renderTarget->getLastArraySliceIndex() - baseSlice + 1;
	const unsigned sliceSizeIn256Bytes = renderTarget->getZSliceSizeInBytes() >> 8;
	gfxc->waitForGraphicsWrites
	(
		renderTarget->getZWriteAddress256ByteBlocks() + baseSlice * sliceSizeIn256Bytes, 
		slices * sliceSizeIn256Bytes, 
		waitTargetSlot,
		Gnm::kCacheActionNone, 
		Gnm::kExtendedCacheActionFlushAndInvalidateDbCache, 
		Gnm::kStallCommandBufferParserDisable
	);
}

int Framework::Modulo( int x, int m )
{
	return ( x % m + m ) % m;
}

float Framework::saturate(float value)
{
	return std::min(1.f, std::max(0.f, value));
}

float Framework::smoothStep(float edge0, float edge1, float x)
{
	x = saturate((x - edge0)/(edge1 - edge0));
	return x * x * (3 - 2 * x);
}

Framework::ShaderResourceBindings::ShaderResourceBindings(const Gnm::InputUsageSlot *slot, int slots, ShaderResourceBinding *binding, int bindings)
: m_binding(binding)
, m_bindings(bindings)
{
	for(int b = 0; b < bindings; ++b)
		for(int s = 0; s < slots; ++s)
			if(slot[s].m_apiSlot == binding[b].m_apiSlot && slot[s].m_usageType == binding[b].m_usageType)
			{
				binding[b].m_startRegister = slot[s].m_startRegister;
				binding[b].m_isValid = 1;
				break;
			}
}

bool Framework::ShaderResourceBindings::areValid() const
{
	for(int b = 0; b < m_bindings; ++b)
		if(0 == m_binding[b].m_isValid)
			return false;
	return true;
}

Framework::Marker::Marker(sce::Gnmx::GnmxGfxContext &gfxc, const char *debugString)
: m_gfxc(gfxc)
{
	m_gfxc.pushMarker(debugString);
}

Framework::Marker::~Marker()
{
	m_gfxc.popMarker();
}

uint64_t Framework::Random::GetULong() // get a random integer between [0..2**64-1]
{
	m_state = m_state * 2862933555777941757 + 3037000493;
	return m_state;
}

uint32_t Framework::Random::GetUint() // get a random integer between [0..2**32-1]
{
	return GetULong() >> (64-32);
}

uint16_t Framework::Random::GetUShort() // get a random integer between [0..2**16-1]
{
	return GetULong() >> (64-16);
}

uint8_t Framework::Random::GetUByte() // get a random integer between [0..2**8-1]
{
	return GetULong() >> (64-8);
}

uint32_t Framework::Random::GetUint( uint32_t a ) // get a random integer between [0..a]
{
	return GetUint() % (a + 1);
}

uint32_t Framework::Random::GetUint( uint32_t a, uint32_t b ) // get a random integer between [a..b]
{
	return a + GetUint( b - a );
}

float Framework::Random::GetFloat() // get a random value between [1..2)
{
	union
	{
		float m_f;
		uint32_t m_i;
	} u;
	u.m_i = 0x3f800000 | (GetULong() >> (64-23));
	return u.m_f;
}

float Framework::Random::GetFloat( float a ) // get a random value between [0..a)
{
	return GetFloat() * a - a;
}

float Framework::Random::GetFloat( float a, float b ) // get a random value between [a..b)
{
	return a + GetFloat( b - a );
}

Vector3 Framework::Random::GetUnitVector3() // get a random unit vector
{
	while(true)
	{
		Vector3 result(GetFloat(-1,1), GetFloat(-1,1), GetFloat(-1,1));
		if(length(result) <= 1.f)
			return normalize(result);
	}
}

void Framework::AcquireFileContents(void*& pointer, uint32_t& bytes, const char* filename)
{
//	SCE_GNM_ASSERT_MSG(access(filename, R_OK) == 0, "** Asset Not Found: %s\n", filename);
	FILE *fp = fopen(filename, "rb");
	SCE_GNM_ASSERT(fp);
	fseek(fp, 0, SEEK_END);
	bytes = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	pointer = malloc(bytes);
	fread(pointer, 1, bytes, fp);
	fclose(fp);
}

void Framework::ReleaseFileContents(void* pointer)
{
	free(pointer);
}

Framework::ReadFile::ReadFile(const char *filename)
{
	m_name = filename;
	m_begin = nullptr;
	m_end = nullptr;
	SCE_GNM_ASSERT_MSG(nullptr != filename, "filename parameter must not be nullptr.");
	FILE *file = fopen(filename, "rb");
	SCE_GNM_ASSERT_MSG(nullptr != file, "unable to open filename \"%s\" for reading.", filename);
	fseek(file,0,SEEK_END);
	const uint32_t lengthInBytes = ftell(file);
	fseek(file,0,SEEK_SET);
	m_begin = static_cast<uint8_t*>(malloc(lengthInBytes));
	m_end = m_begin + lengthInBytes;
	fread(m_begin,1,lengthInBytes,file);
	fclose(file);
}

Framework::ReadFile::~ReadFile()
{
	if(nullptr != m_begin)
		free(m_begin);
}

Framework::WriteFile::WriteFile(const char *filename)
{
	m_name = filename;
	m_begin = nullptr;
	m_end = nullptr;
	SCE_GNM_ASSERT_MSG(nullptr != filename,"filename parameter must not be nullptr.");
}

int Framework::WriteFile::resize(int size) const
{
	uint8_t *temp = static_cast<uint8_t *>(malloc(size));
	memset(temp, 0, size);
	if(nullptr == temp)
		return -1;
	memcpy(temp, m_begin, std::min(size, length()));
	if(nullptr != m_begin)
		free(m_begin);
	const_cast<uint8_t *&>(m_begin) = temp;
	const_cast<uint8_t *&>(m_end) = m_begin + size;
	return 0;
}

Framework::WriteFile::~WriteFile()
{
	FILE *file = fopen(m_name, "wb");
	SCE_GNM_ASSERT_MSG(nullptr != file,"unable to open filename \"%s\" for writing.", m_name);
	fwrite(m_begin, length(), 1, file);
	fclose(file);
	if(nullptr != m_begin)
		free(m_begin);
}

int Framework::LoadVsShader(VsShader *destination,const Memory &source, sce::Gnmx::Toolkit::Allocators *allocators)
{
	Gnmx::Toolkit::EmbeddedVsShader embeddedVsShader ={static_cast<uint32_t *>(static_cast<void *>(source.m_begin)),source.m_name};
	embeddedVsShader.initializeWithAllocators(allocators);
	destination->m_shader = embeddedVsShader.m_shader;
	destination->m_fetchShader = nullptr;
	destination->m_offsetsTable = embeddedVsShader.m_offsetsTable;
	return 0;
}

int Framework::LoadVsMakeFetchShader(VsShader *destination,const Memory &source,Gnmx::Toolkit::Allocators *allocators,Gnm::FetchShaderInstancingMode const *instancing,const uint32_t numElementsInInstancingTable)
{
	LoadVsShader(destination, source, allocators);
	destination->m_fetchShader = allocators->m_garlic.allocate(Gnmx::Toolkit::calculateMemoryRequiredForVsFetchShader(source.m_begin));
	uint32_t shaderModifier;
	Gnmx::generateVsFetchShader(destination->m_fetchShader,&shaderModifier,destination->m_shader,instancing,numElementsInInstancingTable);
	destination->m_shader->applyFetchShaderModifier(shaderModifier);
	return 0;
}

int Framework::LoadPsShader(PsShader *destination,const Memory &source, sce::Gnmx::Toolkit::Allocators *allocators)
{
	Gnmx::Toolkit::EmbeddedPsShader embeddedPsShader ={static_cast<uint32_t *>(static_cast<void *>(source.m_begin)), source.m_name};
	embeddedPsShader.initializeWithAllocators(allocators);
	destination->m_shader = embeddedPsShader.m_shader;
	destination->m_offsetsTable = embeddedPsShader.m_offsetsTable;
	return 0;
}

int Framework::LoadCsShader(CsShader *destination,const Memory &source, sce::Gnmx::Toolkit::Allocators *allocators)
{
	Gnmx::Toolkit::EmbeddedCsShader embeddedCsShader ={static_cast<uint32_t *>(static_cast<void *>(source.m_begin)), source.m_name};
	embeddedCsShader.initializeWithAllocators(allocators);
	destination->m_shader = embeddedCsShader.m_shader;
	destination->m_offsetsTable = embeddedCsShader.m_offsetsTable;
	return 0;
}

int Framework::LoadHsShader(HsShader *destination,const Memory &source, sce::Gnmx::Toolkit::Allocators *allocators)
{
	Gnmx::Toolkit::EmbeddedHsShader embeddedHsShader ={static_cast<uint32_t *>(static_cast<void *>(source.m_begin)), source.m_name};
	embeddedHsShader.initializeWithAllocators(allocators);
	destination->m_shader = embeddedHsShader.m_shader;
	destination->m_offsetsTable = embeddedHsShader.m_offsetsTable;
	return 0;
}

int Framework::LoadEsShader(EsShader *destination,const Memory &source, sce::Gnmx::Toolkit::Allocators *allocators)
{
	Gnmx::Toolkit::EmbeddedEsShader embeddedEsShader ={static_cast<uint32_t *>(static_cast<void *>(source.m_begin)), source.m_name};
	embeddedEsShader.initializeWithAllocators(allocators);
	destination->m_shader = embeddedEsShader.m_shader;
	destination->m_fetchShader = nullptr;
	destination->m_offsetsTable = embeddedEsShader.m_offsetsTable;
	return 0;
}

int Framework::LoadLsShader(LsShader *destination,const Memory &source, sce::Gnmx::Toolkit::Allocators *allocators)
{
	Gnmx::Toolkit::EmbeddedLsShader embeddedLsShader ={static_cast<uint32_t *>(static_cast<void *>(source.m_begin)), source.m_name};
	embeddedLsShader.initializeWithAllocators(allocators);
	destination->m_shader = embeddedLsShader.m_shader;
	destination->m_fetchShader = nullptr;
	destination->m_offsetsTable = embeddedLsShader.m_offsetsTable;
	return 0;
}

int Framework::LoadGsShader(GsShader *destination,const Memory &source, sce::Gnmx::Toolkit::Allocators *allocators)
{
	char gsName[128];
	char vsName[128];
	snprintf(gsName,128,"%s (Geometry)",source.m_name);
	snprintf(vsName,128,"%s (Vertex)",source.m_name);
	Gnmx::Toolkit::EmbeddedGsShader embeddedGsShader ={static_cast<uint32_t *>(static_cast<void *>(source.m_begin)),gsName,vsName};
	embeddedGsShader.initializeWithAllocators(allocators);
	destination->m_shader = embeddedGsShader.m_shader;
	destination->m_offsetsTable = embeddedGsShader.m_offsetsTable;
	return 0;
}

int Framework::LoadCsVsShader(CsVsShader *destination,const Memory &source, sce::Gnmx::Toolkit::Allocators *allocators)
{
	char csName[128],vsName[128];
	sprintf(csName,"%s (Compute)",source.m_name);
	sprintf(vsName,"%s (Vertex)",source.m_name);
	Gnmx::Toolkit::EmbeddedCsVsShader embeddedCsVsShader ={static_cast<uint32_t *>(static_cast<void *>(source.m_begin)),csName,vsName};
	embeddedCsVsShader.initializeWithAllocators(allocators);
	destination->m_shader = embeddedCsVsShader.m_shader;
	destination->m_fetchShaderCs = nullptr;
	destination->m_fetchShaderVs = nullptr;
	destination->m_offsetsTableCs = embeddedCsVsShader.m_offsetsTableCs;
	destination->m_offsetsTableVs = embeddedCsVsShader.m_offsetsTableVs;
	return 0;
}

int Framework::LoadCsVsMakeFetchShaders(CsVsShader *destination,const Memory &source, Gnmx::Toolkit::Allocators *allocators,Gnm::FetchShaderInstancingMode const *instancing,const uint32_t numElementsInInstancingTable)
{
	LoadCsVsShader(destination, source, allocators);
	char csName[128],vsName[128];
	sprintf(csName,"%s (Compute)",source.m_name);
	sprintf(vsName,"%s (Vertex)",source.m_name);
	destination->m_fetchShaderVs = allocators->m_garlic.allocate(Gnmx::Toolkit::calculateMemoryRequiredForCsVsFetchShaderVs(source.m_begin));
	destination->m_fetchShaderCs = allocators->m_garlic.allocate(Gnmx::Toolkit::calculateMemoryRequiredForCsVsFetchShaderCs(source.m_begin));
	uint32_t shaderModifierVs,shaderModifierCs;
	Gnmx::generateVsFetchShader(destination->m_fetchShaderVs,&shaderModifierVs,destination->m_shader,instancing,numElementsInInstancingTable);
	Gnmx::generateCsFetchShader(destination->m_fetchShaderCs,&shaderModifierCs,destination->m_shader,instancing,numElementsInInstancingTable);
	destination->m_shader->applyFetchShaderModifiers(shaderModifierVs,shaderModifierCs);
	return 0;
}

Gnmx::VsShader* Framework::LoadVsShader(const char* filename, Gnmx::Toolkit::Allocators *allocators)
{
	VsShader vsShader;
	LoadVsShader(&vsShader, ReadFile(filename), allocators);
	return vsShader.m_shader;
}

Gnmx::VsShader* Framework::LoadVsMakeFetchShader(void **fetchShader,const char* filename,Gnmx::Toolkit::Allocators *allocators,Gnm::FetchShaderInstancingMode const *instancing,const uint32_t numElementsInInstancingTable)
{
	VsShader vsShader;
	LoadVsMakeFetchShader(&vsShader, ReadFile(filename), allocators,instancing,numElementsInInstancingTable);
	*fetchShader = vsShader.m_fetchShader;
	return vsShader.m_shader;
}

Gnmx::PsShader* Framework::LoadPsShader(const char* filename, Gnmx::Toolkit::Allocators *allocators)
{
	PsShader psShader;
	LoadPsShader(&psShader, ReadFile(filename), allocators);
	return psShader.m_shader;
}

Gnmx::CsShader* Framework::LoadCsShader(const char* filename, Gnmx::Toolkit::Allocators *allocators)
{
	CsShader csShader;
	LoadCsShader(&csShader, ReadFile(filename), allocators);
	return csShader.m_shader;
}

Gnmx::HsShader* Framework::LoadHsShader(const char* filename, Gnmx::Toolkit::Allocators *allocators)
{
	HsShader hsShader;
	LoadHsShader(&hsShader, ReadFile(filename), allocators);
	return hsShader.m_shader;
}

Gnmx::EsShader* Framework::LoadEsShader(const char* filename, Gnmx::Toolkit::Allocators *allocators)
{
	EsShader esShader;
	LoadEsShader(&esShader, ReadFile(filename), allocators);
	return esShader.m_shader;
}

Gnmx::LsShader* Framework::LoadLsShader(const char* filename, Gnmx::Toolkit::Allocators *allocators)
{
	LsShader lsShader;
	LoadLsShader(&lsShader, ReadFile(filename), allocators);
	return lsShader.m_shader;
}

Gnmx::GsShader* Framework::LoadGsShader(const char* filename, Gnmx::Toolkit::Allocators *allocators)
{
	GsShader gsShader;
	LoadGsShader(&gsShader, ReadFile(filename), allocators);
	return gsShader.m_shader;
}

Gnmx::CsVsShader* Framework::LoadCsVsMakeFetchShaders(void **fetchShaderVs, void **fetchShaderCs, const char* filename, Gnmx::Toolkit::Allocators *allocators, Gnm::FetchShaderInstancingMode const *instancing, const uint32_t numElementsInInstancingTable)
{
	CsVsShader csVsShader;
	LoadCsVsMakeFetchShaders(&csVsShader, ReadFile(filename), allocators, instancing, numElementsInInstancingTable);
	*fetchShaderVs = csVsShader.m_fetchShaderVs;
	*fetchShaderCs = csVsShader.m_fetchShaderCs;
	return csVsShader.m_shader;
}

Gnmx::CsVsShader* Framework::LoadCsVsShader(const char* filename, Gnmx::Toolkit::Allocators *allocators)
{
	CsVsShader csVsShader;
	LoadCsVsShader(&csVsShader, ReadFile(filename), allocators);
	return csVsShader.m_shader;
}

sce::Gnmx::VsShader *Framework::LoadVertexProgram(sce::Shader::Binary::Program *sbp, const char* filename, Gnmx::Toolkit::IAllocator *allocator)
{
	void* pointer; uint32_t bytes;
	AcquireFileContents(pointer, bytes, filename);
	return Gnmx::Toolkit::loadAndAllocateVertexProgram(sbp, pointer, bytes, allocator);
}

sce::Gnmx::PsShader *Framework::LoadPixelProgram(sce::Shader::Binary::Program *sbp, const char* filename, Gnmx::Toolkit::IAllocator *allocator)
{
	void* pointer; uint32_t bytes;
	AcquireFileContents(pointer, bytes, filename);
	return Gnmx::Toolkit::loadAndAllocatePixelProgram(sbp, pointer, bytes, allocator);
}

sce::Gnmx::CsShader *Framework::LoadComputeProgram(sce::Shader::Binary::Program *sbp, const char* filename, Gnmx::Toolkit::IAllocator *allocator)
{
	void* pointer; uint32_t bytes;
	AcquireFileContents(pointer, bytes, filename);
	return Gnmx::Toolkit::loadAndAllocateComputeProgram(sbp, pointer, bytes, allocator);
}

sce::Gnmx::HsShader *Framework::LoadHullProgram(sce::Shader::Binary::Program *sbp, const char* filename, Gnmx::Toolkit::IAllocator *allocator)
{
	void* pointer; uint32_t bytes;
	AcquireFileContents(pointer, bytes, filename);
	return Gnmx::Toolkit::loadAndAllocateHullProgram(sbp, pointer, bytes, allocator);
}

sce::Gnmx::EsShader *Framework::LoadExportProgram(sce::Shader::Binary::Program *sbp, const char* filename, Gnmx::Toolkit::IAllocator *allocator)
{
	void* pointer; uint32_t bytes;
	AcquireFileContents(pointer, bytes, filename);
	return Gnmx::Toolkit::loadAndAllocateExportProgram(sbp, pointer, bytes, allocator);
}

sce::Gnmx::LsShader *Framework::LoadLocalProgram(sce::Shader::Binary::Program *sbp, const char* filename, Gnmx::Toolkit::IAllocator *allocator)
{
	void* pointer; uint32_t bytes;
	AcquireFileContents(pointer, bytes, filename);
	return Gnmx::Toolkit::loadAndAllocateLocalProgram(sbp, pointer, bytes, allocator);
}

sce::Gnmx::GsShader *Framework::LoadGeometryProgram(sce::Shader::Binary::Program *sbp, const char* filename, Gnmx::Toolkit::IAllocator *allocator)
{
	void* pointer; uint32_t bytes;
	AcquireFileContents(pointer, bytes, filename);
	return Gnmx::Toolkit::loadAndAllocateGeometryProgram(sbp, pointer, bytes, allocator);
}

