/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_TOOLKIT_ALLOCATOR_H_
#define _SCE_GNM_TOOLKIT_ALLOCATOR_H_

#include <gnm.h>

namespace sce
{
	namespace Gnmx
	{
		namespace Toolkit
		{
			struct IAllocator
			{
				void* m_instance;
				void *(*m_allocate)(void *instance, uint32_t size, sce::Gnm::AlignmentType alignment);
				void (*m_release)(void *instance, void *pointer);

				void *allocate(uint32_t size, sce::Gnm::AlignmentType alignment)
				{
					return m_allocate(m_instance, size, alignment);
				}
				void *allocate(sce::Gnm::SizeAlign sizeAlign) 
				{
					return m_allocate(m_instance, sizeAlign.m_size, sizeAlign.m_align);
				}

				void release(void *pointer)
				{
					if(pointer != NULL)
						m_release(m_instance, pointer);
				}
			};
		}
	}
}

#endif /* _SCE_GNM_TOOLKIT_ALLOCATOR_H_ */
