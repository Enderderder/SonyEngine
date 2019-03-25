/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_FRAMEWORK_H_
#define _SCE_GNM_FRAMEWORK_FRAMEWORK_H_

#include "../toolkit/allocator.h"
#include "../toolkit/shader_loader.h"
#include "../toolkit/geommath/geommath.h"
#include "../toolkit/allocators.h"
#include <pthread.h>

#define SAMPLE_FRAMEWORK_FSROOT "/app0"

// Uncomment this to enable Razor GPU thread trace for all samples. This requires PA Debug = "Yes" in target settings.
//#define ENABLE_RAZOR_GPU_THREAD_TRACE
#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
#define RAZOR_GPU_THREAD_TRACE_FILENAME		"/hostapp/thread_trace.rtt"
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE

namespace Framework
{
	enum {kResourceNameLength = 64};

	void waitForGraphicsWrites(sce::Gnmx::GnmxGfxContext *gfxc);
	void waitForGraphicsWritesToRenderTarget(sce::Gnmx::GnmxGfxContext *gfxc, sce::Gnm::RenderTarget *renderTarget, unsigned waitTargetSlot);
	void waitForGraphicsWritesToDepthRenderTarget(sce::Gnmx::GnmxGfxContext *gfxc, sce::Gnm::DepthRenderTarget *depthRenderTarget, unsigned waitTargetSlot);

	int Modulo( int x, int m );
	float saturate(float value);
	float smoothStep(float edge0, float edge1, float x);

	struct Label
	{
		uint32_t m_value;
		uint8_t m_reserved[4];
	} __attribute__((__aligned__(8)));

	struct ShaderResourceBinding
	{
		sce::Gnm::ShaderInputUsageType m_usageType;
		int m_startRegister;
		int m_apiSlot;
		int m_isValid;
	};

	struct ShaderResourceBindings
	{
		ShaderResourceBinding *m_binding;
		int m_bindings;
		uint32_t m_reserved;
		ShaderResourceBindings(const sce::Gnm::InputUsageSlot *slot, int slots, ShaderResourceBinding *binding, int bindings);
		bool areValid() const;
	};

	class Marker
	{
		sce::Gnmx::GnmxGfxContext &m_gfxc;
	public:
		Marker(sce::Gnmx::GnmxGfxContext &gfxc, const char *debugString);
		~Marker();
	};

	struct Random
	{
		uint64_t m_state;
		uint64_t GetULong();
		uint32_t GetUint();
		uint16_t GetUShort();
		uint8_t GetUByte();
		uint32_t GetUint( uint32_t a );
		uint32_t GetUint( uint32_t a, uint32_t b );
		float GetFloat();
		float GetFloat( float a );
		float GetFloat( float a, float b );
		Vector3 GetUnitVector3();
	};

	void AcquireFileContents(void*& pointer, uint32_t& bytes, const char* filename);
	void ReleaseFileContents(void* pointer);

	struct VsShader
	{
		sce::Gnmx::VsShader *m_shader;
		void *m_fetchShader;
		sce::Gnmx::InputOffsetsCache m_offsetsTable;
	};

	struct PsShader
	{
		sce::Gnmx::PsShader *m_shader;
		sce::Gnmx::InputOffsetsCache m_offsetsTable;
	};

	struct CsShader
	{
		sce::Gnmx::CsShader *m_shader;
		sce::Gnmx::InputOffsetsCache m_offsetsTable;
	};

	struct HsShader
	{
		sce::Gnmx::HsShader *m_shader;
		sce::Gnmx::InputOffsetsCache m_offsetsTable;
	};

	struct EsShader
	{
		sce::Gnmx::EsShader *m_shader;
		void *m_fetchShader;
		sce::Gnmx::InputOffsetsCache m_offsetsTable;
	};

	struct LsShader
	{
		sce::Gnmx::LsShader *m_shader;
		void *m_fetchShader;
		sce::Gnmx::InputOffsetsCache m_offsetsTable;
	};

	struct GsShader
	{
		sce::Gnmx::GsShader *m_shader;
		sce::Gnmx::InputOffsetsCache m_offsetsTable;
	};

	struct CsVsShader
	{
		sce::Gnmx::CsVsShader *m_shader;
		void *m_fetchShaderVs;
		void *m_fetchShaderCs;
		sce::Gnmx::InputOffsetsCache m_offsetsTableVs;
		sce::Gnmx::InputOffsetsCache m_offsetsTableCs;
	};

	struct Memory
	{
		const char *m_name;
		uint8_t *m_begin;
		uint8_t *m_end;
		int length() const 
		{
			return m_end - m_begin;
		}
	};

	struct ResizableMemory : public Memory
	{
		virtual int resize(int size) const = 0;
	};

	struct ReadFile : public Memory
	{
		ReadFile(const char *filename);
		~ReadFile();
	};

	struct WriteFile : public ResizableMemory
	{
		WriteFile(const char *filename);
		~WriteFile();
		int resize(int size) const override;
	};

