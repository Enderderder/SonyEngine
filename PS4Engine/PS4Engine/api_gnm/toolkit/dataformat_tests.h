/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_TOOLKIT_DATAFORMAT_TESTS_H
#define _SCE_GNM_TOOLKIT_DATAFORMAT_TESTS_H

#include <gnm.h>
#include <gnmx.h>
#include "allocators.h"

namespace sce
{
	namespace Gnmx
	{
		namespace Toolkit
		{
			struct IAllocator;

			namespace DataFormatTests
			{
				/** @brief Initializes SurfaceFormatTests with an ONION and GARLIC allocator.
					@param allocators a pointer to a Allocators object
					*/
				void initializeWithAllocators(Gnmx::Toolkit::Allocators *allocators);

				struct ReadResult
				{
					Reg32 m_bufferLoad[4];
					Reg32 m_textureLoad[4];
					Reg32 m_simulatorDecode[4];
				};

				/** @brief Builds a Buffer and Texture of the specified DataFormat, and tries to read from them in a compute shader, to see what level of support is provided by hardware for this DataFormat.
					@param result The results of the Buffer, Texture, and CPU-simulated DataFormat decode are stored here
					@param gfxc A graphics context which is used to kick off GPU work
					@param dataFormat The DataFormat to be tested
					@param allocator A memory allocator which is used to allocate memory for buffers and textures
					@param src Memory that has already been formatted in the desired dataFormat
				*/
				void read(ReadResult *result, GnmxGfxContext *gfxc, const Gnm::DataFormat dataFormat, Toolkit::IAllocator *allocator, uint32_t *src);

				struct WriteResult
				{
					uint32_t m_rasterizerExport[4];
					uint32_t m_bufferStore[4];
					uint32_t m_textureStore[4];
					uint32_t m_simulatorEncode[4];
				};

				/** @brief Builds a RenderTarget, Buffer and Texture of the specified DataFormat, and tries to write to them from pixel and compute shaders, to see what level of support is provided by hardware.
					@param result The results of the RenderTarget, Buffer, Texture, and CPU-simulated DataFormat encodes are stored here
					@param gfxc A graphics context which is used to kick off GPU work
					@param dataFormat The DataFormat to be tested
					@param allocator A memory allocator which is used to allocate memory for buffers and textures
					@param src simulated shader VGPRs which already contain the data to be encoded into the DataFormat
				*/
				void write(WriteResult *result, GnmxGfxContext *gfxc, const Gnm::DataFormat dataFormat, Toolkit::IAllocator *allocator, Reg32 *src);
			}
		}
	}
}

#endif /* _SCE_GNM_TOOLKIT_DATAFORMAT_TESTS_H */
