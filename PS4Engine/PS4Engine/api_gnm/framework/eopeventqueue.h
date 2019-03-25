/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_EOPEVENTQUEUE_H_
#define _SCE_GNM_FRAMEWORK_EOPEVENTQUEUE_H_

#include "../toolkit/allocator.h"
#include "../toolkit/shader_loader.h"
#include "../toolkit/geommath/geommath.h"
#include "../toolkit/allocators.h"
#include <pthread.h>

namespace Framework
{
	class EopEventQueue
	{
		SceKernelEqueue m_equeue;
		const char *m_name;
	public:
		EopEventQueue(const char *name);
		~EopEventQueue();

		SCE_GNM_API_DEPRECATED("These APIs are no longer supported.")
		bool queueIsEmpty() const { return true; }
		SCE_GNM_API_DEPRECATED("These APIs are no longer supported.")
		void setMaximumPendingEvents(uint32_t maximumPendingEvents) { SCE_GNM_UNUSED(maximumPendingEvents); }
		SCE_GNM_API_DEPRECATED("These APIs are no longer supported.")
		void addPendingEvent() { }

		void waitForEvent();
		SceKernelEqueue getEventQueue() { return m_equeue; }
	};

}

#endif // _SCE_GNM_FRAMEWORK_EOPEVENTQUEUE_H_
