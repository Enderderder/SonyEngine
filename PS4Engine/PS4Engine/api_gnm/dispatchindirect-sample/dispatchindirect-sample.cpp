/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/frame.h"
using namespace sce;

namespace 
{
	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.m_config.m_depthBufferIsMeaningful = false;
	framework.m_config.m_lightingIsMeaningful = false;

	framework.initialize( "DispatchIndirect", 
		"Sample code for dispatching a compute shader whose threadgroup count is unknown when the command buffer is built.", 
		"This sample program illustrates how one can dispatch a compute shader without knowing its threadgroup count "
		"at the time the command buffer is built, by having the GPU write a threadgroup count into a buffer "
		"after the command buffer is submitted, and before the dispatch is issued.");

	struct Constants
	{
		Matrix4Unaligned m_inverseProjection;
		Matrix4Unaligned m_worldToView;
		Matrix4Unaligned m_viewToLocal;
		Vector4Unaligned m_clearColor;
		Vector4Unaligned m_lightPosition;
		Vector4Unaligned m_lightColor;
		Vector4Unaligned m_ambientColor;
		Vector4Unaligned m_nearFar;
	};

	int error = 0;

	Framework::CsShader precalculateShader, renderShader;

	error = Framework::LoadCsShader(&precalculateShader, Framework::ReadFile("/app0/dispatchindirect-sample/precalculate_c.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&renderShader, Framework::ReadFile("/app0/dispatchindirect-sample/render_c.sb"), &framework.m_allocators);

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Constants *m_constants;
		Gnm::DispatchIndirectArgs *m_dispatchIndirectMem;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),Gnm::kAlignmentOfBufferInBytes,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_dispatchIndirectMem,SCE_KERNEL_WC_GARLIC,sizeof(*frame->m_dispatchIndirectMem),Gnm::kAlignmentOfIndirectArgsInBytes,Gnm::kResourceTypeGenericBuffer,"Buffer &d DispatchIndirectArgs",buffer);
		memset(frame->m_dispatchIndirectMem,0,sizeof(Gnm::DispatchIndirectArgs));
	}

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		Gnm::Buffer dispatch_indir_buffer;
		dispatch_indir_buffer.initAsDataBuffer(frame->m_dispatchIndirectMem, Gnm::kDataFormatR32Uint, sizeof(Gnm::DispatchIndirectArgs) / sizeof(uint32_t));
		dispatch_indir_buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we bind as RWBuffer, so it's GPU coherent

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
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

		Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);

		Gnm::Texture texture;
		texture.initFromRenderTarget(&bufferCpuIsWriting->m_renderTarget, false);
		texture.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we will bind this texture as an RWTexture, so GPU coherent it is.
		Gnm::DataFormat dataFormat = texture.getDataFormat();
		if(Gnm::kTextureChannelTypeSrgb == dataFormat.m_bits.m_channelType)
			dataFormat.m_bits.m_channelType = Gnm::kTextureChannelTypeUNorm;

		gfxc->setCsShader(precalculateShader.m_shader, &precalculateShader.m_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStageCs, 0, 1, &texture);
		gfxc->setRwBuffers(Gnm::kShaderStageCs, 0, 1, &dispatch_indir_buffer);
		gfxc->dispatch(1,1,1);

		Gnmx::Toolkit::synchronizeComputeToCompute(&gfxc->m_dcb);
		// an L2 flush is required for the dispatchIndirect.
		gfxc->flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2,0,Gnm::kStallCommandBufferParserEnable);

		Matrix4 post_adjust = Matrix4::identity();
		for(int q=0; q<3; q++)
		{
			const int iS = q==0 ? framework.m_config.m_targetWidth : (q==1 ? framework.m_config.m_targetHeight : 1);
			post_adjust.setRow( q, Vector4(iS*(q==0?0.5f:0), iS*(q==1?-0.5f:0), q==2?0.5f:0, iS*0.5f) );
		}
		Matrix4 m44InvProj = inverse(post_adjust*framework.m_projectionMatrix);

		const float radians = framework.GetSecondsElapsedApparent() * 0.5f;

		Constants *constants = frame->m_constants;
		constants->m_inverseProjection = transpose(m44InvProj);
		constants->m_worldToView = transpose(framework.m_worldToViewMatrix);
		const Matrix4 localToWorld = Matrix4::rotationZYX(Vector3(radians, radians, 0.f));
		const Matrix4 localToView = framework.m_worldToViewMatrix * localToWorld;
		const Matrix4 viewToLocal = inverse(localToView);
		constants->m_viewToLocal = transpose(viewToLocal);
		constants->m_clearColor = framework.getClearColor();
		constants->m_lightPosition = framework.getLightPositionInWorldSpace();
		constants->m_lightColor = framework.getLightColor();
		constants->m_ambientColor = framework.getAmbientColor();
		constants->m_nearFar.x = framework.GetDepthNear();
		constants->m_nearFar.y = framework.GetDepthFar();

		gfxc->setCsShader(renderShader.m_shader, &renderShader.m_offsetsTable);

		Gnm::Buffer buffer;
		buffer.initAsConstantBuffer(constants, sizeof(*constants));
		buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &buffer);

		gfxc->setRwTextures(Gnm::kShaderStageCs, 0, 1, &texture);

		gfxc->setBaseIndirectArgs(Gnm::kShaderTypeCompute, frame->m_dispatchIndirectMem);
		gfxc->stallCommandBufferParser();
		gfxc->dispatchIndirect(0);

		Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);


		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
}
