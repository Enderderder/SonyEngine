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
using namespace sce;

#define DYNAMIC_CONSTANT_BUFFER

namespace
{
	class vertexBufferDescriptor 
	{
	public:
		const char   *m_semanticName;
		const uint8_t m_semanticIndex;
		uint8_t m_reserved0[7];
	};

	// generate vertex input remap table based on given vertex shader program and vertex buffer description
	// caller needs to allocate memory for remap table according to the number of vertex buffer descriptors
	static void generateVertexInputRemapTable(const Shader::Binary::Program* vsp, const vertexBufferDescriptor *vertexBufferDescs, uint32_t numVertexBufferDescs, uint32_t *remapTable)
	{
		SCE_GNM_ASSERT(vsp);
		SCE_GNM_ASSERT(vertexBufferDescs);
		SCE_GNM_ASSERT(numVertexBufferDescs);
		SCE_GNM_ASSERT(remapTable);

		// loop through all vertex input attributes and check if each attribute has corresponding vertex buffer descriptor
		for(uint8_t i = 0; i < vsp->m_numInputAttributes; ++i)
		{
			Shader::Binary::Attribute* attrib = vsp->m_inputAttributes + i;

			Shader::Binary::PsslSemantic psslSemantic = (Shader::Binary::PsslSemantic)attrib->m_psslSemantic;
			// An input with system semantic does not need a slot in the remap table; ignore it
			if(psslSemantic != sce::Shader::Binary::kSemanticUserDefined)
				// Any semantic other than user defined ones are system
				continue;

			// look for the corresponding vertex buffer semantic name
			bool found = false;
			for ( uint32_t j = 0; j < numVertexBufferDescs; j++ )
			{
				if (strcmp((const char*)attrib->getSemanticName(),  vertexBufferDescs[j].m_semanticName) == 0 &&
					attrib->m_semanticIndex == vertexBufferDescs[j].m_semanticIndex)
				{
					found = true;
					break;
				}
			}

			// error if there is no verterx buffer descriptor corresponding to this input attribute
			if (!found)
			{
				printf("Error: vertex shader input semantic %s%d was not found in the vertex buffer semantic name list\n", (const char*)attrib->getSemanticName(), attrib->m_semanticIndex);
				SCE_GNM_ERROR("Error: vertex shader input semantic %s%d was not found in the vertex buffer semantic name list\n", (const char*)attrib->getSemanticName(), attrib->m_semanticIndex);
				exit(1);
			}
		}

		// loop through all vertex buffer descriptors and fill out remap table entries
		for ( uint32_t i = 0; i < numVertexBufferDescs; i++ )
		{
			Shader::Binary::Attribute *inputAttribute = vsp->getInputAttributeBySemanticNameAndIndex( 
				vertexBufferDescs[i].m_semanticName,
				vertexBufferDescs[i].m_semanticIndex);

			if (inputAttribute)
				remapTable[i] = inputAttribute->m_resourceIndex;
			else
				remapTable[i] = 0xFFFFFFFF; // unused
		}
	}

