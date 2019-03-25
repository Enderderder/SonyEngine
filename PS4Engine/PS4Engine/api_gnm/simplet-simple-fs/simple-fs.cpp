/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

// Simplet #3
// This simplet demonstrates how to use a fetch shader and remap table to selectively fetch vertex input data from a vertex buffer.

#include "../toolkit/simplet-common.h"
#include <pm4_dump.h>
using namespace sce;

bool screenshot = false;

// {
//   "title" : "simplet-simple-fs",
//   "overview" : "This sample demonstrates simple fetch shading.",
//   "explanation" : "This sample creates a fetch shader to match a vertex shader, and uses it to shade vertices in a simple rendering."
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
	uint32_t targetWidth  = 1920;
	uint32_t targetHeight = 1080;
	SimpletUtil::getResolution(targetWidth, targetHeight);

	//
	// Setup the render target:
	//

	Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb;
	Gnm::RenderTarget fbTarget;
	Gnm::TileMode tileMode;
	GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeColorTargetDisplayable, format, 1);
	Gnm::SizeAlign fbSize = Gnmx::Toolkit::init(&fbTarget, targetWidth, targetHeight, 1, format, tileMode, Gnm::kNumSamples1, Gnm::kNumFragments1, NULL, NULL);

	void			*fbCpuBaseAddr = SimpletUtil::allocateGarlicMemory(fbSize);
	uint8_t *fbBaseAddr = static_cast<uint8_t*>(fbCpuBaseAddr);
	memset(fbCpuBaseAddr, 0xFF, fbSize.m_size);

	fbTarget.setAddresses(fbBaseAddr, nullptr, nullptr);



	//
	// Load the vertex and pixel shader binary to video memory
	//

	Gnmx::VsShader	*vertexShader = SimpletUtil::LoadVsShaderFromMemory(s_vex_vv);
	Gnmx::PsShader	*pixelShader = SimpletUtil::LoadPsShaderFromMemory(s_pix_p);


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

	// Setup the render target and some render state
	dcb.setRenderTarget(0, &fbTarget);
	dcb.setDepthRenderTarget(NULL);
	dcb.setRenderTargetMask(0xF);
	Gnm::ClipControl clipReg;
	clipReg.init();
	dcb.setClipControl(clipReg);

	Gnmx::setupScreenViewport(&dcb, 0, 0, targetWidth, targetHeight, 0.5f, 0.5f);

	// Set the vertex shader and the pixel shader
	dcb.setVsShader(&vertexShader->m_vsStageRegisters, 0);
	dcb.setPsShader(&pixelShader->m_psStageRegisters);


	//


	// This vertex shader takes a position and a color as input.
	// A fetch shader needs to be created to gather these data and
	// it needs to be hooked to the vertex shader.


	//


	// Initialize the Constant Buffer
	struct PosColor
	{
		float    pos[4];				// Position for the vertex shader
		uint32_t garbage[3];			// Random data the fetch shader will skip
		uint8_t  color[4];				// Color for the vertex shader
	};

	PosColor vbdata[4] =
	{
		{
			{ -0.5f, -0.5f, 0.0, 1.0f },
			{ 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF},
			{ 255, 0, 0, 255 }
		},

		{
			{ -0.5f, 0.5f, 0.0, 1.0f },
			{ 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF},
			{ 0, 255, 255, 255 }
		},

		{
			{ 0.5f, 0.5f, 0.0, 1.0f },
			{ 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF},
			{ 255, 255, 0, 255 }
		},

		{
			{ 0.5f, -0.5f, 0.0, 1.0f },
			{ 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF},
			{ 0, 0, 255, 255 }
		},
	};

	// The VB needs to be in GPU accessible memory:
	void *vb = SimpletUtil::allocateGarlicMemory(sizeof(vbdata), Gnm::kAlignmentOfBufferInBytes);
	uint8_t *vbBaseAddr = static_cast<uint8_t*>(vb);
	memcpy(vb, vbdata, sizeof(vbdata));


	//


	// Each element of the VB needs to be described by Gnm::Buffer instance. These instances need to be placed in an array.
	// By default, the order in the array should match the order of the vertex shader inputs
	//
	// In case the order in the VB doesn't match the vertex input; two solutions are possible:
	// - creating a new array of Gnm::Buffer to describe the expected input.
	// - or, using a fetch shader remap table (as shown below)


	//

	//
	// Setup the array of vertex buffer descriptors
	//

	// The array of buffer descriptors also needs to be in GPU accessible memory, because we're bypassing the CUE and
	// need to refer to the array directly from the GPU.
	Gnm::Buffer *buffers = (Gnm::Buffer*)SimpletUtil::allocateGarlicMemory(sizeof(Gnm::Buffer)*3, Gnm::kAlignmentOfBufferInBytes);
	buffers[0].initAsVertexBuffer(vbBaseAddr,    Gnm::kDataFormatR32G32B32A32Float, sizeof(PosColor), 4);
	buffers[1].initAsVertexBuffer(vbBaseAddr+16, Gnm::kDataFormatR32G32B32Uint,      sizeof(PosColor), 4);
	buffers[2].initAsVertexBuffer(vbBaseAddr+28, Gnm::kDataFormatR8G8B8A8Unorm,     sizeof(PosColor), 4);

	buffers[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
	buffers[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
	buffers[2].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
	//


	// Semantic wise, the vertex shader expects:
	//   0 -> Position
	//   1 -> Color
	//
	// The vertex buffer is setup as follow:
	//   0 -> Position
	//   1 -> Garbage
	//   2 -> Color
	//
	// The remap semantic table is a table an integer describing the expected vertex input position for each element
	// in the array of vertex buffer descritor.
	//
	// In this sample:
	//   0 -> Position  ->  0
	//   1 -> Garbage   -> -1 -- Unused value.
	//   2 -> Color     ->  1
	//
	// The value of -1 is purely arbitrary; the importance is to have a value which cannot be matched with a vertex semantic.
	// In this case since the vertex shader have only 2 input semantic value (0 for the position and 1 for the color), we
	// could have used: 2 to represent the unused value.

	uint32_t remapSemanticTable[3] = {0, 0xFFFFFFFF, 1};


	//


	// The following section creates the fetch shader.
	// The creation process is done in three steps

	// 1. Create a fetch shader build state:
	Gnm::FetchShaderBuildState fb = {0};
	Gnm::generateVsFetchShaderBuildState(&fb, (const Gnm::VsStageRegisters*)&vertexShader->m_vsStageRegisters, vertexShader->m_numInputSemantics, nullptr, 0, fb.m_vertexBaseUsgpr, fb.m_instanceBaseUsgpr);
	fb.m_numInputSemantics				= vertexShader->m_numInputSemantics;
	fb.m_inputSemantics					= vertexShader->getInputSemanticTable();
	fb.m_numInputUsageSlots       = vertexShader->m_common.m_numInputUsageSlots;
	fb.m_inputUsageSlots				= vertexShader->getInputUsageSlotTable();
	fb.m_numElementsInRemapTable	= sizeof(remapSemanticTable)/sizeof(uint32_t);
	fb.m_semanticsRemapTable	= remapSemanticTable;

	// 2. Allocate the fetch shader video memory
	uint32_t *fs = (uint32_t *)SimpletUtil::allocateGarlicMemory(fb.m_fetchShaderBufferSize, Gnm::kAlignmentOfFetchShaderInBytes);

	// 3. Create the fetch shader itself:
	Gnm::generateFetchShader(fs, &fb);

	vertexShader->m_vsStageRegisters.applyFetchShaderModifier(fb.m_shaderModifier);

	// Vertex shader binary code contains a input usage slot table describing how its resources need to be set.
	// In this simplet, the setup of this resources is hardcoded.
	// The following code make sure the hardcoded settings match the expected input to avoid blue screening.
	//
	// Note that: the Gnmx::ConstantUpdateEngine does the mapping automatically.

	const uint32_t fsStartRegister = SimpletUtil::getStartRegister(vertexShader->getInputUsageSlotTable(), vertexShader->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageSubPtrFetchShader, 0);
	const uint32_t buffersStartRegister = SimpletUtil::getStartRegister(vertexShader->getInputUsageSlotTable(), vertexShader->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsagePtrVertexBufferTable, 0);

	SCE_GNM_ASSERT(fsStartRegister < 16);
	SCE_GNM_ASSERT(buffersStartRegister < 16);

	dcb.setPointerInUserData(Gnm::kShaderStageVs, fsStartRegister, fs);
	dcb.setPointerInUserData(Gnm::kShaderStageVs, buffersStartRegister, buffers);

	uint32_t psInputs[32];
	Gnm::generatePsShaderUsageTable(psInputs,
									  vertexShader->getExportSemanticTable(), vertexShader->m_numExportSemantics,
									  pixelShader->getPixelInputSemanticTable(), pixelShader->m_numInputSemantics);
	dcb.setPsShaderUsage(psInputs, pixelShader->m_numInputSemantics);


	//


	// The setup is now complete; time to draw the quad:
	dcb.setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
	dcb.drawIndexAuto(4);
	dcb.writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, (void *)label, Gnm::kEventWriteSource64BitsImmediate, 0x1, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);

	SimpletUtil::registerRenderTargetForDisplay(&fbTarget);

	//
	// Dumping the PM4 stream:
	//

	void *cbAddrGPU	= dcb.m_beginptr;
	uint32_t	cbSizeInByte = static_cast<uint32_t>(dcb.m_cmdptr - dcb.m_beginptr)*4;
	//
	FILE *file = fopen("/app0/log.pm4", "w");
	Gnm::Pm4Dump::dumpPm4PacketStream(file, (uint32_t*)cbAddrGPU, cbSizeInByte/4);
	fclose(file);

	//

	bool done = false;
	while (!done)
	{
		*label =2;

		// Submit the command buffer:
		void *ccbAddrGPU = 0;
		uint32_t ccbSizeInByte = 0;
		int32_t state = Gnm::submitCommandBuffers(1, &cbAddrGPU, &cbSizeInByte, &ccbAddrGPU, &ccbSizeInByte);
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
