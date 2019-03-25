
#include <stdio.h>
#include <stdlib.h>
#include <scebase.h>
#include <kernel.h>
#include <gnmx.h>
#include <video_out.h>
#include "../PS4Engine/api_gnm/toolkit/toolkit.h"
#include "../PS4Engine/common/allocator.h"
#include "../PS4Engine/common/shader_loader.h"

#include "Engine/Util.h"

using namespace sce;
using namespace sce::Gnmx;

size_t sceLibcHeapSize = 64 * 1024 * 1024;

bool readRawTextureData(const char *path, void *address, size_t size)
{
	bool success = false;

	FILE *fp = fopen(path, "rb");
	if (fp != NULL)
	{
		success = readFileContents(address, size, fp);
		fclose(fp);
	}

	return success;
}

int main(int argc, const char *argv[])
{
	
	Gnm::GpuMode gpuMode = Gnm::getGpuMode();
	int ret;

	// Initialize the WB_ONION memory allocator
	LinearAllocator onionAllocator;
	ret = onionAllocator.initialize(
		util::kOnionMemorySize, SCE_KERNEL_WB_ONION,
		SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_ALL);
	if (ret != SCE_OK)
		return ret;

	// Initialize the WC_GARLIC memory allocator
	// NOTE: CPU reads from GARLIC write-combined memory have a very low
	//       bandwidth so they are disabled for safety in this sample
	LinearAllocator garlicAllocator;
	ret = garlicAllocator.initialize(
		util::kGarlicMemorySize,
		SCE_KERNEL_WC_GARLIC,
		SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_ALL);
	if (ret != SCE_OK)
		return ret;

	// Open the video output port
	int videoOutHandle = sceVideoOutOpen(0, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
	if (videoOutHandle < 0)
	{
		printf("sceVideoOutOpen failed: 0x%08X\n", videoOutHandle);
		return videoOutHandle;
	}

	// Initialize the flip rate: 0: 60Hz, 1: 30Hz or 2: 20Hz
	ret = sceVideoOutSetFlipRate(videoOutHandle, 0);
	if (ret != SCE_OK)
	{
		printf("sceVideoOutSetFlipRate failed: 0x%08X\n", ret);
		return ret;
	}

	// Create the event queue for used to synchronize with end-of-pipe interrupts
	SceKernelEqueue eopEventQueue;
	ret = sceKernelCreateEqueue(&eopEventQueue, "EOP QUEUE");
	if (ret != SCE_OK)
	{
		printf("sceKernelCreateEqueue failed: 0x%08X\n", ret);
		return ret;
	}

	// Register for the end-of-pipe events
	ret = Gnm::addEqEvent(eopEventQueue, Gnm::kEqEventGfxEop, NULL);
	if (ret != SCE_OK)
	{
		printf("Gnm::addEqEvent failed: 0x%08X\n", ret);
		return ret;
	}

	// Initialize the Toolkit module
	sce::Gnmx::Toolkit::Allocators toolkitAllocators;
	onionAllocator.getIAllocator(toolkitAllocators.m_onion);
	garlicAllocator.getIAllocator(toolkitAllocators.m_garlic);
	Toolkit::initializeWithAllocators(&toolkitAllocators);

	// For simplicity reasons the sample uses a single GfxContext for each
	// frame. Implementing more complex schemes where multipleGfxContext-s
	// are submitted in each frame is possible as well, but it is out of the
	// scope for this basic sample.
	typedef struct RenderContext
	{
		Gnmx::GnmxGfxContext    gfxContext;
#if SCE_GNMX_ENABLE_GFX_LCUE
		void                   *resourceBuffer;
		void                   *dcbBuffer;
#else
		void                   *cueHeap;
		void                   *dcbBuffer;
		void                   *ccbBuffer;
#endif
		volatile uint32_t      *contextLabel;
	}
	RenderContext;

	typedef struct DisplayBuffer
	{
		Gnm::RenderTarget       renderTarget;
		int                     displayIndex;
	}
	DisplayBuffer;

	enum RenderContextState
	{
		kRenderContextFree = 0,
		kRenderContextInUse,
	};

	RenderContext renderContexts[util::kRenderContextCount];
	RenderContext *renderContext = renderContexts;
	uint32_t renderContextIndex = 0;

	// Initialize all the render contexts
	for (uint32_t i = 0; i < util::kRenderContextCount; ++i)
	{
#if SCE_GNMX_ENABLE_GFX_LCUE
		// Calculate the size of the resource buffer for the given number of draw calls
		const uint32_t resourceBufferSizeInBytes =
			Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(
				Gnmx::LightweightConstantUpdateEngine::kResourceBufferGraphics,
				kNumLcueBatches);

		// Allocate the LCUE resource buffer memory
		renderContexts[i].resourceBuffer = garlicAllocator.allocate(
			resourceBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes);

		if (!renderContexts[i].resourceBuffer)
		{
			printf("Cannot allocate the LCUE resource buffer memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		// Allocate the draw command buffer
		renderContexts[i].dcbBuffer = onionAllocator.allocate(
			kDcbSizeInBytes,
			Gnm::kAlignmentOfBufferInBytes);

		if (!renderContexts[i].dcbBuffer)
		{
			printf("Cannot allocate the draw command buffer memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		// Initialize the GfxContext used by this rendering context
		renderContexts[i].gfxContext.init(
			renderContexts[i].dcbBuffer,		// Draw command buffer address
			kDcbSizeInBytes,					// Draw command buffer size in bytes
			renderContexts[i].resourceBuffer,	// Resource buffer address
			resourceBufferSizeInBytes,			// Resource buffer address in bytes
			NULL);								// Global resource table

#else // SCE_GNMX_ENABLE_GFX_LCUE

		// Allocate the CUE heap memory
		renderContexts[i].cueHeap = garlicAllocator.allocate(
			Gnmx::ConstantUpdateEngine::computeHeapSize(util::kCueRingEntries),
			Gnm::kAlignmentOfBufferInBytes);

		if (!renderContexts[i].cueHeap)
		{
			printf("Cannot allocate the CUE heap memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		// Allocate the draw command buffer
		renderContexts[i].dcbBuffer = onionAllocator.allocate(
			util::kDcbSizeInBytes,
			Gnm::kAlignmentOfBufferInBytes);

		if (!renderContexts[i].dcbBuffer)
		{
			printf("Cannot allocate the draw command buffer memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		// Allocate the constants command buffer
		renderContexts[i].ccbBuffer = onionAllocator.allocate(
			util::kCcbSizeInBytes,
			Gnm::kAlignmentOfBufferInBytes);

		if (!renderContexts[i].ccbBuffer)
		{
			printf("Cannot allocate the constants command buffer memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		// Initialize the GfxContext used by this rendering context
		renderContexts[i].gfxContext.init(
			renderContexts[i].cueHeap,
			util::kCueRingEntries,
			renderContexts[i].dcbBuffer,
			util::kDcbSizeInBytes,
			renderContexts[i].ccbBuffer,
			util::kCcbSizeInBytes);
#endif	// SCE_GNMX_ENABLE_GFX_LCUE

		renderContexts[i].contextLabel = (volatile uint32_t*)onionAllocator.allocate(4, 8);
		if (!renderContexts[i].contextLabel)
		{
			printf("Cannot allocate a GPU label\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		renderContexts[i].contextLabel[0] = kRenderContextFree;
	}

	DisplayBuffer displayBuffers[util::kDisplayBufferCount];
	DisplayBuffer *backBuffer = displayBuffers;
	uint32_t backBufferIndex = 0;

	// Convenience array used by sceVideoOutRegisterBuffers()
	void *surfaceAddresses[util::kDisplayBufferCount];

	SceVideoOutResolutionStatus status;
	if (SCE_OK == sceVideoOutGetResolutionStatus(videoOutHandle, &status) && status.fullHeight > 1080)
	{
		util::kDisplayBufferWidth *= 2;
		util::kDisplayBufferHeight *= 2;
	}

	// Initialize all the display buffers
	for (uint32_t i = 0; i < util::kDisplayBufferCount; ++i)
	{
		// Compute the tiling mode for the render target
		Gnm::TileMode tileMode;
		Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb;
		ret = GpuAddress::computeSurfaceTileMode(
			gpuMode, // NEO or base
			&tileMode,										// Tile mode pointer
			GpuAddress::kSurfaceTypeColorTargetDisplayable,	// Surface type
			format,											// Surface format
			1);												// Elements per pixel
		if (ret != SCE_OK)
		{
			printf("GpuAddress::computeSurfaceTileMode: 0x%08X\n", ret);
			return ret;
		}

		// Initialize the render target descriptor

		Gnm::RenderTargetSpec spec;
		spec.init();
		spec.m_width = util::kDisplayBufferWidth;
		spec.m_height = util::kDisplayBufferHeight;
		spec.m_pitch = 0;
		spec.m_numSlices = 1;
		spec.m_colorFormat = format;
		spec.m_colorTileModeHint = tileMode;
		spec.m_minGpuMode = gpuMode;
		spec.m_numSamples = Gnm::kNumSamples1;
		spec.m_numFragments = Gnm::kNumFragments1;
		spec.m_flags.enableCmaskFastClear = 0;
		spec.m_flags.enableFmaskCompression = 0;
		ret = displayBuffers[i].renderTarget.init(&spec);
		if (ret != SCE_GNM_OK)
			return ret;

		Gnm::SizeAlign sizeAlign = displayBuffers[i].renderTarget.getColorSizeAlign();
		// Allocate the render target memory
		surfaceAddresses[i] = garlicAllocator.allocate(sizeAlign);
		if (!surfaceAddresses[i])
		{
			printf("Cannot allocate the render target memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}
		displayBuffers[i].renderTarget.setAddresses(surfaceAddresses[i], 0, 0);

		displayBuffers[i].displayIndex = i;
	}

	// Initialization the VideoOut buffer descriptor. The pixel format must
	// match with the render target data format, which in this case is
	// Gnm::kDataFormatB8G8R8A8UnormSrgb
	SceVideoOutBufferAttribute videoOutBufferAttribute;
	sceVideoOutSetBufferAttribute(
		&videoOutBufferAttribute,
		SCE_VIDEO_OUT_PIXEL_FORMAT_B8_G8_R8_A8_SRGB,
		SCE_VIDEO_OUT_TILING_MODE_TILE,
		SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
		backBuffer->renderTarget.getWidth(),
		backBuffer->renderTarget.getHeight(),
		backBuffer->renderTarget.getPitch());

	// Register the buffers to the slot: [0..kDisplayBufferCount-1]
	ret = sceVideoOutRegisterBuffers(
		videoOutHandle,
		0, // Start index
		surfaceAddresses,
		util::kDisplayBufferCount,
		&videoOutBufferAttribute);
	if (ret != SCE_OK)
	{
		printf("sceVideoOutRegisterBuffers failed: 0x%08X\n", ret);
		return ret;
	}

	// Compute the tiling mode for the depth buffer
	Gnm::DataFormat depthFormat = Gnm::DataFormat::build(util::kZFormat);
	Gnm::TileMode depthTileMode;
	ret = GpuAddress::computeSurfaceTileMode(
		gpuMode, // NEO or Base
		&depthTileMode,									// Tile mode pointer
		GpuAddress::kSurfaceTypeDepthOnlyTarget,		// Surface type
		depthFormat,									// Surface format
		1);												// Elements per pixel
	if (ret != SCE_OK)
	{
		printf("GpuAddress::computeSurfaceTileMode: 0x%08X\n", ret);
		return ret;
	}

	// Initialize the depth buffer descriptor
	Gnm::DepthRenderTarget depthTarget;
	Gnm::SizeAlign stencilSizeAlign;
	Gnm::SizeAlign htileSizeAlign;


	Gnm::DepthRenderTargetSpec spec;
	spec.init();
	spec.m_width = util::kDisplayBufferWidth;
	spec.m_height = util::kDisplayBufferHeight;
	spec.m_pitch = 0;
	spec.m_numSlices = 1;
	spec.m_zFormat = depthFormat.getZFormat();
	spec.m_stencilFormat = util::kStencilFormat;
	spec.m_minGpuMode = gpuMode;
	spec.m_numFragments = Gnm::kNumFragments1;
	spec.m_flags.enableHtileAcceleration = util::kHtileEnabled ? 1 : 0;


	ret = depthTarget.init(&spec);
	if (ret != SCE_GNM_OK)
		return ret;

	Gnm::SizeAlign depthTargetSizeAlign = depthTarget.getZSizeAlign();

	// Initialize the HTILE buffer, if enabled
	if (util::kHtileEnabled)
	{
		htileSizeAlign = depthTarget.getHtileSizeAlign();
		void *htileMemory = garlicAllocator.allocate(htileSizeAlign);
		if (!htileMemory)
		{
			printf("Cannot allocate the HTILE buffer\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		depthTarget.setHtileAddress(htileMemory);
	}

	// Initialize the stencil buffer, if enabled
	void *stencilMemory = NULL;
	stencilSizeAlign = depthTarget.getStencilSizeAlign();
	if (util::kStencilFormat != Gnm::kStencilInvalid)
	{
		stencilMemory = garlicAllocator.allocate(stencilSizeAlign);
		if (!stencilMemory)
		{
			printf("Cannot allocate the stencil buffer\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}
	}

	// Allocate the depth buffer
	void *depthMemory = garlicAllocator.allocate(depthTargetSizeAlign);
	if (!depthMemory)
	{
		printf("Cannot allocate the depth buffer\n");
		return SCE_KERNEL_ERROR_ENOMEM;
	}
	depthTarget.setAddresses(depthMemory, stencilMemory);


	/** Update Function */

	for (uint32_t frameIndex = 0; frameIndex < 1000; ++frameIndex)
	{
		Gnmx::GnmxGfxContext &gfxc = renderContext->gfxContext;

		// Wait until the context label has been written to make sure that the
		// GPU finished parsing the command buffers before overwriting them
		while (renderContext->contextLabel[0] != kRenderContextFree)
		{
			// Wait for the EOP event
			SceKernelEvent eopEvent;
			int count;
			ret = sceKernelWaitEqueue(eopEventQueue, &eopEvent, 1, &count, NULL);
			if (ret != SCE_OK)
			{
				printf("sceKernelWaitEqueue failed: 0x%08X\n", ret);
			}
		}

		// Reset the flip GPU label
		renderContext->contextLabel[0] = kRenderContextInUse;

		// Reset the graphical context and initialize the hardware state
		gfxc.reset();
		gfxc.initializeDefaultHardwareState();

		// In a real-world scenario, any rendering of off-screen buffers or
		// other compute related processing would go here

		// The waitUntilSafeForRendering stalls the GPU until the scan-out
		// operations on the current display buffer have been completed.
		// This command is not blocking for the CPU.
		//
		// NOTE
		// This command should be used right before writing the display buffer.
		//
		gfxc.waitUntilSafeForRendering(videoOutHandle, backBuffer->displayIndex);

		// Setup the viewport to match the entire screen.
		// The z-scale and z-offset values are used to specify the transformation
		// from clip-space to screen-space
		gfxc.setupScreenViewport(
			0,			// Left
			0,			// Top
			backBuffer->renderTarget.getWidth(),
			backBuffer->renderTarget.getHeight(),
			0.5f,		// Z-scale
			0.5f);		// Z-offset

		// Bind the render & depth targets to the context
		gfxc.setRenderTarget(0, &backBuffer->renderTarget);
		gfxc.setDepthRenderTarget(&depthTarget);

		// Clear the color and the depth target
		Toolkit::SurfaceUtil::clearRenderTarget(gfxc, &backBuffer->renderTarget, util::kClearColor);
		Toolkit::SurfaceUtil::clearDepthTarget(gfxc, &depthTarget, 1.f);

		// Submit a draw call
	// 	gfxc.setPrimitiveType(Gnm::kPrimitiveTypeTriList);
	// 	gfxc.setIndexSize(Gnm::kIndexSize16);
	// 	gfxc.drawIndex(kIndexCount, indexData);

		// Submit the command buffers, request a flip of the display buffer and
		// write the GPU label that determines the render context state (free)
		// and trigger a software interrupt to signal the EOP event queue
		ret = gfxc.submitAndFlipWithEopInterrupt(
			videoOutHandle,
			backBuffer->displayIndex,
			SCE_VIDEO_OUT_FLIP_MODE_VSYNC,
			0,
			sce::Gnm::kEopFlushCbDbCaches,
			const_cast<uint32_t*>(renderContext->contextLabel),
			kRenderContextFree,
			sce::Gnm::kCacheActionWriteBackAndInvalidateL1andL2);
		if (ret != sce::Gnm::kSubmissionSuccess)
		{
			// Analyze the error code to determine whether the command buffers
			// have been submitted to the GPU or not
			if (ret & sce::Gnm::kStatusMaskError)
			{
				// Error codes in the kStatusMaskError family block submissions
				// so we need to mark this render context as not-in-flight
				renderContext->contextLabel[0] = kRenderContextFree;
			}

			printf("GfxContext::submitAndFlip failed: 0x%08X\n", ret);
		}

		// Signal the system that every draw for this frame has been submitted.
			// This function gives permission to the OS to hibernate when all the
			// currently running GPU tasks (graphics and compute) are done.
		ret = Gnm::submitDone();
		if (ret != SCE_OK)
		{
			printf("Gnm::submitDone failed: 0x%08X\n", ret);
		}

		// Rotate the display buffers
		backBufferIndex = (backBufferIndex + 1) % util::kDisplayBufferCount;
		backBuffer = displayBuffers + backBufferIndex;

		// Rotate the render contexts
		renderContextIndex = (renderContextIndex + 1) % util::kRenderContextCount;
		renderContext = renderContexts + renderContextIndex;

		/** This is where the for loop end */
	}

	// Wait for the GPU to be idle before deallocating its resources
	for (uint32_t i = 0; i < util::kRenderContextCount; ++i)
	{
		if (renderContexts[i].contextLabel)
		{
			while (renderContexts[i].contextLabel[0] != kRenderContextFree)
			{
				sceKernelUsleep(1000);
			}
		}
	}

	// Unregister the EOP event queue
	ret = Gnm::deleteEqEvent(eopEventQueue, Gnm::kEqEventGfxEop);
	if (ret != SCE_OK)
	{
		printf("Gnm::deleteEqEvent failed: 0x%08X\n", ret);
	}

	// Destroy the EOP event queue
	ret = sceKernelDeleteEqueue(eopEventQueue);
	if (ret != SCE_OK)
	{
		printf("sceKernelDeleteEqueue failed: 0x%08X\n", ret);
	}

	// Terminate the video output
	ret = sceVideoOutClose(videoOutHandle);
	if (ret != SCE_OK)
	{
		printf("sceVideoOutClose failed: 0x%08X\n", ret);
	}

	// Releasing manually each allocated resource is not necessary as we are
	// terminating the linear allocators for ONION and GARLIC here.
	onionAllocator.terminate();
	garlicAllocator.terminate();

	return 0;
}