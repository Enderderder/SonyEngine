/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

// Simplet #7
// This simplet shows how to setup ls, hs , ds shaders to create tessellated geometry from input control points

#include "../toolkit/simplet-common.h"
#include <pm4_dump.h>
#include "std_cbuffer.h"
using namespace sce;

bool screenshot = false;

// {
//   "title" : "simplet-tess-gs",
//   "overview" : "This sample demonstrates the simultaneous use of tessellation and geometry shading.",
//   "explanation" : "This sample allocates and sets up the buffers required for tessellation and geometry shading, then renders a simple scene to demonstrate these features."
// }

static const unsigned s_lsshader_vl[] = {
	#include "LSshader_vl.h"
};
static const unsigned s_patch_de[] = {
	#include "patch_de.h"
};
static const unsigned s_patch_h[] = {
	#include "patch_h.h"
};
static const unsigned s_shader_g[] = {
	#include "shader_g.h"
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
	memset(fbCpuBaseAddr, 0x0, fbSize.m_size);

	fbTarget.setAddresses(fbBaseAddr, nullptr, nullptr);


	//
	// Load all shader binaries to video memory
	//
	Gnmx::LsShader *localShader = SimpletUtil::LoadLsShaderFromMemory(s_lsshader_vl);
	Gnmx::HsShader *hullShader = SimpletUtil::LoadHsShaderFromMemory(s_patch_h);
	Gnmx::EsShader *exportShader = SimpletUtil::LoadEsShaderFromMemory(s_patch_de);
	Gnmx::GsShader *geometryShader = SimpletUtil::LoadGsShaderFromMemory(s_shader_g);
	Gnmx::PsShader *pixelShader = SimpletUtil::LoadPsShaderFromMemory(s_shader_p);


	//
	// Allocate esgs and gsvs rings
	//
	const uint32_t esgsRingSize = Gnm::kGsRingSizeSetup4Mb, gsvsRingSize = Gnm::kGsRingSizeSetup4Mb;
	void *esgsRing = SimpletUtil::allocateGarlicMemory(esgsRingSize, Gnm::kAlignmentOfBufferInBytes);
	void *gsvsRing = SimpletUtil::allocateGarlicMemory(gsvsRingSize, Gnm::kAlignmentOfBufferInBytes);


	//
	// Create a synchronization point:
	//
	volatile uint64_t *label = (uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));


	//
	// Setup the Command and constant buffers:
	//
	Gnmx::GfxContext gfxc;



	const uint32_t kNumRingEntries = 64;
	const uint32_t cueHeapSize = Gnmx::ConstantUpdateEngine::computeHeapSize(kNumRingEntries);
	gfxc.init(SimpletUtil::allocateGarlicMemory(cueHeapSize, Gnm::kAlignmentOfBufferInBytes), kNumRingEntries, // Constant Update Engine
				SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes,	 // Draw command buffer
				SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes	 // Constant command buffer
			  );

	// set tessellation factor buffer
	void *tfBufferPtr = sce::Gnmx::Toolkit::allocateTessellationFactorRingBuffer();

	//
	// Allocate and Initialize tessellation factor buffer
	//
#if !SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	void *globalResourceTablePtr = SimpletUtil::allocateGarlicMemory(SCE_GNM_SHADER_GLOBAL_TABLE_SIZE, Gnm::kAlignmentOfBufferInBytes);
	gfxc.setGlobalResourceTableAddr(globalResourceTablePtr);
