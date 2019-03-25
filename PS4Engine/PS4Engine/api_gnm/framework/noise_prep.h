/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_NOISE_PREP_H_
#define _SCE_GNM_FRAMEWORK_NOISE_PREP_H_

#include <gnmx.h>
#include "../toolkit/allocator.h"
#include "../toolkit/allocators.h"

namespace Framework
{
	struct Allocator;
	void InitNoise(sce::Gnmx::Toolkit::IAllocator* allocator);
	void InitNoise(sce::Gnmx::Toolkit::Allocators* allocators);
	void SetNoiseDataBuffers(sce::Gnmx::GnmxGfxContext &gfxc, sce::Gnm::ShaderStage stage);
}

#endif // _SCE_GNM_FRAMEWORK_NOISE_PREP_H_
