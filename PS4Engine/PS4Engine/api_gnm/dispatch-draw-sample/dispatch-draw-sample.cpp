/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "std_cbuffer.h"
using namespace sce;

#define BUILD_DATA_FASTER_SLIGHTLY_LARGER 1

#include <gnmx/dispatchdraw.h>

namespace
{
	Framework::GnmSampleFramework framework;
	Matrix4 viewToWorldForCulling;
	Matrix4 viewProjectionMatrixForCulling;
	bool bViewProjectionMatrixForCullingUpdated = true;

	void IntersectPlanes(Vector4& p, const Vector4& p1, const Vector4& p2, const Vector4& p3)
	{
		const float EPSILON = 0.0001f;
		const Vector3 u = cross(p2.getXYZ(), p3.getXYZ());
		const float denom = dot(p1.getXYZ(), u);
		SCE_GNM_ASSERT_MSG(fabsf(denom) > EPSILON, "fabsf(denom) > EPSILON");
#ifndef _DEBUG
		SCE_GNM_UNUSED(EPSILON);
#endif
		p.setXYZ((-p1.getW() * u + cross(p1.getXYZ(), -p3.getW() * p2.getXYZ() - -p2.getW() * p3.getXYZ())) / denom);
		p.setW(1.f);
		const float a = fabsf(dot(p, p1));
		const float b = fabsf(dot(p, p2));
		const float c = fabsf(dot(p, p3));
		SCE_GNM_ASSERT(a < EPSILON);
		SCE_GNM_ASSERT(b < EPSILON);
		SCE_GNM_ASSERT(c < EPSILON);
	}

	Framework::DebugVertex MakeDebugVertex(Vector4 const& position, Vector4 const& color)
	{
		Framework::DebugVertex debugVertex;
		debugVertex.m_position.x = position.getX();
		debugVertex.m_position.y = position.getY();
		debugVertex.m_position.z = position.getZ();
		debugVertex.m_color.x = color.getX();
		debugVertex.m_color.y = color.getY();
		debugVertex.m_color.z = color.getZ();
		debugVertex.m_color.w = color.getW();
		return debugVertex;
	}

	//============================================================================================
	// TempAllocator is a simple Gnmx::Toolkit::IAllocator which allows a single allocation
	// from one temp buffer.

	void *TempAllocator_allocate(void* instance, uint32_t size, sce::Gnm::AlignmentType alignment);
	void TempAllocator_release(void* instance, void* pointer);

	struct TempAllocator : public Gnmx::Toolkit::IAllocator {
		void* m_pTempBuffer;
		uint32_t m_sizeofTempBuffer;
		bool m_bIsAllocated;

		TempAllocator(void* pTempBuffer, uint32_t size)
		{
			m_instance = this;
			m_pTempBuffer = pTempBuffer;
			m_sizeofTempBuffer = size;
			m_bIsAllocated = false;
			m_allocate = TempAllocator_allocate;
			m_release = TempAllocator_release;
		}
	};

	void *TempAllocator_allocate(void* instance, uint32_t size, sce::Gnm::AlignmentType alignment)
	{
		TempAllocator* pTempAllocator = (TempAllocator*)instance;
		if (pTempAllocator->m_bIsAllocated || pTempAllocator->m_pTempBuffer == NULL || pTempAllocator->m_sizeofTempBuffer < size)
			return NULL;
		uintptr_t pAllocation = (uintptr_t)pTempAllocator->m_pTempBuffer;
		uintptr_t maskAlign = (uintptr_t)(alignment-1);
		if (alignment && !(alignment & maskAlign)) {
			pAllocation = (pAllocation + maskAlign) &~maskAlign;
			if (pAllocation + size > (uintptr_t)pTempAllocator->m_pTempBuffer + pTempAllocator->m_sizeofTempBuffer)
				return NULL;
		}
		pTempAllocator->m_bIsAllocated = true;
		return (void*)pAllocation;
	}
	void TempAllocator_release(void* instance, void* pointer)
	{
		TempAllocator* pTempAllocator = (TempAllocator*)instance;
		if (!pTempAllocator->m_bIsAllocated || pointer < pTempAllocator->m_pTempBuffer || pointer >= (void*)((uintptr_t)pTempAllocator->m_pTempBuffer + pTempAllocator->m_sizeofTempBuffer))
			return;
		pTempAllocator->m_bIsAllocated = false;
	}

	//============================================================================================

	void dispatchDraw_overrideComputeContantBuffer(Gnmx::GnmxGfxContext *gfxc, const Gnmx::DispatchDrawTriangleCullIndexData *pDispatchDrawIndexData, uint32_t userSgprConstBuffer0, Gnm::Buffer constantBufferForCompute)
	{
		// This is just a copy of GfxContext::dispatchDraw that overrides the user SGPRs for constant buffer 0 for the compute shader.
		// Normally, the compute shader in dispatchDraw always receives the same input data as the vertex shader.  
		// This hack allows us to cull in compute using a different view frustum than is used to transform the vertices in the vertex 
		// shader, which allows us to freeze the view frustum and look at what the dispatch draw compute shader was culling from a 
		// different angle.
		Gnmx::GnmxDispatchCommandBuffer &m_acb = gfxc->m_acb;
		Gnmx::GnmxDrawCommandBuffer &m_dcb = gfxc->m_dcb;

#if !SCE_GNMX_ENABLE_GFX_LCUE
		Gnmx::GnmxConstantCommandBuffer &m_ccb = gfxc->m_ccb;
		Gnmx::ConstantUpdateEngine &m_cue = gfxc->m_cue;
#else // !SCE_GNMX_ENABLE_GFX_LCUE
		Gnmx::LightweightGraphicsConstantUpdateEngine &m_lwcue = gfxc->m_lwcue;
#endif // !SCE_GNMX_ENABLE_GFX_LCUE		

		const uint32_t kDispatchDrawFlagInDispatchDraw = Gnmx::GnmxGfxContext::kDispatchDrawFlagInDispatchDraw;
		typedef Gnmx::BaseGfxContext::DispatchDrawData DispatchDrawData;
		typedef Gnmx::BaseGfxContext::DispatchDrawSharedData DispatchDrawSharedData;
		typedef Gnmx::BaseGfxContext::DispatchDrawV0Data DispatchDrawV0Data;
		DispatchDrawSharedData const &m_dispatchDrawSharedData = gfxc->m_dispatchDrawSharedData;
		DispatchDrawSharedData *&m_pDispatchDrawSharedData = gfxc->m_pDispatchDrawSharedData;
		uint32_t &m_dispatchDrawIndexDeallocMask = gfxc->m_dispatchDrawIndexDeallocMask;
		uint16_t &m_dispatchDrawNumInstancesMinus1 = gfxc->m_dispatchDrawNumInstancesMinus1;
		uint16_t &m_dispatchDrawInstanceStepRate0Minus1 = gfxc->m_dispatchDrawInstanceStepRate0Minus1;
		uint16_t &m_dispatchDrawInstanceStepRate1Minus1 = gfxc->m_dispatchDrawInstanceStepRate1Minus1;
		uint16_t &m_dispatchDrawFlags = gfxc->m_dispatchDrawFlags;

		//To prevent dispatch draw deadlocks, this must be set to the value of the internal config register VGT_VTX_VECT_EJECT_REG + 2.  VGT_VTX_VECT_EJECT_REG = 127, so numPrimsPerVgt must be 129 for dispatch draw
		static const uint32_t numPrimsPerVgt = 129;
		// The subDrawIndexCount must be a multiple of 2*numPrimsPerVgt primitives worth of indices
		// (numPrimsPerVgt*3*2 for triangle primitives) to prevent deadlocks.
		// There is not much reason to set it to any multiple of the minimum value (774 indices), as
		// that is already large enough that overhead from sub-draws is probably minimal, and using
		// a larger value would correspondingly reduce the effective IRB size.
		static const uint32_t subDrawIndexCount = numPrimsPerVgt*3*2;

		uint32_t indexOffset = 0;		//Do we need to be able to set the index offset for dispatch draw?

		SCE_GNM_ASSERT_MSG((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) != 0, "dispatchDraw can only be called between beginDispatchDraw and endDispatchDraw");
		SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_magic == Gnmx::kDispatchDrawTriangleCullIndexDataMagic, "dispatchDraw: index data magic is wrong");
		SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_versionMajor == Gnmx::kDispatchDrawTriangleCullIndexDataVersionMajor, "dispatchDraw: index data version major index does not match");
		SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_numIndexDataBlocks > 0 && pDispatchDrawIndexData->m_numIndexDataBlocks <= 0xFFFF, "dispatchDraw: m_numIndexDataBlocks must be in the range [1:65536]");
		SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_numIndexBits >= 1 && pDispatchDrawIndexData->m_numIndexBits <= 16, "dispatchDraw: m_numIndexBits must be in the range [1:16]");
		SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_numIndexSpaceBits < pDispatchDrawIndexData->m_numIndexBits, "dispatchDraw: m_numIndexSpaceBits must be less than m_numIndexBits");

		// Store bufferInputData and numBlocksTotal to kShaderInputUsagePtrDispatchDraw data
#if !SCE_GNMX_ENABLE_GFX_LCUE
		sce::Gnmx::CsShader const* pDdCsShader = m_cue.m_currentAcbCSB;
#else
		sce::Gnmx::CsShader const* pDdCsShader = ((sce::Gnmx::CsShader const*)m_lwcue.getBoundShader((sce::Gnm::ShaderStage)sce::Gnmx::LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute));
