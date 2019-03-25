/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/noise_prep.h"
#include "../framework/frame.h"
#include "std_cbuffer.h"
typedef unsigned int uint;
using namespace sce;

#include "gnmx/lwgfxcontext.h"

// uncomment this if you want to use drawIndirectCountMulti() instead of drawIndexIndirectCountMulti() APIs
#define	USE_INDEXED_MESH

// change the sample name based on the configuration defines above
#ifdef USE_INDEXED_MESH
	#define	SAMPLE_NAME "drawIndexIndirectCountMulti"
#else
	#define	SAMPLE_NAME "drawIndirectCountMulti"
#endif	// USE_INDEXED_MESH

namespace
{
	class SimpVert
	{
	public:
		Vector3Unaligned m_position;
		Vector3Unaligned m_normal;
		Vector4Unaligned m_tangent;
		Vector2Unaligned m_texture;
	};
	Framework::GnmSampleFramework framework;
	int triangleCount;
	const uint32_t kDrawCount = 431; // 2, 19, or 431
}

// create a simple deindexed version of the source mesh, so we can use drawIndirectCountMulti()
#ifndef	USE_INDEXED_MESH
static void createDeindexedMesh(Framework::SimpleMesh *sourceMesh, Framework::SimpleMesh *destMesh)
{
	const uint32_t indexCount = sourceMesh->m_indexCount;
	const bool using32BitIndices = (sourceMesh->m_indexType == Gnm::kIndexSize32);
	void *deindexedVerts = 0;

	// allocate room for all the vertices we need (one per source index)
	framework.m_allocators.allocate(&deindexedVerts, SCE_KERNEL_WC_GARLIC, indexCount * sizeof(SimpVert), Gnm::kAlignmentOfBufferInBytes, Gnm::kResourceTypeGenericBuffer, "Deindexed Vertices");

	// now populate the new vertex data space with the vertices associated with the source indices.
	for (int index=0; index < indexCount; index++) {
		if (using32BitIndices) {
			const uint32_t* indexBuffer = (const uint32_t *)sourceMesh->m_indexBuffer;
			((SimpVert*)deindexedVerts)[index] = ((SimpVert*)sourceMesh->m_vertexBuffer)[indexBuffer[index]];
		} else {
			const uint16_t* indexBuffer = (const uint16_t *)sourceMesh->m_indexBuffer;
			((SimpVert*)deindexedVerts)[index] = ((SimpVert*)sourceMesh->m_vertexBuffer)[indexBuffer[index]];
		}
	}

	// copy the bulk of the mesh structure itself
	*destMesh = *sourceMesh;

	// now configure it to use our new vertex data, and scrub index data references
	destMesh->m_vertexBuffer = deindexedVerts;
	destMesh->m_vertexBufferSize = sizeof(SimpVert) * indexCount;
	destMesh->m_vertexCount = indexCount;
	destMesh->m_indexBuffer = 0;
	destMesh->m_indexBufferSize = 0;
	destMesh->m_indexCount = 0;

	// finalise the buffer format stuff
	Framework::SetMeshVertexBufferFormat(destMesh);
}
#endif	// USE_INDEXED_MESH

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);
	framework.initialize( SAMPLE_NAME, 
		"Sample code for launching multiple draw calls with a single GPU command, whose parameters are sourced from GPU buffers.", 
		"This sample program illustrates how one can issue multiple draw calls with a single Gnm command "
		"each of which behaves like an separate indirect draw call.");

	Framework::SimpleMesh isoSurface;
	LoadSimpleMesh(&framework.m_allocators, &isoSurface, "/app0/assets/isosurf.mesh");
	Framework::scaleSimpleMesh(&isoSurface, 0.8f);
	SCE_GNM_ASSERT(isoSurface.m_primitiveType==Gnm::kPrimitiveTypeTriList && sizeof(SimpVert)==isoSurface.m_vertexStride);

	// default our mesh to be the indexed isoSurface 
	Framework::SimpleMesh *sampleMesh = &isoSurface;;

#ifdef USE_INDEXED_MESH
	triangleCount = isoSurface.m_indexCount/3;
#else
	Framework::SimpleMesh deindexedMesh;
	createDeindexedMesh(&isoSurface, &deindexedMesh);
	sampleMesh = &deindexedMesh;
	triangleCount = deindexedMesh.m_vertexCount/3;
#endif	// USE_INDEXED_MESH

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		cbMeshInstance *m_constants;
#ifdef USE_INDEXED_MESH
		Gnm::DrawIndexIndirectArgs *m_drawIndirectMem;
#else
		Gnm::DrawIndirectArgs *m_drawIndirectMem;
