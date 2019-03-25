/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

// Simplet #7
// This simplet shows how to setup ls, hs , ds shaders to create tessellated geometry from input control points

#include "../toolkit/simplet-common.h"
#include "std_cbuffer.h"
using namespace sce;

bool screenshot = false;

// {
//   "title" : "simplet-tessellation",
//   "overview" : "This sample demonstrates hardware tessellation.",
//   "explanation" : "This sample allocates and sets up the buffers required for tessellation, then renders a simple scene to demonstrate this feature."
// }

static const unsigned s_lsshader_vl[] = {
	#include "LSshader_vl.h"
};
static const unsigned s_ccpatch_dv[] = {
	#include "ccpatch_dv.h"
};
static const unsigned s_ccpatch_h[] = {
	#include "ccpatch_h.h"
};
static const unsigned s_shader_p[] = {
	#include "shader_p.h"
};

static void Test()
{
	//
	// Define window size and set up a window to render to it
	//
	uint32_t targetWidth  = 1920;
	uint32_t targetHeight = 1080;
	SimpletUtil::getResolution(targetWidth, targetHeight);

	// set tessellation factor buffer
	void *tfBufferPtr = sce::Gnmx::Toolkit::allocateTessellationFactorRingBuffer();

	//
	// Setup the render target:
	//
	Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb;
	Gnm::RenderTarget fbTarget;
	Gnm::TileMode tileMode;
	GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeColorTargetDisplayable, format, 1);
	Gnm::SizeAlign fbSize = Gnmx::Toolkit::init(&fbTarget, targetWidth, targetHeight, 1, format, tileMode, Gnm::kNumSamples1, Gnm::kNumFragments1, NULL, NULL);

	void			*fbCpuBaseAddr = SimpletUtil::allocateGarlicMemory(fbSize);
	uint8_t *fbBaseAddr	= static_cast<uint8_t*>(fbCpuBaseAddr);
	memset(fbCpuBaseAddr, 0x0, fbSize.m_size);

	fbTarget.setAddresses(fbBaseAddr, nullptr, nullptr);


	//
	// Load all shader binaries to video memory
	//
	Gnmx::LsShader *localShader = SimpletUtil::LoadLsShaderFromMemory(s_lsshader_vl);
	Gnmx::HsShader *hullShader = SimpletUtil::LoadHsShaderFromMemory(s_ccpatch_h);
	Gnmx::VsShader *domainShader = SimpletUtil::LoadVsShaderFromMemory(s_ccpatch_dv);
	Gnmx::PsShader *pixelShader = SimpletUtil::LoadPsShaderFromMemory(s_shader_p);

	// Allocate ALU constants
	void *tessCnstPtr = SimpletUtil::allocateGarlicMemory(256, Gnm::kAlignmentOfBufferInBytes);

	//
	// Create a synchronization point:
	//
	volatile uint64_t *label = (uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));


	//
	// Setup the Command and constant buffers:
	//
	Gnmx::GnmxDrawCommandBuffer dcb;
	Gnmx::GnmxConstantCommandBuffer ccb;
	dcb.init(SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes, NULL, NULL);
	ccb.init(SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes, NULL, NULL);

	dcb.setRenderTarget(0, &fbTarget);
	dcb.setRenderTargetMask(0xF);
	Gnmx::setupScreenViewport(&dcb, 0, 0, targetWidth, targetHeight, 0.5f, 0.5f);

	// Initialize the Constant Update Engine:
	const uint32_t	kNumRingEntries	   = 64;
	const uint32_t	cueHeapSize		   = Gnmx::ConstantUpdateEngine::computeHeapSize(kNumRingEntries);

	Gnmx::ConstantUpdateEngine cue;
	cue.init(SimpletUtil::allocateGarlicMemory(cueHeapSize, Gnm::kAlignmentOfBufferInBytes),kNumRingEntries);
	cue.bindCommandBuffers(&dcb,&ccb,NULL);



	//
	// Allocate and Initialize tessellation factor buffer
	//
	void *globalResourceTablePtr = SimpletUtil::allocateGarlicMemory(SCE_GNM_SHADER_GLOBAL_TABLE_SIZE, Gnm::kAlignmentOfBufferInBytes);
	cue.setGlobalResourceTableAddr(globalResourceTablePtr);


	Gnm::Buffer tfbdef;
	tfbdef.initAsTessellationFactorBuffer(tfBufferPtr, Gnm::kTfRingSizeInBytes);
	tfbdef.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // it's written to, so it's GPU-coherent
	cue.setGlobalDescriptor(Gnm::kShaderGlobalResourceTessFactorBuffer, &tfbdef);

	// Set hardware render in wire frame
	Gnm::PrimitiveSetup primSetupReg;
	primSetupReg.init();
	primSetupReg.setPolygonMode(Gnm::kPrimitiveSetupPolygonModeLine, Gnm::kPrimitiveSetupPolygonModeLine);
	dcb.setLineWidth(8);
	dcb.setPrimitiveSetup(primSetupReg);


	// The HW stages used for tessellation (when geometry shading is disabled) are
	// LS -> HS -> VS -> PS and the shaders assigned to these stages are:
	// Vertex Shader --> Hull Shader --> Domain Shader --> Pixel Shader
	// During LS and HS outputs are written to LDS.
	// The tessellation factors written by the patch constant function
	// of the Hull Shader will be sent off-chip and read later by the tessellation engine.

	//
	//  HS is executed on a per threadgroup (TG) basis.
	//  It is the user's responsibility to determine how many
	//  patches to put into every TG.
	//  the LDS foot-print of a patch and the subsequent TG will be
	//  patch_size = numInputCP*lsStride + numOutputCP*cpStride + numPatchConst*16
	//  tg_size = numPatches*patch_size		(rounded up to nearest multiple of 64)

	//  numPatchConst is the number of outputs (16 bytes each) in the patch constant function of the Hull Shader
	//  which includes the tessellation factor values (since these can be read by the domain shader).
	//  lsStride is the byte size of the output of a single thread/vertex at the LS stage
	//  numInputCP is the number specified in the Hull Shader in the InputPatch<> declaration.
	//  cpStride is the byte size of the output of a single invocation/thread of the main hull shader
	//  numOutputCP is the number of HS threads per patch which is specified in [OUTPUT_CONTROL_POINTS(hs_threads)]
	//  The patch constant function represents one invocation per patch but the compiler will
	//  if possible spread the workload across the number of HS threads per patch you've specified.

	Gnm::TessellationDataConstantBuffer tessConstants;
	Gnm::TessellationRegisters               tessRegs;

	uint32_t hs_vgpr_limit = 0x27;		// assign a number of general purpose registers to each TG
	uint32_t lds_size      = 0x1000;	// assign an amount of LDS to each TG (in this case 4kB)
	                                    // the chosen size will also include outputs by the LS stage
	uint32_t numPatches = 0, vgt_prims = 0;
	Gnmx::computeVgtPrimitiveAndPatchCounts(&vgt_prims, &numPatches, hs_vgpr_limit, lds_size, localShader, hullShader);

	uint8_t numPatchesMinus1 = (uint8_t)(vgt_prims-1);
	dcb.setVgtControl(numPatchesMinus1);	// takes numPrimitives-1 as first arg

	// The compiler expects specific constants to be made available to the hull and domain shader.
	// in order to navigate the LDS layout/structure of the LS and HS stage outputs.
	// The LDS layout/structure used by the various stages is allocated in
	// one big block per TG before any of the stages are issued. This LDS structure
	// consists of the following 3 sections.
	// Section 0: numPatches * numInputCP*lsStride
	// Section 1: numPatches * numOutputCP*cpStride
	// Section 2: numPatches * numPatchConst*16
	tessConstants.init(&hullShader->m_hullStateConstants, localShader->m_lsStride, numPatches, 512.f);			// build constants
	memcpy(tessCnstPtr, &tessConstants, sizeof(tessConstants));
	Gnm::Buffer tcbdef;
	tcbdef.initAsConstantBuffer(tessCnstPtr, sizeof(Gnm::TessellationDataConstantBuffer));	// can pregenerate this.
	tcbdef.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

	// Constant buffer slot 19 is currently reserved by the compiler for
	// these compiler specific constants.
	cue.setConstantBuffers(Gnm::kShaderStageHs, 19,1, &tcbdef);
	cue.setConstantBuffers(Gnm::kShaderStageVs, 19,1, &tcbdef);		// domain shader running at the VS stage

	// Set HW specific registers related to tessellation
	tessRegs.init(&hullShader->m_hullStateConstants, numPatches);
	localShader->m_lsStageRegisters.updateLdsSize(&hullShader->m_hullStateConstants, localShader->m_lsStride, numPatches);

	// Set which stages are enabled
	dcb.setActiveShaderStages(Gnm::kActiveShaderStagesLsHsVsPs);
	cue.setActiveShaderStages(Gnm::kActiveShaderStagesLsHsVsPs);

	// Set Vertex Shader: LS stage is a vertex shader that writes to the LDS.
	void* fetchShaderAddr = 0;					// in this sample we're not using a vertex buffer
	cue.setLsShader(localShader, 0, fetchShaderAddr);		// so we do not have a fetch shader.

	// Set Hull Shader (HS)
	cue.setHsShader(hullShader, &tessRegs);

	// Set Domain Shader: VS stage is a vertex or domain shader writing to the parameter cache.
	// If geometry shading is enabled then the domain shader must use the ES stage which goes off-chip to the ES-GS ring-buffer
	cue.setVsShader(domainShader, 0, (void*)0);

	// Set Pixel Shader
	cue.setPsShader(pixelShader);

	// Initiate the actual rendering
	cue.preDraw();
	dcb.setPrimitiveType(Gnm::kPrimitiveTypePatch);
	dcb.drawIndexAuto(NUM_INPUT_CTRL_POINTS_PER_PATCH*NUMSEGS_X*NUMSEGS_Y);
	cue.postDraw();

	// Set hardware state back to its default state
	dcb.setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
	cue.setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);

	// The GPU contains two VGTs and for good results we wish to toggle between the two
	// for every (approximately) 256 vertices. Since vertex caching is enabled when rendering
	// non tessellated geometry we toggle at every 256 primitives since we expect to
	// have, roughly, the same number of primitives as there are unique vertices going through the VGT.
	dcb.setVgtControl(255); 	// takes numPrimitives-1 as first arg


	//
	// Add a synchronization point:
	//
	dcb.writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, (void *)label, Gnm::kEventWriteSource64BitsImmediate, 0x1, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);

	SimpletUtil::registerRenderTargetForDisplay(&fbTarget);

	//
	// Submit requires a start address and  size of command and constant buffers.
	//
	void *dcbAddrGPU = dcb.m_beginptr;
	uint32_t		dcbSize	   = static_cast<uint32_t>(dcb.m_cmdptr - dcb.m_beginptr)*4;
	void *ccbAddrGPU = ccb.m_beginptr;
	uint32_t		ccbSize	   = static_cast<uint32_t>(ccb.m_cmdptr - ccb.m_beginptr)*4;

	// Loop over the buffer
	bool done = false;
	while (!done)
	{
		*label =2;

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
			fbTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture will never be bound as an RWTexture, so it's safe to declare as read-only.
 			SimpletUtil::SaveTextureToTGA(&fbTexture, "screenshot.tga");
			screenshot = 0;
 		}

		// Display the render target on the screen
		SimpletUtil::requestFlipAndWait();

		// Check to see if user has requested to quit the loop
		done = SimpletUtil::handleUserEvents();
	}

	sce::Gnmx::Toolkit::deallocateTessellationFactorRingBuffer();
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
