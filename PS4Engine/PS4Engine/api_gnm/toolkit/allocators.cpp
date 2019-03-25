/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "allocators.h"
#include <stdarg.h>
#include <stdio.h>

namespace sce { namespace Gnmx { namespace Toolkit {

IAllocator Allocators::getNullIAllocator() 
{ 
	IAllocator result = {0, 0, 0}; return result; 
}

Allocators::Allocators(IAllocator onion, IAllocator garlic, Gnm::OwnerHandle owner)
: m_onion(onion)
, m_garlic(garlic)
, m_owner(owner)
{
}

int Allocators::allocate(void **memory, SceKernelMemoryType sceKernelMemoryType, unsigned size, unsigned alignment, Gnm::ResourceType resourceType, const char *name, ...)
{
	switch(sceKernelMemoryType)
	{
	case SCE_KERNEL_WB_ONION:
		*memory = m_onion.allocate(Gnm::SizeAlign(size, alignment));
		break;
	case SCE_KERNEL_WC_GARLIC:
		*memory = m_garlic.allocate(Gnm::SizeAlign(size,alignment));
		break;
	default:
		return -1;
		break;
	}
	if(Gnm::kInvalidOwnerHandle != m_owner && resourceType != Gnm::kResourceTypeInvalid && name != nullptr)
	{
		va_list args;
		va_start(args,name);
		enum { kBufferSize = 64 };
		char buffer[kBufferSize];
		buffer[0] = 0;
#ifdef __ORBIS__
		vsnprintf(buffer,kBufferSize,name,args);
#else
		_vsnprintf_s(buffer,kBufferSize,_TRUNCATE,name,args);
#endif
		Gnm::registerResource(nullptr,m_owner, *memory, size, buffer, resourceType, 0);
		va_end(args);
	}
	return 0;
}

int Allocators::allocate(void **memory,SceKernelMemoryType sceKernelMemoryType, Gnm::SizeAlign sizeAlign, Gnm::ResourceType resourceType,const char *name, ...)
{
	switch(sceKernelMemoryType)
	{
	case SCE_KERNEL_WB_ONION:
	*memory = m_onion.allocate(sizeAlign);
	break;
	case SCE_KERNEL_WC_GARLIC:
	*memory = m_garlic.allocate(sizeAlign);
	break;
	default:
	return -1;
	break;
	}
	if(Gnm::kInvalidOwnerHandle != m_owner && resourceType != Gnm::kResourceTypeInvalid && name != nullptr)
	{
		va_list args;
		va_start(args,name);
		enum { kBufferSize = 64 };
		char buffer[kBufferSize];
		buffer[0] = 0;
#ifdef __ORBIS__
		vsnprintf(buffer,kBufferSize,name,args);
#else
		_vsnprintf_s(buffer,kBufferSize,_TRUNCATE,name,args);
#endif
		Gnm::registerResource(nullptr,m_owner,*memory,sizeAlign.m_size,buffer,resourceType,0);
		va_end(args);
	}
	return 0;
}

} } }

