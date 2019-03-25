/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/frame.h"
using namespace sce;

namespace
{
	Framework::GnmSampleFramework framework;

	uint32_t g_displayableBuffer = 0;
	uint32_t g_lightsLog10 = 2;
	uint32_t g_meshesLog10 = 2;

	const uint32_t g_powersOf10[4] =
	{
		1,
		10,
		100,
		1000
	};
	enum {kMaximumMeshes = 1000};

	class DepthNormalPassConstants // these constants are used to render out the per-pixel view space normal and depth buffers, which become inputs to the full screen compute shader.
	{
	public:
		Matrix4Unaligned m_localToScreen;
		Matrix4Unaligned m_localToView;
		float m_specularPower;
	};

	typedef DepthNormalPassConstants MainPassConstants;

	class LightingConstants // these constants are used by the full screen compute shader to project depth samples back into view space, and count the number of lights in the scene
	{
	public:
		Matrix4Unaligned m_inverseProjection;
		uint32_t m_lights[4];
		uint32_t m_htile[4];
	};

	struct RegularBuffer
	{
		uint32_t m_size;
		uint32_t m_elements;
		sce::Gnm::Buffer m_buffer; 
		void *m_ptr;
		sce::Gnm::SizeAlign m_sizeAlign; 
		void initialize(Gnmx::Toolkit::IAllocator *allocator, uint32_t size, uint32_t elements, Gnm::ResourceMemoryType resourceMemoryType) 
		{
			m_size = size;
			m_elements = elements;
			m_sizeAlign.m_size = m_elements * m_size;
			m_sizeAlign.m_align = (sce::Gnm::AlignmentType)std::max((uint32_t)sce::Gnm::kAlignmentOfBufferInBytes, m_size); 
			m_ptr = allocator->allocate(m_sizeAlign); 
			m_buffer.initAsRegularBuffer(m_ptr, m_size, m_elements); 
			m_buffer.setResourceMemoryType(resourceMemoryType);
		}
	};

	Framework::MenuItemText g_bufferText[4] =
	{
		{"Back", "This is the final output of the light pre-pass deferred rendering pipeline"},	
		{"Normal/spec", "This is a view-space normal and specular power for each pixel, as output by the first rasterization pass of the algorithm"},  
		{"Diffuse", "This is the total of accumulated diffuse light for each pixel, as output by the tile-based deferred lighting compute shader"},	 
		{"Specular", "This is the total of accumulated specular light for each pixel, as output by the tile-based deferred lighting compute shader"},
	};

