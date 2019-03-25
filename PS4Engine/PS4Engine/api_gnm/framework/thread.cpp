/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "thread.h"
#include "../toolkit/embedded_shader.h"
#include <unistd.h>
#include <sceerror.h>
#include <gnm.h>
using namespace sce;

Framework::Thread::Thread(const char *name)
{
	m_error = 0;
	m_name = name;
	sceKernelCreateEventFlag(&m_eventFlag, name, SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	m_error = scePthreadAttrInit(&m_attr);
	SCE_GNM_ASSERT_MSG(SCE_OK == m_error, "scePthreadAttrInit failed with error code %d", m_error);
	m_error = scePthreadAttrSetdetachstate(&m_attr,SCE_PTHREAD_CREATE_JOINABLE);
	SCE_GNM_ASSERT_MSG(SCE_OK == m_error, "scePthreadAttrSetdetachstate failed with error code %d", m_error);
	m_error = scePthreadAttrSetschedpolicy(&m_attr,SCE_KERNEL_SCHED_FIFO);
	SCE_GNM_ASSERT_MSG(SCE_OK == m_error, "scePthreadAttrSetschedpolicy failed with error code %d", m_error);
	SceKernelSchedParam p = {0};
	p.sched_priority = SCE_KERNEL_PRIO_FIFO_LOWEST;
	m_error = scePthreadAttrSetschedparam(&m_attr,&p);
	SCE_GNM_ASSERT_MSG(SCE_OK == m_error, "scePthreadAttrSetschedparam failed with error code %d", m_error);
	m_error = scePthreadCreate(&m_pthread, &m_attr, staticEntryPoint, this, name); 
	SCE_GNM_ASSERT_MSG(SCE_OK == m_error, "scePthreadCreate failed with error code %d", m_error);
}

int32_t Framework::Thread::getError() const
{
	return m_error;
}

void Framework::Thread::run()
{
	const int32_t error = sceKernelSetEventFlag(m_eventFlag, kStateRunning);
	SCE_GNM_ASSERT(SCE_OK == error);
}

void *Framework::Thread::staticEntryPoint(void *arg)
{
	return static_cast<Thread*>(arg)->entryPoint();
}

void *Framework::Thread::entryPoint()
{
	m_error = sceKernelWaitEventFlag(m_eventFlag, kStateRunning, SCE_KERNEL_EVF_WAITMODE_AND, NULL, NULL);
	if(SCE_OK == m_error)
		m_error = callback();
    pthread_exit(NULL);
}

bool Framework::Thread::isRunning() const
{
	return SCE_OK == sceKernelPollEventFlag(m_eventFlag, kStateRunning, SCE_KERNEL_EVF_WAITMODE_AND, NULL);
}

int32_t Framework::Thread::stop()
{
	int32_t error;
	error = sceKernelClearEventFlag(m_eventFlag, ~kStateRunning);
	SCE_GNM_ASSERT(SCE_OK == error);
	return error;
}

int32_t Framework::Thread::join()
{
	int32_t error;
	SCE_GNM_UNUSED(error);
	error = stop();
	SCE_GNM_ASSERT(SCE_OK == error);
	error = scePthreadJoin(m_pthread, NULL);
	SCE_GNM_ASSERT(SCE_OK == error);
	return m_error;
}

Framework::Thread::~Thread()
{
	if(isRunning())
		join();
	scePthreadAttrDestroy(&m_attr);
}

