/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "toolkit.h"
#include "floating_point.h"
#include <algorithm>
#include "embedded_shader.h"
#include <gnmx/shader_parser.h>
#include <sys/dmem.h>
using namespace sce;

void sce::Gnmx::Toolkit::setViewport(Gnmx::GnmxDrawCommandBuffer *dcb, uint32_t viewportId, int32_t left, int32_t top, int32_t right, int32_t bottom, float dmin, float dmax)
{
	dcb->pushMarker("Toolkit::setViewport");
	const Vector3 scale( (right-left)*0.5f, (top-bottom)*0.5f, 0.5f );
	const Vector3 bias( left + (right-left)*0.5f, top + (bottom-top)*0.5f, 0.5f );
	dcb->setViewport(viewportId, dmin, dmax, (const float*)&scale, (const float*)&bias);
	dcb->popMarker();
}

namespace
{
	static bool s_initialized = false;

	static const uint32_t pix_clear_p[] = {
#include "pix_clear_p.h"
	};
	static const uint32_t pix_generateMipMaps_2d_p[] = {
#include "pix_generateMipMaps_2d_p.h"
	};
	static const uint32_t pix_generateMipMaps_3d_p[] = {
#include "pix_generateMipMaps_3d_p.h"
	};

	static const uint32_t vex_clear_vv[] = {
#include "vex_clear_vv.h"
	};

	static const uint32_t cs_set_uint_c[] = {
#include "cs_set_uint_c.h"
	};
	static const uint32_t cs_set_uint_fast_c[] = {
#include "cs_set_uint_fast_c.h"
	};

	static const uint32_t cs_copybuffer_c[] = {
#include "cs_copybuffer_c.h"
	};
	static const uint32_t cs_copytexture1d_c[] = {
#include "cs_copytexture1d_c.h"
	};
	static const uint32_t cs_copytexture2d_c[] = {
#include "cs_copytexture2d_c.h"
	};
	static const uint32_t cs_copytexture3d_c[] = {
#include "cs_copytexture3d_c.h"
	};
	static const uint32_t cs_copyrawtexture1d_c[] = {
#include "cs_copyrawtexture1d_c.h"
	};
	static const uint32_t cs_copyrawtexture2d_c[] = {
#include "cs_copyrawtexture2d_c.h"
	};
	static const uint32_t cs_copyrawtexture3d_c[] = {
#include "cs_copyrawtexture3d_c.h"
	};

	static const uint32_t copytexture2d_p[] = {
#include "copytexture2d_p.h"
	};

	static sce::Gnmx::Toolkit::EmbeddedCsShader s_set_uint = {cs_set_uint_c};
	static sce::Gnmx::Toolkit::EmbeddedCsShader s_set_uint_fast = {cs_set_uint_fast_c};
	static sce::Gnmx::Toolkit::EmbeddedCsShader s_copyBuffer = {cs_copybuffer_c};
	static sce::Gnmx::Toolkit::EmbeddedCsShader s_copyTexture1d = {cs_copytexture1d_c};
	static sce::Gnmx::Toolkit::EmbeddedCsShader s_copyTexture2d = {cs_copytexture2d_c};
	static sce::Gnmx::Toolkit::EmbeddedCsShader s_copyTexture3d = {cs_copytexture3d_c};
	static sce::Gnmx::Toolkit::EmbeddedCsShader s_copyRawTexture1d = {cs_copyrawtexture1d_c};
	static sce::Gnmx::Toolkit::EmbeddedCsShader s_copyRawTexture2d = {cs_copyrawtexture2d_c};
	static sce::Gnmx::Toolkit::EmbeddedCsShader s_copyRawTexture3d = {cs_copyrawtexture3d_c};
	static sce::Gnmx::Toolkit::EmbeddedPsShader s_pixel = {pix_clear_p};
	static sce::Gnmx::Toolkit::EmbeddedPsShader s_generateMipMaps_2d = {pix_generateMipMaps_2d_p};
	static sce::Gnmx::Toolkit::EmbeddedPsShader s_generateMipMaps_3d = {pix_generateMipMaps_3d_p};
	static sce::Gnmx::Toolkit::EmbeddedPsShader s_copyTexture2d_p = {copytexture2d_p};
	static sce::Gnmx::Toolkit::EmbeddedVsShader s_vex_clear = {vex_clear_vv};

	static sce::Gnmx::Toolkit::EmbeddedCsShader *s_embeddedCsShader[] =
	{
		&s_copyTexture1d,  	 &s_copyTexture2d,    &s_copyTexture3d,
		&s_copyRawTexture1d, &s_copyRawTexture2d, &s_copyRawTexture3d, 		
		&s_copyBuffer,		 &s_set_uint,         &s_set_uint_fast,
	};
	static sce::Gnmx::Toolkit::EmbeddedPsShader *s_embeddedPsShader[] = {&s_pixel, &s_copyTexture2d_p, &s_generateMipMaps_2d, &s_generateMipMaps_3d};
	static sce::Gnmx::Toolkit::EmbeddedVsShader *s_embeddedVsShader[] = {&s_vex_clear};
	static sce::Gnmx::Toolkit::EmbeddedShaders s_embeddedShaders =
	{
		s_embeddedCsShader, sizeof(s_embeddedCsShader)/sizeof(s_embeddedCsShader[0]),
		s_embeddedPsShader, sizeof(s_embeddedPsShader)/sizeof(s_embeddedPsShader[0]),
		s_embeddedVsShader, sizeof(s_embeddedVsShader)/sizeof(s_embeddedVsShader[0]),
	};
};

void sce::Gnmx::Toolkit::SurfaceUtil::fillDwordsWithCpu(uint32_t* address, uint32_t dwords, uint32_t val)
{
	std::fill(address, address+dwords, val);
}