#endif
		if (pDdCsShader->m_version >= 1)
		{
			if (m_pDispatchDrawSharedData == NULL) {
				m_pDispatchDrawSharedData = (DispatchDrawSharedData*)m_acb.allocateFromCommandBuffer(sizeof(DispatchDrawSharedData), Gnm::kEmbeddedDataAlignment4);
				memcpy(m_pDispatchDrawSharedData, &m_dispatchDrawSharedData, sizeof(DispatchDrawSharedData));
			}
			bool const bIsInstancing = true;
			const size_t sizeofDispatchDrawData = bIsInstancing ? Gnmx::kSizeofDispatchDrawTriangleCullV1DataInstancingEnabled : Gnmx::kSizeofDispatchDrawTriangleCullV1DataInstancingDisabled;
			DispatchDrawData *pDispatchDrawData = (DispatchDrawData*)m_acb.allocateFromCommandBuffer(sizeofDispatchDrawData, Gnm::kEmbeddedDataAlignment4);
			pDispatchDrawData->m_pShared = m_pDispatchDrawSharedData;
			pDispatchDrawData->m_bufferInputIndexData = pDispatchDrawIndexData->m_bufferInputIndexData;
			pDispatchDrawData->m_numIndexDataBlocks = (uint16_t)pDispatchDrawIndexData->m_numIndexDataBlocks;
			pDispatchDrawData->m_numIndexBits = pDispatchDrawIndexData->m_numIndexBits;
			pDispatchDrawData->m_numInstancesPerTgMinus1 = pDispatchDrawIndexData->m_numInstancesPerTgMinus1;
			if (bIsInstancing) {
				//NOTE: only required by instancing shader
				pDispatchDrawData->m_instanceStepRate0Minus1 = m_dispatchDrawInstanceStepRate0Minus1;
				pDispatchDrawData->m_instanceStepRate1Minus1 = m_dispatchDrawInstanceStepRate1Minus1;
			}

			// Pass a pointer to the dispatch draw data for the CUE to pass to shaders as kShaderInputUsagePtrDispatchDraw.
			// Its contents must not be modified until after this dispatchDraw call's shaders have finished running.
#if !SCE_GNMX_ENABLE_GFX_LCUE
			m_cue.setDispatchDrawData(pDispatchDrawData, sizeofDispatchDrawData);
#else // !SCE_GNMX_ENABLE_GFX_LCUE
			m_lwcue.setDispatchDrawData(pDispatchDrawData, sizeofDispatchDrawData);
#endif	// !SCE_GNMX_ENABLE_GFX_LCUE	
		}
		else
		{
			uint32_t firstBlock = 0;		//Do we need to be able to render a partial set of blocks?
			// We do not use the CCB to prefetch the dispatch draw control data.
			// Instead we allocate copies from the ACB which the CUE prefetches explicitly.
			// As the current version of DispatchDrawTriangleCullData contains data which changes with every draw call,
			// we must currently allocate and copy 84 bytes for every dispatchDraw call.
			// In the future, we could put data which changes infrequently DispatchDrawTriangleCullCommonData in a 
			// separate structure with a pointer in the base data, which would generally reduce this copy and command 
			// buffer allocation to 52 bytes per dispatchDraw call.
			DispatchDrawV0Data* pDispatchDrawData = (DispatchDrawV0Data*)m_acb.allocateFromCommandBuffer(sizeof(DispatchDrawV0Data), Gnm::kEmbeddedDataAlignment4);
			pDispatchDrawData->m_bufferIrb = m_dispatchDrawSharedData.m_bufferIrb;
			pDispatchDrawData->m_bufferInputIndexData = pDispatchDrawIndexData->m_bufferInputIndexData;
			pDispatchDrawData->m_numIndexDataBlocks = (uint16_t)pDispatchDrawIndexData->m_numIndexDataBlocks;
			pDispatchDrawData->m_gdsOffsetOfIrbWptr = m_dispatchDrawSharedData.m_gdsOffsetOfIrbWptr;
			pDispatchDrawData->m_sizeofIrbInIndices = m_dispatchDrawSharedData.m_bufferIrb.m_regs[2];
			pDispatchDrawData->m_clipCullSettings = m_dispatchDrawSharedData.m_cullSettings;
			pDispatchDrawData->m_numIndexBits = pDispatchDrawIndexData->m_numIndexBits;
			pDispatchDrawData->m_numInstancesPerTgMinus1 = pDispatchDrawIndexData->m_numInstancesPerTgMinus1;
			pDispatchDrawData->m_firstIndexDataBlock = (uint16_t)firstBlock;
			pDispatchDrawData->m_reserved = 0;
			pDispatchDrawData->m_quantErrorScreenX = m_dispatchDrawSharedData.m_quantErrorScreenX;
			pDispatchDrawData->m_quantErrorScreenY = m_dispatchDrawSharedData.m_quantErrorScreenY;
			pDispatchDrawData->m_gbHorizClipAdjust = m_dispatchDrawSharedData.m_gbHorizClipAdjust;
			pDispatchDrawData->m_gbVertClipAdjust = m_dispatchDrawSharedData.m_gbVertClipAdjust;
			//NOTE: only required by instancing shader
			pDispatchDrawData->m_instanceStepRate0Minus1 = m_dispatchDrawInstanceStepRate0Minus1;
			pDispatchDrawData->m_instanceStepRate1Minus1 = m_dispatchDrawInstanceStepRate1Minus1;

			// Pass a pointer to the dispatch draw data for the CUE to pass to shaders as kShaderInputUsagePtrDispatchDraw.
			// Its contents must not be modified until after this dispatchDraw call's shaders have finished running.
#if !SCE_GNMX_ENABLE_GFX_LCUE
			m_cue.setDispatchDrawData(pDispatchDrawData, sizeof(DispatchDrawV0Data));
#else // !SCE_GNMX_ENABLE_GFX_LCUE
			m_lwcue.setDispatchDrawData(pDispatchDrawData, sizeof(DispatchDrawV0Data));
#endif	// !SCE_GNMX_ENABLE_GFX_LCUE	
		}

		// m_cue.preDispatchDraw looks up settings from the currently set CsVsShader
		// NOTE: For the standard triangle culling dispatch draw shaders, many of these settings are fixed:
		//		orderedAppendMode = kDispatchOrderedAppendModeIndexPerThreadgroup
		//		dispatchDrawMode = kDispatchDrawModeIndexRingBufferOnly
		//		dispatchDrawIndexDeallocMask = 0xFC00  (CsShader.m_dispatchDrawIndexDeallocNumBits = 10)
		//		user SGPR locations may vary depending on the vertex shader source which is compiled
		Gnm::DispatchOrderedAppendMode orderedAppendMode;	//from CsShader.m_orderedAppendMode
		Gnm::DispatchDrawMode dispatchDrawMode = Gnm::kDispatchDrawModeIndexRingBufferOnly;	//from VsShader inputUsageSlots (kDispatchDrawModeIndexAndVertexRingBuffers if sgprVrbLoc is found)
		uint32_t dispatchDrawIndexDeallocMask = 0;	//from CsShader.m_dispatchDrawIndexDeallocNumBits
		uint32_t sgprKrbLoc = 0;	//get the user SGPR index from the CsShader inputUsageSlots kShaderInputUsageImmGdsKickRingBufferOffset, which must always be present
		uint32_t sgprVrbLoc = 0;	//get the user SGPR index from the VsShader inputUsageSlots kShaderInputUsageImmVertexRingBufferOffset, if any, or (uint32_t)-1 if not found; dispatchDrawMode returns kDispatchDrawModeIndexAndVertexRingBuffer if found.
		uint32_t sgprInstancesCs = 0;	//get the user SGPR index from the CsShader inputUsageSlots kShaderInputUsageImmDispatchDrawInstances, if any, or (uint32_t)-1 if not found
		uint32_t sgprInstancesVs = 0;	//get the user SGPR index from the VsShader inputUsageSlots kShaderInputUsageImmDispatchDrawInstances, if any, or (uint32_t)-1 if not found

		// Tell the CUE to set up CCB constant data for the current set shaders:
#if !SCE_GNMX_ENABLE_GFX_LCUE
		m_cue.preDispatchDraw(&m_dcb, &m_acb, &m_ccb, &orderedAppendMode, &dispatchDrawIndexDeallocMask, &sgprKrbLoc, &sgprInstancesCs, &dispatchDrawMode, &sgprVrbLoc, &sgprInstancesVs);
#else // !SCE_GNMX_ENABLE_GFX_LCUE
		m_lwcue.preDispatchDraw(&orderedAppendMode, &dispatchDrawIndexDeallocMask, &sgprKrbLoc, &sgprInstancesCs, &dispatchDrawMode, &sgprVrbLoc, &sgprInstancesVs);
#endif // !SCE_GNMX_ENABLE_GFX_LCUE		
		// Notify the GPU of the dispatchDrawIndexDeallocMask required by the current CsShader:
		if (dispatchDrawIndexDeallocMask != m_dispatchDrawIndexDeallocMask) {
			m_dispatchDrawIndexDeallocMask = dispatchDrawIndexDeallocMask;
			m_dcb.setDispatchDrawIndexDeallocationMask(dispatchDrawIndexDeallocMask);
		}

		uint32_t maxInstancesPerCall = (dispatchDrawIndexDeallocMask >> pDispatchDrawIndexData->m_numIndexBits) + ((dispatchDrawIndexDeallocMask & (0xFFFF << pDispatchDrawIndexData->m_numIndexSpaceBits)) != dispatchDrawIndexDeallocMask ? 1 : 0);
		if (maxInstancesPerCall == 0) {
			// For the standard triangle culling dispatch draw shaders, dispatchDrawIndexDeallocMask = 0xFC00,
			// which limits dispatchDraw calls to use no more than 63K (0xFC00) indices:
			uint32_t mask = dispatchDrawIndexDeallocMask, dispatchDrawIndexDeallocNumBits = 0;
			if (!(mask & 0xFF))	mask >>= 8, dispatchDrawIndexDeallocNumBits |= 8;
			if (!(mask & 0xF))	mask >>= 4, dispatchDrawIndexDeallocNumBits |= 4;
			if (!(mask & 0x3))	mask >>= 2, dispatchDrawIndexDeallocNumBits |= 2;
			dispatchDrawIndexDeallocNumBits |= (0x1 &~ mask);
			SCE_GNM_ASSERT_MSG(maxInstancesPerCall > 0, "dispatchDraw requires numIndexBits (%u) < 16 or numIndexSpaceBits (%u) > m_dispatchDrawIndexDeallocNumBits (%u) for the asynchronous compute shader", pDispatchDrawIndexData->m_numIndexBits, pDispatchDrawIndexData->m_numIndexSpaceBits, dispatchDrawIndexDeallocNumBits);

#if !SCE_GNMX_ENABLE_GFX_LCUE
			m_cue.postDispatchDraw(&m_dcb, &m_acb, &m_ccb);
#endif // !SCE_GNMX_ENABLE_GFX_LCUE	
			return;
		}

		uint32_t numInstancesMinus1 = (m_dispatchDrawNumInstancesMinus1 & 0xFFFF);
		uint32_t numCalls = 1, numTgY = 1, numInstancesPerCall = 1;
		uint32_t numTgYLastCall = 1, numInstancesLastCall = 1;
		if (numInstancesMinus1 != 0) {
			// To implement instancing in software in an efficient way, the CS shader can pack some of the bits
			// of the instanceId into the output index data, provided the output indices are using a small enough
			// range of the available 63K index space.
			// This allows each dispatchDraw command buffer command to render multiple instances, reducing the
			// command processing and dispatch/draw overhead by a corresponding factor.  
			// If the total number of instances is greater than the number which can be rendered within a single 
			// dispatch, only a change to the kShaderInputUsageImmDispatchDrawInstances user SGPRs is required
			// between each dispatch, which keeps the overhead of additional instances to a minimum.
			// In addition, if an object is smaller than 256 triangles, it becomes possible to render multiple
			// instances of that object in a single thread group (each of which processes up to 512 triangles),
			// which correspondingly reduces the number of thread groups launched.

			// Here, we calculate how many dispatches and how many thread groups we will have to launch to
			// render the required total number of instances:
			uint32_t numInstances = numInstancesMinus1 + 1;
			uint32_t maxInstancesPerTg = pDispatchDrawIndexData->m_numInstancesPerTgMinus1+1;
			numInstancesLastCall = numInstances;
			numInstancesPerCall = numInstances;
			if (numInstances > maxInstancesPerCall) {
				numCalls = (numInstances + maxInstancesPerCall-1)/maxInstancesPerCall;
				numInstancesPerCall = maxInstancesPerCall;
				numInstancesLastCall -= (numCalls - 1) * maxInstancesPerCall;
			}
			if (numInstancesPerCall > maxInstancesPerTg)
				numTgY = (numInstancesPerCall + maxInstancesPerTg-1)/maxInstancesPerTg;
			if (numInstancesLastCall > maxInstancesPerTg)
				numTgYLastCall = (numInstancesLastCall + maxInstancesPerTg-1)/maxInstancesPerTg;
			SCE_GNM_ASSERT_MSG(numInstancesPerCall*(numCalls-1) + numInstancesLastCall == numInstances && numTgY*(numCalls-1) + numTgYLastCall == (numInstances + maxInstancesPerTg-1)/maxInstancesPerTg, "dispatchDraw instancing internal error");
		}

		//===== BEGIN HACK TO OVERRIDE CONSTANT BUFFER ===========================================
		m_acb.setVsharpInUserData(userSgprConstBuffer0, &constantBufferForCompute);
		//===== END HACK TO OVERRIDE CONSTANT BUFFER =============================================

		SCE_GNM_ASSERT_MSG(sgprKrbLoc < 16, "dispatchDraw requires an asynchronous compute shader with a kShaderInputUsageImmKickRingBufferOffset userdata sgpr");
		SCE_GNM_ASSERT_MSG(dispatchDrawMode == Gnm::kDispatchDrawModeIndexRingBufferOnly, "dispatchDraw with a vertex ring buffer is not supported by this version of Gnmx");
		SCE_GNM_ASSERT_MSG((numInstancesMinus1 == 0) || (sgprInstancesCs < 16 && sgprInstancesVs < 16), "dispatchDraw with instancing requires asynchronous compute and VS shaders with kShaderInputUsageImmDispatchDrawInstances userdata sgprs");

		// Here we iterate over however many dispatchDraw commands are required to render all requested instances,
		// adjusting the kShaderInputUsageImmDispatchDrawInstances user SGPRs for each call:
		uint32_t firstInstance = 0;
		for (uint32_t nCall = 0; nCall+1 < numCalls; ++nCall, firstInstance += numInstancesPerCall) {
			uint32_t dispatchDrawInstances = (firstInstance<<16)|(numInstancesPerCall-1);
			m_acb.setUserData(sgprInstancesCs, dispatchDrawInstances);
			m_dcb.setUserData(Gnm::kShaderStageVs, sgprInstancesVs, dispatchDrawInstances);
			m_acb.dispatchDraw(pDispatchDrawIndexData->m_numIndexDataBlocks, numTgY, 1, orderedAppendMode, sgprKrbLoc);
			m_dcb.dispatchDraw(Gnm::kPrimitiveTypeTriList, indexOffset, subDrawIndexCount, dispatchDrawMode, sgprVrbLoc);
		}
		{
			uint32_t dispatchDrawInstances = (firstInstance<<16)|(numInstancesLastCall-1);
			if (sgprInstancesCs < 16)
				m_acb.setUserData(sgprInstancesCs, dispatchDrawInstances);
			if (sgprInstancesVs < 16)
				m_dcb.setUserData(Gnm::kShaderStageVs, sgprInstancesVs, dispatchDrawInstances);
		}
		m_acb.dispatchDraw(pDispatchDrawIndexData->m_numIndexDataBlocks, numTgYLastCall, 1, orderedAppendMode, sgprKrbLoc);
		m_dcb.dispatchDraw(Gnm::kPrimitiveTypeTriList, indexOffset, subDrawIndexCount, dispatchDrawMode, sgprVrbLoc);

		// Notify the CUE that we are done issuing draw commands that will refer to the current CCB constant data:
