/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_FRAME_H_
#define _SCE_GNM_FRAMEWORK_FRAME_H_

#include <gnmx.h>

namespace Framework
{
	class GnmSampleFramework;

	class SCE_GNM_API_DEPRECATED_CLASS_MSG("please use the createCommandBuffer function instead.") Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		void Init(GnmSampleFramework *framework, unsigned buffer);
	private:
		virtual void doInit(GnmSampleFramework */*framework*/, unsigned /*buffer*/) {}
	};
}

#endif // _SCE_GNM_FRAMEWORK_FRAMEWORK_H_
