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

#if SCE_GNMX_ENABLE_GFX_LCUE
#undef SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES 
#define SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES 0
#endif // SCE_GNMX_ENABLE_GFX_LCUE

namespace
{
	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
#if SCE_GNMX_ENABLE_GFX_LCUE
	// this sample does not support the LCUE
	SCE_GNM_ASSERT_MSG(0, "User Resource Tables (URT) are not supported by the LCUE, please recompile and run with SCE_GNMX_ENABLE_GFX_LCUE 0");
	SCE_GNM_UNUSED(argc); SCE_GNM_UNUSED(argv);
	return 0;
#else
	framework.processCommandLine(argc, argv);

	framework.m_config.m_lightingIsMeaningful = true;

	framework.initialize( "Basic URT", 
		"Sample code to demonstrate rendering a torus with User Resource Tables.",
		"This sample program displays a torus with per-pixel albedo and normal using User Resource Tables.");

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

	int error = 0;
	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader;

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/basic-urt-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/basic-urt-sample/shader_p.sb"), &framework.m_allocators);

	// We allocate a user resource table for the textures (which go into the same table with buffers) so it has to be in garlic.
	Gnm::Texture *textures;
	framework.m_allocators.allocate((void**)&textures, SCE_KERNEL_WC_GARLIC, sizeof(Gnm::Texture) * 2, 16, Gnm::kResourceTypeBufferBaseAddress, "Texture Buffer");
    error = Framework::loadTextureFromGnf(&textures[0], Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&textures[1], Framework::ReadFile("/app0/assets/icelogo-normal.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's OK to mark it as read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's OK to mark it as read-only.

	// Samplers go into a table too but there is only one entry in the sampler table of this shader.
	Gnm::Sampler* trilinearSampler;
	framework.m_allocators.allocate((void**)&trilinearSampler, SCE_KERNEL_WC_GARLIC, sizeof(Gnm::Sampler), 16, Gnm::kResourceTypeBufferBaseAddress, "Sampler Buffer");
	trilinearSampler->init();
	trilinearSampler->setMipFilterMode(Gnm::kMipFilterModeLinear);
	trilinearSampler->setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	Framework::SimpleMesh torusMesh;
	BuildTorusMesh(&framework.m_allocators, "Torus", &torusMesh, 0.8f, 0.2f, 64, 32, 4, 1);

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		// Render the scene:
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
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

		gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader);
		gfxc->setPsShader(pixelShader.m_shader);

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
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		// Set the user tables for resources (textures) and samplers.
		gfxc->setUserResourceTable(Gnm::kShaderStagePs,textures);
		gfxc->setUserSamplerTable(Gnm::kShaderStagePs,trilinearSampler);
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		// We still can use regular CUE for the vertex buffers since vertex buffers go into a separate table from textures and samplers.
		torusMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);
		
		gfxc->setPrimitiveType(torusMesh.m_primitiveType);
		gfxc->setIndexSize(torusMesh.m_indexType);
		gfxc->drawIndex(torusMesh.m_indexCount, torusMesh.m_indexBuffer);
		// Because the framework.EndFrame is going to set up its shaders using regular CUE API we need to clear the user tables before we call it.
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		gfxc->setUserResourceTable(Gnm::kShaderStagePs,NULL);
		gfxc->setUserSamplerTable(Gnm::kShaderStagePs,NULL);
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		framework.EndFrame(*gfxc);
	}
	
	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
#endif // SCE_GNMX_ENABLE_GFX_LCUE
}