	// PSSL semantic match function
	// by using parameter info section of SB file, fixes pixel shader input semantic table, according to the corresponding vertex output
	static void matchVsPsSemantics(const Shader::Binary::Program* vsProgram, const Shader::Binary::Program* psProgram, const Gnmx::VsShader *vtxData, Gnmx::PsShader *pixData)
	{
		// get vertex export table
		uint32_t numExports = vtxData->m_numExportSemantics;
		const Gnm::VertexExportSemantic *vtxExportTable = vtxData->getExportSemanticTable();

		// construct a table of vertex exports for each vertex output attribute
		uint8_t numVsOutputAttributes = vsProgram->m_numOutputAttributes;
		uint8_t *vsExports = new uint8_t[numVsOutputAttributes];
		uint8_t currentExport = 0;
		for ( uint8_t i = 0; i < numVsOutputAttributes; i++ )
		{
			const Shader::Binary::Attribute *a = vsProgram->m_outputAttributes + i;
			Shader::Binary::PsslSemantic vsInputPsslSemantic = (Shader::Binary::PsslSemantic)a->m_psslSemantic;

			// ignore an attribute with system semantic
			// ASSUMPTION: Any vertex output with semantic system does not have an entry in the vertex export table
			if (vsInputPsslSemantic != sce::Shader::Binary::kSemanticUserDefined)
				// Any semantic other than user defined ones are system
				vsExports[i] = 0xFF; 
			else
				vsExports[i] = currentExport++;
		}
		SCE_GNM_ASSERT_MSG( currentExport == numExports, "Not all entries in the export table were covered" );

		// get PS input table
		uint8_t numPsInputs = pixData->m_numInputSemantics;
		SCE_GNM_UNUSED(numPsInputs);
		const Gnm::PixelInputSemantic *psInputTable = pixData->getPixelInputSemanticTable();

		printf("------------------------------------------------\n");
		printf("Debug output from matchVsPsSemantics()\n");
		printf("Pixel shader input table was fixed.\n");

		// loop through PS input attributes and fix each entry in the PS input table based on sematic name
		uint8_t numPsInputAttributes = psProgram->m_numInputAttributes;
		uint8_t currentPsInput = 0;
		for( uint32_t i = 0; i < numPsInputAttributes; i++ )
		{
			// get the PS attribute
			const Shader::Binary::Attribute *psInputAttribute = psProgram->m_inputAttributes + i;

			// ignore an attribute with system semantic 
			// ASSUMPTION: Any pixel input with system semantic does not have an entry in the PS input table
			Shader::Binary::PsslSemantic psInputPsslSemantic = (Shader::Binary::PsslSemantic)psInputAttribute->m_psslSemantic;
			if (psInputPsslSemantic != sce::Shader::Binary::kSemanticUserDefined)
				// Any semantic other than user defined ones are system
				continue;	

			// find a corresponding VS attribute info
			uint32_t vsOutputAttributeIndex = 0xFFFFFFFF;

			// if user defined semantic, get that by name 
			//vsOutputAttributeIndex = vsProgram->getOutputAttributeIndexBySemanticNameAndIndex( (const char*)psInputAttribute->getSemanticName(), psInputAttribute->m_semanticIndex ); 
			for ( uint32_t j = 0; j < vsProgram->m_numOutputAttributes; j++ )
				if (strcmp( (char*)vsProgram->m_outputAttributes[j].getSemanticName(), (const char*)psInputAttribute->getSemanticName() ) == 0  && 
					vsProgram->m_outputAttributes[j].m_semanticIndex == psInputAttribute->m_semanticIndex )
					vsOutputAttributeIndex = j;

			if ( vsOutputAttributeIndex == 0xFFFFFFFF )
			{
				printf("Error: SB Vertex Program does not have corresponding output attribute for a pixel input %s \n", psInputAttribute->getName());
				SCE_GNM_ERROR("SB Vertex Program does not have corresponding output attribute for a pixel input %s \n", psInputAttribute->getName());
				exit(-1);
			}

			// get the resource index from the attribute and translate it to a vs export
			uint8_t vsExport = vsExports[vsOutputAttributeIndex];
			SCE_GNM_ASSERT( vsExport != 0xFF );

			// look for the corresponding entry in the vertex export table, and determine the new PS input semantic from it
			uint16_t newPsInputSemantic = 0xFFFF;
			for ( uint8_t j = 0; j < numExports; j++ )
			{
				if ( vtxExportTable[j].m_outIndex == vsExport )
				{
					newPsInputSemantic = vtxExportTable[j].m_semantic;
				}
			}
			if ( newPsInputSemantic == 0xFFFF )
			{
				printf("Error: Vertex export table does not have corresponding export for the vertex output attribute %d \n", 
					vsOutputAttributeIndex );
				SCE_GNM_ERROR("Vertex export table does not have corresponding export for the vertex output attribute %d \n", 
					vsOutputAttributeIndex);
				exit(-1);
			}

			// fix an entry in the PS input table
			SCE_GNM_ASSERT( currentPsInput < numPsInputs );
			Gnm::PixelInputSemantic psInput = psInputTable[currentPsInput];
			uint16_t origPsInputSemantic = psInput.m_semantic;
			psInput.m_semantic = newPsInputSemantic;
			memcpy(const_cast<Gnm::PixelInputSemantic*>(&psInputTable[currentPsInput]), &psInput, sizeof(Gnm::PixelInputSemantic)); // const_cast for now to workaround constness

			printf( "PSSL: PsIn %10s%d| IL: PsIn v%2d VsOut v%2d| Fixed: PsIn %3d -> %3d: \n", 
				psInputAttribute->getSemanticName(), 
				psInputAttribute->m_semanticIndex, 
				psInputAttribute->m_resourceIndex, 
				vsOutputAttributeIndex,
				origPsInputSemantic,
				psInputTable[currentPsInput].m_semantic );
			currentPsInput++;
		}

		printf("------------------------------------------------\n");

		delete [] vsExports;
	}
	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.initialize( "PSSL Semantic Match", 
		"Sample code to illustrate how a vertex and pixel shader's input/output semantics can be matched at runtime.",
		"This sample program analyzes a vertex and pixel shader binary, in order to then match the output semantics of "
		"the vertex shader with the input semantics of the pixel shader.");

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

