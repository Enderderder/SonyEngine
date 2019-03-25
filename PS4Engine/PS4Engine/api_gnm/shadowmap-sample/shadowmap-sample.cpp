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
#include "std_srtstructs.h"
#include <gnmx/resourcebarrier.h>
using namespace sce;

namespace
{
	Framework::GnmSampleFramework framework;

	class RuntimeOptions
	{
	public:
		bool m_useDepthFastClear;
		bool m_useShadowFastClear;
		bool m_useHtile;
		bool m_useSrt;
		bool m_useDcc;
		RuntimeOptions()
		: m_useDepthFastClear(true)
		, m_useShadowFastClear(true)
		, m_useHtile(true)
		, m_useSrt(false)
		, m_useDcc(Gnm::getGpuMode() == Gnm::kGpuModeNeo)
		{
		}
		bool UseHtile() const
		{
			return m_useHtile;
		}
		bool UseDepthFastClear() const
		{
			return m_useHtile && m_useDepthFastClear;
		}
		bool UseShadowFastClear() const
		{
			return m_useHtile && m_useShadowFastClear;
		}
		bool UseSrt() const
		{
			return m_useSrt;
		}
		bool UseDcc() const
		{
			return m_useDcc;
		}
	};
	RuntimeOptions g_runtimeOptions;

	class MenuCommandBoolNeedsHTile : public Framework::MenuCommandBool
	{
	public:
		MenuCommandBoolNeedsHTile(bool* target)
		: Framework::MenuCommandBool(target)
		{
		}
		virtual bool isEnabled() const
		{
			return g_runtimeOptions.m_useHtile;
		}
	};

	double shadowClearMs = 0;
	double depthColorClearMs = 0;
	float shadowMapSize = 1.f;

	Framework::MenuCommandBool htile(&g_runtimeOptions.m_useHtile);
	MenuCommandBoolNeedsHTile depthFastClear(&g_runtimeOptions.m_useDepthFastClear);
	MenuCommandBoolNeedsHTile shadowFastClear(&g_runtimeOptions.m_useShadowFastClear);
	Framework::MenuCommandFloat menuShadowMapSize(&shadowMapSize, 0.5f, 4.f, 0.25f);
	Framework::MenuCommandBool srt(&g_runtimeOptions.m_useSrt);
	Framework::MenuCommandBool menuUseDcc(&g_runtimeOptions.m_useDcc);
	const Framework::MenuItem menuItem[] =
	{
		{{"Fast depth clear", "If this is enabled, the main scene's depth buffer will be cleared by explicitly setting all dwords in its HTILE buffer to a 'clear' value, rather than by rendering full-screen geometry with a 'clear' state flag. "
		"The advantage to clearing the HTILE buffer is that it is not possible for the hardware to drop down to a 'slow path' without warning if one of many graphics states is set imperfectly before the clear. Moreover, clearing the HTILE buffer "
		"does not itself change graphics state, and so there is no danger that changes to a 'clear' function will silently affect subsequent graphics commands"}, &depthFastClear},
		{{"Shadow fast clear", "Use HTILE to clear shadow depth buffer"}, &shadowFastClear},
		{{"HTILE", "Are HTILE buffers used?"}, &htile},
		{{"Shadow map size", "Length of shadow map edge in world space"}, &menuShadowMapSize},
		{{"Use Shader Resource Table", "Choose SRT or non-SRT shader resource bindings"}, &srt},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };

	const Framework::MenuItem menuItemDcc = {{"Use DCC", "Is Delta Color Compression used?"}, &menuUseDcc};
}

