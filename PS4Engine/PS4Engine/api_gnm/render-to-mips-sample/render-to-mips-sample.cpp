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

Gnm::Texture mipTexture1, mipTexture2, srcTexture;
Gnm::Sampler sampler;
namespace
{
	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader;
	Framework::SimpleMesh cubeMesh;

	const uint32_t kMipCount = 10;

	// names of the two textures that can be displayed at runtime
	Framework::MenuItemText displayTextureText[3] = {
		{"Tinted (PS)", "Tinted in render-to-mip shader (PS shader output)"},
		{"Tinted (RW_Tex)", "Tinted in render-to-mip shader (RW_Texture output)"},
		{"Original", "Original texture, no tint"},
	}; 

	class RuntimeOptions
	{
	public:
		int32_t m_mipLevel;
		uint32_t m_displayTex;
	};
	RuntimeOptions g_runtimeOptions = { 0, 0 };

	Framework::MenuCommandInt mipLevel(&g_runtimeOptions.m_mipLevel, 0, kMipCount-1);
	Framework::MenuCommandEnum displayTexture(displayTextureText, sizeof(displayTextureText)/sizeof(displayTextureText[0]), &g_runtimeOptions.m_displayTex);
	const Framework::MenuItem menuItem[] =
	{
		{{"Mip level", "The mip level to display"}, &mipLevel},
		{{"Texture", "The texture to display"}, &displayTexture},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };

	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);
	framework.initialize( "Render To Mips", 
		"Sample code to demonstrate rendering to the mip levels of a texture.",
		"This sample program displays a cube whose texture's mip levels were generated at runtime, by copying (and tinting) a source texture.");
	framework.appendMenuItems(kMenuItems, menuItem);

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		cbMeshShaderConstants* m_constants;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Constant Buffer",buffer);
	}

	int error = 0;

	error = Framework::loadTextureFromGnf(&srcTexture, Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	// Point-sample the textures so we can see each mip in isolation
	sampler.init();
	sampler.setMipFilterMode(Gnm::kMipFilterModePoint);
	sampler.setXyFilterMode(Gnm::kFilterModePoint, Gnm::kFilterModePoint);

	Gnm::SizeAlign mipSA = Gnmx::Toolkit::initAs2d(&mipTexture1, 512, 512, kMipCount, Gnm::kDataFormatR8G8B8A8Unorm, Gnm::kTileModeThin_1dThin, Gnm::kNumFragments1);
	mipTexture1.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we alias this texture as a RenderTarget, so GPU coherent it is.
	uint32_t *mipPixels1;
	framework.m_allocators.allocate((void**)&mipPixels1, SCE_KERNEL_WC_GARLIC, mipSA, Gnm::kResourceTypeTextureBaseAddress, "Mip Texture 1");
	memset(mipPixels1, 0xFF, mipSA.m_size);
	mipTexture1.setBaseAddress(mipPixels1);

	mipSA                = Gnmx::Toolkit::initAs2d(&mipTexture2, 512, 512, kMipCount, Gnm::kDataFormatR8G8B8A8Unorm, Gnm::kTileModeThin_1dThin, Gnm::kNumFragments1);
	mipTexture2.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we bind this texture as an RW_Texture, so GPU coherent it is.
	uint32_t *mipPixels2;
	framework.m_allocators.allocate((void**)&mipPixels2, SCE_KERNEL_WC_GARLIC, mipSA, Gnm::kResourceTypeTextureBaseAddress, "Mip Texture 2");
	memset(mipPixels2, 0xFF, mipSA.m_size);
	mipTexture2.setBaseAddress(mipPixels2);

	// Generate mips of mipTexture1 by generating a RenderTarget aliased to each mip level of mipTexture1,
	// and rendering the mip image using a full-"screen" pixel shader.
	// We also generate a second mipTexture2 by writing to an RW_Texture in the PS shader. This demonstrates how mipmap data
	// could be written from any shader stage.
	{
		Framework::VsShader fsVs;
		Framework::PsShader fsPs;

		// Load mip-rendering shader. In this case, it's a simple shader that downscales a source texture to the destination
		// texture and applies a tint color.
		error = Framework::LoadVsShader(&fsVs, Framework::ReadFile("/app0/render-to-mips-sample/fs_vv.sb"), &framework.m_allocators);
		error = Framework::LoadPsShader(&fsPs, Framework::ReadFile("/app0/render-to-mips-sample/fs_p.sb"), &framework.m_allocators);

		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		gfxc->setRenderTargetMask(0xF);
		gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);

		gfxc->setDepthRenderTarget(NULL);
		gfxc->setDepthStencilDisable();

		gfxc->setVsShader(fsVs.m_shader, 0, (void*)0, &fsVs.m_offsetsTable);
		gfxc->setPsShader(fsPs.m_shader, &fsPs.m_offsetsTable);

		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);
		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &srcTexture);
		gfxc->setRwTextures(Gnm::kShaderStagePs, 0, 1, &mipTexture2);

		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
		
		// Add a per-mip tint color, for some variety
		static const Vector4Unaligned mipTintColors[kMipCount] =
		{
			{1.0f, 1.0f, 1.0f, 1.0f}, // 0 = white
			{1.0f, 1.0f, 0.6f, 1.0f}, // 1 = yellow
			{1.0f, 0.6f, 1.0f, 1.0f}, // 2 = magenta
			{0.6f, 1.0f, 1.0f, 1.0f}, // 3 = cyan
			{1.0f, 0.6f, 0.6f, 1.0f}, // 4 = red
			{0.6f, 0.6f, 1.0f, 1.0f}, // 5 = blue
			{0.6f, 1.0f, 0.6f, 1.0f}, // 6 = green
			{1.0f, 1.0f, 0.8f, 1.0f}, // 7 = light yellow
			{1.0f, 0.8f, 1.0f, 1.0f}, // 8 = light magenta
			{0.8f, 1.0f, 1.0f, 1.0f}, // 9 = light cyan
		};

		for(uint32_t iMip=0; iMip<kMipCount; ++iMip)
		{
			Gnm::RenderTarget mipTarget;
			int32_t status = mipTarget.initFromTexture(&mipTexture1, iMip, (Gnm::NumSamples)mipTexture1.getNumFragments(), NULL, NULL);
			// If mipTexture1 were a texture array or cubemap, we could use mipTarget.setArrayView() to choose an array slice to render to.
			SCE_GNM_ASSERT_MSG(status == 0, "initFromTexture failed.");
#ifndef _DEBUG
			SCE_GNM_UNUSED(status);
#endif //!_DEBUG
			gfxc->setRenderTarget(0, &mipTarget);

			gfxc->setupScreenViewport(0, 0, mipTarget.getWidth(), mipTarget.getHeight(), 0.5f, 0.5f);

			cbMipShaderConstants* mipConst = static_cast<cbMipShaderConstants*>( gfxc->allocateFromCommandBuffer( sizeof(cbMipShaderConstants), Gnm::kEmbeddedDataAlignment4 ) );
			mipConst->m_tintColor = mipTintColors[iMip];
			mipConst->m_mipLevel  = iMip;
			Gnm::Buffer mipConstBuffer;
			mipConstBuffer.initAsConstantBuffer(mipConst, sizeof(cbMipShaderConstants));
			mipConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
			gfxc->setConstantBuffers( Gnm::kShaderStagePs, 0, 1, &mipConstBuffer );

			gfxc->drawIndexAuto(3);
		}
		gfxc->submit();
	}
	// If we were using the mipTextures in the same command buffer submission, we would need to stall and flush caches here.
	// But as it is, that's handled by the initializeDefaultHardwareState() invoked automatically by the graphics driver.

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/render-to-mips-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/render-to-mips-sample/shader_p.sb"), &framework.m_allocators);

	BuildCubeMesh(&framework.m_allocators, "Cube", &cubeMesh, 1.0f);

	//
	// Main Loop:
	//	
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

		gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
		gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

		cubeMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		//uint32_t mipLevelAsU4_8 = Gnmx::convertF32ToU4_8(static_cast<float>((g_runtimeOptions.m_displayTex == 0) ? g_runtimeOptions.m_mipLevel : g_runtimeOptions.m_mipLevel+1));
		uint32_t mipLevelAsU4_8 = Gnmx::convertF32ToU4_8(static_cast<float>(g_runtimeOptions.m_mipLevel));
		sampler.setLodRange(mipLevelAsU4_8, mipLevelAsU4_8);
		Gnm::Texture *colorTex = NULL;
		switch(g_runtimeOptions.m_displayTex)
		{
		case 0: colorTex = &mipTexture1; break;
		case 1: colorTex = &mipTexture2; break;
		case 2: colorTex = &srcTexture;  break;
		default: SCE_GNM_ERROR("Unhandled display texture ID: %d", g_runtimeOptions.m_displayTex); break;
		}
		gfxc->setTextures( Gnm::kShaderStagePs, 0, 1, colorTex );
		gfxc->setSamplers( Gnm::kShaderStagePs, 0, 1, &sampler );

		const float radians = 0.3f * framework.GetSecondsElapsedApparent();
		const Matrix4 modelMatrix = Matrix4::rotationZYX(Vector3(radians,radians,0.f)) * Matrix4::scale(Vector3(1.1f));
		frame->m_constants->m_modelView = transpose(framework.m_worldToViewMatrix*modelMatrix);
		frame->m_constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*modelMatrix);
		frame->m_constants->m_lightPosition = framework.getLightPositionInViewSpace().getXYZ();
		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(frame->m_constants,sizeof(*frame->m_constants));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

		gfxc->setPrimitiveType(cubeMesh.m_primitiveType);
		gfxc->setIndexSize(cubeMesh.m_indexType);
		gfxc->drawIndex(cubeMesh.m_indexCount, cubeMesh.m_indexBuffer);
		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
    return 0;
}