	// load SB programs from file
	Shader::Binary::Program vsProgram;
	Shader::Binary::Program psProgram;

	Gnmx::VsShader *vertexShader = Framework::LoadVertexProgram(&vsProgram, "/app0/pssl-semantic-match-sample/shader_vv.sb", &framework.m_garlicAllocator);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);

//#define TEST_REDUCED_PIXEL_SHADER
#ifndef TEST_REDUCED_PIXEL_SHADER
	Gnmx::PsShader *pixelShader = Framework::LoadPixelProgram(&psProgram, "/app0/pssl-semantic-match-sample/shader_p.sb", &framework.m_garlicAllocator);
#else
	// test for reduced pixel shader to use less number of input attributes
	Gnmx::PsShader *pixelShader = Framework::LoadPixelProgram(&psProgram, "/app0/pssl-semantic-match-sample/shader_reduced_p.sb", &framework.m_garlicAllocator);
#endif
	Gnmx::InputOffsetsCache pixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&pixelShader_offsetsTable, Gnm::kShaderStagePs, pixelShader);

//#define SAVE_SB
#ifdef SAVE_SB
	Gnmx::Toolkit::saveShaderProgramToFile(&vsProgram, "/hostapp/myShader_vv.sb");
	Gnmx::Toolkit::saveShaderProgramToFile(&psProgram, "/hostapp/myShader_p.sb");
#endif

	matchVsPsSemantics(&vsProgram, &psProgram, vertexShader, pixelShader);

#ifdef DYNAMIC_CONSTANT_BUFFER
	// initialize runtime helper nodes for shader binary element query
	Gnmx::Toolkit::ElementNode *vsNodeList = Gnmx::Toolkit::InitElementNodes(&vsProgram);
	//Toolkit::ElementNode *psNodeList = Toolkit::InitElementNodes(&psProgram);

	// query constant buffer resource information from Program
	sce::Shader::Binary::Buffer* sphereShaderCB = vsProgram.getBufferResourceByName( "cbSphereShaderConstants" );
	uint32_t cbSize = sphereShaderCB->m_strideSize;

	Gnmx::Toolkit::ElementNode *mvNode =  Gnmx::Toolkit::GetElementNodeWithName(vsNodeList, "cbSphereShaderConstants", "m_modelView");
	Gnmx::Toolkit::ElementNode *mvpNode = Gnmx::Toolkit::GetElementNodeWithName(vsNodeList, "cbSphereShaderConstants", "m_modelViewProjection");
	Gnmx::Toolkit::ElementNode *lpNode =  Gnmx::Toolkit::GetElementNodeWithName(vsNodeList, "cbSphereShaderConstants", "m_lightPosition");