int main(int argc, const char *argv[])
{
	framework.m_config.m_lightPositionX = Vector3(0.f, 10.f, -5.f);
	framework.m_config.m_lightTargetX = Vector3(0.f, -5.f, -5.f);
	const Vector3 ideal(1.f, 0.f, 0.f);
	const Vector3 forward = normalize(framework.m_config.m_lightTargetX - framework.m_config.m_lightPositionX);
	framework.m_config.m_lightUpX = normalize(ideal - forward * dot(forward, ideal));

	framework.processCommandLine(argc, argv);

	framework.m_config.m_lightingIsMeaningful = true;
	framework.m_config.m_htile = true;

	framework.initialize( "ShadowMap", 
		"Sample code to illustrate how to render a scene into a shadowmap, and then how to read from that shadowmap while rendering another scene.", 
		"This sample program renders an ICE logo into a shadow map, and then renders the ICE logo atop a plane, while reading this shadowmap.");

	framework.appendMenuItems(kMenuItems, menuItem);

	if (Gnm::getGpuMode() == Gnm::kGpuModeNeo)
	{
		framework.appendMenuItems(1, &menuItemDcc);
	}

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Gnm::DepthRenderTarget m_shadowDepthTarget;
		Gnmx::ResourceBarrier m_shadowResourceBarrier;
		Constants *m_iceLogoConstants;
		Matrix4Unaligned *m_shadowMapConstants;
		Constants *m_quadConstants;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		const uint32_t shadowWidth = 2048;
		const uint32_t shadowHeight = 2048;
		Gnm::SizeAlign shadowHtileSizeAlign;
		Gnm::DataFormat shadowDepthFormat = Gnm::DataFormat::build(Gnm::kZFormat32Float);
		Gnm::TileMode shadowDepthTileMode;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&shadowDepthTileMode,GpuAddress::kSurfaceTypeDepthOnlyTarget,shadowDepthFormat,1);
		Gnm::SizeAlign shadowDepthTargetSizeAlign = Gnmx::Toolkit::init(&frame->m_shadowDepthTarget,shadowWidth,shadowHeight,shadowDepthFormat.getZFormat(),Gnm::kStencilInvalid,shadowDepthTileMode,Gnm::kNumFragments1,NULL,&shadowHtileSizeAlign);
		void *shadowZ;
		framework.m_allocators.allocate(&shadowZ,SCE_KERNEL_WC_GARLIC,shadowDepthTargetSizeAlign,Gnm::kResourceTypeDepthRenderTargetBaseAddress,"Buffer %d Shadow Depth",buffer);
		frame->m_shadowDepthTarget.setZReadAddress(shadowZ);
		frame->m_shadowDepthTarget.setZWriteAddress(shadowZ);
		void *shadowHtile;
		framework.m_allocators.allocate(&shadowHtile,SCE_KERNEL_WC_GARLIC,shadowHtileSizeAlign,Gnm::kResourceTypeDepthRenderTargetHTileAddress,"Buffer %d Shadow HTILE",buffer);
		frame->m_shadowDepthTarget.setHtileAddress(shadowHtile);
		frame->m_shadowResourceBarrier.init(&frame->m_shadowDepthTarget,Gnmx::ResourceBarrier::kUsageDepthSurface,Gnmx::ResourceBarrier::kUsageRoTexture);

		framework.m_allocators.allocate((void**)&frame->m_iceLogoConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_iceLogoConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d ICE logo constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_shadowMapConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_shadowMapConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d shadow map constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_quadConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_quadConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d quad constants",buffer);

		if(Gnm::getGpuMode() == Gnm::kGpuModeNeo)
		{
			framework.m_buffer[buffer].m_renderTarget.setDccCompressionEnable(true);
			const Gnm::SizeAlign dccSizeAlign = framework.m_buffer[buffer].m_renderTarget.getDccSizeAlign();
			void *dccBuffer;
			framework.m_allocators.allocate(&dccBuffer,SCE_KERNEL_WC_GARLIC,dccSizeAlign,Gnm::kResourceTypeGenericBuffer,"Buffer %d DCC Buffer",buffer);
			framework.m_buffer[buffer].m_renderTarget.setDccAddress(dccBuffer);
		}
	}

	int error = 0;

	Framework::VsShader vertexShader, srtVertexShader, shadowVertexShader;
	Framework::PsShader logoPixelShader, logoSrtPixelShader, planePixelShader, planeSrtPixelShader;

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/shadowmap-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadVsMakeFetchShader(&srtVertexShader, Framework::ReadFile("/app0/shadowmap-sample/shader_srt_vv.sb"), &framework.m_allocators);
	error = Framework::LoadVsMakeFetchShader(&shadowVertexShader, Framework::ReadFile("/app0/shadowmap-sample/basic_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&logoPixelShader, Framework::ReadFile("/app0/shadowmap-sample/logo_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&logoSrtPixelShader, Framework::ReadFile("/app0/shadowmap-sample/logo_srt_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&planePixelShader, Framework::ReadFile("/app0/shadowmap-sample/plane_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&planeSrtPixelShader, Framework::ReadFile("/app0/shadowmap-sample/plane_srt_p.sb"), &framework.m_allocators);

	Framework::SimpleMesh shadowCasterMesh, quadMesh;
	LoadSimpleMesh(&framework.m_allocators, &shadowCasterMesh, "/app0/assets/icetext.mesh");
	BuildQuadMesh(&framework.m_allocators, "Quad", &quadMesh, 10);

	Gnm::Sampler *samplers;
	framework.m_allocators.allocate((void**)&samplers, SCE_KERNEL_WC_GARLIC, sizeof(Gnm::Sampler) * 3, 4, Gnm::kResourceTypeGenericBuffer, "Sampler Buffer");
	for(int s=0; s<2; s++)
	{
		samplers[s].init();
		samplers[s].setMipFilterMode(Gnm::kMipFilterModeLinear);
		samplers[s].setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
	}
	samplers[2].init();
	samplers[2].setDepthCompareFunction(Gnm::kDepthCompareLessEqual);
	samplers[2].setWrapMode(Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel);

	Gnm::Texture *textures;
	framework.m_allocators.allocate((void**)&textures, SCE_KERNEL_WC_GARLIC, sizeof(Gnm::Texture) * 3, 4, Gnm::kResourceTypeGenericBuffer, "Texture Buffer");
	error = Framework::loadTextureFromGnf(&textures[0], Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error  == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&textures[1], Framework::ReadFile("/app0/assets/icelogo-normal.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never write to this texture from a shader, so it's OK to mark it as read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never write to this texture from a shader, so it's OK to mark it as read-only.

	// Projection Matrix
	framework.SetViewToWorldMatrix(inverse(Matrix4::lookAt(Point3(0,0,0), Point3(0,-0.5f,-1), Vector3(0,1,0))));

	// Set up frame timing
	enum { kShadowTimer, kDepthColorClearTimer, kTimers };
	const Gnm::SizeAlign timerSizeAlign = Gnmx::Toolkit::Timers::getRequiredBufferSizeAlign(kTimers);
	void *timerPtr;
	framework.m_allocators.allocate(&timerPtr, SCE_KERNEL_WC_GARLIC, timerSizeAlign, Gnm::kResourceTypeGenericBuffer, "Timer Buffer");
	Gnmx::Toolkit::Timers timers;
	timers.initialize(timerPtr, kTimers);

	const Matrix4 clipSpaceToTextureSpace = Matrix4::translation(Vector3(0.5f, 0.5f, 0.5f)) * Matrix4::scale(Vector3(0.5f, -0.5f, 0.5f));

	while( !framework.m_shutdownRequested )
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		textures[2].initFromDepthRenderTarget(&frame->m_shadowDepthTarget, false);
		textures[2].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never write to this texture from a shader, so it's OK to mark it as read-only.

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		const float d = shadowMapSize * 0.5f;
		const Matrix4 shadowTargetProjection = Matrix4::frustum(-d, d, -d, d, 1, 100.0f);
		const Matrix4 shadowTargetViewProjection = shadowTargetProjection * framework.m_worldToLightMatrix;
		const Matrix4 shadowMapViewProjection = clipSpaceToTextureSpace * shadowTargetViewProjection;

		frame->m_shadowDepthTarget.setHtileAccelerationEnable(g_runtimeOptions.UseHtile());
		bufferCpuIsWriting->m_depthTarget.setHtileAccelerationEnable(g_runtimeOptions.UseHtile());

		if (Gnm::getGpuMode() == Gnm::kGpuModeNeo)
		{
			bufferCpuIsWriting->m_renderTarget.setDccCompressionEnable(g_runtimeOptions.UseDcc());
		}
		if (g_runtimeOptions.UseDcc())
		{
			memset(bufferCpuIsWriting->m_renderTarget.getDccAddress(), 0xFF, bufferCpuIsWriting->m_renderTarget.getDccSliceSizeInBytes());
		}

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		const float radians = framework.GetSecondsElapsedApparent();
		const Matrix4 logoMatrix = Matrix4::translation(Vector3(0,0,-5)) * Matrix4::rotationZYX(Vector3(-radians,-radians,0));
		const Matrix4 quadMatrix = Matrix4::translation(Vector3(0,-5,-5)) * Matrix4::rotationX(-1.57f);

		// Render Shadow Map

		timers.begin(gfxc->m_dcb, kShadowTimer);
		if(g_runtimeOptions.UseShadowFastClear())
		{
			Gnm::Htile htile = {};
			htile.m_hiZ.m_zMask = 0; // tile is clear
			htile.m_hiZ.m_maxZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is > 1.f
			htile.m_hiZ.m_minZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is < 1.f
			Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(*gfxc, &frame->m_shadowDepthTarget, htile);
			gfxc->setDepthClearValue(1.f);
		}
		else
			Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &frame->m_shadowDepthTarget, 1.f);

		gfxc->setRenderTargetMask(0x0); // we're about to render a shadow map, so we should turn off rasterization to color buffers.

		timers.end(gfxc->m_dcb, kShadowTimer);
		gfxc->setDepthRenderTarget(&frame->m_shadowDepthTarget);
		gfxc->setupScreenViewport(0, 0, frame->m_shadowDepthTarget.getWidth(), frame->m_shadowDepthTarget.getHeight(), 0.5f, 0.5f);
		{
			Gnm::ClipControl clipReg;
			clipReg.init();
			gfxc->setClipControl(clipReg);
		}

		gfxc->setVsShader(shadowVertexShader.m_shader, 0, shadowVertexShader.m_fetchShader, &shadowVertexShader.m_offsetsTable);
		gfxc->setPsShader((Gnmx::PsShader*)NULL, NULL);	

		shadowCasterMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		gfxc->setRenderTargetMask(0x0);

		Gnm::Buffer shadowMapConstantBuffer;
		shadowMapConstantBuffer.initAsConstantBuffer(frame->m_shadowMapConstants, sizeof(*frame->m_shadowMapConstants));
		shadowMapConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.

		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &shadowMapConstantBuffer);
		*frame->m_shadowMapConstants = transpose(shadowTargetViewProjection * logoMatrix);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);

		primSetupReg.setPolygonOffsetEnable(Gnm::kPrimitiveSetupPolygonOffsetEnable, Gnm::kPrimitiveSetupPolygonOffsetEnable);
		gfxc->setPrimitiveSetup(primSetupReg);
		gfxc->setPolygonOffsetFront(5.0f, 6000.0f);
		gfxc->setPolygonOffsetBack(5.0f, 6000.0f);
		gfxc->setPolygonOffsetZFormat(Gnm::kZFormat32Float);

		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceFront);
		gfxc->setPrimitiveSetup(primSetupReg);

		gfxc->setPrimitiveType(shadowCasterMesh.m_primitiveType);
		gfxc->setIndexSize(shadowCasterMesh.m_indexType);
		gfxc->drawIndex(shadowCasterMesh.m_indexCount, shadowCasterMesh.m_indexBuffer);

		primSetupReg.setPolygonOffsetEnable(Gnm::kPrimitiveSetupPolygonOffsetDisable, Gnm::kPrimitiveSetupPolygonOffsetDisable);
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		gfxc->setPrimitiveSetup(primSetupReg);

		// Decompress shadow surfaces before we sample from them
		Gnmx::decompressDepthSurface(gfxc, &frame->m_shadowDepthTarget);
		Gnm::DbRenderControl dbRenderControl;
		dbRenderControl.init();
		gfxc->setDbRenderControl(dbRenderControl);

		frame->m_shadowResourceBarrier.write(&gfxc->m_dcb);

		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		gfxc->setPrimitiveSetup(primSetupReg);
		// Render Main View
		{
			Gnm::ClipControl clipReg;
			clipReg.init();
			clipReg.setClipEnable(false);
			gfxc->setClipControl(clipReg);
		}

		dbRenderControl.init();
		dbRenderControl.setDepthClearEnable(true);
		dbRenderControl.setDepthTileWriteBackPolicy(Gnm::kDbTileWriteBackPolicyCompressionAllowed);
		gfxc->setDbRenderControl(dbRenderControl);
		timers.begin(gfxc->m_dcb, kDepthColorClearTimer);
		{
			Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
			if( g_runtimeOptions.UseDepthFastClear() )
			{
				Gnm::Htile htile = {};
				htile.m_hiZ.m_zMask = 0; // tile is clear
				htile.m_hiZ.m_maxZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is > 1.f
				htile.m_hiZ.m_minZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is < 1.f
				Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(*gfxc, &bufferCpuIsWriting->m_depthTarget, htile);
				gfxc->setDepthClearValue(1.f);
			}
			else
			{
				Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
			}
		}
		timers.end(gfxc->m_dcb, kDepthColorClearTimer);
		gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
		gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);
		gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);
		dbRenderControl.setDepthClearEnable(false);
		gfxc->setDbRenderControl(dbRenderControl);
		gfxc->setRenderTargetMask(0xF);
		{
			Gnm::ClipControl clipReg;
			clipReg.init();
			gfxc->setClipControl(clipReg);
		}
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		gfxc->setDepthStencilControl(dsc);

		frame->m_iceLogoConstants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*logoMatrix);
		frame->m_iceLogoConstants->m_lightPosition = framework.getLightPositionInViewSpace();
		frame->m_iceLogoConstants->m_shadow_mvp = transpose(shadowMapViewProjection * logoMatrix);
		frame->m_iceLogoConstants->m_modelView = transpose(framework.m_worldToViewMatrix*logoMatrix);
		frame->m_iceLogoConstants->m_lightColor = framework.getLightColor();
		frame->m_iceLogoConstants->m_ambientColor = framework.getAmbientColor();
		frame->m_iceLogoConstants->m_lightAttenuation = Vector4(1, 0, 0, 0);

		if (!g_runtimeOptions.UseSrt())
		{
			gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
			gfxc->setPsShader(logoPixelShader.m_shader, &logoPixelShader.m_offsetsTable);

			gfxc->setTextures(Gnm::kShaderStagePs, 0, 3, textures);
			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 3, samplers);

			Gnm::Buffer iceLogoConstantBuffer;
			iceLogoConstantBuffer.initAsConstantBuffer(frame->m_iceLogoConstants, sizeof(*frame->m_iceLogoConstants));
			iceLogoConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &iceLogoConstantBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &iceLogoConstantBuffer);
		}
		else
		{
			gfxc->setVsShader(srtVertexShader.m_shader, 0, srtVertexShader.m_fetchShader, &srtVertexShader.m_offsetsTable);
			gfxc->setPsShader(logoSrtPixelShader.m_shader, &logoSrtPixelShader.m_offsetsTable);

			SRT srtBufferLogo;
			srtBufferLogo.samplers = (SamplersInSRT *)samplers;
			srtBufferLogo.textures = (TexturesInSRT *)textures;
			srtBufferLogo.constants = (ConstantsInSRT *)frame->m_iceLogoConstants;
			gfxc->setUserSrtBuffer(Gnm::kShaderStageVs, &srtBufferLogo, sizeof(srtBufferLogo) / sizeof(uint32_t));
			gfxc->setUserSrtBuffer(Gnm::kShaderStagePs, &srtBufferLogo, sizeof(srtBufferLogo) / sizeof(uint32_t));
		}

		shadowCasterMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		gfxc->setPrimitiveType(shadowCasterMesh.m_primitiveType);
		gfxc->setIndexSize(shadowCasterMesh.m_indexType);
		gfxc->drawIndex(shadowCasterMesh.m_indexCount, shadowCasterMesh.m_indexBuffer);

		frame->m_quadConstants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*quadMatrix);
		frame->m_quadConstants->m_lightPosition = framework.getLightPositionInViewSpace();
		frame->m_quadConstants->m_shadow_mvp = transpose(shadowMapViewProjection * quadMatrix);
		frame->m_quadConstants->m_modelView = transpose(framework.m_worldToViewMatrix*quadMatrix);
		frame->m_quadConstants->m_lightColor = framework.getLightColor();
		frame->m_quadConstants->m_ambientColor = framework.getAmbientColor();
		frame->m_quadConstants->m_lightAttenuation = Vector4(1, 0, 0, 0);

		if (!g_runtimeOptions.UseSrt())
		{
			gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
			gfxc->setPsShader(planePixelShader.m_shader, &planePixelShader.m_offsetsTable);

#if SCE_GNMX_ENABLE_GFX_LCUE
			gfxc->setTextures(Gnm::kShaderStagePs, 0, 3, textures);
			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 3, samplers);