#endif //!SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE


	void *tessCnstPtr = SimpletUtil::allocateGarlicMemory(256, Gnm::kAlignmentOfBufferInBytes);


	Gnm::Buffer tfbdef;
	tfbdef.initAsTessellationFactorBuffer(tfBufferPtr, Gnm::kTfRingSizeInBytes);
	tfbdef.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we write to it, so it's GPU-coherent
	gfxc.setTessellationFactorBuffer(tfBufferPtr);


	gfxc.setRenderTarget(0, &fbTarget);
	gfxc.setRenderTargetMask(0xF);
	gfxc.setupScreenViewport(0, 0, targetWidth, targetHeight, 0.5f, 0.5f);


	// Set hardware render in wire frame
	Gnm::PrimitiveSetup primSetupReg;
	primSetupReg.init();
	primSetupReg.setPolygonMode(Gnm::kPrimitiveSetupPolygonModeLine, Gnm::kPrimitiveSetupPolygonModeLine);
	gfxc.setLineWidth(8);
	gfxc.setPrimitiveSetup(primSetupReg);


	// The HW stages used for tessellation (when geometry shading is enabled) are
	// LS -> HS -> ES -> GS (-> VS) -> PS and the shaders assigned to these stages are:
	// Vertex Shader --> Hull Shader --> Domain Shader --> Geometry Shader --> Pixel Shader
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
	//  if possible spread the workload across the number of HS threads per patch you specify.

	Gnm::TessellationDataConstantBuffer tessConstants;

	uint32_t hs_vgpr_limit = 0x27;		// assign a number of general purpose registers to each TG
	uint32_t lds_size      = 0x1000;	// assign an amount of LDS to each TG (in this case 4kB)
	// the chosen size will also include outputs by the LS stage
	uint32_t numPatches = 0, vgt_prims = 0;
	Gnmx::computeVgtPrimitiveAndPatchCounts(&vgt_prims, &numPatches, hs_vgpr_limit, lds_size, localShader, hullShader);

	uint8_t numPatchesMinus1 = (uint8_t)(vgt_prims-1);
	gfxc.setVgtControl(numPatchesMinus1);	// takes numPrimitives-1 as first arg

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

	// in this sample we're not using a vertex buffer so we do not have a fetch shader.
	void* fetchShaderAddr = 0;

	// Set Vertex Shader: LS stage is a vertex shader that writes to the LDS.
	// Set Hull Shader (HS)
	gfxc.setLsHsShaders(localShader, 0, fetchShaderAddr,
		hullShader, numPatches);

	// Set Domain Shader: When geometry shading is enabled and combined with tessellation the domain shader runs at the ES stage.
	// This means the domain shader will write to the ES-GS ring buffer instead of directly to the parameter cache.
	gfxc.setEsShader(exportShader, 0, (void*)0);

	// Set Geometry Shader: GS stage is a geometry shader that writes to the GS-VS ring-buffer
	gfxc.setGsVsShaders(geometryShader);		// sets up copy VS shader too

	// Set Pixel Shader
	gfxc.setPsShader(pixelShader);

	// Constant buffer slot 19 is currently reserved by the compiler for
	// these compiler specific constants.
	gfxc.setConstantBuffers(Gnm::kShaderStageHs, 19,1, &tcbdef);
	gfxc.setConstantBuffers(Gnm::kShaderStageEs, 19,1, &tcbdef);		// domain shader running at the ES stage

	// set GS rings
	gfxc.setEsGsRingBuffer(esgsRing, esgsRingSize, exportShader->m_memExportVertexSizeInDWord);
	gfxc.setGsVsRingBuffers(gsvsRing, gsvsRingSize,
							geometryShader->m_memExportVertexSizeInDWord,
							geometryShader->m_maxOutputVertexCount);

	// Set which stages are enabled
	gfxc.setActiveShaderStages(Gnm::kActiveShaderStagesLsHsEsGsVsPs);

	// Initiate the actual rendering
	gfxc.setPrimitiveType(Gnm::kPrimitiveTypePatch);
	gfxc.drawIndexAuto(NUM_INPUT_CTRL_POINTS_PER_PATCH*NUMSEGS_X*NUMSEGS_Y);

	// Set hardware state back to its default state
	gfxc.setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);

	// In addition to disabling the GS shader stage; the GS Mode must also be set to off.
	gfxc.setGsModeOff();

	// The GPU contains two VGTs and for good results we wish to toggle between the two
	// for every (approximately) 256 vertices. Since vertex caching is enabled when rendering
	// non tessellated geometry we toggle at every 256 primitives since we expect to
	// have, roughly, the same number of primitives as there are unique vertices going through the VGT.
	gfxc.setVgtControl(255); 	// takes numPrimitives-1 as first arg



	//
	// Add a synchronization point:
	//
	gfxc.writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void *)label, 0x1, Gnm::kCacheActionNone);

	SimpletUtil::registerRenderTargetForDisplay(&fbTarget);

	//
	// Submit

	// Loop over the buffer
	bool done = false;
	while (!done)
	{
		*label =2;

		// Submit the command buffers:
		// Note: we are passing the ccb in the last two arguments; when no ccb is used, 0 should be passed in
		int32_t state = gfxc.submit();
		SCE_GNM_ASSERT(state == sce::Gnm::kSubmissionSuccess);

		//Gnm::submitCommandBuffer(Gnm::kRingTypeGraphics, dcbAddrGPU, dcbSize, ccbAddrGPU, ccbSize);

		// Wait until it's done (waiting for the GPU to write "1" in "label")
		uint32_t wait = 0;
		while (*label != 1)
			++wait;

		if(screenshot)
 		{
 			Gnm::Texture fbTexture;
 			fbTexture.initFromRenderTarget(&fbTarget, false);
			fbTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture will never be bound as an RWTexture, so it's safe to declare read-only.
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
