/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/frame.h"
#include "std_cbuffer.h"
#include <gnmx/resourcebarrier.h>
using namespace sce;

namespace
{
	Framework::GnmSampleFramework framework;

	bool g_alphaToMask = false;

	struct Options
	{
		Gnm::NumSamples m_cbSamples;
		Gnm::NumSamples m_detailSamples;
		Gnm::NumFragments m_cbFragments;
		Gnm::NumFragments m_dbFragments;
		int32_t m_sampleIndex;
		uint32_t m_whatToShow;
	};
	Options g_options = {Gnm::kNumSamples4, Gnm::kNumSamples4, Gnm::kNumFragments2, Gnm::kNumFragments2, 0, 1};
	Options g_oldOptions = g_options;
	bool isHardwareResolve() {return g_options.m_whatToShow == 3;}

	static void menuAdjustmentValidator()
	{
		if(int32_t(g_options.m_cbSamples) < int32_t(g_options.m_cbFragments))
			g_options.m_cbSamples = (Gnm::NumSamples)g_options.m_cbFragments;
		if(g_options.m_sampleIndex != g_oldOptions.m_sampleIndex) // if the sample index has changed,
		{
			if(g_options.m_sampleIndex < 0)
				g_options.m_sampleIndex = (1<<g_options.m_cbSamples)-1;
			if(g_options.m_sampleIndex > (1<<g_options.m_cbSamples)-1)
				g_options.m_sampleIndex = 0;
		}
		if(g_options.m_cbSamples != g_oldOptions.m_cbSamples) // if the number of samples has changed,
		{
			if(g_options.m_sampleIndex > (1<<g_options.m_cbSamples)-1)
				g_options.m_sampleIndex = (1<<g_options.m_cbSamples)-1;
		}
	}

	Gnm::RenderTarget multisampledTarget = {};
	Gnm::RenderTarget regularTarget = {};
	Gnm::DepthRenderTarget depthTarget = {};
	const uint32_t numSlices = 1;

	/** @brief Releases the CMASK, FMASK, and color surface associated with a RenderTarget. */
	static void FreeRenderTarget(Gnmx::Toolkit::IAllocator *allocator, Gnm::RenderTarget *target)
	{
		allocator->release(target->getFmaskAddress());
		allocator->release(target->getCmaskAddress());
		allocator->release(target->getBaseAddress());
	}

	/** @brief Initialize a RenderTarget, and allocate all associated surfaces (CMASK and FMASK) if necessary. */
	static void AllocateRenderTarget(Gnmx::Toolkit::IAllocator *allocator, Gnm::RenderTarget *target, Gnm::TileMode tileMode, Gnm::DataFormat format, Gnm::NumSamples numSamples, Gnm::NumFragments numFragments)
	{
		const uint32_t width = framework.m_config.m_targetWidth;
		const uint32_t height = framework.m_config.m_targetHeight;

		const bool isMultisampled = (numSamples != Gnm::kNumSamples1) || (numFragments != Gnm::kNumFragments1);

		Gnm::SizeAlign cmaskSizeAlign, fmaskSizeAlign;
		const Gnm::SizeAlign colorSizeAlign = Gnmx::Toolkit::init(target, width, height, 1, format, tileMode, numSamples, numFragments, isMultisampled ? &cmaskSizeAlign : 0, isMultisampled ? &fmaskSizeAlign : 0);

		void *colorAddr = allocator->allocate(colorSizeAlign);
		SCE_GNM_ASSERT(colorSizeAlign.m_size && colorAddr || !colorSizeAlign.m_size );
		target->setBaseAddress(colorAddr);

		if(isMultisampled)
		{
			void *cmaskAddr = allocator->allocate(cmaskSizeAlign);
			void *fmaskAddr = allocator->allocate(fmaskSizeAlign);
			SCE_GNM_ASSERT(cmaskSizeAlign.m_size && cmaskAddr || !cmaskSizeAlign.m_size);
			SCE_GNM_ASSERT(fmaskSizeAlign.m_size && fmaskAddr || !fmaskSizeAlign.m_size);
			target->setCmaskAddress(cmaskAddr);
			target->setFmaskAddress(fmaskAddr);
		}
		else
		{
			target->setCmaskAddress(nullptr);
			target->setFmaskAddress(nullptr);
		}

		target->setFmaskCompressionEnable(isMultisampled);
		target->setCmaskFastClearEnable(isMultisampled);
	}