void sce::Gnmx::Toolkit::SurfaceUtil::fillDwordsWithDma(GnmxGfxContext &gfxc, void *address, uint32_t dwords, uint32_t val)
{
	gfxc.fillData(address, val, dwords*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearMemoryToUints(GnmxGfxContext &gfxc, void *destination, uint32_t destUints, const uint32_t *source, uint32_t srcUints)
{
	const bool srcUintsIsPowerOfTwo = (srcUints & (srcUints-1)) == 0;

	gfxc.setCsShader(srcUintsIsPowerOfTwo ? s_set_uint_fast.m_shader : s_set_uint.m_shader, 
						  srcUintsIsPowerOfTwo ? &s_set_uint_fast.m_offsetsTable : &s_set_uint.m_offsetsTable);

	Gnm::Buffer destinationBuffer;
	destinationBuffer.initAsDataBuffer(destination, Gnm::kDataFormatR32Uint, destUints);
	destinationBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
	gfxc.setRwBuffers(Gnm::kShaderStageCs, 0, 1, &destinationBuffer);

	Gnm::Buffer sourceBuffer;
	sourceBuffer.initAsDataBuffer(const_cast<uint32_t*>(source), Gnm::kDataFormatR32Uint, srcUints);
	sourceBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	gfxc.setBuffers(Gnm::kShaderStageCs, 0, 1, &sourceBuffer);

	struct Constants
	{
		uint32_t m_destUints;
		uint32_t m_srcUints;
	};
	Constants *constants = (Constants*)gfxc.allocateFromCommandBuffer(sizeof(Constants), Gnm::kEmbeddedDataAlignment4);
	constants->m_destUints = destUints;
	constants->m_srcUints = srcUints - (srcUintsIsPowerOfTwo ? 1 : 0);
	Gnm::Buffer constantBuffer;
	constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
	gfxc.setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &constantBuffer);

	gfxc.dispatch((destUints + Gnm::kThreadsPerWavefront - 1) / Gnm::kThreadsPerWavefront, 1, 1);

	Toolkit::synchronizeComputeToGraphics(&gfxc.m_dcb);
}

void sce::Gnmx::Toolkit::SurfaceUtil::fillDwordsWithCompute(GnmxGfxContext &gfxc, void *address, uint32_t dwords, uint32_t val)
{
	uint32_t *source = static_cast<uint32_t*>(gfxc.allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment4));
	*source = val;
	clearMemoryToUints(gfxc, address, dwords, source, 1);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearBuffer(GnmxGfxContext &gfxc, const Gnm::Buffer* buffer, uint32_t *source, uint32_t sourceUints)
{
	clearMemoryToUints(gfxc, buffer->getBaseAddress(), buffer->getSize() / sizeof(uint32_t), source, sourceUints);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearBuffer(GnmxGfxContext &gfxc, const Gnm::Buffer* buffer, const Vector4 &vector)
{
	uint32_t *source = static_cast<uint32_t*>(gfxc.allocateFromCommandBuffer(sizeof(uint32_t) * 4, Gnm::kEmbeddedDataAlignment4));
	uint32_t dwords = 0;
	Gnmx::Toolkit::dataFormatEncoder(source, &dwords, (Reg32*)&vector, buffer->getDataFormat());
	clearBuffer(gfxc, buffer, source, dwords);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearTexture(GnmxGfxContext &gfxc, const Gnm::Texture* texture, uint32_t *source, uint32_t sourceUints)
{
	uint64_t totalTiledSize = 0;
	Gnm::AlignmentType totalTiledAlign;
	int32_t status = GpuAddress::computeTotalTiledTextureSize(&totalTiledSize, &totalTiledAlign, texture);
	SCE_GNM_ASSERT(status == GpuAddress::kStatusSuccess);
	clearMemoryToUints(gfxc, texture->getBaseAddress(), totalTiledSize / sizeof(uint32_t), source, sourceUints);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearTexture(GnmxGfxContext &gfxc, const Gnm::Texture* texture, const Vector4 &color)
{
	uint32_t *source = static_cast<uint32_t*>(gfxc.allocateFromCommandBuffer(sizeof(uint32_t) * 4, Gnm::kEmbeddedDataAlignment4));
	uint32_t dwords = 0;
	Gnmx::Toolkit::dataFormatEncoder(source, &dwords, (Reg32*)&color, texture->getDataFormat());
	clearTexture(gfxc, texture, source, dwords);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(GnmxGfxContext &gfxc, const Gnm::RenderTarget* renderTarget, uint32_t *source, uint32_t sourceUints)
{
	// NOTE: this slice count is only valid if the array view hasn't changed since initialization!
	const uint32_t numSlices = renderTarget->getLastArraySliceIndex() - renderTarget->getBaseArraySliceIndex() + 1;
	clearMemoryToUints(gfxc, renderTarget->getBaseAddress(), renderTarget->getSliceSizeInBytes()*numSlices / sizeof(uint32_t), source, sourceUints);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(GnmxGfxContext &gfxc, const Gnm::RenderTarget* renderTarget, const Vector4 &color)
{
	uint32_t *source = static_cast<uint32_t*>(gfxc.allocateFromCommandBuffer(sizeof(uint32_t) * 4, Gnm::kEmbeddedDataAlignment4));
	uint32_t dwords = 0;
	Gnmx::Toolkit::dataFormatEncoder(source, &dwords, (Reg32*)&color, renderTarget->getDataFormat());
	clearRenderTarget(gfxc, renderTarget, source, dwords);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearDccBuffer(GnmxGfxContext &gfxc, const sce::Gnm::RenderTarget* renderTarget, const uint8_t dccKey)
{
	auto dword = static_cast<uint32_t*>(gfxc.allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment4));
    *dword = uint32_t(dccKey) * 0x01010101;

    const uint32_t baseSlice      = renderTarget->getBaseArraySliceIndex();
    const uint32_t lastSlice      = renderTarget->getLastArraySliceIndex();
	const uint32_t slices         = lastSlice - baseSlice + 1;
    const uint32_t bytesPerSlice  = renderTarget->getDccSliceSizeInBytes();
    const uint32_t dwordsPerSlice = (bytesPerSlice + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    const uint32_t dwords         = dwordsPerSlice * slices;
    const uint32_t byteOffset     = baseSlice * bytesPerSlice;
    uint8_t *baseAddress          = static_cast<uint8_t*>(renderTarget->getDccAddress());
	clearMemoryToUints(gfxc, baseAddress + byteOffset, dwords, dword, 1);
}

void sce::Gnmx::Toolkit::SurfaceUtil::copyBuffer(GnmxGfxContext &gfxc, const Gnm::Buffer* bufferDst, const Gnm::Buffer* bufferSrc)
{
	SCE_GNM_ASSERT_MSG(bufferDst != 0 && bufferSrc != 0, "bufferDst and bufferSrc must not be NULL.");
	SCE_GNM_ASSERT_MSG(s_initialized, "Must call SurfaceUtil::initialize() before calling this function.");
	SCE_GNM_ASSERT_MSG(bufferDst->getResourceMemoryType() != Gnm::kResourceMemoryTypeRO, "bufferDst can't have memory type Gnm::kResourceMemoryTypeRO");
	SCE_GNM_ASSERT(s_copyBuffer.m_shader);
	SCE_GNM_ASSERT(bufferDst->getNumElements() == bufferSrc->getNumElements());

	gfxc.setBuffers(Gnm::kShaderStageCs, 0, 1, bufferSrc);
	gfxc.setRwBuffers(Gnm::kShaderStageCs, 0, 1, bufferDst);

	gfxc.setCsShader(s_copyBuffer.m_shader, &s_copyBuffer.m_offsetsTable);

	gfxc.dispatch((bufferDst->getNumElements() + Gnm::kThreadsPerWavefront - 1) / Gnm::kThreadsPerWavefront, 1, 1);

	Toolkit::synchronizeComputeToGraphics(&gfxc.m_dcb);
}

static inline uint32_t fastIntLog2(uint32_t i)
{
#ifdef __ORBIS__
    return 31 - __builtin_clz(i | 1);
#else
	unsigned long temp;
	_BitScanReverse(&temp, i | 1);
    return temp;
#endif
}
static inline uint32_t getMaximumMipLevelCount(const Gnm::Texture *texture)
{
	switch(texture->getTextureType())
	{
	case Gnm::kTextureType2dMsaa:
	case Gnm::kTextureType2dArrayMsaa:
		return 1; // MSAA textures can't have mips
	case Gnm::kTextureType3d:
		return 1+::fastIntLog2(std::max(std::max(texture->getWidth(), texture->getHeight()), texture->getDepth()));
	default:
		return 1+::fastIntLog2(         std::max(texture->getWidth(), texture->getHeight())                      );
	}
}

void sce::Gnmx::Toolkit::SurfaceUtil::copyTexture(GnmxGfxContext &gfxc, const Gnm::Texture* textureDst, const Gnm::Texture* textureSrc)
{
	SCE_GNM_ASSERT_MSG(textureDst != 0 && textureSrc != 0, "textureDst and textureSrc must not be NULL.");
	SCE_GNM_ASSERT_MSG(s_initialized, "Must call SurfaceUtil::initialize() before calling this function.");
	SCE_GNM_ASSERT_MSG(s_copyTexture2d.m_shader, "embedded copytexture2d shader not found.");
	SCE_GNM_ASSERT_MSG(textureDst->getWidth()  == textureSrc->getWidth(), "source and destination texture widths do not match (dest=%d, source=%d).", textureDst->getWidth(), textureSrc->getWidth());
	SCE_GNM_ASSERT_MSG(textureDst->getLastMipLevel() - textureDst->getBaseMipLevel() <= textureSrc->getLastMipLevel() - textureSrc->getBaseMipLevel(),
		"textureDst can not have more mip levels than textureSrc.");
	SCE_GNM_ASSERT_MSG(textureDst->getLastArraySliceIndex() - textureDst->getBaseArraySliceIndex() <= textureSrc->getLastArraySliceIndex() - textureSrc->getBaseArraySliceIndex(),
		"textureDst can not have more array slices than textureSrc.");

	// If the data formats of the two textures are identical, we use a different shader that loads and stores the raw pixel bits directly, without any format conversion.
	// This not only preserves precision, but allows some additional tricks (such as copying otherwise-unwritable block-compressed formats by "spoofing" them as writable formats with identical
	// per-pixel sizes).
	bool copyRawPixels = (textureDst->getDataFormat().m_asInt == textureSrc->getDataFormat().m_asInt);

	Gnm::Texture textureDstCopy = *textureDst;
	Gnm::Texture textureSrcCopy = *textureSrc;

    Gnm::Texture *textures[] = {&textureSrcCopy, &textureDstCopy};
    for(auto &t : textures)
    {
		Gnm::DataFormat dataFormat = t->getDataFormat();
        if(copyRawPixels && dataFormat.isBlockCompressedFormat())
        {
		    dataFormat.m_bits.m_channelType = Gnm::kTextureChannelTypeUInt;
		    dataFormat.m_bits.m_channelX = Gnm::kTextureChannelX;
		    dataFormat.m_bits.m_channelY = Gnm::kTextureChannelY;
		    if(8 == dataFormat.getTotalBytesPerElement())
		    {
				dataFormat.m_bits.m_channelZ = Gnm::kTextureChannelConstant0;
				dataFormat.m_bits.m_channelW = Gnm::kTextureChannelConstant0;
				dataFormat.m_bits.m_surfaceFormat = Gnm::kSurfaceFormat32_32;
            }
            else
            {
				dataFormat.m_bits.m_channelZ = Gnm::kTextureChannelZ;
				dataFormat.m_bits.m_channelW = Gnm::kTextureChannelW;
				dataFormat.m_bits.m_surfaceFormat = Gnm::kSurfaceFormat32_32_32_32;
		    }       
            t->setDataFormat(dataFormat);
		    t->setWidthMinus1 ((t->getWidth()  + 3) / 4 - 1);
		    t->setPitchMinus1 ((t->getPitch()  + 3) / 4 - 1);
		    t->setHeightMinus1((t->getHeight() + 3) / 4 - 1);
        }
        const uint32_t maxMipLevel = getMaximumMipLevelCount(t) - 1;
        const uint32_t baseMipLevel =                        std::min(t->getBaseMipLevel(), maxMipLevel);
        const uint32_t lastMipLevel = std::max(baseMipLevel, std::min(t->getLastMipLevel(), maxMipLevel));
        t->setMipLevelRange(baseMipLevel, lastMipLevel);
    }

	EmbeddedCsShader *shader = 0;

	switch(textureDst->getTextureType())
	{
	case Gnm::kTextureType1d:
	case Gnm::kTextureType1dArray:
		SCE_GNM_ASSERT_MSG(textureSrc->getTextureType() == Gnm::kTextureType1d || textureSrc->getTextureType() == Gnm::kTextureType1dArray,
			"textureDst and textureSrc must have the same dimensionality (dst=0x%02X, src=0x%02X).", textureDst->getTextureType(), textureSrc->getTextureType());
		shader = copyRawPixels ? &s_copyRawTexture1d : &s_copyTexture1d;
		break;
	case Gnm::kTextureTypeCubemap:
		// Spoof the cubemap textures as 2D texture arrays.
		Gnmx::Toolkit::initAs2dArray(&textureDstCopy, textureDstCopy.getWidth(), textureDstCopy.getHeight(), textureDstCopy.getLastArraySliceIndex()+1, textureDstCopy.getLastMipLevel()+1, textureDstCopy.getDataFormat(), textureDstCopy.getTileMode(), textureDstCopy.getNumFragments(), false);
		Gnmx::Toolkit::initAs2dArray(&textureSrcCopy, textureSrcCopy.getWidth(), textureSrcCopy.getHeight(), textureSrcCopy.getLastArraySliceIndex()+1, textureSrcCopy.getLastMipLevel()+1, textureSrcCopy.getDataFormat(), textureSrcCopy.getTileMode(), textureSrcCopy.getNumFragments(), false);
		textureDstCopy.setBaseAddress(textureDst->getBaseAddress());
		textureSrcCopy.setBaseAddress(textureSrc->getBaseAddress());
		// Intentional fall-through
	case Gnm::kTextureType2d:
	case Gnm::kTextureType2dArray:
		SCE_GNM_ASSERT_MSG(textureDst->getHeight() == textureSrc->getHeight(), "source and destination texture heights do not match (dest=%d, source=%d).", textureDst->getHeight(), textureSrc->getHeight());
		SCE_GNM_ASSERT_MSG(textureSrc->getTextureType() == Gnm::kTextureType2d || textureSrc->getTextureType() == Gnm::kTextureType2dArray || textureSrc->getTextureType() == Gnm::kTextureTypeCubemap,
			"textureDst and textureSrc must have the same dimensionality (dst=0x%02X, src=0x%02X).", textureDst->getTextureType(), textureSrc->getTextureType());
		shader = copyRawPixels ? &s_copyRawTexture2d : &s_copyTexture2d;
		break;
	case Gnm::kTextureType3d:
		SCE_GNM_ASSERT_MSG(textureDst->getHeight() == textureSrc->getHeight(), "source and destination texture heights do not match (dest=%d, source=%d).", textureDst->getHeight(), textureSrc->getHeight());
		SCE_GNM_ASSERT_MSG(textureDst->getDepth() == textureSrc->getDepth(), "source and destination texture depths do not match (dest=%d, source=%d).", textureDst->getDepth(), textureSrc->getDepth());
		SCE_GNM_ASSERT_MSG(textureSrc->getTextureType() == Gnm::kTextureType3d,
			"textureDst and textureSrc must have the same dimensionality (dst=0x%02X, src=0x%02X).", textureDst->getTextureType(), textureSrc->getTextureType());
		shader = copyRawPixels ? &s_copyRawTexture3d : &s_copyTexture3d;
		break;
	default:
		break; // unsupported texture type -- handled below
	}
	if(shader == 0)
	{
		SCE_GNM_ERROR("textureDst's dimensionality (0x%02X) is not supported by this function.", textureDst->getTextureType());
		return;
	}

	gfxc.setCsShader(shader->m_shader, &shader->m_offsetsTable);

	textureDstCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // The destination texture is GPU-coherent, because we will write to it.
	textureSrcCopy.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // The source texture is read-only, because we'll only ever read from it.

	const uint32_t oldDstMipBase   = textureDstCopy.getBaseMipLevel();
	const uint32_t oldDstMipLast   = textureDstCopy.getLastMipLevel();
	const uint32_t oldDstSliceBase = textureDstCopy.getBaseArraySliceIndex();
	const uint32_t oldDstSliceLast = textureDstCopy.getLastArraySliceIndex();
	for(uint32_t iMip=oldDstMipBase; iMip <= oldDstMipLast; ++iMip)
	{
		textureSrcCopy.setMipLevelRange(iMip, iMip);
		textureDstCopy.setMipLevelRange(iMip, iMip);
		const uint32_t mipWidth  = std::max(textureDstCopy.getWidth() >> iMip, 1U);
		const uint32_t mipHeight = std::max(textureDstCopy.getHeight() >> iMip, 1U);
		const uint32_t mipDepth  = std::max(textureDstCopy.getDepth() >> iMip, 1U);
		for(uint32_t iSlice=oldDstSliceBase; iSlice <= oldDstSliceLast; ++iSlice)
		{
			textureSrcCopy.setArrayView(iSlice, iSlice);
			textureDstCopy.setArrayView(iSlice, iSlice);

			gfxc.setTextures( Gnm::kShaderStageCs, 0, 1, &textureSrcCopy );
			gfxc.setRwTextures( Gnm::kShaderStageCs, 0, 1, &textureDstCopy );

			switch(textureDstCopy.getTextureType())
			{
			case Gnm::kTextureType1d:
			case Gnm::kTextureType1dArray:
				gfxc.dispatch( (mipWidth+63)/64, 1, 1);
				break;
			case Gnm::kTextureTypeCubemap:
			case Gnm::kTextureType2d:
			case Gnm::kTextureType2dArray:
				gfxc.dispatch( (mipWidth+7)/8, (mipHeight+7)/8, 1);
				break;
			case Gnm::kTextureType3d:
				gfxc.dispatch( (mipWidth+3)/4, (mipHeight+3)/4, (mipDepth+3)/4 );
				break;
			default:
				SCE_GNM_ASSERT(0); // This path should have been caught in the previous switch statement
				return;
			}
		}
	}

	Toolkit::synchronizeComputeToGraphics(&gfxc.m_dcb);
}

void sce::Gnmx::Toolkit::SurfaceUtil::copyTextureToRenderTarget(GnmxGfxContext &gfxc, const sce::Gnm::RenderTarget* renderTargetDestination, const sce::Gnm::Texture* textureSource)
{
	gfxc.setRenderTarget(0, renderTargetDestination);
	Gnm::BlendControl blendControl;
	blendControl.init();
	blendControl.setBlendEnable(false);
	gfxc.setBlendControl(0, blendControl);
	gfxc.setRenderTargetMask(0x0000000F); // enable MRT0 output only
	gfxc.setDepthStencilDisable();
	gfxc.setupScreenViewport(0, 0, renderTargetDestination->getWidth(), renderTargetDestination->getHeight(), 0.5f, 0.5f);
	gfxc.setPsShader(s_copyTexture2d_p.m_shader, &s_copyTexture2d_p.m_offsetsTable);
	gfxc.setTextures(Gnm::kShaderStagePs, 0, 1, textureSource);
	Gnmx::renderFullScreenQuad(&gfxc);
}

void sce::Gnmx::Toolkit::SurfaceUtil::copyRenderTarget(GnmxGfxContext &gfxc, const Gnm::RenderTarget* renderTargetDst, const Gnm::RenderTarget* renderTargetSrc)
{
	SCE_GNM_ASSERT(renderTargetSrc->getWidth() == renderTargetDst->getWidth() && renderTargetSrc->getHeight() == renderTargetDst->getHeight());
	Gnm::Texture textureSrc;
	textureSrc.initFromRenderTarget(renderTargetSrc, false); // TODO: we sure this isn't a cubemap?
	textureSrc.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // We aren't going to do anything to this texture, except read from it. So let's mark it as read-only.
	copyTextureToRenderTarget(gfxc, renderTargetDst, &textureSrc);
}

void sce::Gnmx::Toolkit::SurfaceUtil::copyDepthTargetZ(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget* depthTargetDst, const sce::Gnm::DepthRenderTarget* depthTargetSrc)
{
	// NOTE: this slice count is only valid if the array view hasn't changed since initialization!
	uint32_t dstNumSlices = depthTargetDst->getLastArraySliceIndex() - depthTargetDst->getBaseArraySliceIndex() + 1;
	uint32_t srcNumSlices = depthTargetSrc->getLastArraySliceIndex() - depthTargetSrc->getBaseArraySliceIndex() + 1;
	SCE_GNM_ASSERT(dstNumSlices >= srcNumSlices);
	SCE_GNM_ASSERT(depthTargetSrc->getZFormat() == depthTargetDst->getZFormat() && depthTargetSrc->getZSliceSizeInBytes() == depthTargetDst->getZSliceSizeInBytes() && !(depthTargetDst->getZSliceSizeInBytes() & 0xFF));
	synchronizeDepthRenderTargetZGraphicsToCompute(&gfxc.m_dcb, depthTargetDst);
	synchronizeDepthRenderTargetZGraphicsToCompute(&gfxc.m_dcb, depthTargetSrc);
	Gnm::Buffer bufferDst, bufferSrc;
	bufferDst.initAsDataBuffer(depthTargetDst->getZWriteAddress(), Gnm::kDataFormatR32G32B32A32Uint, (depthTargetDst->getZSliceSizeInBytes()*dstNumSlices+15)/16);
	bufferDst.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we'll bind this as RWTexture, so it's GPU coherent.
	bufferSrc.initAsDataBuffer(depthTargetSrc->getZReadAddress(),  Gnm::kDataFormatR32G32B32A32Uint, (depthTargetSrc->getZSliceSizeInBytes()*srcNumSlices+15)/16);
	bufferSrc.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we won't bind this as RWTexture, so read-only is OK
	copyBuffer(gfxc, &bufferDst, &bufferSrc);
}

void sce::Gnmx::Toolkit::initializeWithAllocators(sce::Gnmx::Toolkit::Allocators *allocators)
{
	s_embeddedShaders.initializeWithAllocators(allocators);
	s_initialized = true;
}

static void clearDepthStencil(sce::Gnmx::GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget)
{
	SCE_GNM_ASSERT_MSG(s_initialized, "Must call SurfaceUtil::initialize() before calling this function.");
	SCE_GNM_ASSERT(s_pixel.m_shader);

	gfxc.setRenderTargetMask(0x0);

	gfxc.setPsShader(s_pixel.m_shader, &s_pixel.m_offsetsTable);

	Vector4Unaligned* constantBuffer = static_cast<Vector4Unaligned*>(gfxc.allocateFromCommandBuffer(sizeof(Vector4Unaligned), Gnm::kEmbeddedDataAlignment4));
	*constantBuffer = Vector4(0.f, 0.f, 0.f, 0.f);
	Gnm::Buffer buffer;
	buffer.initAsConstantBuffer(constantBuffer, sizeof(Vector4Unaligned));
	buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
	gfxc.setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &buffer);

	const uint32_t width = depthTarget->getWidth();
	const uint32_t height = depthTarget->getHeight();
	gfxc.setupScreenViewport(0, 0, width, height, 0.5f, 0.5f);
	const uint32_t firstSlice = depthTarget->getBaseArraySliceIndex();
	const uint32_t lastSlice  = depthTarget->getLastArraySliceIndex();
	Gnm::DepthRenderTarget dtCopy = *depthTarget;
	for(uint32_t iSlice=firstSlice; iSlice<=lastSlice; ++iSlice)
	{
		dtCopy.setArrayView(iSlice, iSlice);
		gfxc.setDepthRenderTarget(&dtCopy);
		Gnmx::renderFullScreenQuad(&gfxc);
	}

	gfxc.setRenderTargetMask(0xF);

	Gnm::DbRenderControl dbRenderControl;
	dbRenderControl.init();
	gfxc.setDbRenderControl(dbRenderControl);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearDepthStencilTarget(GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget, const float depthValue, const uint8_t stencilValue)
{
	SCE_GNM_ASSERT_MSG(depthTarget->getStencilReadAddress() != NULL, "Must have a stencil buffer to clear.");
	gfxc.pushMarker("Toolkit::SurfaceUtil::clearDepthStencilTarget");

	Gnm::DbRenderControl dbRenderControl;
	dbRenderControl.init();
	dbRenderControl.setDepthClearEnable(true);
	dbRenderControl.setStencilClearEnable(true);
	gfxc.setDbRenderControl(dbRenderControl);

	Gnm::DepthStencilControl depthControl;
	depthControl.init();
	depthControl.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncAlways);
	depthControl.setStencilFunction(Gnm::kCompareFuncAlways);
	depthControl.setDepthEnable(true);
	depthControl.setStencilEnable(true);
	gfxc.setDepthStencilControl(depthControl);

	Gnm::StencilOpControl stencilOpControl;
	stencilOpControl.init();
	stencilOpControl.setStencilOps(Gnm::kStencilOpReplaceTest, Gnm::kStencilOpReplaceTest, Gnm::kStencilOpReplaceTest);
	gfxc.setStencilOpControl(stencilOpControl);
	const Gnm::StencilControl stencilControl = {0xff, 0xff, 0xff, 0xff};
	gfxc.setStencil(stencilControl);

	gfxc.setDepthClearValue(depthValue);
	gfxc.setStencilClearValue(stencilValue);

	clearDepthStencil(gfxc, depthTarget);

	gfxc.popMarker();
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearStencilTarget(GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget, const uint8_t stencilValue)
{
	SCE_GNM_ASSERT_MSG(depthTarget->getStencilReadAddress() != NULL, "Must have a stencil buffer to clear.");
	gfxc.pushMarker("Toolkit::SurfaceUtil::clearStencilTarget");

	Gnm::DbRenderControl dbRenderControl;
	dbRenderControl.init();
	dbRenderControl.setStencilClearEnable(true);
	gfxc.setDbRenderControl(dbRenderControl);

	Gnm::DepthStencilControl depthControl;
	depthControl.init();
	depthControl.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways);
	depthControl.setStencilFunction(Gnm::kCompareFuncAlways);
	depthControl.setDepthEnable(true);
	depthControl.setStencilEnable(true);
	gfxc.setDepthStencilControl(depthControl);

	Gnm::StencilOpControl stencilOpControl;
	stencilOpControl.init();
	stencilOpControl.setStencilOps(Gnm::kStencilOpReplaceTest, Gnm::kStencilOpReplaceTest, Gnm::kStencilOpReplaceTest);
	gfxc.setStencilOpControl(stencilOpControl);
	const Gnm::StencilControl stencilControl = {0xff, 0xff, 0xff, 0xff};
	gfxc.setStencil(stencilControl);

	gfxc.setStencilClearValue(stencilValue);

	clearDepthStencil(gfxc, depthTarget);

	gfxc.popMarker();
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget, const float depthValue)
{
	gfxc.pushMarker("Toolkit::SurfaceUtil::clearDepthTarget");

	Gnm::DbRenderControl dbRenderControl;
	dbRenderControl.init();
	dbRenderControl.setDepthClearEnable(true);
	gfxc.setDbRenderControl(dbRenderControl);

	Gnm::DepthStencilControl depthControl;
	depthControl.init();
	depthControl.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncAlways);
	depthControl.setStencilFunction(Gnm::kCompareFuncNever);
	depthControl.setDepthEnable(true);
	gfxc.setDepthStencilControl(depthControl);

	gfxc.setDepthClearValue(depthValue);

	clearDepthStencil(gfxc, depthTarget);

	gfxc.popMarker();
}

void sce::Gnmx::Toolkit::synchronizeRenderTargetGraphicsToCompute(sce::Gnmx::GnmxDrawCommandBuffer *dcb, const sce::Gnm::RenderTarget* renderTarget)
{
	// NOTE: this slice count is only valid if the array view hasn't changed since initialization!
	uint32_t numSlices = renderTarget->getLastArraySliceIndex() - renderTarget->getBaseArraySliceIndex() + 1;
	dcb->waitForGraphicsWrites(renderTarget->getBaseAddress256ByteBlocks(), (renderTarget->getSliceSizeInBytes()*numSlices)>>8,
		Gnm::kWaitTargetSlotCb0 | Gnm::kWaitTargetSlotCb1 | Gnm::kWaitTargetSlotCb2 | Gnm::kWaitTargetSlotCb3 |
		Gnm::kWaitTargetSlotCb4 | Gnm::kWaitTargetSlotCb5 | Gnm::kWaitTargetSlotCb6 | Gnm::kWaitTargetSlotCb7,
		Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);
}

void sce::Gnmx::Toolkit::synchronizeDepthRenderTargetZGraphicsToCompute(sce::Gnmx::GnmxDrawCommandBuffer *dcb, const sce::Gnm::DepthRenderTarget* depthTarget)
{
	// NOTE: this slice count is only valid if the array view hasn't changed since initialization!
	uint32_t numSlices = depthTarget->getLastArraySliceIndex() - depthTarget->getBaseArraySliceIndex() + 1;
	dcb->waitForGraphicsWrites(depthTarget->getZWriteAddress256ByteBlocks(), (depthTarget->getZSliceSizeInBytes()*numSlices)>>8,
		Gnm::kWaitTargetSlotDb,
		Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateDbCache, Gnm::kStallCommandBufferParserDisable);
}

void sce::Gnmx::Toolkit::synchronizeGraphicsToCompute( sce::Gnmx::GnmxDrawCommandBuffer *dcb )
{
	volatile uint64_t* label = (volatile uint64_t*)dcb->allocateFromCommandBuffer(sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8); // allocate memory from the command buffer
	uint32_t zero = 0;
	dcb->writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
	// This EOP event does the following:
	// - wait for all rendering to complete
	// - flush and invalidate the GPU's CB, DB, L1, and L2 caches
	// - write 0x1 to the specified label address.
	dcb->writeAtEndOfPipe(
		Gnm::kEopFlushCbDbCaches, 
		Gnm::kEventWriteDestMemory, const_cast<uint64_t*>(label),
		Gnm::kEventWriteSource64BitsImmediate, 0x1,
		Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru
	);
	dcb->waitOnAddress(const_cast<uint64_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1); // tell the CP to wait until the memory has the val 1
}

void sce::Gnmx::Toolkit::synchronizeComputeToGraphics( sce::Gnmx::GnmxDrawCommandBuffer *dcb )
{
	volatile uint64_t* label = (volatile uint64_t*)dcb->allocateFromCommandBuffer( sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8 ); // allocate memory from the command buffer
	uint32_t zero = 0;
	dcb->writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
	dcb->writeAtEndOfShader( Gnm::kEosCsDone, const_cast<uint64_t*>(label), 0x1 ); // tell the CP to write a 1 into the memory only when all compute shaders have finished
	dcb->waitOnAddress( const_cast<uint64_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1 ); // tell the CP to wait until the memory has the val 1
	dcb->flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, 0, Gnm::kStallCommandBufferParserDisable); // tell the CP to flush the L1$ and L2$
}

void sce::Gnmx::Toolkit::synchronizeComputeToCompute( sce::Gnmx::GnmxDrawCommandBuffer *dcb )
{
	volatile uint64_t* label = (volatile uint64_t*)dcb->allocateFromCommandBuffer( sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8 ); // allocate memory from the command buffer
	uint32_t zero = 0;
	dcb->writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
	dcb->writeAtEndOfShader( Gnm::kEosCsDone, const_cast<uint64_t*>(label), 0x1 ); // tell the CP to write a 1 into the memory only when all compute shaders have finished
	dcb->waitOnAddress( const_cast<uint64_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1 ); // tell the CP to wait until the memory has the val 1
	dcb->flushShaderCachesAndWait(Gnm::kCacheActionInvalidateL1, 0, Gnm::kStallCommandBufferParserDisable); // tell the CP to flush the L1$, because presumably the consumers of compute shader output may run on different CUs
}

void sce::Gnmx::Toolkit::synchronizeComputeToCompute( sce::Gnmx::GnmxDispatchCommandBuffer *dcb )
{
	volatile uint64_t* label = (volatile uint64_t*)dcb->allocateFromCommandBuffer( sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8 ); // allocate memory from the command buffer
	uint32_t zero = 0;
	dcb->writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
	dcb->writeReleaseMemEvent(Gnm::kReleaseMemEventCsDone,
							  Gnm::kEventWriteDestMemory,  const_cast<uint64_t*>(label),
							  Gnm::kEventWriteSource64BitsImmediate, 0x1,
							  Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
	dcb->waitOnAddress( const_cast<uint64_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1 ); // tell the CP to wait until the memory has the val 1
	dcb->flushShaderCachesAndWait(Gnm::kCacheActionInvalidateL1, 0); // tell the CP to flush the L1$, because presumably the consumers of compute shader output may run on different CUs
}
void sce::Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget, const Gnm::Htile htile)
{
	gfxc.pushMarker("Toolkit::SurfaceUtil::clearHtileSurface");
	SCE_GNM_ASSERT_MSG(depthTarget->getHtileAddress() != NULL, "depthTarget (0x%p) has no HTILE surface.", depthTarget);
	gfxc.triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta);
    const auto baseSlice     = depthTarget->getBaseArraySliceIndex();
    const auto lastSlice     = depthTarget->getLastArraySliceIndex();
    const auto bytesPerSlice = depthTarget->getHtileSliceSizeInBytes();
    const auto baseAddress   = static_cast<uint8_t *>(depthTarget->getHtileAddress());
	const auto slices          = lastSlice - baseSlice + 1;
    const auto dwordsPerSlice  = bytesPerSlice / sizeof(uint32_t);
    const auto offsetInBytes   = baseSlice * bytesPerSlice;
    const auto dwordsToClear   = slices * dwordsPerSlice;
	fillDwordsWithCompute(gfxc, baseAddress + offsetInBytes, dwordsToClear, htile.m_asInt);
	synchronizeComputeToGraphics(&gfxc.m_dcb);
	gfxc.popMarker();
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget, const float minZ, const float maxZ)
{
	SCE_GNM_ASSERT_MSG(minZ >= 0.f && minZ <= 1.f, "minZ value of %f is not between 0 and 1", minZ);
	SCE_GNM_ASSERT_MSG(maxZ >= 0.f && maxZ <= 1.f, "maxZ value of %f is not between 0 and 1", maxZ);
	Gnm::Htile htile = {};
	htile.m_hiZ.m_zMask = 0;
	htile.m_hiZ.m_minZ = static_cast<uint32_t>(floorf(minZ * Gnm::Htile::kMaximumZValue));
	htile.m_hiZ.m_maxZ = static_cast<uint32_t>(ceilf(maxZ * Gnm::Htile::kMaximumZValue));
	clearHtileSurface(gfxc, depthTarget, htile);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget, const float z)
{
	clearHtileSurface(gfxc, depthTarget, z, z);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget)
{
	clearHtileSurface(gfxc, depthTarget, 1.f);
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearCmaskSurface(GnmxGfxContext &gfxc, const Gnm::RenderTarget *target)
{
	gfxc.pushMarker("Toolkit::SurfaceUtil::clearCmaskSurface");
	SCE_GNM_ASSERT_MSG(target->getCmaskAddress() != NULL, "target (0x%p) has no CMASK surface.", target);
	gfxc.triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta);
	// NOTE: this slice count is only valid if the array view hasn't changed since initialization!
	uint32_t numSlices = target->getLastArraySliceIndex() - target->getBaseArraySliceIndex() + 1;
	uint32_t clearValue = 0x00000000;
	if ( target->getDccCompressionEnable() && target->getCmaskFastClearEnable() )
    {
        if ( target->getFmaskCompressionEnable() )
            clearValue = 0xcccccccc; // CMask Fast Clear is disabled, FMask Compression Enabled
        else
            clearValue = 0xffffffff; // CMask Fast Clear is disabled.
    }
	fillDwordsWithCompute(gfxc, target->getCmaskAddress(), target->getCmaskSliceSizeInBytes()*numSlices/sizeof(uint32_t), clearValue);
	synchronizeComputeToGraphics(&gfxc.m_dcb);
	gfxc.popMarker();
}


void sce::Gnmx::Toolkit::SurfaceUtil::clearDccSurface(GnmxGfxContext &gfxc, const Gnm::RenderTarget *target, Gnm::DccClearValue dccClearValue)
{
	gfxc.pushMarker("Toolkit::SurfaceUtil::clearDccSurface");
	SCE_GNM_ASSERT_MSG(target->getDccAddress() != NULL, "target (0x%p) has no Dcc surface.", target);
	gfxc.triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta);
	gfxc.triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbPixelData);
	// NOTE: this slice count is only valid if the array view hasn't changed since initialization!
	uint32_t numSlices = target->getLastArraySliceIndex() - target->getBaseArraySliceIndex() + 1;
	fillDwordsWithCompute(gfxc, target->getDccAddress(), target->getDccSliceSizeInBytes()*numSlices/sizeof(uint32_t), dccClearValue);
	synchronizeComputeToGraphics(&gfxc.m_dcb);
	gfxc.popMarker();
}


void sce::Gnmx::Toolkit::SurfaceUtil::clearStencilSurface(GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget, const uint8_t value)
{
	gfxc.pushMarker("Toolkit::SurfaceUtil::clearStencilSurface");
	SCE_GNM_ASSERT_MSG(depthTarget->getStencilWriteAddress() != NULL, "depthTarget (0x%p) has no stencil surface.", depthTarget);
	gfxc.triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta);
	uint32_t dword = value;
	dword = (dword << 8) | dword;
	dword = (dword << 8) | dword;
	dword = (dword << 8) | dword;
	// NOTE: this slice count is only valid if the array view hasn't changed since initialization!
	uint32_t numSlices = depthTarget->getLastArraySliceIndex() - depthTarget->getBaseArraySliceIndex() + 1;
	fillDwordsWithCompute(gfxc, depthTarget->getStencilWriteAddress(), depthTarget->getStencilSliceSizeInBytes()*numSlices/sizeof(uint32_t), dword);
	synchronizeComputeToGraphics(&gfxc.m_dcb);
	gfxc.popMarker();
}

