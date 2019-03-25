/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

// Simplet #7
// This simplet shows how to setup es, gs shaders to create geometry

#include "../toolkit/simplet-common.h"
#include "std_cbuffer.h"
using namespace sce;

bool screenshot = false;

// {
//   "title" : "simplet-geometry-shader",
//   "overview" : "This sample demonstrates simple geometry shading.",
//   "explanation" : "This sample sets up required ring buffers and performs simple geometry shading with them."
// }

static const unsigned s_shader_g[] = {
	#include "shader_g.h"
};
static const unsigned s_shader_p[] = {
	#include "shader_p.h"
};
static const unsigned s_shader_ve[] = {
	#include "shader_ve.h"
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
	Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb;
	Gnm::RenderTarget fbTarget;
	Gnm::TileMode tileMode;
	GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeColorTargetDisplayable, format, 1);
	Gnm::SizeAlign fbSize = Gnmx::Toolkit::init(&fbTarget, targetWidth, targetHeight, 1, format, tileMode, Gnm::kNumSamples1, Gnm::kNumFragments1, NULL, NULL);

	void			*fbCpuBaseAddr = SimpletUtil::allocateGarlicMemory(fbSize);
	uint8_t *fbBaseAddr = static_cast<uint8_t*>(fbCpuBaseAddr);
	memset(fbCpuBaseAddr, 0x0, fbSize.m_size);

	fbTarget.setAddresses(fbBaseAddr, 0, 0);


	//
	// Load all shader binaries to video memory
	//
	Gnmx::EsShader  *exportShader = SimpletUtil::LoadEsShaderFromMemory(s_shader_ve);
	Gnmx::GsShader  *geometryShader = SimpletUtil::LoadGsShaderFromMemory(s_shader_g);
	Gnmx::PsShader  *pixelShader = SimpletUtil::LoadPsShaderFromMemory(s_shader_p);

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
			  SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes		 // Constant command buffer
			  );

#if !SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	void *globalResourceTablePtr = SimpletUtil::allocateGarlicMemory(SCE_GNM_SHADER_GLOBAL_TABLE_SIZE, Gnm::kAlignmentOfBufferInBytes);
	gfxc.setGlobalResourceTableAddr(globalResourceTablePtr);
#endif //!SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE

	gfxc.setRenderTarget(0, &fbTarget);
	gfxc.setRenderTargetMask(0xF);
	gfxc.setupScreenViewport(0, 0, targetWidth, targetHeight, 0.5f, 0.5f);

	// Set hardware render in wire frame
	Gnm::PrimitiveSetup primSetupReg;
	primSetupReg.init();
	primSetupReg.setPolygonMode(Gnm::kPrimitiveSetupPolygonModeLine, Gnm::kPrimitiveSetupPolygonModeLine);
	gfxc.setLineWidth(8);
	gfxc.setPrimitiveSetup(primSetupReg);


	// The HW stages used for geometry shading (when tessellation is disabled) are
	// ES -> GS -> VS -> PS and the shaders assigned to these stages are:
	// Vertex Shader --> Geometry Shader --> (Copy VS shader) --> Pixel Shader
	// The shaders assigned to the ES and GS stages both write their data to off-chip memory.

	// All of the work described by a single geometry shader invocation is performed by just one thread.

	// Do we know anything about foot-print yet?

	// set rings
	gfxc.setEsGsRingBuffer(esgsRing, esgsRingSize, exportShader->m_memExportVertexSizeInDWord);
	gfxc.setGsVsRingBuffers(gsvsRing, gsvsRingSize,
							geometryShader->m_memExportVertexSizeInDWord,
							geometryShader->m_maxOutputVertexCount);


	// Set which stages are enabled
	gfxc.setActiveShaderStages(Gnm::kActiveShaderStagesEsGsVsPs);

	// Set Vertex Shader: ES stage is a vertex shader that writes to the ES-GS ring-buffer
//	uint64_t fetchShaderAddr = 0;					// in this sample we're not using a vertex buffer
	gfxc.setEsShader(exportShader, 0, (void*)0);			// so we do not have a fetch shader.

	// Set Geometry Shader: GS stage is a geometry shader that writes to the GS-VS ring-buffer
	gfxc.setGsVsShaders(geometryShader);		// sets up copy VS shader too

	// Set Pixel Shader
	gfxc.setPsShader(pixelShader);


	// Initiate the actual rendering
	gfxc.setPrimitiveType(Gnm::kPrimitiveTypePointList);
	gfxc.drawIndexAuto(NUMSEGS_X*NUMSEGS_Y);

	// Set hardware state back to its default state
	gfxc.setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);

	// In addition of turning off the GS shader stage off; the GS Mode also needs to be turned off.
	gfxc.setGsModeOff();

	//
	// Add a synchronization point:
	//
	gfxc.writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void *)label, 0x1, Gnm::kCacheActionNone);

	SimpletUtil::registerRenderTargetForDisplay(&fbTarget);

	// Loop over the buffer
	bool done = false;
	while (!done)
	{
		*label =2;

		// Submit the command buffers:
		// Note: we are passing the ccb in the last two arguments; when no ccb is used, 0 should be passed in
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
