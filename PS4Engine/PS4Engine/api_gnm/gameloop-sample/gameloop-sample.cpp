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
#include "flatShader.h"
using namespace sce;

namespace 
{
	Framework::Random random = {1 << 16};
	float aspect;
	Matrix4 projection;

	struct FlatVertex
	{
		int8_t m_x;
		int8_t m_y;
	};

	void buildFlatMesh(Gnmx::Toolkit::Allocators *allocators, Framework::SimpleMesh* mesh, const FlatVertex *vertex, uint32_t vertexCount, const uint16_t *index, uint32_t indexCount, const char *name)
	{
		mesh->m_vertexCount = vertexCount;
		mesh->m_vertexStride = sizeof(FlatVertex);
		mesh->m_vertexAttributeCount = 1;
		mesh->m_indexCount = indexCount;
		mesh->m_indexType = Gnm::kIndexSize16;
		mesh->m_primitiveType = Gnm::kPrimitiveTypeTriList;

		mesh->m_vertexBufferSize = mesh->m_vertexCount * sizeof(FlatVertex);
		mesh->m_indexBufferSize = mesh->m_indexCount * sizeof(uint16_t);

		allocators->allocate((void**)&mesh->m_vertexBuffer, SCE_KERNEL_WC_GARLIC, mesh->m_vertexBufferSize, Gnm::kAlignmentOfBufferInBytes, Gnm::kResourceTypeVertexBufferBaseAddress, "%s Vertex Buffer", name);
		allocators->allocate((void**)&mesh->m_indexBuffer,  SCE_KERNEL_WC_GARLIC, mesh->m_indexBufferSize , Gnm::kAlignmentOfBufferInBytes, Gnm::kResourceTypeIndexBufferBaseAddress, "%s Index Buffer", name);

		mesh->m_buffer[0].initAsVertexBuffer(mesh->m_vertexBuffer, Gnm::kDataFormatR8G8Snorm, sizeof(FlatVertex), mesh->m_vertexCount);
		mesh->m_buffer[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
		mesh->m_vertexAttributeCount = 1;

		std::copy(vertex, vertex + vertexCount, static_cast<FlatVertex*>(mesh->m_vertexBuffer));
		std::copy(index, index + indexCount, static_cast<uint16_t*>(mesh->m_indexBuffer));
	}

	// one 3D toroid

	struct ICEToroid
	{
		Matrix4 m_localToWorld;
		Matrix4 m_worldToLocal;
		Vector2 m_position;
		Vector2 m_velocity;
		Vector4 m_color;
		Vector3 m_axis;
		float m_angle;
		float m_angularVelocity;
		float m_scale;
		float m_innerRadius;
		float m_outerRadius;
		float m_innerSphereRadiusSquared;
		float m_outerSphereRadiusSquared;
		void recalculateMatrices()
		{
			m_localToWorld = Matrix4::translation(Vector3(m_position, 0.f)) * Matrix4::rotation(m_angle, m_axis) * Matrix4::scale(Vector3(m_scale)); // * iceToroid[i].m_angle;
			m_worldToLocal = inverse(m_localToWorld);
		}
		static float sqr(float a) {return a * a;}
		bool pointIsInside(const Vector2 &point) const
		{
			const Vector2 delta = point - m_position;
			const float distanceToCenterSquared = dot(delta, delta);
			if(distanceToCenterSquared < m_innerSphereRadiusSquared)
				return false;
			if(distanceToCenterSquared > m_outerSphereRadiusSquared)
				return false;
			const Vector4 local = m_worldToLocal * Point3(point, 0.f);
			const float x2 = sqr(local.getX());
			const float y2 = sqr(local.getY());
			const float z2 = sqr(local.getZ());
			const float a = m_outerRadius;
			const float b = m_innerRadius;
			const float a2 = sqr(a);
			const float b2 = sqr(b);
			const float answer = sqr(x2 + y2 + z2 - (a2 + b2)) - 4 * a * b * (b2 - z2);
			return answer < 0.f;
		}
		void randomize()
		{
			m_scale = random.GetFloat(0.1f,0.5f);
			m_outerRadius = 0.08f;
			m_innerRadius = 0.02f;
			const float innerSphereRadius = (m_outerRadius - m_innerRadius) * m_scale;
			const float outerSphereRadius = (m_outerRadius + m_innerRadius) * m_scale;
			m_innerSphereRadiusSquared = innerSphereRadius * innerSphereRadius;
			m_outerSphereRadiusSquared = outerSphereRadius * outerSphereRadius;
			const float r = m_scale * (m_outerRadius + m_innerRadius);
			const Vector2 source[4] =
			{
				Vector2(      -r, random.GetFloat(1.f)),
				Vector2(aspect+r, random.GetFloat(1.f)),
				Vector2(random.GetFloat(aspect),    -r),
				Vector2(random.GetFloat(aspect),   1+r)
			};
			const Vector2 destination[4] =
			{
				Vector2(      -r, random.GetFloat(1.f)),
				Vector2(aspect+r, random.GetFloat(1.f)),
				Vector2(random.GetFloat(aspect),    -r),
				Vector2(random.GetFloat(aspect),   1+r)
			};
			const uint32_t side = random.GetUint() & 3;
			m_position = source[side];
			m_velocity = normalize(destination[side^1] - source[side]) * 0.1f;
			m_angle = random.GetFloat(0.f,3.f);
			m_axis = random.GetUnitVector3();
			m_angularVelocity = random.GetFloat(0.4f, 0.8f);
			m_color = Vector4(random.GetFloat(1.f), random.GetFloat(1.f), random.GetFloat(1.f), random.GetFloat(1.f));
			recalculateMatrices();
		}
		uint8_t m_reserved0[4];
	};

	struct FlatObject
	{
		Matrix4 m_localToWorld;
		Vector2 m_oldPosition;
		Vector2 m_position;
		Vector2 m_velocity;
		float m_angle;
		float m_angularVelocity;
		float m_scale;
		float m_life;
		void recalculateMatrices()
		{
			const Vector3 position = Vector3(m_position.getX(), m_position.getY(), 0.f);
			m_localToWorld = Matrix4::translation(position) * Matrix4::rotationZ(m_angle) * Matrix4::scale(Vector3(m_scale));
		}
		uint8_t m_reserved0[8];
	};

	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.m_config.m_htile = true;
	framework.m_config.m_cameraControls = false;
	framework.m_config.m_secondsAreMeaningful = false;

	framework.initialize( "Game Loop", 
		"Sample code to illustrate best practices for a standard game loop.",
		"The mini-game 'ICE Toroids' attempts to illustrate best practices.");

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

	int error = 0;

	Framework::VsShader vertexShader, flatVertexShader; 
	Framework::PsShader pixelShader, flatPixelShader; 

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/gameloop-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadVsMakeFetchShader(&flatVertexShader, Framework::ReadFile("/app0/gameloop-sample/flatShader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/gameloop-sample/shader_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&flatPixelShader, Framework::ReadFile("/app0/gameloop-sample/flatShader_p.sb"), &framework.m_allocators);

	Gnm::Texture textures[2];
    Framework::GnfError loadError = Framework::kGnfErrorNone;
	loadError = Framework::loadTextureFromGnf(&textures[0], "/app0/assets/icelogo-color.gnf", 0, &framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );
	loadError = Framework::loadTextureFromGnf(&textures[1], "/app0/assets/icelogo-normal.gnf", 0, &framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we won't bind this as RWTexture, so it's OK to declare read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we won't bind this as RWTexture, so it's OK to declare read-only.

	Gnm::Sampler sampler;
	sampler.init();
	sampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
	sampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	aspect = (float)framework.m_config.m_targetWidth / framework.m_config.m_targetHeight;
	projection = Matrix4::orthographic(0.f, aspect, 1.f, 0.f, -1.f, 1.f);

	Framework::SimpleMesh torusMesh;
	BuildTorusMesh(&framework.m_allocators, "Torus", &torusMesh, 0.08f, 0.02f, 64, 32, 4, 1);

	Framework::SimpleMesh shipMesh;
	{
		const FlatVertex vertex[4] = {{0, 100}, {-127, 127}, {0, -127}, {127, 127}};
		const uint16_t index[6] = {0, 1, 2, 2, 3, 0};
		buildFlatMesh(&framework.m_allocators, &shipMesh, vertex, 4, index, 6, "Ship");
	}

	Framework::SimpleMesh missileMesh;
	{
		const FlatVertex vertex[4] = {{-8, -31}, {8, -31}, {8, 31}, {-8, 31}};
		const uint16_t index[6] = {0, 1, 2, 2, 0, 3};
		buildFlatMesh(&framework.m_allocators, &missileMesh, vertex, 4, index, 6, "Missile");
	}

	// one 2D-looking thing that prances around the game world

	enum {kMaximumFlatObjects = 32};
	FlatObject flatObject[kMaximumFlatObjects];
	for(uint32_t i=0; i<kMaximumFlatObjects; ++i)
	{
		flatObject[i].m_position = Vector2(aspect * 0.5f, 0.5f);
		flatObject[i].m_velocity = Vector2(0.f, 0.f);
		flatObject[i].m_angle = 0.f;
		flatObject[i].m_angularVelocity = 0.f;
		flatObject[i].m_scale = 1.f / 80.f;
		flatObject[i].m_life = 0.f;
	}

	FlatObject *ship = &flatObject[0];
	ship->m_life = 100000.f;
	float gunHeat = 0.f;

	// one group of 2D-looking things that shares the same mesh

	struct FlatObjectGroup
	{
		Framework::SimpleMesh* m_mesh;
		uint32_t m_begin;
		uint32_t m_end;
	};
	enum {kFlatObjectGroups = 2};
	FlatObjectGroup flatObjectGroup[kFlatObjectGroups] =
	{
		{&shipMesh, 0, 1},
		{&missileMesh, 1, kMaximumFlatObjects}
	};

	enum {kToroids = 32};
	ICEToroid iceToroid[kToroids];
	uint32_t iceToroids = kToroids - 4;

	for(uint32_t i = 0; i < iceToroids; ++i)
	{
		iceToroid[i].randomize();
	}

	float oldT = framework.GetSecondsElapsedApparent();
	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		const float t = framework.GetSecondsElapsedApparent();
		const float dt = t - oldT;
		oldT = t;

		// control step

		if(length(framework.m_controllerContext.getLeftStick()) > 0.1f)
		{
			ship->m_angle = atan2f(framework.m_controllerContext.getLeftStick().getX(), -framework.m_controllerContext.getLeftStick().getY());
			ship->m_velocity += framework.m_controllerContext.getLeftStick() * dt * 4.f;
			const float velocity = length(ship->m_velocity);
			if(velocity > 1.f)
				ship->m_velocity *= 1.f / velocity;
		}
		else
		{
			ship->m_velocity *= powf(0.05f, dt);
		}
		if(length(framework.m_controllerContext.getRightStick()) > 0.1f && gunHeat <= 0.f)
		{
			for(uint32_t i = flatObjectGroup[1].m_begin; i != flatObjectGroup[1].m_end; ++i)
			{
				if(flatObject[i].m_life == 0.f)
				{
					flatObject[i] = *ship;
					flatObject[i].m_angle = atan2f(framework.m_controllerContext.getRightStick().getX(), -framework.m_controllerContext.getRightStick().getY());
					flatObject[i].m_velocity = ship->m_velocity + framework.m_controllerContext.getRightStick() * 1.f;
					gunHeat = 0.25f;
					flatObject[i].m_life = gunHeat * 3;
					break;
				}
			}
		}

		//if(iceToroids < kToroids & random.GetFloat(1.f) > 0.9f)
		//{
		//	iceToroid[iceToroids++].randomize();
		//}

		// simulation step

		for(uint32_t i=0; i<iceToroids; ++i)
		{
			iceToroid[i].m_position += iceToroid[i].m_velocity * dt;
			const float r = (iceToroid[i].m_outerRadius + iceToroid[i].m_innerRadius) * iceToroid[i].m_scale;
			if(iceToroid[i].m_position.getX() < -r
		    || iceToroid[i].m_position.getX() > aspect + r
		    || iceToroid[i].m_position.getY() < -r
		    || iceToroid[i].m_position.getY() > 1.f + r)
			{
				iceToroid[i].randomize();
			}
			iceToroid[i].m_angle += iceToroid[i].m_angularVelocity * dt;
			iceToroid[i].recalculateMatrices();
		}
		for(uint32_t i=0; i<kMaximumFlatObjects; ++i)
		{
			flatObject[i].m_oldPosition = flatObject[i].m_position;
			flatObject[i].m_position += flatObject[i].m_velocity * dt;

			const float r = flatObject[i].m_scale;

			if(flatObject[i].m_position.getX() < -r)
				flatObject[i].m_position.setX(aspect + r);
			if(flatObject[i].m_position.getX() > aspect + r)
				flatObject[i].m_position.setX(-r);
			
			if(flatObject[i].m_position.getY() < -r)
				flatObject[i].m_position.setY(1.f + r);
			if(flatObject[i].m_position.getY() > 1.f + r)
				flatObject[i].m_position.setY(-r);

			flatObject[i].m_velocity *= powf(0.05f, dt);
			flatObject[i].m_angle += flatObject[i].m_angularVelocity * dt;
			flatObject[i].recalculateMatrices();
		}
		for(uint32_t i = flatObjectGroup[1].m_begin; i != flatObjectGroup[1].m_end; ++i)
		{
			enum {kPositions = 2};
			Vector2 position[kPositions];
			for(uint32_t p = 0; p <kPositions; ++p)
			{
				const float a = (p+1.f) / kPositions;
				position[p] = flatObject[i].m_oldPosition * a + flatObject[i].m_position * (1.f - a);
			}
			if(flatObject[i].m_life > 0.f)
				for(uint32_t j=0; j<iceToroids; ++j)
					for(uint32_t p=0; p<kPositions; ++p)
					{
						if(iceToroid[j].pointIsInside(position[p]))
						{
							if(flatObject[i].m_life == 0.f)
								continue;
							const uint32_t children = iceToroid[j].m_scale > 0.2 ? random.GetUint(2, 3) : 0;
							for(uint32_t child = 0; child < children; ++child)
							{
								if(iceToroids < kToroids)
								{
									iceToroid[iceToroids].randomize();
									iceToroid[iceToroids].m_position = iceToroid[j].m_position;
									iceToroid[iceToroids].m_scale = iceToroid[j].m_scale / children;
									Vector2 angle(random.GetFloat(-0.5f, 0.5f), 0.f);
									iceToroid[iceToroids].m_velocity = rotate(angle.getX(), flatObject[i].m_velocity / (iceToroid[j].m_scale * 10.f) + iceToroid[j].m_velocity);
									iceToroid[iceToroids].m_angularVelocity *= 2;
									++iceToroids;
								}
							}
							iceToroid[j] = iceToroid[--iceToroids];
							flatObject[i].m_life = 0.f;
							break;
						}
					}
		}		
		for(uint32_t i=0; i<kMaximumFlatObjects; ++i)
		{
			flatObject[i].m_life -= dt;
			if(flatObject[i].m_life < 0.f)
				flatObject[i].m_life = 0.f;
		}
		gunHeat -= dt;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		// Render the scene:
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceNone);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		// Clear
		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
		Gnm::Htile htile = {};
		htile.m_hiZ.m_zMask = 0; // tile is clear
		htile.m_hiZ.m_maxZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is > 1.f
		htile.m_hiZ.m_minZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is < 1.f
		Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(*gfxc, &bufferCpuIsWriting->m_depthTarget, htile);
		gfxc->setDepthClearValue(1.f);
		gfxc->setRenderTargetMask(0xF);
		gfxc->setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
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

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);	

		torusMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);
		gfxc->setPrimitiveType(torusMesh.m_primitiveType);
		gfxc->setIndexSize(torusMesh.m_indexType);

		for(uint32_t i=0; i<iceToroids; ++i)
		{
			Constants *constants = static_cast<Constants*>(gfxc->allocateFromCommandBuffer(sizeof(Constants), Gnm::kEmbeddedDataAlignment4));
			const Matrix4 model = iceToroid[i].m_localToWorld;
			constants->m_modelView = transpose(model);
			constants->m_modelViewProjection = transpose(projection * model);
			constants->m_lightPosition = (Vector4)Point3(0.f, 0.f, 0.f);
			constants->m_lightColor = framework.getLightColor();
			constants->m_ambientColor = framework.getAmbientColor();
			constants->m_lightAttenuation = Vector4(1, 0, 0, 0);
			constants->m_color = iceToroid[i].m_color;

			Gnm::Buffer constantBuffer;
			constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
			constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

			gfxc->drawIndex(torusMesh.m_indexCount, torusMesh.m_indexBuffer);
		}

		for(uint32_t group=0; group<kFlatObjectGroups; ++group)
		{
			gfxc->setVsShader(flatVertexShader.m_shader, 0, flatVertexShader.m_fetchShader, &flatVertexShader.m_offsetsTable);
			gfxc->setPsShader(flatPixelShader.m_shader, &flatPixelShader.m_offsetsTable);
			flatObjectGroup[group].m_mesh->SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);
			gfxc->setPrimitiveType(flatObjectGroup[group].m_mesh->m_primitiveType);
			gfxc->setIndexSize(flatObjectGroup[group].m_mesh->m_indexType);
			for(uint32_t object = flatObjectGroup[group].m_begin; object != flatObjectGroup[group].m_end; ++object)
			{
				if(flatObject[object].m_life <= 0.f)
					continue;
				FlatShaderConstants *constants = static_cast<FlatShaderConstants*>( gfxc->allocateFromCommandBuffer( sizeof(FlatShaderConstants), Gnm::kEmbeddedDataAlignment4 ) );
				constants->m_modelViewProjection = transpose(projection * flatObject[object].m_localToWorld);
				constants->m_color = Vector4(1.f, 1.f, 1.f, 1.f);

				Gnm::Buffer constantBuffer;
				constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
				constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
				gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
				gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);

				gfxc->drawIndex(flatObjectGroup[group].m_mesh->m_indexCount, flatObjectGroup[group].m_mesh->m_indexBuffer);
			}
		}
		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
    return 0;
}