void sce::Gnmx::Toolkit::SurfaceUtil::clearStencilSurface(GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget)
{
	clearStencilSurface(gfxc, depthTarget, 0);
}

sce::Gnm::SizeAlign sce::Gnmx::Toolkit::Timers::getRequiredBufferSizeAlign(uint32_t timers)
{
	Gnm::SizeAlign result;
	result.m_align = sizeof(uint64_t); // timers are 64-bit
	result.m_size = timers * sizeof(Timer);
	return result;
}

void sce::Gnmx::Toolkit::Timers::initialize(void *addr, uint32_t timers)
{
	SCE_GNM_ASSERT_MSG((uintptr_t(addr) % 8)==0, "addr (0x%p) must be 8-byte-aligned.", addr);
	m_addr = addr;
	m_timer = static_cast<Timer*>(addr);
	m_timers = timers;
}

void sce::Gnmx::Toolkit::Timers::begin(GnmxDrawCommandBuffer& dcb, uint32_t timer)
{
	SCE_GNM_ASSERT_MSG(timer < m_timers, "timer index (%d) must be less than m_timers (%ld).", timer, m_timers);
	dcb.writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, &m_timer[timer].m_begin, Gnm::kEventWriteSourceGpuCoreClockCounter, 0, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
}

void sce::Gnmx::Toolkit::Timers::end(GnmxDrawCommandBuffer& dcb, uint32_t timer)
{
	SCE_GNM_ASSERT_MSG(timer < m_timers, "timer index (%d) must be less than m_timers (%ld).", timer, m_timers);
	dcb.writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, &m_timer[timer].m_end, Gnm::kEventWriteSourceGpuCoreClockCounter, 0, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
}