#endif

	Gnm::Texture textures[2];
    Framework::GnfError loadError = Framework::kGnfErrorNone;
	loadError = Framework::loadTextureFromGnf(&textures[0], "/app0/assets/icelogo-color.gnf", 0, &framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );
	loadError = Framework::loadTextureFromGnf(&textures[1], "/app0/assets/icelogo-normal.gnf", 0, &framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare it as read-only.

	Gnm::Sampler sampler;
	sampler.init();
	sampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	sampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	Framework::SimpleMesh simpleMesh;
	BuildTorusMesh(&framework.m_allocators, "Torus", &simpleMesh, 0.8f, 0.2f, 64, 32, 4, 1);

	//
	// Set up a fetch shader with vertex input remap table:
	//

	// We know that in our sphere mesh our vertex buffers are laid out as such:
	// 0: POSITION
	// 1: NORMAL
	// 2: TANGENT
	// 3: TEXCOORD0

	const vertexBufferDescriptor vertexBufferDescs[] = {
		{"POSITION", 0},
		{"NORMAL",	 0},
		{"TANGENT",  0},
		{"TEXCOORD", 0},
	};
	enum {kVertexBufferDescs = sizeof(vertexBufferDescs)/sizeof(vertexBufferDescriptor)};

	uint32_t *remapTable;
	framework.m_allocators.allocate((void**)&remapTable, SCE_KERNEL_WB_ONION, kVertexBufferDescs * sizeof(uint32_t), 4, Gnm::kResourceTypeGenericBuffer, "Vertex Input Remap Table");
	generateVertexInputRemapTable(&vsProgram, vertexBufferDescs, kVertexBufferDescs, remapTable);

	// In this sample, our remap table would result in
	// 0: POSITION  -> 2
	// 1: NORMAL    -> 0
	// 2: TANGENT   -> 1
	// 3: TEXCOORD0 -> 3

	void* fetchShader;
	framework.m_allocators.allocate(&fetchShader, SCE_KERNEL_WC_GARLIC, Gnmx::computeVsFetchShaderSize(vertexShader), Gnm::kAlignmentOfFetchShaderInBytes, Gnm::kResourceTypeFetchShaderBaseAddress, "Fetch Shader");
	uint32_t shaderModifier;
	Gnmx::generateVsFetchShader(fetchShader, &shaderModifier, vertexShader, remapTable, kVertexBufferDescs);

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

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

		gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
		gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLess);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);
		gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);

		gfxc->setVsShader(vertexShader, shaderModifier, fetchShader, &vertexShader_offsetsTable);
		gfxc->setPsShader(pixelShader, &pixelShader_offsetsTable);
		simpleMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

#ifdef DYNAMIC_CONSTANT_BUFFER
		// update the constant buffer
		char *sphereConst = (char*)gfxc->allocateFromCommandBuffer( cbSize, Gnm::kEmbeddedDataAlignment4 );
		const float radians = framework.GetSecondsElapsedApparent() * 0.5f;
		const Matrix4 m = Matrix4::rotationZYX(Vector3(radians,radians,0.f));
		Matrix4 mv = transpose(framework.m_worldToViewMatrix*m);
		Matrix4 mvp = transpose(framework.m_viewProjectionMatrix*m);
		Vector3 lp = framework.getLightPositionInViewSpace().getXYZ();

		memcpy(sphereConst + mvNode->m_element->m_byteOffset,  &mv,  mvNode->m_element->m_size);
		memcpy(sphereConst + mvpNode->m_element->m_byteOffset, &mvp, mvpNode->m_element->m_size);
		memcpy(sphereConst + lpNode->m_element->m_byteOffset,  &lp,  lpNode->m_element->m_size);

		Gnm::Buffer sphereConstBuffer;
		sphereConstBuffer.initAsConstantBuffer(sphereConst, cbSize);
		sphereConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, sphereShaderCB->m_resourceIndex, 1, &sphereConstBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, sphereShaderCB->m_resourceIndex, 1, &sphereConstBuffer);
#else // DYNAMIC_CONSTANT_BUFFER
		cbSphereShaderConstants *sphereConst = static_cast<cbSphereShaderConstants*>( gfxc->allocateFromCommandBuffer( sizeof(cbSphereShaderConstants), Gnm::kEmbeddedDataAlignment4 ) );
		const float radians = framework.GetSecondsElapsedApparent() * 0.5f;
		const Matrix4 m = Matrix4::rotationZYX(Vector3(radians,radians,0.f));
		sphereConst->m_modelView = transpose(framework.m_worldToViewMatrix*m);
		sphereConst->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*m);
		sphereConst->m_lightPosition = (framework.m_worldToViewMatrix * Point3(-1.5,4,9)).getXYZ();

		Gnm::Buffer sphereConstBuffer;
		sphereConstBuffer.initAsConstantBuffer( sphereConst, sizeof(*sphereConst) );
		sphereConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &sphereConstBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &sphereConstBuffer);
#endif // DYNAMIC_CONSTANT_BUFFER

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);
		
		gfxc->setPrimitiveType(simpleMesh.m_primitiveType);
		gfxc->setIndexSize(simpleMesh.m_indexType);
		gfxc->drawIndex(simpleMesh.m_indexCount, simpleMesh.m_indexBuffer);
		framework.EndFrame(*gfxc);
	}

#ifdef DYNAMIC_CONSTANT_BUFFER
	// release runtime helper class for shader binary element query
	Gnmx::Toolkit::ReleaseElementNodes(vsNodeList);
	//Toolkit::ReleaseElementNodes(psNodeList);
#endif

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
    return 0;
}
