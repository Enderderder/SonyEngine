/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_TOOLKIT_MEMORY_REQUESTS_H
#define _SCE_GNM_TOOLKIT_MEMORY_REQUESTS_H

#include <gnm/constants.h>
#include <gnm/gpumem.h>

namespace sce
{
	namespace Gnmx
	{
		namespace Toolkit
		{
			uint64_t roundUpToAlignment(sce::Gnm::Alignment alignment, uint64_t bytes);
			void *roundUpToAlignment(sce::Gnm::Alignment alignment, void *addr);

			class SCE_GNM_API_DEPRECATED_CLASS_MSG("Please use Allocators instead of MemoryRequest.") MemoryRequest
			{
				void *m_begin;
				void *m_pointer;
				void *m_end;
				void alignPointer(Gnm::AlignmentType alignment);
				void copyToPointerFrom(const void *src, uint32_t bytes);
				void advancePointer(uint32_t bytes);
				void validate();
			public:
				Gnm::SizeAlign m_sizeAlign;
				void request(uint32_t size, Gnm::AlignmentType alignment);
				void *redeem(uint32_t size, Gnm::AlignmentType alignment);
				void fulfill(void* pointer);
				void initialize();
			};

			struct SCE_GNM_API_DEPRECATED_CLASS_MSG("Please use Allocators instead of MemoryRequests.") MemoryRequests
			{
				MemoryRequest m_garlic;
				MemoryRequest m_onion;
				void initialize();
			};

		}
	}
}
#endif // _SCE_GNM_TOOLKIT_MEMORY_REQUESTS_H