void sce::Gnmx::Toolkit::Timers::begin(GnmxDispatchCommandBuffer& dcb, uint32_t timer)
{
	SCE_GNM_ASSERT_MSG(timer < m_timers, "timer index (%d) must be less than m_timers (%ld).", timer, m_timers);
	dcb.writeReleaseMemEvent(Gnm::kReleaseMemEventFlushAndInvalidateCbDbCaches, Gnm::kEventWriteDestMemory, &m_timer[timer].m_begin, Gnm::kEventWriteSourceGpuCoreClockCounter, 0, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
}

void sce::Gnmx::Toolkit::Timers::end(GnmxDispatchCommandBuffer& dcb, uint32_t timer)
{
	SCE_GNM_ASSERT_MSG(timer < m_timers, "timer index (%d) must be less than m_timers (%ld).", timer, m_timers);
	dcb.writeReleaseMemEvent(Gnm::kReleaseMemEventFlushAndInvalidateCbDbCaches, Gnm::kEventWriteDestMemory, &m_timer[timer].m_end, Gnm::kEventWriteSourceGpuCoreClockCounter, 0, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
}

uint64_t sce::Gnmx::Toolkit::Timers::readTimerInGpuClocks(uint32_t timer) const
{
	SCE_GNM_ASSERT_MSG(timer < m_timers, "timer index (%d) must be less than m_timers (%ld).", timer, m_timers);
	uint64_t begin = m_timer[timer].m_begin & 0xFFFFFFFF;
	uint64_t end = m_timer[timer].m_end & 0xFFFFFFFF;
	if( end < begin )
		end += uint64_t(1) << 32;
	return end - begin;
}

double sce::Gnmx::Toolkit::Timers::readTimerInMilliseconds(uint32_t timer) const
{
	SCE_GNM_ASSERT_MSG(timer < m_timers, "timer index (%d) must be less than m_timers (%ld).", timer, m_timers);
	return 1000.0 * readTimerInGpuClocks(timer) / SCE_GNM_GPU_CORE_CLOCK_FREQUENCY;
}

int32_t sce::Gnmx::Toolkit::saveTextureToTga(const Gnm::Texture *texture, const char* filename)
{
	SCE_GNM_ASSERT_MSG(texture, "texture must not be a null pointer");
	SCE_GNM_ASSERT_MSG(texture->getDataFormat().m_asInt == Gnm::kDataFormatB8G8R8A8UnormSrgb.m_asInt, "texture format (%d) must be kDataFormatB8G8R8A8UnormSrgb", texture->getDataFormat().m_asInt);
	SCE_GNM_ASSERT_MSG(texture->getTileMode() == Gnm::kTileModeDisplay_LinearAligned, "texture tilemode (%d) must be kTileModeDisplay_LinearAligned", texture->getTileMode());

	FILE *file = fopen(filename, "wb");
	if(file == 0)
		return -1;

#pragma pack(push, 1)
    struct TargaHeader
    {
		uint8_t  idLength;          /* 00h  Size of Image ID field */
		uint8_t  colorMapType;      /* 01h  Color map type */
		uint8_t  imageType;         /* 02h  Image type code */
		uint16_t colorMapIndex;     /* 03h  Color map origin */
		uint16_t colorMapLength;    /* 05h  Color map length */
		uint8_t  colorMapBits;      /* 07h  Depth of color map entries */
		uint16_t xOrigin;           /* 08h  X origin of image */
		uint16_t yOrigin;           /* 0Ah  Y origin of image */
		uint16_t width;             /* 0Ch  Width of image */
		uint16_t height;            /* 0Eh  Height of image */
		uint8_t  pixelDepth;        /* 10h  Image pixel size */
		uint8_t  imageDescriptor;   /* 11h  bits 3-0 give the alpha channel depth, bits 5-4 give direction */

    };
#pragma pack(pop)

	TargaHeader header;
    memset(&header,0, sizeof(header));
    header.imageType = 2;
    header.width = (uint16_t)texture->getPitch(); // for simplicity assume pitch as width
    header.height = (uint16_t)texture->getHeight();
    header.pixelDepth = (uint8_t) texture->getDataFormat().getBitsPerElement();
    header.imageDescriptor = 0x20; // y direction is reversed (from up to down)
	fwrite(&header, sizeof(header), 1, file);

	const void *pixels = texture->getBaseAddress();

	fwrite(pixels, header.width*texture->getHeight()*4, 1, file);

	fclose(file);
	return 0;
}

int32_t sce::Gnmx::Toolkit::saveTextureToPfm(const Gnm::Texture *texture, const uint8_t* pixels, const char *fileName)
{
	//uint32_t df = texture->getDataFormat().m_asInt;
	SCE_GNM_ASSERT_MSG(texture != 0, "texture must not be NULL.");
	SCE_GNM_ASSERT_MSG(pixels != 0, "pixels must not be NULL.");
	SCE_GNM_ASSERT_MSG(texture->getDataFormat().m_asInt == Gnm::kDataFormatR32G32B32A32Float.m_asInt || texture->getDataFormat().m_asInt == Gnm::kDataFormatR32Float.m_asInt,
		"Texture data format must be RGBA32F or R32F.");
	SCE_GNM_ASSERT_MSG(texture->getTileMode() == Gnm::kTileModeDisplay_LinearAligned, "texture tilemode (%d) must be kTileModeDisplay_LinearAligned", texture->getTileMode());

	const uint32_t width = texture->getWidth();
	const uint32_t height = texture->getHeight();
	const uint32_t pitch = texture->getPitch();

	uint8_t format = 0;
	uint32_t bytesPerPixelIn = 0;
	uint32_t bytesPerPixelOut = 0;
	if(texture->getDataFormat().m_asInt == Gnm::kDataFormatR32G32B32A32Float.m_asInt)
	{
		format = 'F';
		bytesPerPixelIn = sizeof(float)*4;
		bytesPerPixelOut = sizeof(float)*3;
	}
	else if(texture->getDataFormat().m_asInt == Gnm::kDataFormatR32Float.m_asInt)
	{
		format = 'f';
		bytesPerPixelIn = sizeof(float)*1;
		bytesPerPixelOut = sizeof(float)*1;
	}
	else
	{
		SCE_GNM_ASSERT( !"PFM file requires float1 or float3 format." );
	}

	FILE *file = fopen(fileName, "wb");
	SCE_GNM_ASSERT(file != 0 && "PFM file can not be opened for writing.");
	fprintf(file, "P%c\n%d %d\n-1.0\n", format, texture->getWidth(), texture->getHeight());
	for( uint32_t scanline=0; scanline<height; ++scanline )
	{
		uint32_t byteOffset = 0;
		for( uint32_t pixel=0; pixel<width; ++pixel )
		{
			const size_t bytesWritten = fwrite(pixels+byteOffset, sizeof(uint8_t), bytesPerPixelOut, file);
			SCE_GNM_ASSERT(bytesWritten == bytesPerPixelOut);
			byteOffset += bytesPerPixelIn;
		}
		pixels += pitch*bytesPerPixelIn;
	}

	fclose(file);
	return 0;
}

void sce::Gnmx::Toolkit::submitAndStall(sce::Gnmx::GnmxDrawCommandBuffer& dcb)
{
	volatile uint64_t* label = static_cast<uint64_t*>(dcb.allocateFromCommandBuffer(sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8));
	*label = 0;
	dcb.writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, const_cast<uint64_t*>(label), Gnm::kEventWriteSource64BitsImmediate, 1, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
	void *cbAddrGPU    = dcb.m_beginptr;
	void *ccbAddrGPU   = 0;
	uint32_t cbSizeInByte = static_cast<uint32_t>(dcb.m_cmdptr - dcb.m_beginptr)*4;
	uint32_t ccbSizeInByte = 0;
	int state = Gnm::submitCommandBuffers(1, &cbAddrGPU, &cbSizeInByte, &ccbAddrGPU, &ccbSizeInByte);
	SCE_GNM_ASSERT(state == sce::Gnm::kSubmissionSuccess);

	volatile uint32_t wait = 0;
	while( *label != 1 )
		wait++;
}

void sce::Gnmx::Toolkit::submitAndStall(sce::Gnmx::GnmxGfxContext& gfxc)
{
	volatile uint64_t* label = static_cast<uint64_t*>(gfxc.allocateFromCommandBuffer(sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8));
	*label = 0;
	// TODO: expose release mem / remove eop
	gfxc.writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, const_cast<uint64_t*>(label), 1, Gnm::kCacheActionNone);
	int32_t state = gfxc.submit();
	SCE_GNM_ASSERT(state == sce::Gnm::kSubmissionSuccess);
	volatile uint32_t wait = 0;
	while( *label != 1 )
		wait++;
}

//////////

static off_t s_tfOffset = 0;

void* sce::Gnmx::Toolkit::allocateTessellationFactorRingBuffer()
{
	if ( !s_tfOffset )
	{
		const uint32_t tfAlignment = 64 * 1024;
		int	ret = sceKernelAllocateDirectMemory(0, SCE_KERNEL_MAIN_DMEM_SIZE,
												Gnm::kTfRingSizeInBytes, tfAlignment, 
												SCE_KERNEL_WC_GARLIC, &s_tfOffset);
	
		void *tfBufferPtr = sce::Gnm::getTessellationFactorRingBufferBaseAddress();
		if ( !ret )
		{
			ret = sceKernelMapDirectMemory(&tfBufferPtr,
										   Gnm::kTfRingSizeInBytes,
										   SCE_KERNEL_PROT_CPU_READ|SCE_KERNEL_PROT_CPU_WRITE|SCE_KERNEL_PROT_GPU_ALL,
										   0, //flags
										   s_tfOffset,
										   tfAlignment);
		}
		SCE_GNM_ASSERT_MSG(!ret, "Allocation of the tessellation factor ring buffer failed!");
		SCE_GNM_ASSERT_MSG(tfBufferPtr == sce::Gnm::getTessellationFactorRingBufferBaseAddress(), "Allocation of the tessellation factor ring buffer failed!");
	}

	return sce::Gnm::getTessellationFactorRingBufferBaseAddress();
}

void sce::Gnmx::Toolkit::deallocateTessellationFactorRingBuffer()
{
	if ( s_tfOffset )
	{
		sceKernelReleaseDirectMemory(s_tfOffset, Gnm::kTfRingSizeInBytes);
	}
	s_tfOffset = 0;
}

void sce::Gnmx::Toolkit::SurfaceUtil::generateMipMaps(GnmxGfxContext &gfxc, const sce::Gnm::Texture *texture)
{
	SCE_GNM_ASSERT_MSG(s_initialized, "Must call SurfaceUtil::initialize() before calling this function.");
	SCE_GNM_ASSERT_MSG(texture != 0, "texture must not be NULL.");

	gfxc.pushMarker("Toolkit::SurfaceUtil::generateMipMaps");

	gfxc.setDepthRenderTarget((Gnm::DepthRenderTarget*)0);

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
	gfxc.setDepthStencilDisable();

	gfxc.setVsShader(s_vex_clear.m_shader, 0, (void*)0, &s_vex_clear.m_offsetsTable);

	gfxc.setPrimitiveType(Gnm::kPrimitiveTypeRectList);

	uint64_t textureSizeInBytes;
	Gnm::AlignmentType textureAlignment;
	GpuAddress::computeTotalTiledTextureSize(&textureSizeInBytes, &textureAlignment, texture);

	struct Constants
	{
		Vector4Unaligned m_mul;
		Vector4Unaligned m_add;
	};

	if(texture->getTextureType() == Gnm::kTextureType3d)
		gfxc.setPsShader(s_generateMipMaps_3d.m_shader, &s_generateMipMaps_3d.m_offsetsTable);
	else
		gfxc.setPsShader(s_generateMipMaps_2d.m_shader, &s_generateMipMaps_2d.m_offsetsTable);

	Gnm::Sampler trilinearSampler;
	trilinearSampler.init();
	trilinearSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	trilinearSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
	gfxc.setSamplers(Gnm::kShaderStagePs, 0, 1, &trilinearSampler);

	for(unsigned targetMip = texture->getBaseMipLevel() + 1; targetMip <= texture->getLastMipLevel(); ++targetMip)
	{
		const unsigned sourceMip = targetMip - 1;

		const float ooTargetWidth  = 1.f / std::max(1U, texture->getWidth()  >> targetMip);
		const float ooTargetHeight = 1.f / std::max(1U, texture->getHeight() >> targetMip);

		if(texture->getTextureType() == Gnm::kTextureType3d)
		{
			const unsigned targetDepths = std::max(1U, texture->getDepth() >> targetMip);

			const float ooTargetDepth  = 1.f / targetDepths;

			for(unsigned targetSlice = 0; targetSlice < targetDepths; ++targetSlice)
			{
				Gnm::Texture sourceTexture = *texture;

				Gnm::RenderTarget destinationRenderTarget;
				destinationRenderTarget.initFromTexture(&sourceTexture, targetMip, (Gnm::NumSamples)sourceTexture.getNumFragments(), NULL, NULL);
				destinationRenderTarget.setArrayView(targetSlice, targetSlice);
				gfxc.setupScreenViewport(0, 0, destinationRenderTarget.getWidth(), destinationRenderTarget.getHeight(), 0.5f, 0.5f);
				gfxc.setRenderTarget(0, &destinationRenderTarget);

				sourceTexture.setMipLevelRange(sourceMip, sourceMip);
				gfxc.setTextures(Gnm::kShaderStagePs, 0, 1, &sourceTexture);

				Constants *constants = (Constants*)gfxc.allocateFromCommandBuffer(sizeof(Constants), Gnm::kEmbeddedDataAlignment4);
				constants->m_mul.x = ooTargetWidth;
				constants->m_mul.y = ooTargetHeight;
				constants->m_mul.z = 0.f;
				constants->m_add.x = 0.f;
				constants->m_add.y = 0.f;
				constants->m_add.z = (targetSlice + 0.5f) * ooTargetDepth;
				Gnm::Buffer constantBuffer;
				constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
				gfxc.setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

				gfxc.drawIndexAuto(3);
			}
		}
		else
		{
			for(unsigned slice = texture->getBaseArraySliceIndex(); slice <= texture->getLastArraySliceIndex(); ++slice)
			{
				Gnm::Texture sourceTexture = *texture;
				if(sourceTexture.getTextureType() == Gnm::kTextureTypeCubemap) // When spoofing a cubemap as a 2D array, we also need to multiply the total slice count (stored in the Depth field) by 6.
				{			
					sourceTexture.setTextureType(Gnm::kTextureType2dArray);
					sourceTexture.setDepthMinus1(sourceTexture.getDepth()*6 - 1);
				}

				Gnm::RenderTarget destinationRenderTarget;
				destinationRenderTarget.initFromTexture(&sourceTexture, targetMip, (Gnm::NumSamples)sourceTexture.getNumFragments(), NULL, NULL);
				destinationRenderTarget.setArrayView(slice, slice);
				gfxc.setupScreenViewport(0, 0, destinationRenderTarget.getWidth(), destinationRenderTarget.getHeight(), 0.5f, 0.5f);
				gfxc.setRenderTarget(0, &destinationRenderTarget);

				sourceTexture.setArrayView(slice, slice);
				sourceTexture.setMipLevelRange(sourceMip, sourceMip);
				gfxc.setTextures(Gnm::kShaderStagePs, 0, 1, &sourceTexture);

				Constants *constants = (Constants*)gfxc.allocateFromCommandBuffer(sizeof(Constants), Gnm::kEmbeddedDataAlignment4);
				constants->m_mul.x = ooTargetWidth;
				constants->m_mul.y = ooTargetHeight;
				constants->m_add.x = 0.f;
				constants->m_add.y = 0.f;
				Gnm::Buffer constantBuffer;
				constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
				gfxc.setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

				gfxc.drawIndexAuto(3);
			}
		}

		// Since each slice is supposed to share no cache lines with any other slice, it should be safe to have no synchronization
		// between slices. But, since each mip reads from the previous mip, we must do synchronization between mips.

		gfxc.waitForGraphicsWrites(
			texture->getBaseAddress256ByteBlocks(), 
			(textureSizeInBytes + 255) / 256,
			Gnm::kWaitTargetSlotCb0, 
			Gnm::kCacheActionWriteBackAndInvalidateL1andL2, 
			Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, 
			Gnm::kStallCommandBufferParserDisable
		);
	}

	// unbind all we have bound
#if !SCE_GNMX_ENABLE_GFX_LCUE // LCUE does not support unbinding of resources and NULL VS
	gfxc.setTextures(Gnm::kShaderStagePs, 0, 1, 0);
	gfxc.setSamplers(Gnm::kShaderStagePs, 0, 1, 0);
	gfxc.setVsShader(0, 0, (void*)0);
#endif //!SCE_GNMX_ENABLE_GFX_LCUE
	gfxc.setRenderTarget(0, 0);
	
	gfxc.setPsShader(0, 0);

}

uint64_t Gnmx::Toolkit::roundUpToAlignment(Gnm::Alignment alignment, uint64_t bytes)
{
	const uint64_t mask = alignment - 1;
	return (bytes + mask) & ~mask;
}

void *Gnmx::Toolkit::roundUpToAlignment(Gnm::Alignment alignment, void *addr)
{
	return reinterpret_cast<void*>(roundUpToAlignment(alignment, reinterpret_cast<uint64_t>(addr)));
}

sce::Gnm::SizeAlign Gnmx::Toolkit::initAs1d(sce::Gnm::Texture *texture,uint32_t width,uint32_t numMipLevels,sce::Gnm::DataFormat format,sce::Gnm::TileMode tileModeHint)
{
	sce::Gnm::TextureSpec spec;
	spec.init();
	spec.m_textureType = sce::Gnm::kTextureType1d;
	spec.m_width = width;
	spec.m_height = 1;
	spec.m_depth = 1;
	spec.m_pitch = 0;
	spec.m_numMipLevels = numMipLevels;
	spec.m_numSlices = 1;
	spec.m_format = format;
	spec.m_tileModeHint = tileModeHint;
	spec.m_minGpuMode = sce::Gnm::kGpuModeBase;
	spec.m_numFragments = sce::Gnm::kNumFragments1;
	int32_t status = texture->init(&spec);
	if(status != SCE_GNM_OK)
		return sce::Gnm::SizeAlign(0,0);
	return texture->getSizeAlign();
}

sce::Gnm::SizeAlign Gnmx::Toolkit::initAs1dArray(sce::Gnm::Texture *texture,uint32_t width,uint32_t numSlices,uint32_t numMipLevels,sce::Gnm::DataFormat format,sce::Gnm::TileMode tileModeHint)
{
	sce::Gnm::TextureSpec spec;
	spec.init();
	spec.m_textureType = sce::Gnm::kTextureType1dArray;
	spec.m_width = width;
	spec.m_height = 1;
	spec.m_depth = 1;
	spec.m_pitch = 0;
	spec.m_numMipLevels = numMipLevels;
	spec.m_numSlices = numSlices;
	spec.m_format = format;
	spec.m_tileModeHint = tileModeHint;
	spec.m_minGpuMode = sce::Gnm::kGpuModeBase;
	spec.m_numFragments = sce::Gnm::kNumFragments1;
	int32_t status = texture->init(&spec);
	if(status != SCE_GNM_OK)
		return sce::Gnm::SizeAlign(0,0);
	return texture->getSizeAlign();
}

sce::Gnm::SizeAlign Gnmx::Toolkit::initAs2d(sce::Gnm::Texture *texture,uint32_t width,uint32_t height,uint32_t numMipLevels,sce::Gnm::DataFormat format,sce::Gnm::TileMode tileModeHint,sce::Gnm::NumFragments numFragments)
{
	sce::Gnm::TextureSpec spec;
	spec.init();
	spec.m_textureType = (numFragments == sce::Gnm::kNumFragments1) ? sce::Gnm::kTextureType2d : sce::Gnm::kTextureType2dMsaa;
	spec.m_width = width;
	spec.m_height = height;
	spec.m_depth = 1;
	spec.m_pitch = 0;
	spec.m_numMipLevels = numMipLevels;
	spec.m_numSlices = 1;
	spec.m_format = format;
	spec.m_tileModeHint = tileModeHint;
	spec.m_minGpuMode = sce::Gnm::kGpuModeBase;
	spec.m_numFragments = sce::Gnm::kNumFragments1;
	int32_t status = texture->init(&spec);
	if(status != SCE_GNM_OK)
		return sce::Gnm::SizeAlign(0,0);
	return texture->getSizeAlign();
}

sce::Gnm::SizeAlign Gnmx::Toolkit::initAs2dArray(sce::Gnm::Texture *texture,uint32_t width,uint32_t height,uint32_t numSlices,uint32_t pitch,uint32_t numMipLevels,sce::Gnm::DataFormat format,sce::Gnm::TileMode tileModeHint,sce::Gnm::NumFragments numFragments,bool isCubemap)
{
	sce::Gnm::TextureSpec spec;
	spec.init();
	if(isCubemap)
		spec.m_textureType = sce::Gnm::kTextureTypeCubemap;
	else
		spec.m_textureType = (numFragments == sce::Gnm::kNumFragments1) ? sce::Gnm::kTextureType2dArray : sce::Gnm::kTextureType2dArrayMsaa;
	spec.m_width = width;
	spec.m_height = height;
	spec.m_depth = 1;
	spec.m_pitch = pitch;
	spec.m_numMipLevels = numMipLevels;
	spec.m_numSlices = numSlices;
	spec.m_format = format;
	spec.m_tileModeHint = tileModeHint;
	spec.m_minGpuMode = sce::Gnm::kGpuModeBase;
	spec.m_numFragments = numFragments;
	int32_t status = texture->init(&spec);
	if(status != SCE_GNM_OK)
		return sce::Gnm::SizeAlign(0,0);
	return texture->getSizeAlign();
}

sce::Gnm::SizeAlign Gnmx::Toolkit::initAs2dArray(sce::Gnm::Texture *texture,uint32_t width,uint32_t height,uint32_t numSlices,uint32_t numMipLevels,sce::Gnm::DataFormat format,sce::Gnm::TileMode tileModeHint,sce::Gnm::NumFragments numFragments,bool isCubemap)
{
	return initAs2dArray(texture,width,height,numSlices,0,numMipLevels,format,tileModeHint,numFragments,isCubemap);
}

sce::Gnm::SizeAlign Gnmx::Toolkit::initAsCubemap(sce::Gnm::Texture *texture,uint32_t width,uint32_t height,uint32_t numMipLevels,sce::Gnm::DataFormat format,sce::Gnm::TileMode tileModeHint)
{
	sce::Gnm::TextureSpec spec;
	spec.init();
	spec.m_textureType = sce::Gnm::kTextureTypeCubemap;
	spec.m_width = width;
	spec.m_height = height;
	spec.m_depth = 1;
	spec.m_pitch = 0;
	spec.m_numMipLevels = numMipLevels;
	spec.m_numSlices = 1;
	spec.m_format = format;
	spec.m_tileModeHint = tileModeHint;
	spec.m_minGpuMode = sce::Gnm::kGpuModeBase;
	spec.m_numFragments = sce::Gnm::kNumFragments1;
	int32_t status = texture->init(&spec);
	if(status != SCE_GNM_OK)
		return sce::Gnm::SizeAlign(0,0);
	return texture->getSizeAlign();
}

sce::Gnm::SizeAlign Gnmx::Toolkit::initAsCubemapArray(sce::Gnm::Texture *texture,uint32_t width,uint32_t height,uint32_t numCubemaps,uint32_t numMipLevels,sce::Gnm::DataFormat format,sce::Gnm::TileMode tileModeHint)
{
	sce::Gnm::TextureSpec spec;
	spec.init();
	spec.m_textureType = sce::Gnm::kTextureTypeCubemap;
	spec.m_width = width;
	spec.m_height = height;
	spec.m_depth = 1;
	spec.m_pitch = 0;
	spec.m_numMipLevels = numMipLevels;
	spec.m_numSlices = numCubemaps;
	spec.m_format = format;
	spec.m_tileModeHint = tileModeHint;
	spec.m_minGpuMode = sce::Gnm::kGpuModeBase;
	spec.m_numFragments = sce::Gnm::kNumFragments1;
	int32_t status = texture->init(&spec);
	if(status != SCE_GNM_OK)
		return sce::Gnm::SizeAlign(0,0);
	return texture->getSizeAlign();
}

sce::Gnm::SizeAlign Gnmx::Toolkit::initAs3d(sce::Gnm::Texture *texture,uint32_t width,uint32_t height,uint32_t depth,uint32_t numMipLevels,sce::Gnm::DataFormat format,sce::Gnm::TileMode tileModeHint)
{
	sce::Gnm::TextureSpec spec;
	spec.init();
	spec.m_textureType = sce::Gnm::kTextureType3d;
	spec.m_width = width;
	spec.m_height = height;
	spec.m_depth = depth;
	spec.m_pitch = 0;
	spec.m_numMipLevels = numMipLevels;
	spec.m_numSlices = 1;
	spec.m_format = format;
	spec.m_tileModeHint = tileModeHint;
	spec.m_minGpuMode = sce::Gnm::kGpuModeBase;
	spec.m_numFragments = sce::Gnm::kNumFragments1;
	int32_t status = texture->init(&spec);
	if(status != SCE_GNM_OK)
		return sce::Gnm::SizeAlign(0,0);
	return texture->getSizeAlign();
}

sce::Gnm::SizeAlign Gnmx::Toolkit::init(sce::Gnm::RenderTarget *renderTarget,uint32_t width,uint32_t height,uint32_t numSlices,uint32_t pitch,sce::Gnm::DataFormat rtFormat,sce::Gnm::TileMode tileModeHint,sce::Gnm::NumSamples numSamples,sce::Gnm::NumFragments numFragments,sce::Gnm::SizeAlign *cmaskSizeAlign,sce::Gnm::SizeAlign *fmaskSizeAlign)
{
	sce::Gnm::RenderTargetSpec spec;
	spec.init();
	spec.m_width = width;
	spec.m_height = height;
	spec.m_pitch = pitch;
	spec.m_numSlices = numSlices;
	spec.m_colorFormat = rtFormat;
	spec.m_colorTileModeHint = tileModeHint;
	spec.m_minGpuMode = sce::Gnm::getGpuMode();
	spec.m_numSamples = numSamples;
	spec.m_numFragments = numFragments;
	spec.m_flags.enableCmaskFastClear   = (cmaskSizeAlign != NULL) ? 1 : 0;
	spec.m_flags.enableFmaskCompression = (fmaskSizeAlign != NULL) ? 1 : 0;
	int32_t status = renderTarget->init(&spec);
	if(status != SCE_GNM_OK)
		return sce::Gnm::SizeAlign(0,0);
	if(cmaskSizeAlign != NULL)
		*cmaskSizeAlign = renderTarget->getCmaskSizeAlign();
	if(fmaskSizeAlign != NULL)
		*fmaskSizeAlign = renderTarget->getFmaskSizeAlign();
	return renderTarget->getColorSizeAlign();
}

sce::Gnm::SizeAlign Gnmx::Toolkit::init(sce::Gnm::RenderTarget *renderTarget,uint32_t width,uint32_t height,uint32_t numSlices,sce::Gnm::DataFormat rtFormat,sce::Gnm::TileMode tileModeHint,sce::Gnm::NumSamples numSamples,sce::Gnm::NumFragments numFragments,sce::Gnm::SizeAlign *cmaskSizeAlign,sce::Gnm::SizeAlign *fmaskSizeAlign)
{
	return init(renderTarget,width,height,numSlices,0,rtFormat,tileModeHint,numSamples,numFragments,cmaskSizeAlign,fmaskSizeAlign);
}

sce::Gnm::SizeAlign Gnmx::Toolkit::init(sce::Gnm::DepthRenderTarget *depthRenderTarget,uint32_t width,uint32_t height,uint32_t numSlices,uint32_t pitch,sce::Gnm::ZFormat zFormat,sce::Gnm::StencilFormat stencilFormat,sce::Gnm::TileMode depthTileModeHint,sce::Gnm::NumFragments numFragments,sce::Gnm::SizeAlign *stencilSizeAlign,sce::Gnm::SizeAlign *htileSizeAlign)
{
	sce::Gnm::DepthRenderTargetSpec spec;
	spec.init();
	spec.m_width = width;
	spec.m_height = height;
	spec.m_pitch = pitch;
	spec.m_numSlices = numSlices;
	spec.m_zFormat = zFormat;
	spec.m_stencilFormat = stencilFormat;
	spec.m_tileModeHint = depthTileModeHint;
	spec.m_minGpuMode = sce::Gnm::getGpuMode();
	spec.m_numFragments = numFragments;
	spec.m_flags.enableHtileAcceleration = (htileSizeAlign != NULL) ? 1 : 0;
	int32_t status = depthRenderTarget->init(&spec);
	if(status != SCE_GNM_OK)
		return sce::Gnm::SizeAlign(0,0);
	if(stencilSizeAlign)
		*stencilSizeAlign = depthRenderTarget->getStencilSizeAlign();
	if(htileSizeAlign)
		*htileSizeAlign = depthRenderTarget->getHtileSizeAlign();
	return depthRenderTarget->getZSizeAlign();
}

sce::Gnm::SizeAlign Gnmx::Toolkit::init(sce::Gnm::DepthRenderTarget *depthRenderTarget,uint32_t width,uint32_t height,sce::Gnm::ZFormat zFormat,sce::Gnm::StencilFormat stencilFormat,sce::Gnm::TileMode depthTileModeHint,sce::Gnm::NumFragments numFragments,sce::Gnm::SizeAlign *stencilSizeAlign,sce::Gnm::SizeAlign *htileSizeAlign)
{
	return init(depthRenderTarget,width,height,1,0,zFormat,stencilFormat,depthTileModeHint,numFragments,stencilSizeAlign,htileSizeAlign);
}

sce::Gnm::SizeAlign Gnmx::Toolkit::init(sce::Gnm::DepthRenderTarget *depthRenderTarget,uint32_t width,uint32_t height,uint32_t numSlices,sce::Gnm::ZFormat zFormat,sce::Gnm::StencilFormat stencilFormat,sce::Gnm::TileMode depthTileModeHint,sce::Gnm::NumFragments numFragments,sce::Gnm::SizeAlign *stencilSizeAlign,sce::Gnm::SizeAlign *htileSizeAlign)
{
	return init(depthRenderTarget,width,height,numSlices,0,zFormat,stencilFormat,depthTileModeHint,numFragments,stencilSizeAlign,htileSizeAlign);
}

template <typename T> int copyOneSampleInternal(T *commandBuffer, Gnm::DepthRenderTarget source, Gnm::RenderTarget destination, Gnmx::Toolkit::BuffersToCopy buffersToCopy, int sampleIndex)
{
    const bool copyDepthToColor = (buffersToCopy & Gnmx::Toolkit::kBuffersToCopyDepth) != 0;
    const bool copyStencilToColor = (buffersToCopy & Gnmx::Toolkit::kBuffersToCopyStencil) != 0;

	Gnm::BlendControl blendControl;
	blendControl.init();
	blendControl.setBlendEnable(false);
	commandBuffer->setBlendControl(0, blendControl);

    Gnm::DepthStencilControl depthStencilControl;
    depthStencilControl.init();
    depthStencilControl.setDepthEnable(false);
    depthStencilControl.setStencilEnable(false);        
    commandBuffer->setDepthStencilControl(depthStencilControl);

    Gnm::DbRenderControl dbRenderControl;
    dbRenderControl.init();
    dbRenderControl.setCopySampleIndex(sampleIndex);
    dbRenderControl.setCopyCentroidEnable(true);
    dbRenderControl.setCopyDepthToColor(copyDepthToColor);
    dbRenderControl.setCopyStencilToColor(copyStencilToColor);
    commandBuffer->setDbRenderControl(dbRenderControl);

    commandBuffer->setRenderTarget(0, &destination);
    commandBuffer->setDepthRenderTarget(&source);
    commandBuffer->setRenderTargetMask(buffersToCopy);
	Gnmx::renderFullScreenQuad(commandBuffer);
    commandBuffer->waitForGraphicsWrites(
        destination.getBaseAddress256ByteBlocks(),
        destination.getSliceSizeInBytes() >> 8, 
        Gnm::kWaitTargetSlotCb0,
        Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
        Gnm::kExtendedCacheActionFlushAndInvalidateCbCache,
        Gnm::kStallCommandBufferParserDisable
    );

    dbRenderControl.init();
    commandBuffer->setDbRenderControl(dbRenderControl);

    return 0;
}

Gnm::RenderTarget fromDepthRenderTarget(Gnm::DepthRenderTarget depthRenderTarget)
{
    SCE_GNM_ASSERT(depthRenderTarget.getTileMode() >= Gnm::kTileModeDepth_2dThin_64 && depthRenderTarget.getTileMode() <= Gnm::kTileModeDepth_2dThin_1K);
    Gnm::Texture texture;
    texture.initFromDepthRenderTarget(&depthRenderTarget, false);
    texture.setTileMode(Gnm::kTileModeThin_2dThin);
    Gnm::RenderTarget renderTarget;
    renderTarget.initFromTexture(&texture, 0);
    return renderTarget;
}

int Gnmx::Toolkit::copyOneSample(sce::Gnmx::GnmxDrawCommandBuffer *dcb, Gnm::DepthRenderTarget source, Gnm::RenderTarget destination, BuffersToCopy buffersToCopy, int sampleIndex)
{
    Gnmx::setupScreenViewport(dcb, 0, 0, destination.getWidth(), destination.getHeight(), 0.5f, 0.5f);
	dcb->setEmbeddedPsShader(sce::Gnm::kEmbeddedPsShaderDummyG32R32);
    return copyOneSampleInternal(dcb, source, destination, buffersToCopy, sampleIndex);
}

int Gnmx::Toolkit::copyOneSample(sce::Gnmx::GnmxDrawCommandBuffer *dcb, Gnm::DepthRenderTarget source, Gnm::DepthRenderTarget destination, BuffersToCopy buffersToCopy, int sampleIndex)
{
    const Gnm::RenderTarget renderTarget = fromDepthRenderTarget(destination);
    return copyOneSample(dcb, source, renderTarget, buffersToCopy, sampleIndex);
}

int Gnmx::Toolkit::copyOneSample(sce::Gnmx::GnmxGfxContext *gfxc, Gnm::DepthRenderTarget source, Gnm::RenderTarget destination, BuffersToCopy buffersToCopy, int sampleIndex)
{
    gfxc->setupScreenViewport(0, 0, destination.getWidth(), destination.getHeight(), 0.5f, 0.5f);
	gfxc->setEmbeddedPsShader(sce::Gnm::kEmbeddedPsShaderDummyG32R32);
    return copyOneSampleInternal(gfxc, source, destination, buffersToCopy, sampleIndex);
}

int Gnmx::Toolkit::copyOneSample(sce::Gnmx::GnmxGfxContext *gfxc, Gnm::DepthRenderTarget source, Gnm::DepthRenderTarget destination, BuffersToCopy buffersToCopy, int sampleIndex)
{
    const Gnm::RenderTarget renderTarget = fromDepthRenderTarget(destination);
    return copyOneSample(gfxc, source, renderTarget, buffersToCopy, sampleIndex);
}

