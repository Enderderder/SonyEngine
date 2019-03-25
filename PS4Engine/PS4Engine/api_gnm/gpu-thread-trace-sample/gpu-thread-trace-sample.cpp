/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/frame.h"
#include "std_cbuffer.h"
#include <razor_gpu_thread_trace.h>
#include <libsysmodule.h>

using namespace sce;

namespace
{
	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	bool isPaDebugEnabled = sce::Gnm::isUserPaEnabled(); // For checking RAZOR THREAD TRACE Dynamic Link status.

	framework.processCommandLine(argc, argv);

	framework.m_config.m_lightingIsMeaningful = true;
	const char kPaDebugOverview [] = "Sample code to demonstrate capturing GPU traces with libSceRazorGpuThreadTrace.";
	const char kNoPaDebugOverview [] = "Sample code to demonstrate capturing GPU trace, this requires Debug Setting Graphics PA Debug=\"Yes\" to call sceRazor functions. Current value is \"No\".";
	framework.initialize( "Gpu Thread Trace", 
		(isPaDebugEnabled) ? kPaDebugOverview : kNoPaDebugOverview,
		"This sample program captures GPU traces for 8 consecutive frames. The trace files are written to the file serving directory and can be viewed within the host tool, Razor GPU Thread Trace Viewer.",
		argc, argv );

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Constants *m_constants;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Command Buffer",buffer);
	}

	void *fetchShader;
	Gnmx::VsShader *vertexShader = Framework::LoadVsMakeFetchShader(&fetchShader, "/app0/gpu-thread-trace-sample/shader_vv.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);

	Gnmx::PsShader *pixelShader = Framework::LoadPsShader("/app0/gpu-thread-trace-sample/shader_p.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache pixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&pixelShader_offsetsTable, Gnm::kShaderStagePs, pixelShader);

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

	Framework::SimpleMesh torusMesh;
	BuildTorusMesh(&framework.m_allocators, "Torus", &torusMesh, 0.8f, 0.2f, 64, 32, 4, 1);

	// initialize thread trace parameters
	sce::Gnm::SqPerfCounter	counters[] = 
	{
		sce::Gnm::kSqPerfCounterWaveCycles,		
		sce::Gnm::kSqPerfCounterWaveReady,		
		sce::Gnm::kSqPerfCounterInsts,			
		sce::Gnm::kSqPerfCounterInstsValu,		
		sce::Gnm::kSqPerfCounterWaitCntAny,		
		sce::Gnm::kSqPerfCounterWaitCntVm,		
		sce::Gnm::kSqPerfCounterWaitCntExp,		
		sce::Gnm::kSqPerfCounterWaitExpAlloc,	
		sce::Gnm::kSqPerfCounterWaitAny,		
		sce::Gnm::kSqPerfCounterIfetch,			
		sce::Gnm::kSqPerfCounterWaitIfetch,		
		sce::Gnm::kSqPerfCounterSurfSyncs,		
		sce::Gnm::kSqPerfCounterEvents,			
		sce::Gnm::kSqPerfCounterInstsBranch,	
		sce::Gnm::kSqPerfCounterValuDepStall,	
		sce::Gnm::kSqPerfCounterCbranchFork		
	};

	uint32_t numCounters = sizeof(counters) / sizeof(counters[0]);

	SceRazorGpuThreadTraceParams params;
	memset(&params, 0, sizeof(SceRazorGpuThreadTraceParams));
	params.sizeInBytes = sizeof(SceRazorGpuThreadTraceParams);
	memcpy(params.counters, counters, sizeof(counters));
	params.numCounters = numCounters;
	params.counterRate = SCE_RAZOR_GPU_THREAD_TRACE_COUNTER_RATE_HIGH;
	params.enableInstructionIssueTracing = false;
	params.shaderEngine0ComputeUnitIndex = 0;
	params.shaderEngine1ComputeUnitIndex = 9;

	SceRazorGpuErrorCode ret = 0;
	uint32_t *streamingCounters = params.streamingCounters;

	// ----- global streaming counters

	if(isPaDebugEnabled){
	// TCC
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterMcWrreq);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterWrite);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterMcRdreq);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterRead);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterHit);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterMiss);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTccPerfCounterReq);

	// TCA
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTcaPerfCounterCycle); 
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTcaPerfCounterBusy);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTcaPerfCounterReqTcs);

	// IA
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kIaPerfCounterIaBusy);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kIaPerfCounterIaDmaReturn);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kIaPerfCounterIaStalled);

	// ----- SE streaming counters

	// SX
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterPaIdleCycles, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterPaReq, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterPaPos, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterClock, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterShPosStarve, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterShColorStarve, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterShPosStall, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kSxPerfCounterShColorStall, SCE_RAZOR_GPU_BROADCAST_ALL);

	// PA-SU
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kPaSuPerfCounterPaInputPrim, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kPaSuPerfCounterPasxReq, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kPaSuPerfCounterPaInputEndOfPacket, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kPaSuPerfCounterPaInputNullPrim, SCE_RAZOR_GPU_BROADCAST_ALL);

	// VGT
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kVgtPerfCounterVgtSpiVsvertSend, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kVgtPerfCounterVgtSpiVsvertEov, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kVgtPerfCounterVgtSpiVsvertStalled, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kVgtPerfCounterVgtSpiVsvertStarvedBusy, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kVgtPerfCounterVgtSpiVsvertStarvedIdle, SCE_RAZOR_GPU_BROADCAST_ALL);

	// TA
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTaPerfCounterBufferWavefronts, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTaPerfCounterImageReadWavefronts, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTaPerfCounterTaBusy, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kTaPerfCounterShFifoBusy, SCE_RAZOR_GPU_BROADCAST_ALL);

	// CB
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kCbPerfCounterCcCacheHit, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kCbPerfCounterCcMcWriteRequest, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kCbPerfCounterCcMcWriteRequestsInFlight, SCE_RAZOR_GPU_BROADCAST_ALL);
	ret |= sceRazorGpuCreateStreamingCounter(streamingCounters++, sce::Gnm::kCbPerfCounterCcMcReadRequest, SCE_RAZOR_GPU_BROADCAST_ALL);
	}

	SCE_GNM_ASSERT(ret == SCE_OK);

	params.numStreamingCounters = streamingCounters - params.streamingCounters;
	params.streamingCounterRate = SCE_RAZOR_GPU_THREAD_TRACE_STREAMING_COUNTER_RATE_HIGH;

	while (!framework.m_shutdownRequested)
	{
		// initialize thread trace for this frame
		if(isPaDebugEnabled)
			ret = sceRazorGpuThreadTraceInit(&params);
		if(ret == SCE_RAZOR_GPU_THREAD_TRACE_ERROR_PA_DEBUG_NOT_ENABLED)
		{
			printf("Please enable PA Debug in target settings\n");
			break;
		}
		SCE_GNM_ASSERT(ret == SCE_OK);

		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		// add commands to the command buffer to signal thread trace to start
		if(isPaDebugEnabled)
			ret = sceRazorGpuThreadTraceStart(&gfxc->m_dcb);
		SCE_GNM_ASSERT(ret == SCE_OK);

		// Render the scene:
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		// Clear
		gfxc->pushMarker("Clear");
		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
		Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
		gfxc->popMarker();
		gfxc->setRenderTargetMask(0xF);
		gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
		gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
		gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLess);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);
		gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);

		gfxc->setVsShader(vertexShader, 0, fetchShader, &vertexShader_offsetsTable);
		gfxc->setPsShader(pixelShader, &pixelShader_offsetsTable);

		torusMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		const float radians = framework.GetSecondsElapsedApparent() * 0.5f;
		const Matrix4 m = Matrix4::rotationZYX(Vector3(radians,radians,0.f));
		Constants *constants = frame->m_constants;
		constants->m_modelView = transpose(framework.m_worldToViewMatrix*m);
		constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*m);
		constants->m_lightPosition = framework.getLightPositionInViewSpace();
		constants->m_lightColor = framework.getLightColor();
		constants->m_ambientColor = framework.getAmbientColor();
		constants->m_lightAttenuation = Vector4(1, 0, 0, 0);

		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(constants, sizeof(Constants));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &trilinearSampler);

		gfxc->pushMarker("Draw torus");
		gfxc->setPrimitiveType(torusMesh.m_primitiveType);
		gfxc->setIndexSize(torusMesh.m_indexType);
		gfxc->drawIndex(torusMesh.m_indexCount, torusMesh.m_indexBuffer);
		gfxc->popMarker();

		// add commands to the command buffer to signal thread trace to stop
		if(isPaDebugEnabled)
			ret = sceRazorGpuThreadTraceStop(&gfxc->m_dcb);
		SCE_GNM_ASSERT(ret == SCE_OK);

		// create a sync point
		// the GPU will write "1" to a label to signify that the trace has finished
		volatile uint64_t *label = (uint64_t*)gfxc->m_dcb.allocateFromCommandBuffer(8, Gnm::kEmbeddedDataAlignment8);
		*label = 0;
		gfxc->writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)label, 1, Gnm::kCacheActionNone);

		framework.EndFrame(*gfxc);

		// wait for the trace to finish
		uint32_t wait = 0;
		while(*label != 1)
			++wait;

		// save the trace
		char const* szFilenameBase = "/hostapp/thread_trace";
		char szFilename[128];
		static uint32_t i = 0;
		sprintf_s(szFilename, 128, "%s_%2d.rtt", szFilenameBase, i++);
		if(isPaDebugEnabled) {
			printf("Saving trace %s\n", szFilename);
			ret = sceRazorGpuThreadTraceSave(szFilename);
		}
		SCE_GNM_ASSERT(ret == SCE_OK);

		// shutdown thread trace
		if(isPaDebugEnabled)
			ret = sceRazorGpuThreadTraceShutdown();
		SCE_GNM_ASSERT(ret == SCE_OK);

		if(isPaDebugEnabled && i == 8)
			break;
	}
	
	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
}
