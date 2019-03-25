/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx.h>
#include <texture_tool.h>
#include <texture_tool/mipped_image.h>
#include "../framework/sample_framework.h"
#include "../framework/gnf_loader.h"
#include <texture_tool/filter.h>
using namespace sce;


#if !SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS

// The purpose of this sample is to show you how to compress a texture with or without mipmaps to a BC format at runtime.
// Once the texture is created, it renders a quad mapped with that texture which covers the entire render target.
// You can modify this sample and use that as a tool to test your textures with the following defines:

#define COMPRESS_AT_RUNTIME // When defined, compression code at run time will be enabled which creates a texture at runtime, otherwise texture is directly loaded from gnf file defined by TEXTURE_FILENAME.
#define USE_COMPUTE_COMPRESSION // When defined, compute compression code at run time will be enabled , otherwise CPU compression code is used which is slower but uses the same code as orbis-image2gnf uses for texture compression.

#define TEXTURE_FILENAME "/fxaa-sample/data/Tx-Italy_sky.gnf"
#define IMAGE_FILENAME "/assets/Tx-Italy_sky.tga"

// Examples:
// To check your gnf which is built offline, comment out COMPRESS_AT_RUNTIME and define TEXTURE_FILENAME by gnf filename in question to view that.
// To find compression error between your source image and compressed texture using CPU, comment out USE_COMPUTE_COMPRESSION and define IMAGE_FILENAME with your source image in question.
//
// NOTE: Additional modification may be needed depending on your test.

#define STDOUT_COMPRESSION_TIME_AND_ERROR // When defined, code to compute and output compression time and error to standard output is enabled. This will not have an effect when COMPRESS_AT_RUNTIME is not defined.

//#define USE_DISPATCH_COMMAND_BUFFER // When defined, a Gnm::DispatchCommandBuffer is created and commands for compute compression are written to that, otherwise DrawCommandBuffer member of gfxc is used to do the same task.
//#define NO_MIPS // when defined work on a single 2D texture otherwise build and compress on mipmapped texture

sce::TextureTool::MippedImage *decompressImageForComapre(const Gnm::Texture texture, uint32_t srgbModes)
{
    Gnm::Texture bcTexture = texture;
    if (srgbModes & 0x04444) // if reader did not convert srgb to linear, we match that in decompression to comapre correctly
        bcTexture.setChannelType(sce::Gnm::kTextureChannelTypeUNorm);
    return (static_cast<sce::TextureTool::MippedImage*> (sce::TextureTool::instantiateImageFromTexture(bcTexture)));
}

int main(int argc, const char *argv[])
{
	Framework::GnmSampleFramework framework;
	framework.processCommandLine(argc, argv);

    framework.m_config.m_cameraControls = false;
    framework.m_config.m_depthBufferIsMeaningful = false;
    framework.m_config.m_clearColorIsMeaningful = false;

	framework.initialize("texture-tool-target",
		"This sample code demonstrates how to compress a 2D texture and its mips to a BC format at runtime using computeCompress API that resides in TextureTool library.",
		"The main point of this sample is to show how you can compress any 2D texture in GPU using textureTool::computeCompress. However, texture tool library is also used "
		"to build an uncompressed 2D texture and its mips, which is not recommended practice for run-time. Once uncompressed data is prepared computeCompress is initialized "
		"for one of supported BC formats (BC1-BC5) and then the input texture is compressed to that format using PS4 GPU. The result (compressed texture and its mips) are "
		"rendered on the screen at the end.");

	Gnmx::GnmxGfxContext *commandBuffers = new Gnmx::GnmxGfxContext[framework.m_config.m_buffers]; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.

	enum {kCommandBufferSizeInBytes = 1 * 1024 * 1024};

	for(uint32_t bufferIndex = 0; bufferIndex < framework.m_config.m_buffers; ++bufferIndex)
	{
		Gnmx::GnmxGfxContext *gfxc = &commandBuffers[bufferIndex];

#if  SCE_GNMX_ENABLE_GFX_LCUE
		const uint32_t kNumBatches = 100; // Estimated number of batches to support within the resource buffer
		const uint32_t resourceBufferSize = Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(Gnmx::LightweightConstantUpdateEngine::kResourceBufferGraphics, kNumBatches);
		gfxc->init
			(		
			framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes), kCommandBufferSizeInBytes,		// Draw command buffer
			framework.m_garlicAllocator.allocate(resourceBufferSize, Gnm::kAlignmentOfBufferInBytes), resourceBufferSize,					// Resource buffer
			NULL																															// Global resource table 
			);
#else
		const uint32_t kNumRingEntries = 64;
		const uint32_t cueHeapSize = Gnmx::ConstantUpdateEngine::computeHeapSize(kNumRingEntries);
		gfxc->init
			(
			framework.m_garlicAllocator.allocate(cueHeapSize, Gnm::kAlignmentOfBufferInBytes), kNumRingEntries,		// Constant Update Engine
			framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes), kCommandBufferSizeInBytes,		// Draw command buffer
			framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes), kCommandBufferSizeInBytes		// Constant command buffer
			);
