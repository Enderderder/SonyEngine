/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/frame.h"
#include <gnmx/resourcebarrier.h>
using namespace sce;

namespace
{
	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader;
	Gnm::Texture texture;
	Gnm::Sampler sampler;
	Framework::SimpleMesh torusMesh, cubeMesh;
	struct ShaderConstants
	{
		Matrix4Unaligned mvp;
	};
	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);
	framework.initialize( "Render To Texture", 
		"Sample code to illustrate how to render into a texture, and then to read from the texture while rendering another scene.", 
		"This sample program renders a torus to an offscreen buffer, and then uses this buffer as a texture to render a cube "
		"in another scene.");

	// Set up depth target for render-to-texture target

	enum { kRenderToTextureTargetWidth = 256 };
	enum { kRenderToTextureTargetHeight = 256 };

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Gnm::Texture m_offscreenTexture;
		Gnm::RenderTarget m_offscreenRenderTarget;
		Gnm::DepthRenderTarget m_offscreenDepthTarget;
		Gnmx::ResourceBarrier m_offscreenResourceBarrier;
		ShaderConstants *m_torusConst;
		ShaderConstants *m_cubeConst;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_torusConst,SCE_KERNEL_WB_ONION,sizeof(*frame->m_torusConst),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d torus constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_cubeConst,SCE_KERNEL_WB_ONION,sizeof(*frame->m_cubeConst),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d cube constants",buffer);

		Gnm::DataFormat depthFormat = Gnm::DataFormat::build(Gnm::kZFormat32Float);
		Gnm::TileMode depthTileMode;
		uint32_t numSlices = 1;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&depthTileMode,GpuAddress::kSurfaceTypeDepthOnlyTarget,depthFormat,1);
		Gnm::SizeAlign depthTargetSizeAlign = Gnmx::Toolkit::init(&frame->m_offscreenDepthTarget,kRenderToTextureTargetWidth,kRenderToTextureTargetHeight,depthFormat.getZFormat(),Gnm::kStencilInvalid,depthTileMode,Gnm::kNumFragments1,NULL,NULL);
		void *depth2CpuBaseAddr;
		framework.m_allocators.allocate(&depth2CpuBaseAddr,SCE_KERNEL_WC_GARLIC,depthTargetSizeAlign,Gnm::kResourceTypeDepthRenderTargetBaseAddress,"Buffer %d Offscreen Depth",buffer);
		frame->m_offscreenDepthTarget.setZReadAddress(depth2CpuBaseAddr);
		frame->m_offscreenDepthTarget.setZWriteAddress(depth2CpuBaseAddr);

		// Set up render-to-texture target and texture
		Gnm::TileMode offScreenTileMode;
		Gnm::DataFormat offscreenFormat = Gnm::kDataFormatR8G8B8A8Unorm;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&offScreenTileMode,GpuAddress::kSurfaceTypeColorTarget,offscreenFormat,1);
		Gnm::SizeAlign offScreenRTSizeAlign = Gnmx::Toolkit::init(&frame->m_offscreenRenderTarget,kRenderToTextureTargetWidth,kRenderToTextureTargetHeight,numSlices,offscreenFormat,offScreenTileMode,Gnm::kNumSamples1,Gnm::kNumFragments1,NULL,NULL);

		void *renderTargetColorAddress;
		framework.m_allocators.allocate(&renderTargetColorAddress,SCE_KERNEL_WC_GARLIC,offScreenRTSizeAlign,Gnm::kResourceTypeRenderTargetBaseAddress,"Buffer %d Offscreen Color",buffer);

		frame->m_offscreenRenderTarget.setAddresses(renderTargetColorAddress,nullptr,nullptr);
		frame->m_offscreenTexture.initFromRenderTarget(&frame->m_offscreenRenderTarget,false);
		frame->m_offscreenTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never write to this texture from a shader, so it's OK to mark the texture as read-only.
		frame->m_offscreenResourceBarrier.init(&frame->m_offscreenRenderTarget,Gnmx::ResourceBarrier::kUsageRenderTarget,Gnmx::ResourceBarrier::kUsageRoTexture);
	}

	int error = 0;

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/render-to-texture-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/render-to-texture-sample/shader_p.sb"), &framework.m_allocators);

	BuildTorusMesh(&framework.m_allocators, "Torus", &torusMesh, 0.8f, 0.2f, 64, 32, 4, 1);
	BuildCubeMesh(&framework.m_allocators, "Cube", &cubeMesh, 1.0f);

	// Set up texture
	error = Framework::loadTextureFromGnf(&texture, Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error  == Framework::kGnfErrorNone );

	texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never write to this texture from a shader, so it's OK to mark the texture as read-only.

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
			// Clear
			Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &frame->m_offscreenRenderTarget, Vector4(0.25f,0.5f,0.25f,1.f));
			Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &frame->m_offscreenDepthTarget, 1.f);
			gfxc->setRenderTargetMask(0xF);

			Gnm::DepthStencilControl dsc;
			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
			dsc.setDepthEnable(true);
			gfxc->setDepthStencilControl(dsc);

			gfxc->setRenderTarget(0, &frame->m_offscreenRenderTarget);
			gfxc->setDepthRenderTarget(&frame->m_offscreenDepthTarget);
			gfxc->setupScreenViewport(0, 0, frame->m_offscreenRenderTarget.getWidth(), frame->m_offscreenRenderTarget.getHeight(), 0.5f, 0.5f);

			gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
			gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

			torusMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

			gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &texture);
			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);

			Gnm::Buffer torusConstBuffer;
			torusConstBuffer.initAsConstantBuffer(frame->m_torusConst, sizeof(*frame->m_torusConst));
			torusConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
			const float radians = 0.3f * framework.GetSecondsElapsedApparent();
			const Matrix4 modelMatrix = Matrix4::rotationZYX(Vector3(radians,radians,0.f));
			const float aspect = (float)frame->m_offscreenRenderTarget.getWidth() / (float)frame->m_offscreenRenderTarget.getHeight();
			const Matrix4 torusProjMatrix = Matrix4::frustum( -aspect, aspect, -1, 1, framework.GetDepthNear(), framework.GetDepthFar() );
			const Matrix4 viewMatrix = Matrix4::lookAt(Point3(2,0,0), Point3(0,0,0), Vector3(0,1,0));
			const Matrix4 torusViewProjMatrix = torusProjMatrix * viewMatrix;
			frame->m_torusConst->mvp = transpose(torusViewProjMatrix*modelMatrix);
			gfxc->setConstantBuffers( Gnm::kShaderStageVs, 0, 1, &torusConstBuffer );

			gfxc->setPrimitiveType(torusMesh.m_primitiveType);
			gfxc->setIndexSize(torusMesh.m_indexType);
			gfxc->drawIndex(torusMesh.m_indexCount, torusMesh.m_indexBuffer);
		}

		frame->m_offscreenResourceBarrier.write(&gfxc->m_dcb);

		// Render to main back buffer
		{
			// Clear
			Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
			Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
			gfxc->setRenderTargetMask(0xF);

			Gnm::DepthStencilControl dsc;
			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
			dsc.setDepthEnable(true);
			gfxc->setDepthStencilControl(dsc);

			gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
			gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);
			gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);

			gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
			gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

			cubeMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

			gfxc->setTextures( Gnm::kShaderStagePs, 0, 1, &frame->m_offscreenTexture );
			gfxc->setSamplers( Gnm::kShaderStagePs, 0, 1, &sampler );

			Gnm::Buffer cubeConstBuffer;
			cubeConstBuffer.initAsConstantBuffer(frame->m_cubeConst, sizeof(*frame->m_cubeConst));
			cubeConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
			const float radians = 0.3f * framework.GetSecondsElapsedApparent();
			const Matrix4 modelMatrix = Matrix4::rotationZYX(Vector3(radians,radians,0.f)) * Matrix4::scale(Vector3(1.1f));
			frame->m_cubeConst->mvp = transpose(framework.m_viewProjectionMatrix*modelMatrix);
			gfxc->setConstantBuffers( Gnm::kShaderStageVs, 0, 1, &cubeConstBuffer );

			gfxc->setPrimitiveType(cubeMesh.m_primitiveType);
			gfxc->setIndexSize(cubeMesh.m_indexType);
			gfxc->drawIndex(cubeMesh.m_indexCount, cubeMesh.m_indexBuffer);
		}

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
    return 0;
}