	/** @brief Re-initialize all RenderTargets and DepthRenderTargets after a change in sample counts. */
	static void RecreateTargets(Gnmx::Toolkit::IAllocator *allocator)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();

		// Must release old buffers before allocating any new ones. Stack allocator!
		allocator->release(depthTarget.getHtileAddress());
		allocator->release(depthTarget.getZReadAddress());
		FreeRenderTarget(allocator, &regularTarget);
		FreeRenderTarget(allocator, &multisampledTarget);

		const uint32_t width = framework.m_config.m_targetWidth;
		const uint32_t height = framework.m_config.m_targetHeight;

		// Recreate color buffers
		Gnm::DataFormat colorFormat = Gnm::kDataFormatB8G8R8A8UnormSrgb;
		Gnm::TileMode multisampledTileMode, regularTileMode;
		// When using the hardware MSAA resolve, the source and destination target must have the same micro tile mode and linearity.
		// In this sample, we're resolving directly into the final displayable buffer, so the multisampled target must be displayable as well.
		if (isHardwareResolve())
			multisampledTileMode = bufferCpuIsWriting->m_renderTarget.getTileMode();
		else
			GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &multisampledTileMode, GpuAddress::kSurfaceTypeColorTarget, colorFormat, 1 << g_options.m_cbFragments);
		// This requirement isn't necessary when resolving with a pixel shader, so the intermediate target can be non-displayable.
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &regularTileMode, GpuAddress::kSurfaceTypeColorTarget, colorFormat, 1);

		AllocateRenderTarget(allocator, &multisampledTarget, multisampledTileMode, colorFormat, g_options.m_cbSamples, g_options.m_cbFragments);
		AllocateRenderTarget(allocator, &regularTarget, regularTileMode, colorFormat, Gnm::kNumSamples1, Gnm::kNumFragments1);

		// recreate depth buffers
		const Gnm::DataFormat depthFormat = Gnm::DataFormat::build(Gnm::kZFormat32Float);
		Gnm::TileMode depthTileMode;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &depthTileMode, GpuAddress::kSurfaceTypeDepthOnlyTarget, depthFormat, 1 << Gnm::kNumFragments8);
		Gnm::SizeAlign depthHtileSizeAlign;
		const Gnm::SizeAlign depthSizeAlign = Gnmx::Toolkit::init(&depthTarget, width, height, numSlices, depthFormat.getZFormat(), Gnm::kStencilInvalid, depthTileMode, Gnm::kNumFragments8, NULL, &depthHtileSizeAlign);
		void *depthAddr = allocator->allocate(depthSizeAlign);
		void *htileAddr = allocator->allocate(depthHtileSizeAlign);
 	
		depthTarget.setZReadAddress(depthAddr);
		depthTarget.setZWriteAddress(depthAddr);
		depthTarget.setHtileAddress(htileAddr);
		depthTarget.setHtileAccelerationEnable(true);
	}

	Framework::MenuItemText modeText[4] = {
		{"Show Fmask", "Entries in FMASK with samples == 'sampleIndex'"}, 
		{"Show Fragment", "A fragment that contributes to resolved image"}, 
		{"Software Resolve", "Output of software resolve shader"},
		{"Hardware Resolve", "Output of hardware resolve"},
	};

	Framework::MenuCommandBool alphaToMask(&g_alphaToMask);
	Framework::MenuCommandEnum mode(modeText, sizeof(modeText)/sizeof(modeText[0]), &g_options.m_whatToShow);
	Framework::MenuCommandUintPowerOfTwo cbSamples((uint32_t*)&g_options.m_cbSamples, (uint32_t)Gnm::kNumSamples2, (uint32_t)Gnm::kNumSamples16);
	Framework::MenuCommandUintPowerOfTwo cbFragments((uint32_t*)&g_options.m_cbFragments, (uint32_t)Gnm::kNumFragments2, (uint32_t)Gnm::kNumFragments8);
	Framework::MenuCommandUintPowerOfTwo detailSamples((uint32_t*)&g_options.m_detailSamples, (uint32_t)Gnm::kNumSamples2, (uint32_t)Gnm::kNumSamples8);
	Framework::MenuCommandUintPowerOfTwo dbFragments((uint32_t*)&g_options.m_dbFragments, (uint32_t)Gnm::kNumFragments2, (uint32_t)Gnm::kNumFragments8);
	Framework::MenuCommandInt sampleIndex(&g_options.m_sampleIndex);
	const Framework::MenuItem menuItem[] =
	{
		{{"Alpha To Mask", "Distributes samples such that the proper percentage are covered when looking at any subset of samples"}, &alphaToMask},
		{{"Mode", "Enables choosing among several ways to visualize the color buffer"}, &mode},
		{{"CB Samples", "Number of color samples per pixel, each of which refers to a CB Fragment"}, &cbSamples},
		{{"CB Fragments", "Number of color fragments per pixel - like a palette from which CB Samples draw"}, &cbFragments},
		{{"Detail Samples", "Number of samples used for geometric edge anti-aliasing"}, &detailSamples},
		{{"DB Fragments", "Number of depth fragments per pixel"}, &dbFragments},
		{{"Sample Index", "The index of the color sample to display"}, &sampleIndex},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
}

