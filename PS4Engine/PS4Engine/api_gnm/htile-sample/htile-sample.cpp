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
#include "shared_symbols.h"
using namespace sce;

namespace
{
	Framework::GnmSampleFramework framework;

	class ShaderConstants
	{
	public:
		uint32_t m_tiles[4];
		uint32_t m_htileField[4];
		uint32_t m_format[4];
		uint32_t m_layout[4];
		uint32_t m_offset[4];
	};

	enum { kVisualizers = 9 };
	const uint32_t numSlices = 1;

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Constants *m_constants;
		Gnm::Buffer m_compressedHtileBuffer; // a reference to the HTILE buffer, created by hardware, that corresponds to a decompressed depth buffer		
		Gnm::Buffer m_decompressedHtileBuffer; // a buffer to hold an HTILE buffer that is created from a decompressed depth buffer by a compute shader		
		Gnm::Buffer m_softwareHtileBuffer;
	};

	class Visualizer
	{
	public:
		const char* m_caption;
		uint32_t m_captionX;
		uint32_t m_captionY;
		uint32_t m_scale;
		uint32_t m_pixelX;
		uint32_t m_pixelY;
		uint32_t m_tilesWide;
		uint32_t m_tilesTall;
		uint32_t m_field0;
		uint32_t m_field1;
		uint8_t m_reserved0[4];
		Gnm::Buffer Frame::*m_htileBuffer;
		Gnm::Buffer m_buffer;
		ShaderConstants* m_shaderConstants;		
		struct PixelShaderConstants {uint32_t m_screenPixelOffset[4];};
		PixelShaderConstants *m_pixelShaderConstants;
		Gnm::Buffer m_pixelShaderConstantBuffer;
		struct VertexShaderConstants {Vector4Unaligned m_position[4];};
		VertexShaderConstants *m_vertexShaderConstants;
		Gnm::Buffer m_vertexShaderConstantBuffer;
		uint32_t pixelsWide() const {return m_tilesWide * m_scale;}
		uint32_t pixelsTall() const {return m_tilesTall * m_scale;}
		void Initialize(uint32_t screenPixelsWide, uint32_t screenPixelsTall)
		{
			m_shaderConstants->m_tiles[0] = m_tilesWide;
			m_shaderConstants->m_tiles[1] = m_tilesTall;
			m_shaderConstants->m_tiles[2] = (Gnm::getGpuMode() == Gnm::kGpuModeNeo) ? 1 : 0;
			m_shaderConstants->m_htileField[0] = m_field0;
			m_shaderConstants->m_htileField[1] = m_field1;
			m_shaderConstants->m_format[0] = kHTileFormatNoStencil;
			m_shaderConstants->m_layout[0] = kHTileLayoutTiled;
			m_shaderConstants->m_layout[1] = kHTileLayoutTiled;
			m_shaderConstants->m_offset[0] = m_pixelX;
			m_shaderConstants->m_offset[1] = m_pixelY;
			m_shaderConstants->m_offset[2] = m_scale;
			const float windowPixelsWide = m_tilesWide;
			const float windowPixelsTall = m_tilesTall;
			const float windowPixelX = m_pixelX;
			const float windowPixelY = m_pixelY;
			m_vertexShaderConstants->m_position[0].x = (windowPixelX                   ) / (float)screenPixelsWide;
			m_vertexShaderConstants->m_position[0].y = (windowPixelY                   ) / -(float)screenPixelsTall + 1;
			m_vertexShaderConstants->m_position[1].x = (windowPixelX + windowPixelsWide) / (float)screenPixelsWide;
			m_vertexShaderConstants->m_position[1].y = (windowPixelY                   ) / -(float)screenPixelsTall + 1;
			m_vertexShaderConstants->m_position[2].x = (windowPixelX                   ) / (float)screenPixelsWide;
			m_vertexShaderConstants->m_position[2].y = (windowPixelY + windowPixelsTall) / -(float)screenPixelsTall + 1;
			m_vertexShaderConstants->m_position[3].x = (windowPixelX + windowPixelsWide) / (float)screenPixelsWide;
			m_vertexShaderConstants->m_position[3].y = (windowPixelY + windowPixelsTall) / -(float)screenPixelsTall + 1;
			m_pixelShaderConstants->m_screenPixelOffset[0] = m_pixelX;
			m_pixelShaderConstants->m_screenPixelOffset[1] = m_pixelY;
		}
	};

	const uint32_t g_hiStencilModeDecoder[64][4] =
	{
		{0x0000}, // 00000000000000
		{0x0001}, // 00000000000001
		{0x0002}, // 00000000000010
		{0x0003}, // 00000000000011
		{0x0004}, // 00000000000100
		{0x0005}, // 00000000000101
		{0x0006}, // 00000000000110
		{0x0007}, // 00000000000111

		{0x0008}, // 00000000001000
		{0x0009}, // 00000000001001
		{0x000a}, // 00000000001010
		{0x000b}, // 00000000001011
		{0x000c}, // 00000000001100
		{0x000d}, // 00000000001101
		{0x000e}, // 00000000001110
		{0x000f}, // 00000000001111

		{0x0011}, // 00000000010001
		{0x0013}, // 00000000010011
		{0x0015}, // 00000000010101
		{0x0017}, // 00000000010111
		{0x0019}, // 00000000011001
		{0x001b}, // 00000000011011
		{0x001d}, // 00000000011101
		{0x001f}, // 00000000011111

		{0x0023}, // 00000000100011
		{0x0027}, // 00000000100111
		{0x002b}, // 00000000101011
		{0x002f}, // 00000000101111
		{0x0033}, // 00000000110011
		{0x0037}, // 00000000110111
		{0x003b}, // 00000000111011
		{0x003f}, // 00000000111111

		{0x004f}, // 00000001001111
		{0x005f}, // 00000001011111
		{0x006f}, // 00000001101111
		{0x007f}, // 00000001111111

		{0x009f}, // 00000010011111
		{0x00bf}, // 00000010111111
		{0x00df}, // 00000011011111
		{0x00ff}, // 00000011111111

		{0x013f}, // 00000100111111
		{0x017f}, // 00000101111111
		{0x01bf}, // 00000110111111
		{0x01ff}, // 00000111111111

		{0x027f}, // 00001001111111
		{0x02ff}, // 00001011111111
		{0x037f}, // 00001101111111
		{0x03ff}, // 00001111111111

		{0x04ff}, // 00010011111111
		{0x05ff}, // 00010111111111
		{0x06ff}, // 00011011111111
		{0x07ff}, // 00011111111111

		{0x09ff}, // 00100111111111
		{0x0bff}, // 00101111111111
		{0x0dff}, // 00110111111111
		{0x0fff}, // 00111111111111

		{0x13ff}, // 01001111111111
		{0x17ff}, // 01011111111111
		{0x1bff}, // 01101111111111
		{0x1fff}, // 01111111111111

		{0x27ff}, // 10011111111111
		{0x2fff}, // 10111111111111
		{0x37ff}, // 11011111111111
		{0x3fff}, // 11111111111111
	};

	Framework::VsShader clearVsShader;
	Framework::PsShader clearPsShader;
	void partiallyClearDepth(sce::Gnmx::GnmxGfxContext &gfxc, const Gnm::DepthRenderTarget *depthTarget, float depthValue, unsigned tileX, unsigned tileY, unsigned tilesWide, unsigned tilesTall)
	{
		gfxc.pushMarker("partiallyClearDepth");

		Gnm::DbRenderControl dbRenderControl;
		dbRenderControl.init();
		dbRenderControl.setDepthClearEnable(true);
		gfxc.setDbRenderControl(dbRenderControl);

		Gnm::DepthStencilControl depthControl;
		depthControl.init();
		depthControl.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncAlways);
		depthControl.setStencilFunction(Gnm::kCompareFuncNever);
		depthControl.setDepthEnable(true);
		gfxc.setDepthStencilControl(depthControl);
		gfxc.setDepthClearValue(depthValue);
		gfxc.setRenderTargetMask(0x0);

		gfxc.setVsShader(clearVsShader.m_shader, 0, 0, &clearVsShader.m_offsetsTable);
		gfxc.setPsShader(clearPsShader.m_shader, &clearPsShader.m_offsetsTable);
		gfxc.setPrimitiveType(Gnm::kPrimitiveTypeRectList);
		gfxc.setIndexSize(Gnm::kIndexSize16);

		const uint32_t width = depthTarget->getWidth();
		const uint32_t height = depthTarget->getHeight();

		Vector4Unaligned* constantBuffer = static_cast<Vector4Unaligned*>(gfxc.allocateFromCommandBuffer(sizeof(Vector4Unaligned), Gnm::kEmbeddedDataAlignment4));
		const unsigned pixelX = tileX * 8;
		const unsigned pixelY = tileY * 8;
		const unsigned pixelsWide = tilesWide * 8;
		const unsigned pixelsTall = tilesTall * 8;
		constantBuffer->x =   pixelsWide * 2.f / width;
		constantBuffer->y =   pixelX     * 2.f / width - 1.f;
		constantBuffer->z = -(pixelsTall * 2.f / height      );           
		constantBuffer->w = -(pixelY     * 2.f / height - 1.f);

		Gnm::Buffer buffer;
		buffer.initAsConstantBuffer(constantBuffer, sizeof(Vector4Unaligned));
		buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc.setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &buffer);

		gfxc.setupScreenViewport(0, 0, width, height, 0.5f, 0.5f);
		const uint32_t firstSlice = depthTarget->getBaseArraySliceIndex();
		const uint32_t lastSlice  = depthTarget->getLastArraySliceIndex();
		Gnm::DepthRenderTarget dtCopy = *depthTarget;
		for(uint32_t iSlice=firstSlice; iSlice<=lastSlice; ++iSlice)
		{
			dtCopy.setArrayView(iSlice, iSlice);
			gfxc.setDepthRenderTarget(&dtCopy);
			gfxc.drawIndexAuto(3);
		}

		gfxc.setRenderTargetMask(0xF);

		dbRenderControl.init();
		gfxc.setDbRenderControl(dbRenderControl);
		gfxc.popMarker();
	}

	int g_partialClearMethod = 0;
	uint32_t g_partialClearX = 128;
	uint32_t g_partialClearY = 64;
	uint32_t g_partialClearWidth = 1;
	uint32_t g_partialClearHeight = 1;
	bool g_resummarizeHTile = false;

	Framework::MenuItemText g_partialClearMethodText[] =
	{
		{"None",     "Do not partially clear HTILE"},
		{"Hardware", "Partially clear HTILE with Depth Block hardware"},
		{"Software", "Partially clear HTILE with Compute Shader software"},
	};

	class MenuCommandPartialClear : public Framework::MenuCommand
	{
		Framework::Menu m_menu;
		Framework::MenuCommandEnum menuCommandEnumPartialClearMethod;
		Framework::MenuCommandUint menuCommandUintPartialClearX;
		Framework::MenuCommandUint menuCommandUintPartialClearY;
		Framework::MenuCommandUint menuCommandUintPartialClearWidth;
		Framework::MenuCommandUint menuCommandUintPartialClearHeight;
	public:
		MenuCommandPartialClear()
		: menuCommandEnumPartialClearMethod(g_partialClearMethodText, sizeof(g_partialClearMethodText)/sizeof(g_partialClearMethodText[0]), &g_partialClearMethod)
		, menuCommandUintPartialClearX(&g_partialClearX)
		, menuCommandUintPartialClearY(&g_partialClearY)
		, menuCommandUintPartialClearWidth(&g_partialClearWidth, 1, 1920/8)
		, menuCommandUintPartialClearHeight(&g_partialClearHeight, 1, 1080/8)
		{
			const Framework::MenuItem menuItem[] =
			{
				{{"Method", 
				"This sample can clear a sub-rectangle of the HTILE buffer, either using Depth Block hardware or a Compute Shader. "
				"The Depth Block hardware method does not require an HTILE cache flush, but the Compute Shader method is potentially "
				"more flexible, as it needn't set every HTILE DWORD to the same value, etc"
				}, &menuCommandEnumPartialClearMethod},
				{{"X",      "The leftmost tile X coordinate of a region to partially clear"}, &menuCommandUintPartialClearX},
				{{"Y",      "The topmost tile Y coordinate of a region to partially clear"}, &menuCommandUintPartialClearY},
				{{"Width",  "The tile width of a region to partially clear"}, &menuCommandUintPartialClearWidth},
				{{"Height", "The tile height of a region to partially clear"}, &menuCommandUintPartialClearHeight},
			};
			enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
			m_menu.appendMenuItems(kMenuItems, &menuItem[0]);
		}
		virtual Framework::Menu* getMenu()
		{
			return &m_menu;
		}
		virtual void adjust(int) const {}
	};
	MenuCommandPartialClear menuCommandPartialClear;
	Framework::MenuCommandBool menuResummarize(&g_resummarizeHTile);
	const Framework::MenuItem menuItem[] =
	{
		{{"Partial Clear", "Efficiently partially clear DepthRenderTargets"}, &menuCommandPartialClear},
		{{"Resummarize HTile", "Forces HTile min/max values to stay up to date with the actual z values"}, &menuResummarize },
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.m_config.m_htile = true;
	framework.m_config.m_lightingIsMeaningful = true;

	framework.initialize("HTILE", 
		"Sample code for visualizing the contents of an HTILE buffer, and for summarizing a depth buffer into an HTILE buffer in compute.", 
		"This sample program renders a torus, uses compute to read the resulting depth buffer and generate a corresponding HTILE buffer, "
		"and then visualizes the HTILE buffer so generated by the hardware alongside the HTILE buffer generated by the compute shader.");

	framework.appendMenuItems(kMenuItems, menuItem);

	int error = 0;

	Framework::VsShader vertexShader, visualizerVertexShader;
	Framework::PsShader visualizerPixelShader, pixelShader;
	Framework::CsShader summarizerComputeShader, partialClearComputeShader;
	Gnm::Texture textures[2];

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/htile-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/htile-sample/shader_p.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&summarizerComputeShader, Framework::ReadFile("/app0/htile-sample/cs_htile_c.sb"), &framework.m_allocators);
	error = Framework::LoadCsShader(&partialClearComputeShader, Framework::ReadFile("/app0/htile-sample/cs_htile_partial_clear_c.sb"), &framework.m_allocators);
	error = Framework::LoadVsMakeFetchShader(&visualizerVertexShader, Framework::ReadFile("/app0/htile-sample/visualizer_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&visualizerPixelShader, Framework::ReadFile("/app0/htile-sample/visualizer_p.sb"), &framework.m_allocators);
	error = Framework::LoadVsMakeFetchShader(&clearVsShader, Framework::ReadFile("/app0/htile-sample/clear_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&clearPsShader, Framework::ReadFile("/app0/htile-sample/clear_p.sb"), &framework.m_allocators);
	error = Framework::loadTextureFromGnf(&textures[0], Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&textures[1], Framework::ReadFile("/app0/assets/icelogo-normal.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never bind this as RWTexture, so it's OK to mark it as read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never bind this as RWTexture, so it's OK to mark it as read-only.

	Gnm::Sampler samplers[2];
	for(int s=0; s<2; s++)
	{
		samplers[s].init();
		samplers[s].setMipFilterMode(Gnm::kMipFilterModeLinear);
		samplers[s].setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
	}

	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();

		void *compressedHtileBufferAddr,*softwareHtileBufferAddr;

		const auto htileSliceSizeInBytes = bufferCpuIsWriting->m_depthTarget.getHtileSliceSizeInBytes() * numSlices;
		const auto alignment = Gnm::kAlignmentOfBufferInBytes;
		const auto resourceType = Gnm::kResourceTypeBufferBaseAddress;

		framework.m_allocators.allocate(&compressedHtileBufferAddr,SCE_KERNEL_WC_GARLIC,htileSliceSizeInBytes,alignment,resourceType,"Buffer %d compressed HTILE buffer",buffer);
		framework.m_allocators.allocate(&softwareHtileBufferAddr,SCE_KERNEL_WC_GARLIC,htileSliceSizeInBytes,alignment,resourceType,"Buffer %d software HTILE buffer",buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),alignment,resourceType,"Buffer %d constant buffer",buffer);

		frame->m_compressedHtileBuffer.initAsRegularBuffer(compressedHtileBufferAddr,sizeof(uint32_t),bufferCpuIsWriting->m_depthTarget.getHtileSliceSizeInBytes()*numSlices/sizeof(uint32_t));
		frame->m_compressedHtileBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's never bound as RWBuffer, so read-only is OK

		frame->m_softwareHtileBuffer.initAsRegularBuffer(softwareHtileBufferAddr,sizeof(uint32_t),bufferCpuIsWriting->m_depthTarget.getHtileSliceSizeInBytes()*numSlices/sizeof(uint32_t));
		frame->m_softwareHtileBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // it's bound as an RWTexture, so it's GPU coherent

		char temp[Framework::kResourceNameLength];
		snprintf(temp,Framework::kResourceNameLength,"Buffer %d decompressed HTILE buffer",buffer);
		Gnm::registerResource(nullptr,framework.m_allocators.m_owner,frame->m_decompressedHtileBuffer.getBaseAddress(),frame->m_decompressedHtileBuffer.getSize(),temp,Gnm::kResourceTypeBufferBaseAddress,0);
	}

	const uint32_t tilesWide = (framework.m_buffer[0].m_depthTarget.getPitch()  + kPixelsPerTileWide - 1) / kPixelsPerTileWide;
	const uint32_t tilesTall = (framework.m_buffer[0].m_depthTarget.getHeight() + kPixelsPerTileTall - 1) / kPixelsPerTileTall;
	const uint32_t scale = 1;
	const uint32_t pixelsWide = tilesWide * scale;
	const uint32_t pixelsTall = tilesTall * scale;

	const uint32_t characterWidthInPixels = (1 << framework.m_config.m_log2CharacterWidth);
	const uint32_t characterHeightInPixels = (1 << framework.m_config.m_log2CharacterHeight);

	const uint32_t cw = characterWidthInPixels;
	const uint32_t ch = characterHeightInPixels;
	const uint32_t tw = (pixelsWide + 1 + cw - 1) / cw;
	const uint32_t th = (pixelsTall + 1 + ch - 1) / ch;
	const uint32_t rw = tw * cw;
	const uint32_t rh = th * ch;
	const uint32_t tx = 1;
	const uint32_t ty = framework.m_buffer[0].m_window.m_heightInCharacters - th*3;
	const uint32_t px = cw * (tx + framework.m_buffer[0].m_window.m_positionXInCharactersRelativeToScreen);
	const uint32_t py = ch * (ty + framework.m_buffer[0].m_window.m_positionYInCharactersRelativeToScreen);
	Visualizer g_visualizer[kVisualizers] =
	{
		{ "Compressed ZMask",   (tx+tw*0), (ty+th*0), scale, px+rw*0, py+rh*0, tilesWide, tilesTall, kHTileNoStencilZMask, kHTileNoStencilZMask, {}, &Frame::m_compressedHtileBuffer },
		{ "Compressed MinZ",    (tx+tw*0), (ty+th*1), scale, px+rw*0, py+rh*1, tilesWide, tilesTall, kHTileNoStencilMinZ,  kHTileNoStencilMinZ,  {}, &Frame::m_compressedHtileBuffer },
		{ "Compressed MaxZ",    (tx+tw*0), (ty+th*2), scale, px+rw*0, py+rh*2, tilesWide, tilesTall, kHTileNoStencilMaxZ,  kHTileNoStencilMaxZ,  {}, &Frame::m_compressedHtileBuffer },
		{ "Decompressed ZMask", (tx+tw*1), (ty+th*0), scale, px+rw*1, py+rh*0, tilesWide, tilesTall, kHTileNoStencilZMask, kHTileNoStencilZMask, {}, &Frame::m_decompressedHtileBuffer },
		{ "Decompressed MinZ",  (tx+tw*1), (ty+th*1), scale, px+rw*1, py+rh*1, tilesWide, tilesTall, kHTileNoStencilMinZ,  kHTileNoStencilMinZ,  {}, &Frame::m_decompressedHtileBuffer },
		{ "Decompressed MaxZ",  (tx+tw*1), (ty+th*2), scale, px+rw*1, py+rh*2, tilesWide, tilesTall, kHTileNoStencilMaxZ,  kHTileNoStencilMaxZ,  {}, &Frame::m_decompressedHtileBuffer },
		{ "Software ZMask",     (tx+tw*2), (ty+th*0), scale, px+rw*2, py+rh*0, tilesWide, tilesTall, kHTileNoStencilZMask, kHTileNoStencilZMask, {}, &Frame::m_softwareHtileBuffer },
		{ "Software MinZ",      (tx+tw*2), (ty+th*1), scale, px+rw*2, py+rh*1, tilesWide, tilesTall, kHTileNoStencilMinZ,  kHTileNoStencilMinZ,  {}, &Frame::m_softwareHtileBuffer },
		{ "Software MaxZ",      (tx+tw*2), (ty+th*2), scale, px+rw*2, py+rh*2, tilesWide, tilesTall, kHTileNoStencilMaxZ,  kHTileNoStencilMaxZ,  {}, &Frame::m_softwareHtileBuffer },
	};

	for( int instance=0; instance<kVisualizers; ++instance )
	{
		framework.m_allocators.allocate((void**)&g_visualizer[instance].m_shaderConstants, SCE_KERNEL_WC_GARLIC, sizeof(ShaderConstants), 4, Gnm::kResourceTypeConstantBufferBaseAddress, "Visualizer %d Constant Buffer", instance);
		framework.m_allocators.allocate((void**)&g_visualizer[instance].m_pixelShaderConstants, SCE_KERNEL_WC_GARLIC, sizeof(Visualizer::PixelShaderConstants), 4, Gnm::kResourceTypeConstantBufferBaseAddress, "Visualizer %d Pixel Constant Buffer", instance);
		framework.m_allocators.allocate((void**)&g_visualizer[instance].m_vertexShaderConstants, SCE_KERNEL_WC_GARLIC, sizeof(Visualizer::VertexShaderConstants), 4, Gnm::kResourceTypeConstantBufferBaseAddress, "Visualizer %d Vertex Constant Buffer", instance);

		g_visualizer[instance].m_buffer.initAsConstantBuffer(g_visualizer[instance].m_shaderConstants, sizeof(ShaderConstants));
		g_visualizer[instance].m_pixelShaderConstantBuffer.initAsConstantBuffer(g_visualizer[instance].m_pixelShaderConstants, sizeof(Visualizer::PixelShaderConstants));
		g_visualizer[instance].m_vertexShaderConstantBuffer.initAsConstantBuffer(g_visualizer[instance].m_vertexShaderConstants, sizeof(Visualizer::VertexShaderConstants));
		g_visualizer[instance].m_buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // never bound as RWBuffer, so read-only is OK
		g_visualizer[instance].m_pixelShaderConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // never bound as RWBuffer, so read-only is OK
		g_visualizer[instance].m_vertexShaderConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // never bound as RWBuffer, so read-only is OK
		g_visualizer[instance].Initialize(framework.m_config.m_targetWidth, framework.m_config.m_targetHeight);
	}

	Framework::SimpleMesh quadMesh;
	BuildQuadMesh(&framework.m_allocators, "Quad", &quadMesh, 1.f);

	Framework::SimpleMesh torusMesh;
	BuildTorusMesh(&framework.m_allocators, "Torus", &torusMesh, 0.8f, 0.2f, 32, 32, 8.f, 2.f);

	uint32_t *hiStencilModeDecoder;
	framework.m_allocators.allocate((void**)&hiStencilModeDecoder, SCE_KERNEL_WC_GARLIC, sizeof(g_hiStencilModeDecoder), 4, Gnm::kResourceTypeBufferBaseAddress, "Hi Stencil Mode Decoder Buffer");
	memcpy(hiStencilModeDecoder, g_hiStencilModeDecoder, sizeof(g_hiStencilModeDecoder));
	Gnm::Buffer hiStencilModeDecoderConstantBuffer;
	hiStencilModeDecoderConstantBuffer.initAsConstantBuffer(hiStencilModeDecoder, sizeof(g_hiStencilModeDecoder));

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace( Gnm::kPrimitiveSetupCullFaceBack );
		primSetupReg.setFrontFace( Gnm::kPrimitiveSetupFrontFaceCcw );
		gfxc->setPrimitiveSetup( primSetupReg );

		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
		Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);

		Gnm::BlendControl blendControl;
		blendControl.init();
		blendControl.setBlendEnable(false);
		gfxc->setBlendControl(0, blendControl);
		gfxc->setRenderTargetMask(0xF);
		gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
		gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);
		gfxc->setupScreenViewport( 0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f );

		// render some graphics
		{
			Gnm::Buffer constantBuffer;
			constantBuffer.initAsConstantBuffer(frame->m_constants, sizeof(*frame->m_constants));
			constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

			gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
			gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

			torusMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

			gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 2, samplers);

			const float radians = framework.GetSecondsElapsedApparent() * 0.5f;

			const Matrix4 m = Matrix4::rotationZYX(Vector3(radians,radians,0.f));

			frame->m_constants->m_modelView = transpose(framework.m_worldToViewMatrix*m);
			frame->m_constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*m);
			frame->m_constants->m_lightPosition = framework.getLightPositionInViewSpace();
			frame->m_constants->m_lightColor = framework.getLightColor();
			frame->m_constants->m_ambientColor = framework.getAmbientColor();
			frame->m_constants->m_lightAttenuation = Vector4(1, 0, 0, 0);

			gfxc->setPrimitiveType(torusMesh.m_primitiveType);
			gfxc->setIndexSize(torusMesh.m_indexType);
			gfxc->drawIndex(torusMesh.m_indexCount, torusMesh.m_indexBuffer);

			if (g_resummarizeHTile)
			{
				Gnm::DbRenderControl dbRenderControl;
				dbRenderControl.init();
				dbRenderControl.setHtileResummarizeEnable(true); // Turn Resummarize on!
				gfxc->setDbRenderControl(dbRenderControl);

				Gnm::RenderOverrideControl ovr;
				ovr.init();
				ovr.setForceZValid(true); // need to force Z read with this so the DB is forced to read tiles.
				gfxc->setRenderOverrideControl(ovr);

				// disable z writes
				dsc.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways);
				dsc.setDepthEnable(true);
				gfxc->setDepthStencilControl(dsc);

				// disable color writes
				gfxc->setRenderTargetMask(0);

				Gnmx::renderFullScreenQuad(gfxc); // Resummarize HTILE by forcing z reads over the full screen.

				// restore color writes
				gfxc->setRenderTargetMask(0xF);
				// restore resummarization status
				dbRenderControl.setHtileResummarizeEnable(false); // Turn Resummarize off.
				gfxc->setDbRenderControl(dbRenderControl);
				ovr.setForceZValid(false); // restore override.
				gfxc->setRenderOverrideControl(ovr);
			}
		}

		switch(g_partialClearMethod)
		{
		case 1:
			partiallyClearDepth(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f, g_partialClearX, g_partialClearY, g_partialClearWidth, g_partialClearHeight);
			break;
		case 2:
			{
				volatile uint64_t* label = (volatile uint64_t*)gfxc->allocateFromCommandBuffer( sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8 ); // allocate memory from the command buffer
				*label = 0x0; // set the memory to have the val 0
				gfxc->writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, const_cast<uint64_t*>(label), 2, Gnm::kCacheActionNone);
				gfxc->waitOnAddress(const_cast<uint64_t*>(label), 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 2);
				gfxc->triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta);
				gfxc->setCsShader(partialClearComputeShader.m_shader, &partialClearComputeShader.m_offsetsTable);
				Gnm::Buffer htileBuffer;
				void *address = bufferCpuIsWriting->m_depthTarget.getHtileAddress();
				const unsigned dwords = bufferCpuIsWriting->m_depthTarget.getHtileSliceSizeInBytes() / sizeof(uint32_t) * numSlices;
				htileBuffer.initAsRegularBuffer(address, sizeof(uint32_t), dwords);
				htileBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
				struct ConstantBuffer
				{
					uint32_t m_htileRegionTileX; 
					uint32_t m_htileRegionTileY;
					uint32_t m_htileRegionTilesWide;
					uint32_t m_htileRegionTilesTall;
					uint32_t m_htileValue;
					uint32_t m_neoMode;
					uint32_t m_padding[2];
				};
				ConstantBuffer* constantBufferAddress = static_cast<ConstantBuffer*>(gfxc->allocateFromCommandBuffer(sizeof(ConstantBuffer), Gnm::kEmbeddedDataAlignment4));
				constantBufferAddress->m_htileRegionTileX = g_partialClearX;
				constantBufferAddress->m_htileRegionTileY = g_partialClearY;
				constantBufferAddress->m_htileRegionTilesWide = g_partialClearWidth;
				constantBufferAddress->m_htileRegionTilesTall = g_partialClearHeight;
				constantBufferAddress->m_htileValue = 0xFFFFFFF0U;
				constantBufferAddress->m_neoMode = (Gnm::getGpuMode() == Gnm::kGpuModeNeo) ? 1 : 0;
				Gnm::Buffer constantBuffer;
				constantBuffer.initAsConstantBuffer(constantBufferAddress, sizeof(*constantBufferAddress));
				gfxc->setRwBuffers(Gnm::kShaderStageCs, 0, 1, &htileBuffer);
				gfxc->setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &g_visualizer[0].m_buffer);
				gfxc->setConstantBuffers(Gnm::kShaderStageCs, 1, 1, &constantBuffer);
				gfxc->dispatch((constantBufferAddress->m_htileRegionTilesWide + 7) / 8, (constantBufferAddress->m_htileRegionTilesTall + 7) / 8, 1);
				Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
				break;
			}
		default:
			break;
		}

		// wait for the rendering to complete. flush the HTILE cache, and copy the HTILE buffer away before the decompress destroys its ZMASK field
		{
			volatile uint64_t* label = (volatile uint64_t*)gfxc->allocateFromCommandBuffer( sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8 ); // allocate memory from the command buffer
			*label = 0x0; // set the memory to have the val 0
			gfxc->writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, const_cast<uint64_t*>(label), 1, Gnm::kCacheActionNone);
			gfxc->waitOnAddress(const_cast<uint64_t*>(label), 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);

			gfxc->triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta);
			gfxc->copyData(frame->m_compressedHtileBuffer.getBaseAddress(), bufferCpuIsWriting->m_depthTarget.getHtileAddress(), bufferCpuIsWriting->m_depthTarget.getHtileSliceSizeInBytes()*numSlices, Gnm::kDmaDataBlockingEnable);
		}

		// decompress the depth buffer, so we can use it as a texture below
		{
			Gnmx::decompressDepthSurface(gfxc, &bufferCpuIsWriting->m_depthTarget);
			Gnm::DbRenderControl dbRenderControl;
			dbRenderControl.init();
			gfxc->setDbRenderControl(dbRenderControl);
			gfxc->triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta);
		}
		
		frame->m_decompressedHtileBuffer.initAsRegularBuffer(bufferCpuIsWriting->m_depthTarget.getHtileAddress(), sizeof(uint32_t), bufferCpuIsWriting->m_depthTarget.getHtileSliceSizeInBytes()*numSlices/sizeof(uint32_t) );
		frame->m_decompressedHtileBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never bind as RWBuffer, so read-only is OK

		// summarize the depth buffer into a software-generated HTILE buffer
		{
			Gnm::Texture depthTexture;
			depthTexture.initFromDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget, false);
			depthTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never bind this as RWTexture, so it's OK to mark it as read-only.

			gfxc->setCsShader(summarizerComputeShader.m_shader, &summarizerComputeShader.m_offsetsTable);

			gfxc->setTextures(Gnm::kShaderStageCs, 0, 1, &depthTexture);
			gfxc->setRwBuffers(Gnm::kShaderStageCs, 0, 1, &frame->m_softwareHtileBuffer);

			gfxc->setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &g_visualizer[0].m_buffer);

			const uint32_t pixelsWide = depthTexture.getWidth();
			const uint32_t pixelsTall = depthTexture.getHeight();

			gfxc->dispatch( (pixelsWide+kThreadsPerWavefront-1)/kThreadsPerWavefront, (pixelsTall+kThreadsPerWavefront-1)/kThreadsPerWavefront, 1 );
		}

		Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);

		// visualize the hardware-generated and software-generated HTILE buffers as grayscale images
		{
			gfxc->setPsShader(visualizerPixelShader.m_shader, &visualizerPixelShader.m_offsetsTable);
			gfxc->setVsShader(visualizerVertexShader.m_shader, 0, visualizerVertexShader.m_fetchShader, &visualizerVertexShader.m_offsetsTable);
			gfxc->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
			DbgFont::Cell cell;
			cell.m_backgroundAlpha = 7;
			cell.m_backgroundColor = DbgFont::kBlack;
			cell.m_foregroundAlpha = 15;
			cell.m_foregroundColor = DbgFont::kWhite;
			cell.m_character       = ' ';
			for( int instance=0; instance<kVisualizers; ++instance )
			{
				Visualizer *visualizer = &g_visualizer[instance];
				gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &visualizer->m_vertexShaderConstantBuffer);
				gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &visualizer->m_buffer);
				gfxc->setConstantBuffers(Gnm::kShaderStagePs, 1, 1, &hiStencilModeDecoderConstantBuffer);
				gfxc->setConstantBuffers(Gnm::kShaderStagePs, 2, 1, &visualizer->m_pixelShaderConstantBuffer);
				gfxc->setBuffers(Gnm::kShaderStagePs, 0, 1, &(frame->*(visualizer->m_htileBuffer)));
				gfxc->drawIndexAuto(3);

				DbgFont::Window window;
				window.initialize(&bufferCpuIsWriting->m_window, visualizer->m_captionX, visualizer->m_captionY, visualizer->pixelsWide() / characterWidthInPixels, 1);
				window.clear(cell);
				window.printf(visualizer->m_caption);
			}
		}

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
	return 0;
}
