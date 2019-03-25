/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/noise_prep.h"
#include "../framework/frame.h"
#include "std_cbuffer.h"
#include <vector>
using namespace sce;

namespace
{
	Framework::GnmSampleFramework framework;

	enum Strategy
	{
		kNormalMap,
		kDerivativeMap,
		kHeightMap,
		kBiasedHeightMap,
		k3TapHeightMap,
		kBicubicHeightMap,
		kStrategyCount,
	};
	Strategy strategy = kNormalMap;

	bool colorSpecularIsEnabled = true;
	bool bumpEffectIsEnabled = true;
	float addToScale = 0;
	bool microBumpIsEnabled = true;
	bool lowPrecisionDerivNormal = true;

	Framework::MenuItemText strategyText[kStrategyCount] = { 
		{"Normal map", "A texel stores (nx, ny)"}, 
		{"Derivative map", "A texel stores (du/dh, dv/dh)"}, 
		{"Height map", "A texel stores (h)"}, 
		{"Biased height map", "A texel stores (h)"}, 
		{"3-tap height map", "Uses forward differencing"}, 
		{"Bicubic height map", "Less mag filter artifacts"}, 
	};

	class MenuCommandStrategy : public Framework::MenuCommandEnum
	{
	public:
		MenuCommandStrategy()
		: Framework::MenuCommandEnum(strategyText, sizeof(strategyText)/sizeof(strategyText[0]), &strategy)
		{
		}
		virtual bool isEnabled() const
		{
			return bumpEffectIsEnabled;
		}
	};

	class MenuCommandBumpEffect : public Framework::MenuCommandBool
	{
	public:
		MenuCommandBumpEffect()
		: Framework::MenuCommandBool(&bumpEffectIsEnabled)
		{
		}
		virtual void printValue(DbgFont::Window *window)
		{
			MenuCommandBool::printValue(window);
			if(bumpEffectIsEnabled)
			{
				window->printf( " (tangent %s", strategy == kNormalMap ? "used" : "none");
				window->printf( ", %s)", strategy < kHeightMap ? (lowPrecisionDerivNormal ? "DXT1" : "BC5") : "BC4");
			}
		}
	};

	MenuCommandStrategy menuStrategy;
	Framework::MenuCommandBool menuColorSpecIsEnabled(&colorSpecularIsEnabled);
	MenuCommandBumpEffect menuBumpEffectIsEnabled;
	Framework::MenuCommandBool menuMicroBumpIsEnabled(&microBumpIsEnabled);
	Framework::MenuCommandBool menuLowPrecisionDerivNormal(&lowPrecisionDerivNormal);
	const Framework::MenuItem menuItem[] =
	{	
		{{"Shader", "Select among several per-pixel lighting techniques"}, &menuStrategy},
		{{"Color spec", "Enable or disable per-pixel albedo and specular"}, &menuColorSpecIsEnabled},
		{{"Bump effect", "Enable or disable per-pixel normal perturbation"}, &menuBumpEffectIsEnabled},
		{{"Toggle format (BC5/DXT1)", "Select between old DXT formats and new BC formats"}, &menuLowPrecisionDerivNormal},
		{{"Micro-Bump", "A detail map is used for per-pixel normal perturbation"}, &menuMicroBumpIsEnabled},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.initialize( "Alternatives to NMap", 
		"Per-pixel lighting on next-gen requires no per-vertex tangent space or per-pixel tangent space normal.", 
		"This sample program displays per-pixel lighting with height-maps and derivative-maps, "
		"which do not require a per-vertex tangent space as normal-maps do. Height-maps "
		"may be stored in 4bpp BC4 format, at higher fidelity than a normal-map in 4bpp DXT1 format.");

	framework.appendMenuItems(kMenuItems, menuItem);

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		cbMeshInstance *m_constants;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Command Buffer",buffer);
	}

	Framework::InitNoise(&framework.m_allocators);

	Gnm::Texture normalTexture, derivativeTexture, heightTexture, colorSpecularTexture, dummyTexture, detailTexture, DXT1normalTexture, DXT1derivativeTexture;