#endif
	}

	//
	// Allocate some GPU buffers: shaders
	//
	Gnmx::VsShader *vertexShader = Framework::LoadVsShader("/app0/texture-tool-target-sample/shader_vv.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);

	Gnmx::PsShader *pixelShader = Framework::LoadPsShader("/app0/texture-tool-target-sample/shader_p.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache pixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&pixelShader_offsetsTable, Gnm::kShaderStagePs, pixelShader);

    uint32_t fssz = Gnmx::computeVsFetchShaderSize(vertexShader);
	void *fetchShaderPtr = framework.m_garlicAllocator.allocate(fssz, Gnm::kAlignmentOfShaderInBytes );
	uint32_t shaderModifier;
	Gnmx::generateVsFetchShader(fetchShaderPtr, &shaderModifier, vertexShader);

	//
	// Build the texture in runtime
	//
    Gnm::Texture bcTexture; // block compressed Texture to be created

    using namespace sce::TextureTool;

    // Note: TextureTool GpuMappedAllocator is initialized with systemAllocator by default which its memory won't be visible by GPU.
    // Therefore we need change that to a true GpuMappedAllocator so that uncompressed pixels data will be accessible by compute program for texture compression.
    setToolsGpuMappedAllocator(*(sce::TextureTool::Raw::IAllocator*)&framework.m_garlicAllocator);

    Gnm::Sampler sampler;
	sampler.init();
	sampler.setXyFilterMode(Gnm::kFilterModePoint, Gnm::kFilterModePoint);
    sampler.setWrapMode( Gnm::kWrapModeClampBorder, Gnm::kWrapModeClampBorder, Gnm::kWrapModeClampBorder);
	sampler.setMipFilterMode(Gnm::kMipFilterModeLinear);

    sce::Gnm::DataFormat format = sce::Gnm::kDataFormatBc1UnormSrgb;
#ifdef COMPRESS_AT_RUNTIME
    uint32_t srgbModes = (format.m_bits.m_channelType == sce::Gnm::kTextureChannelTypeSrgb) ? 0x04444 : 0; // don't convert pixel values from srgb space to linear when loading the image (tga default assumes RGB to be in srgb and converts them to linear)
    GnmTextureGen *loadedImage = loadImage("/app0" IMAGE_FILENAME, srgbModes);
#ifdef NO_MIPS
    Image *srcImage=NULL;
    if(loadedImage->getType()==k2DImage)
            srcImage = (static_cast<Image*> (loadedImage));
    else
    if(loadedImage->getType()==k2DImageMipped)
    {
            MippedImage *mippedImage=(static_cast<MippedImage*> (loadedImage));
	        Filters::BoxFilter filter;
            srcImage = mippedImage->getImage(0);
    }
    SCE_GNM_ASSERT( srcImage!=NULL ); // make sure we have a mipped image at this point
    uint32_t baseWidth = srcImage->getWidth();
    uint32_t baseHeight = srcImage->getHeight();
#else
    MippedImage *srcImage=NULL;
    if(loadedImage->getType()==k2DImageMipped)
    {
            srcImage = (static_cast<MippedImage*> (loadedImage));
    }else
    if(loadedImage->getType()==k2DImage)
    {
	        Filters::BoxFilter filter;
            srcImage = new MippedImage(*(static_cast<Image*>(loadedImage)),filter, 4); // To show how to build a mipmap with n mips (in this case n is 4), otherwise remove the last argument to build all the mips
    }
    SCE_GNM_ASSERT( srcImage!=NULL ); // make sure we have a mipped image at this point
    uint32_t baseWidth = srcImage->getImage(0)->getWidth();
    uint32_t baseHeight = srcImage->getImage(0)->getHeight();
#endif

    Gnm::TileMode  tileMode = Gnm::kTileModeThin_1dThin;


    // create T# of the compressed texture
    Gnm::SizeAlign pixelsSa = srcImage->initTexture(bcTexture, format, tileMode);
    void *pixelsPtr = framework.m_garlicAllocator.allocate(pixelsSa);
    SCE_GNM_ASSERT( pixelsPtr!=0 );
    memset(pixelsPtr, 0, pixelsSa.m_size);
    bcTexture.setBaseAddress( pixelsPtr );

    // prepare the input texture T# (uncompressed data format of the source image)
    Gnm::Texture srcTexture;
    Gnm::DataFormat srcFormats[4] = { Gnm::kDataFormatR32Float, Gnm::kDataFormatR32G32Float, Gnm::kDataFormatB8G8R8X8Unorm, Gnm::kDataFormatR32G32B32A32Float};
    uint32_t numChannels = srcImage->getNumChannels();
    SCE_GNM_ASSERT( numChannels<5 );
    sce::Gnm::DataFormat srcFormat = srcFormats[numChannels-1];
    Gnm::SizeAlign srcPixelsSa = srcImage->initTexture(srcTexture, srcFormat, tileMode);
    void *srcPixelsPtr = framework.m_garlicAllocator.allocate(srcPixelsSa);
    size_t textureSize = srcImage->fillSurface( (uint8_t *)srcPixelsPtr, srcPixelsSa.m_size, tileMode, srcFormat);
    SCE_GNM_ASSERT( srcPixelsSa.m_size == textureSize );

    srcTexture.setBaseAddress(srcPixelsPtr);

#ifdef STDOUT_COMPRESSION_TIME_AND_ERROR // check the compression time and error
    clock_t startTime = clock(); // start the clock here to include initialization and preperation of dcb
#endif
    // allocate memory for BlockEncoder binary and constants if srcImage pixels can be mapped to a rwTexture with alignment restrictions required by PS4 ( to directly create a t# for it )
    void *vmBuffer = NULL;
#ifdef USE_COMPUTE_COMPRESSION
    uint32_t imagePitch =  baseWidth * sizeof(float) * numChannels; // uncompressed data are floats * number of channels which are linearly aligned
    uint32_t pitchAlignment = (numChannels==1)? 16 : 8;
    vmBuffer = ((baseHeight&1) || (imagePitch & pitchAlignment))? NULL : framework.m_garlicAllocator.allocate(kBlockEncoderBufferSize, 256);
#else
    SCE_GNM_UNUSED(baseWidth);
    SCE_GNM_UNUSED(baseHeight);
#endif
    BlockEncoder bcEncoder;
    if( bcEncoder.initializeComputeCompress(vmBuffer, format)==0 )
    {
        //
        // Create a synchronization point:
        //
        volatile uint64_t *label     = (uint64_t *)framework.m_onionAllocator.allocate(sizeof(uint64_t), 8);
        *label = 2;
#ifdef SAWP_GREEN_WITH_ALPHA
		if(format.getSurfaceFormat() == Gnm::kSurfaceFormatBc3)
		{
			{
				Gnm::DataFormat dataFormat;
				dataFormat = bcTexture.getDataFormat();
				dataFormat.m_bits.m_channelX = Gnm::kTextureChannelX;
				dataFormat.m_bits.m_channelY = Gnm::kTextureChannelW;
				dataFormat.m_bits.m_channelZ = Gnm::kTextureChannelZ;
				dataFormat.m_bits.m_channelW = Gnm::kTextureChannelY;
				bcTexture.setDataFormat(dataFormat);
			}
			{
				Gnm::DataFormat dataFormat = srcTexture.getDataFormat();
				dataFormat.m_bits.m_channelX = Gnm::kTextureChannelX;
				dataFormat.m_bits.m_channelY = Gnm::kTextureChannelW;
				dataFormat.m_bits.m_channelZ = Gnm::kTextureChannelZ;
				dataFormat.m_bits.m_channelW = Gnm::kTextureChannelY;
				srcTexture.setDataFormat(dataFormat);
			}
		}
#endif

#ifdef USE_DISPATCH_COMMAND_BUFFER
        const uint32_t ringSize = 1024*4;
        uint32_t *ring      = (uint32_t *)framework.m_onionAllocator.allocate(ringSize*sizeof(uint32_t), Gnm::kAlignmentOfComputeQueueRingBufferInBytes);
        uint32_t *readPtr   = (uint32_t *)framework.m_onionAllocator.allocate(sizeof(uint32_t), 4);

        Gnm::DispatchCommandBuffer dcb;
        uint32_t uqdId;
		int ret = sce::Gnm::mapComputeQueue(&uqdId, 0, 0, ring, ringSize, readPtr );
		SCE_GNM_ASSERT(ret == 0);
		SCE_GNM_UNUSED(ret);
        dcb.init(ring, ringSize*sizeof(uint32_t), NULL, NULL);

		int result = bcEncoder.computeCompress(&bcTexture, &srcTexture, &dcb);
		SCE_GNM_ASSERT(result == 0);
		SCE_GNM_UNUSED(result); // avoid warning for release configuration
        const uint64_t zero = 0;

        dcb.writeReleaseMemEvent(Gnm::kReleaseMemEventCsDone,
							     Gnm::kEventWriteDestMemory, (void*)label,
							     Gnm::kEventWriteSource64BitsImmediate, 0x1,
							     Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
        dcb.waitOnAddress((void*)label, 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
        dcb.writeDataInline((void*)label, &zero, 2, Gnm::kWriteDataConfirmEnable);
        uint32_t cbSize = static_cast<uint32_t>(dcb.m_cmdptr - dcb.m_beginptr)*4;

        sce::Gnm::dingDong(uqdId, cbSize/4);
#else
	    Gnmx::GnmxGfxContext *gfxc = &commandBuffers[framework.m_backBufferIndex];
        Gnm::DrawCommandBuffer &dcb= gfxc->m_dcb;

        int result = bcEncoder.computeCompress(&bcTexture, &srcTexture, &dcb);
        SCE_GNM_ASSERT(result == 0);
		SCE_GNM_UNUSED(result); // avoid warning for release configuration
#endif
        Gnmx::Toolkit::submitAndStall(*gfxc);

#ifdef USE_DISPATCH_COMMAND_BUFFER
        sce::Gnm::unmapComputeQueue(uqdId);
#endif
    }else
    {
        sce::Gnm::DataFormat encodingFormat = format;
        if (encodingFormat.m_bits.m_channelType == sce::Gnm::kTextureChannelTypeSrgb)
            encodingFormat.m_bits.m_channelType = sce::Gnm::kTextureChannelTypeUNorm;// don't convert pixel values to srgb space to when compressing the image. (because at load we load them as unorm)
	    srcImage->fillSurface( (uint8_t *)pixelsPtr, pixelsSa.m_size, tileMode, format);
    }

#ifdef STDOUT_COMPRESSION_TIME_AND_ERROR // check the compression time and error
	clock_t  stopTime = clock();
	float elaspsedTime = (float)(stopTime-startTime)/CLOCKS_PER_SEC;

    MippedImage *decompressedImage = decompressImageForComapre(bcTexture, srgbModes);
    // compute error per mip
#ifndef NO_MIPS
    for(uint32_t mip=0; mip<srcImage->getNumberOfMips(); ++mip)
    {
        const Image *mipImage = srcImage->getImage(mip);
        float mipError=0;
        float alphaMultipliedError = mipImage->compare(decompressedImage->getImage(mip), &mipError, NULL);
        SCE_GNM_UNUSED(alphaMultipliedError);
        uint32_t numCompares = mipImage->getNumPixelsRequired(false)*format.getNumComponents();
        float errorPerPixel = sqrt(mipError/numCompares)*255;
        float peakSignal2Noise = 20 *log10(255/errorPerPixel);
	    printf("mip %i) Total error %f. Average per pixel error = %f.  Signal to noise (db) = %f\n", mip, mipError, errorPerPixel,  peakSignal2Noise);
    }
#endif
    float totalError=0;
    float alphaMultipliedError = srcImage->compare(decompressedImage, &totalError, NULL);
    SCE_GNM_UNUSED(alphaMultipliedError);
    uint32_t numPixels = srcImage->getNumPixelsRequired(false);
    uint32_t numCompares = numPixels*format.getNumComponents();
	printf("\n**************************************************\n");
	printf("Total compression time: %f in seconds.\n", elaspsedTime);
	printf("Total chi-square error = %f.\n", totalError);
    printf("Per pixel Error (rms) = %f\n", sqrt(totalError/ numCompares) * 255);
    if (format.getSurfaceFormat() == Gnm::kSurfaceFormatBc6) // depending on output format
    {
        float hdrError = srcImage->CompareRelative(bcTexture, NULL); // compute relative error between image instantiated from the texture and the original source image.
        printf("Ralative rms (Hdr)     = %f\n", sqrt(hdrError / numCompares) * 255);
    }
    printf("**************************************************\n\n");
#endif

#else
    // This is an easy way of checkig a texture just load the gnf and we are done.
    Framework::GnfError loadError = Framework::loadTextureFromGnf(&bcTexture, "/app0" TEXTURE_FILENAME, 0, &framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );
	SCE_GNM_UNUSED(loadError);
#endif // COMPRESS_AT_RUNTIME

	// Main Loop:
	//
	while (!framework.m_shutdownRequested)
	{
		Gnmx::GnmxGfxContext *gfxc = &commandBuffers[framework.m_backBufferIndex]; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.

		gfxc->reset();
		framework.BeginFrame( *gfxc );

        // Clear
        Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &framework.m_backBuffer->m_renderTarget, framework.getClearColor());
        Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &framework.m_backBuffer->m_depthTarget, 1.0f);
		gfxc->setRenderTargetMask(0xF);
		gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
		gfxc->setRenderTarget(0, &framework.m_backBuffer->m_renderTarget);
		gfxc->setDepthRenderTarget(&framework.m_backBuffer->m_depthTarget);

		gfxc->setupScreenViewport(0, 0, framework.m_backBuffer->m_renderTarget.getWidth(), framework.m_backBuffer->m_renderTarget.getHeight(), 0.5f, 0.5f);

		gfxc->setVsShader(vertexShader, shaderModifier, fetchShaderPtr, &vertexShader_offsetsTable);
 		gfxc->setPsShader(pixelShader, &pixelShader_offsetsTable);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &bcTexture);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);

        // draw screen quad
		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
		gfxc->drawIndexAuto( 3);
		framework.EndFrame( *gfxc );
    }
	Gnmx::GnmxGfxContext *gfxc = &commandBuffers[framework.m_backBufferIndex];
	framework.terminate(*gfxc);
    return 0;
}

#else // SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS

int main()
{
	// This samples doesn't currently support SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
	return 0;
}

#endif // SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
