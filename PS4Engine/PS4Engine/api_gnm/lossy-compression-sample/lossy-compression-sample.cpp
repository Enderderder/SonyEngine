/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx.h>
#include <texture_tool.h>
#include <texture_tool/raw/memoryimage.h>
#include <texture_tool/raw/helpers.h>
#include <texture_tool/raw/reformat.h>
#include <texture_tool/raw/tiling.h>
#include "../framework/sample_framework.h"
#include "../framework/gnf_loader.h"
#include <texture_tool/filter.h>
using namespace sce;

#define COMPRESS_AT_RUNTIME // When defined, compression code at run time will be enabled which creates a texture at runtime, otherwise texture is directly loaded from gnf file defined by TEXTURE_FILENAME.
#define USE_COMPUTE_COMPRESSION // When defined, compute compression code at run time will be enabled , otherwise CPU compression code is used which is slower but uses the same code as orbis-image2gnf uses for texture compression.

#define TEXTURE_FILENAME "/fxaa-sample/data/Tx-Italy_sky.gnf"
#define IMAGE_FILENAME "/assets/Tx-Italy_sky.tga"

Gnm::Texture uncompressedTexture;
Gnm::Texture uncompressedMip0;
Gnm::Buffer oneColorBuffer;
extern const uint32_t g_oneColorBufferTable[256];

struct Textures
{
    Gnm::DataFormat m_dataFormat;
    Gnm::DataFormat m_spoofDataFormat;
    const char *m_name;
    int m_compressedPercentage;
    int m_originalBpp;
    int m_compressedBpp;

    Framework::CsShader m_compressShader;    
    Framework::CsShader m_decompressShader;

    Gnm::Texture m_compressed;
    Gnm::Texture m_compressedCopy;

    Gnm::Texture m_compressedMip1;
    Gnm::Texture m_compressedMip0;
};

Textures bc1 = {Gnm::kDataFormatBc1UnormNoAlpha, Gnm::kDataFormatR32G32Uint      , "BC1", 50, 4, 2};
Textures bc5 = {Gnm::kDataFormatBc5Unorm       , Gnm::kDataFormatR32G32B32A32Uint, "BC5", 25, 8, 6};
Textures bc7 = {Gnm::kDataFormatBc7Unorm       , Gnm::kDataFormatR32G32B32A32Uint, "BC7", 50, 8, 4};
Textures *textures[] = {&bc1, &bc5, &bc7};

void compress(Gnmx::GnmxGfxContext *gfxc, const Textures *textures)
{
    gfxc->setCsShader(textures->m_compressShader.m_shader, &textures->m_compressShader.m_offsetsTable);
    gfxc->setTextures(Gnm::kShaderStageCs, 0, 1, &textures->m_compressedMip1);
    gfxc->setTextures(Gnm::kShaderStageCs, 1, 1, &uncompressedMip0);
    gfxc->setRwTextures(Gnm::kShaderStageCs, 0, 1, &textures->m_compressedMip0);
    const auto microtilesWide = (textures->m_compressed.getWidth()  + 31) / 32;
    const auto microtilesTall = (textures->m_compressed.getHeight() + 31) / 32;
    gfxc->dispatch(microtilesWide, microtilesTall, 1);
    Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
}

void decompress(Gnmx::GnmxGfxContext *gfxc, const Textures *textures)
{
    gfxc->setCsShader(textures->m_decompressShader.m_shader, &textures->m_decompressShader.m_offsetsTable);
    gfxc->setTextures(Gnm::kShaderStageCs, 0, 1, &textures->m_compressedMip1);
    if(textures->m_dataFormat.getSurfaceFormat() == Gnm::kSurfaceFormatBc1)
        gfxc->setBuffers(Gnm::kShaderStageCs, 1, 1, &oneColorBuffer);
    gfxc->setRwTextures(Gnm::kShaderStageCs, 0, 1, &textures->m_compressedMip0);
    const auto microtilesWide = (textures->m_compressed.getWidth()  + 31) / 32;
    const auto microtilesTall = (textures->m_compressed.getHeight() + 31) / 32;
    gfxc->dispatch(microtilesWide, microtilesTall, 1);
    Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
}

Framework::MenuItemText formatText[] = {
	{"BC1 (4 bits => 2 bits)", "BC1 Texture Format"}, 
	{"BC5 (8 bits => 6 bits)", "BC5 Texture Format"}, 
	{"BC7 (8 bits => 4 bits)", "BC7 Texture Format"}, 
};
int g_format = 0;
Framework::MenuCommandEnum format(formatText, sizeof(formatText)/sizeof(formatText[0]), &g_format);

const Framework::MenuItem menuItem[] =
{
    {{"Format", "Texture Format"}, &format},
};
enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };

int main(int argc, const char *argv[])
{
	Framework::GnmSampleFramework framework;
	framework.processCommandLine(argc, argv);
    
    framework.m_config.m_cameraControls = false;
    framework.m_config.m_depthBufferIsMeaningful = false;
    framework.m_config.m_clearColorIsMeaningful = false;

	framework.initialize("lossy-compression",
		"This demonstrates replacing some percentage of MIP0 with long runs of '0' bytes, which makes a texture ZLIB compress better for downloading and streaming.",
        "Each 32x32 texel region of MIP0 is replaced with a run of 0 bytes, followed by a run of compressed data. "
		"MIP0 is reconstructed from a mixture of this compressed data and MIP1.");

    framework.appendMenuItems(kMenuItems, menuItem);

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
	}

	Framework::LoadCsShader(&bc1.m_compressShader   , Framework::ReadFile("/app0/lossy-compression-sample/compress_bc1_c.sb"),   &framework.m_allocators);
	Framework::LoadCsShader(&bc1.m_decompressShader , Framework::ReadFile("/app0/lossy-compression-sample/decompress_bc1_c.sb"), &framework.m_allocators);
	Framework::LoadCsShader(&bc5.m_compressShader   , Framework::ReadFile("/app0/lossy-compression-sample/compress_bc5_c.sb"),   &framework.m_allocators);
	Framework::LoadCsShader(&bc5.m_decompressShader , Framework::ReadFile("/app0/lossy-compression-sample/decompress_bc5_c.sb"), &framework.m_allocators);
	Framework::LoadCsShader(&bc7.m_compressShader   , Framework::ReadFile("/app0/lossy-compression-sample/compress_bc7_c.sb"),   &framework.m_allocators);
	Framework::LoadCsShader(&bc7.m_decompressShader , Framework::ReadFile("/app0/lossy-compression-sample/decompress_bc7_c.sb"), &framework.m_allocators);

	//
	// Allocate some GPU buffers: shaders
	//
	Gnmx::VsShader *vertexShader = Framework::LoadVsShader("/app0/lossy-compression-sample/shader_vv.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);

	Gnmx::PsShader *pixelShader = Framework::LoadPsShader("/app0/lossy-compression-sample/shader_p.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache pixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&pixelShader_offsetsTable, Gnm::kShaderStagePs, pixelShader);

    uint32_t fssz = Gnmx::computeVsFetchShaderSize(vertexShader);
	void *fetchShaderPtr = framework.m_garlicAllocator.allocate(fssz, Gnm::kAlignmentOfShaderInBytes );
	uint32_t shaderModifier;
	Gnmx::generateVsFetchShader(fetchShaderPtr, &shaderModifier, vertexShader);

    void *oneColorBufferPointer = framework.m_garlicAllocator.allocate(sizeof(g_oneColorBufferTable), 4);
    memcpy(oneColorBufferPointer, g_oneColorBufferTable, sizeof(g_oneColorBufferTable));
    oneColorBuffer.initAsDataBuffer(oneColorBufferPointer, Gnm::kDataFormatR32Uint, sizeof(g_oneColorBufferTable)/sizeof(g_oneColorBufferTable[0]));

    using namespace sce::TextureTool;

    Raw::Configuration g_configuration;
    Raw::MemoryImage loadedImage;
    Raw::load(&loadedImage, "/app0" IMAGE_FILENAME, &g_configuration, 0);

    Raw::MemoryImage uncompressedImage;
    Raw::makeMipMaps(&loadedImage, &uncompressedImage, loadedImage.getDataFormat(), &g_configuration, 0);
    uncompressedTexture = *uncompressedImage.getGnmTexture();
    {
        Gnm::SizeAlign sizeAlign = uncompressedTexture.getSizeAlign();
        uncompressedTexture.setBaseAddress(framework.m_garlicAllocator.allocate(sizeAlign));
        Raw::MemoryRegion memoryRegion;
        memoryRegion.m_begin = (uint8_t *)uncompressedTexture.getBaseAddress();
        memoryRegion.m_end = memoryRegion.m_begin + sizeAlign.m_size;
  	    Raw::tile(memoryRegion, &uncompressedImage, &g_configuration, Raw::kTileMethodMultiThreaded);   
    }
    uncompressedMip0 = uncompressedTexture;
    uncompressedMip0.setMipLevelRange(0,0);

    for(unsigned i = 0; i < sizeof(textures)/sizeof(textures[0]); ++i)
    {
        Raw::MemoryImage compressedImage;
        Raw::reformat(&uncompressedImage, &compressedImage, textures[i]->m_dataFormat, &g_configuration);
        textures[i]->m_compressed = *compressedImage.getGnmTexture();
        {
            Gnm::SizeAlign sizeAlign = textures[i]->m_compressed.getSizeAlign();
            textures[i]->m_compressed.setBaseAddress(framework.m_garlicAllocator.allocate(sizeAlign));
            Raw::MemoryRegion memoryRegion;
            memoryRegion.m_begin = (uint8_t *)textures[i]->m_compressed.getBaseAddress();
            memoryRegion.m_end = memoryRegion.m_begin + sizeAlign.m_size;
  	        Raw::tile(memoryRegion, &compressedImage, &g_configuration, Raw::kTileMethodMultiThreaded);   
            textures[i]->m_compressedCopy = textures[i]->m_compressed;
            textures[i]->m_compressedCopy.setBaseAddress(framework.m_garlicAllocator.allocate(sizeAlign));
            memcpy(textures[i]->m_compressedCopy.getBaseAddress(), textures[i]->m_compressed.getBaseAddress(), sizeAlign.m_size);
        }

        textures[i]->m_compressedMip1 = textures[i]->m_compressed;
        textures[i]->m_compressedMip1.setMipLevelRange(1,1);

        textures[i]->m_compressedMip0 = textures[i]->m_compressed;
        textures[i]->m_compressedMip0.setWidthMinus1 ((textures[i]->m_compressedMip0.getWidth()  + 3) / 4 - 1);
        textures[i]->m_compressedMip0.setHeightMinus1((textures[i]->m_compressedMip0.getHeight() + 3) / 4 - 1);
        textures[i]->m_compressedMip0.setPitchMinus1 ((textures[i]->m_compressedMip0.getPitch()  + 3) / 4 - 1);
        textures[i]->m_compressedMip0.setDataFormat(textures[i]->m_spoofDataFormat);
        textures[i]->m_compressedMip0.setMipLevelRange(0,0);
	    textures[i]->m_compressedMip0.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); 

		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
		gfxc->reset();
        compress(gfxc, textures[i]); 
        decompress(gfxc, textures[i]); 
        Gnmx::Toolkit::submitAndStall(*gfxc);
    }

	while(!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

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

		DbgFont::Window window;
		window.initialize(&bufferCpuIsWriting->m_window, 0, (1080 - 512 - 48) / 16, (1920 - 16) / 8, 1);
		DbgFont::Cell cell;
		cell.m_backgroundAlpha = 7;
		cell.m_backgroundColor = DbgFont::kBlack;
		cell.m_foregroundAlpha = 15;
		cell.m_foregroundColor = DbgFont::kWhite;
		cell.m_character       = ' ';
		window.clear(cell);
        window.setCursor(     0, 0);
		window.printf("%s %d BITS PER TEXEL", textures[g_format]->m_name, textures[g_format]->m_originalBpp);
        window.setCursor( 520/8, 0);
		window.printf("%s %d BITS PER TEXEL (%d%% OF MIP0 BYTES ARE 0)", textures[g_format]->m_name, textures[g_format]->m_compressedBpp, textures[g_format]->m_compressedPercentage);
        window.setCursor(1040/8, 0);
		window.printf("ABSOLUTE DIFFERENCE BETWEEN LEFT AND RIGHT IMAGES");

		gfxc->setVsShader(vertexShader, shaderModifier, fetchShaderPtr, &vertexShader_offsetsTable);
 		gfxc->setPsShader(pixelShader, &pixelShader_offsetsTable);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &textures[g_format]->m_compressedCopy);
		gfxc->setTextures(Gnm::kShaderStagePs, 1, 1, &textures[g_format]->m_compressed    );

        // draw screen quad
		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
		gfxc->drawIndexAuto(3);
		framework.EndFrame(*gfxc);
    }

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
}

