/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/noise_prep.h"
#include "../framework/frame.h"
#include "std_cbuffer.h"
using namespace sce;

namespace
{
	uint32_t iMaterialIndex = 0;
	Framework::MenuItemText materialIndexText[5] = {
		{"Clay", "Low frequency 3D noise"},
		{"Wood", "Noisy banded gradient"},
		{"Marble", "Noisy luma ramp gradient"},
		{"Liquid", "Noisy luma ramp gradient with liquid"},
		{"Triplanar", "A height map for each primary axis"},
	};

	Framework::MenuCommandEnum menuCommandMaterialIndex(materialIndexText, sizeof(materialIndexText)/sizeof(materialIndexText[0]), &iMaterialIndex);
	const Framework::MenuItem menuItem[] =
	{
		{{"Material", "Choose among various procedural materials"}, &menuCommandMaterialIndex},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.initialize( "Procedural", 
		"A variety of useful procedural shading techniques are illustrated.",
		"This sample program renders an object with one of several procedural shaders, including "
		"wood and marble.");

	framework.appendMenuItems(kMenuItems, menuItem);

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		cbMeshInstance *m_constants;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Constant Buffer",buffer);
	}

	Framework::InitNoise(&framework.m_allocators);

	int error = 0;

	Gnm::Texture marbleColorTexture, lavaTexture;
	Gnm::Texture genericPatternTexture[2];

	error = Framework::loadTextureFromGnf(&marbleColorTexture, Framework::ReadFile("/app0/assets/pal_marble.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&lavaTexture, Framework::ReadFile("/app0/assets/pal_lava.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&genericPatternTexture[0], Framework::ReadFile("/app0/assets/gen_pattern0.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&genericPatternTexture[1], Framework::ReadFile("/app0/assets/gen_pattern3.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	marbleColorTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.
	lavaTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.
	genericPatternTexture[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.
	genericPatternTexture[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.

	Gnm::Sampler noMipMapSampler;
	noMipMapSampler.init();
	noMipMapSampler.setMipFilterMode(Gnm::kMipFilterModeNone);
	noMipMapSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
	noMipMapSampler.setLodRange(0, 0);

	Gnm::Sampler trilinearSampler;
	trilinearSampler.init();
	trilinearSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	trilinearSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	Framework::PsShader mainPixelShader, woodPixelShader, marblePixelShader, liquidPixelShader, triplanarPixelShader;
	Framework::VsShader vertexShader;

	error = Framework::LoadPsShader(&mainPixelShader, Framework::ReadFile("/app0/procedural-sample/shader_clay_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&woodPixelShader, Framework::ReadFile("/app0/procedural-sample/shader_wood_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&marblePixelShader, Framework::ReadFile("/app0/procedural-sample/shader_marble_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&liquidPixelShader, Framework::ReadFile("/app0/procedural-sample/shader_liquid_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&triplanarPixelShader, Framework::ReadFile("/app0/procedural-sample/shader_triplanar_p.sb"), &framework.m_allocators);
	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/procedural-sample/shader_vv.sb"), &framework.m_allocators);

	Framework::PsShader *pixelShader[] ={&mainPixelShader,&woodPixelShader,&marblePixelShader,&liquidPixelShader,&triplanarPixelShader};

	Framework::SimpleMesh isoSurface;
	LoadSimpleMesh(&framework.m_allocators, &isoSurface, "/app0/assets/isosurf.mesh");
	Framework::scaleSimpleMesh(&isoSurface, 0.15f);

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		gfxc->pushMarker("set default registers");
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);
		gfxc->popMarker();

		gfxc->pushMarker("clear screen");

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

		gfxc->popMarker();

		gfxc->pushMarker("prepare shader inputs");

		gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
		gfxc->setPsShader(pixelShader[iMaterialIndex]->m_shader, &pixelShader[iMaterialIndex]->m_offsetsTable);
		isoSurface.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(frame->m_constants,sizeof(*frame->m_constants));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

		Framework::SetNoiseDataBuffers(*gfxc, Gnm::kShaderStagePs);

		Matrix4 m44LocalToWorld = Matrix4::scale(Vector3(1.5f));
		static float ax=0, ay=-2*0.33f, az=0;

		const float fShaderTime = framework.GetSecondsElapsedApparent();

		ay =  fShaderTime*0.2f*600*0.005f;
		ax = -fShaderTime*0.11f*600*0.0045f;
		m44LocalToWorld *= Matrix4::rotationZYX(Vector3(ax,ay,az));

		const Matrix4 m44WorldToCam = framework.m_worldToViewMatrix;
		const Matrix4 m44LocToCam = m44WorldToCam * m44LocalToWorld;
		const Matrix4 m44CamToLoc = inverse(m44LocToCam);
		const Matrix4 m44MVP = framework.m_projectionMatrix * m44LocToCam;
		const Matrix4 m44WorldToLocal = inverse(m44LocalToWorld);

		const Vector4 vLpos_world(30,30,50,1);
		const Vector4 vLpos2_world(50,50,-60,1);
		frame->m_constants->g_vCamPos = m44CamToLoc * Vector4(0,0,0,1);
		frame->m_constants->g_vLightPos = m44WorldToLocal * vLpos_world;
		frame->m_constants->g_vLightPos2 = m44WorldToLocal * vLpos2_world;
		frame->m_constants->g_mWorld = transpose(m44LocalToWorld);
		frame->m_constants->g_mWorldViewProjection = transpose(m44MVP);
		frame->m_constants->g_fTime = fShaderTime;

		// set marble colors
		if(iMaterialIndex>=2)
		{
			if(iMaterialIndex==4)
			{
				gfxc->setTextures(Gnm::kShaderStagePs, 2, 2, genericPatternTexture);
				gfxc->setSamplers(Gnm::kShaderStagePs, 2, 1, &trilinearSampler);
			}
			else
			{
				if(iMaterialIndex==2) gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &marbleColorTexture);
				else gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &lavaTexture);
				gfxc->setSamplers(Gnm::kShaderStagePs, 2, 1, &noMipMapSampler);
			}
		}

		gfxc->popMarker();

		gfxc->pushMarker("draw call");
		gfxc->setPrimitiveType(isoSurface.m_primitiveType);
		gfxc->setIndexSize(isoSurface.m_indexType);
		gfxc->drawIndex(isoSurface.m_indexCount, isoSurface.m_indexBuffer);
		gfxc->popMarker();

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
    return 0;
}
