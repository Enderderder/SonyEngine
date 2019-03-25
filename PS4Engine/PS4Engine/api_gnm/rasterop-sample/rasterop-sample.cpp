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

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Constants *m_constants;
		Gnm::RenderTarget m_ropTarget;
		Gnm::Texture m_ropTexture;
		Gnmx::ResourceBarrier m_ropBarrier;
		uint8_t m_reserved0[4];
	};

	class RuntimeOptions
	{
	public:
		uint32_t m_rasterOpIndex;
		uint8_t m_src, m_dst;
		RuntimeOptions()
			: m_rasterOpIndex(12)
			, m_src(5) // '0101'
			, m_dst(3) // '0011'
		{
		}
		uint8_t m_reserved0[2];
	};
	RuntimeOptions g_runtimeOptions;

	void byteToBinary(uint8_t in, char out[9])
	{
		out[0] = (in & (1<<7)) ? '1' : '0';
		out[1] = (in & (1<<6)) ? '1' : '0';
		out[2] = (in & (1<<5)) ? '1' : '0';
		out[3] = (in & (1<<4)) ? '1' : '0';
		out[4] = (in & (1<<3)) ? '1' : '0';
		out[5] = (in & (1<<2)) ? '1' : '0';
		out[6] = (in & (1<<1)) ? '1' : '0';
		out[7] = (in & (1<<0)) ? '1' : '0';
		out[8] = 0;
	}

	class MenuCommandUint8Binary : public Framework::MenuCommandUint8
	{
	public:
		MenuCommandUint8Binary(uint8_t* target, uint8_t min=0, uint8_t max=0, const char *format = 0)
			: MenuCommandUint8(target, min, max, format)
		{

		}
		virtual void printValue(sce::DbgFont::Window *window)
		{
			char bits[9];
			byteToBinary(*m_target, bits); 
			window->printf("%8s 0x%02X %3d", bits, *m_target, *m_target);
		}
	};


	class MenuCommandUint8Pixel : public Framework::MenuCommand
	{
		Frame *m_frames;
	protected:
		volatile uint8_t *m_x;
		volatile uint8_t *m_y;
	public:
		MenuCommandUint8Pixel(uint8_t *currentX, uint8_t *currentY)
			: MenuCommand()
			, m_x((volatile uint8_t*)currentX)
			, m_y((volatile uint8_t*)currentY)
		{
		}
		void setFrames(Frame *frames)
		{
			m_frames = frames;
		}
		virtual void printValue(sce::DbgFont::Window *window)
		{
			uint8_t x = *m_x;
			uint8_t y = *m_y;
			const Gnm::Texture tex = m_frames[framework.getIndexOfBufferCpuIsWriting()].m_ropTexture;
			if (x >= tex.getWidth() || y >= tex.getHeight())
				window->printf("????????");
			else
			{
				uint8_t *pixels = (uint8_t *)tex.getBaseAddress();
				char bits[9];
				uint8_t byte = pixels[tex.getPitch()*y + x];
				byteToBinary(byte, bits); 
				window->printf("%8s 0x%02X %3d", bits, byte, byte);
			}
		}
		virtual void adjust(int) const
		{
			return; // read-only
		}
	};

	const uint32_t rasterOpEnumValues[] =
	{
		Gnm::kRasterOpBlackness,
		Gnm::kRasterOpNor,
		Gnm::kRasterOpAndInverted,
		Gnm::kRasterOpCopyInverted,
		Gnm::kRasterOpAndReverse,
		Gnm::kRasterOpInvert,
		Gnm::kRasterOpXor,
		Gnm::kRasterOpNand,
		Gnm::kRasterOpAnd,
		Gnm::kRasterOpEquiv,
		Gnm::kRasterOpNoop,
		Gnm::kRasterOpOrInverted,
		Gnm::kRasterOpCopy,
		Gnm::kRasterOpOrReverse,
		Gnm::kRasterOpOr,
		Gnm::kRasterOpSet,
	};
	const uint32_t rasterOpEnumValueCount = sizeof(rasterOpEnumValues) / sizeof(rasterOpEnumValues[0]);
	Framework::MenuItemText rasterOpText[] =
	{
		{"Gnm::kRasterOpBlackness",    "0            "},
		{"Gnm::kRasterOpNor",          "~(src |  dst)"},
		{"Gnm::kRasterOpAndInverted",  " ~src &  dst "},
		{"Gnm::kRasterOpCopyInverted", " ~src        "},
		{"Gnm::kRasterOpAndReverse",   "  src & ~dst "},
		{"Gnm::kRasterOpInvert",       "        ~dst "},
		{"Gnm::kRasterOpXor",          "  src ^  dst "},
		{"Gnm::kRasterOpNand",         "~(src &  dst)"},
		{"Gnm::kRasterOpAnd",          "  src &  dst "},
		{"Gnm::kRasterOpEquiv",        "~(src ^  dst)"},
		{"Gnm::kRasterOpNoop",         "         dst "},
		{"Gnm::kRasterOpOrInverted",   " ~src |  dst "},
		{"Gnm::kRasterOpCopy",         "  src        "},
		{"Gnm::kRasterOpOrReverse",    "  src | ~dst "},
		{"Gnm::kRasterOpOr",           "  src |  dst "},
		{"Gnm::kRasterOpSet",          "1            "},
	};

	class MenuCommandEnumSparse : public Framework::MenuCommandEnum
	{
	protected:
		const uint32_t *m_menuToEnumMap;
	public:
		MenuCommandEnumSparse(Framework::MenuItemText* menuItemText, const uint32_t *menuToEnumMap, uint32_t menuItems, uint32_t* target, Framework::Validator* validator = 0)
			:	MenuCommandEnum(menuItemText, menuItems, target, validator)
			,	m_menuToEnumMap(menuToEnumMap) // index of kRasterOpCopy
		{
		}
		virtual void printValue(sce::DbgFont::Window *window)
		{
			uint32_t target = *m_target;
			window->printf("%s (0x%02X)", m_menuItemText[target].m_name, m_menuToEnumMap[target]);
		}
	};


	MenuCommandEnumSparse  menuRasterOp(rasterOpText, rasterOpEnumValues, rasterOpEnumValueCount, &g_runtimeOptions.m_rasterOpIndex);
	MenuCommandUint8Binary menuSrcByte(&g_runtimeOptions.m_src, 0, 255);
	MenuCommandUint8Binary menuDestByte(&g_runtimeOptions.m_dst, 0, 255);
	MenuCommandUint8Pixel  menuOutByte(&g_runtimeOptions.m_src, &g_runtimeOptions.m_dst);
	const Framework::MenuItem menuItem[] =
	{
		{{"Raster Op", "Raster operation"}, &menuRasterOp},
		{{"Source Byte", "Source byte"}, &menuSrcByte},
		{{"Dest Byte",   "Destination byte (current RT contents)"}, &menuDestByte},
		{{"Output Byte", "Output of ROP(src,dest)"}, &menuOutByte},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.m_config.m_lightingIsMeaningful = false;

	framework.initialize( "RasterOp", 
		"Sample code to demonstrate Gnm's Boolean raster ops.",
		"This sample program displays the results of enabling Gnm's Boolean raster ops.");
	framework.appendMenuItems(kMenuItems, menuItem);

	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(Constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Constant Buffer",buffer);
		// Initialize a 256x256 render target
		Gnm::SizeAlign ropSizeAlign = Gnmx::Toolkit::init(&frame->m_ropTarget,256,256,1,Gnm::kDataFormatR8Uint,Gnm::kTileModeDisplay_LinearAligned,Gnm::kNumSamples1,Gnm::kNumFragments1,NULL,NULL);
		uint8_t *ropTargetPixelsCpu;
		uint8_t *ropTargetPixelsGpu;
		framework.m_allocators.allocate((void**)&ropTargetPixelsCpu,SCE_KERNEL_WB_ONION,ropSizeAlign,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d ROP Texture",buffer);
		framework.m_allocators.allocate((void**)&ropTargetPixelsGpu,SCE_KERNEL_WC_GARLIC,ropSizeAlign,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d ROP Target",buffer);

		GpuAddress::TilingParameters ropTargetTp;
		ropTargetTp.initFromRenderTarget(&frame->m_ropTarget,0);
		memset(ropTargetPixelsCpu,0x80,ropSizeAlign.m_size);
		GpuAddress::tileSurface(ropTargetPixelsGpu,ropTargetPixelsCpu,&ropTargetTp);
		frame->m_ropTarget.setBaseAddress(ropTargetPixelsGpu);
		frame->m_ropTexture.initFromRenderTarget(&frame->m_ropTarget,false);
		frame->m_ropBarrier.init(&frame->m_ropTarget,Gnmx::ResourceBarrier::kUsageRenderTarget,Gnmx::ResourceBarrier::kUsageRoTexture);
	}

	menuOutByte.setFrames(frames);

	int error = 0;
	Framework::PsShader ropShader0, ropShader1, pixelShader;
	Framework::VsShader vertexShader;

	error = Framework::LoadPsShader(&ropShader0, Framework::ReadFile("/app0/rasterop-sample/rop_pass0_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&ropShader1, Framework::ReadFile("/app0/rasterop-sample/rop_pass1_p.sb"), &framework.m_allocators);
	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/rasterop-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/rasterop-sample/shader_p.sb"), &framework.m_allocators);

	Gnm::Sampler pointSampler;
	pointSampler.init();
	pointSampler.setMipFilterMode(Gnm::kMipFilterModePoint);
	pointSampler.setXyFilterMode(Gnm::kFilterModePoint, Gnm::kFilterModePoint);

	Framework::SimpleMesh quadMesh;
	BuildQuadMesh(&framework.m_allocators, "Quad", &quadMesh, 3.5f);

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);
		gfxc->setRenderTargetMask(0xF);
		gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);

		// Render the base layer to the ROP texture
		gfxc->setupScreenViewport(0, 0, frame->m_ropTarget.getWidth(), frame->m_ropTarget.getHeight(), 0.5f, 0.5f);
		gfxc->setRenderTarget(0, &frame->m_ropTarget);
		gfxc->setDepthRenderTarget(NULL);
		gfxc->setDepthStencilDisable();
		gfxc->setPsShader(ropShader0.m_shader, &ropShader0.m_offsetsTable);
		gfxc->setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);
		Gnmx::renderFullScreenQuad(gfxc);
		gfxc->setCbControl(Gnm::kCbModeNormal, (Gnm::RasterOp)rasterOpEnumValues[g_runtimeOptions.m_rasterOpIndex]);
		gfxc->setPsShader(ropShader1.m_shader, &ropShader1.m_offsetsTable);
		Gnmx::renderFullScreenQuad(gfxc);
		gfxc->setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);
		frame->m_ropBarrier.write(&gfxc->m_dcb);

		// Render the scene main scene

		// Clear
		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
		Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
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

		const float radians = 0;//framework.GetSecondsElapsedApparent() * 0.5f;
		const Matrix4 m = Matrix4::rotationZYX(Vector3(radians,radians,0.f));
		Constants *constants = frame->m_constants;
		constants->m_modelView = transpose(framework.m_worldToViewMatrix*m);
		constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*m);

		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(constants, sizeof(Constants));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &frame->m_ropTexture);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &pointSampler);
		
		gfxc->setPrimitiveType(quadMesh.m_primitiveType);
		gfxc->setIndexSize(quadMesh.m_indexType);
		gfxc->drawIndex(quadMesh.m_indexCount, quadMesh.m_indexBuffer);
		framework.EndFrame(*gfxc);
	}
	
	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
}
