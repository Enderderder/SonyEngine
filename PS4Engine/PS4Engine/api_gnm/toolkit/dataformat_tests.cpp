/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "toolkit.h"
#include "embedded_shader.h"
#include "dataformat_interpreter.h"
#include "dataformat_tests.h"
#include "floating_point.h"
#include "allocator.h"
#include <algorithm>
using namespace sce;

static const uint32_t cs_cleartexture1d_c[] = {
#include "cs_cleartexture1d_c.h"
};
static const uint32_t cs_cleartexture2d_c[] = {
#include "cs_cleartexture2d_c.h"
};
static const uint32_t cs_cleartexture3d_c[] = {
#include "cs_cleartexture3d_c.h"
};
static const uint32_t cs_clearbuffer_c[] = {
#include "cs_clearbuffer_c.h"
};
static const uint32_t pix_clear_p[] = {
#include "pix_clear_p.h"
};
static const uint32_t cs_test_buffer_read_c[] = {
#include "cs_test_buffer_read_c.h"
};
static const uint32_t cs_test_texture_read_c[] = {
#include "cs_test_texture_read_c.h"
};

static sce::Gnmx::Toolkit::EmbeddedCsShader s_cleartexture1d = {cs_cleartexture1d_c, "Clear Texture 1D Compute Shader"};
static sce::Gnmx::Toolkit::EmbeddedCsShader s_cleartexture2d = {cs_cleartexture2d_c, "Clear Texture 2D Compute Shader"};
static sce::Gnmx::Toolkit::EmbeddedCsShader s_cleartexture3d = {cs_cleartexture3d_c, "Clear Texture 3D Compute Shader"};
static sce::Gnmx::Toolkit::EmbeddedCsShader s_clearbuffer = {cs_clearbuffer_c, "Clear Buffer Compute Shader"};
static sce::Gnmx::Toolkit::EmbeddedCsShader s_test_buffer_read = {cs_test_buffer_read_c, "Test Buffer Read Compute Shader"};
static sce::Gnmx::Toolkit::EmbeddedCsShader s_test_texture_read = {cs_test_texture_read_c, "Test Texture Read Compute Shader"};
static sce::Gnmx::Toolkit::EmbeddedPsShader s_pix_clear_p = {pix_clear_p, "Clear Pixel Shader"};

static sce::Gnmx::Toolkit::EmbeddedCsShader *s_embeddedCsShader[] =
{
	&s_cleartexture1d, &s_cleartexture2d, &s_cleartexture3d, &s_clearbuffer, &s_test_buffer_read, &s_test_texture_read
};
static sce::Gnmx::Toolkit::EmbeddedPsShader *s_embeddedPsShader[] =
{
	&s_pix_clear_p,
};
static sce::Gnmx::Toolkit::EmbeddedShaders s_embeddedShaders =
{
	s_embeddedCsShader, sizeof(s_embeddedCsShader)/sizeof(s_embeddedCsShader[0]),
	s_embeddedPsShader, sizeof(s_embeddedPsShader)/sizeof(s_embeddedPsShader[0]),
};

void clearBufferWithBufferStore(sce::Gnmx::GnmxGfxContext &gfxc, const Gnm::Buffer *buffer, const Vector4 &color)
{
	gfxc.setCsShader(s_clearbuffer.m_shader, &s_clearbuffer.m_offsetsTable);

	Gnm::Buffer bufferCopy = *buffer;
	bufferCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination buffer is GPU-coherent, because we will write to it.
	gfxc.setRwBuffers(Gnm::kShaderStageCs, 0, 1, &bufferCopy);

	Vector4 *constants = (Vector4*)gfxc.allocateFromCommandBuffer(sizeof(Vector4), Gnm::kEmbeddedDataAlignment4);
	*constants = color;
	Gnm::Buffer constantBuffer;
	constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
	gfxc.setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &constantBuffer);

	gfxc.dispatch((buffer->getNumElements() + 63) / 64, 1, 1);

	sce::Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc.m_dcb);
}

