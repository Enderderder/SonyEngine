/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

// Simplet #1
// This simplet is bare bone gnm code to render a triangle on the screen.

#include "../toolkit/simplet-common.h"
#include <pm4_dump.h>
using namespace sce;

bool screenshot = false;

// {
//   "title" : "simplet-single-triangle",
//   "overview" : "This sample demonstrates the simple rendering of a single triangle.",
//   "explanation" : "This sample renders a single triangle with minimal shading."
// }

static const unsigned s_pix_p[] = {
	#include "pix_p.h"
};
static const unsigned s_vex_vv[] = {
	#include "vex_vv.h"
};

static void Test()
{
	//
	// Define window size and set up a window to render to it
	//
	uint32_t targetWidth = 1920;
	uint32_t targetHeight = 1080;
	SimpletUtil::getResolution(targetWidth, targetHeight);

	//
	// Setup the render target:
	//

	const Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb; // render target pixel format

	// Creating render target descriptor and initializing it.
	Gnm::RenderTarget fbTarget;
	Gnm::TileMode tileMode;
	GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeColorTargetDisplayable, format, 1);
	Gnm::SizeAlign fbSize = Gnmx::Toolkit::init(&fbTarget, targetWidth, targetHeight, 1, format, tileMode, Gnm::kNumSamples1, Gnm::kNumFragments1, NULL, NULL);


	// Allocate render target buffer in video memory
	void *fbBaseAddr = SimpletUtil::allocateGarlicMemory(fbSize);

	// In order to simplify the code, the simplet are using a memset to clear the render target.
	// This method should NOT be used once the GPU start using the memory.
	memset(fbBaseAddr, 0xFF, fbSize.m_size);

	// Set render target memory base address (gpu address)
	fbTarget.setAddresses(fbBaseAddr, 0, 0);



	//
	// Load the vertex and pixel shader binary to video memory
	//

	// The toolkit function will load the shader from file; allocate a shared video memory buffer,
	// copy the binary code into it; then return a newly allocated Gnmx::...Shader object.
	// It is up to the caller to free this memory.
	Gnmx::VsShader *vertexShader = SimpletUtil::LoadVsShaderFromMemory(s_vex_vv);
	Gnmx::PsShader *pixelShader = SimpletUtil::LoadPsShaderFromMemory(s_pix_p);



	//
	// Create a synchronization point:
	//

	// Allocate a buffer in video shared memory for synchronization
	// Note: the pointer is "volatile" to make sure the compiler doesn't optimized out the read to that memory address.
	volatile uint64_t *label = (volatile uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));



	//
	// Setup the Command Buffer:
	//

	// Create and initiallize a draw command buffer
	Gnm::DrawCommandBuffer dcb;
	dcb.init(SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes, NULL, NULL);

	// Unless using MRT, the pixel shader will output to render target 0.
	dcb.setRenderTarget(0, &fbTarget);

	// Enable RGBA write to Render Target 0
	// MRT color mask is a bit mask to control which color components are written into which render target
	// starting from the low bits to high bits per render targets (up to 8 render targets).
	dcb.setRenderTargetMask(0xF);

	Gnmx::setupScreenViewport(&dcb, 0, 0, targetWidth, targetHeight, 0.5f, 0.5f);

	// Set the vertex shader and the pixel shader
	// Note:
	//   This vertex shader is very basic and doesn't use any inputs resources such as constant buffer, vertex buffer, fetch shader...
	//   The output vertices are generated using just the vertex index.
	//   The same goes for the pixel shader. It doesn't have any particular input either (no UV...) nor input resources (no textures...).
	//   Therefore there is no need for vs-output -> ps-input mapping.
	dcb.setVsShader(&vertexShader->m_vsStageRegisters, 0);
	dcb.setPsShader(&pixelShader->m_psStageRegisters);


	// Draw the triangle.
	dcb.setPrimitiveType(Gnm::kPrimitiveTypeTriList);
	
	// Using drawIndex:
	//dcb.drawIndexAuto(3);
	dcb.setIndexSize(Gnm::kIndexSize32, Gnm::kCachePolicyBypass);
	void *mem = SimpletUtil::allocateOnionMemory(64, 64);
	const uint32_t indices[] = { 0, 1 , 2 };
	memcpy(mem, indices, sizeof(indices));
	dcb.drawIndex(3, mem);


	// The following line will write at the address "label" the value 0x1.
	// This write will only occur once the previous commands have been completed.
	// It allows synchronization between CPU and GPU.
	dcb.writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, (void *)label, Gnm::kEventWriteSource64BitsImmediate, 0x1, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);

	SimpletUtil::registerRenderTargetForDisplay(&fbTarget);

	//
	// Dumping the PM4 stream:
	//

	// In order to submit a command buffer or dump the PM4 packet stream,
	// the code requires a start address and a size.
	const uint32_t cbSizeInByte = static_cast<uint32_t>(dcb.m_cmdptr - dcb.m_beginptr)*4;

	// The pm4 dump is done in the stdout; the surrounding code just redirect this output
	// inside a log file.
	FILE *file = fopen("/app0/log.pm4", "w");
	Gnm::Pm4Dump::dumpPm4PacketStream(file, dcb.m_beginptr, cbSizeInByte/4);
	fclose(file);

	//


	bool done = false;
	while (!done)
	{
		*label =2;

		// Submit the command buffer:
		void *dcbAddrGPU = dcb.m_beginptr;
		uint32_t dcbSizeInBytes = cbSizeInByte;
		void *ccbAddrGPU = 0;
		uint32_t ccbSizeInBytes = 0;
		int32_t state = Gnm::submitCommandBuffers(1, &dcbAddrGPU, &dcbSizeInBytes, &ccbAddrGPU, &ccbSizeInBytes);
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
