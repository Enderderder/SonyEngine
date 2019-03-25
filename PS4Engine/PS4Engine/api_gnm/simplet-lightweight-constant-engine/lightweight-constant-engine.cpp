/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

// Simplet #?
// This simplet shows the setup of the lightweight-ConstantUpdateEngine process with the DCB.

#include "../toolkit/simplet-common.h"

using namespace sce;
using namespace Gnmx;

// {
//   "title" : "simplet-lightweight-constant-engine",
//   "overview" : "This sample demonstrates how to use the lightweight constant update engine using the replacement graphics context.",
//   "explanation" : "This sample uses low-level Gnm commands to implement constant update functionality to render a small array of quadrilaterals."
// }

static const unsigned s_pix_p[] = {
	#include "pix_p.h"
};
static const unsigned s_vex_vv[] = {
	#include "vex_vv.h"
};

static void Test(bool screenshot)
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

	const Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb; // render target pixel format

	Gnm::RenderTarget fbTarget;
	Gnm::TileMode tileMode;
	GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeColorTargetDisplayable, format, 1);
	Gnm::SizeAlign fbSize = Gnmx::Toolkit::init(&fbTarget, targetWidth, targetHeight, 1, format, tileMode, Gnm::kNumSamples1, Gnm::kNumFragments1, NULL, NULL);

	// Allocate render target buffer in video memory
	void *fbCpuBaseAddr = SimpletUtil::allocateGarlicMemory(fbSize);

	// In order to simplify the code, the simplet are using a memset to clear the render target.
	// This method should NOT be used once the GPU start using the memory.
	memset(fbCpuBaseAddr, 0xFF, fbSize.m_size);

	// Set render target memory base address (gpu address)
	uint8_t *fbBaseAddr = static_cast<uint8_t*>(fbCpuBaseAddr);
	fbTarget.setAddresses(fbBaseAddr, nullptr, nullptr);

	//
	// Load the basic VS and PS
	// Note: at load time we will generate and cache the input resource tables, as they only need to be generated once and can be reused during bind time
	// 

	Gnmx::VsShader *vertexShader = SimpletUtil::LoadVsShaderFromMemory(s_vex_vv);
	InputResourceOffsets vertexShaderResourceOffsets;
	generateInputResourceOffsetTable(&vertexShaderResourceOffsets, Gnm::kShaderStageVs, vertexShader);

	Gnmx::PsShader  *pixelShader = SimpletUtil::LoadPsShaderFromMemory(s_pix_p);
	InputResourceOffsets pixelShaderResourceOffsets;
	generateInputResourceOffsetTable(&pixelShaderResourceOffsets, Gnm::kShaderStagePs, pixelShader);

	//
	// Create a synchronization point:
	//

	volatile uint64_t *label = (volatile uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));


	//------------------------------------------------------------------------------------------------//
	//
	// Data Setup
	//
	//------------------------------------------------------------------------------------------------//

	//
	// Setup the VB for 6 quads.
	//

	#define ADD_QUAD(cx, cy, len)					\
			(cx)-(len), (cy)-(len), 0.0f, 1.0f,		\
			(cx)-(len),	(cy)+(len), 0.0f, 1.0f,		\
			(cx)+(len),	(cy)+(len), 0.0f, 1.0f,     \
			(cx)+(len), (cy)-(len), 0.0f, 1.0f,		

	float vertexBufferData[] =
	{
		ADD_QUAD(-0.6f, -0.4f, 0.2f)
		ADD_QUAD( 0.0f, -0.4f, 0.2f)
		ADD_QUAD( 0.6f, -0.4f, 0.2f)

		ADD_QUAD(-0.6f,	 0.4f, 0.2f)
		ADD_QUAD( 0.0f,	 0.4f, 0.2f)
		ADD_QUAD( 0.6f,	 0.4f, 0.2f)
	};

	void *vb = SimpletUtil::allocateGarlicMemory(sizeof(vertexBufferData), Gnm::kAlignmentOfBufferInBytes);
	memcpy(vb, vertexBufferData, sizeof(vertexBufferData));

	const uint32_t vertexSize = sizeof(float)*4;
	const uint32_t kNumQuads = sizeof(vertexBufferData) / (vertexSize*4);
	Gnm::Buffer *vbds = (Gnm::Buffer *)SimpletUtil::allocateGarlicMemory(sizeof(Gnm::Buffer)*kNumQuads, Gnm::kAlignmentOfBufferInBytes);

	Gnm::Buffer vb_template;
	vb_template.initAsVertexBuffer(nullptr, Gnm::kDataFormatR32G32B32A32Float, vertexSize, 4);
	for (uint32_t iQuad = 0; iQuad < kNumQuads; ++iQuad)
	{
		vb_template.setBaseAddress(static_cast<uint8_t*>(vb) + iQuad * 64);
		memcpy(vbds+iQuad, &vb_template, sizeof(vb_template));
	}

	//
	// Setup the data for the vertex colors
	//
	float cbdata[] =
	{
		0.0f, 0.0f, 0.0f, 1.0f, // black
		1.0f, 0.0f, 0.0f, 1.0f, // red
		1.0f, 1.0f, 0.0f, 1.0f, // yellow
		1.0f, 1.0f, 1.0f, 1.0f, // white

		0.0f, 0.0f, 0.0f, 1.0f, // black
		0.0f, 0.0f, 1.0f, 1.0f, // blue
		0.0f, 1.0f, 1.0f, 1.0f, // cyan
		1.0f, 1.0f, 1.0f, 1.0f, // white

		1.0f, 1.0f, 1.0f, 1.0f, // white
		0.0f, 1.0f, 1.0f, 1.0f, // cyan
		0.0f, 0.0f, 1.0f, 1.0f, // blue
		0.0f, 0.0f, 0.0f, 1.0f, // black

		1.0f, 1.0f, 1.0f, 1.0f, // white
		1.0f, 1.0f, 0.0f, 1.0f, // yellow
		1.0f, 0.0f, 0.0f, 1.0f, // red
		0.0f, 0.0f, 0.0f, 1.0f, // black

		1.0f, 0.0f, 0.0f, 1.0f, // red
		0.0f, 1.0f, 0.0f, 1.0f, // green
		0.0f, 0.0f, 1.0f, 1.0f, // blue
		1.0f, 0.0f, 1.0f, 1.0f, // magenta

		1.0f, 0.0f, 1.0f, 1.0f, // magenta
		0.0f, 0.0f, 1.0f, 1.0f, // blue
		0.0f, 1.0f, 0.0f, 1.0f, // yellow
		1.0f, 0.0f, 0.0f, 1.0f, // red
	};

	//
	// Generate the Fetch Shader:
	//

	Gnm::FetchShaderBuildState fb = {0};
	Gnm::generateVsFetchShaderBuildState(&fb, (const Gnm::VsStageRegisters*)&vertexShader->m_vsStageRegisters, vertexShader->m_numInputSemantics, nullptr, 0, fb.m_vertexBaseUsgpr, fb.m_instanceBaseUsgpr);

	const Gnm::InputUsageSlot *inputUsageSlots = vertexShader->getInputUsageSlotTable();
	fb.m_numInputSemantics				= vertexShader->m_numInputSemantics;
	fb.m_inputSemantics					= vertexShader->getInputSemanticTable();
	fb.m_numInputUsageSlots				= vertexShader->m_common.m_numInputUsageSlots;
	fb.m_inputUsageSlots				= inputUsageSlots;
	fb.m_numElementsInRemapTable		= 0;
	fb.m_semanticsRemapTable			= 0;

	uint32_t *fs = (uint32_t *)SimpletUtil::allocateGarlicMemory(fb.m_fetchShaderBufferSize, Gnm::kAlignmentOfFetchShaderInBytes);
	Gnm::generateFetchShader(fs, &fb);

	const uint32_t fetchShaderStartRegister       = SimpletUtil::getStartRegister(inputUsageSlots, vertexShader->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageSubPtrFetchShader, 0);
	const uint32_t vertexBufferTableStartRegister = SimpletUtil::getStartRegister(inputUsageSlots, vertexShader->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsagePtrVertexBufferTable, 0);
	const uint32_t constantBufferStartRegister    = SimpletUtil::getStartRegister(inputUsageSlots, vertexShader->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageImmConstBuffer, 0);

	SCE_GNM_ASSERT(fetchShaderStartRegister       < 16);
	SCE_GNM_ASSERT(vertexBufferTableStartRegister < 16);
	SCE_GNM_ASSERT(constantBufferStartRegister    < 16);

	//
	// Initialize the DCB and LCUE
	//

	// Initialize the draw command buffer
	GnmxDrawCommandBuffer dcb;
	dcb.init(SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes, NULL, NULL);

	// Initialize the Lightweight Constant Update Engine
	// Note: The size of the resource buffer is arbitrary for the sample (and entirely up to the user)
	// it just needs to be large enough to contain all resource bindings required by the submit. (eg. submit resource buffer size * resource buffer count)
	const uint32_t kResourceBufferSize = 1*1024*1024;
	const uint32_t kResourceBufferCount = 1;
	uint32_t* resourceBuffer = (uint32_t*)SimpletUtil::allocateGarlicMemory(kResourceBufferSize, Gnm::kAlignmentOfBufferInBytes);

	LightweightGraphicsConstantUpdateEngine lwcue;
	lwcue.init(&resourceBuffer, kResourceBufferCount, uint32_t(kResourceBufferSize/sizeof(uint32_t)), NULL);
	lwcue.setDrawCommandBuffer(&dcb); // the LCUE will defer bindings such as shaders until draw time, so it's necessary to provide a pointer to the dcb.
	
	//------------------------------------------------------------------------------------------------//
	//
	// Generate GPU Command Stream
	//
	//------------------------------------------------------------------------------------------------//

	// Set default rendering states for beginning of frame, and 
	// bind the render target
	dcb.setRenderTarget(0, &fbTarget);
	dcb.setRenderTargetMask(0xF);

	Gnmx::setupScreenViewport(&dcb, 0, 0, targetWidth, targetHeight, 0.5f, 0.5f);

	// The sample draws 6 different quads on the screen.
	// Each quad is drawn using different draw commands.
	//
	// The quads are drawn in different colors fetching them from the attached constant buffer.
	// The change of constant buffer between draws will be done via binding the base address to the LCUE.
	{
		//
		// Note: the following code will not rely on the Gnmx::GfxContext wrapper.
		//

		// When using the Lightweight-ContantUpdateEngine, every resource needs to be set through it
		// including shaders.
		// Note: When using the LCUE, shader bindings must occur prior to any resource bindings required, 
		// failing to do so will result in undefined behavior. (corrupt output, crashes)
		// Note: Vertex shader and the fetch shader aren't individually set but rather as a pair.
		// While this is common, the LCUE does allows them to be set individually (see Gnm reference for more information)
		lwcue.setVsShader(vertexShader, fb.m_shaderModifier, fs, &vertexShaderResourceOffsets);
		lwcue.setPsShader(pixelShader, &pixelShaderResourceOffsets);

		// The constant buffer size is 16 floats
		// Each color block will be copied in this memory region when needed
		void *cb = SimpletUtil::allocateGarlicMemory(sizeof(cbdata), Gnm::kAlignmentOfBufferInBytes);
		uint8_t *cbAddr = static_cast<uint8_t*>(cb);
		memcpy(cb, cbdata, sizeof(cbdata));	// this will copy the colorData into garlic memory

		// buf_col default settings.
		// The base address will be set in the loop.
		Gnm::Buffer buf_col;
		buf_col.initAsConstantBuffer(nullptr, 64);

		// For each quad:
		for(unsigned i = 0; i < kNumQuads; i++)
		{
			// Set the Vertex Buffer in the LCUE
			lwcue.setVertexBuffers(Gnm::kShaderStageVs, 0, 1, vbds+i);

			// Set base address of the colorBuffer in the resource descriptor, and bind the constant buffer to the LCUE
			buf_col.setBaseAddress(cbAddr + 64*i);
			lwcue.setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &buf_col);

			// When using the LCUE manually (outside of LCUE GraphicsContext), it is required to call preDraw() before a draw.
			// preDraw() will commit the necessary resources descriptors to memory for the batch and bind required shaders.
			lwcue.preDraw();
			dcb.setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
			dcb.drawIndexAuto(4);
		}
	}

	//
	// Add a synchronization point:
	//

	dcb.writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, (void *)label, Gnm::kEventWriteSource64BitsImmediate, 0x1, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);

	//------------------------------------------------------------------------------------------------//
	//
	// Display / Submits:
	//
	//------------------------------------------------------------------------------------------------//

	SimpletUtil::registerRenderTargetForDisplay(&fbTarget);

	//
	// Dumping the PM4 streams:
	//

	// Note: we only need to setup the DCB pointers for the submit, LCUE does not use the CCB.
	void *dcbAddrGPU = dcb.m_beginptr;
	uint32_t dcbSize = static_cast<uint32_t>(dcb.m_cmdptr - dcb.m_beginptr)*4;

