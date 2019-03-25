/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 02.508.051
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/noise_prep.h"
#include "../framework/frame.h"
#include "std_cbuffer.h"
#include "histogram_structs.h"
using namespace sce;

// Enable histogram verification on the CPU. After each frame, histogram is copied to CPU visible memory
// and CPU verifies that the sum of pixels in all intensity buckets matches total number of pixels of
// input texture.
#define SANITY_CHECK_HISTOGRAM_DATA (1)

namespace
{
	enum { kRenderToTextureTargetWidth = 4096 };
	enum { kRenderToTextureTargetHeight = 4096 };
	enum { kRenderToTextureTargetNumPixels = kRenderToTextureTargetWidth * kRenderToTextureTargetHeight };

	uint32_t iMaterialIndex = 0;
	uint32_t iMeshIndex = 0;
	uint32_t iHistogramShader = 0;
    uint32_t iFlipModeIndex = 1;
	Framework::MenuItemText materialIndexText[5] = {
		{"Clay", "Low frequency 3D noise"},
		{"Wood", "Noisy banded gradient"},
		{"Marble", "Noisy luma ramp gradient"},
		{"Liquid", "Noisy luma ramp gradient with liquid"},
		{"Triplanar", "A height map for each primary axis"},
	};
	Framework::MenuItemText meshIndexText[2] = {
		{"Iso surface", "Iso surface mesh"},
		{"ICE logo", "ICE logo mesh"},
	};
	Framework::MenuItemText histogramShaderIndexText[6] = {
		{"GDS optimized", "Use global data share with 8x8 texels per thread writes to LDS"},
		{"GDS simple", "Use global data share to compute histogram"},
		{"GDS + LDS", "Use global data share with local data share speedup"},
		{"RW buffer optimized", "Use RW buffer with 8x8 texels per thread writes to LDS"},
		{"RW buffer", "Use RW buffer to compute histogram"},
		{"RW buffer + LDS", "Use RW buffer with local data share speedup"},
	};
    Framework::MenuItemText flipModeIndexText[2] = {
        {"FLIP_MODE_VSYNC", "Flip on real video out vsync"},
        {"FLIP_MODE_HSYNC", "Flip ASAP (but not immediate)"},
    };

	Framework::MenuCommandEnum menuCommandMaterialIndex(materialIndexText, sizeof(materialIndexText)/sizeof(materialIndexText[0]), &iMaterialIndex);
	Framework::MenuCommandEnum menuCommandMeshIndex(meshIndexText, sizeof(meshIndexText) / sizeof(meshIndexText[0]), &iMeshIndex);
	Framework::MenuCommandEnum menuCommandHistogramShaderIndex(histogramShaderIndexText, sizeof(histogramShaderIndexText) / sizeof(histogramShaderIndexText[0]), &iHistogramShader);
    Framework::MenuCommandEnum menuCommandFlipModeIndex(flipModeIndexText, sizeof(flipModeIndexText) / sizeof(flipModeIndexText[0]), &iFlipModeIndex);
	const Framework::MenuItem menuItem[] =
	{
		{{"Material", "Choose among various procedural materials"}, &menuCommandMaterialIndex},
		{{"Geometry", "Choose among various meshes"}, &menuCommandMeshIndex},
		{{"Histogram shader", "Choose among different histogram compute shaders"}, &menuCommandHistogramShaderIndex},
        {{"Flip mode", "Choose display flip mode (vsync, hsync, window, multi)"}, &menuCommandFlipModeIndex},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };

	enum {
		kHistogramShaderOptimizedGds		= 0,
		kHistogramShaderGdsOnly				= 1,
		kHistogramShaderGdsWithLds			= 2,
		kHistogramShaderOptimizedRWBuffer	= 3,
		kHistogramShaderRWBufferOnly		= 4,
		kHistogramShaderRWBufferWithLds		= 5,
	};

    Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

    framework.m_config.m_depthBufferIsMeaningful = false;

