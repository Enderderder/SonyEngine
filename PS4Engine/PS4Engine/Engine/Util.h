#pragma once



using namespace sce;
using namespace sce::Gnmx;

namespace util
{
	static uint32_t kDisplayBufferWidth = 1920;
	static uint32_t kDisplayBufferHeight = 1080;
	static const uint32_t kDisplayBufferCount = 3;
	static const uint32_t kRenderContextCount = 2;
	static const Gnm::ZFormat kZFormat = Gnm::kZFormat32Float;
	static const Gnm::StencilFormat kStencilFormat = Gnm::kStencil8;
	static const bool kHtileEnabled = true;
	static const Vector4 kClearColor = Vector4(0.5f, 0.5f, 0.5f, 1);
	static const size_t kOnionMemorySize = 16 * 1024 * 1024;
	static const size_t kGarlicMemorySize = 64 * 4 * 1024 * 1024;

#if SCE_GNMX_ENABLE_GFX_LCUE
	static const uint32_t kNumLcueBatches = 100;
	static const size_t kDcbSizeInBytes = 2 * 1024 * 1024;
#else
	static const uint32_t kCueRingEntries = 64;
	static const size_t kDcbSizeInBytes = 2 * 1024 * 1024;
	static const size_t kCcbSizeInBytes = 256 * 1024;
#endif





}
