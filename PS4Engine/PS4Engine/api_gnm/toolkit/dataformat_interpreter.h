/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_TOOLKIT_DATAFORMAT_INTERPRETER_H
#define _SCE_GNM_TOOLKIT_DATAFORMAT_INTERPRETER_H

#include <gnm.h>
#include <gnmx.h>
#include "floating_point.h"

namespace sce
{
	namespace Gnmx
	{
		namespace Toolkit
		{
			struct IAllocator;

			/** @brief A simulated VGPR
			*/
			union Reg32
			{
				enum {kBias = 127};
				struct 
				{
					uint32_t m_mantissa:23;
					uint32_t m_exponent:8;
					uint32_t m_sign:1;
				} bits;
				float    f;
				uint32_t u;
				int32_t  i;
			};

			/** @brief Encodes four 32-bit virtual VGPRs into 128 bits of memory in the specified DataFormat.
			   @param dest       The 128 bits of memory to encode into.
               @param destDwords The number of contiguous 32-bit words that were written to dest is stored here.
			   @param src        The four 32-bit virtual VGPRs to encode from.
			   @param dataFormat The format of the data.
			   */
			void dataFormatEncoder(uint32_t *__restrict dest, uint32_t *__restrict destDwords, const Reg32 *__restrict src, const Gnm::DataFormat dataFormat);

			/** @brief Decodes 128 bits of memory in the specified DataFormat into four 32-bit virtual VGPRs.
			   @param dest       The four 32-bit virtual VGPRs to encode into.
			   @param src        The 128 bits of memory to encode from.
			   @param dataFormat The format of the data.
			   */
			void dataFormatDecoder(Reg32 *__restrict dest, const uint32_t *__restrict src, const Gnm::DataFormat dataFormat);
		}
	}
}

#endif /* _SCE_GNM_TOOLKIT_DATAFORMAT_INTERPRETER_H */
