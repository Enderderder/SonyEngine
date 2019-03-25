/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "eopeventqueue.h"
#include <unistd.h>
#include <sceerror.h>
#include <gnm.h>
using namespace sce;

Framework::EopEventQueue::EopEventQueue(const char *name)
{
	int32_t ret = 0;
	ret = sceKernelCreateEqueue(&m_equeue, name);
	SCE_GNM_ASSERT_MSG(ret >= 0, "sceKernelCreateEqueue returned error code %d.", ret);
	m_name = name;
	ret=sce::Gnm::addEqEvent(m_equeue, sce::Gnm::kEqEventGfxEop, NULL);
	SCE_GNM_ASSERT_MSG(ret==0,  "sce::Gnm::addEqEvent returned error code %d.", ret);
}

Framework::EopEventQueue::~EopEventQueue()
{
	sceKernelDeleteEqueue(m_equeue);
}

void Framework::EopEventQueue::waitForEvent()
{
	SceKernelEvent ev;
	int num;
	const int32_t error = sceKernelWaitEqueue(m_equeue, &ev, 1, &num, nullptr);
	SCE_GNM_ASSERT(error == SCE_OK);		
}