	struct Texture { Gnm::Texture* m_texture; const char* m_filename; };
	Texture texture[] =
	{
		{&normalTexture, "crown/crown_normalBC5"},
		{&derivativeTexture, "crown/crown_derivBC5"},
		{&heightTexture, "crown/heightBC4"},
		{&colorSpecularTexture, "crown/diffspecBC3"},
		{&dummyTexture, "dummy"},
		{&detailTexture, "detail_map"},
		{&DXT1normalTexture, "crown/crown_normalDXT1"},
		{&DXT1derivativeTexture, "crown/crown_derivDXT1"},
	};
	enum {kTextures = sizeof(texture) / sizeof(texture[0])};

	int error = 0;

	for(uint32_t i=0; i<kTextures; ++i)
	{
		char temp[256];
		strncpy(temp, "/app0/assets/", 64);
		strncat(temp, texture[i].m_filename, 128);
		strncat(temp, ".gnf", 8);
		error = Framework::loadTextureFromGnf(texture[i].m_texture, Framework::ReadFile(temp), 0, &framework.m_allocators);
		SCE_GNM_ASSERT(error == Framework::kGnfErrorNone);
		texture[i].m_texture->setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture will never be bound as an RWTexture, so it's safe to declare it as read-only.
	}

	Gnm::Sampler trilinearSampler;
	trilinearSampler.init();
	trilinearSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	trilinearSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	// assume all incarnations of the bump map (deriv/height/normal) have the same resolution
	const int bumpTextureWidth = derivativeTexture.getWidth();
	const int bumpTextureHeight = derivativeTexture.getHeight();

	Framework::VsShader tangentSpaceVertexShader, noTangentSpaceVertexShader;
	Framework::PsShader noBumpPixelShader;

