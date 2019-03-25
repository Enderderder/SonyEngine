/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "frame.h"
#include "sample_framework.h"
#include <gnm.h>
using namespace sce;

namespace Framework
{
	void Frame::Init(GnmSampleFramework *framework, unsigned buffer)
	{
		createCommandBuffer(&m_commandBuffer, framework, buffer);
		doInit(framework, buffer);
	}
}