#if !SCE_GNMX_ENABLE_GFX_LCUE
		m_cue.postDispatchDraw(&m_dcb, &m_acb, &m_ccb);
#endif // !SCE_GNMX_ENABLE_GFX_LCUE	
	}

	uint32_t const kMaxObjects = 4096;
	uint32_t const kMinSizeofIndexRingBuffer = 37*256;
	uint32_t const kMaxSizeofIndexRingBuffer = 512*1024;
	uint32_t const kaObjectNumQuadsInner[] = {
		  3,	//   52 verts     72 tris   4 inst/tg
		  4,	//   85 verts    128 tris   3 inst/tg
		  5,	//  126 verts    200 tris   2 inst/tg
		  7,	//  232 verts    392 tris   1 tg
		 14,	//  855 verts   1568 tris   4 tg
		 21,	// 1870 verts   3528 tris   9 tg
		 28,	// 3277 verts   6272 tris  15 tg
		 35,	// 5040 verts   9800 tris  24 tg
		 42,	// 7267 verts  14112 tris  34 tg
		 49,	// 9850 verts  19208 tris  46 tg
		 56,	//12825 verts  25088 tris  60 tg
		 63,	//16192 verts  31752 tris  76 tg
		 70,	//19951 verts  39200 tris  93 tg
		 77,	//24102 verts  47432 tris 112 tg
		 84,	//28645 verts  56448 tris 133 tg
		 91,	//33580 verts  66248 tris 157 tg
		 98,	//38907 verts  76832 tris 182 tg
		105,	//44626 verts  88200 tris 208 tg
		112,	//50737 verts 100352 tris 237 tg
	};
	uint32_t const kNumObjectMeshSteps = (uint32_t)(sizeof(kaObjectNumQuadsInner)/sizeof(kaObjectNumQuadsInner[0]));

	enum UseDispatchDraw {
		kUseDraw =						0,
		kUseDispatchDraw =				1,
		kUseMixedDrawAndDispatchDraw =	2,
	};
	static Framework::MenuItemText kaMenuItemTextUseDispatchDraw[] = {
		{ "False", "Use normal vertex processing pipeline" },
		{ "True", "Use dispatch draw CS-VS pipeline" },
		{ "Mixed", "Alternate 4 draw calls to dispatch draw, 4 to normal pipeline" },
	};

	class RuntimeOptions
	{
	public:
		uint32_t m_numObjects;
		uint32_t m_objectMeshStep;
		uint32_t m_numParameters;
		uint32_t m_sizeofIndexRingBuffer;
		uint32_t m_numKickRingBufferElems;
		uint32_t m_numComputeUnits;
		uint32_t m_numWavesPerCu;
		uint32_t m_numPrimsPerVgt;
		uint32_t m_numInstancesPerDraw;
		uint32_t m_useDispatchDraw;
		bool m_objectCull;
		bool m_useInstancing;
		bool m_canFreezeCullingFrustum;
		bool m_freezeCullingFrustum;
		RuntimeOptions()
			: m_numObjects(64)
			, m_objectMeshStep(8)
			, m_numParameters(4)
			, m_sizeofIndexRingBuffer(48*4*256)
			, m_numKickRingBufferElems(8)
			, m_numComputeUnits(14)
			, m_numWavesPerCu(16)
			, m_numInstancesPerDraw(32)
			, m_useDispatchDraw(kUseDispatchDraw)
			, m_objectCull(true)
			, m_useInstancing(false)
			, m_canFreezeCullingFrustum(true)
			, m_freezeCullingFrustum(false)
		{
		}
		uint32_t numObjects() const { return m_numObjects; }
		uint32_t objectMeshStep() const { return m_objectMeshStep; }
		uint32_t objectNumQuadsInner() const { return kaObjectNumQuadsInner[m_objectMeshStep]; }
		uint32_t numParameters() const { return m_numParameters; }
		bool frustumCullObjects() const { return m_objectCull; }
		bool freezeCullingFrustum() const {	return m_freezeCullingFrustum; }
		UseDispatchDraw useDispatchDraw() const { return (UseDispatchDraw)m_useDispatchDraw; }
		uint32_t sizeofIndexRingBuffer() const { return m_sizeofIndexRingBuffer; }
		uint32_t numKickRingBufferElems() const { return m_numKickRingBufferElems; }
		uint32_t numComputeUnits() const { return m_numComputeUnits; }
		uint32_t numWavesPerCu() const { return m_numWavesPerCu; }
		bool useInstancing() const { return m_useInstancing; }
		uint32_t numInstancesPerDraw() const { return m_numInstancesPerDraw; }
	};
	RuntimeOptions g_runtimeOptions;

	class MenuCommandNumTri : public Framework::MenuCommandUint
	{
	public:
		MenuCommandNumTri(uint32_t* target, uint32_t min = 0, uint32_t max = 0)
			: Framework::MenuCommandUint(target, min, max)
		{
			m_step = 1;
			m_wrapMode = Framework::MenuCommandUint::kClamp;
		}
		virtual void printValue(sce::DbgFont::Window* dest)
		{
			unsigned int meshStep = *m_target;
			unsigned int numQuadsInner = kaObjectNumQuadsInner[meshStep];
			dest->printf("%u", numQuadsInner*numQuadsInner*8);
		}
	};
	class MenuCommandLateAllocVs : public Framework::MenuCommandUint
	{
	public:
		MenuCommandLateAllocVs(uint32_t* target)
			: Framework::MenuCommandUint(target, 1, 64)
		{
			m_step = 1;
			m_wrapMode = Framework::MenuCommandUint::kClamp;
		}
		virtual void printValue(sce::DbgFont::Window* dest)
		{
			uint32_t lateAllocVs = *m_target;
			if (lateAllocVs == 0 || lateAllocVs >= 64)
				dest->printf("No Limit");
			else
				dest->printf("%u", lateAllocVs);
		}
	};
	class MenuCommandUintNeedsInstancing : public Framework::MenuCommandUint
	{
	public:
		MenuCommandUintNeedsInstancing(uint32_t* target, uint32_t min = 0, uint32_t max = 0, char const* format = NULL, uint32_t step = 1)
			: Framework::MenuCommandUint(target, min, max, format)
		{
			m_step = step;
		}
		virtual bool isDocumented() const {return true;}
		virtual bool isEnabled() const
		{
			return g_runtimeOptions.m_useInstancing;
		}
	};
	class MenuCommandUintNeedsDispatchDraw : public Framework::MenuCommandUint
	{
	public:
		MenuCommandUintNeedsDispatchDraw(uint32_t* target, uint32_t min = 0, uint32_t max = 0, char const* format = NULL, uint32_t step = 1)
			: Framework::MenuCommandUint(target, min, max, format)
		{
			m_step = step;
		}
		virtual bool isEnabled() const
		{
			return g_runtimeOptions.m_useDispatchDraw != kUseDraw;
		}
	};
	class MenuCommandBoolNeedsDispatchDraw : public Framework::MenuCommandBool
	{
	public:
		MenuCommandBoolNeedsDispatchDraw(bool* target)
			: Framework::MenuCommandBool(target)
		{
		}
		virtual bool isEnabled() const
		{
			return g_runtimeOptions.m_useDispatchDraw != kUseDraw;
		}
	};

	class MenuCommandMatrixCullingFrustum : public Framework::MenuCommandMatrix
	{
		Framework::GnmSampleFramework *m_gsf;
	public:
		MenuCommandMatrixCullingFrustum(Framework::GnmSampleFramework *gsf)
			: Framework::MenuCommandMatrix(&viewToWorldForCulling, true)
		{
			m_gsf = gsf;
			viewToWorldForCulling = m_gsf->m_viewToWorldMatrix;
			viewProjectionMatrixForCulling = m_gsf->m_viewProjectionMatrix;
		}
		virtual bool isEnabled() const
		{
			return true; //g_runtimeOptions.m_freezeCullingFrustum;
		}
		void input(Framework::MenuHost *src)
		{
			m_step = (float)m_gsf->actualSecondsPerFrame() * 2.f;
			Framework::MenuCommandMatrix::input(src);
			if (updated_by_input_since_last_call()) {
				Matrix4 worldToViewForCulling = inverse(viewToWorldForCulling);
				viewProjectionMatrixForCulling = m_gsf->m_projectionMatrix * worldToViewForCulling;
				bViewProjectionMatrixForCullingUpdated = true;
				if (!g_runtimeOptions.m_freezeCullingFrustum)
					g_runtimeOptions.m_freezeCullingFrustum = true;
			}
		}
	};

	class MenuCommandSizeofIndexRingBuffer : public Framework::MenuCommandUint
	{
	public:
		MenuCommandSizeofIndexRingBuffer(uint32_t* target)
			: Framework::MenuCommandUint(target, kMinSizeofIndexRingBuffer, kMaxSizeofIndexRingBuffer, "%u bytes")
		{
			m_step = 256;
		}
		virtual bool isEnabled() const
		{
			return g_runtimeOptions.m_useDispatchDraw != kUseDraw;
		}
	};


	Framework::MenuCommandUint num_objects(&g_runtimeOptions.m_numObjects, 1, kMaxObjects);
	MenuCommandNumTri tri_count(&g_runtimeOptions.m_objectMeshStep, 0, kNumObjectMeshSteps-1);
	Framework::MenuCommandBool object_cull_en(&g_runtimeOptions.m_objectCull);
	Framework::MenuCommandBool instancing_en(&g_runtimeOptions.m_useInstancing);
	MenuCommandUintNeedsInstancing num_instances(&g_runtimeOptions.m_numInstancesPerDraw, 1, kMaxObjects);
	Framework::MenuCommandUint num_parameters(&g_runtimeOptions.m_numParameters, 4, 32);
	Framework::MenuCommandBool freeze_frustum(&g_runtimeOptions.m_freezeCullingFrustum);
	MenuCommandMatrixCullingFrustum frustum_matrix(&framework);
	Framework::MenuCommandEnum dispatch_draw_en(kaMenuItemTextUseDispatchDraw, 3, &g_runtimeOptions.m_useDispatchDraw);
	MenuCommandSizeofIndexRingBuffer sizeof_irb(&g_runtimeOptions.m_sizeofIndexRingBuffer);
	MenuCommandUintNeedsDispatchDraw num_krb_elems(&g_runtimeOptions.m_numKickRingBufferElems, 1, 40);
	MenuCommandUintNeedsDispatchDraw num_compute_units(&g_runtimeOptions.m_numComputeUnits, 1, 14);
	MenuCommandUintNeedsDispatchDraw waves_per_cu(&g_runtimeOptions.m_numWavesPerCu, 4, 40, NULL, 4);

	const Framework::MenuItem menuItem[] =
	{
		{{"Object Count", "Number of toruses displayed.  Left to decrease, right to increase"}, &num_objects},
		{{"Tri Count", "Number of triangle counts for a torus"}, &tri_count},
		{{"Object Culling", "Cull objects against view frustum"}, &object_cull_en},
		{{"Instancing", "Use instancing"}, &instancing_en},
		{{"Instance Ct.", "Number of instances to render in each draw call"}, &num_instances},
		{{"Param Count", "Number of parameters used for the vertex shader"}, &num_parameters},
		{{"Freeze Culling", "Freeze updating the view matrix for culling"}, &freeze_frustum},
		{{"Move Culling", "Move the culling frustum by analog sticks"}, &frustum_matrix},
		{{"DispatchDraw", "Use dispatch draw, or standard pipeline?"}, &dispatch_draw_en},
		{{"IRB size", "Size of the dispatch draw index ring buffer"}, &sizeof_irb},
		{{"KRB count", "Size of dispatch draw kick ring buffer in elements [1:40]"}, &num_krb_elems},
		{{"CU count", "Number of CUs available to dispatch draw compute [1:14]"}, &num_compute_units},
		{{"Waves/CU", "Number of dispatch draw compute waves per CU [4:40]"}, &waves_per_cu},
	};
	enum { kNumMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
}

