/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/gnf_loader.h"
#include "../framework/noise_prep.h"
#include "../framework/frame.h"
#include "yifplayer/yif2gpu_reader.h"
#include "std_cbuffer.h"
#include "yifplayer/std_cbuffer.h"
using namespace sce;

namespace
{
	int32_t iTessFactor = 16;
	int32_t iNumCopies = 1;
	bool bWireFrameOn = true;
	bool bSimpleShader = true;
	bool bPatchesEnabled = true;
	bool bAnimationEnabled = true;
	bool bComputeSkinningEnabled = false;
	CYif2GpuReader* g_scene = 0;
	Framework::GnmSampleFramework framework;
	void *globalResourceTablePtr;

	class MenuCommandHSVert : public Framework::MenuCommand
	{
	public:
		MenuCommandHSVert()
		{
		}
		virtual void adjust(int) const
		{
			framework.StallUntilGpuIsIdle();
			g_scene->ToggleReadVertsInHS();
		}
		virtual void printValue(DbgFont::Window *window)
		{
			const bool bIsVertsHS = g_scene->IsModeReadVertsInHS();
			window->printf( bIsVertsHS ? "true" : "false" );
		}
	};

	class MenuCommandDSTang : public Framework::MenuCommand
	{
	public:
		MenuCommandDSTang()
		{
		}
		virtual void adjust(int) const
		{
			framework.StallUntilGpuIsIdle();
			g_scene->ToggleTangInDS();
		}
		virtual void printValue(DbgFont::Window *window)
		{
			const bool bIsGenTangDS = g_scene->IsModeGenTangsInDS();
			window->printf( bIsGenTangDS ? "true" : "false" );
		}
		virtual bool isDocumented() const {return true;}
		virtual bool isEnabled() const
		{
			const bool bIsGregory = g_scene->IsPatchModeGregory();
			return !bIsGregory;
		}
	};

	class MenuCommandNrPatches : public Framework::MenuCommand
	{
	public:
		MenuCommandNrPatches()
		{
		}
		virtual void adjust(int delta) const
		{
			switch( delta )
			{
			case 1:
				framework.StallUntilGpuIsIdle();
				g_scene->IncreaseNrPatchesOffset();
				break;
			case -1:
				framework.StallUntilGpuIsIdle();
				g_scene->DecreaseNrPatchesOffset();
				break;
			}
		}
		virtual void printValue(DbgFont::Window *window)
		{
			window->printf("regular %d irregular %d", g_scene->GetNrRegularPatches(), g_scene->GetNrIrregularPatches());
		}
	};

	class MenuCommandNrThreads : public Framework::MenuCommand
	{
	public:
		MenuCommandNrThreads()
		{
		}
		virtual void adjust(int) const
		{
			framework.StallUntilGpuIsIdle();
			g_scene->ToggleNrThreadsPerRegularPatch();
		}
		virtual void printValue(DbgFont::Window *window)
		{
			window->printf( "%d", g_scene->GetNrThreadsPerRegularPatch() );
		}
	};

	class MenuCommandPatchesMethod : public Framework::MenuCommand
	{
	public:
		Framework::GnmSampleFramework *m_gnmSampleFramework;
		MenuCommandPatchesMethod(Framework::GnmSampleFramework *gnmSampleFramework)
		: m_gnmSampleFramework(gnmSampleFramework)
		{
		}
		virtual void adjust(int) const
		{
			m_gnmSampleFramework->StallUntilGpuIsIdle();
			g_scene->TogglePatchesMethod();
		}
		virtual void printValue(DbgFont::Window *window)
		{
			const bool bIsGregory = g_scene->IsPatchModeGregory();
			window->printf( bIsGregory ? "Greg" : "ACC" );
		}
	};

	class MenuCommandPatches : public Framework::MenuCommandBool
	{
	public:
		MenuCommandPatches()
		: MenuCommandBool(&bPatchesEnabled)
		{
		}
		virtual void adjust(int delta) const
		{
			MenuCommandBool::adjust(delta);
			framework.StallUntilGpuIsIdle();
			g_scene->SetTessSupported(bPatchesEnabled);
		}
	};

	class MenuCommandAnimation : public Framework::MenuCommandBool
	{
	public:
		MenuCommandAnimation()
		: MenuCommandBool(&bAnimationEnabled)
		{
		}
		virtual void adjust(int delta) const
		{
			MenuCommandBool::adjust(delta);
			framework.StallUntilGpuIsIdle();
			g_scene->SetAnimationSupported(bAnimationEnabled);
		}
	};

