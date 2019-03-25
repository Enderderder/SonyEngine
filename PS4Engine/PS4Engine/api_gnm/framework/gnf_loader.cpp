/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "gnf_loader.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "gnf.h"
using namespace sce;

namespace
{
    sce::Gnf::UserData *getUserData(const Gnf::Contents *contents)
    {
        if(NULL == contents)
            return NULL;
        return const_cast<sce::Gnf::UserData *>
		(
			static_cast<const sce::Gnf::UserData *>
			(
				static_cast<const void *>
				(
					static_cast<const uint8_t*>(static_cast<const void *>(contents + 1)) 
					+ contents->m_numTextures * sizeof(sce::Gnm::Texture)
				)
			)
		);
    }

    // content size is sizeof(sce::Gnf::Contents)+gnfContents->m_numTextures*sizeof(sce::Gnm::Texture)+ paddings which is a variable of: gnfContents->alignment
    uint32_t computeContentSize(const sce::Gnf::Contents *gnfContents)
    {
        if(NULL == gnfContents)
            return 0;
        sce::Gnf::UserData *userData = getUserData(gnfContents);
		const int userDataSize = (Gnf::kUserDataMagic == userData->m_magicNumber) ? userData->m_size : 0;
        uint32_t headerSize = (uint32_t) (((const uint8_t *)userData  - (const uint8_t *)gnfContents) + userDataSize + sizeof(sce::Gnf::Header));
        // add the paddings
        uint32_t align = 1<<gnfContents->m_alignment; // actual alignment
	    size_t mask = align-1;
        uint32_t missAligned = ( headerSize & mask ); // number of bytes after the alignemnet point
	    if(missAligned) // if none zero we have to add paddings
	    {
            headerSize += align- missAligned;
        }
        return headerSize-sizeof(sce::Gnf::Header);
    }

}

int Framework::loadTextureFromGnf(sce::Gnm::Texture *outTexture, const Memory &source, uint8_t textureIndex, Gnmx::Toolkit::Allocators* allocators)
{
	if(outTexture == NULL)
		return kGnfErrorInvalidPointer;
	const sce::Gnf::Header   *header   = reinterpret_cast<const sce::Gnf::Header   *>(source.m_begin);
	const sce::Gnf::Contents *contents = (const sce::Gnf::Contents *)(header + 1);

	if(header->m_magicNumber != sce::Gnf::kMagic)
		return Framework::kGnfErrorNotGnfFile;
	if(contents->m_alignment>31)
		return Framework::kGnfErrorAlignmentOutOfRange;
	if(contents->m_version == 1)
	{
		if((contents->m_numTextures*sizeof(sce::Gnm::Texture) + sizeof(sce::Gnf::Contents)) != header->m_contentsSize)
			return Framework::kGnfErrorContentsSizeMismatch;
	} else
	{
		if(contents->m_version != sce::Gnf::kVersion)
			return Framework::kGnfErrorVersionMismatch;
		if(computeContentSize(contents) > header->m_contentsSize)
			return Framework::kGnfErrorContentsSizeMismatch;
	}
	const sce::Gnm::SizeAlign pixelsSa = getTexturePixelsSize(contents, textureIndex);
    void *pixelsAddr;
	allocators->allocate(&pixelsAddr, SCE_KERNEL_WC_GARLIC, pixelsSa, Gnm::kResourceTypeTextureBaseAddress, source.m_name);
	if(nullptr == pixelsAddr)
		return kGnfErrorOutOfMemory;

	const void *pixelsSrc = (uint8_t*)source.m_begin + sizeof(*header) + header->m_contentsSize + getTexturePixelsByteOffset(contents, textureIndex);
	memcpy(pixelsAddr, pixelsSrc, pixelsSa.m_size);
	*outTexture = *patchTextures(const_cast<sce::Gnf::Contents*>(contents), textureIndex, 1, &pixelsAddr);
	return kGnfErrorNone;
}

Framework::GnfError Framework::loadTextureFromGnf(sce::Gnm::Texture *outTexture,const char *fileName,uint8_t textureIndex,Gnmx::Toolkit::Allocators* allocators)
{
	if(nullptr == fileName)
		return kGnfErrorInvalidPointer;
	ReadFile file(fileName);
	return static_cast<Framework::GnfError>(loadTextureFromGnf(outTexture, file, textureIndex, allocators));
}

