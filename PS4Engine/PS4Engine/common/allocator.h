/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2013 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _ALLOCATOR_H_
#define _ALLOCATOR_H_

#include <gnm.h>
#include <kernel.h>
#include <scebase.h>

#include "../api_gnm/toolkit/allocators.h"

template<typename T> inline T alignToPow2(const T size, const T alignment)
{
	return (size + alignment-T(1)) & (~(alignment-T(1)));
}

template<typename T> inline T alignTo(const T size, const T alignment)
{
	return ((size + alignment-T(1)) / alignment) * alignment;
}

class LinearAllocator
{
public:
	LinearAllocator()
	:
		m_baseAddr(NULL),
		m_dmemOffset(0),
		m_curSize(0),
		m_maxSize(0)
	{
	}

	int32_t initialize(
		const size_t memorySize,
		const SceKernelMemoryType memoryType,
		const int memoryProtection)
	{
		int32_t ret;

		ret = terminate();
		if( ret != SCE_OK )
			return ret;

		const size_t memoryAlignment = 64UL * 1024UL;
		m_maxSize = alignToPow2(memorySize, memoryAlignment);

		ret = sceKernelAllocateDirectMemory(
			0,
			sceKernelGetDirectMemorySize() - 1,
			m_maxSize,
			memoryAlignment,
			memoryType,
			&m_dmemOffset);

		if( ret != SCE_OK )
		{
			printf("sceKernelAllocateDirectMemory failed: 0x%08X\n", ret);
			return ret;
		}

		void *baseAddress = NULL;
		ret = sceKernelMapDirectMemory(
			&baseAddress,
			m_maxSize,
			memoryProtection,
			0,
			m_dmemOffset,
			memoryAlignment);

		if( ret != SCE_OK )
		{
			printf("sceKernelMapDirectMemory failed: 0x%08X\n", ret);
			return ret;
		}

		m_baseAddr = static_cast<uint8_t*>(baseAddress);

		return SCE_OK;
	}

	int32_t terminate()
	{
		int32_t tmpRet, ret = SCE_OK;

		if( m_baseAddr )
		{
			tmpRet = sceKernelMunmap(m_baseAddr, m_maxSize);
			if( tmpRet != SCE_OK )
			{
				printf("sceKernelMunmap failed: 0x%08X\n", tmpRet);
				ret = tmpRet;
			}
		}

		if( m_dmemOffset )
		{
			tmpRet = sceKernelReleaseDirectMemory(m_dmemOffset, m_maxSize);
			if( tmpRet != SCE_OK )
			{
				printf("sceKernelReleaseDirectMemory failed: 0x%08X\n", tmpRet);
				ret = tmpRet;
			}
		}

		m_baseAddr = NULL;
		m_dmemOffset = 0;
		m_curSize = 0;
		m_maxSize = 0;

		return ret;
	}

	void* allocate(size_t size, size_t align)
	{
		const size_t offset = alignTo(m_curSize, align);
		const size_t newSize = offset + size;

		if( newSize > m_maxSize )
			return NULL;

		m_curSize = newSize;

		return m_baseAddr + offset;
	}

	void* allocate(sce::Gnm::SizeAlign sizeAlign)
	{
		return allocate(sizeAlign.m_size, sizeAlign.m_align);
	}

	void getIAllocator(sce::Gnmx::Toolkit::IAllocator &ialloc)
	{
		ialloc.m_instance = this;
		ialloc.m_allocate = allocCallback;
		ialloc.m_release = releaseCallback;
	}

private:
	static void* allocCallback(void *instance, uint32_t size, sce::Gnm::AlignmentType alignment)
	{
		LinearAllocator *allocator = static_cast<LinearAllocator*>(instance);
		return allocator->allocate(size, alignment);
	}

	static void releaseCallback(void *, void *)
	{
		printf("Cannot release single memory blocks on a linear allocator\n");
	}

private:
	uint8_t *m_baseAddr;
	off_t m_dmemOffset;
	size_t m_curSize;
	size_t m_maxSize;
};

#endif
