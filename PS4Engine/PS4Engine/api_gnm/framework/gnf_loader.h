/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_GNF_LOADER_H_
#define _SCE_GNM_FRAMEWORK_GNF_LOADER_H_

#include <gnm/texture.h>
#include "../toolkit/allocator.h"
#include "../toolkit/allocators.h"
#include "framework.h"

namespace Framework
{
	/**
	 * @brief Indicates the result of a GNF load operation
	 */
	enum GnfError
	{
		kGnfErrorNone                   =  0, // Operation was successful; no error
		kGnfErrorInvalidPointer         = -1, // Caller passed an invalid/NULL pointer to a GNF loader function
		kGnfErrorNotGnfFile             = -2, // Attempted to load a file that isn't a GNF file (bad magic number in header)
		kGnfErrorCorruptHeader          = -3, // Attempted to load a GNF file with corrupt header data
		kGnfErrorFileIsTooShort         = -4, // Attempted to load a GNF file whose size is smaller than the size reported in its header
		kGnfErrorVersionMismatch        = -5, // Attempted to load a GNF file created by a different version of the GNF code
		kGnfErrorAlignmentOutOfRange    = -6, // Attempted to load a GNF file with corrupt header data (surface alignment > 2^31 bytes)
		kGnfErrorContentsSizeMismatch   = -7, // Attempted to load a GNF file with corrupt header data (wrong size in GNF header contents)
		kGnfErrorCouldNotOpenFile       = -8, // Unable to open a file for reading
		kGnfErrorOutOfMemory            = -9, // Internal memory allocation failed
		kGnfErrorCouldNotWriteGnfFile   = -10, // Attempted to write the GNF header to the file, but fwrite failed.
		kGnfErrorCouldNotWriteTexture   = -11, // Attempted to write the Texture object to the file, but fwrite failed.
		kGnfErrorCouldNotWriteContents  = -12, // Attempted to fseek to the end of the Contents structure, but fseek failed.
		kGnfErrorCouldNotWriteTexels    = -13, // Attempted to write the texels to the file, but fwrite failed.
	};

	
    /** @brief Load a texture from a GNF file. If successful, the texture's pixels will be stored in shared VRAM, and must be freed by
	 *          the caller.
	 * @param outTexture The Texture object to be filled in with the texture loaded from the GNF file.
	 * @param fileName Filename of the GNF file to read from.
	 * @param textureIndex Index of the texture to be read from the file.
	 * @return Zero if successful; otherwise, a non-zero error code.
	 */
	int loadTextureFromGnf(sce::Gnm::Texture *outTexture, const Memory &source, uint8_t textureIndex,sce::Gnmx::Toolkit::Allocators* allocators);
    GnfError loadTextureFromGnf(sce::Gnm::Texture *outTexture, const char *fileName, uint8_t textureIndex, sce::Gnmx::Toolkit::Allocators* allocators);

    /** @brief Load a texture from a GNF file. If successful, the texture's pixels will be stored in shared VRAM, and must be freed by
	 *          the caller.
	 * @param outTexture The Texture object to be filled in with the texture loaded from the GNF file.
	 * @param fileName Filename of the GNF file to read from.
	 * @param textureIndex Index of the texture to be read from the file.
	 * @return Zero if successful; otherwise, a non-zero error code.
	 */
    GnfError loadTextureFromGnf(sce::Gnm::Texture *outTexture, const char *fileName, uint8_t textureIndex, sce::Gnmx::Toolkit::IAllocator* allocator);

    /** @brief Save a texture to a GNF file.
	 * @param fileName Filename of the GNF file to be written.
	 * @param inTexture The Texture object to be written to the GNF file.
	 * @return Zero if successful; otherwise, a non-zero error code.
	 */
	GnfError saveTextureToGnf(const char *fileName, sce::Gnm::Texture *inTexture);
}

#endif // _SCE_GNM_FRAMEWORK_GNF_LOADER_H_