void clearTextureWithTextureStore(sce::Gnmx::GnmxGfxContext &gfxc, const Gnm::Texture* texture, const Vector4 &color)
{
	sce::Gnmx::Toolkit::EmbeddedCsShader *shader = 0;

	uint32_t dim[3] = {1, 1, 1};
	switch(texture->getTextureType())
	{
	case Gnm::kTextureType1d:
	case Gnm::kTextureType1dArray:
		shader = &s_cleartexture1d;
		dim[0] = (texture->getWidth()+63) / 64;
		break;
	case Gnm::kTextureType2d:
	case Gnm::kTextureType2dArray:
		shader = &s_cleartexture2d;
		dim[0] = (texture->getWidth() +7) / 8;
		dim[1] = (texture->getHeight()+7) / 8;
		break;
	case Gnm::kTextureType3d:
		shader = &s_cleartexture3d;
		dim[0] = (texture->getWidth() +3) / 4;
		dim[1] = (texture->getHeight()+3) / 4;
		dim[2] = (texture->getDepth() +3) / 4;
		break;
	default:
		SCE_GNM_ERROR("textureDst's dimensionality (0x%02X) is not supported by this function.", texture->getTextureType());
		return;
	}

	gfxc.setCsShader(shader->m_shader, &shader->m_offsetsTable);

	Gnm::Texture textureCopy = *texture;
	textureCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
	gfxc.setRwTextures(Gnm::kShaderStageCs, 0, 1, &textureCopy);

	Vector4 *constants = (Vector4*)gfxc.allocateFromCommandBuffer(sizeof(Vector4), Gnm::kEmbeddedDataAlignment4);
	*constants = color;
	Gnm::Buffer constantBuffer;
	constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
	gfxc.setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &constantBuffer);

	gfxc.dispatch(dim[0], dim[1], dim[2]);

	sce::Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc.m_dcb);
}

void clearRenderTargetWithShaderExport(sce::Gnmx::GnmxGfxContext &gfxc, const Gnm::RenderTarget* renderTarget, const Vector4 &color)
{
	gfxc.setRenderTarget(0, renderTarget);
	gfxc.setDepthRenderTarget((Gnm::DepthRenderTarget*)NULL);
	gfxc.setPsShader(s_pix_clear_p.m_shader, &s_pix_clear_p.m_offsetsTable);

	Gnm::BlendControl blendControl;
	blendControl.init();
	blendControl.setBlendEnable(false);
	gfxc.setBlendControl(0, blendControl);
	gfxc.setRenderTargetMask(0x0000000F);

	Gnm::DepthStencilControl dsc;
	dsc.init();
	dsc.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways);
	dsc.setDepthEnable(false);
	gfxc.setDepthStencilControl(dsc);
	gfxc.setupScreenViewport(0, 0, renderTarget->getWidth(), renderTarget->getHeight(), 0.5f, 0.5f);
	gfxc.setDepthStencilDisable();

	Vector4 *constants = (Vector4*)gfxc.allocateFromCommandBuffer(sizeof(Vector4), Gnm::kEmbeddedDataAlignment4);
	*constants = color;
	Gnm::Buffer constantBuffer;
	constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
	gfxc.setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

	gfxc.setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);
	sce::Gnmx::renderFullScreenQuad(&gfxc);
}

