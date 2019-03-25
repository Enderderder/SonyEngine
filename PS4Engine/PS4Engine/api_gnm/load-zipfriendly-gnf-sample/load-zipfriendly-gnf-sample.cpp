/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx.h>
#include <texture_tool.h>
#include <texture_tool/mipped_image.h>
#include "../framework/sample_framework.h"
#include "../framework/gnf_loader.h"
#include <texture_tool/filter.h>
#include <gnmx/shader_parser.h>

using namespace sce;

#define NUM_TEXTURE_TYPES (4)

int main(int argc, const char *argv[])
{
	Framework::GnmSampleFramework framework;
	framework.removeAllMenuItems();
	Framework::MenuItemText texturesMenuItems[NUM_TEXTURE_TYPES] = {
		{ "Packed BC7", "Mip0 data is invalid while it is packed" },
		{ "Unpacked BC7", "Mip0 after being unpacked" },
		{ "Mode5 only BC7", "When only mode5 blocks are used; similar to this technique restriction" },
		{ "Default BC7", "When no restriction is applied" },
	};
	int currentTexture = 0;
	Framework::MenuCommandEnum texturesMenu(texturesMenuItems, sizeof(texturesMenuItems) / sizeof(texturesMenuItems[0]), &currentTexture);
	const Framework::MenuItem menuItem[] =
	{
		{ { "Current texture", "Shows the result of compression of a source image (assumes/represents RGBA data)" }, &texturesMenu },
	};
	enum { kMenuItems = sizeof(menuItem) / sizeof(menuItem[0]) };
	framework.appendMenuItems(kMenuItems, menuItem);
	framework.processCommandLine(argc, argv);

	framework.m_config.m_cameraControls = false;
	framework.m_config.m_depthBufferIsMeaningful = false;
	framework.m_config.m_clearColorIsMeaningful = false;

	framework.initialize("load-zipfriendly-gnf-sample",
		"This sample code demonstrates how to unpack bc7 zip friendly gnf to a valid bc7 gnf.",
		"Packed texture (the first texture) is a gnf when its mip0 redundant data is zeroed out and rearranged, so that it would zip roughly 40% better. This technique would save bandwidth when textures are transferred from disk to memory, but it loses some quality in return.<Br>"
		"Once loaded, mip0 can be unpacked with gnf::unpacker.unpack API (the second texture). The third and 4th textures are provided to visually compare the loss of quality when this technique is used."
		);


	Gnmx::GnmxGfxContext *commandBuffers = new Gnmx::GnmxGfxContext[framework.m_config.m_buffers]; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	for (uint32_t bufferIndex = 0; bufferIndex < framework.m_config.m_buffers; ++bufferIndex)
		createCommandBuffer(&commandBuffers[bufferIndex], &framework, bufferIndex);

	//
	// Allocate some GPU buffers: shaders
	//
	Gnmx::VsShader *vertexShader = Framework::LoadVsShader("/app0/load-zipfriendly-gnf-sample/shader_vv.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);

	Gnmx::PsShader *pixelShader = Framework::LoadPsShader("/app0/load-zipfriendly-gnf-sample/shader_p.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache pixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&pixelShader_offsetsTable, Gnm::kShaderStagePs, pixelShader);

	uint32_t fssz = Gnmx::computeVsFetchShaderSize(vertexShader);
	void *fetchShaderPtr = framework.m_garlicAllocator.allocate(fssz, Gnm::kAlignmentOfShaderInBytes);
	uint32_t shaderModifier;
	Gnmx::generateVsFetchShader(fetchShaderPtr, &shaderModifier, vertexShader);

	Gnm::Sampler sampler;
	sampler.init();
	sampler.setXyFilterMode(Gnm::kFilterModePoint, Gnm::kFilterModePoint);
	sampler.setWrapMode(Gnm::kWrapModeClampBorder, Gnm::kWrapModeClampBorder, Gnm::kWrapModeClampBorder);
	sampler.setMipFilterMode(Gnm::kMipFilterModeLinear);

	Gnmx::GnmxGfxContext *gfxc = &commandBuffers[framework.m_backBufferIndex];
	// build the unpacker object once
	void *vmBuffer = framework.m_garlicAllocator.allocate(Gnf::BC7Unpacker::kBC7UnpackerShaderSize, 256);
	Gnf::BC7Unpacker unpacker(vmBuffer);
	Gnm::DrawCommandBuffer &dcb = gfxc->m_dcb;

	// load all the textures
	const char *textureNames[NUM_TEXTURE_TYPES] = { "/app0/assets/zipFrienlyBc7.gnf", "/app0/assets/zipFrienlyBc7.gnf", "/app0/assets/mode5onlyBc7.gnf", "/app0/assets/normalBc7.gnf" };
	Gnm::Texture textures[4];
	for (int iTexture = 0; iTexture < NUM_TEXTURE_TYPES; iTexture++)
	{ 
		Framework::GnfError loadError = Framework::loadTextureFromGnf(&textures[iTexture], textureNames[iTexture], 0, &framework.m_allocators);
		SCE_GNM_UNUSED(loadError);
		if (iTexture == 0) // To show how unpacked data are, base mip is set to zero in ordr to turn off unpacker data from unpacking this texture date. 
			textures[iTexture].setMipLevelRange(0, textures[iTexture].getLastMipLevel());
		// add as many unpacking commands as needed like this
		if (Gnf::BC7Unpacker::isBc7PackedTexture(&textures[iTexture]))
		{
			unpacker.unpack(&textures[iTexture], &dcb );
		}
	}
	// unpack all zipped textures.
	Gnmx::Toolkit::submitAndStall(*gfxc);

	// Main Loop:
	//
	while (!framework.m_shutdownRequested)
	{
		Gnmx::GnmxGfxContext *gfxc = &commandBuffers[framework.m_backBufferIndex]; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.

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

		gfxc->setVsShader(vertexShader, shaderModifier, fetchShaderPtr, &vertexShader_offsetsTable);
 		gfxc->setPsShader(pixelShader, &pixelShader_offsetsTable);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &textures[currentTexture]);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);

        // draw screen quad
		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
		gfxc->drawIndexAuto( 3);
		framework.EndFrame( *gfxc );
    }
	framework.terminate(*gfxc);
    return 0;
}
