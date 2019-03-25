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
	class VolumeConstants
	{
	public:
		Matrix4Unaligned m_modelViewProjection; // model-view-projection matrix
		Matrix4Unaligned m_model; // model matrix
	};

	Gnm::Texture texture;

	int32_t mipLevel = 0;
	class MenuCommandMipLevel : public Framework::MenuCommand
	{
	public:
		MenuCommandMipLevel()
		{
		}
		virtual void printValue(DbgFont::Window *window)
		{
			window->printf("%d", mipLevel);
		}
		virtual void adjust(int delta) const
		{
			texture.setMipLevelRange(Framework::Modulo( texture.getBaseMipLevel()+delta, texture.getLastMipLevel()+1 ), texture.getLastMipLevel());
			mipLevel = texture.getBaseMipLevel();
		}
	};

	MenuCommandMipLevel MipLevel;
	const Framework::MenuItem menuItem[] =
	{
		{{"Mip level", "Adjust the mip level for texture sampling"}, &MipLevel},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };

	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);
	framework.initialize("Volume", 
		"Sample code to illustrate the programmatic creation of a volume texture.",
		"This sample program generates a volume texture programmatically, and then uses "
		"it to render a torus such that the world position of the fragment is used as "
		"a UVW lookup into the volume texture.");
	framework.appendMenuItems(kMenuItems, menuItem);

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		VolumeConstants *m_constants;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Command Buffer",buffer);
	}

	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader;

	int error = 0;

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/volume-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/volume-sample/shader_p.sb"), &framework.m_allocators);

	//
	// Set up the Texture
	//
	const uint32_t volumeTexWidth  = 128;
	const uint32_t volumeTexHeight = 64;
	const uint32_t volumeTexDepth  = 32;
	const Gnm::DataFormat volumeFormat = Gnm::kDataFormatB8G8R8A8Unorm;
	uint32_t mipCount = 1; // the base level
	{
		uint32_t mipWidth = volumeTexWidth, mipHeight = volumeTexHeight, mipDepth = volumeTexDepth;
		while(mipWidth > 1 || mipHeight > 1 || mipDepth > 1)
		{
			mipWidth  = std::max(mipWidth>>1, 1U);
			mipHeight = std::max(mipHeight>>1, 1U);
			mipDepth  = std::max(mipDepth>>1, 1U);
			++mipCount;
		}
	}
	//mipCount = 1;
	Gnm::TileMode volumeTileMode = Gnm::kTileModeThin_1dThin;
	int32_t addrStatus = 0;
