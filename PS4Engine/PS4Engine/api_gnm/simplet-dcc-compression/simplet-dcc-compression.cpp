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
//   "title" : "simplet-dcc-compression",
//   "overview" : "This sample demonstrates the use of DCC compression of render target color buffers.",
//   "explanation" : "This sample demonstrates the use of DCC compression of render target color buffers."
// }

static const unsigned s_shader_p[] = {
	#include "pix_p.h"
};
static const unsigned s_shader_v[] = {
	#include "vex_vv.h"
};
static const unsigned s_copytex2d_c[] = {
	#include "cs_copyrawtexture2d_c.h"
};

static void copyRenderTarget(Gnm::DrawCommandBuffer *dcb, Gnm::RenderTarget *destTarget, const Gnm::RenderTarget *srcTarget, const Gnmx::CsShader *copyShader)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL");
	SCE_GNM_ASSERT_MSG(destTarget, "destTarget must not be NULL");
	SCE_GNM_ASSERT_MSG(srcTarget, "srcTarget must not be NULL");
	SCE_GNM_ASSERT_MSG(copyShader, "copyShader must not be NULL");
	// This function assumes src and dest targets are identical, except for their tilemodes.
	// It could be generalized to support format conversion (using cs_copytexture2d instead of cs_copyrawtexture2d), but would lose the ability to
	// write to sRGB targets.
	SCE_GNM_ASSERT_MSG(destTarget->getWidth() == srcTarget->getWidth() && destTarget->getHeight() == srcTarget->getHeight(), "src and dest dimensions must match");
	SCE_GNM_ASSERT_MSG(destTarget->getDataFormat().m_asInt == destTarget->getDataFormat().m_asInt, "src and dest format must match");

	const Gnm::InputUsageSlot *copyTexUsageSlots = copyShader->getInputUsageSlotTable();
	SCE_GNM_ASSERT_MSG(copyTexUsageSlots[0].m_usageType == Gnm::kShaderInputUsageImmResource,   "unexpected usage type %d for shader slot %d", copyTexUsageSlots[0].m_usageType, 0);
	SCE_GNM_ASSERT_MSG(copyTexUsageSlots[1].m_usageType == Gnm::kShaderInputUsageImmRwResource, "unexpected usage type %d for shader slot %d", copyTexUsageSlots[1].m_usageType, 1);

	Gnm::Texture fbTex;
	fbTex.initFromRenderTarget(srcTarget, false);
	Gnm::Texture displayTex;
	displayTex.initFromRenderTarget(destTarget, false);
	// displayTex will be a R/W texture, so can't use the default RO memory type
	displayTex.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
	// R/W sRGB textures don't work. Change them to plain old UNorm instead for copying purposes; we're just copying raw bits anyway.
	Gnm::DataFormat fmt = fbTex.getDataFormat();
	if (fmt.m_bits.m_channelType == Gnm::kTextureChannelTypeSrgb)
		fmt.m_bits.m_channelType = Gnm::kTextureChannelTypeUNorm;
	fbTex.setDataFormat(fmt);
	displayTex.setDataFormat(fmt);

	dcb->waitForGraphicsWrites(srcTarget->getBaseAddress256ByteBlocks(), srcTarget->getSliceSizeInBytes()/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
		Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);
	dcb->setTsharpInUserData(Gnm::kShaderStageCs, 0, &fbTex);
	dcb->setTsharpInUserData(Gnm::kShaderStageCs, 8, &displayTex);
	dcb->setCsShader(&copyShader->m_csStageRegisters);
	dcb->dispatch( (displayTex.getWidth()+7)/8, (displayTex.getHeight()+7)/8, 1);
}

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

	// This sample renders to an offscreen buffer, and then samples from the offscreen buffer to generate the final display target.
	// This is necessary to test the DCC-without-decompress code path.
	Gnm::TileMode fbTileMode = Gnm::kTileModeThin_2dThin;

	Gnm::RenderTargetSpec rtSpec;
	rtSpec.init();
	rtSpec.m_width = targetWidth;
	rtSpec.m_height = targetHeight;
	rtSpec.m_pitch = 0;
	rtSpec.m_numSlices = 1;
	rtSpec.m_colorFormat = format;
	rtSpec.m_colorTileModeHint = fbTileMode;
	rtSpec.m_minGpuMode = Gnm::kGpuModeNeo;
	rtSpec.m_numSamples = Gnm::kNumSamples1;
	rtSpec.m_numFragments = Gnm::kNumFragments1;
	rtSpec.m_flags.asInt = 0;
	//rtSpec.m_flags.enableCmaskFastClear = 1;
	rtSpec.m_flags.enableDccCompression = 1;
	//rtSpec.m_flags.enableColorTextureWithoutDecompress = 1;
	Gnm::RenderTarget fbTarget;
	int32_t rtStatus = fbTarget.init(&rtSpec);
	SCE_GNM_ASSERT_MSG(rtStatus == SCE_GNM_OK, "RenderTarget init failed (0x%08X)", rtStatus);
	fbTarget.setBaseAddress( SimpletUtil::allocateGarlicMemory(fbTarget.getColorSizeAlign()) );
	if (fbTarget.getCmaskFastClearEnable())
		fbTarget.setCmaskAddress( SimpletUtil::allocateGarlicMemory(fbTarget.getCmaskSizeAlign()) );
	if (fbTarget.getFmaskCompressionEnable())
		fbTarget.setFmaskAddress( SimpletUtil::allocateGarlicMemory(fbTarget.getFmaskSizeAlign()) );
	if (fbTarget.getDccCompressionEnable())
		fbTarget.setDccAddress( SimpletUtil::allocateGarlicMemory(fbTarget.getDccSizeAlign()) );
	if (fbTarget.getCmaskAddress() && !fbTarget.getFmaskAddress())
		fbTarget.disableFmaskCompressionForMrtWithCmask();
	fbTarget.setCmaskClearColor(0xFF004040, 0x00000000);
	if (fbTarget.getDccCompressionEnable())
	{
		// 0x00 = DCC fast cleared to RGBA=[0,0,0,0], if TC-compatible.
		// 0xFF = uncompressed (recommended value at init time)
		memset(fbTarget.getDccAddress(), 0xFF, fbTarget.getDccSliceSizeInBytes()*rtSpec.m_numSlices);
		if (rtSpec.m_flags.enableCmaskFastClear && rtSpec.m_flags.enableFmaskCompression)
		{
			memset(fbTarget.getCmaskAddress(), 0xCC, fbTarget.getCmaskSliceSizeInBytes()*rtSpec.m_numSlices);
		}
		else if (rtSpec.m_flags.enableCmaskFastClear && !rtSpec.m_flags.enableFmaskCompression)
		{
			memset(fbTarget.getCmaskAddress(), 0xFF, fbTarget.getCmaskSliceSizeInBytes()*rtSpec.m_numSlices);
		}
	}

	//
	// Load all shader binaries to video memory
	//
	Gnmx::VsShader  *vertexShader = SimpletUtil::LoadVsShaderFromMemory(s_shader_v);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);
	Gnmx::PsShader  *pixelShader = SimpletUtil::LoadPsShaderFromMemory(s_shader_p);
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

	gfxc.setRenderTarget(0, &fbTarget);
	gfxc.setRenderTargetMask(0xF);
	gfxc.setupScreenViewport(0, 0, targetWidth, targetHeight, 0.5f, 0.5f);

	// Clear the CMASK buffer.
	if (fbTarget.getCmaskFastClearEnable())
	{
		Gnmx::Toolkit::SurfaceUtil::clearCmaskSurface(gfxc, &fbTarget);
	}
	// Set which stages are enabled
	gfxc.setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
	// Set shaders
	gfxc.setVsShader(vertexShader, 0, nullptr, &vertexShader_offsetsTable);
	gfxc.setPsShader(pixelShader,&pixelShader_offsetsTable);

	// Initiate the actual rendering
	gfxc.setPrimitiveType(Gnm::kPrimitiveTypeTriList);
	gfxc.drawIndexAuto(3);

	// fbTarget.setCmaskClearColor(0xFF808000, 0x00000000); // uncomment and look, fast-clear artifacts along the triangle edges!

	gfxc.waitForGraphicsWrites(fbTarget.getBaseAddress256ByteBlocks(), fbTarget.getSliceSizeInBytes()/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
		Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);
	if (fbTarget.getDccCompressionEnable() && !rtSpec.m_flags.enableColorTextureWithoutDecompress)
	{
		Gnmx::decompressDccSurface(&gfxc, &fbTarget); // implicit fast-clear as well.
	}
	else if (fbTarget.getCmaskFastClearEnable())
	{
		Gnmx::eliminateFastClear(&gfxc, &fbTarget);
	}

	Gnm::TileMode displayTileMode = Gnm::kTileModeDisplay_2dThin;
	Gnm::RenderTarget displayTarget;
	Gnm::SizeAlign displaySA = Gnmx::Toolkit::init(&displayTarget, fbTarget.getWidth(), fbTarget.getHeight(), 1, fbTarget.getDataFormat(),
		displayTileMode, fbTarget.getNumSamples(), fbTarget.getNumFragments(), NULL, NULL);
	void *displayBaseAddr = SimpletUtil::allocateGarlicMemory(displaySA);
	displayTarget.setAddresses(displayBaseAddr, nullptr, nullptr);
	Gnmx::CsShader *copyTexShader     = SimpletUtil::LoadCsShaderFromMemory(s_copytex2d_c);
	copyRenderTarget(&gfxc.m_dcb, &displayTarget, &fbTarget, copyTexShader);


	//
	// Add a synchronization point:
	//
	gfxc.writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void *)label, 0x1, Gnm::kCacheActionNone);

	SimpletUtil::registerRenderTargetForDisplay(&displayTarget);

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
			fbTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture will never be bound as an RWTexture, so it's safe to declare as read-only.
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
