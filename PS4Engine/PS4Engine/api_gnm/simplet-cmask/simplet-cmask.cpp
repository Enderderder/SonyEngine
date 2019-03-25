/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../toolkit/simplet-common.h"
#include <pm4_dump.h>
using namespace sce;

bool screenshot = false;

// {
//   "title" : "simplet-cmask",
//   "overview" : "This sample demonstrates the use of CMASK fast clear of render target color buffers.",
//   "explanation" : "This sample demonstrates the use of CMASK fast clear of render target color buffers."
// }

static const unsigned s_shader_p[] = {
	#include "pix_p.h"
};
static const unsigned s_shader_v[] = {
	#include "vex_vv.h"
};

static void Test()
{
	//
	// Define window size and set up a window to render to it
	//
	uint32_t targetWidth  = 1920;
	uint32_t targetHeight = 1080;
	SimpletUtil::getResolution(targetWidth, targetHeight);

	//
	// Setup the render target:
	//
	Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb;

	Gnm::TileMode tileMode = Gnm::kTileModeDisplay_2dThin;
	GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeColorTargetDisplayable, format, 1);

	Gnm::RenderTarget fbTarget;
	Gnm::SizeAlign cmaskSA;
	Gnm::SizeAlign colorSA = Gnmx::Toolkit::init(&fbTarget, targetWidth, targetHeight, 1, format, tileMode, Gnm::kNumSamples1, Gnm::kNumFragments1, &cmaskSA, nullptr);
	
	void *fbBaseAddr    = SimpletUtil::allocateGarlicMemory(colorSA);
	void *cmaskBaseAddr = SimpletUtil::allocateGarlicMemory(cmaskSA);
	fbTarget.setAddresses(fbBaseAddr, cmaskBaseAddr, nullptr);
	fbTarget.disableFmaskCompressionForMrtWithCmask();

	//
	// Load all shader binaries to video memory
	//
	Gnmx::VsShader  *vertexShader = SimpletUtil::LoadVsShaderFromMemory(s_shader_v);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);

	Gnmx::PsShader *pixelShader = SimpletUtil::LoadPsShaderFromMemory(s_shader_p);
	Gnmx::InputOffsetsCache pixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&pixelShader_offsetsTable, Gnm::kShaderStagePs, pixelShader);

	//
	// Create a synchronization point:
	//
	volatile uint64_t *label = (uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));


	//
	// Setup the Command and constant buffers:
	//
	Gnmx::GnmxGfxContext gfxc;

#if SCE_GNMX_ENABLE_GFX_LCUE
	const uint32_t kNumBatches = 100; // Estimated number of batches to support within the resource buffer
	const uint32_t resourceBufferSize = Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(Gnmx::LightweightConstantUpdateEngine::kResourceBufferGraphics, kNumBatches);
	gfxc.init
		(
		SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes,	// Draw command buffer
		SimpletUtil::allocateGarlicMemory(resourceBufferSize, Gnm::kAlignmentOfBufferInBytes), resourceBufferSize,					                        // Resource buffer
		NULL																															                    // Global resource table 
		);
#else
	const uint32_t kNumRingEntries = 64;
	const uint32_t cueHeapSize = Gnmx::ConstantUpdateEngine::computeHeapSize(kNumRingEntries);
	gfxc.init(SimpletUtil::allocateGarlicMemory(cueHeapSize, Gnm::kAlignmentOfBufferInBytes), kNumRingEntries, // Constant Update Engine
			  SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes,	 // Draw command buffer
			  SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes		 // Constant command buffer
			  );
#endif
	fbTarget.setCmaskClearColor(0xFF004040, 0x00000000);
	// Clear the CMASK buffer.
	if (fbTarget.getCmaskFastClearEnable())
	{
		Gnmx::Toolkit::SurfaceUtil::clearCmaskSurface(gfxc, &fbTarget);
	}

	Gnm::DepthStencilControl dsc;
	dsc.init();
	gfxc.setDepthStencilControl(dsc);
	gfxc.setDepthRenderTarget(nullptr);

	gfxc.setRenderTarget(0, &fbTarget);
	gfxc.setRenderTargetMask(0xF);
	gfxc.setupScreenViewport(0, 0, targetWidth, targetHeight, 0.5f, 0.5f);

	// Set which stages are enabled
	gfxc.setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
	// Set shaders
	gfxc.setVsShader(vertexShader, 0, nullptr, &vertexShader_offsetsTable);
	gfxc.setPsShader(pixelShader, &pixelShader_offsetsTable);

	// Initiate the actual rendering
	gfxc.setPrimitiveType(Gnm::kPrimitiveTypeTriList);
	gfxc.drawIndexAuto(3);

	// clearColor[0] = 0xFF808000;
	// gfxc.setCmaskClearColor(0, clearColor); // uncomment and look, fast-clear artifacts along the triangle edges!

	gfxc.waitForGraphicsWrites(fbTarget.getBaseAddress256ByteBlocks(), fbTarget.getSliceSizeInBytes()/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
		Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);
	if (fbTarget.getCmaskFastClearEnable())
	{
		Gnmx::eliminateFastClear(&gfxc, &fbTarget);
	}

	//
	// Add a synchronization point:
	//
	gfxc.writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void *)label, 0x1, Gnm::kCacheActionNone);

	SimpletUtil::registerRenderTargetForDisplay(&fbTarget);

	//
	// Dumping the PM4 stream:
	//

	// In order to submit a command buffer or dump the PM4 packet stream,
	// the code requires a start address and a size.
	const uint32_t cbSizeInByte = static_cast<uint32_t>(gfxc.m_dcb.m_cmdptr - gfxc.m_dcb.m_beginptr)*4;

	// The pm4 dump is done in the stdout; the surrounding code just redirect this output
	// inside a log file.
	FILE *file = fopen("/app0/log.pm4", "w");
	Gnm::Pm4Dump::dumpPm4PacketStream(file, gfxc.m_dcb.m_beginptr, cbSizeInByte/4);
	fclose(file);

	// Loop over the buffer
	bool done = false;
	while (!done)
	{
		*label =2;

		// Submit the command buffers:
		int32_t state = gfxc.submit();
		SCE_GNM_ASSERT(state == sce::Gnm::kSubmissionSuccess);

		// Wait until it's done (waiting for the GPU to write "1" in "label")
		uint32_t wait = 0;
		while (*label != 1)
			++wait;

		if(screenshot)
 		{
 			Gnm::Texture fbTexture;
 			fbTexture.initFromRenderTarget(&fbTarget, false);
 			SimpletUtil::SaveTextureToTGA(&fbTexture, "screenshot.tga");
			screenshot = 0;
 		}

		// Display the render target on the screen
		SimpletUtil::requestFlipAndWait();

		// Check to see if user has requested to quit the loop
		done = SimpletUtil::handleUserEvents();
	}
}


int main(int argc, char *argv[])
{
	if (argc > 1 && !strcmp(argv[1], "screenshot"))
		screenshot = true;

	//
	// Initializes system and video memory and memory allocators.
	//
	SimpletUtil::Init();

	{
		//
		// Run the test
		//
		Test();
	}

	return 0;
}