void Gnmx::Toolkit::DataFormatTests::read(Gnmx::Toolkit::DataFormatTests::ReadResult *result, GnmxGfxContext *gfxc, const Gnm::DataFormat dataFormat, Gnmx::Toolkit::IAllocator *allocator, uint32_t *src)
{
	enum {kWidth = 64};
	enum {kHeight = 64};
	enum {kElements = 128};
	memset(result, 0, sizeof(*result));

	Reg32 *destBufferAddress = (Reg32*)allocator->allocate(Gnm::kDataFormatR32G32B32A32Float.getBytesPerElement() * kElements, Gnm::kAlignmentOfBufferInBytes);
	Gnm::Buffer destBuffer;
	destBuffer.initAsDataBuffer(destBufferAddress, Gnm::kDataFormatR32G32B32A32Float, kElements);
	destBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);

	if(dataFormat.supportsBuffer())
	{
		gfxc->reset();

		void *srcBufferAddress = allocator->allocate(dataFormat.getBytesPerElement(), Gnm::kAlignmentOfBufferInBytes);
		memcpy(srcBufferAddress, src, dataFormat.getBytesPerElement());
		Gnm::Buffer srcBuffer;
		srcBuffer.initAsDataBuffer(srcBufferAddress, dataFormat, 1);
		srcBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

		memset(destBufferAddress, 0, Gnm::kDataFormatR32G32B32A32Float.getBytesPerElement() * kElements);

		gfxc->setCsShader(s_test_buffer_read.m_shader, &s_test_buffer_read.m_offsetsTable);
		gfxc->setBuffers(Gnm::kShaderStageCs, 0, 1, &srcBuffer);
		gfxc->setRwBuffers(Gnm::kShaderStageCs, 0, 1, &destBuffer);
		gfxc->dispatch(1, 1, 1);
		Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
		submitAndStall(*gfxc);
		result->m_bufferLoad[0] = destBufferAddress[0];
		result->m_bufferLoad[1] = destBufferAddress[1];
		result->m_bufferLoad[2] = destBufferAddress[2];
		result->m_bufferLoad[3] = destBufferAddress[3];

		allocator->release(srcBufferAddress);
	}
	if(dataFormat.getSurfaceFormat() != Gnm::kSurfaceFormat32_32_32)
	{
		gfxc->reset();

		Gnm::Texture texture;
		Gnm::TileMode tileMode;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeRwTextureFlat, dataFormat, 1);
		Gnm::SizeAlign sizeAlign = Gnmx::Toolkit::initAs2d(&texture, kWidth, kHeight, 1, dataFormat, tileMode, Gnm::kNumFragments1);
		uint32_t *textureAddress = (uint32_t*)allocator->allocate(sizeAlign);
		for(uint32_t dword = 0; dword < sizeAlign.m_size / sizeof(uint32_t); dword += 4)
		{
			textureAddress[dword+0] = src[0];
			textureAddress[dword+1] = src[1];
			textureAddress[dword+2] = src[2];
			textureAddress[dword+3] = src[3];
		}
		texture.setBaseAddress(textureAddress);
		texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

		gfxc->setCsShader(s_test_texture_read.m_shader, &s_test_texture_read.m_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStageCs, 0, 1, &texture);
		gfxc->setRwBuffers(Gnm::kShaderStageCs, 0, 1, &destBuffer);
		gfxc->dispatch(1, 1, 1);
		Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
		submitAndStall(*gfxc);
		result->m_textureLoad[0] = destBufferAddress[0];
		result->m_textureLoad[1] = destBufferAddress[1];
		result->m_textureLoad[2] = destBufferAddress[2];
		result->m_textureLoad[3] = destBufferAddress[3];

		allocator->release(textureAddress);
	}

	allocator->release(destBufferAddress);

	dataFormatDecoder(result->m_simulatorDecode, src, dataFormat);
}