	void createRenderTargetAndTexture(Gnmx::Toolkit::Allocators *allocators, const char *name, Gnm::RenderTarget* renderTarget, Gnm::Texture* texture, Gnm::DataFormat dataFormat, Gnm::TileMode tileMode, uint32_t width, uint32_t height, Gnm::ResourceMemoryType resourceMemoryType)
	{		
		Gnm::SizeAlign cmaskSizeAlign;
		const Gnm::SizeAlign sizeAlign = Gnmx::Toolkit::init(renderTarget, width, height, 1, dataFormat, tileMode, Gnm::kNumSamples1, Gnm::kNumFragments1, &cmaskSizeAlign, 0);
		void *renderTargetPtr, *cmaskPtr;

		allocators->allocate(&renderTargetPtr, SCE_KERNEL_WC_GARLIC, sizeAlign.m_size, sizeAlign.m_align, Gnm::kResourceTypeRenderTargetBaseAddress, "%s COLOR", name);
		allocators->allocate(&cmaskPtr, SCE_KERNEL_WC_GARLIC, sizeAlign.m_size, sizeAlign.m_align, Gnm::kResourceTypeRenderTargetCMaskAddress, "%s CMASK", name);

		renderTarget->setAddresses(renderTargetPtr, cmaskPtr, 0);
		renderTarget->setFmaskCompressionEnable(false);
		renderTarget->setCmaskFastClearEnable(true);
		renderTarget->setFmaskTileMode(renderTarget->getTileMode());
		renderTarget->setFmaskSliceNumTilesMinus1(renderTarget->getSliceSizeDiv64Minus1());
		renderTarget->setFmaskAddress(renderTarget->getCmaskAddress());

		texture->initFromRenderTarget(renderTarget, false);

		char temp[Framework::kResourceNameLength];
		snprintf(temp, Framework::kResourceNameLength, "%s TEXTURE", name);
		Gnm::registerResource(nullptr, allocators->m_owner, texture->getBaseAddress(), sizeAlign.m_size, temp, Gnm::kResourceTypeTextureBaseAddress, 0);

		texture->setResourceMemoryType(resourceMemoryType);
	}

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Gnm::RenderTarget m_backBufferTarget; // the back buffer - the final output of the program
		Gnm::RenderTarget m_viewNormalSpecularPowerTarget; // stores a normal in view space per pixel
		Gnm::RenderTarget m_diffuseLightTarget; // stores accumulated diffuse light per pixel, as output by compute shader
		Gnm::RenderTarget m_specularLightTarget; // stores accumulated specular light per pixel, as output by compute shader
		Gnm::Texture m_backBufferTexture;
		Gnm::Texture m_viewNormalSpecularPowerTexture;
		Gnm::Texture m_diffuseLightTexture;
		Gnm::Texture m_specularLightTexture;
		Gnm::Texture *m_textures[4];
		struct MultipleMainPassConstants
		{
			MainPassConstants m_mainPassConstants[kMaximumMeshes];
		};
		MultipleMainPassConstants *m_multipleMainPassConstants;
		LightingConstants *m_lightingConstants;
		struct AsynchronousJob
		{
			Gnmx::ComputeContext m_computeContext; // the command buffer to run
			volatile uint64_t *m_label; // this is set to 1 when the job is done
			RegularBuffer m_lightPositionView; // the output
			Matrix4 *m_viewMatrix; // the input
		};
		AsynchronousJob m_asynchronousJob;
	};

	void CalculateConstantsForMesh(Frame *frame, uint32_t index) 
	{		
		Framework::Random random = {index};
		MainPassConstants* constant = &frame->m_multipleMainPassConstants->m_mainPassConstants[index];
		const Vector3 position = Vector3(-0.5f,-0.5f,-0.5f) + Vector3( static_cast<float>((index>>0)%4), static_cast<float>((index>>2)%4), static_cast<float>((index>>4)%4) ) / 3;
		const Matrix4 translation = Matrix4::translation( position );
		const Vector3 axis = random.GetUnitVector3();
		const Matrix4 rotation = Matrix4::rotation( framework.GetSecondsElapsedApparent()*0.1f, axis );
		const Matrix4 rotation2 = Matrix4::rotation( framework.GetSecondsElapsedApparent()*1.f, random.GetUnitVector3() );
		const Matrix4 localToWorld = rotation2 * translation * rotation;
		constant->m_localToScreen = transpose(framework.m_viewProjectionMatrix * localToWorld);
		constant->m_localToView = transpose(framework.m_worldToViewMatrix * localToWorld);
		constant->m_specularPower = 5.f + static_cast<float>(random.GetUint() % 8);
	}

	void RenderMesh(Frame *frame, Framework::SimpleMesh* mesh, uint32_t index) // render a single sphere with some random rotation and translation
	{		
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
		MainPassConstants* constant = &frame->m_multipleMainPassConstants->m_mainPassConstants[index];
		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(constant, sizeof(MainPassConstants));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
		gfxc->setPrimitiveType(mesh->m_primitiveType);
		gfxc->setIndexSize(mesh->m_indexType);
		gfxc->drawIndex(mesh->m_indexCount, mesh->m_indexBuffer);
	}

	class MinMax { public: float m_min; float m_max; };
	class MulAdd { public: float m_mul; float m_add; };

	MulAdd MinMaxToMulAdd( MinMax minMax ) // for a desired range [MIN..MAX], compute the values MUL and ADD such that saturate(X*MUL+ADD) is 0 when X is MIN and 1 when X is MAX.
	{
		const float delta = minMax.m_max - minMax.m_min;
		MulAdd mulAdd;
		mulAdd.m_mul = 1.f / delta;
		mulAdd.m_add = -minMax.m_min * mulAdd.m_mul;
		return mulAdd;
	};

	Framework::MenuItemText g_log10Text[4] =
	{
		{"1", "One"},
		{"10", "Ten"},
		{"100", "One Hundred"},
		{"1000", "One Thousand"},
	};

	Framework::MenuCommandEnum menuCommandEnumBuffer(g_bufferText, sizeof(g_bufferText)/sizeof(g_bufferText[0]), &g_displayableBuffer);
	Framework::MenuCommandEnum menuCommandEnumLights(g_log10Text, sizeof(g_log10Text)/sizeof(g_log10Text[0]), &g_lightsLog10);
	Framework::MenuCommandEnum menuCommandEnumMeshes(g_log10Text, sizeof(g_log10Text)/sizeof(g_log10Text[0]), &g_meshesLog10);
	const Framework::MenuItem menuItem[] =
	{
		{{"Displaying", 
		"This sample can display the final output of deferred light pre-pass rendering, or any of the intermediate render targets used in the process of creating it. "
		"First, scene geometry is rasterized to [BC 77]Normal/Spec[/], which holds a view-space normal and specular power for each pixel. Then, a full-screen pass "
		"reads, for each pixel, the depth, view-space normal and specular power, accumulates all lights that touch that pixel, and then writes to [BC 77]Diffuse[/] and [BC 77]Specular[/]. "
		"Finally, scene geometry is rasterized once more, and [BC 77]Diffuse[/] and [BC 77]Specular[/] are read as textures in the pixel shader to apply lighting and output [BC 77]Back[/]"
		}, &menuCommandEnumBuffer},
		{{"Lights", "This is the number of point lights in the scene. At the beginning of each frame, the GPU transforms each light into view space, and stores them into a buffer that is used later "
		"for tile-based deferred lighting in a compute shader. Because each light is very small, many may be required to achieve a reasonable lighting effect"}, &menuCommandEnumLights},
		{{"Meshes", "This is the number of meshes in the scene. Little effort was made to optimize for a large number of meshes. Instancing was not implemented"}, &menuCommandEnumMeshes},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.m_config.m_htile = true;

	framework.initialize("Deferred", 
		"Sample code for performing deferred lighting with the benefit of HTILE acceleration.", 
		"This sample program renders a scene into a depth-normal buffer, "
		"and then uses compute to analyze the resulting HTILE buffer, which guides "
		"a process of lighting 64x64 pixel tiles. The resulting diffuse and specular "
		"light buffers are then used when rendering the scene for a second time.");

	framework.appendMenuItems(kMenuItems, menuItem);

	enum { kMaximumLights = 1024 };
	const uint32_t numSlices = 1;
 
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		SCE_GNM_ASSERT((sizeof(frame->m_textures) / sizeof(frame->m_textures[0])) == (sizeof(g_bufferText) / sizeof(g_bufferText[0])));
		frame->m_textures[0] = &frame->m_backBufferTexture;
		frame->m_textures[1] = &frame->m_viewNormalSpecularPowerTexture;
		frame->m_textures[2] = &frame->m_diffuseLightTexture;
		frame->m_textures[3] = &frame->m_specularLightTexture;

		Gnm::DataFormat bbFormat = Gnm::kDataFormatB8G8R8A8UnormSrgb;
		Gnm::DataFormat gbFormat = Gnm::kDataFormatR16G16B16A16Float;
		Gnm::TileMode bbTileMode,gbTileMode;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&bbTileMode,GpuAddress::kSurfaceTypeColorTarget,bbFormat,1);
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&gbTileMode,GpuAddress::kSurfaceTypeColorTarget,gbFormat,1);

		const auto alignment = Gnm::kAlignmentOfBufferInBytes;
		const auto resourceType = Gnm::kResourceTypeConstantBufferBaseAddress;

		framework.m_allocators.allocate((void**)&frame->m_multipleMainPassConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_multipleMainPassConstants),alignment,resourceType,"Buffer %d Main Pass Constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_lightingConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_lightingConstants),alignment,resourceType,"Buffer %d Lighting Constants",buffer);

		char name[Framework::kResourceNameLength];
		snprintf(name,Framework::kResourceNameLength,"Buffer %d Back Buffer",buffer);
		createRenderTargetAndTexture(&framework.m_allocators,name,&frame->m_backBufferTarget,&frame->m_backBufferTexture,bbFormat,bbTileMode,framework.m_config.m_targetWidth,framework.m_config.m_targetHeight,Gnm::kResourceMemoryTypeRO); // texture is never used as RWTexture.
		snprintf(name,Framework::kResourceNameLength,"Buffer %d View Normal Specular",buffer);
		createRenderTargetAndTexture(&framework.m_allocators,name,&frame->m_viewNormalSpecularPowerTarget,&frame->m_viewNormalSpecularPowerTexture,gbFormat,gbTileMode,framework.m_config.m_targetWidth,framework.m_config.m_targetHeight,Gnm::kResourceMemoryTypeRO); // texture is never used as RWTexture.
		snprintf(name,Framework::kResourceNameLength,"Buffer %d Diffuse Light",buffer);
		createRenderTargetAndTexture(&framework.m_allocators,name,&frame->m_diffuseLightTarget,&frame->m_diffuseLightTexture,gbFormat,gbTileMode,framework.m_config.m_targetWidth,framework.m_config.m_targetHeight,Gnm::kResourceMemoryTypeGC); // texture is used as RWTexture, so GPU coherent it is.
		snprintf(name,Framework::kResourceNameLength,"Buffer %d Specular Light",buffer);
		createRenderTargetAndTexture(&framework.m_allocators,name,&frame->m_specularLightTarget,&frame->m_specularLightTexture,gbFormat,gbTileMode,framework.m_config.m_targetWidth,framework.m_config.m_targetHeight,Gnm::kResourceMemoryTypeGC); // texture is used as RWTexture, so GPU coherent it is.
	}

	int error = 0;

	Framework::VsShader depthNormalVertexShader, mainVertexShader;
	Framework::PsShader depthNormalPixelShader, mainPixelShader;
	Framework::CsShader lightingShader, transformLightsShader;
	Framework::SimpleMesh simpleMesh;
	Gnm::Texture albedoTexture;

	error = Framework::LoadVsMakeFetchShader(&depthNormalVertexShader, Framework::ReadFile("/app0/deferred-sample/depth_normal_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&depthNormalPixelShader, Framework::ReadFile("/app0/deferred-sample/depth_normal_p.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&lightingShader, Framework::ReadFile("/app0/deferred-sample/lighting_c.sb"), &framework.m_allocators);
	error = Framework::LoadVsMakeFetchShader(&mainVertexShader, Framework::ReadFile("/app0/deferred-sample/main_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&mainPixelShader, Framework::ReadFile("/app0/deferred-sample/main_p.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&transformLightsShader, Framework::ReadFile("/app0/deferred-sample/transform_lights_c.sb"), &framework.m_allocators);
	error = Framework::loadTextureFromGnf(&albedoTexture, Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone);

	BuildTorusMesh(&framework.m_allocators,"Torus",&simpleMesh,0.8f*0.1f,0.2f*0.1f,32,16,4,1);

	albedoTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it can be marked read-only.

	Gnm::Sampler albedoSampler;
	albedoSampler.init();
	albedoSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);	
	albedoSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	albedoSampler.setAnisotropyRatio(Gnm::kAnisotropyRatio16);

	RegularBuffer lightPositionWorld; // position of all lights in world space
	RegularBuffer lightAttenuation; // MUL and ADD values such that saturate(DISTANCE*MUL+ADD) is 0 where light fades out, and 1 where light is at full strength.
	RegularBuffer lightColor; // 32-bit RGBA color for lights

	lightPositionWorld.initialize(&framework.m_garlicAllocator, sizeof(Vector4), kMaximumLights, Gnm::kResourceMemoryTypeRO); // position of all lights in world space
	lightAttenuation.initialize(&framework.m_garlicAllocator, sizeof(Vector2), kMaximumLights, Gnm::kResourceMemoryTypeRO); // MUL and ADD values such that saturate(DISTANCE*MUL+ADD) is 0 where light fades out, and 1 where light is at full strength.
	lightColor.initialize(&framework.m_garlicAllocator, sizeof(uint32_t), kMaximumLights, Gnm::kResourceMemoryTypeRO); // 32-bit RGBA color for lights

	for( uint32_t i=0; i<kMaximumLights; ++i ) // do initial setup for all lights. place them randomly in world space and give them random colors.
	{
		Framework::Random random = {i};
		const float minimumDistance =                   0.2f;
		const float maximumDistance = minimumDistance + 0.1f;
		((Vector4*)lightPositionWorld.m_ptr)[i] = Vector4(random.GetFloat(-1.f,1.f), random.GetFloat(-1.f,1.f), random.GetFloat(-1.f, 1.f), maximumDistance );
		MinMax minMax;
		minMax.m_max = minimumDistance;
		minMax.m_min = maximumDistance;
		MulAdd mulAdd = MinMaxToMulAdd( minMax );
		((Vector2*)lightAttenuation.m_ptr)[i] = Vector2( mulAdd.m_mul, mulAdd.m_add );
		uint32_t r = random.GetUint( 32, 64 );
		uint32_t g = random.GetUint( 32, 64 );
		uint32_t b = random.GetUint( 32, 64 );
		((uint32_t*)lightColor.m_ptr)[i] = (r<<0) + (g<<8) + (b<<16);
	}

	Framework::ShaderResourceBinding shaderResourceBinding[] =
	{
		{Gnm::kShaderInputUsageImmResource, 0},
		{Gnm::kShaderInputUsageImmRwResource, 0},
		{Gnm::kShaderInputUsageImmConstBuffer, 0},
	};
	Framework::ShaderResourceBindings transformLightsShaderBindings(
		transformLightsShader.m_shader->getInputUsageSlotTable(), 
		transformLightsShader.m_shader->m_common.m_numInputUsageSlots, 
		shaderResourceBinding, 
		sizeof(shaderResourceBinding) / sizeof(shaderResourceBinding[0])
	);
	SCE_GNM_ASSERT(transformLightsShaderBindings.areValid());

	for(uint32_t job = 0; job < framework.m_config.m_buffers; ++job)
	{
		Frame::AsynchronousJob &dest = frames[job].m_asynchronousJob;
		dest.m_lightPositionView.initialize(&framework.m_garlicAllocator, sizeof(Vector4), kMaximumLights, Gnm::kResourceMemoryTypeGC); // position of all lights in view space
		framework.m_allocators.allocate((void**)&dest.m_viewMatrix, SCE_KERNEL_WC_GARLIC, sizeof(Matrix4), 16, Gnm::kResourceTypeConstantBufferBaseAddress, "Buffer %d View Matrix", job);
		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(dest.m_viewMatrix, sizeof(Matrix4));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only sis OK
		framework.m_allocators.allocate((void**)&dest.m_label, SCE_KERNEL_WC_GARLIC, sizeof(uint64_t), sizeof(uint64_t), Gnm::kResourceTypeLabel, "Buffer %d Label", job);
		*dest.m_label = 1;

		Gnmx::ComputeContext &cmpc = dest.m_computeContext;

		void *indirectBuffer;
		framework.m_allocators.allocate(&indirectBuffer, SCE_KERNEL_WB_ONION, Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes, Gnm::kResourceTypeDispatchCommandBufferBaseAddress, "Buffer %d Dispatch Command Buffer", job);
		cmpc.init(indirectBuffer, Gnm::kIndirectBufferMaximumSizeInBytes);
		cmpc.reset();

		cmpc.setCsShader(&transformLightsShader.m_shader->m_csStageRegisters);

		cmpc.setVsharpInUserData(transformLightsShaderBindings.m_binding[0].m_startRegister, &lightPositionWorld.m_buffer); 
		cmpc.setVsharpInUserData(transformLightsShaderBindings.m_binding[1].m_startRegister, &dest.m_lightPositionView.m_buffer); 
		cmpc.setVsharpInUserData(transformLightsShaderBindings.m_binding[2].m_startRegister, &constantBuffer);

		cmpc.m_dcb.dispatch((kMaximumLights + Gnm::kThreadsPerWavefront - 1) / Gnm::kThreadsPerWavefront, 1, 1);

		cmpc.writeReleaseMemEvent(Gnm::kReleaseMemEventCsDone,
								  Gnm::kEventWriteDestMemory, (void*)dest.m_label,
								  Gnm::kEventWriteSource64BitsImmediate, 0x1,
								  Gnm::kCacheActionInvalidateL1, Gnm::kCachePolicyLru);
	}

	const uint32_t memRingSize = 4 * 1024;
	void *memRing, *memReadPtr;
	framework.m_allocators.allocate(&memRing, SCE_KERNEL_WB_ONION, memRingSize, 256, Gnm::kResourceTypeGenericBuffer, "Memory Ring");
	framework.m_allocators.allocate(&memReadPtr, SCE_KERNEL_WB_ONION, 4, 16, Gnm::kResourceTypeGenericBuffer, "Memory Read Pointer");
	memset(memRing, 0, memRingSize);
	Gnmx::ComputeQueue computeQueue;
	computeQueue.initialize(0,3);
	computeQueue.map(memRing, memRingSize/4, memReadPtr);

	while(!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		// at this moment (on the CPU) kick off some work in a compute queue that a compute shader in the graphics queue will read.
		SCE_GNM_ASSERT(*frame->m_asynchronousJob.m_label == 1);
		*frame->m_asynchronousJob.m_label = 0;
		*frame->m_asynchronousJob.m_viewMatrix = framework.m_worldToViewMatrix;
		computeQueue.submit(&frame->m_asynchronousJob.m_computeContext);
		// at this point on the CPU, the compute job on the GPU may have already kicked off.

		Gnm::Texture depthTexture;
		depthTexture.initFromDepthRenderTarget( &bufferCpuIsWriting->m_depthTarget, false );
		depthTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it can be marked read-only.

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace( Gnm::kPrimitiveSetupCullFaceBack );
		primSetupReg.setFrontFace( Gnm::kPrimitiveSetupFrontFaceCcw );
		gfxc->setPrimitiveSetup( primSetupReg );
		Gnm::ClipControl clipReg;
		clipReg.init();
		gfxc->setClipControl( clipReg );

		const uint32_t lights = g_powersOf10[g_lightsLog10];
		const uint32_t meshes = g_powersOf10[g_meshesLog10];

		for( uint32_t i=0; i<meshes; ++i )
		{
			CalculateConstantsForMesh(frame, i);
		}

		// depth/normal pass. here we render out a view space per pixel normal and depth, which later becomes input to the compute shader that does full screen lighting.
		{		
			Gnmx::Toolkit::SurfaceUtil::clearCmaskSurface(*gfxc, &frame->m_viewNormalSpecularPowerTarget);
			Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);

			gfxc->setRenderTarget(0, &frame->m_viewNormalSpecularPowerTarget);
			gfxc->setupScreenViewport(0, 0, frame->m_viewNormalSpecularPowerTarget.getWidth(), frame->m_viewNormalSpecularPowerTarget.getHeight(), 0.5f, 0.5f);
			gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);
			gfxc->setRenderTargetMask(0xF);
			gfxc->setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);

			Gnm::DepthStencilControl depthStencilControl;
			depthStencilControl.init();
			depthStencilControl.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
			depthStencilControl.setDepthEnable(true);
			gfxc->setDepthStencilControl(depthStencilControl);

			gfxc->setVsShader(depthNormalVertexShader.m_shader, 0, depthNormalVertexShader.m_fetchShader, &depthNormalVertexShader.m_offsetsTable);
			gfxc->setPsShader(depthNormalPixelShader.m_shader, &depthNormalPixelShader.m_offsetsTable);

			const uint32_t elements = 2;
			Gnm::Buffer buffers[elements];
			const Framework::MeshVertexBufferElement element[elements] = { Framework::kPosition, Framework::kNormal };
			Framework::SetMeshVertexBufferFormat( buffers, &simpleMesh, element, elements );
			gfxc->setVertexBuffers( Gnm::kShaderStageVs, 0, elements, buffers );

			for( uint32_t i=0; i<meshes; ++i )
			{
				RenderMesh(frame, &simpleMesh, i);
			}
			// For tiles that were not rasterized even once, fill now with clear color.
			//
			Framework::waitForGraphicsWritesToRenderTarget(gfxc, &frame->m_viewNormalSpecularPowerTarget, Gnm::kWaitTargetSlotCb0);
			Gnmx::eliminateFastClear(gfxc, &frame->m_viewNormalSpecularPowerTarget);
		}

		// decompress depth buffer, because we can't read from it as a texture otherwise

		Gnmx::decompressDepthSurface(gfxc, &bufferCpuIsWriting->m_depthTarget);

		// lighting pass. dispatch a compute shader with one 64 thread wavefront per 64x64 pixels of screen.
		// this wavefront calculates the sub-view frustum of the 64x64 pixels, including near and far planes
		// that correspond to the 8x8 ZMIN and ZMAX values in the HTILE buffer. lights are then
		// broadphase frustum culled in parallel, one thread per light. then, for each 8x8 tile in the 64x64 pixels,
		// any lights that passed the broadphase test are narrowphase frustum culled against the single ZMIN and ZMAX
		// value in the HTILE buffer for that 8x8 tile. Then the 8x8 pixels in the tile are lit in parallel
		// and the result is written to the diffuse and specular light buffers.

		{

			gfxc->setCsShader(lightingShader.m_shader, &lightingShader.m_offsetsTable);

			LightingConstants* constant = frame->m_lightingConstants;
			Matrix4 post_adjust = Matrix4::identity();
			for(int q=0; q<3; q++)
			{
				const int iS = q==0 ? framework.m_config.m_targetWidth : (q==1 ? framework.m_config.m_targetHeight : 1);
				post_adjust.setRow( q, Vector4(iS*(q==0?0.5f:0), iS*(q==1?-0.5f:0), q==2?0.5f:0, iS*0.5f) );
			}
			Matrix4 m44InvProj = inverse(post_adjust*framework.m_projectionMatrix);
			constant->m_inverseProjection = transpose(m44InvProj);
			constant->m_lights[0] = lights;
			constant->m_htile[0] = (Gnm::getGpuMode() == Gnm::kGpuModeNeo) ? 1 : 0;

			Gnm::Buffer constantBuffer;
			constantBuffer.initAsConstantBuffer(constant, sizeof(LightingConstants));
			constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
			gfxc->setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &constantBuffer);

			Gnm::Buffer buffer;
			buffer.initAsDataBuffer( bufferCpuIsWriting->m_depthTarget.getHtileAddress(), Gnm::kDataFormatR32Uint, (bufferCpuIsWriting->m_depthTarget.getHtileSliceSizeInBytes()*numSlices+3)/4 );
			buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // never bound as RWBuffer, so read-only is OK

			gfxc->setTextures( Gnm::kShaderStageCs, 0, 1, &depthTexture ); // depth as rendered in the depth/normal pass
			gfxc->setTextures( Gnm::kShaderStageCs, 1, 1, &frame->m_viewNormalSpecularPowerTexture ); // view space normal, as rendered in the depth/normal pass
			gfxc->setBuffers(Gnm::kShaderStageCs, 2, 1, &frame->m_asynchronousJob.m_lightPositionView.m_buffer);
			gfxc->setBuffers(Gnm::kShaderStageCs, 3, 1, &lightAttenuation.m_buffer);
			gfxc->setBuffers(Gnm::kShaderStageCs, 4, 1, &lightColor.m_buffer);
			gfxc->setBuffers( Gnm::kShaderStageCs, 5, 1, &buffer );

			gfxc->setRwTextures( Gnm::kShaderStageCs, 0, 1, &frame->m_diffuseLightTexture ); // accumulation of diffuse contributions from all lights
			gfxc->setRwTextures( Gnm::kShaderStageCs, 1, 1, &frame->m_specularLightTexture ); // accumulation of specular contributions from all lights

			// we're about to consume the output of a lighting job we kicked asynchronously a while ago, so make sure it's done before we need its output
			gfxc->waitOnAddress(const_cast<uint64_t*>(frame->m_asynchronousJob.m_label), 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);
			gfxc->dispatch( (framework.m_config.m_targetWidth+Gnm::kThreadsPerWavefront-1)/Gnm::kThreadsPerWavefront, (framework.m_config.m_targetHeight+Gnm::kThreadsPerWavefront-1)/Gnm::kThreadsPerWavefront, 1 );
		}

		Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
		
		// main pass. now that there is a diffuse light and specular light sample for each pixel, we can rasterize all the spheres again,
		// this time reading the light samples we just buffered. it's important that we rasterize objects here with exactly the same
		// math with which we rasterized them in the beginning of the frame.

		{
			Gnmx::Toolkit::SurfaceUtil::clearCmaskSurface(*gfxc, &frame->m_backBufferTarget);
			gfxc->setRenderTarget(0, &frame->m_backBufferTarget);
			gfxc->setupScreenViewport(0, 0, frame->m_backBufferTarget.getWidth(), frame->m_backBufferTarget.getHeight(), 0.5f, 0.5f);
			gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

			// Update the CMASK clear color in the RenderTarget, if necessary.
			//
			const Vector4 fatClearColor = framework.getClearColor();
			uint32_t dwords = 0;
			uint32_t clearColor[2] = {};
			Gnmx::Toolkit::dataFormatEncoder(clearColor, &dwords, (Gnmx::Toolkit::Reg32*)&fatClearColor, frame->m_backBufferTarget.getDataFormat());
			frame->m_backBufferTarget.setCmaskClearColor(clearColor[0], clearColor[1]);

			Gnmx::Toolkit::SurfaceUtil::clearCmaskSurface(*gfxc, &frame->m_backBufferTarget);
			gfxc->setRenderTarget(0, &frame->m_backBufferTarget);
			gfxc->setupScreenViewport(0, 0, frame->m_backBufferTarget.getWidth(), frame->m_backBufferTarget.getHeight(), 0.5f, 0.5f);
			gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

			Gnm::DepthStencilControl depthStencilControl;
			depthStencilControl.init();
			depthStencilControl.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncLessEqual);
			depthStencilControl.setDepthEnable(true);
			gfxc->setDepthStencilControl(depthStencilControl);

			gfxc->setPsShader(mainPixelShader.m_shader, &mainPixelShader.m_offsetsTable);
			gfxc->setVsShader(mainVertexShader.m_shader, 0, mainVertexShader.m_fetchShader, &mainVertexShader.m_offsetsTable);

			const uint32_t elements = 2;
			Gnm::Buffer buffers[elements];
			const Framework::MeshVertexBufferElement element[elements] = {Framework::kPosition, Framework::kTexture};
			Framework::SetMeshVertexBufferFormat(buffers, &simpleMesh, element, elements);
			gfxc->setVertexBuffers(Gnm::kShaderStageVs, 0, elements, buffers);

			gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &frame->m_diffuseLightTexture); // accumulation of diffuse contributions from all lights
			gfxc->setTextures(Gnm::kShaderStagePs, 1, 1, &frame->m_specularLightTexture); // accumulation of specular contributions from all lights
			gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &albedoTexture); // texture with samples of albedo and specular power for the surface of an object
			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &albedoSampler);
			for( uint32_t i=0; i<meshes; ++i )
			{
				RenderMesh(frame, &simpleMesh, i);
			}

			// For tiles that were not rasterized even once, fill now with clear color.
			//
			Framework::waitForGraphicsWritesToRenderTarget(gfxc, &frame->m_backBufferTarget, Gnm::kWaitTargetSlotCb0);
			Gnmx::eliminateFastClear(gfxc, &frame->m_backBufferTarget);
		}

		// Final blit to the main render target
		// Before we blit, we need to flush/invalidate all outstanding CB/DB/SX writes
		gfxc->triggerEvent(Gnm::kEventTypeCacheFlushAndInvEvent);
		Gnmx::Toolkit::SurfaceUtil::copyTextureToRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, frame->m_textures[g_displayableBuffer]);

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
	return 0;
}
