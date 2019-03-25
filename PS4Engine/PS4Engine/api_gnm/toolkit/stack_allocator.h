/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_TOOLKIT_STACK_ALLOCATOR_H_
#define _SCE_GNM_TOOLKIT_STACK_ALLOCATOR_H_

#include "allocator.h"
#include <sys/dmem.h>

namespace sce
{
	namespace Gnmx
	{
		namespace Toolkit
		{
			struct StackAllocator
			{
				StackAllocator();
				~StackAllocator();
				enum {kMaximumAllocations = 8192};
				uint8_t *m_allocation[kMaximumAllocations];
				uint8_t *m_base;
				off_t m_offset;
				size_t m_size;
				off_t m_top;
				uint64_t m_allocations;
				uint64_t m_alignment;
				SceKernelMemoryType m_type;
				bool m_isInitialized;
				uint8_t m_reserved[3];
				void init(SceKernelMemoryType type, uint32_t size);
				void *allocate(uint32_t size, uint32_t alignment);
				void *allocate(Gnm::SizeAlign sizeAlign)
				{
					return allocate(sizeAlign.m_size, sizeAlign.m_align);
				}
				void release(void* pointer);
				void deinit();
			};

			IAllocator GetInterface(StackAllocator *stackAllocator);
		}
	}
}

#endif /* _SCE_GNM_TOOLKIT_STACK_ALLOCATOR_H_ */
