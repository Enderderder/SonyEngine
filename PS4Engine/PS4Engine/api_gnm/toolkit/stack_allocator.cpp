﻿/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "stack_allocator.h"
using namespace sce;

Gnmx::Toolkit::StackAllocator::StackAllocator()
: m_isInitialized(false)
{
}

Gnmx::Toolkit::StackAllocator::~StackAllocator()
{
	if(m_isInitialized)
	{
		deinit();
	}
}

void Gnmx::Toolkit::StackAllocator::init(SceKernelMemoryType type, uint32_t size)
{
	SCE_GNM_ASSERT(false == m_isInitialized);
	m_allocations = 0;
	m_top = 0;
	m_type = type;
	m_size = size;
	m_alignment = 2 * 1024 * 1024;
	m_base = 0;
	int retSys = sceKernelAllocateDirectMemory(0,
		SCE_KERNEL_MAIN_DMEM_SIZE,
		m_size,
		m_alignment, // alignment
		m_type,
		&m_offset);
	SCE_GNM_ASSERT(retSys == 0);
	retSys = sceKernelMapDirectMemory(&reinterpret_cast<void*&>(m_base),
		m_size,
		SCE_KERNEL_PROT_CPU_READ|SCE_KERNEL_PROT_CPU_WRITE|SCE_KERNEL_PROT_GPU_ALL,
		0,						//flags
		m_offset,
		m_alignment);
	SCE_GNM_ASSERT(retSys == 0);
	m_isInitialized = true;
}

void Gnmx::Toolkit::StackAllocator::deinit()
{
	SCE_GNM_ASSERT(true == m_isInitialized);
	int ret = sceKernelReleaseDirectMemory(m_offset, m_size);
	SCE_GNM_ASSERT(ret == 0);
	m_isInitialized = false;
}

void *Gnmx::Toolkit::StackAllocator::allocate(uint32_t size, uint32_t alignment)
{
	SCE_GNM_ASSERT(m_allocations < kMaximumAllocations);
    SCE_GNM_ASSERT(alignment > 0);
	const off_t mask = off_t(alignment) - 1;
    SCE_GNM_ASSERT((off_t(alignment) & mask) == 0);
	m_top = (m_top + mask) & ~mask;
	void* result = m_allocation[m_allocations++] = m_base + m_top;
	m_top += size;
	SCE_GNM_ASSERT(m_top <= static_cast<off_t>(m_size));
	return result;
}

void Gnmx::Toolkit::StackAllocator::release(void* pointer)
{
	SCE_GNM_ASSERT(m_allocations > 0);
	uint8_t* lastPointer = m_allocation[--m_allocations];
	SCE_GNM_ASSERT(lastPointer == pointer);
	m_top = lastPointer - m_base; // this may not rewind far enough if subsequent allocation has looser alignment than previous one
}

namespace
{
	void *allocate(void *instance, uint32_t size, sce::Gnm::AlignmentType alignment)
	{
		return static_cast<Gnmx::Toolkit::StackAllocator*>(instance)->allocate(size, alignment);
	}
	void release(void *instance, void *pointer)
	{
		static_cast<Gnmx::Toolkit::StackAllocator*>(instance)->release(pointer);
	}
};

Gnmx::Toolkit::IAllocator Gnmx::Toolkit::GetInterface(Toolkit::StackAllocator *stackAllocator)
{
	Toolkit::IAllocator allocator;
	allocator.m_instance = stackAllocator;
	allocator.m_allocate = allocate;
	allocator.m_release = release;
	return allocator;
}

