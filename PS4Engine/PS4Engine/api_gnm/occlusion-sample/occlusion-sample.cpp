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
}

int main(int argc, const char *argv[])
{
	framework.m_config.m_lightPositionX = Vector3(0,5,5);
	const Vector3 ideal(1.f, 0.f, 0.f);
	const Vector3 forward = normalize(framework.m_config.m_lightTargetX - framework.m_config.m_lightPositionX);
	framework.m_config.m_lightUpX = normalize(ideal - forward * dot(forward, ideal));

	framework.processCommandLine(argc, argv);

	framework.m_config.m_lightingIsMeaningful = true;

	framework.initialize("Occlusion", 
		"Sample code for occlusion culling hardware.", 
		"This sample renders a sphere, and then renders a torus behind the sphere. "
		"Occlusion culling hardware is used to determine when the torus is completely occluded by the sphere. "
		"When the torus is completely occluded, a message is displayed to indicate this.");

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Constants *m_sphereConstants;
		Constants *m_torusConstants;
		Gnm::OcclusionQueryResults *m_queryResults;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_sphereConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_sphereConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Sphere Constant Buffer",buffer);
		framework.m_allocators.allocate((void**)&frame->m_torusConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_torusConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Torus Constant Buffer",buffer);
		framework.m_allocators.allocate((void**)&frame->m_queryResults,SCE_KERNEL_WC_GARLIC,sizeof(Gnm::OcclusionQueryResults),16,Gnm::kResourceTypeGenericBuffer,"Buffer %d Occlusion Query Results",buffer);
	}

	int error = 0;
	Framework::VsShader vertexShader; 
	Framework::PsShader pixelShader; 
	Gnm::Texture textures[2];

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/occlusion-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/occlusion-sample/shader_p.sb"), &framework.m_allocators);
	error = Framework::loadTextureFromGnf(&textures[0], Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&textures[1], Framework::ReadFile("/app0/assets/icelogo-normal.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never bind this as RWTexture, so it's safe to mark it as read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never bind this as RWTexture, so it's safe to mark it as read-only.

	Gnm::Sampler samplers[2];
	for(int s=0; s<2; s++)
	{
		samplers[s].init();
		samplers[s].setMipFilterMode(Gnm::kMipFilterModeLinear);
		samplers[s].setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
	}

	Framework::SimpleMesh sphereMesh, torusMesh;
	BuildSphereMesh(&framework.m_allocators, "Sphere", &sphereMesh, 1.0f, 64, 32);
	BuildTorusMesh(&framework.m_allocators, "Torus", &torusMesh, 0.8f, 0.2f, 64, 32, 4, 1);

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		frame->m_queryResults->reset();

		gfxc->setDepthClearValue(0.f);

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

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

		gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 2, samplers);

	    const float angle = framework.GetSecondsElapsedApparent();

		// Draw the foreground (occluder) sphere
		{
			gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
			sphereMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

			Gnm::Buffer sphereConstantBuffer;
			sphereConstantBuffer.initAsConstantBuffer(frame->m_sphereConstants,sizeof(*frame->m_sphereConstants));
			sphereConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &sphereConstantBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &sphereConstantBuffer);

			const Matrix4 scale = Matrix4::identity();
			const Matrix4 rot = Matrix4::rotationZYX(Vector3(angle,angle,0.f));
			const Matrix4 m = scale * rot;

			frame->m_sphereConstants->m_lightPosition    = framework.getLightPositionInViewSpace();
			frame->m_sphereConstants->m_lightColor       = framework.getLightColor();
			frame->m_sphereConstants->m_ambientColor     = framework.getAmbientColor();
			frame->m_sphereConstants->m_lightAttenuation = Vector4(1,0,0,0);
			frame->m_sphereConstants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix * m);
			frame->m_sphereConstants->m_modelView = transpose(framework.m_worldToViewMatrix * m);

			gfxc->setPrimitiveType(sphereMesh.m_primitiveType);
			gfxc->setIndexSize(sphereMesh.m_indexType);
			gfxc->drawIndex(sphereMesh.m_indexCount, sphereMesh.m_indexBuffer);
		}

		// Draw the background (occluded) torus, oscillating back and forth on the X axis
		{
			gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
			torusMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

			Gnm::Buffer torusConstantBuffer;
			torusConstantBuffer.initAsConstantBuffer(frame->m_torusConstants,sizeof(*frame->m_torusConstants));
			torusConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &torusConstantBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &torusConstantBuffer);

			const Matrix4 scale = Matrix4::scale( Vector3(0.5f,0.5f,0.5f) );
			const Matrix4 trans = Matrix4::translation( Vector3(4.0f*sinf(framework.GetSecondsElapsedApparent()), 0, -3.0f) );
			const Matrix4 m = scale * trans;

			frame->m_torusConstants->m_lightPosition    = framework.getLightPositionInViewSpace();
			frame->m_torusConstants->m_lightColor       = framework.getLightColor();
			frame->m_torusConstants->m_ambientColor     = framework.getAmbientColor();
			frame->m_torusConstants->m_lightAttenuation = Vector4(1,0,0,0);
			frame->m_torusConstants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*m);
			frame->m_torusConstants->m_modelView = transpose(framework.m_worldToViewMatrix*m);

			gfxc->writeOcclusionQuery(Gnm::kOcclusionQueryOpBeginWithoutClear, frame->m_queryResults);
			gfxc->setDbCountControl(Gnm::kDbCountControlPerfectZPassCountsEnable, 0);

			gfxc->setPrimitiveType(torusMesh.m_primitiveType);
			gfxc->setIndexSize(torusMesh.m_indexType);
			gfxc->drawIndex(torusMesh.m_indexCount, torusMesh.m_indexBuffer);
			gfxc->writeOcclusionQuery(Gnm::kOcclusionQueryOpEnd, frame->m_queryResults);
			gfxc->setDbCountControl(Gnm::kDbCountControlPerfectZPassCountsDisable, 0);
		}

		// render text only if the torus didn't pass any Z tests
		{
			gfxc->setZPassPredicationEnable(frame->m_queryResults, Gnm::kPredicationZPassHintWait, Gnm::kPredicationZPassActionDrawIfNotVisible);
			const DbgFont::Cell cell = {0, DbgFont::kWhite, 15, DbgFont::kRed, 7};
			bufferCpuIsWriting->m_window.deferredPrintf(*gfxc, 0, bufferCpuIsWriting->m_window.m_heightInCharacters-1, cell, 64, "The torus didn't pass any Z tests.");
			gfxc->setZPassPredicationDisable();
		}

		// render text only if the torus passed some Z tests
		{
			gfxc->setZPassPredicationEnable(frame->m_queryResults, Gnm::kPredicationZPassHintWait, Gnm::kPredicationZPassActionDrawIfVisible);
			const DbgFont::Cell cell = {0, DbgFont::kWhite, 15, DbgFont::kGreen, 7};
			bufferCpuIsWriting->m_window.deferredPrintf(*gfxc, 0, bufferCpuIsWriting->m_window.m_heightInCharacters-1, cell, 64, "The torus passed 000000 Z tests.");

			framework.displayTimer(*gfxc, 17, bufferCpuIsWriting->m_window.m_heightInCharacters-1, frame->m_queryResults, 6, 6, 1);

			gfxc->setZPassPredicationDisable();
		}

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
    return 0;
}
