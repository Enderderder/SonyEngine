/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2013 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <stdio.h>
#include <unistd.h>
#include <gnf.h>
#include <gnm\texture.h>

#include "texture_util.h"

#include "..\api_gnm\toolkit\allocators.h"

using namespace sce;
using namespace sce::Gnmx;

namespace TextureUtil
{
	/** @brief Loads a GNF file header and verifies that header contains valid information
	  * @param outHeader Pointer to GNF header structure to be filled with this call
	  * @param gnfFile File pointer to read this data from
	  * @return Zero if successful; otherwise, a non-zero error code.
	  */
	GnfError loadGnfHeader(sce::Gnf::Header *outHeader, FILE *gnfFile)
	{
		if (outHeader == NULL || gnfFile == NULL)
		{
			return TextureUtil::kGnfErrorInvalidPointer;
		}
		outHeader->m_magicNumber     = 0;
		outHeader->m_contentsSize    = 0;
		fseek(gnfFile, 0, SEEK_SET);
		fread(outHeader, sizeof(sce::Gnf::Header), 1, gnfFile);
		if (outHeader->m_magicNumber != sce::Gnf::kMagic)
		{
			return TextureUtil::kGnfErrorNotGnfFile;
		}
		if (outHeader->m_contentsSize>sce::Gnf::kMaxContents)
		{
			return TextureUtil::kGnfErrorCorruptHeader;
		}
		return TextureUtil::kGnfErrorNone;
	}

	// content size is sizeof(sce::Gnf::Contents)+gnfContents->m_numTextures*sizeof(sce::Gnm::Texture)+ paddings which is a variable of: gnfContents->alignment
	uint32_t computeContentSize(const sce::Gnf::Contents *gnfContents)
	{
		// compute the size of used bytes
		uint32_t headerSize = sizeof(sce::Gnf::Header) + sizeof(sce::Gnf::Contents) + gnfContents->m_numTextures*sizeof(sce::Gnm::Texture);
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

	/** @brief Loads GNF contents and verifies that the contents contain valid information
	  * @param outContents Pointer to gnf contents to be read
	  * @param elementCnt The number of elements to read into gnfContents. Usually read from the Gnf::Header object.
	  * @param gnfFile File pointer to read this data from
	  * @return Zero if successful; otherwise, a non-zero error code.
	*/
	GnfError readGnfContents(sce::Gnf::Contents *outContents, uint32_t contentsSizeInBytes, FILE *gnfFile)
	{
		if(outContents == NULL || gnfFile == 0)
		{
			return TextureUtil::kGnfErrorInvalidPointer;
		}
		fseek(gnfFile, sizeof(sce::Gnf::Header), SEEK_SET);

		size_t bytesRead = fread(outContents, 1, contentsSizeInBytes, gnfFile);
		if(bytesRead!=contentsSizeInBytes)
		{
			return TextureUtil::kGnfErrorFileIsTooShort;
		}

		if(outContents->m_alignment>31)
		{
			return TextureUtil::kGnfErrorAlignmentOutOfRange;
		}
		if( outContents->m_version == 1 )
		{
			if( (outContents->m_numTextures*sizeof(sce::Gnm::Texture) + sizeof(sce::Gnf::Contents)) != contentsSizeInBytes )
			{
				return TextureUtil::kGnfErrorContentsSizeMismatch;
			}
		}else
		{
			if( outContents->m_version != sce::Gnf::kVersion )
			{
				return TextureUtil::kGnfErrorVersionMismatch;
			}else
			{
				if( computeContentSize(outContents) != contentsSizeInBytes)
					return TextureUtil::kGnfErrorContentsSizeMismatch;
			}
		}
		return TextureUtil::kGnfErrorNone;
	}

	GnfError loadTextureFromGnf(
		sce::Gnm::Texture *outTexture,
		const char *fileName,
		uint8_t textureIndex,
		sce::Gnmx::Toolkit::Allocators &allocators)
	{
		if( (fileName==NULL) || (outTexture==NULL) )
		{
			return kGnfErrorInvalidPointer;
		}
		// SCE_GNM_ASSERT_MSG(access(fileName, R_OK) == 0, "** Asset Not Found: %s\n", fileName);

		GnfError result = kGnfErrorNone;
		FILE *gnfFile = NULL;
		sce::Gnf::Contents *gnfContents = NULL;
		do
		{
			gnfFile = fopen(fileName, "rb");
			if (gnfFile == 0)
			{
				result = kGnfErrorCouldNotOpenFile;
				break;
			}

			sce::Gnf::Header header;
			result = loadGnfHeader(&header,gnfFile);
			if (result != 0)
			{
				break;
			}

			gnfContents = (sce::Gnf::Contents *)malloc(header.m_contentsSize);
			if(!gnfContents)
			{
				printf("Memory allocation failed \n");
				break;
			}
			result = readGnfContents(gnfContents, header.m_contentsSize, gnfFile );
			if (result)
			{
				break;
			}

			sce::Gnm::SizeAlign pixelsSa = getTexturePixelsSize(gnfContents, textureIndex);
			void *pixelsAddr = allocators.m_garlic.allocate(pixelsSa);
			if( pixelsAddr==0 ) // memory allocation failed
			{
				result = kGnfErrorOutOfMemory;
				break;
			}

			fseek(gnfFile, sizeof(sce::Gnf::Header) + header.m_contentsSize + getTexturePixelsByteOffset(gnfContents, textureIndex) , SEEK_SET);
			fread(pixelsAddr, pixelsSa.m_size, 1, gnfFile);
			*outTexture = *patchTextures(gnfContents, textureIndex, 1, &pixelsAddr);
		}
		while(0);

		free(gnfContents);
		if (gnfFile != NULL)
			fclose(gnfFile);
		return result;
	}
}