//	addrStatus = GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &volumeTileMode, GpuAddress::kSurfaceTypeTextureVolume, volumeFormat, 1);
	SCE_GNM_ASSERT(addrStatus == GpuAddress::kStatusSuccess);
	Gnm::SizeAlign sa0 = Gnmx::Toolkit::initAs3d(&texture, volumeTexWidth, volumeTexHeight, volumeTexDepth, mipCount, volumeFormat, volumeTileMode);
	const uint32_t linearBaseLevelSize = volumeTexWidth*volumeTexHeight*volumeTexDepth*volumeFormat.getBytesPerElement();
	uint32_t *linearTexels;
	uint8_t *finalTexels;
	framework.m_allocators.allocate((void**)&linearTexels, SCE_KERNEL_WB_ONION, linearBaseLevelSize, 4, Gnm::kResourceTypeGenericBuffer, "Linear Texels");
	framework.m_allocators.allocate((void**)&finalTexels,  SCE_KERNEL_WC_GARLIC, sa0, Gnm::kResourceTypeTextureBaseAddress, "Final Texels");
	memset(finalTexels, 0x00, sa0.m_size);
	GpuAddress::TilingParameters tp;
	addrStatus = tp.initFromTexture(&texture, 0, 0); // We'll update these further inside the loop
	SCE_GNM_ASSERT(addrStatus == GpuAddress::kStatusSuccess);

	const unsigned iMip = 0;

	// Update tiling params
	tp.m_linearWidth  = std::max(volumeTexWidth  >> iMip, 1U);
	tp.m_linearHeight = std::max(volumeTexHeight >> iMip, 1U);
	tp.m_linearDepth  = std::max(volumeTexDepth  >> iMip, 1U);
	tp.m_mipLevel = iMip;
	tp.m_baseTiledPitch = (iMip > 0) ? texture.getPitch() : 0;
	memset(linearTexels, 0x80, linearBaseLevelSize);
	// Generate linear texel data
	for(uint32_t z=0; z<tp.m_linearDepth; ++z)
	{
		for(uint32_t y=0; y<tp.m_linearHeight; ++y)
		{
			for(uint32_t x=0; x<tp.m_linearWidth; ++x)
			{
				linearTexels[z*tp.m_linearHeight*tp.m_linearWidth + y*tp.m_linearWidth + x] =
					0xFF000000 |
					((uint32_t)(((float)x + 0.5f) * 255.0f / (float)tp.m_linearWidth)  << 16) |
					((uint32_t)(((float)y + 0.5f) * 255.0f / (float)tp.m_linearHeight) <<  8) |
					((uint32_t)(((float)z + 0.5f) * 255.0f / (float)tp.m_linearDepth)  <<  0);
			}
		}
	}
	// Tile into final texel buffer
	uint64_t mipOffset = 0;
	uint64_t mipSize = 0;
	addrStatus = GpuAddress::computeTextureSurfaceOffsetAndSize(&mipOffset, &mipSize, &texture, iMip, 0);
	SCE_GNM_ASSERT(addrStatus == GpuAddress::kStatusSuccess);
	printf("Mip %d: offset=0x%08lX size=0x%08X\n", iMip, mipOffset, (uint32_t)mipSize);
	addrStatus = GpuAddress::tileSurface(finalTexels+mipOffset, linearTexels, &tp);
	SCE_GNM_ASSERT(addrStatus == GpuAddress::kStatusSuccess);
	addrStatus = GpuAddress::detileSurface(linearTexels, finalTexels+mipOffset, &tp); // completely redundant; included for test coverage purposes
	SCE_GNM_ASSERT(addrStatus == GpuAddress::kStatusSuccess);

	texture.setBaseAddress(finalTexels);
	texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never write to this texture from a shader, so it's OK to mark it as read-only.

	{
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
		Gnmx::Toolkit::SurfaceUtil::generateMipMaps(*gfxc, &texture);
		Gnmx::Toolkit::submitAndStall(*gfxc);
	}

	Gnm::Sampler sampler;
	sampler.init();
	sampler.setWrapMode(Gnm::kWrapModeWrap, Gnm::kWrapModeWrap, Gnm::kWrapModeWrap);
	sampler.setXyFilterMode(Gnm::kFilterModePoint, Gnm::kFilterModePoint);
	sampler.setZFilterMode(Gnm::kZFilterModePoint);
	sampler.setMipFilterMode(Gnm::kMipFilterModeNone);
	sampler.setBorderColor(Gnm::kBorderColorOpaqueBlack);

	Framework::SimpleMesh simpleMesh;
	BuildTorusMesh(&framework.m_allocators, "Torus", &simpleMesh, 0.8f, 0.2f, 64, 32, 4, 1);

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

		simpleMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(frame->m_constants,sizeof(*frame->m_constants));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &texture);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);

		const float seconds = framework.GetSecondsElapsedApparent();
		const Matrix4 modelMatrix = Matrix4::translation( Vector3(sinf(seconds),0,0) );

		frame->m_constants->m_model = transpose(modelMatrix);
		frame->m_constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*modelMatrix);

		gfxc->setPrimitiveType(simpleMesh.m_primitiveType);
		gfxc->setIndexSize(simpleMesh.m_indexType);
		gfxc->drawIndex(simpleMesh.m_indexCount, simpleMesh.m_indexBuffer);

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
}