	class MenuCommandComputeSkinning : public Framework::MenuCommandBool
	{
	public:
		MenuCommandComputeSkinning()
		: MenuCommandBool(&bComputeSkinningEnabled)
		{
		}
		virtual void adjust(int delta) const
		{
			MenuCommandBool::adjust(delta);
			framework.StallUntilGpuIsIdle();
			g_scene->ToggleMode();
		}
	};	

	enum {kMaxCopies = 20};

	Framework::MenuCommandInt tessFactor(&iTessFactor, 1, 16);
	MenuCommandHSVert HSVert;
	MenuCommandDSTang DSTang;
	MenuCommandNrPatches NRPatches;
	MenuCommandNrThreads NRThreads;
	Framework::MenuCommandBool wireframe(&bWireFrameOn);
	MenuCommandPatchesMethod PatchesMethod(&framework);
	Framework::MenuCommandBool simpleShader(&bSimpleShader);
	MenuCommandPatches Patches;
	MenuCommandAnimation Animation;
	MenuCommandComputeSkinning ComputeSkinning;
	Framework::MenuCommandInt numReplicas(&iNumCopies, 1, kMaxCopies);
	const Framework::MenuItem menuItem[] =
	{
		{{"Tess factor", "Increases/decreases tessellation factors"}, &tessFactor },		// increases/decreases tessellation factors
		{{"HS Vert", "Enables/disables reading of vertices in the hull shader. If enabled, reduces LDS consumption but serializes fetching of vertices within the hull shader"}, &HSVert},											// enables/disables reading of vertices in the hull shader. If enabled, reduces LDS consumption
																												// but serializes fetching of vertices within the hull shader.

		{{"DS Tang", "Toggles between ACC tangent patches being generated in domain or hull shader. Reduces LDS consumption when built in the domain shader, but generates a longer domain shader"}, &DSTang},			// toggles between ACC tangent patches being generated in domain or hull shader.
																												// reduces LDS consumption when built in the domain shader, but generates a longer domain shader.

		{{"Number of patches", "Increases/decreases the number of patches per HS threadgroup"}, &NRPatches},				// increases/decreases the number of patches per HS threadgroup.
		{{"Threads per reg. patch", "Toggles between 4 and 16 threads per regular patch in the hull shader"}, &NRThreads},  // toggles between 4 and 16 threads per regular patch in the hull shader.
		{{"Wireframe", "Toggles wireframe on/off"}, &wireframe },									// toggles wireframe on/off
		{{"Patches Method", "Toggles between the methods 'Gregory patches' and 'Approximate Catmull-Clark' (ACC)"}, &PatchesMethod},						// toggles between the methods "Gregory patches" and "Approximate Catmull-Clark" (ACC)
		{{"Simple Shader", "Switches shader"}, &simpleShader },							// switches shader
		{{"Tessellation", "Switches between patches and basic VS-PS"}, &Patches},							// switches between patches and basic VS-PS
		{{"Animation", "Enable/disable skinning and animation"}, &Animation},							// enable/disable skinning and animation.
		{{"Skinning in compute", "Enable/disable skinning and animation"}, &ComputeSkinning},							// enable/disable skinning and animation.
		{{"Num Replicas", "Increases/decreases the number of copies"}, &numReplicas},		// increases/decreases the number of copies
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
}

int main(int argc, const char *argv[])
{
#if SCE_GNMX_ENABLE_GFX_LCUE
	// this sample does not support the LCUE
	SCE_GNM_ASSERT_MSG(0, "patches-sample does not support the LCUE, please recompile and run with SCE_GNMX_ENABLE_GFX_LCUE set to 0");
	SCE_GNM_UNUSED(argc); SCE_GNM_UNUSED(argv); SCE_GNM_UNUSED(menuItem);  SCE_GNM_UNUSED(globalResourceTablePtr);
	return 0;
#else
	framework.processCommandLine(argc, argv);

	framework.initialize( "Patches", 
		"Sample code to explore the many parameters that affect the performance of tessellation hardware.",
		"This sample program exposes many parameters which can affect the performance of tessellation hardware, "
		"and enables the user to play with them in order to establish a greater understanding of tessellation performance.");

	framework.appendMenuItems(kMenuItems, menuItem);

	void *tfBufferPtr = sce::Gnmx::Toolkit::allocateTessellationFactorRingBuffer();
#if !SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	globalResourceTablePtr = framework.m_garlicAllocator.allocate(SCE_GNM_SHADER_GLOBAL_TABLE_SIZE, Gnm::kAlignmentOfBufferInBytes);
#endif //!SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		cbSharedConstants *m_constants;
		cbExtraInst *m_extraInst; //[kMaxCopies];
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Constant Buffer",buffer);
		framework.m_allocators.allocate((void**)&frame->m_extraInst,SCE_KERNEL_WB_ONION,sizeof(*frame->m_extraInst) * kMaxCopies,4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d ExtraInst",buffer);
#if !SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
		frame->m_commandBuffer.setGlobalResourceTableAddr(globalResourceTablePtr);
#endif //!SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	}

	enum {kCommandBufferSizeInBytes = 1 * 1024 * 1024};

	Framework::InitNoise(&framework.m_allocators);

	int error = 0;
	Gnm::Texture heightTexture, ambientOcclusionTexture;
	error = Framework::loadTextureFromGnf(&heightTexture, Framework::ReadFile("/app0/assets/iso744_heightmap.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&ambientOcclusionTexture, Framework::ReadFile("/app0/assets/iso744_ao.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	heightTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture will never be bound as an RWTexture, so it's safe to declare it as read-only.
	ambientOcclusionTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture will never be bound as an RWTexture, so it's safe to declare it as read-only.

	Gnm::Sampler trilinearSampler;
	trilinearSampler.init();
	trilinearSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	trilinearSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	Framework::PsShader pixelShader, simplePixelShader;

	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/patches-sample/shader_lit_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&simplePixelShader, Framework::ReadFile("/app0/patches-sample/shader_p.sb"), &framework.m_allocators);

	CYif2GpuReader scene;
	g_scene = &scene;
	scene.ReadScene(&framework.m_allocators, "/app0/assets/iso744_ctrl_skinned.yif");
		
	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);
	
		gfxc->setTessellationFactorBuffer(tfBufferPtr);

		gfxc->pushMarker("set default registers");

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		gfxc->popMarker();

		gfxc->pushMarker("clear screen");

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

		gfxc->popMarker();

		if(bSimpleShader)
			gfxc->setPsShader(simplePixelShader.m_shader);
		else
			gfxc->setPsShader(pixelShader.m_shader);

		Framework::SetNoiseDataBuffers(*gfxc, Gnm::kShaderStagePs);

		Matrix4 rot = Matrix4::rotationZYX( Vector3(3.14f/8, 3.14f/9,0) );				

		const Matrix4 m44WorldToCam = framework.m_worldToViewMatrix;

		const Point3 vLpos_world(150,250,-60);
		const Point3 vLpos2_world(30,50,500);
		frame->m_constants->g_vLightPos = m44WorldToCam * rot * vLpos_world;
		frame->m_constants->g_vLightPos2 = m44WorldToCam * rot * vLpos2_world;
		frame->m_constants->g_fMeshScale = GEN_REP_UNI_SCALE(iNumCopies);

		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(frame->m_constants,sizeof(*frame->m_constants));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

		const float fShaderTime = framework.GetSecondsElapsedApparent();
		const float fDuration = 14.0f;	// some number of seconds

		// unfortunately this animation does not loop properly (quick fix)
		float t = fmodf(fShaderTime / fDuration, 2.0f);
		if(t>1) t = 2-t;

		if(bWireFrameOn)
		{
			primSetupReg.setPolygonMode(Gnm::kPrimitiveSetupPolygonModeLine, Gnm::kPrimitiveSetupPolygonModeLine);
			gfxc->setLineWidth(8);
			gfxc->setPrimitiveSetup(primSetupReg);
		}

		gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &heightTexture);
		gfxc->setTextures(Gnm::kShaderStagePs, 3, 1, &ambientOcclusionTexture);
		gfxc->setSamplers(Gnm::kShaderStagePs, 2, 1, &trilinearSampler);

		scene.DrawScene(*gfxc, frame->m_extraInst, iNumCopies);

		if(bWireFrameOn)
		{
			primSetupReg.setPolygonMode(Gnm::kPrimitiveSetupPolygonModeFill, Gnm::kPrimitiveSetupPolygonModeFill);
			gfxc->setPrimitiveSetup(primSetupReg);
		}
	
		framework.EndFrame(*gfxc);

		static float fX=0, fY=0, fZ=0;
		if(bSimpleShader)
		{ 
			fX=0; 
			fY=0; 
			fZ=0; 
		}
		Matrix4 rot2 = Matrix4::rotationZYX( Vector3(fX, fY, fZ) );
		rot2 *= Matrix4::scale(Vector3(1.25f));
		scene.UpdateTransformationHierarchy(t, m44WorldToCam * rot2, framework.m_projectionMatrix);
		fX = fShaderTime*0.2f; fY = -fShaderTime*0.4f; fZ = fShaderTime*0.264f;

		scene.SetGlobalTessFactor(iTessFactor);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);

	sce::Gnmx::Toolkit::deallocateTessellationFactorRingBuffer();
    return 0;
#endif //SCE_GNMX_ENABLE_GFX_LCUE
}
