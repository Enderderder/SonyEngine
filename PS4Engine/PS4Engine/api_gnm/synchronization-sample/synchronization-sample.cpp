/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/frame.h"
using namespace sce;

namespace
{
	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader;
	Framework::CsShader computeShader[2];
	Gnm::Texture texture;
	Gnm::Sampler sampler;
	Gnm::RenderTarget backBufferRenderTarget;
	Gnm::Texture backBufferTexture;
	Framework::SimpleMesh smallCubeMesh;
	Framework::SimpleMesh cubeMesh;
	uint32_t computeGraphics = 1;
	uint32_t graphicsGraphics = 1;
	uint32_t graphicsCompute = 1;
	const uint32_t numSlices = 1;

	struct ShaderConstants
	{
		Matrix4Unaligned mvp;
	};

	Framework::MenuItemText computeGraphicsText[2] = {
		{"Write, NO STALL, render", "Write, NO STALL, render"},
		{"Write, STALL, render", "Write, STALL, render"},
	};
	Framework::MenuItemText graphicsGraphicsText[2] = {
		{"Render, NO STALL, fetch", "Render, NO STALL, fetch"},
		{"Render, STALL, fetch", "Render, STALL, fetch"},
	};
	Framework::MenuItemText graphicsComputeText[2] = {
		{"Render, NO STALL, fetch", "Render, NO STALL, fetch"},
		{"Render, STALL, fetch", "Render, STALL, fetch"},
	};

	Framework::MenuCommandEnum menuComputeGraphics(computeGraphicsText, 2, &computeGraphics);
	Framework::MenuCommandEnum menuGraphicsGraphics(graphicsGraphicsText, 2, &graphicsGraphics);
	Framework::MenuCommandEnum menuGraphicsCompute(graphicsComputeText, 2, &graphicsCompute);
	const Framework::MenuItem menuItem[] =
	{
		{{"Compute-Graphics", "When STALL is selected, writeAtEndOfShader & waitOnAddress API calls direct the GPU to stall until a compute job has flushed L2$ before starting a graphics job that reads CB$. "
		"If this is not done, the graphics job may pull stale data from main memory into CB$, resulting in a glitch"}, &menuComputeGraphics},
		{{"Graphics-Graphics", "When STALL is selected, a surfaceSync packet directs the GPU to stall until graphics job X has flushed CB$ before starting graphics job Y, which reads X's render target as a texture through L2$. "
		"If this is not done, Y's texture fetches may pull stale data from main memory into L2$, resulting in a glitch"}, &menuGraphicsGraphics},
		{{"Graphics-Compute", "When STALL is selected, a surfaceSync packet directs the GPU to stall until a graphics job has flushed CB$ before starting a compute job that reads its render target as a texture through L2$. "
		"If this is not done, the compute job may pull stale data from main memory into L2$, resulting in a glitch"}, &menuGraphicsCompute},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };

	void switchToUnormFormat(Gnm::Texture *texture)
	{
		// Set channel type in the texture's format to UNorm.
		// This is needed because some textures we are writing in this sample could be in srgb format yet we try to write them with RW_Texture.
		// Since writing into RW_Texture does not properly handle srgb conversion we are going to switch all textures into unorm.
		Gnm::DataFormat srcFrmt = texture->getDataFormat();
		texture->setDataFormat(Gnm::DataFormat::build(srcFrmt.getSurfaceFormat(),Gnm::kTextureChannelTypeUNorm,srcFrmt.getChannel(0),srcFrmt.getChannel(1),srcFrmt.getChannel(2),srcFrmt.getChannel(3)));
	}
	void clearRenderTarget(Gnmx::GnmxGfxContext *gfxc, Gnm::RenderTarget *renderTarget, const Vector4& color)
	{
		Gnm::Texture texture;
		texture.initFromRenderTarget(renderTarget, false);
		switchToUnormFormat(&texture);
		texture.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // this texture will be bound as an RWTexture, and different CUs may hit different cache lines, so GPU-coherent it is.
		gfxc->setCsShader(computeShader[0].m_shader, &computeShader[0].m_offsetsTable);
		Vector4Unaligned* constantBuffer = (Vector4Unaligned*)gfxc->allocateFromCommandBuffer(sizeof(Vector4),Gnm::kEmbeddedDataAlignment4);
		*constantBuffer = color;
		Gnm::Buffer buffer;
		buffer.initAsConstantBuffer(constantBuffer, sizeof(*constantBuffer));
		buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &buffer);
		gfxc->setRwTextures(Gnm::kShaderStageCs, 0, 1, &texture);
		gfxc->dispatch( (renderTarget->getWidth()+7)/8, (renderTarget->getHeight()+7)/8, 1);
	}

	void copyRenderTarget(Gnmx::GnmxGfxContext *gfxc, Gnm::RenderTarget *dest, Gnm::RenderTarget *src)
	{
		Gnm::Texture textureDst;
		Gnm::Texture textureSrc;
		textureDst.initFromRenderTarget(dest, false);
		switchToUnormFormat(&textureDst);
		textureDst.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // this texture will be bound as an RWTexture, and different CUs may hit different cache lines, so GPU-coherent it is.
		textureSrc.initFromRenderTarget(src, false);
		switchToUnormFormat(&textureSrc);
		textureSrc.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture won't be bound as an RWTexture, so it's OK to declare it read-only.
		gfxc->setCsShader(computeShader[1].m_shader, &computeShader[1].m_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStageCs, 0, 1, &textureSrc);
		gfxc->setRwTextures(Gnm::kShaderStageCs, 0, 1, &textureDst);
		gfxc->dispatch((dest->getWidth()+7)/8, (dest->getHeight()+7)/8, 1);
	}

	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.initialize( "Synchronization", 
		"Sample code to illustrate how lack of explicit synchronization can cause glitches.", 
		"This sample program renders an ICE cube to an offscreen buffer, and then uses this buffer as a texture to render a cube "
		"in the main scene. Explicit synchronization is required after the offscreen buffer is cleared, after the offscreen "
		"scene is rendered, and before the back buffer for the scene is copied to the framework's back buffer.");

	framework.appendMenuItems(kMenuItems, menuItem);

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Gnm::DepthRenderTarget m_depthTarget;
		Gnm::RenderTarget m_offscreenRenderTarget;
		Gnm::Texture m_offscreenTexture;
		uint8_t m_reserved0[4];
		ShaderConstants* m_torusConst;
		ShaderConstants* m_cubeConst;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_torusConst,SCE_KERNEL_WB_ONION,sizeof(*frame->m_torusConst),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Torus Constant Buffer",buffer);
		framework.m_allocators.allocate((void**)&frame->m_cubeConst,SCE_KERNEL_WB_ONION,sizeof(*frame->m_cubeConst),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Torus Constant Buffer",buffer);

		enum { kRenderToTextureTargetWidth = 256 };
		enum { kRenderToTextureTargetHeight = 256 };

		Gnm::DataFormat depthFormat = Gnm::DataFormat::build(Gnm::kZFormat32Float);
		Gnm::TileMode depthTileMode;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&depthTileMode,GpuAddress::kSurfaceTypeDepthOnlyTarget,depthFormat,1);
		Gnm::SizeAlign depthTargetSizeAlign = Gnmx::Toolkit::init(&frame->m_depthTarget,kRenderToTextureTargetWidth,kRenderToTextureTargetHeight,depthFormat.getZFormat(),Gnm::kStencilInvalid,depthTileMode,Gnm::kNumFragments1,NULL,NULL);
		void *depthTargetPtr;
		framework.m_allocators.allocate(&depthTargetPtr,SCE_KERNEL_WC_GARLIC,depthTargetSizeAlign,Gnm::kResourceTypeDepthRenderTargetBaseAddress,"Buffer %d Depth Buffer",buffer);
		const uint32_t shadowZBaseAddress256 = static_cast<uint32_t>(reinterpret_cast<uint64_t>(depthTargetPtr)>>8);
		frame->m_depthTarget.setZReadAddress256ByteBlocks(shadowZBaseAddress256);
		frame->m_depthTarget.setZWriteAddress256ByteBlocks(shadowZBaseAddress256);

		// Set up render-to-texture target and texture
		Gnm::TileMode offScreenTileMode;
		Gnm::DataFormat offscreenFormat = Gnm::kDataFormatR8G8B8A8Unorm;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&offScreenTileMode,GpuAddress::kSurfaceTypeColorTarget,offscreenFormat,1);
		Gnm::SizeAlign offScreenRTSizeAlign = Gnmx::Toolkit::init(&frame->m_offscreenRenderTarget,kRenderToTextureTargetWidth,kRenderToTextureTargetHeight,numSlices,offscreenFormat,offScreenTileMode,Gnm::kNumSamples1,Gnm::kNumFragments1,NULL,NULL);
		void *offScreenRTPtr;
		framework.m_allocators.allocate(&offScreenRTPtr,SCE_KERNEL_WC_GARLIC,offScreenRTSizeAlign,Gnm::kResourceTypeRenderTargetBaseAddress,"Buffer %d Render Target",buffer);
		frame->m_offscreenRenderTarget.setAddresses(offScreenRTPtr,nullptr,nullptr);
		frame->m_offscreenTexture.initFromRenderTarget(&frame->m_offscreenRenderTarget,false);
		frame->m_offscreenTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we'll never bind this texture as an RWTexture, so it's safe to declare read-only.
	}

	// Set up back buffer target
	Gnm::TileMode backBufferTileMode;
	Gnm::DataFormat backBufferFormat = Gnm::kDataFormatR8G8B8A8Unorm;
	GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &backBufferTileMode, GpuAddress::kSurfaceTypeColorTarget, backBufferFormat, 1);
	Gnm::SizeAlign backBufferRTSizeAlign = Gnmx::Toolkit::init(&backBufferRenderTarget, framework.m_config.m_targetWidth, framework.m_config.m_targetHeight, numSlices, backBufferFormat, backBufferTileMode, Gnm::kNumSamples1, Gnm::kNumFragments1, NULL, NULL);
	void *backBufferRTPtr;
	framework.m_allocators.allocate(&backBufferRTPtr, SCE_KERNEL_WC_GARLIC, backBufferRTSizeAlign, Gnm::kResourceTypeRenderTargetBaseAddress, "Back Buffer");
	backBufferRenderTarget.setAddresses(backBufferRTPtr, nullptr, nullptr);	
	backBufferTexture.initFromRenderTarget(&backBufferRenderTarget, false);
	backBufferTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we'll never bind this texture as an RWTexture, so it's safe to declare read-only.

	int error = 0;

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/synchronization-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/synchronization-sample/shader_p.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&computeShader[0], Framework::ReadFile("/app0/synchronization-sample/cs_cleartexture2d_c.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&computeShader[1], Framework::ReadFile("/app0/synchronization-sample/cs_copytexture2d_c.sb"), &framework.m_allocators);

	BuildCubeMesh(&framework.m_allocators, "Cube", &smallCubeMesh, 1.0f);
	BuildCubeMesh(&framework.m_allocators, "Cube", &cubeMesh, 1.0f);

	// Set up texture
	error = Framework::loadTextureFromGnf(&texture, Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone);

	texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we'll never bind this texture as an RWTexture, so it's safe to declare read-only.

	sampler.init();
	sampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	sampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	while( !framework.m_shutdownRequested )
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		// Render to offscreen buffer
		{
			clearRenderTarget(gfxc, &frame->m_offscreenRenderTarget, Vector4(0.f, 0.5f, 0.5f, 1.f));
			if(computeGraphics)
			{
				Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
			}
			Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &frame->m_depthTarget, 1.f);
			gfxc->setRenderTargetMask(0xF);

			Gnm::DepthStencilControl dsc;
			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
			dsc.setDepthEnable(true);
			gfxc->setDepthStencilControl(dsc);

			gfxc->setRenderTarget(0, &frame->m_offscreenRenderTarget);
			gfxc->setDepthRenderTarget(&frame->m_depthTarget);
			gfxc->setupScreenViewport(0, 0, frame->m_offscreenRenderTarget.getWidth(), frame->m_offscreenRenderTarget.getHeight(), 0.5f, 0.5f);

			gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
			gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

			smallCubeMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

			gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &texture);
			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);

			Gnm::Buffer torusConstBuffer;
			torusConstBuffer.initAsConstantBuffer(frame->m_torusConst, sizeof(*frame->m_torusConst));
			torusConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
			const float radians = 0.3f * framework.GetSecondsElapsedApparent();
			const Matrix4 modelMatrix = Matrix4::rotationZYX(Vector3(radians,radians,0.f));
			const float aspect = (float)frame->m_offscreenRenderTarget.getWidth() / (float)frame->m_offscreenRenderTarget.getHeight();
			const Matrix4 torusProjMatrix = Matrix4::frustum( -aspect, aspect, -1, 1, framework.GetDepthNear(), framework.GetDepthFar() );
			const Matrix4 viewMatrix = Matrix4::lookAt(Point3(2,0,0), Point3(0,0,0), Vector3(0,1,0));
			const Matrix4 torusViewProjMatrix = torusProjMatrix * viewMatrix;
			frame->m_torusConst->mvp = transpose(torusViewProjMatrix*modelMatrix);
			gfxc->setConstantBuffers( Gnm::kShaderStageVs, 0, 1, &torusConstBuffer );

			gfxc->setPrimitiveType(smallCubeMesh.m_primitiveType);
			gfxc->setIndexSize(smallCubeMesh.m_indexType);
			gfxc->drawIndex(smallCubeMesh.m_indexCount, smallCubeMesh.m_indexBuffer);
			if(graphicsGraphics)
			{
				// Sync on the offscreen target
				gfxc->waitForGraphicsWrites(frame->m_offscreenRenderTarget.getBaseAddress256ByteBlocks(), (frame->m_offscreenRenderTarget.getSliceSizeInBytes()*numSlices)>>8, Gnm::kWaitTargetSlotCb0,
					Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable );
			}
		}

		// Render to back buffer
		{
			clearRenderTarget(gfxc, &backBufferRenderTarget, framework.getClearColor());
			Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb); // we do this even when compute-graphics sync is off, because otherwise it'd just blackscreen.
			Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
			gfxc->setRenderTargetMask(0xF);

			Gnm::DepthStencilControl dsc;
			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
			dsc.setDepthEnable(true);
			gfxc->setDepthStencilControl(dsc);

			gfxc->setRenderTarget(0, &backBufferRenderTarget);
			gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);
			gfxc->setupScreenViewport(0, 0, backBufferRenderTarget.getWidth(), backBufferRenderTarget.getHeight(), 0.5f, 0.5f);

			gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
			gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

			cubeMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs );

			gfxc->setTextures( Gnm::kShaderStagePs, 0, 1, &frame->m_offscreenTexture );
			gfxc->setSamplers( Gnm::kShaderStagePs, 0, 1, &sampler );

			Gnm::Buffer cubeConstBuffer;
			cubeConstBuffer.initAsConstantBuffer(frame->m_cubeConst, sizeof(*frame->m_cubeConst));
			cubeConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
			const float radians = 0.3f * framework.GetSecondsElapsedApparent();
			const Matrix4 modelMatrix = Matrix4::rotationZYX(Vector3(radians,radians,0.f)) * Matrix4::scale(Vector3(1.1f));
			frame->m_cubeConst->mvp = transpose(framework.m_viewProjectionMatrix*modelMatrix);
			gfxc->setConstantBuffers( Gnm::kShaderStageVs, 0, 1, &cubeConstBuffer );

			gfxc->setPrimitiveType(cubeMesh.m_primitiveType);
			gfxc->setIndexSize(cubeMesh.m_indexType);
			gfxc->drawIndex(cubeMesh.m_indexCount, cubeMesh.m_indexBuffer);
		}

		if(graphicsCompute)
		{
			Gnmx::Toolkit::synchronizeRenderTargetGraphicsToCompute(&gfxc->m_dcb, &backBufferRenderTarget);
		}
		copyRenderTarget(gfxc, &bufferCpuIsWriting->m_renderTarget, &backBufferRenderTarget);
		if(computeGraphics)
		{
			Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
		}
		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
}