#endif // USE_INDEXED_MESH
	};

	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),Gnm::kEmbeddedDataAlignment4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Constant Buffer",buffer);
		framework.m_allocators.allocate((void**)&frame->m_drawIndirectMem,SCE_KERNEL_WC_GARLIC,kDrawCount*sizeof(*frame->m_drawIndirectMem),Gnm::kAlignmentOfIndirectArgsInBytes,Gnm::kResourceTypeGenericBuffer,"Buffer %d IndirectArgs",buffer);
		memset(frame->m_drawIndirectMem,0,kDrawCount*sizeof(*frame->m_drawIndirectMem));
		const uint32_t kElementsPerDraw = (triangleCount/kDrawCount)*3;
		for(uint32_t iDraw=0; iDraw<kDrawCount; ++iDraw)
		{
			frame->m_drawIndirectMem[iDraw].m_instanceCount = 1;
#ifdef USE_INDEXED_MESH
			frame->m_drawIndirectMem[iDraw].m_indexCountPerInstance = kElementsPerDraw;
			frame->m_drawIndirectMem[iDraw].m_startIndexLocation = kElementsPerDraw*iDraw;
#else
			frame->m_drawIndirectMem[iDraw].m_vertexCountPerInstance = kElementsPerDraw;
			frame->m_drawIndirectMem[iDraw].m_startVertexLocation = kElementsPerDraw*iDraw;
#endif	// USE_INDEXED_MESH
		}
	}

	// allocate memory for an indirect count
	uint32_t *indirectCountAddr = NULL;
	framework.m_allocators.allocate((void**)&indirectCountAddr, SCE_KERNEL_WC_GARLIC, sizeof(uint32_t), Gnm::kAlignmentOfBufferInBytes, Gnm::kResourceTypeGenericBuffer, "Indirect Count");
	*indirectCountAddr = kDrawCount;

	Framework::InitNoise(&framework.m_allocators);
	
	Gnm::Sampler trilinearSampler;
	trilinearSampler.init();
	trilinearSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	trilinearSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	void* fetchShader;
	Gnmx::VsShader *vertexShader = Framework::LoadVsMakeFetchShader(&fetchShader, "/app0/drawindirectmulti-sample/main_vv.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);

	Gnmx::PsShader *pixelShader = Framework::LoadPsShader("/app0/drawindirectmulti-sample/main_p.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache pixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&pixelShader_offsetsTable, Gnm::kShaderStagePs, pixelShader);

	const float fFov = 60;
	const float fNear = 1;
	const float fFar = 100;
	const float fHalfWidthAtMinusNear = fNear * tanf(fFov*M_PI/360);
	const float fHalfHeightAtMinusNear = fHalfWidthAtMinusNear;
	const float fS = ((float) framework.m_config.m_targetWidth) / framework.m_config.m_targetHeight;
	framework.SetProjectionMatrix(Matrix4::frustum(-fS*fHalfWidthAtMinusNear, fS*fHalfWidthAtMinusNear, -fHalfHeightAtMinusNear, fHalfHeightAtMinusNear, fNear, fFar));
	framework.SetViewToWorldMatrix(inverse(Matrix4::lookAt(Point3(0,0,2), Point3(0,0,-12), Vector3(0,1,0))));

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

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

		gfxc->pushMarker("prepare shader inputs");

		gfxc->setVsShader(vertexShader, 0, fetchShader, &vertexShader_offsetsTable);
		gfxc->setPsShader(pixelShader, &pixelShader_offsetsTable);

		Framework::SetNoiseDataBuffers(*gfxc, Gnm::kShaderStagePs);

		Matrix4 m44LocalToWorld;
		static float ax=0, ay=-2*0.33f, az=0;
		const float fShaderTime = framework.GetSecondsElapsedApparent();	
		ay =  fShaderTime*0.2f*600*0.005f;
		ax = -fShaderTime*0.11f*600*0.0045f;
		m44LocalToWorld = Matrix4::rotationZYX(Vector3(ax,ay,az));
		m44LocalToWorld.setCol3(Vector4(0,0,-12,1));	

		const Matrix4 m44WorldToCam = framework.m_worldToViewMatrix;
		const Matrix4 m44LocToCam = m44WorldToCam * m44LocalToWorld;
		const Matrix4 m44CamToLoc = inverse(m44LocToCam);
		const Matrix4 m44MVP = framework.m_projectionMatrix * m44LocToCam;
		const Matrix4 m44WorldToLocal = inverse(m44LocalToWorld);

		sampleMesh->SetVertexBuffer(*gfxc, Gnm::kShaderStageVs );

		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(frame->m_constants,sizeof(*frame->m_constants));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

		const Vector4 vLpos_world(30,30,50,1);
		const Vector4 vLpos2_world(50,50,-60,1);
		frame->m_constants->g_vCamPos = m44CamToLoc * Vector4(0,0,0,1);
		frame->m_constants->g_vLightPos = m44WorldToLocal * vLpos_world;
		frame->m_constants->g_vLightPos2 = m44WorldToLocal * vLpos2_world;
		frame->m_constants->g_mWorld = transpose(m44LocalToWorld);
		frame->m_constants->g_mWorldViewProjection = transpose(m44MVP);
		frame->m_constants->g_fTime = fShaderTime;

		gfxc->popMarker();

		// Note: let's wait for everything to finish up.. It makes analysing trace data much more straightforward
		Gnmx::Toolkit::synchronizeGraphicsToCompute(&gfxc->m_dcb);

		gfxc->pushMarker("draw call");
		{
			gfxc->setPrimitiveType(isoSurface.m_primitiveType);
			gfxc->setBaseIndirectArgs(Gnm::kShaderTypeGraphics, frame->m_drawIndirectMem);

#ifdef USE_INDEXED_MESH
			gfxc->setIndexSize(isoSurface.m_indexType);
			gfxc->setIndexBuffer(isoSurface.m_indexBuffer);
			gfxc->setIndexCount(triangleCount*3);
#endif	// USE_INDEXED_MESH

			gfxc->stallCommandBufferParser();

			// update our indirect count (in GPU memory)
//			*indirectCountAddr = (*indirectCountAddr % kDrawCount) + 1;
			*indirectCountAddr = kDrawCount;

#ifdef USE_INDEXED_MESH
			gfxc->drawIndexIndirectCountMulti(0, kDrawCount, indirectCountAddr);
#else
			gfxc->drawIndirectCountMulti(0, kDrawCount, indirectCountAddr);
#endif	// USE_INDEXED_MESH
		}
		gfxc->popMarker();

		// Note: calling draw[Index]IndirectCountMulti() and other indirect draw types affect the instance count for subsequent draws, it is required to reset
		// number of instances back to a valid number.
		gfxc->setNumInstances(1);

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
    return 0;
}
