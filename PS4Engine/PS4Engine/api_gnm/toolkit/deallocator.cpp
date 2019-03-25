/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "deallocator.h"
using namespace sce;

Gnmx::Toolkit::Deallocator::Deallocator(IAllocator iallocator)
: m_iallocator(iallocator)
, m_allocations(0)
{
}

Gnmx::Toolkit::Deallocator::~Deallocator()
{
	deallocate();
}

void Gnmx::Toolkit::Deallocator::deallocate()
{
	while(m_allocations--)
		m_iallocator.release(m_allocation[m_allocations]);
}

void *Gnmx::Toolkit::Deallocator::allocate(uint32_t size, uint32_t alignment)
{
	SCE_GNM_ASSERT(m_allocations < kMaximumAllocations);
	return m_allocation[m_allocations++] = m_iallocator.allocate(size, alignment);
}

void *Gnmx::Toolkit::Deallocator::allocate(Gnm::SizeAlign sizeAlign)
{
	return allocate(sizeAlign.m_size, sizeAlign.m_align);
}

namespace
{
	void *allocate(void *instance, uint32_t size, sce::Gnm::AlignmentType alignment)
	{
		return static_cast<Gnmx::Toolkit::Deallocator*>(instance)->allocate(size, alignment);
	}
	void release(void * /*instance*/, void * /*pointer*/)
	{
		SCE_GNM_ASSERT("not supported");
	}
};

Gnmx::Toolkit::IAllocator Gnmx::Toolkit::GetInterface(Toolkit::Deallocator *deallocator)
{
	Toolkit::IAllocator allocator;
	allocator.m_instance = deallocator;
	allocator.m_allocate = allocate;
	allocator.m_release = release;
	return allocator;
}