struct MatrixTransposed {
	Vector4Unaligned col0;
	Vector4Unaligned col1;
	Vector4Unaligned col2;
};

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.m_config.m_lightingIsMeaningful = 1;
	framework.m_config.m_displayDebugObjects = 1;
	framework.m_config.m_dumpPackets = 1;

	framework.initialize( "DispatchDraw", 
		"Sample code to demonstrate rendering a torus using dispatch draw shaders.",
		"This sample program shows how to use the dispatch-draw function.  The dispatch-draw function realizes an efficient vertex processing, by passing indices and vertex parameters generated in the compute shader to the vertex shader automatically. For example, you can run backface, off screen and zero area cullings in the compute shader, and pass the reduced indices to the vertex shader. For the dispatch-draw function, PSSL runs in a special mode. Both a culling binary code for the compute shader and a transformation binary code for the vertex shader are generated from one vertex shader source code.");

	framework.appendMenuItems(kNumMenuItems, menuItem);

	Gnmx::GnmxGfxContext *commandBuffers = static_cast<Gnmx::GnmxGfxContext*>(framework.m_onionAllocator.allocate(sizeof(Gnmx::GnmxGfxContext) * framework.m_config.m_buffers, 16)); // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	Constants *constantBuffers = static_cast<Constants*>(framework.m_onionAllocator.allocate(kMaxObjects * sizeof(Constants) * framework.m_config.m_buffers, Gnm::kAlignmentOfBufferInBytes));
	Constants *constantBuffersFrozen = static_cast<Constants*>(framework.m_onionAllocator.allocate(kMaxObjects * sizeof(Constants) * framework.m_config.m_buffers, Gnm::kAlignmentOfBufferInBytes));
	MatrixTransposed *aMatrixInstance = static_cast<MatrixTransposed*>(framework.m_onionAllocator.allocate(kMaxObjects * sizeof(MatrixTransposed) * framework.m_config.m_buffers, Gnm::kAlignmentOfBufferInBytes));

	enum {kCommandBufferSizeInBytes = 2 * 1024 * 1024};

	for(uint32_t bufferIndex = 0; bufferIndex < framework.m_config.m_buffers; ++bufferIndex)
	{
#if  SCE_GNMX_ENABLE_GFX_LCUE
		const uint32_t kNumBatches = 100; // Estimated number of batches to support within the resource buffer
		const uint32_t resourceBufferSize = Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(Gnmx::LightweightConstantUpdateEngine::kResourceBufferGraphics, kNumBatches);

		void* pDcb = framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes);
		void* pResourceBuffer = framework.m_garlicAllocator.allocate(resourceBufferSize, Gnm::kAlignmentOfBufferInBytes);
		Gnmx::GnmxGfxContext *gfxc = &commandBuffers[bufferIndex];
		gfxc->init( pDcb, kCommandBufferSizeInBytes,	// Draw command buffer
			pResourceBuffer, resourceBufferSize,		// Resource buffer
			NULL										// Global resource table 
			);
#else
		const uint32_t kNumRingEntries = 64;
		const uint32_t cueHeapSize = Gnmx::ConstantUpdateEngine::computeHeapSize(kNumRingEntries);
		void* pCueHeap = framework.m_garlicAllocator.allocate(cueHeapSize, Gnm::kAlignmentOfBufferInBytes);
		void* pDcb = framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes);
		void* pCcb = framework.m_garlicAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes);
		Gnmx::GnmxGfxContext *gfxc = &commandBuffers[bufferIndex];
		gfxc->init(pCueHeap, kNumRingEntries,		// Constant Update Engine
			pDcb, kCommandBufferSizeInBytes,		// Draw command buffer
			pCcb, kCommandBufferSizeInBytes			// Constant command buffer
			);
 		printf("CUE Heap[%u]:     0x%p:0x%p\n", bufferIndex, pCueHeap, (void*)((uintptr_t)pCueHeap + cueHeapSize));
 		printf("DCB[%u]:          0x%p:0x%p\n", bufferIndex, pDcb, (void*)((uintptr_t)pDcb + Gnm::kIndirectBufferMaximumSizeInBytes));
 		printf("CCB[%u]:          0x%p:0x%p\n", bufferIndex, pCcb, (void*)((uintptr_t)pCcb + Gnm::kIndirectBufferMaximumSizeInBytes));