int main( int argc, const char* argv[] )
{
	framework.processCommandLine(argc, argv);

	framework.m_config.m_depthBufferIsMeaningful = false;
	framework.m_config.m_clearColorIsMeaningful = false;
	framework.m_config.m_lightingIsMeaningful = true;

	framework.initialize("EQAA", 
		"Sample code for handling EQAA hardware, which extends the functionality of MSAA.",
		"This sample program illustrates how to manipulate EQAA hardware, which extends the functionality of MSAA.");

	framework.appendMenuItems(kMenuItems, menuItem);
	framework.m_menuAdjustmentValidator = menuAdjustmentValidator;

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Constants *m_constants;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Command Buffer",buffer);
	}

	Gnm::Sampler resSampler;
	resSampler.init();

	int error = 0;
	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader, resolvePixelShader;

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/eqaa-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/eqaa-sample/shader_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&resolvePixelShader, Framework::ReadFile("/app0/eqaa-sample/pixResolveMsaa_p.sb"), &framework.m_allocators);

	Framework::SimpleMesh simpleMesh;
	BuildTorusMesh(&framework.m_allocators, "Torus", &simpleMesh, 0.8f, 0.2f, 16, 8, 4, 1);

	Gnm::Sampler colorSampler, normalSampler, defaultSampler;
	colorSampler.init();
	colorSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
	colorSampler.setAnisotropyRatio(Gnm::kAnisotropyRatio16);
	normalSampler.init();
	defaultSampler.init();
	Gnm::Texture colorTex, normalTex;
	error = Framework::loadTextureFromGnf(&colorTex, Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&normalTex, Framework::ReadFile("/app0/assets/icelogo-normal.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	colorTex.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we won't ever bind this texture as RWTexture, so it's safe to declare read-only
	normalTex.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we won't ever bind this texture as RWTexture, so it's safe to declare read-only

	multisampledTarget.setAddresses((void*)0, (void*)0, (void*)0);
	regularTarget.setAddresses((void*)0, (void*)0, (void*)0);
	depthTarget.setAddresses((void*)0, (void*)0);
	RecreateTargets(&framework.m_garlicAllocator);

	while( !framework.m_shutdownRequested )
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		Gnm::Texture multisampledTexture, fmaskTexture;
		if(memcmp(&g_options,&g_oldOptions,sizeof(g_options)) != 0)
		{
			RecreateTargets(&framework.m_garlicAllocator);
			g_oldOptions = g_options;
		}

		multisampledTexture.initFromRenderTarget(&multisampledTarget, false);
		multisampledTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // won't ever bind this texture as RWTexture, so it's safe to declare read-only
		fmaskTexture.initAsFmask(&multisampledTarget);
		fmaskTexture.setBaseAddress(multisampledTarget.getFmaskAddress());
		fmaskTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // won't ever bind this texture as RWTexture, so it's safe to declare read-only

		gfxc->pushMarker("Begin Rendering");
		gfxc->setRenderTargetMask(0x0000000F);

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		gfxc->pushMarker("Clear");
		regularTarget.setCmaskClearColor(0xFF000000, 0x00000000);
		multisampledTarget.setCmaskClearColor(0xFF000000, 0x00000000);
		if (regularTarget.getCmaskFastClearEnable())
			Gnmx::Toolkit::SurfaceUtil::clearCmaskSurface(*gfxc, &regularTarget);
		if (multisampledTarget.getCmaskFastClearEnable())
			Gnmx::Toolkit::SurfaceUtil::clearCmaskSurface(*gfxc, &multisampledTarget);
		Gnm::Htile htile = {};
		htile.m_hiZ.m_zMask = 0; // tile is clear
		htile.m_hiZ.m_maxZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is > 1.f
		htile.m_hiZ.m_minZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is < 1.f
		if (depthTarget.getHtileAccelerationEnable())
			Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(*gfxc, &depthTarget, htile);
		gfxc->setDepthClearValue(1.f);
		gfxc->setRenderTargetMask(0xF);
		gfxc->setRenderTarget(0, &multisampledTarget);
		gfxc->setDepthRenderTarget(&depthTarget);

		gfxc->popMarker();

		gfxc->pushMarker("Enable EQAA");

		Gnm::DepthEqaaControl eqaaReg;
		eqaaReg.init();
		eqaaReg.setMaxAnchorSamples(g_options.m_detailSamples);
		eqaaReg.setAlphaToMaskSamples(g_options.m_detailSamples);
		eqaaReg.setStaticAnchorAssociations(true);
		gfxc->setDepthEqaaControl(eqaaReg);

		gfxc->setScanModeControl(Gnm::kScanModeControlAaEnable, Gnm::kScanModeControlViewportScissorEnable);
		gfxc->setAaDefaultSampleLocations(g_options.m_detailSamples);

		gfxc->popMarker();

		gfxc->pushMarker("Draw Mesh");
		Gnm::DbRenderControl dbRenderControl;
		dbRenderControl.init();
		dbRenderControl.setDepthClearEnable(false);
		gfxc->setDbRenderControl(dbRenderControl);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);

		gfxc->setupScreenViewport(0, 0, multisampledTarget.getWidth(), multisampledTarget.getHeight(), 0.5f, 0.5f);
		{
			Gnm::ClipControl clipReg;
			clipReg.init();
			gfxc->setClipControl(clipReg);
		}

		Constants *constants = frame->m_constants;
		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.

		gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
		gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

		simpleMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &colorTex);
		gfxc->setTextures(Gnm::kShaderStagePs, 1, 1, &normalTex);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &colorSampler);
		gfxc->setSamplers(Gnm::kShaderStagePs, 1, 1, &normalSampler);

		const float angle = 0.1f * (float)framework.GetSecondsElapsedApparent();

		Matrix4 modelMatrix = Matrix4::rotationZYX(Vector3(angle,angle,0.f));
		modelMatrix *= Matrix4::scale(Vector3(0.75f));
		constants->m_modelView = transpose( framework.m_worldToViewMatrix * modelMatrix );
		constants->m_modelViewProjection = transpose( framework.m_viewProjectionMatrix * modelMatrix );
		constants->m_lightColor = framework.getLightColor();
		constants->m_ambientColor = framework.getAmbientColor();
		constants->m_lightAttenuation = Vector4(1, 0, 0, 0);

		constants->m_lightPosition = framework.getLightPositionInViewSpace();

		constants->m_eyePosition = framework.getCameraPositionInWorldSpace();
		constants->m_rtInfo.m_numSamples = 1<<g_options.m_cbSamples;
		constants->m_rtInfo.m_numFragments = 1<<g_options.m_cbFragments;
		constants->m_rtInfo.m_whatToShow = g_options.m_whatToShow;
		constants->m_rtInfo.m_sampleIndex = g_options.m_sampleIndex;
		constants->m_rtInfo.m_width = framework.m_config.m_targetWidth;
		constants->m_rtInfo.m_height = framework.m_config.m_targetHeight;

		Gnm::AlphaToMaskControl alphaToMask;
		alphaToMask.init();
		alphaToMask.setEnabled(Gnm::kAlphaToMaskEnable);
		alphaToMask.setPixelDitherThresholds(
			Gnm::kAlphaToMaskDitherThreshold3,
			Gnm::kAlphaToMaskDitherThreshold1, 
			Gnm::kAlphaToMaskDitherDefault, 
			Gnm::kAlphaToMaskDitherThreshold3);
		alphaToMask.setDitherMode(Gnm::kAlphaToMaskDitherModeDisabled);
		if (g_alphaToMask)
		{
			gfxc->setAlphaToMaskControl(alphaToMask);
		}

		gfxc->setPrimitiveType(simpleMesh.m_primitiveType);
		gfxc->setIndexSize(simpleMesh.m_indexType);

		gfxc->drawIndex(simpleMesh.m_indexCount, simpleMesh.m_indexBuffer);

		gfxc->popMarker();

		gfxc->setRenderTargetMask(0x0000000F);

		alphaToMask.setEnabled(Gnm::kAlphaToMaskDisable);
		if (g_alphaToMask)
			gfxc->setAlphaToMaskControl(alphaToMask);

		gfxc->pushMarker("Resolve");

		if(isHardwareResolve())
		{
			// Hardware resolve doesn't seem to work if the source & destination's micro tile modes don't match.
			Gnmx::hardwareMsaaResolve(gfxc, &bufferCpuIsWriting->m_renderTarget, &multisampledTarget);

			gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
			gfxc->setDepthRenderTarget((Gnm::DepthRenderTarget *)NULL);
		}
		else
		{
			Gnmx::decompressFmaskSurface(gfxc, &multisampledTarget);

			Gnmx::ResourceBarrier resourceBarrier;
			resourceBarrier.init(&multisampledTarget, Gnmx::ResourceBarrier::kUsageRenderTarget, Gnmx::ResourceBarrier::kUsageRoTexture);
			resourceBarrier.write(&gfxc->m_dcb);

			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncAlways);
			dsc.setDepthEnable(false);
			gfxc->setDepthStencilControl(dsc);

			// Final resolve to the main render target (which must be untiled)
			gfxc->setScanModeControl(Gnm::kScanModeControlAaDisable, Gnm::kScanModeControlViewportScissorEnable);

			gfxc->setRenderTarget(0, &regularTarget);
			gfxc->setDepthRenderTarget((Gnm::DepthRenderTarget *)NULL);

			gfxc->setMarker("Shader Resolve");
			gfxc->setPsShader(resolvePixelShader.m_shader, &resolvePixelShader.m_offsetsTable);

//			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &resSampler);
//			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &defaultSampler);
			gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &multisampledTexture);
			gfxc->setTextures(Gnm::kShaderStagePs, 1, 1, &fmaskTexture);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

			Gnmx::renderFullScreenQuad(gfxc);

			Gnmx::Toolkit::SurfaceUtil::copyRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, &regularTarget);
		}

		gfxc->setScanModeControl(Gnm::kScanModeControlAaDisable, Gnm::kScanModeControlViewportScissorEnable);
		gfxc->setAaDefaultSampleLocations(Gnm::kNumSamples1);
		
		gfxc->popMarker(); // "Resolve"
		gfxc->popMarker(); // "Begin Rendering"

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
	return 0;
}

