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
#include "cull_cb.h"
using namespace sce;

namespace
{
	bool g_followCamera = true;
	bool g_displayOccluder = true;
	bool g_displayBackface = false;
	bool g_displayFrustum  = false;
	Framework::MenuCommandBool menuCommandFollowCamera(&g_followCamera);
	Framework::MenuCommandBool menuCommandDisplayOccluder(&g_displayOccluder);
	Framework::MenuCommandBool menuCommandDisplayBackface(&g_displayBackface);
	Framework::MenuCommandBool menuCommandDisplayFrustum(&g_displayFrustum);
	const Framework::MenuItem menuItem[] =
	{
		{{"Culling Follows Camera", 
		"When true, the compute shader that culls triangles always uses the same matrix as the rendering pipeline. "
		"As the camera moves around the scene, the compute shader uses the new camera matrix to cull triangles. "
	    "When false, the compute shader stops tracking the camera - it continues to use the same matrix it used "
		"on the previous frame, regardless of whether the camera has moved. This helps to illustrate which triangles "
		"are being culled by the compute shader"
		}, &menuCommandFollowCamera},
		{{"Display Occluder",
		"When true, displays world-space occluder quad as magenta spheres and planes"
		},&menuCommandDisplayOccluder},
		{{"Display Backface",
		"When true, displays backface culling polyhedra vertices as red, and base of culling frustum which contains camera as gray"
		},&menuCommandDisplayBackface},
		{{"Display Frustum",
		"When true, displays view frustum used for frustum culling as blue spheres and planes"
		},&menuCommandDisplayFrustum},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
	Framework::GnmSampleFramework framework;
	int vertices;
	int indices;
	int triangles;
	int trianglePairs;
	Gnm::DataFormat dataFormatOfIndexBuffer;
	int sizeOfIndexBufferInBytes;

	void dispatch(Gnmx::GnmxGfxContext *gfxc, unsigned items)
	{
		unsigned groupX = items;
		unsigned groupY = 1;
		if(groupX > 1024)
		{
			groupY = (groupX + 1023) / 1024;
			groupX = 1024;
		}
		gfxc->dispatch(groupX, groupY, 1);
	}
	void dispatchAndStall(Gnmx::GnmxGfxContext *gfxc, unsigned items)
	{
		dispatch(gfxc,items);
		Gnmx::Toolkit::submitAndStall(*gfxc);
	}
	Vector4 IntersectionPoint(const Vector4 &a, const Vector4 &b, const Vector4& c)
	{
		const float f = -dot(a.getXYZ(),cross(b.getXYZ(),c.getXYZ()));

		const Vector3 v1 = (a.getW() * (cross(b.getXYZ(),c.getXYZ())));
		const Vector3 v2 = (b.getW() * (cross(c.getXYZ(),a.getXYZ())));
		const Vector3 v3 = (c.getW() * (cross(a.getXYZ(),b.getXYZ())));

		const Vector3 vec = v1 + v2 + v3;
		return Vector4(vec / f, 1.f);
	}
	Vector4 MakePlane(const Vector4 &a, const Vector4 &b, const Vector4 &c)
	{
		const Vector3 abc = cross((b-a).getXYZ(), (c-b).getXYZ());
		const float d = -dot(abc, a.getXYZ());
		return Vector4(abc, d);
	}
	class Frustum
	{
	public:
		enum { X,Y,Z,W };
		enum 
		{
			kLeft,    // +X
			kRight,   // -X
			kBottom,  // +Y
			kTop,     // -Y
			kNear,    // +Z
			kFar      // -Z
		};
		Vector4 m_plane[6];
		Vector4 m_corner[8];
		void init(const Matrix4 &matrix)
		{
			m_plane[kLeft  ] = matrix.getRow(W) + matrix.getRow(X);
			m_plane[kRight ] = matrix.getRow(W) - matrix.getRow(X);
			m_plane[kBottom] = matrix.getRow(W) + matrix.getRow(Y);
			m_plane[kTop   ] = matrix.getRow(W) - matrix.getRow(Y);
			m_plane[kNear  ] = matrix.getRow(W) + matrix.getRow(Z);
			m_plane[kFar   ] = matrix.getRow(W) - matrix.getRow(Z);
			for(int i = 0; i < 8; ++i)
			{
				const auto x = ((i>>0)&1);
				const auto y = ((i>>1)&1);
				const auto z = ((i>>2)&1);
				m_corner[i] = IntersectionPoint(m_plane[0+x], m_plane[2+y], m_plane[4+z]);				
			}
		}

	};
}


int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);
	framework.m_config.m_displayDebugObjects = true;

	framework.initialize( "DrawIndirect", 
		"Generates 126-bit facing mask and 64-bit bounding box for each 16-triangle chunk of mesh, and then DrawIndirect's 2-triangle chunks that pass facing, frustum, and occluder culling.",
		"For each 16-triangle chunk of mesh, this sample pre-calculates a local-space bounding box and a backface mask for each of 14 view locations outside the mesh. "
		"When the mesh is rendered, the view location polyhedron containing the camera is found, and if none of its vertices can see the triangle chunk, the chunk is backface-culled. "
		"Then, the chunk's bounding box is tested against the view frustum, and if it lies completely outside any of the view frustum planes, it is frustum-culled. "
		"Then, the chunk's bounding box is tested against several world-space quad occluders, and if it lies completely inside any occluder, it is occlusion-culled. "
		"Since these operations are performed on 64 chunks of 16 triangles in parallel, performance is expected to exceed that of working on 64 triangles in parallel."
	);

	framework.appendMenuItems(kMenuItems, menuItem);

	Framework::SimpleMesh baseMesh;
	LoadSimpleMesh(&framework.m_garlicAllocator,&baseMesh,"/app0/assets/crown/crown.mesh");

	// Because the crown mesh was designed for PS3, we tessellate it by 4x to more closely match a PS4 asset.

	Framework::SimpleMesh tessellatedBaseMesh;
	tessellate(&tessellatedBaseMesh,&baseMesh,&framework.m_garlicAllocator);

	// The vast majority of meshes in games do not have uncompressed vertex buffers.
	// Here, we make compressed vertex buffers in order to more closely match production code.

	Framework::Mesh efficientMesh;
	const Gnm::DataFormat dataFormat[] =
	{
		Gnm::kDataFormatR10G10B10A2Unorm,
		Gnm::kDataFormatR8G8B8A8Snorm,
		Gnm::kDataFormatR8G8B8A8Snorm,
		Gnm::kDataFormatR16G16Float
	};
	efficientMesh.compress(dataFormat,&tessellatedBaseMesh,&framework.m_garlicAllocator);
	efficientMesh.m_bufferToModel = Matrix4::scale(Vector3(30,30,30)) * efficientMesh.m_bufferToModel;

	vertices = efficientMesh.m_vertexCount;
	indices = efficientMesh.m_indexCount;
	triangles = indices / 3;
	trianglePairs = (triangles + 1) / 2;

	dataFormatOfIndexBuffer = (efficientMesh.m_indexType == Gnm::kIndexSize32) ? Gnm::kDataFormatR32Uint : Gnm::kDataFormatR16Uint;
	sizeOfIndexBufferInBytes = indices * dataFormatOfIndexBuffer.getBytesPerElement();

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		FastCullConstants *m_fastCullConstants;
		cbMeshInstance *m_constants;
		void *m_culledIndex;
		Gnm::DrawIndexIndirectArgs *m_drawIndirectArgs;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_fastCullConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_fastCullConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Fast Cull Constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Constant Buffer",buffer);
		framework.m_allocators.allocate((void**)&frame->m_culledIndex,SCE_KERNEL_WC_GARLIC,sizeOfIndexBufferInBytes,Gnm::kAlignmentOfBufferInBytes,Gnm::kResourceTypeBufferBaseAddress,"Buffer %d Culled Indices",buffer);
		framework.m_allocators.allocate((void**)&frame->m_drawIndirectArgs,SCE_KERNEL_WC_GARLIC,sizeof(*frame->m_drawIndirectArgs),Gnm::kAlignmentOfIndirectArgsInBytes,Gnm::kResourceTypeBufferBaseAddress,"Buffer %d DrawIndexIndirect Arguments",buffer);
		memset(frame->m_drawIndirectArgs,0,sizeof(Gnm::DrawIndexIndirectArgs));
	}

	Framework::InitNoise(&framework.m_garlicAllocator);
	
	Gnm::Sampler trilinearSampler;
	trilinearSampler.init();
	trilinearSampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	trilinearSampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	int error = 0;

	Framework::CsShader generateBoundingBoxShader, generateCullingShader, fastCullShader;
	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader;

	error = Framework::LoadCsShader(&generateBoundingBoxShader, Framework::ReadFile("/app0/drawindirect-sample/generate_bounding_boxes_c.sb"),&framework.m_allocators);
	error = Framework::LoadCsShader(&generateCullingShader, Framework::ReadFile("/app0/drawindirect-sample/generate_culling_c.sb"),&framework.m_allocators);
	error = Framework::LoadCsShader(&fastCullShader, Framework::ReadFile("/app0/drawindirect-sample/fast_cull_c.sb"),&framework.m_allocators);
	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/drawindirect-sample/main_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/drawindirect-sample/main_p.sb"), &framework.m_allocators);

	Gnm::Buffer vertexBuffer, indexBuffer, boundingBoxMinBuffer, boundingBoxMaxBuffer;

	const unsigned vertexSets = (triangles + 15) / 16;
	const Gnm::DataFormat boundingBoxDataFormat = efficientMesh.m_buffer[0].getDataFormat();
	const unsigned boundingBoxMemorySize = vertexSets * boundingBoxDataFormat.getBytesPerElement();
	void *boundingBoxMinMemory, *boundingBoxMaxMemory;
	framework.m_allocators.allocate(&boundingBoxMinMemory, SCE_KERNEL_WC_GARLIC, boundingBoxMemorySize, 4, Gnm::kResourceTypeBufferBaseAddress, "Bounding Box Min Buffer");
	framework.m_allocators.allocate(&boundingBoxMaxMemory, SCE_KERNEL_WC_GARLIC, boundingBoxMemorySize, 4, Gnm::kResourceTypeBufferBaseAddress, "Bounding Box Max Buffer");

	indexBuffer.initAsDataBuffer(efficientMesh.m_indexBuffer, dataFormatOfIndexBuffer, efficientMesh.m_indexCount);	// texture coordinates
	vertexBuffer.initAsDataBuffer(efficientMesh.m_buffer[0].getBaseAddress(), efficientMesh.m_buffer[0].getDataFormat(), vertices);
	boundingBoxMinBuffer.initAsDataBuffer(boundingBoxMinMemory, boundingBoxDataFormat, vertexSets);
	boundingBoxMaxBuffer.initAsDataBuffer(boundingBoxMaxMemory, boundingBoxDataFormat, vertexSets);

	indexBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // never bound as RWBuffer, so it's GPU coherent
	vertexBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // never bound as RWBuffer, so it's GPU coherent
	boundingBoxMinBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
	boundingBoxMaxBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);

	const float fFov = 60;
	const float fNear = 1;
	const float fFar = 100;
	const float fHalfWidthAtMinusNear = fNear * tanf(fFov*M_PI/360);
	const float fHalfHeightAtMinusNear = fHalfWidthAtMinusNear;
	const float fS = ((float) framework.m_config.m_targetWidth) / framework.m_config.m_targetHeight;
	framework.SetProjectionMatrix(Matrix4::frustum(-fS*fHalfWidthAtMinusNear, fS*fHalfWidthAtMinusNear, -fHalfHeightAtMinusNear, fHalfHeightAtMinusNear, fNear, fFar));
	framework.SetViewToWorldMatrix(inverse(Matrix4::lookAt(Point3(0,0,2), Point3(0,0,-12), Vector3(0,1,0))));

	enum {kBackfaceBuffers = 14};
	Gnm::Buffer backfaceBuffer[kBackfaceBuffers]; // tells us, for a given convex hull face, which triangles are front facing when the eye is closer to that face than all the others.
	uint32_t *backfaceBufferData[kBackfaceBuffers];

	Vector4 mini,maxi;
	efficientMesh.getMinMaxFromAttribute(&mini,&maxi,0);
	const Vector3 n = mini.getXYZ();
	const Vector3 x = maxi.getXYZ();
	const Vector3 c = (x + n) * 0.5f;
	Vector3 s = (x - n) * 0.5f;
	s = Vector3(maxElem(s), maxElem(s), maxElem(s));
	const Vector3 a = c - s * 1.5f;
	const Vector3 b = c + s * 1.5f;
	Vector3 convexHullVertex[kBackfaceBuffers] =
	{
		Vector3(a.getX(),a.getY(),a.getZ()), 
		Vector3(a.getX(),a.getY(),b.getZ()),
		Vector3(a.getX(),b.getY(),a.getZ()),
		Vector3(a.getX(),b.getY(),b.getZ()),
		Vector3(b.getX(),a.getY(),a.getZ()),
		Vector3(b.getX(),a.getY(),b.getZ()),
		Vector3(b.getX(),b.getY(),a.getZ()),
		Vector3(b.getX(),b.getY(),b.getZ()),

		Vector3(c.getX(),c.getY(),a.getZ()),
		Vector3(c.getX(),c.getY(),b.getZ()),
		Vector3(c.getX(),a.getY(),c.getZ()),
		Vector3(c.getX(),b.getY(),c.getZ()),
		Vector3(a.getX(),c.getY(),c.getZ()),
		Vector3(b.getX(),c.getY(),c.getZ()),
	};
	for(auto i = 0; i <kBackfaceBuffers; ++i)
	{
		convexHullVertex[i] = c + mulPerElem(normalize(convexHullVertex[i]-c), s) * 1.5f;
	}
	struct Sorted
	{
		unsigned m_index;
		float m_distanceSquared;
		bool operator<(const Sorted& rhs) const
		{
			return m_distanceSquared < rhs.m_distanceSquared;
		}
	};
	Sorted sorted[kBackfaceBuffers] = 
	{
		{0},{1},{2},{3},{4},{5},{6},{7}
	};

	// the following scope does all the required setup for fast frustum and back-face culling

	{
		Gnmx::GnmxGfxContext *gfxc = &frames[framework.getIndexOfBufferCpuIsWriting()].m_commandBuffer;

		// this is to generate a bounding box for each vertex-set of 64 triangles

		{
			gfxc->reset();
			gfxc->setCsShader(generateBoundingBoxShader.m_shader, &generateBoundingBoxShader.m_offsetsTable);
			FastCullConstants *constants = static_cast<FastCullConstants*>(gfxc->allocateFromCommandBuffer(sizeof(FastCullConstants),Gnm::kEmbeddedDataAlignment4));
			constants->m_triangles = triangles;
			Gnm::Buffer constantBuffer;
			constantBuffer.initAsConstantBuffer(constants,sizeof(*constants));
			constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
			gfxc->setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &constantBuffer);
			gfxc->setBuffers(Gnm::kShaderStageCs, 0, 1, &indexBuffer);
			gfxc->setBuffers(Gnm::kShaderStageCs, 1, 1, &vertexBuffer);
			gfxc->setRwBuffers(Gnm::kShaderStageCs, 0, 1, &boundingBoxMinBuffer);
			gfxc->setRwBuffers(Gnm::kShaderStageCs, 1, 1, &boundingBoxMaxBuffer);
			dispatchAndStall(gfxc, (vertexSets + 3) / 4);
			Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
			boundingBoxMinBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
			boundingBoxMaxBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
		}

		// this is to generate one bit-array buffer for each convex hull face.
		// for each triangle in the mesh, there is a bit which says if this triangle is in any way front facing from an eye position closest to that convex hull face.

		for(auto i = 0; i < kBackfaceBuffers; ++i)
		{
			gfxc->reset();
			gfxc->setCsShader(generateCullingShader.m_shader, &generateCullingShader.m_offsetsTable);
			const unsigned triangleWords = (trianglePairs + 31) / 32;
			const unsigned vertexSetWords = (vertexSets + 31) / 32;
			const unsigned words = triangleWords + vertexSetWords;
			framework.m_allocators.allocate((void**)&backfaceBufferData[i], SCE_KERNEL_WC_GARLIC, words * sizeof(uint32_t), 4, Gnm::kResourceTypeBufferBaseAddress, "Prebaked Backface Buffer %d", i);
			memset(backfaceBufferData[i], 0, words * sizeof(uint32_t));
			backfaceBuffer[i].initAsDataBuffer(backfaceBufferData[i], Gnm::kDataFormatR32Uint, words);
			backfaceBuffer[i].setResourceMemoryType(Gnm::kResourceMemoryTypeGC);
			FastCullConstants *constants = static_cast<FastCullConstants*>(gfxc->allocateFromCommandBuffer(sizeof(FastCullConstants),Gnm::kEmbeddedDataAlignment4));
			constants->m_triangles = triangles;
			constants->m_eye[0] = (Vector4)convexHullVertex[i];
			constants->m_eye[1] = (Vector4)(c + (convexHullVertex[i] - c) * 10.f);
			Gnm::Buffer constantBuffer;
			constantBuffer.initAsConstantBuffer(constants,sizeof(*constants));
			constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
			gfxc->setConstantBuffers(Gnm::kShaderStageCs,0,1,&constantBuffer);
			gfxc->setBuffers(Gnm::kShaderStageCs,0,1,&indexBuffer);
			gfxc->setBuffers(Gnm::kShaderStageCs,1,1,&vertexBuffer);
			gfxc->setRwBuffers(Gnm::kShaderStageCs,0,1,&backfaceBuffer[i]);
			dispatchAndStall(gfxc, (trianglePairs + 63) / 64);
			Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
			backfaceBuffer[i].setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
		}
	}

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		Matrix4 m44LocalToWorld;
		static float ax=0, ay=-2*0.33f, az=0;
		const float fShaderTime = framework.GetSecondsElapsedApparent();	
		ay =  fShaderTime*0.2f*600*0.005f;
		ax = -fShaderTime*0.11f*600*0.0045f;
		m44LocalToWorld = Matrix4::translation(Vector3(0,0,-12)) * Matrix4::rotationZYX(Vector3(ax,ay,az)) * efficientMesh.m_bufferToModel;

		const Matrix4 m44WorldToCam = framework.m_worldToViewMatrix;
		const Matrix4 m44LocToCam = m44WorldToCam * m44LocalToWorld;
		const Matrix4 m44CamToLoc = inverse(m44LocToCam);
		const Matrix4 m44MVP = framework.m_projectionMatrix * m44LocToCam;
		const Matrix4 m44WorldToLocal = inverse(m44LocalToWorld);

		{
			gfxc->pushMarker("fast frustum and back-face culling in compute");
			static Matrix4 m44CullingWorldToCam;
			static Matrix4 viewProjectionMatrixForCulling;
			if(g_followCamera)
			{
				m44CullingWorldToCam = m44WorldToCam;
				viewProjectionMatrixForCulling = framework.m_viewProjectionMatrix;
			}

			const Matrix4 m44CullingLocToCam = m44CullingWorldToCam * m44LocalToWorld;
			const Matrix4 m44CullingCamToLoc = inverse(m44CullingLocToCam);
			const Matrix4 modelViewProjectionMatrixForCulling = viewProjectionMatrixForCulling * m44LocalToWorld;

			Frustum localFrustum;
			localFrustum.init(modelViewProjectionMatrixForCulling);

			const Vector3 cullingCameraPositionInLocalSpace = (m44CullingCamToLoc * Vector4(0,0,0,1)).getXYZ();
			const Vector3 cullingCameraPositionInWorldSpace = (m44LocalToWorld * Vector4(cullingCameraPositionInLocalSpace,1.f)).getXYZ();

			Gnm::Buffer drawIndirectArgsBuffer;
			drawIndirectArgsBuffer.initAsDataBuffer(frame->m_drawIndirectArgs,Gnm::kDataFormatR32Uint,sizeof(Gnm::DrawIndexIndirectArgs) / sizeof(uint32_t));
			drawIndirectArgsBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // it's bound as RWBuffer, so it's GPU-coherent

			Gnm::Buffer culledIndexBuffer;
			culledIndexBuffer.initAsDataBuffer(frame->m_culledIndex,dataFormatOfIndexBuffer,indices);
			culledIndexBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // it's bound as RWBuffer, so it's GPU-coherent

			gfxc->fillData(frame->m_drawIndirectArgs, 0, sizeof(uint32_t), Gnm::kDmaDataBlockingDisable); // zero indices
			gfxc->fillData((void*)((uintptr_t)frame->m_drawIndirectArgs+sizeof(uint32_t)), 1, sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);  // one instance

			gfxc->setCsShader(fastCullShader.m_shader, &fastCullShader.m_offsetsTable);

			frame->m_fastCullConstants->m_triangles = triangles;

			for(auto i = 0; i < kBackfaceBuffers; ++i)
			{
				sorted[i].m_distanceSquared = lengthSqr(cullingCameraPositionInLocalSpace - convexHullVertex[i]);
				sorted[i].m_index = i;
			}
			std::nth_element(sorted,sorted + 2,sorted + kBackfaceBuffers);

			framework.m_backBuffer->m_debugObjects.BeginDebugMesh();
			Frustum worldFrustum;
			worldFrustum.init(viewProjectionMatrixForCulling);

			const Vector4 occluderWorld[5] =
			{
				Vector4(-0.5f,-0.5f,-5,1.f),
				Vector4(0.5f,-0.5f,-5,1.f),
				Vector4(0.5f,0.5f,-5,1.f),
				Vector4(-0.5f,0.5f,-5,1.f),
				Vector4(cullingCameraPositionInWorldSpace,1.0f)
			};
			const Vector4 occluderLocal[5] =
			{
				m44WorldToLocal * occluderWorld[0],
				m44WorldToLocal * occluderWorld[1],
				m44WorldToLocal * occluderWorld[2],
				m44WorldToLocal * occluderWorld[3],
				m44WorldToLocal * occluderWorld[4]
			};

			if(g_displayOccluder)
			{
				const Vector4 color = Vector4(1,0,1,0.25f);
				for(auto i = 0; i < 5; ++i)
				{
					framework.m_backBuffer->m_debugObjects.AddDebugSphere(occluderWorld[i].getXYZ(),0.1f,color,framework.m_viewProjectionMatrix);
				}
				Vector3 a = occluderWorld[0].getXYZ();
				Vector3 b = occluderWorld[1].getXYZ();
				Vector3 c = occluderWorld[2].getXYZ();
				Vector3 d = occluderWorld[3].getXYZ();
				framework.m_backBuffer->m_debugObjects.AddDebugQuad(color,a,b,c,d);
			}
			if(g_displayFrustum && (g_followCamera == false))
			{
				Vector3 corner[8];
				for(int i = 0; i < 8; ++i)
					corner[i] = worldFrustum.m_corner[i].getXYZ();
				for(int i = 4; i < 8; ++i)
					corner[i] = corner[i-4] * 0.75f + corner[i] * 0.25f;
				for(int i = 0; i < 8; ++i)
					framework.m_backBuffer->m_debugObjects.AddDebugSphere(corner[i],0.1f,Vector4(0,1,0,1),framework.m_viewProjectionMatrix);
				const unsigned offset[6][4] =
				{
					{0,2,6,4},
					{1,3,7,5},
					{0,1,5,4},
					{2,3,7,6},
					{0,1,3,2},
					{4,5,7,6},
				};
				for(auto plane = 0; plane < 5; ++plane)
				{
					Vector3 a = corner[offset[plane][0]];
					Vector3 b = corner[offset[plane][1]];
					Vector3 c = corner[offset[plane][2]];
					Vector3 d = corner[offset[plane][3]];
					Vector3 m = (a + b + c + d) * 0.25f;
					const float t = 0.01f;
					a = a * (1.f-t) + m * t;
					b = b * (1.f-t) + m * t;
					c = c * (1.f-t) + m * t;
					d = d * (1.f-t) + m * t;
					framework.m_backBuffer->m_debugObjects.AddDebugQuad(Vector4(0,0,1,0.25f),a,b,c,d);
				}
			}
			if(g_displayBackface)
			{
				for(auto i = 0; i < kBackfaceBuffers; ++i)
				{
					const Vector4 color = i < 3 ? Vector4(1,1,0,1) : Vector4(1,0,0,1);
					const auto j = sorted[i].m_index;
					framework.m_backBuffer->m_debugObjects.AddDebugSphere((m44LocalToWorld * Vector4(convexHullVertex[j],1.f)).getXYZ(), 0.1f, color, framework.m_viewProjectionMatrix);
				}
				const Vector3 point[3] = {
					convexHullVertex[sorted[0].m_index],
					convexHullVertex[sorted[1].m_index],
					convexHullVertex[sorted[2].m_index],
				};
				Framework::DebugVertex debugVertex[3];
				debugVertex[0].m_color = Vector4(1,1,1,0.25f);
				debugVertex[1].m_color = Vector4(1,1,1,0.25f);
				debugVertex[2].m_color = Vector4(1,1,1,0.25f);
				debugVertex[0].m_position = (m44LocalToWorld * Vector4(point[0],1.f)).getXYZ();
				debugVertex[1].m_position = (m44LocalToWorld * Vector4(point[1],1.f)).getXYZ();
				debugVertex[2].m_position = (m44LocalToWorld * Vector4(point[2],1.f)).getXYZ();
				framework.m_backBuffer->m_debugObjects.AddDebugTriangle(debugVertex[0],debugVertex[1],debugVertex[2]);
			}
			framework.m_backBuffer->m_debugObjects.EndDebugMesh(Vector4(1,1,1,1),framework.m_viewProjectionMatrix);

			frame->m_fastCullConstants->m_frustumPlane[0] = localFrustum.m_plane[Frustum::kNear  ];
			frame->m_fastCullConstants->m_frustumPlane[1] = localFrustum.m_plane[Frustum::kLeft  ];
			frame->m_fastCullConstants->m_frustumPlane[2] = localFrustum.m_plane[Frustum::kRight ];
			frame->m_fastCullConstants->m_frustumPlane[3] = localFrustum.m_plane[Frustum::kBottom];
			frame->m_fastCullConstants->m_frustumPlane[4] = localFrustum.m_plane[Frustum::kTop   ];

			frame->m_fastCullConstants->m_occluders = g_displayOccluder ? 1 : 0;
			frame->m_fastCullConstants->m_occluderPlane[0][0] = MakePlane(occluderLocal[4], occluderLocal[0], occluderLocal[1]);
			frame->m_fastCullConstants->m_occluderPlane[0][1] = MakePlane(occluderLocal[4], occluderLocal[1], occluderLocal[2]);
			frame->m_fastCullConstants->m_occluderPlane[0][2] = MakePlane(occluderLocal[4], occluderLocal[2], occluderLocal[3]);
			frame->m_fastCullConstants->m_occluderPlane[0][3] = MakePlane(occluderLocal[4], occluderLocal[3], occluderLocal[0]);
			frame->m_fastCullConstants->m_occluderPlane[0][4] = MakePlane(occluderLocal[0], occluderLocal[1], occluderLocal[2]);

			frame->m_fastCullConstants->m_eye[0] = Vector4(cullingCameraPositionInLocalSpace, 1.f);

			Gnm::Buffer fastCullConstantBuffer;
			fastCullConstantBuffer.initAsConstantBuffer(frame->m_fastCullConstants,sizeof(*frame->m_fastCullConstants));
			fastCullConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

			gfxc->setConstantBuffers(Gnm::kShaderStageCs,0,1,&fastCullConstantBuffer);
			gfxc->setBuffers(Gnm::kShaderStageCs,0,1,&boundingBoxMinBuffer);
			gfxc->setBuffers(Gnm::kShaderStageCs,1,1,&boundingBoxMaxBuffer);
			gfxc->setBuffers(Gnm::kShaderStageCs,2,1,&backfaceBuffer[sorted[0].m_index]);
			gfxc->setBuffers(Gnm::kShaderStageCs,3,1,&backfaceBuffer[sorted[1].m_index]);
			gfxc->setBuffers(Gnm::kShaderStageCs,4,1,&backfaceBuffer[sorted[2].m_index]);
			gfxc->setBuffers(Gnm::kShaderStageCs,5,1,&indexBuffer);
			gfxc->setBuffers(Gnm::kShaderStageCs,6,1,&vertexBuffer);
			gfxc->setRwBuffers(Gnm::kShaderStageCs,0,1,&drawIndirectArgsBuffer);
			gfxc->setRwBuffers(Gnm::kShaderStageCs,1,1,&culledIndexBuffer);
	
			const unsigned oneWavePerThousandTriangles = (vertexSets + 63) / 64;
			dispatch(gfxc, oneWavePerThousandTriangles); // one wavefront per 16x64 (1,024) triangles
			
			Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);
			gfxc->popMarker();
		}

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
		gfxc->popMarker();

		gfxc->setRenderTargetMask(0xF);
		gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
		gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);
		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);
		gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);

		gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
		gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);

		Framework::SetNoiseDataBuffers(*gfxc, Gnm::kShaderStagePs);

		// draw from generated culled index buffer
		efficientMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs );	

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

		gfxc->pushMarker("rasterize compute-backface-culled geometry");

		gfxc->setPrimitiveType(efficientMesh.m_primitiveType);
		gfxc->setIndexSize(efficientMesh.m_indexType);

		gfxc->setBaseIndirectArgs(Gnm::kShaderTypeGraphics, frame->m_drawIndirectArgs);
		gfxc->setIndexBuffer(frame->m_culledIndex);
		gfxc->setIndexCount(triangles*3);

		gfxc->stallCommandBufferParser();
		gfxc->drawIndexIndirect(0);
		
		gfxc->popMarker();

		// Note: calling drawIndexIndirect() and other indirect draw types affect the instance count for subsequent draws, it is required to reset
		// number of instances back to a valid number.
		gfxc->setNumInstances(1);

		const DbgFont::Cell cell ={0,DbgFont::kWhite,15,DbgFont::kGreen,7};
		bufferCpuIsWriting->m_window.deferredPrintf(*gfxc,0,bufferCpuIsWriting->m_window.m_heightInCharacters-1,cell,64,"       of %06d indices were drawn.", triangles * 3);
		framework.displayUnsigned(*gfxc,0,bufferCpuIsWriting->m_window.m_heightInCharacters-1,frame->m_drawIndirectArgs,6);

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
}