	framework.initialize( "Histogram with GDS", 
		"Illustrates a method to generate color histogram into GDS memory.",
        "This sample demonstrates the use of on-chip Global Data Share (GDS) memory to share data "
        "between shader stages. It also shows how to read data from GDS into CPU visible memory. "
        "Lastly, the sample implements equivalent method using RWBuffers for comparison.");

	framework.appendMenuItems(kMenuItems, menuItem);

	class HistogramFrame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Gnm::DepthRenderTarget m_depthTarget;
		Gnm::Texture m_offscreenTexture;
		Gnm::RenderTarget m_offscreenRenderTarget;
		uint8_t m_reserved0[4];
		volatile HistogramColorData *m_histogramData;
		void *m_histogramRwBufferMem;
		volatile uint64_t *m_histogramDataReadyLabel;
		uint32_t m_gdsMemoryOffset;
		uint8_t m_reserved1[4];
	};
	HistogramFrame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(uint32_t buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		HistogramFrame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		char temp[Framework::kResourceNameLength];

		Gnm::DataFormat depthFormat = Gnm::DataFormat::build(Gnm::kZFormat32Float);
		Gnm::TileMode depthTileMode;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&depthTileMode,GpuAddress::kSurfaceTypeDepthOnlyTarget,depthFormat,1);

		// Offscreen depth target per frame
		Gnm::SizeAlign depthTargetSizeAlign = Gnmx::Toolkit::init(&frame->m_depthTarget,kRenderToTextureTargetWidth,kRenderToTextureTargetHeight,depthFormat.getZFormat(),Gnm::kStencilInvalid,depthTileMode,Gnm::kNumFragments1,NULL,NULL);
		void *depth2CpuBaseAddr;
		framework.m_allocators.allocate(&depth2CpuBaseAddr, SCE_KERNEL_WC_GARLIC, depthTargetSizeAlign, Gnm::kResourceTypeDepthRenderTargetBaseAddress, "Buffer %d Offscreen Depth", buffer);
		frame->m_depthTarget.setZReadAddress(depth2CpuBaseAddr);
		frame->m_depthTarget.setZWriteAddress(depth2CpuBaseAddr);

		Gnm::TileMode offScreenTileMode;
		Gnm::DataFormat offscreenFormat = Gnm::kDataFormatR8G8B8A8Unorm;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&offScreenTileMode,GpuAddress::kSurfaceTypeColorTarget,offscreenFormat,1);

		// Offscreen render target per frame
		Gnm::SizeAlign offScreenRTSizeAlign = Gnmx::Toolkit::init(&frame->m_offscreenRenderTarget,kRenderToTextureTargetWidth,kRenderToTextureTargetHeight,1,offscreenFormat,offScreenTileMode,Gnm::kNumSamples1,Gnm::kNumFragments1,NULL,NULL);
		void *color2CpuBaseAddr;
		framework.m_allocators.allocate(&color2CpuBaseAddr, SCE_KERNEL_WC_GARLIC, offScreenRTSizeAlign, Gnm::kResourceTypeRenderTargetBaseAddress, "Buffer %d Offscreen RenderTarget", buffer);
		frame->m_offscreenRenderTarget.setAddresses(color2CpuBaseAddr,0,0);
		frame->m_offscreenTexture.initFromRenderTarget(&frame->m_offscreenRenderTarget,false);
		frame->m_offscreenTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never write to this texture from a shader, so it's OK to mark the texture as read-only.

		sprintf(temp,"Buffer %d Offscreen Texture",buffer);
		Gnm::registerResource(nullptr,framework.m_allocators.m_owner,frame->m_offscreenTexture.getBaseAddress(),offScreenRTSizeAlign.m_size,temp,Gnm::kResourceTypeTextureBaseAddress,0);

		// Gds memory for histogram compute shader
		framework.m_allocators.allocate((void **)&frame->m_histogramData, SCE_KERNEL_WB_ONION, sizeof(HistogramColorData), 4, Gnm::kResourceTypeBufferBaseAddress, "Buffer %d Histogram Color Data", buffer);
		memset((void*)frame->m_histogramData,0,sizeof(HistogramColorData));

		// RW buffer for histogram compute shader
		framework.m_allocators.allocate((void **)&frame->m_histogramRwBufferMem,SCE_KERNEL_WC_GARLIC,sizeof(HistogramColorData), 4,Gnm::kResourceTypeBufferBaseAddress,"Buffer %d Histogram Buffer",buffer);
		memset(frame->m_histogramRwBufferMem,0,sizeof(HistogramColorData));

		// In case of enabled histogram data sanity check, use this label to wait on data
		framework.m_allocators.allocate((void **)&frame->m_histogramDataReadyLabel, SCE_KERNEL_WB_ONION, sizeof(uint64_t), sizeof(uint64_t), Gnm::kResourceTypeLabel, "Buffer %d Histogram Label", buffer);

		// Per-frame gds memory offset
		frame->m_gdsMemoryOffset = buffer * sizeof(HistogramColorData);
	}

	// Set up depth target for render-to-texture target
	const float kLog2NumTexelsOffscreen = log2f((float)kRenderToTextureTargetHeight * kRenderToTextureTargetWidth);

	// Setup quad mesh
	Framework::SimpleMesh quadMesh;
	BuildQuadMesh(&framework.m_allocators, "Quad", &quadMesh, 1.0f);

	Framework::InitNoise(&framework.m_allocators);

	Gnm::Texture marbleColorTexture, lavaTexture;
    Framework::GnfError loadError = Framework::kGnfErrorNone;
	loadError = Framework::loadTextureFromGnf(&marbleColorTexture, "/app0/assets/pal_marble.gnf", 0, &framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );
	loadError = Framework::loadTextureFromGnf(&lavaTexture, "/app0/assets/pal_lava.gnf", 0, &framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );

	marbleColorTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.
	lavaTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.

	Gnm::Texture genericPatternTexture[2];
	loadError = Framework::loadTextureFromGnf(&genericPatternTexture[0], "/app0/assets/gen_pattern0.gnf", 0, &framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );
	loadError = Framework::loadTextureFromGnf(&genericPatternTexture[1], "/app0/assets/gen_pattern3.gnf", 0, &framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );

	genericPatternTexture[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.
	genericPatternTexture[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.

	Gnm::Sampler pointSampler;
	pointSampler.init();

	Gnm::Sampler noMipMapSampler;
	noMipMapSampler.init();
	noMipMapSampler.setMipFilterMode(Gnm::kMipFilterModeNone);
	noMipMapSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
	noMipMapSampler.setLodRange(0, 0);

	Gnm::Sampler trilinearSampler;
	trilinearSampler.init();
	trilinearSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	trilinearSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	char temp[128];

	Framework::PsShader mainPixelShader, woodPixelShader, marblePixelShader, liquidPixelShader, triplanarPixelShader, texturePixelShader, colorPixelShader;
	Framework::PsShader* pixelShader[] = {&mainPixelShader, &woodPixelShader, &marblePixelShader, &liquidPixelShader, &triplanarPixelShader, &texturePixelShader, &colorPixelShader};
	const char *pixelShaderName[] = {"clay", "wood", "marble", "liquid", "triplanar", "texture", "color"};
	for(size_t i = 0; i < sizeof(pixelShader) / sizeof(pixelShader[0]); ++i)
	{
		snprintf(temp, 128, "/app0/histogram-gds-sample/shader_%s_p.sb", pixelShaderName[i]);
		Framework::ReadFile file(temp);
		Framework::LoadPsShader(pixelShader[i], file, &framework.m_allocators);
	}

	Framework::CsShader histogramComputeShaderGdsOptimized, histogramComputeShaderGds, histogramComputeShaderGdsLds, histogramComputeShaderRWOptimized, histogramComputeShaderRW, histogramComputeShaderRWLds;
	Framework::CsShader* computeShader[] ={&histogramComputeShaderGdsOptimized, &histogramComputeShaderGds, &histogramComputeShaderGdsLds,
												 &histogramComputeShaderRWOptimized,&histogramComputeShaderRW,&histogramComputeShaderRWLds};
	const char *computeShaderName[] = {"optimized_gds", "gds", "gds_lds", "optimized_rw", "rw", "rw_lds"};
	for(size_t i = 0; i < sizeof(computeShader) / sizeof(computeShader[0]); ++i)
	{
		snprintf(temp,128,"/app0/histogram-gds-sample/shader_histogram_%s_c.sb",computeShaderName[i]);
		Framework::ReadFile file(temp);
		Framework::LoadCsShader(computeShader[i], file, &framework.m_allocators);
	}

	Framework::VsShader meshVertexShader, histogramVertexShader;
	Framework::VsShader *vertexShader[] = {&meshVertexShader, &histogramVertexShader};
	const char *vertexShaderName[] = {"shader", "shader_histogram_gen"};
	for(size_t i = 0; i < sizeof(vertexShader) / sizeof(vertexShader[0]); ++i)
	{
		snprintf(temp,128,"/app0/histogram-gds-sample/%s_vv.sb",vertexShaderName[i]);
		Framework::ReadFile file(temp);
		Framework::LoadVsMakeFetchShader(vertexShader[i], file, &framework.m_allocators);
	}

	Framework::SimpleMesh isoSurface;
	LoadSimpleMesh(&framework.m_allocators, &isoSurface, "/app0/assets/isosurf.mesh");
	Framework::scaleSimpleMesh(&isoSurface, 0.3f);

	Framework::SimpleMesh iceMesh;
	LoadSimpleMesh(&framework.m_allocators, &iceMesh, "/app0/assets/icetext.mesh");
	Framework::scaleSimpleMesh(&iceMesh, 0.5f);

	Framework::SimpleMesh* meshes[] = { &isoSurface, &iceMesh };

	while (!framework.m_shutdownRequested)
	{
        framework.m_config.m_flipMode = iFlipModeIndex + 1; // +1 to convert to SceVideoOutFlipMode enum values

        Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		HistogramFrame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
        Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

        *frame->m_histogramDataReadyLabel = 1;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		gfxc->pushMarker("set default registers");
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);
		gfxc->popMarker();

		// Render to offscreen buffer
		gfxc->pushMarker("offscreen rendering");
		{
			gfxc->pushMarker("clear offscreen");
			Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &frame->m_offscreenRenderTarget, framework.getClearColor());
			Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &frame->m_depthTarget, 1.f);
			gfxc->setRenderTargetMask(0xF);

			gfxc->setRenderTarget(0, &frame->m_offscreenRenderTarget);
			gfxc->setDepthRenderTarget(&frame->m_depthTarget);
			Gnm::DepthStencilControl dsc;
			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
			dsc.setDepthEnable(true);
			gfxc->setDepthStencilControl(dsc);
			gfxc->setupScreenViewport(0, 0, frame->m_offscreenRenderTarget.getWidth(), frame->m_offscreenRenderTarget.getHeight(), 0.5f, 0.5f);
			gfxc->popMarker();

			gfxc->pushMarker("prepare shader inputs");

            gfxc->setVsShader(meshVertexShader.m_shader, 0, meshVertexShader.m_fetchShader, &meshVertexShader.m_offsetsTable);
            gfxc->setPsShader(pixelShader[iMaterialIndex]->m_shader, &pixelShader[iMaterialIndex]->m_offsetsTable);
			meshes[iMeshIndex]->SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

			cbMeshInstance * cnstBuf = static_cast<cbMeshInstance*>(gfxc->allocateFromCommandBuffer(sizeof(cbMeshInstance), Gnm::kEmbeddedDataAlignment4));
			Gnm::Buffer isoConstBuffer;
			isoConstBuffer.initAsConstantBuffer(cnstBuf, sizeof(*cnstBuf));
			isoConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &isoConstBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &isoConstBuffer);

			Framework::SetNoiseDataBuffers(*gfxc, Gnm::kShaderStagePs);

			Matrix4 m44LocalToWorld = Matrix4::scale(Vector3(1.5f));
			static float ax = 0, ay = -2 * 0.33f, az = 0;

			const float fShaderTime = framework.GetSecondsElapsedApparent();

			ay = fShaderTime*0.2f * 600 * 0.005f;
			ax = -fShaderTime*0.11f * 600 * 0.0045f;
			m44LocalToWorld *= Matrix4::translation(Vector3(0, 0, -1.0f));
			m44LocalToWorld *= Matrix4::rotationZYX(Vector3(ax, ay, az));

			Matrix4 m44WorldToCam = framework.m_worldToViewMatrix;
			const Matrix4 m44LocToCam = m44WorldToCam * m44LocalToWorld;
			const Matrix4 m44CamToLoc = inverse(m44LocToCam);
			const Matrix4 m44MVP = framework.m_projectionMatrix * m44LocToCam;
			const Matrix4 m44WorldToLocal = inverse(m44LocalToWorld);

			const Vector4 vLpos_world(30, 30, 50, 1);
			const Vector4 vLpos2_world(50, 50, -60, 1);
			cnstBuf->g_vCamPos = m44CamToLoc * Vector4(0, 0, 0, 1);
			cnstBuf->g_vLightPos = m44WorldToLocal * vLpos_world;
			cnstBuf->g_vLightPos2 = m44WorldToLocal * vLpos2_world;
			cnstBuf->g_mWorld = transpose(m44LocalToWorld);
			cnstBuf->g_mWorldViewProjection = transpose(m44MVP);
			cnstBuf->g_fTime = fShaderTime;

			// set marble colors
			if (iMaterialIndex >= 2)
			{
				if (iMaterialIndex == 4)
				{
					gfxc->setTextures(Gnm::kShaderStagePs, 2, 2, genericPatternTexture);
					gfxc->setSamplers(Gnm::kShaderStagePs, 2, 1, &trilinearSampler);
				}
				else
				{
					if (iMaterialIndex == 2) gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &marbleColorTexture);
					else gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &lavaTexture);
					gfxc->setSamplers(Gnm::kShaderStagePs, 2, 1, &noMipMapSampler);
				}
			}

			gfxc->popMarker();

			gfxc->pushMarker("draw call");
			gfxc->setPrimitiveType(meshes[iMeshIndex]->m_primitiveType);
			gfxc->setIndexSize(meshes[iMeshIndex]->m_indexType);
			gfxc->drawIndex(meshes[iMeshIndex]->m_indexCount, meshes[iMeshIndex]->m_indexBuffer);
			Gnmx::ResourceBarrier offscreenBarrier;
			offscreenBarrier.init(&frame->m_offscreenRenderTarget, Gnmx::ResourceBarrier::kUsageRenderTarget, Gnmx::ResourceBarrier::kUsageRoTexture);
			offscreenBarrier.enableDestinationCacheFlushAndInvalidate(true);
			offscreenBarrier.write(&gfxc->m_dcb);
			gfxc->popMarker();
		}
		gfxc->popMarker();

		// Compute histogram of offscreen texture
		bool useGds = iHistogramShader == kHistogramShaderOptimizedGds ||
					  iHistogramShader == kHistogramShaderGdsOnly ||
					  iHistogramShader == kHistogramShaderGdsWithLds;

		Gnm::Buffer histogramRwBuffer;
		histogramRwBuffer.initAsDataBuffer(frame->m_histogramRwBufferMem,Gnm::kDataFormatR32G32B32Uint,sizeof(HistogramColorData) / (3 * sizeof(uint32_t)));
		histogramRwBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we bind as RWBuffer, so it's GPU coherent

		gfxc->pushMarker("histogram generation");
		{
			// Set histogram compute shader and resources
            gfxc->setCsShader(computeShader[iHistogramShader]->m_shader, &computeShader[iHistogramShader]->m_offsetsTable);
			gfxc->setTextures(Gnm::kShaderStageCs, 0, 1, &frame->m_offscreenTexture);
			gfxc->setSamplers(Gnm::kShaderStageCs, 0, 1, &pointSampler);

			if (useGds)
			{
				// Clear global data share memory and bind
				gfxc->m_dcb.dmaData(Gnm::kDmaDataDstGds, frame->m_gdsMemoryOffset, Gnm::kDmaDataSrcData, 0, sizeof(HistogramColorData), Gnm::kDmaDataBlockingEnable);
				gfxc->setGdsMemoryRange(Gnm::kShaderStageCs, frame->m_gdsMemoryOffset, sizeof(HistogramColorData));
			}
			else
			{
				// Clear RW buffer and bind
				gfxc->fillData(histogramRwBuffer.getBaseAddress(), 0, histogramRwBuffer.getSize(), Gnm::kDmaDataBlockingEnable);
				gfxc->setRwBuffers(Gnm::kShaderStageCs, 0, 1, &histogramRwBuffer);
			}

			// Calculate optimal thread group configuration
			const uint32_t kNumThreadsX = 64;
			const uint32_t kNumThreadsY = 1;
			const uint32_t kNumTexelsPerThreadXY =
				// Keep in sync with TEXELS_PER_THREAD_XY in shader_histogram_optimized_c.pssl
				(iHistogramShader == kHistogramShaderOptimizedGds || iHistogramShader == kHistogramShaderOptimizedRWBuffer) ? 8 : 1;
            const uint32_t kTextureWidth = frame->m_offscreenTexture.getWidth();
            const uint32_t kTextureHeight = frame->m_offscreenTexture.getHeight();
			const uint32_t kDispatchSizeX = (uint32_t)ceilf((float)kTextureWidth / (kNumThreadsX * kNumTexelsPerThreadXY));
			const uint32_t kDispatchSizeY = (uint32_t)ceilf((float)kTextureHeight / (kNumThreadsY * kNumTexelsPerThreadXY));

			// Dispatch
			gfxc->dispatch(kDispatchSizeX, kDispatchSizeY, 1);

			// Since VS will consume GDS/RW data, wait for histogram generation to complete before trying to render it
			Gnmx::ResourceBarrier histogramDataBarrier;
            histogramDataBarrier.init(&histogramRwBuffer, Gnmx::ResourceBarrier::kUsageRwBuffer, Gnmx::ResourceBarrier::kUsageRoBuffer);
			histogramDataBarrier.enableDestinationCacheFlushAndInvalidate(true);
			histogramDataBarrier.write(&gfxc->m_dcb);

		#if SANITY_CHECK_HISTOGRAM_DATA
			const uint64_t zero = 0;

			gfxc->pushMarker("histogram sanity check");

			if (useGds)
			{
			#if 1
				// Copy out GDS data to CPU memory using GPU backend and use writeAtEndOfShader to write histogram data-ready marker
                gfxc->readDataFromGds(Gnm::kEosCsDone, (void*)frame->m_histogramData, frame->m_gdsMemoryOffset / sizeof(uint32_t), sizeof(HistogramColorData) / sizeof(uint32_t));
                gfxc->writeAtEndOfShader(Gnm::kEosCsDone, (void*)frame->m_histogramDataReadyLabel, 0);
			#else
				// Copy out GDS data to CPU memory using CP DMA and use writeDataInline to write histogram data-ready marker
                gfxc->m_dcb.dmaData(Gnm::kDmaDataDstMemory, (uintptr_t)frame->m_histogramData, Gnm::kDmaDataSrcGds, frame->m_gdsMemoryOffset, sizeof(HistogramColorData), Gnm::kDmaDataBlockingEnable);
				gfxc->writeDataInline((void*)frame->m_histogramDataReadyLabel, &zero, 2, Gnm::kWriteDataConfirmEnable);
			#endif
			}
			else
			{
				// Copy RW buffer data to CPU memory with blocking CP DMA and write histogram data-ready marker
                gfxc->copyData((void*)frame->m_histogramData, histogramRwBuffer.getBaseAddress(), histogramRwBuffer.getSize(), Gnm::kDmaDataBlockingEnable);
                gfxc->writeDataInline((void*)frame->m_histogramDataReadyLabel, &zero, 2, Gnm::kWriteDataConfirmEnable);
			}

			gfxc->popMarker();
		#endif
		}
		gfxc->popMarker();

		// Render to main back buffer
		gfxc->pushMarker("main buffer rendering");
		{
			gfxc->pushMarker("clear main");
            Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
            Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
			gfxc->setRenderTargetMask(0xF);

			Gnm::DepthStencilControl dsc;
			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
			dsc.setDepthEnable(true);
			gfxc->setDepthStencilControl(dsc);

            gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
            gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);
            gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);
			gfxc->popMarker();

			gfxc->pushMarker("prepare offscreen texture shaders");
            gfxc->setVsShader(meshVertexShader.m_shader, 0, meshVertexShader.m_fetchShader, &meshVertexShader.m_offsetsTable);
			gfxc->setPsShader(texturePixelShader.m_shader, &texturePixelShader.m_offsetsTable);
			
			quadMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

            gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &frame->m_offscreenTexture);
			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &trilinearSampler);

			Matrix4Unaligned* mvp = static_cast<Matrix4Unaligned *>(gfxc->allocateFromCommandBuffer(sizeof(Matrix4Unaligned), Gnm::kEmbeddedDataAlignment4));
			Gnm::Buffer quadConstBuffer;
			quadConstBuffer.initAsConstantBuffer(mvp, sizeof(Matrix4Unaligned));
			quadConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
			*mvp = transpose(Matrix4::translation(Vector3(-0.5f, 0.5f, 0)));
			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &quadConstBuffer);
			gfxc->popMarker();

			gfxc->pushMarker("draw offscreen texture");
			gfxc->setPrimitiveType(quadMesh.m_primitiveType);
			gfxc->setIndexSize(quadMesh.m_indexType);
			gfxc->drawIndex(quadMesh.m_indexCount, quadMesh.m_indexBuffer);
			gfxc->popMarker();

			gfxc->pushMarker("drawing color histograms");
			gfxc->setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
			gfxc->setVsShader(histogramVertexShader.m_shader, 0, histogramVertexShader.m_fetchShader, &histogramVertexShader.m_offsetsTable);
			gfxc->setPsShader(colorPixelShader.m_shader, &colorPixelShader.m_offsetsTable);
            gfxc->setGdsMemoryRange(Gnm::kShaderStageVs, frame->m_gdsMemoryOffset, sizeof(HistogramColorData));
            gfxc->setBuffers(Gnm::kShaderStageVs, 0, 1, &histogramRwBuffer);

			gfxc->pushMarker("red histogram");
			HistogramConstants* redHistogramConstants = static_cast<HistogramConstants *>(gfxc->allocateFromCommandBuffer(sizeof(HistogramConstants), Gnm::kEmbeddedDataAlignment4));
			Gnm::Buffer redHistogramConstBuffer;
			redHistogramConstBuffer.initAsConstantBuffer(redHistogramConstants, sizeof(HistogramConstants));
			redHistogramConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
			redHistogramConstants->g_mWorldViewProjection = Matrix4::identity();
			redHistogramConstants->g_vHistogramColor.x = 1.0f;
			redHistogramConstants->g_vHistogramColor.y = 0.0f;
			redHistogramConstants->g_vHistogramColor.z = 0.0f;
			redHistogramConstants->g_vHistogramColor.w = 1.0f;
			redHistogramConstants->g_histogramColorChannel = 0;
			redHistogramConstants->g_dataInGds = useGds ? 1 : 0;
			redHistogramConstants->g_histogramBarVerticalScale = 1.0f / kLog2NumTexelsOffscreen;
			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &redHistogramConstBuffer);
            gfxc->drawIndexAuto(kNumHistogramColors * 4); // 256 colors * 4 vertices per bar
			gfxc->popMarker();

			gfxc->pushMarker("green histogram");
			HistogramConstants* greenHistogramConstants = static_cast<HistogramConstants *>(gfxc->allocateFromCommandBuffer(sizeof(HistogramConstants), Gnm::kEmbeddedDataAlignment4));
			Gnm::Buffer greenHistogramConstBuffer;
			greenHistogramConstBuffer.initAsConstantBuffer(greenHistogramConstants, sizeof(HistogramConstants));
			greenHistogramConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
			greenHistogramConstants->g_mWorldViewProjection = transpose(Matrix4::translation(Vector3(0.0f, -1.0f, 0)));
			greenHistogramConstants->g_vHistogramColor.x = 0.0f;
			greenHistogramConstants->g_vHistogramColor.y = 1.0f;
			greenHistogramConstants->g_vHistogramColor.z = 0.0f;
			greenHistogramConstants->g_vHistogramColor.w = 1.0f;
			greenHistogramConstants->g_histogramColorChannel = 1;
			greenHistogramConstants->g_dataInGds = useGds ? 1 : 0;
			greenHistogramConstants->g_histogramBarVerticalScale = 1.0f / kLog2NumTexelsOffscreen;
			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &greenHistogramConstBuffer);
            gfxc->drawIndexAuto(kNumHistogramColors * 4); // 256 colors * 4 vertices per bar
			gfxc->popMarker();

			gfxc->pushMarker("blue histogram");
			HistogramConstants* blueHistogramConstants = static_cast<HistogramConstants *>(gfxc->allocateFromCommandBuffer(sizeof(HistogramConstants), Gnm::kEmbeddedDataAlignment4));
			Gnm::Buffer blueHistogramConstBuffer;
			blueHistogramConstBuffer.initAsConstantBuffer(blueHistogramConstants, sizeof(HistogramConstants));
			blueHistogramConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
			blueHistogramConstants->g_mWorldViewProjection = transpose(Matrix4::translation(Vector3(-1.0f, -1.0f, 0)));
			blueHistogramConstants->g_vHistogramColor.x = 0.0f;
			blueHistogramConstants->g_vHistogramColor.y = 0.0f;
			blueHistogramConstants->g_vHistogramColor.z = 1.0f;
			blueHistogramConstants->g_vHistogramColor.w = 1.0f;
			blueHistogramConstants->g_histogramColorChannel = 2;
			blueHistogramConstants->g_dataInGds = useGds ? 1 : 0;
			blueHistogramConstants->g_histogramBarVerticalScale = 1.0f / kLog2NumTexelsOffscreen;
			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &blueHistogramConstBuffer);
            gfxc->drawIndexAuto(kNumHistogramColors * 4); // 256 colors * 4 vertices per bar
			gfxc->popMarker();

			gfxc->popMarker();
		}
		gfxc->popMarker();

		framework.EndFrame(*gfxc);

	#if SANITY_CHECK_HISTOGRAM_DATA 
        while (*frame->m_histogramDataReadyLabel != 0);

		uint32_t sumRed = 0, sumGreen = 0, sumBlue = 0;
        for (uint32_t i = 0; i < kNumHistogramColors; ++i)
		{
            sumRed += frame->m_histogramData->pixelCount[i][0];
            sumGreen += frame->m_histogramData->pixelCount[i][1];
            sumBlue += frame->m_histogramData->pixelCount[i][2];
		}

		SCE_GNM_ASSERT_MSG(sumRed == kRenderToTextureTargetNumPixels, "Incorrect number of red pixels.");
		SCE_GNM_ASSERT_MSG(sumGreen == kRenderToTextureTargetNumPixels, "Incorrect number of green pixels.");
		SCE_GNM_ASSERT_MSG(sumBlue == kRenderToTextureTargetNumPixels, "Incorrect number of blue pixels.");
	#endif
	}

    HistogramFrame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
    Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
    framework.terminate(*gfxc);
    return 0;
}
