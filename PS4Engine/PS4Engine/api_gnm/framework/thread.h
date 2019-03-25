/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_THREAD_H_
#define _SCE_GNM_FRAMEWORK_THREAD_H_

#include <pthread.h>

namespace Framework
{
	class Thread
	{
		int32_t m_error;
	public:
		uint32_t m_reserved;
	private:
		enum State
		{
			kStateRunning = 1,
		};
		ScePthreadAttr m_attr;
		ScePthread m_pthread;
		SceKernelEventFlag m_eventFlag;	
		const char *m_name;
		static void *staticEntryPoint(void *arg);
		void *entryPoint();
		virtual int32_t callback() = 0;
	protected:
		int32_t stop();
	public:
		Thread(const char *name);
		virtual ~Thread();
		int32_t join();
		bool isRunning() const;
		void run();
		int32_t getError() const;
	};

}

#endif // _SCE_GNM_FRAMEWORK_THREAD_H_