	int LoadVsShader(VsShader *destination,const Memory& source,sce::Gnmx::Toolkit::Allocators *allocators);
	int LoadVsMakeFetchShader(VsShader *destination,const Memory& source,sce::Gnmx::Toolkit::Allocators *allocators,sce::Gnm::FetchShaderInstancingMode const *instancing = nullptr,const uint32_t numElementsInInstancingTable = 0);
	int LoadPsShader(PsShader *destination,const Memory& source,sce::Gnmx::Toolkit::Allocators *allocators);
	int LoadCsShader(CsShader *destination,const Memory& source,sce::Gnmx::Toolkit::Allocators *allocators);
	int LoadHsShader(HsShader *destination,const Memory& source,sce::Gnmx::Toolkit::Allocators *allocators);
	int LoadEsShader(EsShader *destination,const Memory& source,sce::Gnmx::Toolkit::Allocators *allocators);
	int LoadLsShader(LsShader *destination,const Memory& source,sce::Gnmx::Toolkit::Allocators *allocators);
	int LoadGsShader(GsShader *destination,const Memory& source,sce::Gnmx::Toolkit::Allocators *allocators);
	int LoadCsVsShader(CsVsShader *destination,const Memory& source,sce::Gnmx::Toolkit::Allocators *allocators);
	int LoadCsVsMakeFetchShaders(CsVsShader *destination,const Memory& source,sce::Gnmx::Toolkit::Allocators *allocators,sce::Gnm::FetchShaderInstancingMode const *instancing = nullptr,const uint32_t numElementsInInstancingTable = 0);

	sce::Gnmx::VsShader* LoadVsMakeFetchShader(void** fetchShader, const char* filename, sce::Gnmx::Toolkit::Allocators *allocators, sce::Gnm::FetchShaderInstancingMode const *instancing, const uint32_t numElementsInInstancingTable);
	inline sce::Gnmx::VsShader* LoadVsMakeFetchShader(void** fetchShader, const char* filename, sce::Gnmx::Toolkit::Allocators *allocators)
	{
		return LoadVsMakeFetchShader(fetchShader, filename, allocators, (sce::Gnm::FetchShaderInstancingMode const*)nullptr, 0);
	}
	sce::Gnmx::VsShader* LoadVsShader(const char* filename, sce::Gnmx::Toolkit::Allocators *allocators);
	sce::Gnmx::PsShader* LoadPsShader(const char* filename, sce::Gnmx::Toolkit::Allocators *allocators);
	sce::Gnmx::CsShader* LoadCsShader(const char* filename, sce::Gnmx::Toolkit::Allocators *allocators);
	sce::Gnmx::HsShader* LoadHsShader(const char* filename, sce::Gnmx::Toolkit::Allocators *allocators);
	sce::Gnmx::EsShader* LoadEsShader(const char* filename, sce::Gnmx::Toolkit::Allocators *allocators);
	sce::Gnmx::LsShader* LoadLsShader(const char* filename, sce::Gnmx::Toolkit::Allocators *allocators);
	sce::Gnmx::GsShader* LoadGsShader(const char* filename, sce::Gnmx::Toolkit::Allocators *allocators);
	sce::Gnmx::CsVsShader* LoadCsVsMakeFetchShaders(void** fetchShaderVs, void** fetchShaderCs, const char* filename, sce::Gnmx::Toolkit::Allocators *allocators, sce::Gnm::FetchShaderInstancingMode const *instancing, const uint32_t numElementsInInstancingTable);
	inline sce::Gnmx::CsVsShader* LoadCsVsMakeFetchShaders(void** fetchShaderVs, void** fetchShaderCs, const char* filename, sce::Gnmx::Toolkit::Allocators *allocators)
	{
		return LoadCsVsMakeFetchShaders(fetchShaderVs, fetchShaderCs, filename, allocators, (sce::Gnm::FetchShaderInstancingMode const*)nullptr, 0);
	}
	sce::Gnmx::CsVsShader* LoadCsVsShader(const char* filename, sce::Gnmx::Toolkit::Allocators *allocators);

	sce::Gnmx::VsShader *LoadVertexProgram  (sce::Shader::Binary::Program *sbp, const char* filename, sce::Gnmx::Toolkit::IAllocator *allocator);
	sce::Gnmx::PsShader *LoadPixelProgram   (sce::Shader::Binary::Program *sbp, const char* filename, sce::Gnmx::Toolkit::IAllocator *allocator);
	sce::Gnmx::CsShader *LoadComputeProgram (sce::Shader::Binary::Program *sbp, const char* filename, sce::Gnmx::Toolkit::IAllocator *allocator);
	sce::Gnmx::HsShader *LoadHullProgram    (sce::Shader::Binary::Program *sbp, const char* filename, sce::Gnmx::Toolkit::IAllocator *allocator);
	sce::Gnmx::EsShader *LoadExportProgram  (sce::Shader::Binary::Program *sbp, const char* filename, sce::Gnmx::Toolkit::IAllocator *allocator);
	sce::Gnmx::LsShader *LoadLocalProgram   (sce::Shader::Binary::Program *sbp, const char* filename, sce::Gnmx::Toolkit::IAllocator *allocator);
	sce::Gnmx::GsShader *LoadGeometryProgram(sce::Shader::Binary::Program *sbp, const char* filename, sce::Gnmx::Toolkit::IAllocator *allocator);
}

#endif // _SCE_GNM_FRAMEWORK_FRAMEWORK_H_
