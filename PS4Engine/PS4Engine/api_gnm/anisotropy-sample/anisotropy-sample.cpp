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
#include <vector>
using namespace sce;

namespace
{
	Framework::GnmSampleFramework framework;

	class RuntimeOptions
	{
	public:
		Gnm::FilterMode m_magFilterMode;
		Gnm::FilterMode m_minFilterMode;
		Gnm::MipFilterMode m_mipFilterMode;
		uint32_t m_samplerModFactor;
		uint32_t m_mipClampRegionSize;
		int32_t m_lodBias;
		int32_t m_lodBiasSec;
		Gnm::AnisotropyRatio m_maxAnisoRatio;
		int32_t m_anisoBias;
		uint32_t m_anisoThreshold;
		RuntimeOptions()
			: m_magFilterMode(Gnm::kFilterModeAnisoBilinear)
			, m_minFilterMode(Gnm::kFilterModeAnisoBilinear)
			, m_mipFilterMode(Gnm::kMipFilterModeLinear)
			, m_samplerModFactor(Gnm::kSamplerModulationFactor1_0000)
			, m_mipClampRegionSize(0)
			, m_lodBias(0)
			, m_lodBiasSec(0)
			, m_maxAnisoRatio(Gnm::kAnisotropyRatio16)
			, m_anisoBias(0)
			, m_anisoThreshold(0)
		{
		}
		Gnm::FilterMode MagFilterMode() const { return m_magFilterMode; }
		Gnm::FilterMode MinFilterMode() const { return m_minFilterMode; }
		Gnm::MipFilterMode MipFilterMode() const { return m_mipFilterMode; }
		Gnm::SamplerModulationFactor SamplerModFactor() const { return (Gnm::SamplerModulationFactor)m_samplerModFactor; }
		uint32_t MipClampRegionSize() const { return m_mipClampRegionSize; }
		int32_t LodBias() const { return m_lodBias; }
		int32_t LodBiasSec() const { return m_lodBiasSec; }
		Gnm::AnisotropyRatio MaxAnisoRatio() const { return (Gnm::AnisotropyRatio)m_maxAnisoRatio; }
		int32_t AnisoBias() const { return m_anisoBias; }
		uint32_t AnisoThreshold() const { return m_anisoThreshold; }
	};
	RuntimeOptions g_runtimeOptions;

	Framework::MenuItemText maxAnisoRatioText[] = {
		{"AnisotropyRatio1",         "Max ratio of 1:1"},
		{"AnisotropyRatio2",         "Max ratio of 2:1"},
		{"AnisotropyRatio4",         "Max ratio of 4:1"},
		{"AnisotropyRatio8",         "Max ratio of 8:1"},
		{"AnisotropyRatio16",        "Max ratio of 16:1"},
	};
	Framework::MenuItemText filterModeText[] = {
		{"Point",					"Sample a single texel"},
		{"Bilinear",				"Sample mutiple texels and interpolate"},
		{"AnisoPoint",				"Sample multiple texels in an anisotropic coverage pattern"},
		{"AnisoBilinear",			"Sample multiple texels in an anisotropic coverage pattern"},
	};
	Framework::MenuItemText mipFilterModeText[] = {
		{"None",					"Always sample base mip"},
		{"Point",					"Sample from one mip exclusively"},
		{"Linear",					"Sample from neighboring mips and interpolate"},
	};

	class MenuCommandIntFixedPoint : public Framework::MenuCommandInt
	{
	protected:
		int32_t m_shiftAmount;
		int32_t m_signMask;
		int32_t m_valueMask;
		uint8_t m_reserved[4];
	public:
		MenuCommandIntFixedPoint(int32_t* target, int32_t integerBits, int32_t fractionalBits, int32_t min=0, int32_t max=0)
			: MenuCommandInt(target, min, max)
			, m_shiftAmount(fractionalBits)
			, m_signMask(1 << (integerBits+fractionalBits))
			, m_valueMask(m_signMask-1)
		{
		}
		virtual void printValue(sce::DbgFont::Window *window)
		{
			int32_t value = *m_target & m_valueMask;
			if (*m_target & m_signMask)
			{
				value |= ~m_valueMask;
			}
			float asFloat = (float)(value) / (float)(1<<m_shiftAmount);
			window->printf("%.4f [0x%08X]", *m_target, asFloat);
		}
	};

