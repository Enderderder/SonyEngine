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
	const uint32_t kMaxInstances = 50;

	int32_t g_stepRate0 = 5;
	int32_t g_numInstances = 35;
	Framework::MenuCommandInt menuCommandIntInsts(&g_numInstances, 1, kMaxInstances);
	Framework::MenuCommandInt menuCommandIntRate0(&g_stepRate0, 1, kMaxInstances);
	const Framework::MenuItem menuItem[] =
	{
		{{"Instances", "Number of instances (i.e: number of spheres in this sample)"}, &menuCommandIntInsts },
		{{"Rate0", "(Here: maximum number of objects per row)"}, &menuCommandIntRate0 },
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.m_config.m_lightingIsMeaningful = false;

	framework.initialize( "Instancing", 
		"Sample code to demonstrate rendering a torus with instancing hardware.",
		"This sample program displays many instances of a torus with per-pixel albedo and normal. "
		"It is meant to demonstrate the functionality of instancing hardware.");

	framework.appendMenuItems(kMenuItems, menuItem);

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Constants *m_constants;
		Vector3Unaligned *m_perInstance; //[kMaxInstances];
		Vector3Unaligned *m_perInstanceRate0; //[kMaxInstances];
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Constant Buffer",buffer);
		framework.m_allocators.allocate((void**)&frame->m_perInstance,SCE_KERNEL_WB_ONION,sizeof(*frame->m_perInstance) * kMaxInstances,4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Per Instance Buffer",buffer);
		framework.m_allocators.allocate((void**)&frame->m_perInstanceRate0,SCE_KERNEL_WB_ONION,sizeof(*frame->m_perInstanceRate0) * kMaxInstances,4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Per Instance Rate 0 Buffer",buffer);
		memset(frame->m_perInstance,0,sizeof(*frame->m_perInstance) * kMaxInstances);
		memset(frame->m_perInstanceRate0,0,sizeof(*frame->m_perInstanceRate0) * kMaxInstances);
	}

	int error = 0;

	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader;
	Gnm::Texture textures[2];

	const Gnm::FetchShaderInstancingMode instancingData[] =
	{
		Gnm::kFetchShaderUseVertexIndex,			 // Position
		Gnm::kFetchShaderUseVertexIndex,			 // Normal
		Gnm::kFetchShaderUseVertexIndex,			 // Tangent
		Gnm::kFetchShaderUseVertexIndex,			 // TextureUV
		Gnm::kFetchShaderUseInstanceId,				 // InstancePos
		Gnm::kFetchShaderUseInstanceIdOverStepRate0, // Rate0Pos
	};
	uint32_t const numElementsInInstancingData = sizeof(instancingData)/sizeof(instancingData[0]);

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/instancing-sample/shader_vv.sb"), &framework.m_allocators, instancingData, numElementsInInstancingData);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/instancing-sample/shader_p.sb"), &framework.m_allocators);
	error = Framework::loadTextureFromGnf(&textures[0], Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&textures[1], Framework::ReadFile("/app0/assets/icelogo-normal.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture will never be bound as an RWTexture, so it's safe to declare read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture will never be bound as an RWTexture, so it's safe to declare read-only.

	Gnm::Sampler sampler;
	sampler.init();
	sampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	sampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	Framework::SimpleMesh simpleMesh;
	BuildTorusMesh(&framework.m_allocators, "Torus", &simpleMesh, 0.8f*0.2f, 0.2f*0.2f, 32, 16, 4, 1);

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		// Render the scene:

		gfxc->setDepthClearValue(0.f);

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		const uint32_t numInstances1 = (g_numInstances+g_stepRate0-1) / g_stepRate0;

		for (uint32_t iInstance = 0; iInstance < kMaxInstances; ++iInstance)
		{
			const int32_t stepX = iInstance % g_stepRate0;
			frame->m_perInstance[iInstance].x = -1.2f + 0.4f * float(stepX);
		}

		for (uint32_t iRate = 0; iRate < numInstances1; ++iRate)
		{
			frame->m_perInstanceRate0[iRate].y = -1.2f + 0.4f * float(iRate);
		}

		// Clear
		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
		Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
		gfxc->setRenderTargetMask(0xF);

		gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
		gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);
		gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);

		gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
		gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

		simpleMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs );

		Gnm::Buffer constantBuffer;
		Gnm::Buffer perInstanceBuffer;
		Gnm::Buffer perRate0Buffer;
		constantBuffer.initAsConstantBuffer(frame->m_constants,sizeof(*frame->m_constants));
		perInstanceBuffer.initAsVertexBuffer(frame->m_perInstance,Gnm::kDataFormatR32G32B32Float,sizeof(frame->m_perInstance[0]),kMaxInstances);
		perRate0Buffer.initAsVertexBuffer(frame->m_perInstanceRate0,Gnm::kDataFormatR32G32B32Float,sizeof(frame->m_perInstanceRate0[0]),kMaxInstances);
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		perInstanceBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
		perRate0Buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK

		gfxc->setVertexBuffers(Gnm::kShaderStageVs, 4, 1, &perInstanceBuffer);
		gfxc->setVertexBuffers(Gnm::kShaderStageVs, 5, 1, &perRate0Buffer);

		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);

		gfxc->setNumInstances(g_numInstances);
		gfxc->setInstanceStepRate(g_stepRate0, 1); // steprate1 is unused -> default to 1.

	    const float radians = 0; // framework.GetSecondsElapsedApparent() * 0.5f;

		const Matrix4 m = Matrix4::rotationZYX(Vector3(radians,radians,0.f));

		frame->m_constants->m_modelView = transpose(framework.m_worldToViewMatrix*m);
		frame->m_constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*m);
		frame->m_constants->m_lightPosition = framework.getLightPositionInViewSpace();
		frame->m_constants->m_lightColor = framework.getLightColor();
		frame->m_constants->m_ambientColor = framework.getAmbientColor();
		frame->m_constants->m_lightAttenuation = Vector4(1, 0, 0, 0);

		gfxc->setPrimitiveType(simpleMesh.m_primitiveType);
		gfxc->setIndexSize(simpleMesh.m_indexType);
		gfxc->drawIndex(simpleMesh.m_indexCount, simpleMesh.m_indexBuffer);

		// reset instancing
		gfxc->setNumInstances(1);
		gfxc->setInstanceStepRate(1, 1); 

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
    return 0;
}
