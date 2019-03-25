/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/frame.h"
#include "std_cbuffer.h"
using namespace sce;

namespace
{
	Framework::GnmSampleFramework framework;
	enum {kQuads = 100};
}

int main(int argc, const char *argv[])
{
	enum ShaderSelect 
	{
		kOriginalShader,
		kReZShader,
		kNumShaders
	};
	ShaderSelect selectedShader = kOriginalShader;
	bool forceShaderMode = false;
	uint32_t sortOption = 1;
	uint32_t expectedZ = 0;
	uint32_t requestedZ = 0;
	
	Framework::MenuItemText sortText[] = {
		{"Back to front", "Back to front"},
		{"Front to back", "Front to back"}
	};
	const char* zBehaviorText[] = {
		"LateZ",
		"EarlyZ",
		"ReZ"
	};
	
	Framework::MenuItemText shaderSelectText[] = {
		{"Original","Select the original shader, requesting EarlyZ but running with LateZ because of the pixel discard"},
		{"ReZ","Select the shader compiled with the ReZ option"}
	};
	Framework::MenuCommandBool forceShaderModeCmd(&forceShaderMode);
	Framework::MenuCommandEnum sortObjectsCmd(sortText,sizeof(sortText)/sizeof(sortText[0]),&sortOption);
	Framework::MenuCommandEnum shaderSelectCmd(shaderSelectText,sizeof(shaderSelectText)/sizeof(shaderSelectText[0]),&selectedShader);
	const Framework::MenuItem menuItem[] =
	{
		{{"Select shader","Select shader"},&shaderSelectCmd},
		{{"Force shader's z", "Forces the z mode set in the shader ignoring adjustments needed to accommodate pixel kill and other options"},&forceShaderModeCmd},
		{{"Sort objects", "Sorts objects back to front/front to back"},&sortObjectsCmd }
		
	};
	const int kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]);

	framework.processCommandLine(argc, argv);

	framework.m_config.m_lightingIsMeaningful = true;

	framework.initialize( "Depth mode", 
		"Sample code to demonstrate effects of different depth modes.",
		"This sample program displays several quads that can be rendered with different depth modes and sorting.");
	framework.appendMenuItems(kMenuItems, menuItem);

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
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants) * kQuads,4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Command Buffer",buffer);
	}

	int error = 0;

	Framework::VsShader vertexShader;
	Framework::PsShader pixelShaders[kNumShaders];

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/depth-mode-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShaders[kOriginalShader], Framework::ReadFile("/app0/depth-mode-sample/shader_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShaders[kReZShader], Framework::ReadFile("/app0/depth-mode-sample/shader_rez_p.sb"), &framework.m_allocators);


	Gnm::Texture textures[2];
	error = Framework::loadTextureFromGnf(&textures[0], Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&textures[1], Framework::ReadFile("/app0/assets/icelogo-normal.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's OK to mark it as read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's OK to mark it as read-only.

	Gnm::Sampler trilinearSampler;
	trilinearSampler.init();
	trilinearSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	trilinearSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	Framework::SimpleMesh quadMesh;
	
	BuildQuadMesh(&framework.m_garlicAllocator,&quadMesh,2.0f);

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	
		Framework::PsShader* pixelShader = &pixelShaders[selectedShader];

		expectedZ = pixelShader->m_shader->m_psStageRegisters.getExpectedGpuZBehavior();
		requestedZ = pixelShader->m_shader->m_psStageRegisters.getShaderZBehavior();

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		// Render the scene:
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceNone);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		// Clear
		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
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

		gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
		gfxc->setPsShader(pixelShader->m_shader, &pixelShader->m_offsetsTable);

		quadMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		Gnm::RenderOverrideControl ovr;
		ovr.init();
		ovr.setForceShaderZBehavior(forceShaderMode);
		gfxc->setRenderOverrideControl(ovr);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &trilinearSampler);

		gfxc->setPrimitiveType(quadMesh.m_primitiveType);
		gfxc->setIndexSize(quadMesh.m_indexType);

		bufferCpuIsWriting->m_window.setCursor(0, 24);
		bufferCpuIsWriting->m_window.printf("%cRequested z-mode:%s",forceShaderMode?'*':' ',zBehaviorText[requestedZ]);
		bufferCpuIsWriting->m_window.setCursor(0, 25);
		bufferCpuIsWriting->m_window.printf("%cExpected z-mode: %s",forceShaderMode?' ':'*',zBehaviorText[expectedZ]);
		for(int t=0; t != kQuads; ++t)
		{
			const int n = sortOption == 0? (kQuads-1)-t : t;
			const float radians = framework.GetSecondsElapsedApparent() * (0.5f + n*0.01f);
			const Matrix4 m = Matrix4::rotationZYX(Vector3(0.f,0.f,radians)) * Matrix4::translation(Vector3(0.f,0.f,-n*0.1f));
			Constants *constants = frame->m_constants + t;
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

			gfxc->drawIndex(quadMesh.m_indexCount, quadMesh.m_indexBuffer);
		}

		
		framework.EndFrame(*gfxc);
	}
	
	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
}