	Framework::MenuCommandEnum menuMagFilterMode(filterModeText, sizeof(filterModeText)/sizeof(filterModeText[0]), &g_runtimeOptions.m_magFilterMode);
	Framework::MenuCommandEnum menuMinFilterMode(filterModeText, sizeof(filterModeText)/sizeof(filterModeText[0]), &g_runtimeOptions.m_minFilterMode);
	Framework::MenuCommandEnum menuMipFilterMode(mipFilterModeText, sizeof(mipFilterModeText)/sizeof(mipFilterModeText[0]), &g_runtimeOptions.m_mipFilterMode);
	Framework::MenuCommandUint menuSamplerModFactor(&g_runtimeOptions.m_samplerModFactor, 0, 7);
	Framework::MenuCommandUint menuMipClampRegionSize(&g_runtimeOptions.m_mipClampRegionSize, 0, 15);
	MenuCommandIntFixedPoint   menuLodBias(&g_runtimeOptions.m_lodBias, 5, 8, 0, 16*1024-1);
	MenuCommandIntFixedPoint   menuLodBiasSec(&g_runtimeOptions.m_lodBiasSec, 1, 4, 0, 63);
	Framework::MenuCommandEnum menuMaxAnisoRatio(maxAnisoRatioText, sizeof(maxAnisoRatioText)/sizeof(maxAnisoRatioText[0]), &g_runtimeOptions.m_maxAnisoRatio);
	MenuCommandIntFixedPoint   menuAnisoBias(&g_runtimeOptions.m_anisoBias, 1, 5, 0, 63);
	Framework::MenuCommandUint menuAnisoThreshold(&g_runtimeOptions.m_anisoThreshold, 0, 7);
	const Framework::MenuItem menuItem[] =
	{
		{{"T# SamplerModFactor", "Scales S#'s MipClampRegionSize, ZClampRegionSize, AnisoBias, LodBiasSec, AnisoThreshold"}, &menuSamplerModFactor},
		{{"S# MagFilterMode", "Controls magnification filtering"}, &menuMagFilterMode},
		{{"S# MinFilterMode", "Controls minification filtering"}, &menuMinFilterMode},
		{{"S# MipFilterMode", "Controls filtering across mip levels"}, &menuMipFilterMode},
		{{"S# MipClampRegionSize", "Remaps fractional bits of LOD level (scaled by SamplerModFactor)"}, &menuMipClampRegionSize},
		{{"S# LodBias", "Biases mip level (S5.8)"}, &menuLodBias},
		{{"S# LodBiasSec", "Biases mip level (S1.4, added to LodBias, scaled by SamplerModFactor)"}, &menuLodBiasSec},
		{{"S# AnisoRatio", "Log2 of maximum anisotropic filtering ratio"}, &menuMaxAnisoRatio},
		{{"S# AnisoBias", "Biases anisotropic filtering ratio (U1.5, scaled by SamplerModFactor)"}, &menuAnisoBias},
		{{"S# AnisoThreshold", "Anisotropic filtering quality/speed tradeoff (scaled by SamplerModFactor)"}, &menuAnisoThreshold},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.m_config.m_lightingIsMeaningful = false;

	framework.initialize( "Anisotropy", 
		"Sample code to demonstrate anisotropic texture filtering.",
		"This sample program displays a plane and allows the adjustment of texture/sampler properties related to filtering.");
	framework.appendMenuItems(kMenuItems, menuItem);

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
		createCommandBuffer(&frame->m_commandBuffer, &framework, buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Command Buffer", buffer);
	}

	int error;
	
	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader;
	Gnm::Texture textures[2];

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/anisotropy-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/anisotropy-sample/shader_p.sb"), &framework.m_allocators);
	error = Framework::loadTextureFromGnf(&textures[0], Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&textures[1], Framework::ReadFile("/app0/assets/icelogo-normal.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's OK to mark it as read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's OK to mark it as read-only.

	Gnm::Sampler sampler;
	sampler.init();

	Framework::SimpleMesh quadMesh;
	BuildQuadMesh(&framework.m_allocators, "Quad", &quadMesh, 100);

	//framework.SetDepthNear(0.0001f);
	const float depthNear = 0.001f;
	const float depthFar = 100.0f;
	const float aspect = (float)framework.m_config.m_targetWidth / (float)framework.m_config.m_targetHeight;
	framework.m_projectionMatrix = Matrix4::perspective( 1.57f, aspect, depthNear, depthFar );
	//framework.SetProjectionMatrix( Matrix4::frustum( -aspect, aspect, -1, 1, depthNear, depthFar ) );

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = &frames[framework.getIndexOfBufferCpuIsWriting()];
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		for(uint32_t iTex=0; iTex<2; ++iTex)
		{
			textures[iTex].setSamplerModulationFactor((Gnm::SamplerModulationFactor)g_runtimeOptions.m_samplerModFactor);
		}
		sampler.setMipClampRegionSize(g_runtimeOptions.m_mipClampRegionSize);
		sampler.setLodBias(g_runtimeOptions.m_lodBias, g_runtimeOptions.m_lodBiasSec);
		sampler.setXyFilterMode(g_runtimeOptions.m_magFilterMode, g_runtimeOptions.m_minFilterMode);
		sampler.setMipFilterMode(g_runtimeOptions.m_mipFilterMode);
		sampler.setAnisotropyRatio(g_runtimeOptions.m_maxAnisoRatio);
		sampler.setAnisotropyBias(g_runtimeOptions.m_anisoBias);
		sampler.setAnisotropyThreshold(g_runtimeOptions.m_anisoThreshold);

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		// Render the scene:
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		// Clear
		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
		Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
		gfxc->setRenderTargetMask(0xF);
		gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
		gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
		gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLess);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);
		gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);

		gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
		gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);
		quadMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		const Matrix4 quadMatrix = Matrix4::translation(Vector3(0,-1,0)) * Matrix4::rotationX(-1.57f);
		Constants* quadConstants = frame->m_constants;
		Gnm::Buffer quadConstantBuffer;
		quadConstantBuffer.initAsConstantBuffer(quadConstants, sizeof(Constants));
		quadConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &quadConstantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &quadConstantBuffer);

		Matrix4 viewToWorld = framework.m_viewToWorldMatrix;
		Vector4 col3 = viewToWorld.getCol3();
		if (col3.getY().getAsFloat() < -0.99f)
		{
			col3.setY(-0.99f);
			viewToWorld.setCol3(col3);
			framework.SetViewToWorldMatrix(viewToWorld);
		}

		Vector2 uvOffset = framework.GetSecondsElapsedApparent()*Vector2(0.06f, 0.06f);
		quadConstants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*quadMatrix);
		quadConstants->m_lightPosition = framework.getLightPositionInViewSpace();
		quadConstants->m_modelView = transpose(framework.m_worldToViewMatrix*quadMatrix);
		quadConstants->m_lightColor = framework.getLightColor();
		quadConstants->m_lightAttenuation = Vector4(1, 0, 0, 0);
		quadConstants->m_uvOffset = uvOffset;

		quadMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);
		
		gfxc->setPrimitiveType(quadMesh.m_primitiveType);
		gfxc->setIndexSize(quadMesh.m_indexType);
		gfxc->drawIndex(quadMesh.m_indexCount, quadMesh.m_indexBuffer);
		framework.EndFrame(*gfxc);
	}
	
	Frame *frame = &frames[framework.getIndexOfBufferCpuIsWriting()];
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
}
