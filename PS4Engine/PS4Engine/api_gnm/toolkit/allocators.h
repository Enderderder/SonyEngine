/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_TOOLKIT_ALLOCATORS_H_
#define _SCE_GNM_TOOLKIT_ALLOCATORS_H_

#include <gnm.h>
#include <sys/dmem.h>
#include "allocator.h"

namespace sce
{
	namespace Gnmx
	{
		namespace Toolkit
		{
			class Allocators
			{
				static IAllocator getNullIAllocator();
			public:
				Allocators(IAllocator onion = getNullIAllocator(), IAllocator garlic = getNullIAllocator(), Gnm::OwnerHandle owner = Gnm::kInvalidOwnerHandle);
				IAllocator m_onion;
				IAllocator m_garlic;
				Gnm::OwnerHandle m_owner;
				uint8_t m_reserved[4];
				int allocate(void **memory, SceKernelMemoryType sceKernelMemoryType, unsigned size, unsigned alignment, Gnm::ResourceType resourceType = Gnm::kResourceTypeInvalid, const char *name = nullptr, ...);
				int allocate(void **memory,SceKernelMemoryType sceKernelMemoryType,Gnm::SizeAlign sizeAlign,Gnm::ResourceType resourceType = Gnm::kResourceTypeInvalid,const char *name = nullptr,...);
			};
		}
	}
}

#endif /* _SCE_GNM_TOOLKIT_ALLOCATOR_H_ */