#endif // SCE_GNMX_ENABLE_GFX_LCUE

			Gnm::Buffer quadConstantBuffer;
			quadConstantBuffer.initAsConstantBuffer(frame->m_quadConstants, sizeof(*frame->m_quadConstants));
			quadConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &quadConstantBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &quadConstantBuffer);
		}
		else
		{
			gfxc->setVsShader(srtVertexShader.m_shader, 0, srtVertexShader.m_fetchShader, &srtVertexShader.m_offsetsTable);
			gfxc->setPsShader(planeSrtPixelShader.m_shader, &planeSrtPixelShader.m_offsetsTable);

			SRT srtBufferQuad;
			srtBufferQuad.samplers = (SamplersInSRT *)samplers;
			srtBufferQuad.textures = (TexturesInSRT *)textures;
			srtBufferQuad.constants = (ConstantsInSRT *)frame->m_quadConstants;
			gfxc->setUserSrtBuffer(Gnm::kShaderStageVs, &srtBufferQuad, sizeof(srtBufferQuad) / sizeof(uint32_t));
			gfxc->setUserSrtBuffer(Gnm::kShaderStagePs, &srtBufferQuad, sizeof(srtBufferQuad) / sizeof(uint32_t));
		}

		quadMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);
		gfxc->setPrimitiveType(quadMesh.m_primitiveType);
		gfxc->setIndexSize(quadMesh.m_indexType);
		gfxc->drawIndex(quadMesh.m_indexCount, quadMesh.m_indexBuffer);

		if (Gnm::getGpuMode() == Gnm::kGpuModeNeo)
		{
			// Decompress and disable DCC for menu rendering
			Gnmx::decompressDccSurface(gfxc, &bufferCpuIsWriting->m_renderTarget);
			bufferCpuIsWriting->m_renderTarget.setDccCompressionEnable(false);
		}

		framework.EndFrame(*gfxc);

		shadowClearMs = timers.readTimerInMilliseconds(kShadowTimer);
		depthColorClearMs = timers.readTimerInMilliseconds(kDepthColorClearTimer);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
	return 0;
}