#endif
	}

	void *fetchShaderDdVs, *fetchShaderDdCs;
	Gnmx::CsVsShader *computeVertexShader = Framework::LoadCsVsMakeFetchShaders(&fetchShaderDdVs, &fetchShaderDdCs, "/app0/dispatch-draw-sample/shader_ddtc.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache computeVertexShader_offsetsTableVS;
	Gnmx::InputOffsetsCache computeVertexShader_offsetsTableCS;
	generateInputOffsetsCacheForDispatchDraw(&computeVertexShader_offsetsTableCS, &computeVertexShader_offsetsTableVS, computeVertexShader);

	void *fetchShader;
	Gnmx::VsShader *vertexShader = Framework::LoadVsMakeFetchShader(&fetchShader, "/app0/dispatch-draw-sample/shader_vv.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);

	void *fetchShaderInstanced;
	Gnm::FetchShaderInstancingMode instancingData[7] = {
		Gnm::kFetchShaderUseInstanceId, Gnm::kFetchShaderUseInstanceId, Gnm::kFetchShaderUseInstanceId,
		Gnm::kFetchShaderUseVertexIndex, Gnm::kFetchShaderUseVertexIndex, Gnm::kFetchShaderUseVertexIndex, Gnm::kFetchShaderUseVertexIndex,
	};
	uint32_t const numElementsInInstancingData = sizeof(instancingData) / sizeof(instancingData[0]);

	Gnmx::VsShader *vertexShaderInstanced = Framework::LoadVsMakeFetchShader(&fetchShaderInstanced, "/app0/dispatch-draw-sample/shader_inst_vv.sb", &framework.m_allocators, instancingData, numElementsInInstancingData);
	Gnmx::InputOffsetsCache vertexShaderInstanced_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShaderInstanced_offsetsTable, Gnm::kShaderStageVs, vertexShaderInstanced);
	
	void *fetchShaderDdVsInstanced, *fetchShaderDdCsInstanced;
	Gnmx::CsVsShader *computeVertexShaderInstanced = Framework::LoadCsVsMakeFetchShaders(&fetchShaderDdVsInstanced, &fetchShaderDdCsInstanced, "/app0/dispatch-draw-sample/shader_inst_ddtci.sb", &framework.m_allocators, instancingData, numElementsInInstancingData);
	Gnmx::InputOffsetsCache computeVertexShaderInstanced_offsetsTableVS;
	Gnmx::InputOffsetsCache computeVertexShaderInstanced_offsetsTableCS;
	generateInputOffsetsCacheForDispatchDraw(&computeVertexShaderInstanced_offsetsTableCS, &computeVertexShaderInstanced_offsetsTableVS, computeVertexShaderInstanced);

	Gnmx::PsShader *pixelShader = Framework::LoadPsShader("/app0/dispatch-draw-sample/shader_p.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache pixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&pixelShader_offsetsTable, Gnm::kShaderStagePs, pixelShader);

	// Since the compute and VS stage shaders for dispatch draw share the input usage data and CUE ring data for the VS stage,
	// the V# for ConstBuffer[0] must embedded as immediate user sgpr register values in order for us to be able to set it independently.
	uint32_t userSgprComputeConstBuffer0 = (unsigned int)-1;
	{
		Gnm::InputUsageSlot const* inputUsageSlotsCs = computeVertexShader->getComputeShader()->getInputUsageSlotTable();
		for (uint32_t iUsage = 0, iUsageEnd = computeVertexShader->getComputeShader()->m_common.m_numInputUsageSlots; iUsage < iUsageEnd; ++iUsage) {
			if (inputUsageSlotsCs[iUsage].m_startRegister < 16 && inputUsageSlotsCs[iUsage].m_usageType == Gnm::kShaderInputUsageImmConstBuffer && inputUsageSlotsCs[iUsage].m_apiSlot == 0) {
				userSgprComputeConstBuffer0 = inputUsageSlotsCs[iUsage].m_startRegister;
				break;
			}
		}
	}
	// Without this, we can't have the compute shader cull using a different view matrix from the VS shader:
	g_runtimeOptions.m_canFreezeCullingFrustum = (userSgprComputeConstBuffer0 != (unsigned int)-1);

	Gnm::Texture textures[2];
	Framework::GnfError loadError = Framework::kGnfErrorNone;
	loadError = Framework::loadTextureFromGnf(&textures[0], "/app0/assets/icelogo-color.gnf", 0, &framework.m_allocators);
	SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );
	loadError = Framework::loadTextureFromGnf(&textures[1], "/app0/assets/icelogo-normal.gnf", 0, &framework.m_allocators);
	SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's OK to mark it as read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's OK to mark it as read-only.

	Gnm::Sampler trilinearSampler;
	trilinearSampler.init();
	trilinearSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	trilinearSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	const float kfObjectRadius = 1.0f;
	const float kfObjectSpacing = kfObjectRadius * 2.0f * 1.1f;
	const float kfObjectCullingRadius = kfObjectRadius;

	Framework::SimpleMesh aTorusMesh[kNumObjectMeshSteps];

	for (unsigned int nMeshStep = kNumObjectMeshSteps; nMeshStep-- > 0;)
	{
		uint16_t const nNumQuadsInner = (uint16_t)kaObjectNumQuadsInner[nMeshStep];
		Framework::BuildTorusMesh(Framework::kBuildVerticesAndOptimizedIndices, &framework.m_garlicAllocator, &aTorusMesh[nMeshStep], kfObjectRadius*0.8f, kfObjectRadius*0.2f, nNumQuadsInner*4, nNumQuadsInner, 4, 1);
	}

	//===========================================================================
	// build dispatch draw index block data
	Gnmx::DispatchDrawTriangleCullIndexData aDispatchDrawInputData[kNumObjectMeshSteps];
	
	{
		size_t sizeofIndexBufferCpuCopy = kaObjectNumQuadsInner[kNumObjectMeshSteps-1]*kaObjectNumQuadsInner[kNumObjectMeshSteps-1]*4 * 2 * 3 * sizeof(uint16_t);
		TempAllocator tempAllocator(memalign(64, sizeofIndexBufferCpuCopy), sizeofIndexBufferCpuCopy);	// cache-line aligned so our memcpy will be faster
		uint16_t *pDispatchDrawIndexDataCpuCopy = NULL;
		size_t sizeofDispatchDrawIndexDataCpuCopy = 0;

		for (unsigned int nMeshStep = kNumObjectMeshSteps; nMeshStep-- > 0;)
		{
			uint16_t const nNumQuadsInner = (uint16_t)kaObjectNumQuadsInner[nMeshStep];

			Framework::SimpleMesh torusMeshDispatchDraw;
			memset(&torusMeshDispatchDraw, 0, sizeof(torusMeshDispatchDraw));

			//Build our dispatch-draw optimized mesh indices into our temporary CPU memory buffer
			Framework::BuildTorusMesh(Framework::kBuildDispatchDrawOptimizedIndices, &tempAllocator, &torusMeshDispatchDraw, kfObjectRadius*0.8f, kfObjectRadius*0.2f, nNumQuadsInner*4, nNumQuadsInner, 4, 1);
			SCE_GNM_ASSERT(torusMeshDispatchDraw.m_indexBufferSize <= sizeofIndexBufferCpuCopy);

#if BUILD_DATA_FASTER_SLIGHTLY_LARGER
			int64_t sizeofDispatchDrawInputDataMax = Gnmx::getMaxSizeofDispatchDrawInputData(&(aDispatchDrawInputData[nMeshStep]), torusMeshDispatchDraw.m_primitiveType, torusMeshDispatchDraw.m_indexCount);
			if (sizeofDispatchDrawInputDataMax < 0) {
				printf("ERROR: getMaxSizeofDispatchDrawInputData reports invalid input primitive type or index count, error code:0x%08x\n", (uint32_t)sizeofDispatchDrawInputDataMax);
				SCE_GNM_ASSERT(!(sizeofDispatchDrawInputDataMax < 0));
				return -1;
			}
			//Create our chunked index data in CPU memory so we have fast cached access from the CPU, then copy it to GPU Garlic memory
			if (pDispatchDrawIndexDataCpuCopy == NULL) {
				sizeofDispatchDrawIndexDataCpuCopy = sizeofDispatchDrawInputDataMax;
				pDispatchDrawIndexDataCpuCopy = (uint16_t*)memalign(64, sizeofDispatchDrawIndexDataCpuCopy);	// cache-line aligned so our memcpy will be faster
			} else {
				SCE_GNM_ASSERT((uint32_t)sizeofDispatchDrawInputDataMax <= sizeofDispatchDrawIndexDataCpuCopy);
			}
			int64_t sizeofDispatchDrawInputData = Gnmx::createDispatchDrawInputData(&(aDispatchDrawInputData[nMeshStep]), torusMeshDispatchDraw.m_primitiveType, torusMeshDispatchDraw.m_indexCount, (uint16_t const*)torusMeshDispatchDraw.m_indexBuffer, pDispatchDrawIndexDataCpuCopy, sizeofDispatchDrawIndexDataCpuCopy);
			if (sizeofDispatchDrawInputData < 0) {
				printf("ERROR: createDispatchDrawInputData reports invalid input data, error code:0x%08x\n", (uint32_t)sizeofDispatchDrawInputData);
				SCE_GNM_ASSERT(!(sizeofDispatchDrawInputData < 0));
				return -1;
			}
#else
			int64_t sizeofDispatchDrawInputData = Gnmx::getSizeofDispatchDrawInputData(&(aDispatchDrawInputData[nMeshStep]), torusMeshDispatchDraw.m_primitiveType, torusMeshDispatchDraw.m_indexCount, (uint16_t const*)torusMeshDispatchDraw.m_indexBuffer);
			if (sizeofDispatchDrawInputData < 0) {
				printf("ERROR: getSizeofDispatchDrawInputData reports invalid input data, error code:0x%08x\n", (uint32_t)sizeofDispatchDrawInputData);
				SCE_GNM_ASSERT(!(sizeofDispatchDrawInputData < 0));
				return -1;
			}
			//Create our chunked index data in CPU memory so we have fast cached access from the CPU, then copy it to GPU Garlic memory
			if (pDispatchDrawIndexDataCpuCopy == NULL) {
				sizeofDispatchDrawIndexDataCpuCopy = sizeofDispatchDrawInputData;
				pDispatchDrawIndexDataCpuCopy = (uint16_t*)memalign(64, sizeofDispatchDrawIndexDataCpuCopy);	// cache-line aligned so our memcpy will be faster
			} else {
				SCE_GNM_ASSERT((uint32_t)sizeofDispatchDrawInputData <= sizeofDispatchDrawIndexDataCpuCopy);
			}
			int64_t ret = Gnmx::createDispatchDrawInputData(&(aDispatchDrawInputData[nMeshStep]), torusMeshDispatchDraw.m_primitiveType, torusMeshDispatchDraw.m_indexCount, (uint16_t const*)torusMeshDispatchDraw.m_indexBuffer, pDispatchDrawIndexDataCpuCopy, sizeofDispatchDrawInputData);
			SCE_GNM_ASSERT(ret == sizeofDispatchDrawInputData);
#endif
			tempAllocator.release(torusMeshDispatchDraw.m_indexBuffer);

			void *pDispatchDrawInputData = framework.m_garlicAllocator.allocate(sizeofDispatchDrawInputData, 64); // cache-line aligned so our memcpy will be faster
			memcpy(pDispatchDrawInputData, pDispatchDrawIndexDataCpuCopy, sizeofDispatchDrawInputData);
			aDispatchDrawInputData[nMeshStep].m_bufferInputIndexData.setBaseAddress(pDispatchDrawInputData);

			printf("IndexData[%2u]:   0x%p:0x%p (%u triangles, %u blocks, %u inst/tg)\n", nMeshStep, pDispatchDrawInputData, (void*)((uintptr_t)pDispatchDrawInputData + sizeofDispatchDrawInputData), torusMeshDispatchDraw.m_indexCount/3, aDispatchDrawInputData[nMeshStep].m_numIndexDataBlocks, aDispatchDrawInputData[nMeshStep].m_numInstancesPerTgMinus1+1);
			printf("Vertices[%2u]:    0x%p:0x%p (%u vertices, %u index bits, %u index space bits, %u inst/call)\n", nMeshStep, aTorusMesh[nMeshStep].m_vertexBuffer, (void*)((uintptr_t)aTorusMesh[nMeshStep].m_vertexBuffer + aTorusMesh[nMeshStep].m_vertexBufferSize), aTorusMesh[nMeshStep].m_vertexCount, aDispatchDrawInputData[nMeshStep].m_numIndexBits, aDispatchDrawInputData[nMeshStep].m_numIndexSpaceBits, (0xFC00>>aDispatchDrawInputData[nMeshStep].m_numIndexBits) + ((0xFC00 & (0xFFFF << aDispatchDrawInputData[nMeshStep].m_numIndexSpaceBits)) != 0xFC00 ? 1 : 0));
		}
		if (pDispatchDrawIndexDataCpuCopy)
			free(pDispatchDrawIndexDataCpuCopy);
		if (tempAllocator.m_pTempBuffer)
			free(tempAllocator.m_pTempBuffer);
	}


	//===========================================================================
	// set up compute queues and index ring buffer for dispatch draw
	uint32_t pipe_id = 0, queue_id = 3;
	const uint32_t sizeofComputeQueueRing = 4 * 1024;
	void *pComputeQueueRing = framework.m_onionAllocator.allocate(sizeofComputeQueueRing, 256);
	void *pComputeQueueReadPtr = framework.m_onionAllocator.allocate(4, 16);
	memset(pComputeQueueRing, 0, sizeofComputeQueueRing);
	Gnmx::ComputeQueue computeQueue;
	computeQueue.initialize(pipe_id, queue_id);
	{
		bool bSuccessMappingQueue = computeQueue.map(pComputeQueueRing, sizeofComputeQueueRing/4, pComputeQueueReadPtr);
		SCE_GNM_ASSERT(bSuccessMappingQueue);
 		printf("ComputeQueue:    0x%p:0x%p\n", pComputeQueueRing, (void*)((uintptr_t)pComputeQueueRing + sizeofComputeQueueRing));
 		printf("ComputeQueueRptr:0x%p:0x%p\n", pComputeQueueReadPtr, (void*)((uintptr_t)pComputeQueueReadPtr + 4));
	}
	for(uint32_t bufferIndex = 0; bufferIndex < framework.m_config.m_buffers; ++bufferIndex)
	{
		void* pAcb = framework.m_onionAllocator.allocate(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes);
		Gnmx::GnmxGfxContext *gfxc = &commandBuffers[bufferIndex];
		gfxc->initDispatchDrawCommandBuffer(pAcb, Gnm::kIndirectBufferMaximumSizeInBytes);	// Asynchronous compute command buffer
		// all gfxc can share the same compute queue, as the compute queue is used only once
		gfxc->setDispatchDrawComputeQueue(&computeQueue);
		printf("ACB[%u]:          0x%p:0x%p\n", bufferIndex, pAcb, (void*)((uintptr_t)pAcb + Gnm::kIndirectBufferMaximumSizeInBytes));
	}
	void* pIndexRingBuffer = framework.m_garlicAllocator.allocate(kMaxSizeofIndexRingBuffer, Gnm::kAlignmentOfIndexRingBufferInBytes);
	printf("IRB:             0x%p:0x%p\n", pIndexRingBuffer, (void*)((uintptr_t)pIndexRingBuffer + kMaxSizeofIndexRingBuffer));


	printf("DDTC.CS:         0x%p\n", computeVertexShader->getComputeShader()->getBaseAddress());
	printf("DDTC.VS:         0x%p\n", computeVertexShader->getVertexShader()->getBaseAddress());
	printf("DDTCI.CS:        0x%p\n", computeVertexShaderInstanced->getComputeShader()->getBaseAddress());
	printf("DDTCI.VS:        0x%p\n", computeVertexShaderInstanced->getVertexShader()->getBaseAddress());

	enum {X, Y, Z, W};
	enum {Xp, Xm, Yp, Ym, Zp, Zm};
	enum {
		XpYpZp, XmYpZp, 
		XpYmZp, XmYmZp, 
		XpYpZm, XmYpZm, 
		XpYmZm, XmYmZm, 
	};
	Vector4 aFrustumVertex[4*6];
	Framework::GnmSampleFramework::Plane aFrustumPlane[Framework::GnmSampleFramework::kPlanes];

	while (!framework.m_shutdownRequested)
	{
		uint32_t const config_numObjects = g_runtimeOptions.numObjects();
		uint32_t const config_nMesh = g_runtimeOptions.objectMeshStep();
		uint32_t const config_numParameters = g_runtimeOptions.numParameters();
		bool const config_bFreezeCullingFrustum = g_runtimeOptions.freezeCullingFrustum();
		UseDispatchDraw const config_useDispatchDraw = g_runtimeOptions.useDispatchDraw();
		uint32_t const config_sizeofIndexRingBuffer = g_runtimeOptions.sizeofIndexRingBuffer();
		uint32_t const config_numKickRingBufferElems = g_runtimeOptions.numKickRingBufferElems();
		uint32_t const config_numComputeUnits = g_runtimeOptions.numComputeUnits();
		uint32_t const config_numWavesPerCu = g_runtimeOptions.numWavesPerCu();
		bool const config_bUseInstancing = g_runtimeOptions.useInstancing();
		uint32_t const config_numInstancesPerDraw = g_runtimeOptions.numInstancesPerDraw();
		bool const config_frustumCullObjects = g_runtimeOptions.frustumCullObjects();

		bool bUpdateFrustumPlanes = false, bUpdateFrustumVertices = false;
		if (!config_bFreezeCullingFrustum) {
			bool bViewProjectionMatrixUpdated = memcmp(&framework.m_viewProjectionMatrix, &viewProjectionMatrixForCulling, sizeof(viewProjectionMatrixForCulling));
			viewToWorldForCulling = framework.m_viewToWorldMatrix;
			viewProjectionMatrixForCulling = framework.m_viewProjectionMatrix;
			if (config_frustumCullObjects && bViewProjectionMatrixUpdated)
				bUpdateFrustumPlanes = true;
		} else {
			if (bViewProjectionMatrixForCullingUpdated) {
				bViewProjectionMatrixForCullingUpdated = false;
				bUpdateFrustumPlanes = bUpdateFrustumVertices = true;
			}
		}

		if (bUpdateFrustumPlanes) {
			bUpdateFrustumPlanes = false;
#if 1 // OGL_CLIP_SPACE
			aFrustumPlane[Xp].m_equation = viewProjectionMatrixForCulling.getRow(W) + viewProjectionMatrixForCulling.getRow(X);
			aFrustumPlane[Xm].m_equation = viewProjectionMatrixForCulling.getRow(W) - viewProjectionMatrixForCulling.getRow(X);
			aFrustumPlane[Yp].m_equation = viewProjectionMatrixForCulling.getRow(W) + viewProjectionMatrixForCulling.getRow(Y);
			aFrustumPlane[Ym].m_equation = viewProjectionMatrixForCulling.getRow(W) - viewProjectionMatrixForCulling.getRow(Y);
			aFrustumPlane[Zp].m_equation = viewProjectionMatrixForCulling.getRow(W) + viewProjectionMatrixForCulling.getRow(Z);
			aFrustumPlane[Zm].m_equation = viewProjectionMatrixForCulling.getRow(W) - viewProjectionMatrixForCulling.getRow(Z);
#else
			aFrustumPlane[Xp].m_equation = viewProjectionMatrixForCulling.getRow(W) + viewProjectionMatrixForCulling.getRow(X);
			aFrustumPlane[Xm].m_equation = viewProjectionMatrixForCulling.getRow(W) - viewProjectionMatrixForCulling.getRow(X);
			aFrustumPlane[Yp].m_equation = viewProjectionMatrixForCulling.getRow(W) - viewProjectionMatrixForCulling.getRow(Y);
			aFrustumPlane[Ym].m_equation = viewProjectionMatrixForCulling.getRow(W) + viewProjectionMatrixForCulling.getRow(Y);
			aFrustumPlane[Zp].m_equation =                                            viewProjectionMatrixForCulling.getRow(Z);
			aFrustumPlane[Zm].m_equation = viewProjectionMatrixForCulling.getRow(W) - viewProjectionMatrixForCulling.getRow(Z);
#endif
			for(uint32_t i = 0; i < Framework::GnmSampleFramework::kPlanes; ++i)
			{
				const float f = 1.f / length(aFrustumPlane[i].m_equation.getXYZ());
				aFrustumPlane[i].m_equation *= f;
			}
		}
		if (bUpdateFrustumVertices) {
			bUpdateFrustumVertices = false;
			Vector4 aFrustumCorner[Framework::GnmSampleFramework::kCorners];
			IntersectPlanes(aFrustumCorner[XpYpZp], aFrustumPlane[Zp].m_equation, aFrustumPlane[Yp].m_equation, aFrustumPlane[Xp].m_equation);
			IntersectPlanes(aFrustumCorner[XmYpZp], aFrustumPlane[Zp].m_equation, aFrustumPlane[Yp].m_equation, aFrustumPlane[Xm].m_equation);
			IntersectPlanes(aFrustumCorner[XpYmZp], aFrustumPlane[Zp].m_equation, aFrustumPlane[Ym].m_equation, aFrustumPlane[Xp].m_equation);
			IntersectPlanes(aFrustumCorner[XmYmZp], aFrustumPlane[Zp].m_equation, aFrustumPlane[Ym].m_equation, aFrustumPlane[Xm].m_equation);
			IntersectPlanes(aFrustumCorner[XpYpZm], aFrustumPlane[Zm].m_equation, aFrustumPlane[Yp].m_equation, aFrustumPlane[Xp].m_equation);
			IntersectPlanes(aFrustumCorner[XmYpZm], aFrustumPlane[Zm].m_equation, aFrustumPlane[Yp].m_equation, aFrustumPlane[Xm].m_equation);
			IntersectPlanes(aFrustumCorner[XpYmZm], aFrustumPlane[Zm].m_equation, aFrustumPlane[Ym].m_equation, aFrustumPlane[Xp].m_equation);
			IntersectPlanes(aFrustumCorner[XmYmZm], aFrustumPlane[Zm].m_equation, aFrustumPlane[Ym].m_equation, aFrustumPlane[Xm].m_equation);
			Vector4 aFrustumEdge[6];
			aFrustumEdge[0] = aFrustumCorner[XpYpZp] - aFrustumCorner[XmYpZp];	//+X
			aFrustumEdge[1] = aFrustumCorner[XpYpZp] - aFrustumCorner[XpYmZp];	//+Y
			aFrustumEdge[2] = aFrustumCorner[XpYpZp] - aFrustumCorner[XpYpZm];	//+Z(+,+)
			aFrustumEdge[3] = aFrustumCorner[XmYpZp] - aFrustumCorner[XmYpZm];	//+Z(-,+)
			aFrustumEdge[4] = aFrustumCorner[XpYmZp] - aFrustumCorner[XpYmZm];	//+Z(+,-)
			aFrustumEdge[5] = aFrustumCorner[XmYmZp] - aFrustumCorner[XmYmZm];	//+Z(-,-)
			for (uint32_t i = 0; i < 6; ++i)
				aFrustumEdge[i] *= (0.05f / length(aFrustumEdge[i].getXYZ()));
			aFrustumVertex[ 0] = aFrustumCorner[XpYpZp] + aFrustumEdge[0] + aFrustumEdge[1];	//Z+
			aFrustumVertex[ 1] = aFrustumCorner[XmYpZp] - aFrustumEdge[0] + aFrustumEdge[1];
			aFrustumVertex[ 2] = aFrustumCorner[XpYmZp] + aFrustumEdge[0] - aFrustumEdge[1];
			aFrustumVertex[ 3] = aFrustumCorner[XmYmZp] - aFrustumEdge[0] - aFrustumEdge[1];
			aFrustumVertex[ 4] = aFrustumCorner[XmYmZm] - aFrustumEdge[0] - aFrustumEdge[1];	//Z-
			aFrustumVertex[ 5] = aFrustumCorner[XpYmZm] + aFrustumEdge[0] - aFrustumEdge[1];
			aFrustumVertex[ 6] = aFrustumCorner[XmYpZm] - aFrustumEdge[0] + aFrustumEdge[1];
			aFrustumVertex[ 7] = aFrustumCorner[XpYpZm] + aFrustumEdge[0] + aFrustumEdge[1];
			aFrustumVertex[ 8] = aFrustumCorner[XpYpZp] + aFrustumEdge[0] + aFrustumEdge[2];	//Y+
			aFrustumVertex[ 9] = aFrustumCorner[XpYpZm] + aFrustumEdge[0] - aFrustumEdge[2];
			aFrustumVertex[10] = aFrustumCorner[XmYpZp] - aFrustumEdge[0] + aFrustumEdge[3];
			aFrustumVertex[11] = aFrustumCorner[XmYpZm] - aFrustumEdge[0] - aFrustumEdge[3];
			aFrustumVertex[12] = aFrustumCorner[XmYmZm] - aFrustumEdge[0] - aFrustumEdge[5];	//Y-
			aFrustumVertex[13] = aFrustumCorner[XmYmZp] - aFrustumEdge[0] + aFrustumEdge[5];
			aFrustumVertex[14] = aFrustumCorner[XpYmZm] + aFrustumEdge[0] - aFrustumEdge[4];
			aFrustumVertex[15] = aFrustumCorner[XpYmZp] + aFrustumEdge[0] + aFrustumEdge[4];
			aFrustumVertex[16] = aFrustumCorner[XpYpZp] + aFrustumEdge[1] + aFrustumEdge[2];	//X+
			aFrustumVertex[17] = aFrustumCorner[XpYmZp] - aFrustumEdge[1] + aFrustumEdge[4];
			aFrustumVertex[18] = aFrustumCorner[XpYpZm] + aFrustumEdge[1] - aFrustumEdge[2];
			aFrustumVertex[19] = aFrustumCorner[XpYmZm] - aFrustumEdge[1] - aFrustumEdge[4];
			aFrustumVertex[20] = aFrustumCorner[XmYmZm] - aFrustumEdge[1] - aFrustumEdge[5];	//X-
			aFrustumVertex[21] = aFrustumCorner[XmYpZm] + aFrustumEdge[1] - aFrustumEdge[3];
			aFrustumVertex[22] = aFrustumCorner[XmYmZp] - aFrustumEdge[1] + aFrustumEdge[5];
			aFrustumVertex[23] = aFrustumCorner[XmYpZp] + aFrustumEdge[1] + aFrustumEdge[3];
		}

		Gnmx::GnmxGfxContext *gfxc = &commandBuffers[framework.m_backBufferIndex]; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
		gfxc->reset();
		gfxc->initializeDefaultHardwareState();
		framework.BeginFrame(*gfxc);

		if (config_useDispatchDraw != kUseDraw) {
			//NOTE: GfxContext by default limits dispatch draw asynchronous compute to run on CUs [0,2:7] (CU mask 0x0FD). We can limit this further here by successively knocking out additional CUs from 7 through 2:
			if (Gnm::getGpuMode() == Gnm::kGpuModeBase) {
				gfxc->setAsynchronousComputeResourceManagementForBase(Gnm::kShaderEngine0, 0x0FD & (0x0FF >> (7 - (config_numComputeUnits + 0) / 2)));
				gfxc->setAsynchronousComputeResourceManagementForBase(Gnm::kShaderEngine1, 0x0FD & (0x0FF >> (7 - (config_numComputeUnits + 1) / 2)));
			}
			else {
				// In NEO mode this will use double the specified number of CUs.
				gfxc->setAsynchronousComputeResourceManagementForNeo(Gnm::kShaderEngine0, 0x0FD & (0x0FF >> (7 - (config_numComputeUnits + 0) / 2)));
				gfxc->setAsynchronousComputeResourceManagementForNeo(Gnm::kShaderEngine1, 0x0FD & (0x0FF >> (7 - (config_numComputeUnits + 1) / 2)));
				gfxc->setAsynchronousComputeResourceManagementForNeo(Gnm::kShaderEngine2, 0x0FD & (0x0FF >> (7 - (config_numComputeUnits + 0) / 2)));
				gfxc->setAsynchronousComputeResourceManagementForNeo(Gnm::kShaderEngine3, 0x0FD & (0x0FF >> (7 - (config_numComputeUnits + 1) / 2)));
			}
			//NOTE: It is always advisable to limit the number of dispatch draw compute thread groups to approximately balance compute throughput against VS-PS throughput and ensure
			// that dispatch draw compute resource usage does not crowd out VS-PS resource usage.
			// As soon as the GPU becomes VS-PS throughput bound, back-pressure will cause dispatch compute thread groups to stall waiting for IRB space, filling the GPU to the extent 
			// allowed by the CU mask above and the thread group per CU limit.  This in turn restricts resources available to launch VS-PS wavefronts, which, if throughput is bounded by
			// VS or PS shader population, results in the GPU ending up locked into a lower performance state in which VS-PS is resource limited by an unnecessarily large complement of
			// stalled dispatch draw compute wavefronts.
			gfxc->setAsynchronousComputeShaderControl(0, config_numWavesPerCu/4, 0);

			//NOTE: numKickRingBufferElems controls how many asynchronous compute dispatches for dispatch draw can be outstanding at once.
			//      Generally, it's not usually necessary to have many dispatches outstanding because the index ring buffer only has so much
			//		space available for output, and it's not useful to launch compute wavefronts that won't have room.
			//		For this sample, 8 is more than sufficient to run with no loss in performance even with the simplest meshes.
//			uint32_t gdsSizeDispatchDrawArea = gfxc->getRequiredSizeOfGdsDispatchDrawArea(config_numKickRingBufferElems);
			uint32_t gdsOffsetDispatchDrawArea = 0; //or anywhere up to (Gnm::kGdsAccessibleMemorySizeInBytes - gdsSizeDispatchDrawArea) &~0x7;
			uint32_t gdsOaCounterIdIrb = 0;
			gfxc->setupDispatchDrawRingBuffers(pIndexRingBuffer, config_sizeofIndexRingBuffer, config_numKickRingBufferElems, gdsOffsetDispatchDrawArea, gdsOaCounterIdIrb);
		}

		// Render the scene:
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		// Clear
		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &framework.m_backBuffer->m_renderTarget, framework.getClearColor());
		Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &framework.m_backBuffer->m_depthTarget, 1.f);
		gfxc->setRenderTargetMask(0xF);
		gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
		gfxc->setRenderTarget(0, &framework.m_backBuffer->m_renderTarget);
		gfxc->setDepthRenderTarget(&framework.m_backBuffer->m_depthTarget);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLess);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);
		gfxc->setupScreenViewport(0, 0, framework.m_backBuffer->m_renderTarget.getWidth(), framework.m_backBuffer->m_renderTarget.getHeight(), 0.5f, 0.5f);

		Gnm::ClipControl clipControlReg;
		if (config_useDispatchDraw != kUseDraw) {
			gfxc->setupDispatchDrawClipCullSettings(primSetupReg, clipControlReg);
			gfxc->setupDispatchDrawScreenViewport(0, 0, framework.m_backBuffer->m_renderTarget.getWidth(), framework.m_backBuffer->m_renderTarget.getHeight());
		}


		{
			//Hack the VsShader parameter export count to experiment with the performance effects of exporting more parameters:
			Gnmx::VsShader *vsb;
			if (config_useDispatchDraw != kUseDraw) {
					vsb = const_cast<Gnmx::VsShader*>( (config_bUseInstancing ? computeVertexShaderInstanced : computeVertexShader)->getVertexShader() );
			} else
				vsb = config_bUseInstancing ? vertexShaderInstanced : vertexShader;
			vsb->m_vsStageRegisters.m_spiVsOutConfig = (vsb->m_vsStageRegisters.m_spiVsOutConfig &~ 0x3E) | ((config_numParameters-1) << 1);
		}
		if (config_useDispatchDraw != kUseDraw) {
			gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesDispatchDrawVsPs);
			if (config_bUseInstancing)
				gfxc->setDispatchDrawShader(computeVertexShaderInstanced, 0, fetchShaderDdVsInstanced, &computeVertexShaderInstanced_offsetsTableVS,
																		  0, fetchShaderDdCsInstanced, &computeVertexShaderInstanced_offsetsTableCS);
			else
				gfxc->setDispatchDrawShader(computeVertexShader, 0, fetchShaderDdVs, &computeVertexShader_offsetsTableVS,
																 0, fetchShaderDdCs, &computeVertexShader_offsetsTableCS);
		} else if (config_bUseInstancing) {
			gfxc->setVsShader(vertexShaderInstanced, 0, fetchShaderInstanced, &vertexShaderInstanced_offsetsTable);
		} else {
			gfxc->setVsShader(vertexShader, 0, fetchShader, &vertexShader_offsetsTable);
		}

		Framework::SimpleMesh &torusMesh = aTorusMesh[config_nMesh];
		if (config_bUseInstancing) {
			SCE_GNM_ASSERT( 3 + torusMesh.m_vertexAttributeCount < 32 );
			gfxc->setVertexBuffers(Gnm::kShaderStageVs, 3, torusMesh.m_vertexAttributeCount, torusMesh.m_buffer);
			if (config_useDispatchDraw != kUseDraw)
				gfxc->setDispatchDrawInstanceStepRate(1, 1);
			else
				gfxc->setInstanceStepRate(1, 1);
		} else
			torusMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		gfxc->setPsShader(pixelShader, &pixelShader_offsetsTable);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &trilinearSampler);

		if (config_useDispatchDraw != kUseDraw) {
			gfxc->beginDispatchDraw();
		} else {
			gfxc->setPrimitiveType(torusMesh.m_primitiveType);
			gfxc->setIndexSize(torusMesh.m_indexType);
		}

		Vector4Unaligned lightPosition = ToVector4Unaligned(framework.getLightPositionInViewSpace());
		Vector4Unaligned lightColor = ToVector4Unaligned(framework.getLightColor());
		Vector4Unaligned ambientColor = ToVector4Unaligned(framework.getAmbientColor());
		Vector4Unaligned lightAttenuation = ToVector4Unaligned(Vector4(1, 0, 0, 0));

		const float fT = framework.GetSecondsElapsedApparent();
		uint32_t const kPseudoRandomA = 1664525, kPseudoRandomC = 1013904223, kPseudoRandomSeed = 0x1F85E0D2;
		uint32_t uRand = kPseudoRandomSeed;
		uint32_t nObjectDrawn = 0, nDrawCount = 0;
		uint32_t nInstance = 0, nMatrixInstance0 = 0;
		bool bUseDispatchDrawPrev = (config_useDispatchDraw != kUseDraw);
		MatrixTransposed *aMatrixInstanceForFrame = aMatrixInstance + framework.m_backBufferIndex*kMaxObjects;
		for (uint32_t nObject = 0; nObject < config_numObjects; ++nObject, uRand = (uRand * kPseudoRandomA + kPseudoRandomC)) {
			int iZ = (int)( ((nObject >> 2) & 0x001) | ((nObject >> 4) & 0x002) | ((nObject >> 6) & 0x004) | ((nObject >> 8) & 0x008) );
			uint32_t uX = ((nObject >> 3) & 0x001) | ((nObject >> 5) & 0x002) | ((nObject >> 7) & 0x004) | ((nObject >> 9) & 0x008);
			uint32_t uY = ((nObject >> 4) & 0x001) | ((nObject >> 6) & 0x002) | ((nObject >> 8) & 0x004) | ((nObject >>10) & 0x008);
			int iX = (int)( ((nObject >> 0) & 0x001) ? ~uX : uX );
			int iY = (int)( ((nObject >> 1) & 0x001) ? ~uY : uY );
			float fZ = -iZ*kfObjectSpacing;
			float fX = iX*kfObjectSpacing + kfObjectSpacing*0.5f;
			float fY = iY*kfObjectSpacing + kfObjectSpacing*0.5f;

			bool bDrawObject = true;
			if (config_frustumCullObjects) {
				for (int iPlane = 0; iPlane < Framework::GnmSampleFramework::kPlanes; ++iPlane) {
					float fPlaneDist = aFrustumPlane[iPlane].m_equation.getX() * fX + aFrustumPlane[iPlane].m_equation.getY() * fY + aFrustumPlane[iPlane].m_equation.getZ() * fZ + aFrustumPlane[iPlane].m_equation.getW();
					if (fPlaneDist < -kfObjectCullingRadius) {
						bDrawObject = false;
						break;
					}
				}
				if (!bDrawObject && !(config_bUseInstancing && nObject + 1 == config_numObjects && nInstance > 0))
					continue;	//frustum culled object
			}

			Constants *constants = &constantBuffers[config_bUseInstancing ? framework.m_backBufferIndex*kMaxObjects : framework.m_backBufferIndex*kMaxObjects + nObject];
			Matrix4 m;
			if (bDrawObject) {
				float fRotX0 = 0.393f*(int)(((uRand>>6) & 0x7)*2-7), fRotY0 = 0.393f*(int)(((uRand>>9) & 0x7)*2-7), fRotZ0 = 0.393f*(int)(((uRand>>12) & 0x7)*2-7);
				float fRotXRate = 0.17f*(int)(((uRand>>0) & 0x3)*2-3), fRotYRate = 0.17f*(int)(((uRand>>2) & 0x3)*2-3), fRotZRate = 0.17f*(int)(((uRand>>4) & 0x3)*2-3);

				m = Matrix4::rotationZYX(Vector3(fRotX0 + fT*fRotXRate, fRotY0 + fT*fRotYRate, fRotZ0 + fT*fRotZRate));
				m.setTranslation(Vector3(fX, fY, fZ));

				if (config_bUseInstancing) {
					if (nObjectDrawn == 0) {
						constants->m_modelView = transpose(framework.m_worldToViewMatrix);
						constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix);
						constants->m_lightPosition = lightPosition;
						constants->m_lightColor = lightColor;
						constants->m_ambientColor = ambientColor;
						constants->m_lightAttenuation = lightAttenuation;

						Gnm::Buffer constantBuffer;
						constantBuffer.initAsConstantBuffer(constants, sizeof(Constants));
						constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
						gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
						gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);
					}
				} else
				{
					constants->m_modelView = transpose(framework.m_worldToViewMatrix*m);
					constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*m);
					constants->m_lightPosition = lightPosition;
					constants->m_lightColor = lightColor;
					constants->m_ambientColor = ambientColor;
					constants->m_lightAttenuation = lightAttenuation;

					Gnm::Buffer constantBuffer;
					constantBuffer.initAsConstantBuffer(constants, sizeof(Constants));
					constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
					gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
					gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);
				}
			}

			bool bUseDispatchDraw = (config_useDispatchDraw == kUseDispatchDraw) || (config_useDispatchDraw == kUseMixedDrawAndDispatchDraw && !(nDrawCount & 0x4));
			if (bUseDispatchDrawPrev != bUseDispatchDraw) {
				bUseDispatchDrawPrev = bUseDispatchDraw;
				if (bUseDispatchDraw) {
					if (config_bUseInstancing)
						gfxc->setNumInstances(1);
					gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesDispatchDrawVsPs);
					if (config_bUseInstancing)
						gfxc->setDispatchDrawShader(computeVertexShaderInstanced, 0, fetchShaderDdVsInstanced, &computeVertexShaderInstanced_offsetsTableVS,
																				  0, fetchShaderDdCsInstanced, &computeVertexShaderInstanced_offsetsTableCS);
					else
						gfxc->setDispatchDrawShader(computeVertexShader, 0, fetchShaderDdVs, &computeVertexShader_offsetsTableVS,
																		 0, fetchShaderDdCs, &computeVertexShader_offsetsTableCS);

					gfxc->beginDispatchDraw();
				} else {
					gfxc->endDispatchDraw(torusMesh.m_indexType, NULL);

					gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
					if (config_bUseInstancing) {
						gfxc->setVsShader(vertexShaderInstanced, 0, fetchShaderInstanced, &vertexShaderInstanced_offsetsTable);
					} else {
						gfxc->setVsShader(vertexShader, 0, fetchShader, &vertexShader_offsetsTable);
					}

					gfxc->setPrimitiveType(torusMesh.m_primitiveType);
				}
			}

			if (config_bUseInstancing) {
				if (bDrawObject) {
					aMatrixInstanceForFrame[nObjectDrawn].col0 = ToVector4Unaligned( m.getRow(0) );
					aMatrixInstanceForFrame[nObjectDrawn].col1 = ToVector4Unaligned( m.getRow(1) );
					aMatrixInstanceForFrame[nObjectDrawn].col2 = ToVector4Unaligned( m.getRow(2) );
					++nInstance;
				}
				if (nInstance == config_numInstancesPerDraw || nObject+1 == config_numObjects) {
					Gnm::Buffer aBufMatrixInstance[3];
					aBufMatrixInstance[0].initAsVertexBuffer((uint8_t*)(aMatrixInstanceForFrame + nMatrixInstance0) + offsetof(MatrixTransposed, col0), Gnm::kDataFormatR32G32B32A32Float, sizeof(MatrixTransposed), nInstance);
					aBufMatrixInstance[1].initAsVertexBuffer((uint8_t*)(aMatrixInstanceForFrame + nMatrixInstance0) + offsetof(MatrixTransposed, col1), Gnm::kDataFormatR32G32B32A32Float, sizeof(MatrixTransposed), nInstance);
					aBufMatrixInstance[2].initAsVertexBuffer((uint8_t*)(aMatrixInstanceForFrame + nMatrixInstance0) + offsetof(MatrixTransposed, col2), Gnm::kDataFormatR32G32B32A32Float, sizeof(MatrixTransposed), nInstance);
					aBufMatrixInstance[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
					aBufMatrixInstance[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
					aBufMatrixInstance[2].setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
					gfxc->setVertexBuffers(Gnm::kShaderStageVs, 0, 3, aBufMatrixInstance);

					if (bUseDispatchDraw) {
						gfxc->setDispatchDrawNumInstances(nInstance);
						if (!config_bFreezeCullingFrustum) {
							gfxc->dispatchDraw(&aDispatchDrawInputData[config_nMesh]);
						} else {
							Constants *constantsFrozen = &constantBuffersFrozen[framework.m_backBufferIndex*kMaxObjects];
							if (nMatrixInstance0 == 0) {
								constantsFrozen->m_modelView = constants->m_modelView;	//unused by DispatchDraw CS
								constantsFrozen->m_modelViewProjection = transpose(viewProjectionMatrixForCulling);
								constantsFrozen->m_lightPosition = lightPosition;		//unused by DispatchDraw CS
								constantsFrozen->m_lightColor = lightColor;				//unused by DispatchDraw CS
								constantsFrozen->m_ambientColor = ambientColor;			//unused by DispatchDraw CS
								constantsFrozen->m_lightAttenuation = lightAttenuation;	//unused by DispatchDraw CS
							}
							Gnm::Buffer constantBufferFrozen;
							constantBufferFrozen.initAsConstantBuffer(constantsFrozen, sizeof(Constants));
							constantBufferFrozen.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

							//HACK: Since our CS and VS shader read the constant V# from immediate registers, we can hack them to be different here in order to set the view matrix separately.
							// To do this, we have to modify the standard implementation of gfxc->dispatchDraw():
							dispatchDraw_overrideComputeContantBuffer(gfxc, &aDispatchDrawInputData[config_nMesh], userSgprComputeConstBuffer0, constantBufferFrozen);
						}
					} else {
						gfxc->setNumInstances(nInstance);
						gfxc->drawIndex(torusMesh.m_indexCount, torusMesh.m_indexBuffer);
					}

					nMatrixInstance0 = nObjectDrawn+1;
					nInstance = 0;
					++nDrawCount;
				}
			} else {
				if (bUseDispatchDraw) {
					if (!config_bFreezeCullingFrustum) {
						gfxc->dispatchDraw(&aDispatchDrawInputData[config_nMesh]);
					} else {
						Constants *constantsFrozen = &constantBuffersFrozen[framework.m_backBufferIndex*kMaxObjects + nObject];
						constantsFrozen->m_modelView = constants->m_modelView;	//unused by DispatchDraw CS
						constantsFrozen->m_modelViewProjection = transpose(viewProjectionMatrixForCulling*m);
						constantsFrozen->m_lightPosition = lightPosition;		//unused by DispatchDraw CS
						constantsFrozen->m_lightColor = lightColor;				//unused by DispatchDraw CS
						constantsFrozen->m_ambientColor = ambientColor;			//unused by DispatchDraw CS
						constantsFrozen->m_lightAttenuation = lightAttenuation;	//unused by DispatchDraw CS

						Gnm::Buffer constantBufferFrozen;
						constantBufferFrozen.initAsConstantBuffer(constantsFrozen, sizeof(Constants));
						constantBufferFrozen.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

						//HACK: Since our CS and VS shader read the constant V# from immediate registers, we can hack them to be different here in order to set the view matrix separately.
						// To do this, we have to modify the standard implementation of gfxc->dispatchDraw():
						dispatchDraw_overrideComputeContantBuffer(gfxc, &aDispatchDrawInputData[config_nMesh], userSgprComputeConstBuffer0, constantBufferFrozen);
					}
				} else {
					gfxc->drawIndex(torusMesh.m_indexCount, torusMesh.m_indexBuffer);
				}
				++nDrawCount;
			}
			++nObjectDrawn;
		}
		if (config_bUseInstancing) {
			if (bUseDispatchDrawPrev)
				gfxc->setDispatchDrawNumInstances(1);
			else
				gfxc->setNumInstances(1);
		}
		if (bUseDispatchDrawPrev) {
			gfxc->endDispatchDraw(Gnm::kIndexSize16, NULL);
			gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
		}

		// Render view frustum using transparent debug triangles if frozen:
		if (config_bFreezeCullingFrustum) {
			static Vector4 colorFrustum(1.0f, 1.0f, 0.0f, 0.05f);
			const Vector4 colorWhite(1.0f, 1.0f, 1.0f, 1.0f);
			framework.m_backBuffer->m_debugObjects.BeginDebugMesh();
			for (uint32_t i = 0; i < 6; ++i) {
				framework.m_backBuffer->m_debugObjects.AddDebugTriangle(MakeDebugVertex(aFrustumVertex[4*i+0], colorWhite), MakeDebugVertex(aFrustumVertex[4*i+3], colorWhite), MakeDebugVertex(aFrustumVertex[4*i+1], colorWhite));
				framework.m_backBuffer->m_debugObjects.AddDebugTriangle(MakeDebugVertex(aFrustumVertex[4*i+0], colorWhite), MakeDebugVertex(aFrustumVertex[4*i+2], colorWhite), MakeDebugVertex(aFrustumVertex[4*i+3], colorWhite));
			}
			framework.m_backBuffer->m_debugObjects.EndDebugMesh(colorFrustum, framework.m_viewProjectionMatrix);
		}

		framework.EndFrame(*gfxc);
	}

	Gnmx::GnmxGfxContext *gfxc = &commandBuffers[framework.m_backBufferIndex];
	framework.terminate(*gfxc);

	computeQueue.unmap();
	return 0;
}