void Gnmx::Toolkit::DataFormatTests::write(Gnmx::Toolkit::DataFormatTests::WriteResult *result, GnmxGfxContext *gfxc, const Gnm::DataFormat dataFormat, Gnmx::Toolkit::IAllocator *allocator, Reg32 *src)
{
	enum {kWidth = 64};
	enum {kHeight = 64};
	memset(result, 0, sizeof(*result));
	if(dataFormat.getSurfaceFormat() <= Gnm::kSurfaceFormatX24_8_32 && dataFormat.getSurfaceFormat() != Gnm::kSurfaceFormat32_32_32 && dataFormat.getTextureChannelType() <= Gnm::kTextureChannelTypeSrgb && dataFormat.getTextureChannelType() != Gnm::kTextureChannelTypeUScaled && dataFormat.getTextureChannelType() != Gnm::kTextureChannelTypeSScaled && dataFormat.getTextureChannelType() != Gnm::kTextureChannelTypeSNormNoZero)
	{
		gfxc->reset();

		Gnm::RenderTarget renderTarget;
		Gnm::TileMode tileMode;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeColorTarget, dataFormat, 1);
		Gnm::SizeAlign sizeAlign = Gnmx::Toolkit::init(&renderTarget, kWidth, kHeight, 1, dataFormat, tileMode, Gnm::kNumSamples1, Gnm::kNumFragments1, NULL, NULL);
		void *renderTargetAddress = allocator->allocate(sizeAlign);
		memset(renderTargetAddress, 0, sizeAlign.m_size);
		renderTarget.setAddresses(renderTargetAddress, 0, 0);
		const Vector4 color(src[0].f, src[1].f, src[2].f, src[3].f);
		gfxc->m_dcb.flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable); // tell the CP to flush the L1$ and L2$
		clearRenderTargetWithShaderExport(*gfxc, &renderTarget, color);
		Gnmx::Toolkit::synchronizeRenderTargetGraphicsToCompute(&gfxc->m_dcb, &renderTarget);
		gfxc->m_dcb.flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable); // tell the CP to flush the L1$ and L2$
		submitAndStall(*gfxc);
		result->m_rasterizerExport[0] = ((uint32_t*)renderTargetAddress)[0];
		result->m_rasterizerExport[1] = ((uint32_t*)renderTargetAddress)[1];
		result->m_rasterizerExport[2] = ((uint32_t*)renderTargetAddress)[2];
		result->m_rasterizerExport[3] = ((uint32_t*)renderTargetAddress)[3];
		allocator->release(renderTargetAddress);
	}
	if(dataFormat.supportsBuffer())
	{
		gfxc->reset();

		enum {kElements = kWidth * kHeight};
		void *bufferAddress = allocator->allocate(dataFormat.getBytesPerElement() * kElements, Gnm::kAlignmentOfBufferInBytes);
		memset(bufferAddress, 0, dataFormat.getBytesPerElement() * kElements);
		Gnm::Buffer buffer;
		buffer.initAsDataBuffer(bufferAddress, dataFormat, kElements);
		const Vector4 color(src[0].f, src[1].f, src[2].f, src[3].f);
		clearBufferWithBufferStore(*gfxc, &buffer, color);
		submitAndStall(*gfxc);
		result->m_bufferStore[0] = ((uint32_t*)bufferAddress)[0];
		result->m_bufferStore[1] = ((uint32_t*)bufferAddress)[1];
		result->m_bufferStore[2] = ((uint32_t*)bufferAddress)[2];
		result->m_bufferStore[3] = ((uint32_t*)bufferAddress)[3];
		allocator->release(bufferAddress);
	}
	if(dataFormat.getSurfaceFormat() != Gnm::kSurfaceFormat32_32_32)
	{
		gfxc->reset();

		Gnm::Texture texture;
		Gnm::TileMode tileMode;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeRwTextureFlat, dataFormat, 1);
		Gnm::SizeAlign sizeAlign = Gnmx::Toolkit::initAs2d(&texture, kWidth, kHeight, 1, dataFormat, tileMode, Gnm::kNumFragments1);
		void *textureAddress = allocator->allocate(sizeAlign);
		memset(textureAddress, 0, sizeAlign.m_size);
		texture.setBaseAddress(textureAddress);
		texture.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
		const Vector4 color(src[0].f, src[1].f, src[2].f, src[3].f);
		clearTextureWithTextureStore(*gfxc, &texture, color);
		submitAndStall(*gfxc);
		result->m_textureStore[0] = ((uint32_t*)textureAddress)[0];
		result->m_textureStore[1] = ((uint32_t*)textureAddress)[1];
		result->m_textureStore[2] = ((uint32_t*)textureAddress)[2];
		result->m_textureStore[3] = ((uint32_t*)textureAddress)[3];
		allocator->release(textureAddress);
	}
	uint32_t dwords;
	dataFormatEncoder(result->m_simulatorEncode, &dwords, src, dataFormat);
}

void Gnmx::Toolkit::DataFormatTests::initializeWithAllocators(sce::Gnmx::Toolkit::Allocators *allocators)
{
	s_embeddedShaders.initializeWithAllocators(allocators);
}