	error = Framework::LoadVsMakeFetchShader(&tangentSpaceVertexShader, Framework::ReadFile("/app0/alternatives-to-nmap-sample/shader_tang_vv.sb"), &framework.m_allocators);
	error = Framework::LoadVsMakeFetchShader(&noTangentSpaceVertexShader, Framework::ReadFile("/app0/alternatives-to-nmap-sample/shader_notang_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&noBumpPixelShader, Framework::ReadFile("/app0/alternatives-to-nmap-sample/shader_nobump_p.sb"), &framework.m_allocators);

	const char pixelShaderName[][128] =
	{
		"tang", "deriv", "1tapstd", "1tapbias", "3tap", "bicubic",
		"tang_micro", "deriv_micro", "1tapstd_micro", "1tapbias_micro", "3tap_micro", "bicubic_micro",
	};
	enum {kPixelShaders = sizeof(pixelShaderName)/sizeof(pixelShaderName[0])};
	Framework::PsShader pixelShaders[kPixelShaders];
	for(uint32_t i = 0; i < kPixelShaders; ++i)
	{
		char filename[256];
		strncpy(filename, "/app0/alternatives-to-nmap-sample/shader_", 128);
		strncat(filename, pixelShaderName[i], 64);
		strncat(filename, "_p.sb", 8);
		error = Framework::LoadPsShader(&pixelShaders[i], Framework::ReadFile(filename), &framework.m_allocators);
	}

	Framework::SimpleMesh crownMesh;
	LoadSimpleMesh(&framework.m_allocators, &crownMesh, "/app0/assets/crown/crown.mesh");

	float materialSpecificBumpScale =  1.0f/sqrtf((float) (bumpTextureWidth*bumpTextureHeight));
	float meshSpecificBumpScale = Framework::ComputeMeshSpecificBumpScale(&crownMesh);

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = &frames[framework.getIndexOfBufferCpuIsWriting()];
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		const bool bIsNMapping = strategy == kNormalMap && bumpEffectIsEnabled;
		int strategyOffset = microBumpIsEnabled ? 6 : 0;

		Framework::PsShader *pixelShader = bumpEffectIsEnabled ? &pixelShaders[strategy + strategyOffset] : &noBumpPixelShader;
		Framework::VsShader *vertexShader = bIsNMapping ? &tangentSpaceVertexShader : &noTangentSpaceVertexShader;

		{
			Framework::Marker marker(*gfxc, "set default registers");
			Gnm::PrimitiveSetup primSetupReg;
			primSetupReg.init();
			primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
			primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
			gfxc->setPrimitiveSetup(primSetupReg);
		}

		{
			Framework::Marker marker(*gfxc, "clear screen");
			Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
			Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
			gfxc->setRenderTargetMask(0xF);

			gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
			gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

			Gnm::DepthStencilControl dsc;
			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
			dsc.setDepthEnable(true);
			gfxc->setDepthStencilControl(dsc);
			gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);
		}

		{
			Framework::Marker marker(*gfxc, "prepare shader inputs");

			gfxc->setVsShader(vertexShader->m_shader, 0, vertexShader->m_fetchShader, &vertexShader->m_offsetsTable);
			gfxc->setPsShader(pixelShader->m_shader, &pixelShader->m_offsetsTable);

			crownMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

			cbMeshInstance * constants = frame->m_constants;
			Gnm::Buffer isoConstBuffer;
			isoConstBuffer.initAsConstantBuffer(constants, sizeof(*constants) );
			isoConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &isoConstBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &isoConstBuffer);

			Framework::SetNoiseDataBuffers(*gfxc, Gnm::kShaderStagePs);

			const Matrix4 cameraToWorld = framework.m_viewToWorldMatrix;

			const float radians = framework.GetSecondsElapsedApparent() * 0.5f;

			Matrix4 localToWorld;
			localToWorld = Matrix4::translation(Vector3(0.f, -0.1f, 1.f)); // set center of rotation somewhat inside the crown
			localToWorld = Matrix4::rotationZYX(Vector3(radians,radians,0.f)) * localToWorld; // rotate around the Y axis
			localToWorld = Matrix4::translation(Vector3(0.f, 0.f, -5.5f)) * localToWorld; // and set the crown away from the camera

			const Matrix4 worldToCamera = inverse(cameraToWorld);
			const Matrix4 localToCamera = worldToCamera * localToWorld;
			const Matrix4 mvp = framework.m_projectionMatrix * localToCamera;

			const Vector3 lightPositionWorld(30,30,50);
			const Vector3 light2PositionWorld(50,50,-60);

			constants->g_vLightPos = worldToCamera * lightPositionWorld;
			constants->g_vLightPos2 = worldToCamera * light2PositionWorld;
			constants->g_mWorldView = transpose(localToCamera);
			constants->g_mWorldViewProjection = transpose(mvp);
			constants->g_fMeshScale = 33.0f + addToScale;	// applied in vertex shader

			// automatically calculate bump scale
			float finalBumpScale = constants->g_fMeshScale * meshSpecificBumpScale * materialSpecificBumpScale;
			if(strategy != kDerivativeMap)
				constants->g_fBumpScale = finalBumpScale * 32;			// 32 adapts the height map to the normalized derivative map [-1;1]
			else
				constants->g_fBumpScale = finalBumpScale;

			if(colorSpecularIsEnabled)
				gfxc->setTextures(Gnm::kShaderStagePs, 3, 1, &colorSpecularTexture);
			else
				gfxc->setTextures(Gnm::kShaderStagePs, 3, 1, &dummyTexture);

			gfxc->setSamplers(Gnm::kShaderStagePs, 2, 1, &trilinearSampler);

			switch(strategy)
			{
			case kNormalMap:
				gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, lowPrecisionDerivNormal ? &DXT1normalTexture : &normalTexture);
				break;
			case kDerivativeMap:
				gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, lowPrecisionDerivNormal ? &DXT1derivativeTexture : &derivativeTexture);
				break;
			default:
				gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &heightTexture);
			}

			gfxc->setTextures(Gnm::kShaderStagePs, 4, 1, &detailTexture);
		}

		{
			Framework::Marker marker(*gfxc, "draw call");
			gfxc->setPrimitiveType(crownMesh.m_primitiveType);
			gfxc->setIndexSize(crownMesh.m_indexType);
			gfxc->drawIndex(crownMesh.m_indexCount, crownMesh.m_indexBuffer);
		}

		framework.EndFrame(*gfxc);
	}

	Frame *frame = &frames[framework.getIndexOfBufferCpuIsWriting()];
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; 
	framework.terminate(*gfxc);
    return 0;
}