//	fflush(stdout);
// 	freopen(".\\dlog.pm4", "w", stdout);
// 	Gnm::Pm4Dump::dumpPm4PacketStream(static_cast<uint32_t*>(Gnm::translateGpuToCpuAddress(dcbAddrGPU)), dcbSize/4, Gnm::translateGpuToCpuAddress);
// 	fflush(stdout);
// 	fclose(stdout);

	bool done = false;
	while (!done)
	{
		// Reset finish label to "2" can be any number but "1"
		*label = 2;

		// Submit the command buffers:
		// Note: we are passing 0 for the ccb in the last two arguments; LCUE does not use the CCB so this needs to remain null
		int32_t state = Gnm::submitCommandBuffers(1, &dcbAddrGPU, &dcbSize, 0, 0);
		SCE_GNM_UNUSED(state);
 		SCE_GNM_ASSERT(state == sce::Gnm::kSubmissionSuccess);

		// Block CPU from continuing until the GPU is complete (waiting for the GPU to write "1" in "GPUCompleteLabel")
		uint32_t wait = 0;
		while (*label != 1)
			++wait;

		if(screenshot)
 		{
 			Gnm::Texture fbTexture;
 			fbTexture.initFromRenderTarget(&fbTarget, false);
 			SimpletUtil::SaveTextureToTGA(&fbTexture, "screenshot.tga");
			screenshot = false;
 		}

		// Display the render target on the screen
		SimpletUtil::requestFlipAndWait();

		// Check to see if user has requested to quit the loop
		done = SimpletUtil::handleUserEvents();
	}
}


int main(int argc, char *argv[])
{
	bool screenshot = false;
	if (argc > 1 && !strcmp(argv[1], "screenshot"))
		screenshot = true;

	//
	// Initializes system and video memory and memory allocators.
	//
	SimpletUtil::Init();

	{
		Test(screenshot);
	}

	return 0;
}
