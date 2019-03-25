/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <framework/sample_framework.h>
#include <framework/simple_mesh.h>
#include <framework/gnf_loader.h>
#include <framework/frame.h>
using namespace sce;

int main(int argc, const char *argv[])
{
	Framework::GnmSampleFramework framework;
	framework.processCommandLine(argc, argv);

	framework.m_config.m_cameraControls = false;
	framework.m_config.m_depthBufferIsMeaningful = false;
	framework.m_config.m_clearColorIsMeaningful = false;

    framework.initialize( "Texture Array", 
		"This sample demonstrates how to use texture arrays.",
        "This sample uses a texture array to render multiple textures using one T#.");

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		uint32_t *m_constants;
		Gnm::Buffer m_constantBuffer;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Command Buffer",buffer);
		frame->m_constantBuffer.initAsConstantBuffer(frame->m_constants,sizeof(*frame->m_constants));
		frame->m_constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
	}

	int error = 0;

	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader;

	error = Framework::LoadVsShader(&vertexShader, Framework::ReadFile("/app0/texture-array-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/texture-array-sample/shader_p.sb"), &framework.m_allocators);

	//
	// Setup the Texture
	//
    Gnm::Texture srcTex;
	error = Framework::loadTextureFromGnf(&srcTex, Framework::ReadFile("/app0/assets/texture_array.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	srcTex.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture won't be bound as RWTexture, so it's OK to declare read-only

	Gnm::Sampler sampler;
	sampler.init();
	sampler.setMipFilterMode(Gnm::kMipFilterModeLinear);

	//
	// Main Loop:
	//	
	while (!framework.m_shutdownRequested)
	{
		Frame *frame = frames + framework.m_backBufferIndex;
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.

		gfxc->reset();
		framework.BeginFrame(*gfxc);

        // Clear
        Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &framework.m_backBuffer->m_renderTarget, framework.getClearColor());
        Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &framework.m_backBuffer->m_depthTarget, 1.0f);
        gfxc->setRenderTargetMask(0xF);

        gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
		gfxc->setRenderTarget(0, &framework.m_backBuffer->m_renderTarget);
		gfxc->setDepthRenderTarget(&framework.m_backBuffer->m_depthTarget);

		gfxc->setupScreenViewport(0, 0, framework.m_backBuffer->m_renderTarget.getWidth(), framework.m_backBuffer->m_renderTarget.getHeight(), 0.5f, 0.5f);

		gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
 		gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

        // constant update
	    *frame->m_constants = static_cast<uint32_t>(framework.GetSecondsElapsedApparent() * 30);
	    gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &frame->m_constantBuffer);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &srcTex);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);
		
        // draw screen quad
		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
		gfxc->drawIndexAuto(3);
		framework.EndFrame(*gfxc);
    }

	Frame *frame = frames + framework.m_backBufferIndex;
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
    return 0;
}
