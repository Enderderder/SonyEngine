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
#include "comp_buildn_cbuffer.h"
using namespace sce;

namespace
{
	enum PostBumpShaderType
	{
		kSharingViaLDS,
		kNoIntDiv,
		kShareSerially2x2,
		kNoSharing,		
	};
	PostBumpShaderType shaderIndex = kNoSharing; //kSharingViaLDS;
	bool bEarlyOut = false;
	Framework::MenuItemText shaderText[4] = 
	{ 
		{"Sharing via LDS", "Compute: threads share via LDS"}, 
		{"No integer divisions", "Compute: no integer divisions"}, 
		{"Share serially 2x2", "Compute: share 2x2 (no LDS)"}, 
		{"No sharing", "Pixel: no sharing among threads"}, 
	};

	Framework::MenuCommandEnum menuCommandEnumShaderName(shaderText, sizeof(shaderText)/sizeof(shaderText[0]), &shaderIndex);
	Framework::MenuCommandBool menuCommandBoolEarlyOut(&bEarlyOut);
	const Framework::MenuItem menuItem[] =
	{
		{{"Shader", "Select among various shading regimes"}, &menuCommandEnumShaderName},
		{{"Per-Tile Early Out", "Toggle an optimization that early-outs for screen tiles that contain no graphics"}, &menuCommandBoolEarlyOut},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.m_config.m_htile = false;
	framework.m_config.m_clearColorIsMeaningful = false;

	framework.initialize("Post Bump", 
		"Sample code to illustrate how per-pixel height-mapping can be performed as a post-process step, rather than while pixel shading.",
		"This sample program renders a scene and exports per-pixel height-mapping to an offscreen buffer, which is later used "
		"to apply per-pixel lighting to the scene as a post-process.");

	framework.appendMenuItems(kMenuItems, menuItem);

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		cbMeshInstance *m_meshInstanceConstants;
		cbCompBuildNorm *m_buildNormalConstants;
		volatile uint32_t *m_label;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_meshInstanceConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_meshInstanceConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Mesh Constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_buildNormalConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_buildNormalConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Build Normal Constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_label,SCE_KERNEL_WB_ONION,sizeof(*frame->m_label),8,Gnm::kResourceTypeLabel,"Buffer %d Label",buffer);
	}

	Framework::InitNoise(&framework.m_allocators);

	int error = 0;

	Gnm::Texture heightMapTexture;
	error = Framework::loadTextureFromGnf(&heightMapTexture, Framework::ReadFile("/app0/assets/crown/heightBC4.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	heightMapTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it can be declared as read-only.

	Gnm::Sampler linearSampler;
	linearSampler.init();
	linearSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	linearSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	struct BuildNormalShaders
	{
		Framework::CsShader m_compute;
		Framework::CsShader m_computeNoIntDiv; // optimized for no integer divisions, which can be slow
		Framework::CsShader m_computeNoLDS;    // optimized for no LDS, which has higher latency than GPRS
		Framework::PsShader m_pixel;
	};
	BuildNormalShaders buildNormalShaders[2];

	error = Framework::LoadCsShader(&buildNormalShaders[0].m_compute, Framework::ReadFile("/app0/post-bump-sample/buildnormal_c.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&buildNormalShaders[0].m_computeNoIntDiv, Framework::ReadFile("/app0/post-bump-sample/buildnormal_noidiv_c.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&buildNormalShaders[0].m_computeNoLDS, Framework::ReadFile("/app0/post-bump-sample/buildnormal_nolds_c.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&buildNormalShaders[0].m_pixel, Framework::ReadFile("/app0/post-bump-sample/buildnormal_p.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&buildNormalShaders[1].m_compute, Framework::ReadFile("/app0/post-bump-sample/buildnormal_earlyo_c.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&buildNormalShaders[1].m_computeNoIntDiv, Framework::ReadFile("/app0/post-bump-sample/buildnormal_noidiv_earlyo_c.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&buildNormalShaders[1].m_computeNoLDS, Framework::ReadFile("/app0/post-bump-sample/buildnormal_nolds_earlyo_c.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&buildNormalShaders[1].m_pixel, Framework::ReadFile("/app0/post-bump-sample/buildnormal_earlyo_p.sb"), &framework.m_allocators);

	void *fetchShader;
	Gnmx::VsShader *vertexShader = Framework::LoadVsMakeFetchShader(&fetchShader, "/app0/post-bump-sample/shader_vv.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);

	Gnmx::VsShader *copyVertexShader = Framework::LoadVsShader("/app0/post-bump-sample/basic_cpy_vv.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache copyVertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&copyVertexShader_offsetsTable, Gnm::kShaderStageVs, copyVertexShader);

	Gnmx::PsShader *pixelShader = Framework::LoadPsShader("/app0/post-bump-sample/shader_p.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache pixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&pixelShader_offsetsTable, Gnm::kShaderStagePs, pixelShader);

	Gnm::RenderTarget myRenderTarget;

	int iW = framework.m_buffer[0].m_renderTarget.getWidth(), iH = framework.m_buffer[0].m_renderTarget.getHeight();
	Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8Unorm;
	Gnm::TileMode rtTileMode;
	GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &rtTileMode, GpuAddress::kSurfaceTypeColorTarget, format, 1);
	Gnm::SizeAlign rtSizeAlign = Gnmx::Toolkit::init(&myRenderTarget, iW, iH, 1, format, rtTileMode, Gnm::kNumSamples1, Gnm::kNumFragments1, NULL, NULL);
	void *renderTargetBaseAddress;
	framework.m_allocators.allocate(&renderTargetBaseAddress, SCE_KERNEL_WC_GARLIC, rtSizeAlign, Gnm::kResourceTypeRenderTargetBaseAddress, "Render Target");
	myRenderTarget.setAddresses(renderTargetBaseAddress, nullptr, nullptr);

	Gnm::Texture renderTargetTexture;
	renderTargetTexture.initFromRenderTarget(&myRenderTarget, false);
	renderTargetTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.

	Framework::SimpleMesh crownMesh;
	LoadSimpleMesh(&framework.m_allocators, &crownMesh, "/app0/assets/crown/crown.mesh");

	while(!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		Gnm::Texture depthTargetTexture;
		depthTargetTexture.initFromDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget, false);
		depthTargetTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.

		gfxc->pushMarker("set default registers");
		gfxc->setDepthClearValue(0.f);

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);
		gfxc->popMarker();

		*frame->m_label = 0;

		gfxc->pushMarker("clear screen");
			Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &myRenderTarget, Vector4(0.f,0.f,0.f,0.f));
			Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
			gfxc->setRenderTargetMask(0xF);

			gfxc->setRenderTarget(0, &myRenderTarget);
			gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

			gfxc->setupScreenViewport(0, 0, myRenderTarget.getWidth(), myRenderTarget.getHeight(), 0.5f, 0.5f);
			Gnm::DepthStencilControl dsc;
			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
			dsc.setDepthEnable(true);
			gfxc->setDepthStencilControl(dsc);
		gfxc->popMarker();

		gfxc->pushMarker("prepare shader inputs");
			gfxc->setVsShader(vertexShader, 0, fetchShader, &vertexShader_offsetsTable);
			crownMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);
			gfxc->setPsShader(pixelShader, &pixelShader_offsetsTable);

			Gnm::Buffer meshInstanceConstantBuffer;
			meshInstanceConstantBuffer.initAsConstantBuffer(frame->m_meshInstanceConstants,sizeof(*frame->m_meshInstanceConstants));
			meshInstanceConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &meshInstanceConstantBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &meshInstanceConstantBuffer);

			Framework::SetNoiseDataBuffers(*gfxc, Gnm::kShaderStagePs);

			gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &heightMapTexture);
			gfxc->setSamplers(Gnm::kShaderStagePs, 2, 1, &linearSampler);

			const float fShaderTime = framework.GetSecondsElapsedApparent();

			Matrix4 m44LocalToWorld;
			static float ax=0, ay=-2*0.33f, az=0;
			ay = fShaderTime*0.25f*2;
			m44LocalToWorld = Matrix4::rotationZYX(Vector3(ax,ay,az));
			m44LocalToWorld.setCol3(Vector4(0,-1,-10,1));

			const Matrix4 m44WorldToCam = framework.m_worldToViewMatrix;
			const Matrix4 m44LocToCam = m44WorldToCam * m44LocalToWorld;
			const Matrix4 m44CamToLoc = inverse(m44LocToCam);
			const Matrix4 m44MVP = framework.m_projectionMatrix * m44LocToCam;
			const Matrix4 m44WorldToLocal = inverse(m44LocalToWorld);

			const Vector4 vLpos_world(30,30,50,1);
			const Vector4 vLpos2_world(50,50,-60,1);
			frame->m_meshInstanceConstants->g_vCamPos = m44CamToLoc * Vector4(0,0,0,1);
			frame->m_meshInstanceConstants->g_vLightPos = m44WorldToLocal * vLpos_world;
			frame->m_meshInstanceConstants->g_vLightPos2 = m44WorldToLocal * vLpos2_world;
			frame->m_meshInstanceConstants->g_mLocalToView = transpose(m44LocToCam);
			frame->m_meshInstanceConstants->g_mWorldViewProjection = transpose(m44MVP);
		gfxc->popMarker();

		gfxc->pushMarker("draw call");
			gfxc->setPrimitiveType(crownMesh.m_primitiveType);
			gfxc->setIndexSize(crownMesh.m_indexType);
			gfxc->drawIndex(crownMesh.m_indexCount, crownMesh.m_indexBuffer);
			gfxc->writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)frame->m_label, 1, Gnm::kCacheActionNone);
		gfxc->popMarker();

		gfxc->waitOnAddress((void*)frame->m_label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);

		Matrix4 post_adjust = Matrix4::identity();
		for(int q=0; q<3; q++)
		{
			const int iS = q==0 ? iW : (q==1 ? iH : 1);
			post_adjust.setRow(q, Vector4(iS*(q==0?0.5f:0), iS*(q==1?-0.5f:0), q==2?0.5f:0, iS*0.5f) );
		}
		Matrix4 m44InvProj = inverse(post_adjust*framework.m_projectionMatrix);
		frame->m_meshInstanceConstants->g_mInvScrProjection = transpose(m44InvProj);

		if(shaderIndex == kNoSharing)
		{
			gfxc->pushMarker("prepare post-process inputs");
				gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
				gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

				Gnm::DepthStencilControl dsc;
				dsc.init();
				dsc.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways);
				dsc.setDepthEnable(true);
				gfxc->setDepthStencilControl(dsc);

				gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &renderTargetTexture);
				gfxc->setTextures(Gnm::kShaderStagePs, 3, 1, &depthTargetTexture);
				gfxc->setSamplers(Gnm::kShaderStagePs, 2, 1, &linearSampler);

				gfxc->setVsShader(copyVertexShader, 0, (void*)0, &copyVertexShader_offsetsTable);

				const int eo = bEarlyOut ? 1 : 0;
				gfxc->setPsShader(buildNormalShaders[eo].m_pixel.m_shader, &buildNormalShaders[eo].m_pixel.m_offsetsTable);
			gfxc->popMarker();

			gfxc->pushMarker("perform post-process");
				gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &renderTargetTexture);
				gfxc->setTextures(Gnm::kShaderStagePs, 3, 1, &depthTargetTexture);
				gfxc->setSamplers(Gnm::kShaderStagePs, 2, 1, &linearSampler);
				gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &meshInstanceConstantBuffer);
				gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &meshInstanceConstantBuffer);

				gfxc->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
				gfxc->drawIndexAuto(3);
				dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
				gfxc->setDepthStencilControl(dsc);
				gfxc->writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)frame->m_label, 2, Gnm::kCacheActionNone);
			gfxc->popMarker();
		}
		else // compute versions
		{
			gfxc->pushMarker("prepare post-process inputs");
				const int eo = bEarlyOut ? 1 : 0;
				Framework::CsShader * computeShader = shaderIndex==kShareSerially2x2 ? &buildNormalShaders[eo].m_computeNoLDS : (shaderIndex==kNoIntDiv ? &buildNormalShaders[eo].m_computeNoIntDiv : &buildNormalShaders[eo].m_compute);

				gfxc->setCsShader(computeShader->m_shader, &computeShader->m_offsetsTable);

				Gnm::Texture writableRenderTargetTexture;
				writableRenderTargetTexture.initFromRenderTarget(&bufferCpuIsWriting->m_renderTarget, false);
				writableRenderTargetTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // this texture will be bound as an RWTexture, and different CUs may hit different cache lines, so GPU coherent it is.
				gfxc->setRwTextures(Gnm::kShaderStageCs, 1, 1, &writableRenderTargetTexture);

				frame->m_buildNormalConstants->g_mInvProj = transpose(m44InvProj);

				Gnm::Buffer buildNormalConstantBuffer;
				buildNormalConstantBuffer.initAsConstantBuffer(frame->m_buildNormalConstants,sizeof(*frame->m_buildNormalConstants));
				buildNormalConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
				gfxc->setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &buildNormalConstantBuffer);

				gfxc->setTextures(Gnm::kShaderStageCs, 2, 1, &renderTargetTexture);
				gfxc->setTextures(Gnm::kShaderStageCs, 3, 1, &depthTargetTexture);
				
				
			gfxc->popMarker();

			gfxc->pushMarker("perform post-process");
				if(shaderIndex == kNoIntDiv)
				{
					gfxc->dispatch((iW+TILEX_NOIDIV-1)/TILEX_NOIDIV, (iH+TILEY_NOIDIV-1)/TILEY_NOIDIV, 1);
				}
				else
				{
					gfxc->dispatch((iW+TILEX-1)/TILEX, (iH+TILEY-1)/TILEY, 1);
				}
				gfxc->writeAtEndOfShader(Gnm::kEosCsDone, (void*)frame->m_label, 2);
			gfxc->popMarker();
		}
		gfxc->waitOnAddress((void*)frame->m_label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 2);
		Framework::waitForGraphicsWritesToRenderTarget(gfxc, &bufferCpuIsWriting->m_renderTarget, Gnm::kWaitTargetSlotCb0);
		gfxc->setDepthStencilDisable();
		gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
		gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);
		gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
    return 0;
}