const uint32_t g_oneColorBufferTable[256] = {
    0x00000000 , //   0 => 5 bit[ 0=>  0, 0=>  0]=0, 6 bit[ 0=>  0, 0=>  0]=0
    0x00200000 , //   1 => 5 bit[ 0=>  0, 0=>  0]=1, 6 bit[ 0=>  0, 1=>  4]=0
    0x08210000 , //   2 => 5 bit[ 0=>  0, 1=>  8]=1, 6 bit[ 0=>  0, 1=>  4]=1
    0x08410000 , //   3 => 5 bit[ 0=>  0, 1=>  8]=0, 6 bit[ 0=>  0, 2=>  8]=0
    0x08610000 , //   4 => 5 bit[ 0=>  0, 1=>  8]=1, 6 bit[ 0=>  0, 3=> 12]=0
    0x10820000 , //   5 => 5 bit[ 0=>  0, 2=> 16]=0, 6 bit[ 0=>  0, 4=> 16]=0
    0x10820000 , //   6 => 5 bit[ 0=>  0, 2=> 16]=1, 6 bit[ 0=>  0, 4=> 16]=1
    0x18A30000 , //   7 => 5 bit[ 0=>  0, 3=> 24]=1, 6 bit[ 0=>  0, 5=> 20]=0
    0x18C30000 , //   8 => 5 bit[ 0=>  0, 3=> 24]=0, 6 bit[ 0=>  0, 6=> 24]=0
    0x18E30000 , //   9 => 5 bit[ 0=>  0, 3=> 24]=1, 6 bit[ 0=>  0, 7=> 28]=0
    0x20E40000 , //  10 => 5 bit[ 0=>  0, 4=> 33]=1, 6 bit[ 0=>  0, 7=> 28]=1
    0x21040000 , //  11 => 5 bit[ 0=>  0, 4=> 33]=0, 6 bit[ 0=>  0, 8=> 32]=0
    0x21240000 , //  12 => 5 bit[ 0=>  0, 4=> 33]=1, 6 bit[ 0=>  0, 9=> 36]=0
    0x29450000 , //  13 => 5 bit[ 0=>  0, 5=> 41]=0, 6 bit[ 0=>  0,10=> 40]=0
    0x29650000 , //  14 => 5 bit[ 0=>  0, 5=> 41]=1, 6 bit[ 0=>  0,11=> 44]=0
    0x31260020 , //  15 => 5 bit[ 0=>  0, 6=> 49]=1, 6 bit[ 1=>  4, 9=> 36]=0
    0x31860000 , //  16 => 5 bit[ 0=>  0, 6=> 49]=0, 6 bit[ 0=>  0,12=> 48]=0
    0x31A60000 , //  17 => 5 bit[ 0=>  0, 6=> 49]=1, 6 bit[ 0=>  0,13=> 52]=0
    0x39C70000 , //  18 => 5 bit[ 0=>  0, 7=> 57]=1, 6 bit[ 0=>  0,14=> 56]=0
    0x39470040 , //  19 => 5 bit[ 0=>  0, 7=> 57]=0, 6 bit[ 2=>  8,10=> 40]=0
    0x39E70000 , //  20 => 5 bit[ 0=>  0, 7=> 57]=1, 6 bit[ 0=>  0,15=> 60]=0
    0x32060801 , //  21 => 5 bit[ 1=>  8, 6=> 49]=0, 6 bit[ 0=>  0,16=> 65]=0
    0x41E80020 , //  22 => 5 bit[ 0=>  0, 8=> 66]=0, 6 bit[ 1=>  4,15=> 60]=0
    0x42280000 , //  23 => 5 bit[ 0=>  0, 8=> 66]=1, 6 bit[ 0=>  0,17=> 69]=0
    0x4A490000 , //  24 => 5 bit[ 0=>  0, 9=> 74]=0, 6 bit[ 0=>  0,18=> 73]=0
    0x0A612004 , //  25 => 5 bit[ 4=> 33, 1=>  8]=0, 6 bit[ 0=>  0,19=> 77]=0
    0x51CA0060 , //  26 => 5 bit[ 0=>  0,10=> 82]=1, 6 bit[ 3=> 12,14=> 56]=0
    0x528A0000 , //  27 => 5 bit[ 0=>  0,10=> 82]=0, 6 bit[ 0=>  0,20=> 81]=0
    0x02A02805 , //  28 => 5 bit[ 5=> 41, 0=>  0]=0, 6 bit[ 0=>  0,21=> 85]=0
    0x3AC71002 , //  29 => 5 bit[ 2=> 16, 7=> 57]=0, 6 bit[ 0=>  0,22=> 89]=0
    0x59EB0080 , //  30 => 5 bit[ 0=>  0,11=> 90]=0, 6 bit[ 4=> 16,15=> 60]=0
    0x5AEB0000 , //  31 => 5 bit[ 0=>  0,11=> 90]=1, 6 bit[ 0=>  0,23=> 93]=0
    0x630C0000 , //  32 => 5 bit[ 0=>  0,12=> 99]=0, 6 bit[ 0=>  0,24=> 97]=0
    0x23242004 , //  33 => 5 bit[ 4=> 33, 4=> 33]=0, 6 bit[ 0=>  0,25=>101]=0
    0x6B4D0000 , //  34 => 5 bit[ 0=>  0,13=>107]=1, 6 bit[ 0=>  0,26=>105]=0
    0x6B0D0020 , //  35 => 5 bit[ 0=>  0,13=>107]=0, 6 bit[ 1=>  4,24=> 97]=0
    0x2B652004 , //  36 => 5 bit[ 4=> 33, 5=> 41]=0, 6 bit[ 0=>  0,27=>109]=0
    0x738E0000 , //  37 => 5 bit[ 0=>  0,14=>115]=1, 6 bit[ 0=>  0,28=>113]=0
    0x73AE0000 , //  38 => 5 bit[ 0=>  0,14=>115]=0, 6 bit[ 0=>  0,29=>117]=0
    0x732E0040 , //  39 => 5 bit[ 0=>  0,14=>115]=1, 6 bit[ 2=>  8,25=>101]=0
    0x7BCF0000 , //  40 => 5 bit[ 0=>  0,15=>123]=0, 6 bit[ 0=>  0,30=>121]=0
    0x3BE72004 , //  41 => 5 bit[ 4=> 33, 7=> 57]=0, 6 bit[ 0=>  0,31=>125]=0
    0x83D00020 , //  42 => 5 bit[ 0=>  0,16=>132]=1, 6 bit[ 1=>  4,30=>121]=0
    0x84100000 , //  43 => 5 bit[ 0=>  0,16=>132]=0, 6 bit[ 0=>  0,32=>130]=0
    0x44282004 , //  44 => 5 bit[ 4=> 33, 8=> 66]=0, 6 bit[ 0=>  0,33=>134]=0
    0x8C510000 , //  45 => 5 bit[ 0=>  0,17=>140]=1, 6 bit[ 0=>  0,34=>138]=0
    0x8BF10040 , //  46 => 5 bit[ 0=>  0,17=>140]=0, 6 bit[ 2=>  8,31=>125]=0
    0x0C614008 , //  47 => 5 bit[ 8=> 66, 1=>  8]=0, 6 bit[ 0=>  0,35=>142]=0
    0x748E1002 , //  48 => 5 bit[ 2=> 16,14=>115]=0, 6 bit[ 0=>  0,36=>146]=0
    0x94B20000 , //  49 => 5 bit[ 0=>  0,18=>148]=0, 6 bit[ 0=>  0,37=>150]=0
    0x13C24088 , //  50 => 5 bit[ 8=> 66, 2=> 16]=0, 6 bit[ 4=> 16,30=>121]=0
    0x9CD30000 , //  51 => 5 bit[ 0=>  0,19=>156]=0, 6 bit[ 0=>  0,38=>154]=0
    0x5CEB2004 , //  52 => 5 bit[ 4=> 33,11=> 90]=0, 6 bit[ 0=>  0,39=>158]=0
    0xA5140000 , //  53 => 5 bit[ 0=>  0,20=>165]=1, 6 bit[ 0=>  0,40=>162]=0
    0xA5340000 , //  54 => 5 bit[ 0=>  0,20=>165]=0, 6 bit[ 0=>  0,41=>166]=0
    0x64EC2024 , //  55 => 5 bit[ 4=> 33,12=> 99]=0, 6 bit[ 1=>  4,39=>158]=0
    0x7D4F1803 , //  56 => 5 bit[ 3=> 24,15=>123]=0, 6 bit[ 0=>  0,42=>170]=0
    0xAD750000 , //  57 => 5 bit[ 0=>  0,21=>173]=0, 6 bit[ 0=>  0,43=>174]=0
    0x2D854008 , //  58 => 5 bit[ 8=> 66, 5=> 41]=0, 6 bit[ 0=>  0,44=>178]=0
    0xB5160040 , //  59 => 5 bit[ 0=>  0,22=>181]=0, 6 bit[ 2=>  8,40=>162]=0
    0xA5B40801 , //  60 => 5 bit[ 1=>  8,20=>165]=0, 6 bit[ 0=>  0,45=>182]=0
    0x25C44809 , //  61 => 5 bit[ 9=> 74, 4=> 33]=0, 6 bit[ 0=>  0,46=>186]=0
    0xBDF70000 , //  62 => 5 bit[ 0=>  0,23=>189]=0, 6 bit[ 0=>  0,47=>190]=0
    0x7D2F2064 , //  63 => 5 bit[ 4=> 33,15=>123]=0, 6 bit[ 3=> 12,41=>166]=0
    0xC6180000 , //  64 => 5 bit[ 0=>  0,24=>198]=1, 6 bit[ 0=>  0,48=>195]=0
    0xC6380000 , //  65 => 5 bit[ 0=>  0,24=>198]=0, 6 bit[ 0=>  0,49=>199]=0
    0x45C84048 , //  66 => 5 bit[ 8=> 66, 8=> 66]=0, 6 bit[ 2=>  8,46=>186]=0
    0xBE570801 , //  67 => 5 bit[ 1=>  8,23=>189]=0, 6 bit[ 0=>  0,50=>203]=0
    0xCE790000 , //  68 => 5 bit[ 0=>  0,25=>206]=0, 6 bit[ 0=>  0,51=>207]=0
    0x4E894008 , //  69 => 5 bit[ 8=> 66, 9=> 74]=0, 6 bit[ 0=>  0,52=>211]=0
    0xD5FA0060 , //  70 => 5 bit[ 0=>  0,26=>214]=0, 6 bit[ 3=> 12,47=>190]=0
    0x96B22004 , //  71 => 5 bit[ 4=> 33,18=>148]=0, 6 bit[ 0=>  0,53=>215]=0
    0x16C2600C , //  72 => 5 bit[12=> 99, 2=> 16]=0, 6 bit[ 0=>  0,54=>219]=0
    0xDEFB0000 , //  73 => 5 bit[ 0=>  0,27=>222]=0, 6 bit[ 0=>  0,55=>223]=0
    0x5F0B4008 , //  74 => 5 bit[ 8=> 66,11=> 90]=0, 6 bit[ 0=>  0,56=>227]=0
    0x0EC1682D , //  75 => 5 bit[13=>107, 1=>  8]=0, 6 bit[ 1=>  4,54=>219]=0
    0xE73C0000 , //  76 => 5 bit[ 0=>  0,28=>231]=0, 6 bit[ 0=>  0,57=>231]=0
    0x674C4008 , //  77 => 5 bit[ 8=> 66,12=> 99]=0, 6 bit[ 0=>  0,58=>235]=0
    0xEF7D0000 , //  78 => 5 bit[ 0=>  0,29=>239]=0, 6 bit[ 0=>  0,59=>239]=0
    0xAEF52044 , //  79 => 5 bit[ 4=> 33,21=>173]=0, 6 bit[ 2=>  8,55=>223]=0
    0x2F85600C , //  80 => 5 bit[12=> 99, 5=> 41]=0, 6 bit[ 0=>  0,60=>243]=0
    0xF7BE0000 , //  81 => 5 bit[ 0=>  0,30=>247]=0, 6 bit[ 0=>  0,61=>247]=0
    0xB7D62004 , //  82 => 5 bit[ 4=> 33,22=>181]=0, 6 bit[ 0=>  0,62=>251]=0
    0x3706606C , //  83 => 5 bit[12=> 99, 6=> 49]=0, 6 bit[ 3=> 12,56=>227]=0
    0xFFFF0000 , //  84 => 5 bit[ 0=>  0,31=>255]=0, 6 bit[ 0=>  0,63=>255]=0
    0x7FCF4028 , //  85 => 5 bit[ 8=> 66,15=>123]=0, 6 bit[ 1=>  4,62=>251]=0
    0xF7FE0821 , //  86 => 5 bit[ 1=>  8,30=>247]=0, 6 bit[ 1=>  4,63=>255]=0
    0xE73C1082 , //  87 => 5 bit[ 2=> 16,28=>231]=0, 6 bit[ 4=> 16,57=>231]=0
    0x87D04048 , //  88 => 5 bit[ 8=> 66,16=>132]=0, 6 bit[ 2=>  8,62=>251]=0
    0xFFFF0841 , //  89 => 5 bit[ 1=>  8,31=>255]=0, 6 bit[ 2=>  8,63=>255]=0
    0xCFD92064 , //  90 => 5 bit[ 4=> 33,25=>206]=0, 6 bit[ 3=> 12,62=>251]=0
    0x4F4960AC , //  91 => 5 bit[12=> 99, 9=> 74]=0, 6 bit[ 5=> 20,58=>235]=0
    0xF7FE1062 , //  92 => 5 bit[ 2=> 16,30=>247]=0, 6 bit[ 3=> 12,63=>255]=0
    0xC7D82885 , //  93 => 5 bit[ 5=> 41,24=>198]=0, 6 bit[ 4=> 16,62=>251]=0
    0xFFFF1082 , //  94 => 5 bit[ 2=> 16,31=>255]=0, 6 bit[ 4=> 16,63=>255]=0
    0xEF7D18C3 , //  95 => 5 bit[ 3=> 24,29=>239]=0, 6 bit[ 6=> 24,59=>239]=0
    0x9FD340A8 , //  96 => 5 bit[ 8=> 66,19=>156]=0, 6 bit[ 5=> 20,62=>251]=0
    0xF7FE18A3 , //  97 => 5 bit[ 3=> 24,30=>247]=0, 6 bit[ 5=> 20,63=>255]=0
    0xE7DC20C4 , //  98 => 5 bit[ 4=> 33,28=>231]=0, 6 bit[ 6=> 24,62=>251]=0
    0x678C60EC , //  99 => 5 bit[12=> 99,12=> 99]=0, 6 bit[ 7=> 28,60=>243]=0
    0xFFFF18C3 , // 100 => 5 bit[ 3=> 24,31=>255]=0, 6 bit[ 6=> 24,63=>255]=0
    0xEFDD20E4 , // 101 => 5 bit[ 4=> 33,29=>239]=0, 6 bit[ 7=> 28,62=>251]=0
    0x6FED60EC , // 102 => 5 bit[12=> 99,13=>107]=0, 6 bit[ 7=> 28,63=>255]=0
    0xF7BE2104 , // 103 => 5 bit[ 4=> 33,30=>247]=0, 6 bit[ 8=> 32,61=>247]=0
    0xB7D64108 , // 104 => 5 bit[ 8=> 66,22=>181]=0, 6 bit[ 8=> 32,62=>251]=0
    0x37E68110 , // 105 => 5 bit[16=>132, 6=> 49]=0, 6 bit[ 8=> 32,63=>255]=0
    0xFE7F21C4 , // 106 => 5 bit[ 4=> 33,31=>255]=0, 6 bit[14=> 56,51=>207]=0
    0x7FCF612C , // 107 => 5 bit[12=> 99,15=>123]=0, 6 bit[ 9=> 36,62=>251]=0
    0x2FE58931 , // 108 => 5 bit[17=>140, 5=> 41]=0, 6 bit[ 9=> 36,63=>255]=0
    0xF7DE2945 , // 109 => 5 bit[ 5=> 41,30=>247]=0, 6 bit[10=> 40,62=>251]=0
    0x869061EC , // 110 => 5 bit[12=> 99,16=>132]=0, 6 bit[15=> 60,52=>211]=0
    0xFFFF2945 , // 111 => 5 bit[ 5=> 41,31=>255]=0, 6 bit[10=> 40,63=>255]=0
    0xCFD94168 , // 112 => 5 bit[ 8=> 66,25=>206]=0, 6 bit[11=> 44,62=>251]=0
    0x4FE98170 , // 113 => 5 bit[16=>132, 9=> 74]=0, 6 bit[11=> 44,63=>255]=0
    0xF6BE3206 , // 114 => 5 bit[ 6=> 49,30=>247]=0, 6 bit[16=> 65,53=>215]=0
    0xD7DA4188 , // 115 => 5 bit[ 8=> 66,26=>214]=0, 6 bit[12=> 48,62=>251]=0
    0x57EA8190 , // 116 => 5 bit[16=>132,10=> 82]=0, 6 bit[12=> 48,63=>255]=0
    0xFFDF31A6 , // 117 => 5 bit[ 6=> 49,31=>255]=0, 6 bit[13=> 52,62=>251]=0
    0x9F13620C , // 118 => 5 bit[12=> 99,19=>156]=0, 6 bit[16=> 65,56=>227]=0
    0xF7FE39A7 , // 119 => 5 bit[ 7=> 57,30=>247]=0, 6 bit[13=> 52,63=>255]=0
    0xE7DC41C8 , // 120 => 5 bit[ 8=> 66,28=>231]=0, 6 bit[14=> 56,62=>251]=0
    0xA7F461CC , // 121 => 5 bit[12=> 99,20=>165]=0, 6 bit[14=> 56,63=>255]=0
    0xFF7F3A07 , // 122 => 5 bit[ 7=> 57,31=>255]=0, 6 bit[16=> 65,59=>239]=0
    0xEFDD41E8 , // 123 => 5 bit[ 8=> 66,29=>239]=0, 6 bit[15=> 60,62=>251]=0
    0x6FED81F0 , // 124 => 5 bit[16=>132,13=>107]=0, 6 bit[15=> 60,63=>255]=0
    0xF7BE4208 , // 125 => 5 bit[ 8=> 66,30=>247]=0, 6 bit[16=> 65,61=>247]=0
    0xE7DC4A09 , // 126 => 5 bit[ 9=> 74,28=>231]=0, 6 bit[16=> 65,62=>251]=0
    0x67EC8A11 , // 127 => 5 bit[17=>140,12=> 99]=0, 6 bit[16=> 65,63=>255]=0
    0xFF3F4268 , // 128 => 5 bit[ 8=> 66,31=>255]=0, 6 bit[19=> 77,57=>231]=0
    0xBFD7622C , // 129 => 5 bit[12=> 99,23=>189]=0, 6 bit[17=> 69,62=>251]=0
    0x3FE7A234 , // 130 => 5 bit[20=>165, 7=> 57]=0, 6 bit[17=> 69,63=>255]=0
    0xF7DE4A49 , // 131 => 5 bit[ 9=> 74,30=>247]=0, 6 bit[18=> 73,62=>251]=0
    0x87508290 , // 132 => 5 bit[16=>132,16=>132]=0, 6 bit[20=> 81,58=>235]=0
    0xFFFF4A49 , // 133 => 5 bit[ 9=> 74,31=>255]=0, 6 bit[18=> 73,63=>255]=0
    0xEFDD526A , // 134 => 5 bit[10=> 82,29=>239]=0, 6 bit[19=> 77,62=>251]=0
    0x8FF18270 , // 135 => 5 bit[16=>132,17=>140]=0, 6 bit[19=> 77,63=>255]=0
    0xF77E52AA , // 136 => 5 bit[10=> 82,30=>247]=0, 6 bit[21=> 85,59=>239]=0
    0xD7DA628C , // 137 => 5 bit[12=> 99,26=>214]=0, 6 bit[20=> 81,62=>251]=0
    0x57EAA294 , // 138 => 5 bit[20=>165,10=> 82]=0, 6 bit[20=> 81,63=>255]=0
    0xFFDF52AA , // 139 => 5 bit[10=> 82,31=>255]=0, 6 bit[21=> 85,62=>251]=0
    0x9F9382D0 , // 140 => 5 bit[16=>132,19=>156]=0, 6 bit[22=> 89,60=>243]=0
    0x4FE9AAB5 , // 141 => 5 bit[21=>173, 9=> 74]=0, 6 bit[21=> 85,63=>255]=0
    0xF7DE5ACB , // 142 => 5 bit[11=> 90,30=>247]=0, 6 bit[22=> 89,62=>251]=0
    0xA7F482D0 , // 143 => 5 bit[16=>132,20=>165]=0, 6 bit[22=> 89,63=>255]=0
    0xFFBF5AEB , // 144 => 5 bit[11=> 90,31=>255]=0, 6 bit[23=> 93,61=>247]=0
    0xEFDD62EC , // 145 => 5 bit[12=> 99,29=>239]=0, 6 bit[23=> 93,62=>251]=0
    0x6FEDA2F4 , // 146 => 5 bit[20=>165,13=>107]=0, 6 bit[23=> 93,63=>255]=0
    0xD67A73AE , // 147 => 5 bit[14=>115,26=>214]=0, 6 bit[29=>117,51=>207]=0
    0xF7DE630C , // 148 => 5 bit[12=> 99,30=>247]=0, 6 bit[24=> 97,62=>251]=0
    0x77EEA314 , // 149 => 5 bit[20=>165,14=>115]=0, 6 bit[24=> 97,63=>255]=0
    0xFFDF632C , // 150 => 5 bit[12=> 99,31=>255]=0, 6 bit[25=>101,62=>251]=0
    0xBE9783D0 , // 151 => 5 bit[16=>132,23=>189]=0, 6 bit[30=>121,52=>211]=0
    0x3FE7C338 , // 152 => 5 bit[24=>198, 7=> 57]=0, 6 bit[25=>101,63=>255]=0
    0xF7DE6B4D , // 153 => 5 bit[13=>107,30=>247]=0, 6 bit[26=>105,62=>251]=0
    0xC7F88350 , // 154 => 5 bit[16=>132,24=>198]=0, 6 bit[26=>105,63=>255]=0
    0xDEBB7BEF , // 155 => 5 bit[15=>123,27=>222]=0, 6 bit[31=>125,53=>215]=0
    0xFFDF6B6D , // 156 => 5 bit[13=>107,31=>255]=0, 6 bit[27=>109,62=>251]=0
    0x8FF1A374 , // 157 => 5 bit[20=>165,17=>140]=0, 6 bit[27=>109,63=>255]=0
    0xF7DE738E , // 158 => 5 bit[14=>115,30=>247]=0, 6 bit[28=>113,62=>251]=0
    0xD6DA8410 , // 159 => 5 bit[16=>132,26=>214]=0, 6 bit[32=>130,54=>219]=0
    0x87F0AB95 , // 160 => 5 bit[21=>173,16=>132]=0, 6 bit[28=>113,63=>255]=0
    0xFFDF73AE , // 161 => 5 bit[14=>115,31=>255]=0, 6 bit[29=>117,62=>251]=0
    0xDFFB83B0 , // 162 => 5 bit[16=>132,27=>222]=0, 6 bit[29=>117,63=>255]=0
    0x5F2BC418 , // 163 => 5 bit[24=>198,11=> 90]=0, 6 bit[32=>130,57=>231]=0
    0xF7DE7BCF , // 164 => 5 bit[15=>123,30=>247]=0, 6 bit[30=>121,62=>251]=0
    0xA7F4A3D4 , // 165 => 5 bit[20=>165,20=>165]=0, 6 bit[30=>121,63=>255]=0
    0xFFDF7BEF , // 166 => 5 bit[15=>123,31=>255]=0, 6 bit[31=>125,62=>251]=0
    0xEF9D8410 , // 167 => 5 bit[16=>132,29=>239]=0, 6 bit[32=>130,60=>243]=0
    0xAFF5A3F4 , // 168 => 5 bit[20=>165,21=>173]=0, 6 bit[31=>125,63=>255]=0
    0x2F25E45C , // 169 => 5 bit[28=>231, 5=> 41]=0, 6 bit[34=>138,57=>231]=0
    0xF7DE8410 , // 170 => 5 bit[16=>132,30=>247]=0, 6 bit[32=>130,62=>251]=0
    0x77EEC418 , // 171 => 5 bit[24=>198,14=>115]=0, 6 bit[32=>130,63=>255]=0
    0xFFDF8430 , // 172 => 5 bit[16=>132,31=>255]=0, 6 bit[33=>134,62=>251]=0
    0xBF57A474 , // 173 => 5 bit[20=>165,23=>189]=0, 6 bit[35=>142,58=>235]=0
    0x6FEDCC39 , // 174 => 5 bit[25=>206,13=>107]=0, 6 bit[33=>134,63=>255]=0
    0xF7DE8C51 , // 175 => 5 bit[17=>140,30=>247]=0, 6 bit[34=>138,62=>251]=0
    0xC7F8A454 , // 176 => 5 bit[20=>165,24=>198]=0, 6 bit[34=>138,63=>255]=0
    0x4768E49C , // 177 => 5 bit[28=>231, 8=> 66]=0, 6 bit[36=>146,59=>239]=0
    0xFFDF8C71 , // 178 => 5 bit[17=>140,31=>255]=0, 6 bit[35=>142,62=>251]=0
    0x8FF1C478 , // 179 => 5 bit[24=>198,17=>140]=0, 6 bit[35=>142,63=>255]=0
    0xF7DE9492 , // 180 => 5 bit[18=>148,30=>247]=0, 6 bit[36=>146,62=>251]=0
    0xE79C9CB3 , // 181 => 5 bit[19=>156,28=>231]=0, 6 bit[37=>150,60=>243]=0
    0x97F2C498 , // 182 => 5 bit[24=>198,18=>148]=0, 6 bit[36=>146,63=>255]=0
    0xFFDF94B2 , // 183 => 5 bit[18=>148,31=>255]=0, 6 bit[37=>150,62=>251]=0
    0xDFFBA4B4 , // 184 => 5 bit[20=>165,27=>222]=0, 6 bit[37=>150,63=>255]=0
    0x5FABE4DC , // 185 => 5 bit[28=>231,11=> 90]=0, 6 bit[38=>154,61=>247]=0
    0xF7DE9CD3 , // 186 => 5 bit[19=>156,30=>247]=0, 6 bit[38=>154,62=>251]=0
    0xE7FCA4D4 , // 187 => 5 bit[20=>165,28=>231]=0, 6 bit[38=>154,63=>255]=0
    0xFE7F9D93 , // 188 => 5 bit[19=>156,31=>255]=0, 6 bit[44=>178,51=>207]=0
    0xEFDDA4F4 , // 189 => 5 bit[20=>165,29=>239]=0, 6 bit[39=>158,62=>251]=0
    0xAFF5C4F8 , // 190 => 5 bit[24=>198,21=>173]=0, 6 bit[39=>158,63=>255]=0
    0xF7DEA514 , // 191 => 5 bit[20=>165,30=>247]=1, 6 bit[40=>162,62=>251]=0
    0xF69EA5B4 , // 192 => 5 bit[20=>165,30=>247]=0, 6 bit[45=>182,52=>211]=0
    0xA7F4CD19 , // 193 => 5 bit[25=>206,20=>165]=0, 6 bit[40=>162,63=>255]=0
    0xDFDBB536 , // 194 => 5 bit[22=>181,27=>222]=0, 6 bit[41=>166,62=>251]=0
    0xFFFFA534 , // 195 => 5 bit[20=>165,31=>255]=0, 6 bit[41=>166,63=>255]=0
    0x7EAFE5DC , // 196 => 5 bit[28=>231,15=>123]=0, 6 bit[46=>186,53=>215]=0
    0xF7DEAD55 , // 197 => 5 bit[21=>173,30=>247]=0, 6 bit[42=>170,62=>251]=0
    0xC7F8C558 , // 198 => 5 bit[24=>198,24=>198]=0, 6 bit[42=>170,63=>255]=0
    0x87D0E57C , // 199 => 5 bit[28=>231,16=>132]=0, 6 bit[43=>174,62=>251]=0
    0xFEDFADF5 , // 200 => 5 bit[21=>173,31=>255]=0, 6 bit[47=>190,54=>219]=0
    0xCFF9C578 , // 201 => 5 bit[24=>198,25=>206]=0, 6 bit[43=>174,63=>255]=0
    0xF7DEB596 , // 202 => 5 bit[22=>181,30=>247]=1, 6 bit[44=>178,62=>251]=0
    0xF7FEB596 , // 203 => 5 bit[22=>181,30=>247]=0, 6 bit[44=>178,63=>255]=0
    0x96F2E61C , // 204 => 5 bit[28=>231,18=>148]=0, 6 bit[48=>195,55=>223]=0
    0xFFDFB5B6 , // 205 => 5 bit[22=>181,31=>255]=0, 6 bit[45=>182,62=>251]=0
    0xDFFBC5B8 , // 206 => 5 bit[24=>198,27=>222]=0, 6 bit[45=>182,63=>255]=0
    0x8FD1EDDD , // 207 => 5 bit[29=>239,17=>140]=0, 6 bit[46=>186,62=>251]=0
    0xF75EBE17 , // 208 => 5 bit[23=>189,30=>247]=0, 6 bit[48=>195,58=>235]=0
    0xE7FCC5D8 , // 209 => 5 bit[24=>198,28=>231]=0, 6 bit[46=>186,63=>255]=0
    0xFFDFBDF7 , // 210 => 5 bit[23=>189,31=>255]=1, 6 bit[47=>190,62=>251]=0
    0xFFFFBDF7 , // 211 => 5 bit[23=>189,31=>255]=0, 6 bit[47=>190,63=>255]=0
    0xAFB5E61C , // 212 => 5 bit[28=>231,21=>173]=0, 6 bit[48=>195,61=>247]=0
    0xF7DEC618 , // 213 => 5 bit[24=>198,30=>247]=1, 6 bit[48=>195,62=>251]=0
    0xF75EC658 , // 214 => 5 bit[24=>198,30=>247]=0, 6 bit[50=>203,58=>235]=0
    0xB7F6E61C , // 215 => 5 bit[28=>231,22=>181]=0, 6 bit[48=>195,63=>255]=0
    0xFFDFC638 , // 216 => 5 bit[24=>198,31=>255]=1, 6 bit[49=>199,62=>251]=0
    0xFFFFC638 , // 217 => 5 bit[24=>198,31=>255]=0, 6 bit[49=>199,63=>255]=0
    0xFF7FC678 , // 218 => 5 bit[24=>198,31=>255]=1, 6 bit[51=>207,59=>239]=0
    0xF7DECE59 , // 219 => 5 bit[25=>206,30=>247]=0, 6 bit[50=>203,62=>251]=0
    0xE7FCD65A , // 220 => 5 bit[26=>214,28=>231]=0, 6 bit[50=>203,63=>255]=0
    0xFFDFCE79 , // 221 => 5 bit[25=>206,31=>255]=1, 6 bit[51=>207,62=>251]=0
    0xFF9FCE99 , // 222 => 5 bit[25=>206,31=>255]=0, 6 bit[52=>211,60=>243]=0
    0xCFF9E67C , // 223 => 5 bit[28=>231,25=>206]=0, 6 bit[51=>207,63=>255]=0
    0xF7DED69A , // 224 => 5 bit[26=>214,30=>247]=1, 6 bit[52=>211,62=>251]=0
    0xF7FED69A , // 225 => 5 bit[26=>214,30=>247]=0, 6 bit[52=>211,63=>255]=0
    0xC7B8EEBD , // 226 => 5 bit[29=>239,24=>198]=0, 6 bit[53=>215,61=>247]=0
    0xFFDFD6BA , // 227 => 5 bit[26=>214,31=>255]=0, 6 bit[53=>215,62=>251]=0
    0xEFFDDEBB , // 228 => 5 bit[27=>222,29=>239]=0, 6 bit[53=>215,63=>255]=0
    0xEE7DDF7B , // 229 => 5 bit[27=>222,29=>239]=1, 6 bit[59=>239,51=>207]=0
    0xF7DEDEDB , // 230 => 5 bit[27=>222,30=>247]=0, 6 bit[54=>219,62=>251]=0
    0xE7FCE6DC , // 231 => 5 bit[28=>231,28=>231]=0, 6 bit[54=>219,63=>255]=0
    0xFFDFDEFB , // 232 => 5 bit[27=>222,31=>255]=1, 6 bit[55=>223,62=>251]=0
    0xFE9FDF9B , // 233 => 5 bit[27=>222,31=>255]=0, 6 bit[60=>243,52=>211]=0
    0xEFFDE6FC , // 234 => 5 bit[28=>231,29=>239]=0, 6 bit[55=>223,63=>255]=0
    0xEFDDE71C , // 235 => 5 bit[28=>231,29=>239]=1, 6 bit[56=>227,62=>251]=0
    0xF7FEE71C , // 236 => 5 bit[28=>231,30=>247]=0, 6 bit[56=>227,63=>255]=0
    0xF6BEE7BC , // 237 => 5 bit[28=>231,30=>247]=1, 6 bit[61=>247,53=>215]=0
    0xFFDFE73C , // 238 => 5 bit[28=>231,31=>255]=1, 6 bit[57=>231,62=>251]=0
    0xFFFFE73C , // 239 => 5 bit[28=>231,31=>255]=0, 6 bit[57=>231,63=>255]=0
    0xFFDFE75C , // 240 => 5 bit[28=>231,31=>255]=1, 6 bit[58=>235,62=>251]=0
    0xF6DEEFDD , // 241 => 5 bit[29=>239,30=>247]=1, 6 bit[62=>251,54=>219]=0
    0xF7FEEF5D , // 242 => 5 bit[29=>239,30=>247]=0, 6 bit[58=>235,63=>255]=0
    0xF7DEEF7D , // 243 => 5 bit[29=>239,30=>247]=1, 6 bit[59=>239,62=>251]=0
    0xFFFFEF7D , // 244 => 5 bit[29=>239,31=>255]=0, 6 bit[59=>239,63=>255]=0
    0xFEFFEFFD , // 245 => 5 bit[29=>239,31=>255]=1, 6 bit[63=>255,55=>223]=0
    0xF7DEF79E , // 246 => 5 bit[30=>247,30=>247]=1, 6 bit[60=>243,62=>251]=0
    0xF7FEF79E , // 247 => 5 bit[30=>247,30=>247]=0, 6 bit[60=>243,63=>255]=0
    0xF7DEF7BE , // 248 => 5 bit[30=>247,30=>247]=1, 6 bit[61=>247,62=>251]=0
    0xFFDFF7BE , // 249 => 5 bit[30=>247,31=>255]=1, 6 bit[61=>247,62=>251]=1
    0xFFFFF7BE , // 250 => 5 bit[30=>247,31=>255]=0, 6 bit[61=>247,63=>255]=0
    0xFFDFF7DE , // 251 => 5 bit[30=>247,31=>255]=1, 6 bit[62=>251,62=>251]=0
    0xF7FEFFDF , // 252 => 5 bit[31=>255,30=>247]=0, 6 bit[62=>251,63=>255]=0
    0xF7FEFFDF , // 253 => 5 bit[31=>255,30=>247]=1, 6 bit[62=>251,63=>255]=1
    0xFFDFFFFF , // 254 => 5 bit[31=>255,31=>255]=1, 6 bit[63=>255,62=>251]=0
    0xFFFFFFFF , // 255 => 5 bit[31=>255,31=>255]=0, 6 bit[63=>255,63=>255]=0
};