Framework::GnfError Framework::loadTextureFromGnf(sce::Gnm::Texture *outTexture, const char *fileName, uint8_t textureIndex, Gnmx::Toolkit::IAllocator* allocator)
{
	Gnmx::Toolkit::Allocators allocators(*allocator, *allocator);
	return loadTextureFromGnf(outTexture, fileName, textureIndex, &allocators);
}

uint32_t computeContentsSize(uint32_t log2Alignment)
{
	uint32_t sizeOfContentsAndHeaderInBytes = sizeof(sce::Gnf::Header) + sizeof(sce::Gnf::Contents) + sizeof(sce::Gnm::Texture);
	const uint32_t align = 1 << log2Alignment;
	const size_t mask = align - 1;
	const uint32_t misAligned = (sizeOfContentsAndHeaderInBytes & mask); // number of bytes after the alignment point
	if(misAligned) // if not zero, we have to add padding
		sizeOfContentsAndHeaderInBytes += align - misAligned;
	return sizeOfContentsAndHeaderInBytes - sizeof(sce::Gnf::Header);
}

uint32_t floorLog2(uint32_t value)
{
#ifdef __ORBIS__
    return 31 - __builtin_clz(value | 1);
#else
	unsigned long temp;
	_BitScanReverse(&temp, value | 1);
    return temp;
#endif
}


Framework::GnfError Framework::saveTextureToGnf(const char *fileName, sce::Gnm::Texture *inTexture)
{
	if((fileName==NULL) || (inTexture==NULL))
	{
		return kGnfErrorInvalidPointer;
	}

	FILE *file = fopen(fileName, "wb");
	if(file == 0)
	{
		return kGnfErrorCouldNotOpenFile;
	}

	int32_t addrStatus;
	uint64_t gnfTexelSizeInBytes;
	sce::Gnm::AlignmentType gnfTexelAlign;
	addrStatus = sce::GpuAddress::computeTotalTiledTextureSize(&gnfTexelSizeInBytes, &gnfTexelAlign, inTexture);

	unsigned long log2Alignment = floorLog2(gnfTexelAlign);
	const uint32_t contentsSize = computeContentsSize(log2Alignment);
	const uint32_t outputFileSizeInBytes = static_cast<uint32_t>(sizeof(sce::Gnf::Header) + contentsSize + gnfTexelSizeInBytes);

	sce::Gnf::GnfFile gnfFile;
	gnfFile.header.m_magicNumber = sce::Gnf::kMagic;
	gnfFile.header.m_contentsSize = contentsSize;
	gnfFile.contents.m_version = sce::Gnf::kVersion;
	gnfFile.contents.m_numTextures = 1;
	gnfFile.contents.m_alignment = static_cast<uint8_t>(log2Alignment);
	gnfFile.contents.m_streamSize = outputFileSizeInBytes;
	if(fwrite(&gnfFile, 1, sizeof(gnfFile), file) != sizeof(gnfFile))
	{
		fclose(file);
		return kGnfErrorCouldNotWriteGnfFile;
	}

	sce::Gnm::Texture texture = *inTexture;
	texture.m_regs[0] = 0; 
	texture.m_regs[1] = (texture.m_regs[1] & 0xffffff00) | log2Alignment;
	texture.m_regs[7] = static_cast<uint32_t>(gnfTexelSizeInBytes);
	if(fwrite(&texture, 1, sizeof(texture), file) != sizeof(texture))
	{
		fclose(file);
		return kGnfErrorCouldNotWriteTexture;
	}

	if(fseek(file, sizeof(sce::Gnf::Header) + contentsSize, SEEK_SET) != 0)
	{
		fclose(file);
		return kGnfErrorCouldNotWriteContents;
	}

	if(fwrite(inTexture->getBaseAddress(), 1, gnfTexelSizeInBytes, file) != gnfTexelSizeInBytes)
	{
		fclose(file);
		return kGnfErrorCouldNotWriteTexels;
	}

	fclose(file);

	return kGnfErrorNone;
}
