/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

// Simplet #5
// This simplet shows the synchronization process between CCB and DCB.

#include "../toolkit/simplet-common.h"
using namespace sce;

bool screenshot = false;

// {
//   "title" : "simplet-constant-engine",
//   "overview" : "This sample demonstrates how to use the constant update engine without the benefit of the Gnmx library.",
//   "explanation" : "This sample uses low-level Gnm commands to implement constant update functionality to render a small array of quadrilaterals."
// }

static const unsigned s_pix_p[] = {
	#include "pix_p.h"
};
static const unsigned s_vex_vv[] = {
	#include "vex_vv.h"
};

static void Test(bool usingGnmx)
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

	// Creating render target descriptor and initializing it.
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
	//

	Gnmx::VsShader *vertexShader = SimpletUtil::LoadVsShaderFromMemory(s_vex_vv);
	Gnmx::PsShader  *pixelShader = SimpletUtil::LoadPsShaderFromMemory(s_pix_p);


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

	float vbdata[] =
	{
		ADD_QUAD(-0.6f, -0.4f, 0.2f)
		ADD_QUAD( 0.0f, -0.4f, 0.2f)
		ADD_QUAD( 0.6f, -0.4f, 0.2f)

		ADD_QUAD(-0.6f,	 0.4f, 0.2f)
		ADD_QUAD( 0.0f,	 0.4f, 0.2f)
		ADD_QUAD( 0.6f,	 0.4f, 0.2f)
	};

	void *vb = SimpletUtil::allocateGarlicMemory(sizeof(vbdata), Gnm::kAlignmentOfBufferInBytes);
	memcpy(vb, vbdata, sizeof(vbdata));


	//

	const uint32_t vertexSize = sizeof(float)*4;
	const uint32_t kNumQuads = sizeof(vbdata) / (vertexSize*4);
	Gnm::Buffer *vbds = (Gnm::Buffer *)SimpletUtil::allocateGarlicMemory(sizeof(Gnm::Buffer)*kNumQuads, Gnm::kAlignmentOfBufferInBytes);

	Gnm::Buffer vb_template;
	vb_template.initAsVertexBuffer(NULL, Gnm::kDataFormatR32G32B32A32Float, vertexSize, 4);
	for (uint32_t iQuad = 0; iQuad < kNumQuads; ++iQuad)
	{
		vb_template.setBaseAddress(static_cast<uint8_t*>(vb) + iQuad * 64);
		memcpy(vbds+iQuad, &vb_template, sizeof(vb_template));
	}


	//
	// Setup the data for the CB
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
	fb.m_numInputSemantics			   = vertexShader->m_numInputSemantics;
	fb.m_inputSemantics				   = vertexShader->getInputSemanticTable();
	fb.m_numInputUsageSlots	   = vertexShader->m_common.m_numInputUsageSlots;
	fb.m_inputUsageSlots			   = inputUsageSlots;
	fb.m_numElementsInRemapTable = 0;
	fb.m_semanticsRemapTable   = 0;

	uint32_t *fs = (uint32_t *)SimpletUtil::allocateGarlicMemory(fb.m_fetchShaderBufferSize, Gnm::kAlignmentOfFetchShaderInBytes);
	Gnm::generateFetchShader(fs, &fb);

	const uint32_t fetchShaderStartRegister       = SimpletUtil::getStartRegister(inputUsageSlots, vertexShader->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageSubPtrFetchShader, 0);
	const uint32_t vertexBufferTableStartRegister = SimpletUtil::getStartRegister(inputUsageSlots, vertexShader->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsagePtrVertexBufferTable, 0);
	const uint32_t constantBufferStartRegister    = SimpletUtil::getStartRegister(inputUsageSlots, vertexShader->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageImmConstBuffer, 0);

	SCE_GNM_ASSERT(fetchShaderStartRegister       < 16);
	SCE_GNM_ASSERT(vertexBufferTableStartRegister < 16);
	SCE_GNM_ASSERT(constantBufferStartRegister    < 16);

	//
	// Initilize the DCB and CCB
	//

	Gnmx::GnmxDrawCommandBuffer dcb;
	Gnmx::GnmxConstantCommandBuffer ccb;
	dcb.init(SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes, NULL, NULL);
	ccb.init(SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes, NULL, NULL);


	// Set the render target and some render states:
	dcb.setRenderTarget(0, &fbTarget);
	dcb.setRenderTargetMask(0xF);

	Gnmx::setupScreenViewport(&dcb, 0, 0, targetWidth, targetHeight, 0.5f, 0.5f);

	// The sample draws 6 different quads on the screen.
	// Each quad is drawn using different draw commands.
	//
	// The quads are drawn in different colors fetching them from the attached constant buffer.
	// The change of constant buffer between draws will be done using the CCB pipe.
	// This can either be managed manually using low-level Gnm commands, or through the higher-level
	// Gnmx::ConstantUpdateEngine class. The following code demonstrates both methods.
	//
	if ( usingGnmx )
	{
		//------------------------------------------------------------------------------------------------//
		//
		// With Contant Update Engine:
		//
		//------------------------------------------------------------------------------------------------//

		//
		// Note that the following code will not rely on the Gnmx::GfxContext wrapper.
		//

		// Initialize the Constant Update Engine:
		const uint32_t	kNumRingEntries	   = 64;
		const uint32_t	cueHeapSize		   = Gnmx::ConstantUpdateEngine::computeHeapSize(kNumRingEntries);

		Gnmx::ConstantUpdateEngine cue;
		cue.init(SimpletUtil::allocateGarlicMemory(cueHeapSize, Gnm::kAlignmentOfBufferInBytes),kNumRingEntries);
		cue.bindCommandBuffers(&dcb,&ccb,NULL);


		// When using the ContantUpdateEngine, every resources need to be set through it:
		// including the shaders.
		// Note that the vertex shader and the fetch shader aren't individually set but rather as a pair.
		// When no fetch shader is needed; the code should pass 0 or NULL for this argument.
		cue.setVsShader(vertexShader, fb.m_shaderModifier, fs);
		cue.setPsShader(pixelShader);


		// The constant buffer size is 16 floats
		// Each color block will be copied in this memory region when needed
		void			*cb		= SimpletUtil::allocateGarlicMemory(sizeof(cbdata), Gnm::kAlignmentOfBufferInBytes);
		uint8_t *cbAddr = static_cast<uint8_t*>(cb);
		memcpy(cb, cbdata, sizeof(cbdata));

		// buf_col default settings.
		// The base address will be set in the loop.
		Gnm::Buffer buf_col;
		buf_col.initAsConstantBuffer(nullptr, 64);

		// For each quad:
		for (int i = 0; i < 6; i++)
		{
			// Set the VB in the CUE
			cue.setVertexBuffers(Gnm::kShaderStageVs, 0, 1, vbds+i);

			// Unlike the code in the else statement below, the actual color data is not written to the cp ram.
			// Instead, a new buffer descriptor is created between draw calls that points to different area of that color array.
			buf_col.setBaseAddress(cbAddr + 64*i);
			cue.setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &buf_col);

			// When using the CUE manually (outside of GfxContext), it is required to surround each draw call with cue.preDraw, and cue.postDraw.
			// These two functions will update the necessary resources and insert the synchronization code.
			cue.preDraw();
			dcb.setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
			dcb.drawIndexAuto(4);
			cue.postDraw();
		}
	}
	else
	{
		//------------------------------------------------------------------------------------------------//
		//
		// Setup the DCB:
		//
		//------------------------------------------------------------------------------------------------//


		// As mentioned above, in this section of code, the update of the constant buffer will be done through cpDma,
		// the ccb will be used to syncronize the update of this memory region shared by all draw calls


		// The constant buffer size is 16 float
		void *cb = SimpletUtil::allocateGarlicMemory(64, Gnm::kAlignmentOfBufferInBytes);

		Gnm::Buffer buf_col = {};
		buf_col.setStride(16);
		buf_col.setNumElements(4);
		buf_col.setFormat(Gnm::kDataFormatR32G32B32A32Float);
		buf_col.setBaseAddress(cb);

		// Setup Vertex Shader and Pixel Shader:
		dcb.setVsShader(&vertexShader->m_vsStageRegisters, fb.m_shaderModifier);
		dcb.setPsShader(&pixelShader->m_psStageRegisters);
		// Note: when using the CUE, there is no need to setup the mapping between the vertex and pixel shader,
		// it's all done automatically internally.
		uint32_t psInputs[32];
		Gnm::generatePsShaderUsageTable(psInputs,
										  vertexShader->getExportSemanticTable(), vertexShader->m_numExportSemantics,
										  pixelShader->getPixelInputSemanticTable(), pixelShader->m_numInputSemantics);
		dcb.setPsShaderUsage(psInputs, pixelShader->m_numInputSemantics);


		// Setup shared user Data:
		// As mentioned above, the constant buffer memory will be patched by the cpDma; the resource descriptor doesn't change
		// between draws.
		dcb.setPointerInUserData(Gnm::kShaderStageVs, fetchShaderStartRegister, fs);
		dcb.setVsharpInUserData(Gnm::kShaderStageVs, constantBufferStartRegister, &buf_col);

		for (uint32_t iQuad = 0; iQuad < 6; ++iQuad)
		{
			dcb.waitOnCe();
			dcb.setPointerInUserData(Gnm::kShaderStageVs, vertexBufferTableStartRegister, vbds + iQuad);
			dcb.setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
			dcb.drawIndexAuto(4);
			dcb.incrementDeCounter();
		}

		// The DCB is now setup, we still need to setup the CCB


		//------------------------------------------------------------------------------------------------//
		//
		// Setup the CCB:
		//
		//------------------------------------------------------------------------------------------------//


		uint8_t *cpWriteAddr = static_cast<uint8_t*>((void *)cb);

		// Write all constant buffer data to the cpRam
		ccb.writeToCpRam(0, cbdata, sizeof(cbdata)/sizeof(uint32_t));

		for (uint16_t iQuad = 0; iQuad < 6; ++iQuad)
		{
			ccb.waitOnDeCounterDiff(1);
			// Dump the constant buffer data needed for the current draw in the cb memory region:
			ccb.cpRamDump(cpWriteAddr, 64*iQuad, 16);
			ccb.incrementCeCounter();
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

	void *dcbAddrGPU = dcb.m_beginptr;
	uint32_t		dcbSize	   = static_cast<uint32_t>(dcb.m_cmdptr - dcb.m_beginptr)*4;
	void *ccbAddrGPU = ccb.m_beginptr;
	uint32_t		ccbSize	   = static_cast<uint32_t>(ccb.m_cmdptr - ccb.m_beginptr)*4;

	//
	//fflush(stdout);
	//freopen(".\\dlog.pm4", "w", stdout);
	//Gnm::Pm4Dump::dumpPm4PacketStream(static_cast<uint32_t*>(Gnm::translateGpuToCpuAddress(dcbAddrGPU)), dcbSize/4, Gnm::translateGpuToCpuAddress);
	//fflush(stdout);
	//freopen(".\\clog.pm4", "w", stdout);
	//Gnm::Pm4Dump::dumpPm4PacketStream(static_cast<uint32_t*>(Gnm::translateGpuToCpuAddress(ccbAddrGPU)), ccbSize/4, Gnm::translateGpuToCpuAddress);
	//freopen("con", "w", stdout);
	//


	bool done = false;
	while (!done)
	{
		*label = 2;

		// Submit the command buffers:
		// Note: we are passing the ccb in the last two arguments; when no ccb is used, 0 should be passed in
		int32_t state = Gnm::submitCommandBuffers(1, &dcbAddrGPU, &dcbSize, &ccbAddrGPU, &ccbSize);
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
	bool usingGnmx  = true;

	if (argc > 1 && !strcmp(argv[1], "screenshot"))
		screenshot = true;

	//
	// Initializes system and video memory and memory allocators.
	//
	SimpletUtil::Init();

	{
		Test(usingGnmx);
	}

	return 0;
}
