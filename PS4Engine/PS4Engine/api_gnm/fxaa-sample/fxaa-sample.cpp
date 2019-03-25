/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/frame.h"
#include "data/scene.h"
#include "std_srtstructs.h"
#include <gnmx/resourcebarrier.h>
using namespace sce;

namespace
{
	bool g_enableFXAA = false;
	bool g_useSrt = false;
	Framework::MenuCommandBool menuCommandBoolFXAA(&g_enableFXAA);
	Framework::MenuCommandBool menuCommandBoolSrt(&g_useSrt);
	const Framework::MenuItem menuItem[] =
	{
		{{"Toggle FXAA", "Toggle the FXAA anti-aliasing effect"}, &menuCommandBoolFXAA},
		{{"Use Shader Resource Table", "Choose SRT or non-SRT shader resource bindings"}, &menuCommandBoolSrt},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
	const uint32_t numSlices = 1;

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Gnm::RenderTarget m_shadowTarget;
		Gnm::RenderTarget m_aaTarget;
		Gnm::DepthRenderTarget m_shadowDepthTarget;
		Gnm::Texture m_shadowMapTexture,m_aaTexture;
		Gnmx::ResourceBarrier m_aaResourceBarrier;
		Matrices *m_shadowMatrices;
		Matrices *m_sceneMatrices;
		LightBuffer *m_lightBufferShadow;
		LightBuffer *m_lightBufferScene;
	};

}

enum ShaderAssetName
{
	kSimpleShader,
	konlyColorTex,
	kShadowPass,
	kAntialiasingPass,
};

struct ShaderAsset
{
	const char *m_vertexShaderName;
	const char *m_pixelShaderName;
	Framework::VsShader m_vsShader;
	Framework::PsShader m_psShader;
	Framework::VsShader m_vsShaderSrt;
	Framework::PsShader m_psShaderSrt;
	void *m_fetchShaderSrt;
};

struct TextureAsset // the filename of and Gnm object for a texture, together for convenience.
{
	const char *m_name;
	Gnm::Texture m_texture;
};

struct MaterialAsset
{
	const char   *m_name;			
	ShaderAsset  *m_shaderAsset;	
	TextureAsset *m_textureAsset;	
};

ShaderAsset shaderAsset[] =
{
	{"simpleShader", "simpleShader"},
	{"onlyColorTex", "onlyColorTex"},
	{"shadow", "shadow"},
	{"fxaa2", "fxaa2"},
};
enum {kShaderAssets = sizeof(shaderAsset) / sizeof(shaderAsset[0])};

TextureAsset textureAsset[] = 
{
	{"TX-Italy_Building_A"},
	{"TX-Italy_Dirt_D"},
	{"TX-Italy_Dirt_D"},
	{"TX-Italy_Ground"},
	{"TX-Italy_Macho"},
	{"TX-Italy_Macho_A"},
	{"TX-Italy_Macho_B"},
	{"TX-Italy_Obj"},
	{"TX-Italy_Sky"},
	{"TX-Italy_Water"},
};
enum {kTextureAssets = sizeof(textureAsset) / sizeof(textureAsset[0])};

const MaterialAsset materialAsset[] =
{
	{ "BackGround_mesh_Italy_Building_A_dirty_PNU", &shaderAsset[0], &textureAsset[0] },
	{ "Building_mesh_Italy_Building_A_dirty_PNU", &shaderAsset[0], &textureAsset[0] },
	{ "Ground_mesh_Italy_Ground_metalL_PNU", &shaderAsset[0], &textureAsset[3] },
	{ "Macho_mesh_Italy_Macho_A_metalH_PNU", &shaderAsset[0], &textureAsset[6] },
	{ "Macho_mesh_Italy_Macho_B_metalH_PNU", &shaderAsset[0], &textureAsset[5] },
	{ "Macho_mesh_Italy_Macho_C_metalH_PNU", &shaderAsset[0], &textureAsset[4] },
	{ "fan_mesh_Italy_Macho_metalH_PNU", &shaderAsset[0], &textureAsset[4] },
	{ "OBJ_mesh_Italy_Ground_metalL_PNU", &shaderAsset[0], &textureAsset[2] },
	{ "OBJ_mesh_Italy_Obj_dirty_PNU", &shaderAsset[0], &textureAsset[7] },
	{ "OBJ_mesh_Italy_Obj_MetalL_PNU", &shaderAsset[0], &textureAsset[7] },
	{ "Sky_mesh_Italy_sky_PU", &shaderAsset[1], &textureAsset[8] },
	{ "Italy_Water_metalH_PNU", &shaderAsset[0], &textureAsset[9] },
};
enum {kMaterialAssets = sizeof(materialAsset) / sizeof(materialAsset[0])};

void LoadShaderAsset(ShaderAsset *shaderAsset, Gnmx::Toolkit::Allocators *allocators)
{
	char filePath[256];
	int error = 0;

	snprintf(filePath, 256, "/app0/fxaa-sample/%s_vv.sb", shaderAsset->m_vertexShaderName);
	error = Framework::LoadVsMakeFetchShader(&shaderAsset->m_vsShader, Framework::ReadFile(filePath), allocators);
	snprintf(filePath, 256, "/app0/fxaa-sample/%s_p.sb", shaderAsset->m_pixelShaderName);
	error = Framework::LoadPsShader(&shaderAsset->m_psShader, Framework::ReadFile(filePath), allocators);
	snprintf(filePath, 256, "/app0/fxaa-sample/%s_srt_vv.sb", shaderAsset->m_vertexShaderName);
	error = Framework::LoadVsMakeFetchShader(&shaderAsset->m_vsShaderSrt, Framework::ReadFile(filePath), allocators);
	snprintf(filePath, 256, "/app0/fxaa-sample/%s_srt_p.sb", shaderAsset->m_pixelShaderName);
	error = Framework::LoadPsShader(&shaderAsset->m_psShaderSrt, Framework::ReadFile(filePath), allocators);
}

void LoadTextureAsset(TextureAsset* textureAsset, Gnmx::Toolkit::Allocators *allocators)
{
	char filePath[256];
	snprintf(filePath, 256, "/app0/fxaa-sample/data/%s.gnf", textureAsset->m_name);
	Framework::loadTextureFromGnf(&textureAsset->m_texture, Framework::ReadFile(filePath), 0, allocators);
	textureAsset->m_texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // won't ever bind this texture as RWTexture, so it's OK to mark it as read-only.
	if(textureAsset->m_texture.getDataFormat().m_asInt == Gnm::kDataFormatBc1Unorm.m_asInt)
		textureAsset->m_texture.setDataFormat(Gnm::kDataFormatBc1UnormSrgb); // texture asset pipeline is gamma-ambiguous, so let's force sRGB here.
}

float m_angle;
bool m_meshRotates[gScene_NumMeshes];
Mesh m_meshTable[gScene_NumMeshes];

bool loadMesh(Gnmx::Toolkit::Allocators *allocators, Mesh &mesh, const char *dataDirPath,const char **materialNames)
{
	const MeshDeclaration &mdecl = *mesh.meshDeclaration;

	char filePath[256];
	snprintf(filePath, 256, "/app0/fxaa-sample/%s/%s", dataDirPath, mdecl.fileName);
	FILE *file = fopen(filePath, "rb");
	if(file == NULL) return false;

	const uint32_t sizeVertexBuffer = mdecl.numVertices * mdecl.stride;
	allocators->allocate(&mesh.m_vertexBuffer, SCE_KERNEL_WC_GARLIC, sizeVertexBuffer, Gnm::kAlignmentOfBufferInBytes, Gnm::kResourceTypeVertexBufferBaseAddress, "%s Vertex Buffer", mdecl.fileName);

	const uint32_t sizeIndexBuffer = mdecl.numIndices * sizeof(uint32_t);
	allocators->allocate(&mesh.m_indexBuffer,SCE_KERNEL_WC_GARLIC,sizeIndexBuffer,Gnm::kAlignmentOfBufferInBytes,Gnm::kResourceTypeIndexBufferBaseAddress,"%s Index Buffer",mdecl.fileName);

	size_t n;
	n = fread(mesh.m_vertexBuffer, sizeVertexBuffer, 1, file);
	if(n != 1)
	{
		fclose(file);
		return false;
	}

	n = fread(mesh.m_indexBuffer, sizeIndexBuffer, 1, file);
	if(n != 1)
	{
		fclose(file);
		return false;
	}

	fclose(file);

	if(materialNames)
	{
		char buf[256];
		strncpy(buf, mdecl.fileName, 256);
		char *p = buf + strlen(buf) - 5; // Chop off the ".mesh" suffix
		*p = 0;

		int materialIndex = 0;
		const char **mn = materialNames;
		while(*mn != NULL)
		{
			size_t len = std::min(strlen(*mn), strlen(buf));
			if(strcmp(p - len, *mn) == 0)
			{
				mesh.materialIndex = materialIndex;
				break;
			}
			mn++;
			materialIndex++;
		}
	}

	return true;
}

bool loadScene(Gnmx::Toolkit::Allocators* allocators, 
				int numMeshes, MeshDeclaration **meshTable, Mesh *mesh,
				const char **materialNames)
{
	printf("Loading mesh ");
	for(int m = 0; m < numMeshes; m++)
	{
		mesh[m].meshDeclaration = meshTable[m];
		bool res = loadMesh(allocators, mesh[m], "data", materialNames);
		if(!res)
		{
			printf(
				"\n"
				"ERRPR:failed to load. %s\n",
				mesh[m].meshDeclaration->fileName);
			return false;
		}

		if(materialNames && (mesh[m].materialIndex < 0))
		{
			printf(
				"\n"
				"ERRPR:material name doesn't correspond. %s\n",
				mesh[m].meshDeclaration->fileName);
			return false;
		}
		printf(".");
		fflush(stdout);
	}
	printf("done\n");

	return true;
}

static uint32_t numTris = 0;

void draw(	Frame *frame,
			const Gnm::Texture* shadowMapTexture,
			const Matrix4 *matrixView,
			const Matrix4 *matrixProjection,
			const Matrix4 *matrixShadowMap,
			const Vector3 *vtxLPosInView,
			const Vector4 *lightColor,
			const Vector4 *ambientColor,
			bool drawDepth = false)
{
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	for(int m = 0; m < gScene_NumMeshes; m++)
	{
		Matrices *matrices = drawDepth ? &frame->m_shadowMatrices[m] : &frame->m_sceneMatrices[m];
		LightBuffer *lightBuffer = drawDepth ? &frame->m_lightBufferShadow[m] : &frame->m_lightBufferScene[m];

		const Mesh &mesh = m_meshTable[m];
		const MeshDeclaration &mdecl = *(mesh.meshDeclaration);

		SCE_GNM_ASSERT(mdecl.numInstance == 1);
		for(uint32_t i = 0; i < mdecl.numInstance; i++)
		{
			PS2xTex2xSamplerSrt srt;

			if (drawDepth)
			{
				if (g_useSrt)
				{
					gfxc->setVsShader(shaderAsset[kShadowPass].m_vsShaderSrt.m_shader, 0, shaderAsset[kShadowPass].m_vsShaderSrt.m_fetchShader, &shaderAsset[kShadowPass].m_vsShaderSrt.m_offsetsTable);
				}
				else
				{
					gfxc->setVsShader(shaderAsset[kShadowPass].m_vsShader.m_shader, 0, shaderAsset[kShadowPass].m_vsShader.m_fetchShader, &shaderAsset[kShadowPass].m_vsShader.m_offsetsTable);
				}
			}
			else
			{
				Gnm::Sampler sampler, shadowSampler;
				sampler.init();
				sampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
				sampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
				shadowSampler.init();

				if (g_useSrt)
				{
					PS2xTex2xSamplerSrtData* srtBuffer = (PS2xTex2xSamplerSrtData*)gfxc->allocateFromCommandBuffer(sizeof(PS2xTex2xSamplerSrtData), Gnm::kEmbeddedDataAlignment16);
					srtBuffer->txDiffuse = materialAsset[mesh.materialIndex].m_textureAsset->m_texture;
					srtBuffer->samLinear = sampler;
					srtBuffer->txShadowMap = *shadowMapTexture;
					srtBuffer->samShadow = shadowSampler;
					srt.srtData = srtBuffer;
					gfxc->setVsShader(materialAsset[mesh.materialIndex].m_shaderAsset->m_vsShaderSrt.m_shader, 0, materialAsset[mesh.materialIndex].m_shaderAsset->m_vsShaderSrt.m_fetchShader, &materialAsset[mesh.materialIndex].m_shaderAsset->m_vsShaderSrt.m_offsetsTable);
					gfxc->setPsShader(materialAsset[mesh.materialIndex].m_shaderAsset->m_psShaderSrt.m_shader, &materialAsset[mesh.materialIndex].m_shaderAsset->m_psShaderSrt.m_offsetsTable);
				}
				else
				{
					gfxc->setVsShader(materialAsset[mesh.materialIndex].m_shaderAsset->m_vsShader.m_shader, 0, materialAsset[mesh.materialIndex].m_shaderAsset->m_vsShader.m_fetchShader, &materialAsset[mesh.materialIndex].m_shaderAsset->m_vsShader.m_offsetsTable);
					gfxc->setPsShader(materialAsset[mesh.materialIndex].m_shaderAsset->m_psShader.m_shader, &materialAsset[mesh.materialIndex].m_shaderAsset->m_psShader.m_offsetsTable);
					gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &materialAsset[mesh.materialIndex].m_textureAsset->m_texture);
					gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);
					gfxc->setTextures(Gnm::kShaderStagePs, 1, 1, shadowMapTexture);
					gfxc->setSamplers(Gnm::kShaderStagePs, 1, 1, &shadowSampler);
				}
			}


			if (g_useSrt)
			{
				VSSrt vsSrt;
				vsSrt.constants.initAsRegularBuffer(matrices, sizeof(*matrices), 1);
				vsSrt.constants.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
				gfxc->setUserSrtBuffer(Gnm::kShaderStageVs, &vsSrt, sizeof(vsSrt) / sizeof(uint32_t));
			}
			else
			{
				Gnm::Buffer matricesBuffer;
				matricesBuffer.initAsConstantBuffer(matrices, sizeof(*matrices));
				matricesBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
				gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &matricesBuffer);
			}

			matrices->view = transpose(*matrixView);
			matrices->projection = transpose(*matrixProjection);
			if( matrixShadowMap ) 
				matrices->shadow = transpose(*matrixShadowMap);

			gfxc->pushMarker(mesh.meshDeclaration->fileName);
			const Matrix4 matrixWorld = (const Matrix4&)mdecl.mtxInstance[i];

			// Apply rotation, if appropriate
			const Matrix4 pRotMat = Matrix4::rotationY(m_angle);
			if(m_meshRotates[m])
				matrices->world = transpose(pRotMat*matrixWorld);
			else
				matrices->world = transpose(matrixWorld);

			const MeshDeclaration &mdecl = *mesh.meshDeclaration;

			// setup lights position
			if( vtxLPosInView )
			{
				if (g_useSrt)
				{
					srt.constants.initAsRegularBuffer(lightBuffer, sizeof(*lightBuffer), 1);
					srt.constants.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
				}
				else
				{
					Gnm::Buffer pixConstBuffer;
					pixConstBuffer.initAsConstantBuffer(lightBuffer, sizeof(*lightBuffer));
					pixConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
					gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &pixConstBuffer);
				}
				lightBuffer->lightPosition = (Vector4)vtxLPosInView[0];
				lightBuffer->lightColor = *lightColor;
				lightBuffer->ambientColor = *ambientColor;
			}

			if (g_useSrt && !drawDepth)
			{
				gfxc->setUserSrtBuffer(Gnm::kShaderStagePs, &srt, sizeof(srt) / sizeof(uint32_t));
			}

			{
				Gnm::Buffer buffer[16];
				int buffers = 0;
				if( mdecl.position >= 0 )
				{
					buffer[buffers].initAsVertexBuffer(static_cast<uint8_t*>(mesh.m_vertexBuffer) + mdecl.position, Gnm::kDataFormatR32G32B32Float, mdecl.stride, mdecl.numVertices);
					buffer[buffers].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's read-only.
					++buffers;
				}
				if( !drawDepth && mdecl.normal >= 0 )
				{
					buffer[buffers].initAsVertexBuffer(static_cast<uint8_t*>(mesh.m_vertexBuffer) + mdecl.normal, Gnm::kDataFormatR32G32B32Float, mdecl.stride, mdecl.numVertices);
					buffer[buffers].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's read-only.
					++buffers;
				}
				if( !drawDepth && mdecl.texcoord >= 0 )
				{
					buffer[buffers].initAsVertexBuffer(static_cast<uint8_t*>(mesh.m_vertexBuffer) + mdecl.texcoord, Gnm::kDataFormatR32G32Float, mdecl.stride, mdecl.numVertices);
					buffer[buffers].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's read-only.
					++buffers;
				}

				if (buffers > 0)
				{
					gfxc->setVertexBuffers( Gnm::kShaderStageVs, 0, buffers, buffer );
				}
			}
			gfxc->setPrimitiveType(Gnm::kPrimitiveTypeTriList);
			gfxc->setIndexSize(Gnm::kIndexSize32);

			gfxc->drawIndex(mdecl.numIndices, mesh.m_indexBuffer);
			numTris += mdecl.numIndices /3;
			gfxc->popMarker();
		}
	}
}

void Init(Gnmx::Toolkit::Allocators *allocators) // load textures, meshes, and shaders.
{
	for(int t = 0; t < kTextureAssets; t++)
	{
		LoadTextureAsset(&textureAsset[t], allocators);
	}
	const char *materialNames[kMaterialAssets + 1];
	for(int m = 0; m < kMaterialAssets; m++) materialNames[m] = materialAsset[m].m_name;
	materialNames[kMaterialAssets] = NULL;
	if(!loadScene(allocators, gScene_NumMeshes, gScene_MeshDecTable, m_meshTable, materialNames))
	{
		SCE_GNM_ASSERT(false);
	}

	for(int m=0; m<gScene_NumMeshes; ++m)
	{
		const Mesh &mesh = m_meshTable[m];
		const MeshDeclaration &mdecl = *(mesh.meshDeclaration);
		if(strncmp(mdecl.fileName, "Macho_mesh", 10) == 0)
			m_meshRotates[m] = true;
		else if(strncmp(mdecl.fileName, "fan_mesh", 8) == 0)
			m_meshRotates[m] = true;
		else
			m_meshRotates[m] = false;
	}

	for(uint32_t s = 0; s < kShaderAssets; ++s)
	{
		LoadShaderAsset(&shaderAsset[s], allocators);
	}
}

void ShadowPass(Frame *frame, const Matrix4 &matrixLightView, const Matrix4 &matrixLightProj) // render the scene from the light's point of view into a depth buffer.
{
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	gfxc->pushMarker("Shadow Pass");
	// Shadow map pass
	const int nWidth = 4096;
	const int nHeight = 4096;
	gfxc->setupScreenViewport(0, 0, nWidth, nHeight, 0.5f, 0.5f);
	{
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);
		Gnm::DepthEqaaControl eqaaReg;
		eqaaReg.init();
		eqaaReg.setMaxAnchorSamples(Gnm::kNumSamples1);
		eqaaReg.setAlphaToMaskSamples(Gnm::kNumSamples1);
		eqaaReg.setStaticAnchorAssociations(true);
		gfxc->setDepthEqaaControl(eqaaReg);
		Gnm::ClipControl clipReg;
		clipReg.init();
		gfxc->setClipControl(clipReg);
	}

	Gnm::DepthStencilControl dsc;
	dsc.init();
	dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
	dsc.setDepthEnable(true);
	gfxc->setDepthStencilControl(dsc);
	gfxc->setRenderTargetMask(0x00000000);

	{
		Gnm::DbRenderControl dbRenderControl;
		dbRenderControl.init();
		dbRenderControl.setDepthClearEnable(false);
		gfxc->setDbRenderControl(dbRenderControl);
	}

	draw(frame, NULL, &matrixLightView, &matrixLightProj, NULL, NULL, NULL, NULL, true);
	gfxc->popMarker();
}

Framework::GnmSampleFramework framework;

void MainPass(Frame *frame, Gnm::Texture shadowMapTexture, Matrix4& matrixLightProj, Matrix4& matrixLightView, const Vector4 &lightColor, const Vector4 &ambientColor) // render the scene from the camera's point of view
{
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

	gfxc->pushMarker("Main Pass");
	Gnm::DbRenderControl dbRenderControl;
	dbRenderControl.init();
	dbRenderControl.setDepthClearEnable(false);
	gfxc->setDbRenderControl(dbRenderControl);

	Gnm::DepthStencilControl dsc;
	dsc.init();
	dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
	dsc.setDepthEnable(true);
	gfxc->setDepthStencilControl(dsc);

	{
		// Setup perspective matrix.  Using the equation from the PS3 TSM sample so that images will match exactly
		const float aspect = (float)framework.m_config.m_targetWidth/(float)framework.m_config.m_targetHeight;
		const float fovyRadians = 45.0f * M_PI/180.0f;
		const float zNear = 0.1f;
		const float zFar = 250.0f;
		const float f = tan( M_PI/2.0f - fovyRadians * 0.5f );
		const float rangeInv = 1.0f / ( zNear - zFar );
		{
			const Matrix4 matrixProjection(
				Vector4(f/aspect,0.f,0.f,0.f),
				Vector4(0.f,f,0.f,0.f),
				Vector4(0.f,0.f,( zNear + zFar ) * rangeInv,-1.0f),
				Vector4(0.f,0.f,zNear * zFar * rangeInv * 2.0f,0.f)
			);
			framework.SetProjectionMatrix( matrixProjection );
		}

		// Setup shadow map matrix
		const Matrix4 matrixShadowMap = matrixLightProj * matrixLightView;
		const Vector3 vLgtInView = framework.getLightPositionInViewSpace().getXYZ();
		draw(frame, &shadowMapTexture, &framework.m_worldToViewMatrix, &framework.m_projectionMatrix, &matrixShadowMap, &vLgtInView, &lightColor, &ambientColor);
	}
	gfxc->popMarker();
}

// this sample demonstrates the FXAA method of anti-aliasing.

int main(int argc, const char *argv[])
{
	framework.m_config.m_lightPositionX = Vector3(-4.0f, 20.0f, 5.0f);
	framework.m_config.m_lightTargetX = Vector3(0, 6, 0);
	framework.m_config.m_lightUpX = Vector3(0, 1, 0);
	framework.m_config.m_ambientColor = 0; // black
	framework.m_config.m_lookAtPosition = (Vector3)Point3(3.46189f,11.7358f,7.42404f);
	framework.m_config.m_lookAtTarget = (Vector3)Point3(0,6,0);
	framework.m_config.m_lookAtUp = (Vector3)Vector3(0,1,0);

	framework.processCommandLine(argc, argv);

	framework.m_config.m_clearColorIsMeaningful = false;
	framework.m_config.m_lightingIsMeaningful = true;
	framework.m_config.m_depthBufferIsMeaningful = false;

	framework.initialize( "FXAA", 
		"Sample code for anti-aliasing a scene with the FXAA algorithm.", 
		"This sample renders a complex scene with shading and shadow mapping, and then "
		"optionally applies FXAA anti-aliasing.");

	framework.appendMenuItems(kMenuItems, menuItem);

	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_shadowMatrices,SCE_KERNEL_WB_ONION,sizeof(Matrices)*gScene_NumMeshes,4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Shadow Matrices");
		framework.m_allocators.allocate((void**)&frame->m_sceneMatrices,SCE_KERNEL_WB_ONION,sizeof(Matrices)*gScene_NumMeshes,4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Scene Matrices");
		framework.m_allocators.allocate((void**)&frame->m_lightBufferShadow,SCE_KERNEL_WB_ONION,sizeof(LightBuffer)*gScene_NumMeshes,4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Light Buffer Shadow");
		framework.m_allocators.allocate((void**)&frame->m_lightBufferScene,SCE_KERNEL_WB_ONION,sizeof(LightBuffer)*gScene_NumMeshes,4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Light Buffer Scene");

		// Set up the anti-aliasing buffer
		Gnm::DataFormat aaFormat = Gnm::kDataFormatR8G8B8A8UnormSrgb;
		Gnm::TileMode aaTileMode;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&aaTileMode,GpuAddress::kSurfaceTypeColorTarget,aaFormat,1);
		Gnm::SizeAlign aaSizeAlign = Gnmx::Toolkit::init(&frame->m_aaTarget,framework.m_config.m_targetWidth,framework.m_config.m_targetHeight,numSlices,aaFormat,aaTileMode,Gnm::kNumSamples1,Gnm::kNumFragments1,NULL,NULL);
		void *aaCpuBaseAddr;
		framework.m_allocators.allocate(&aaCpuBaseAddr,SCE_KERNEL_WC_GARLIC,aaSizeAlign,Gnm::kResourceTypeRenderTargetBaseAddress,"Buffer %d AA RenderTarget COLOR",buffer);
		frame->m_aaTarget.setBaseAddress(aaCpuBaseAddr);

		// Set up shadowmap buffer
		int nShadowWidth = 4096,nShadowHeight = 4096;
		Gnm::SizeAlign shadowHtileSizeAlign;
		Gnm::DataFormat shadowDepthFormat = Gnm::DataFormat::build(Gnm::kZFormat32Float);
		Gnm::TileMode shadowTileMode,shadowDepthTileMode;
		// Set up front buffer render target
		Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&shadowTileMode,GpuAddress::kSurfaceTypeColorTarget,format,1);
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&shadowDepthTileMode,GpuAddress::kSurfaceTypeDepthOnlyTarget,shadowDepthFormat,1);
		Gnmx::Toolkit::init(&frame->m_shadowTarget,nShadowWidth,nShadowHeight,1,format,shadowTileMode,Gnm::kNumSamples1,Gnm::kNumFragments1,NULL,NULL);
		Gnm::SizeAlign shadowDepthTargetSizeAlign = Gnmx::Toolkit::init(&frame->m_shadowDepthTarget,nShadowWidth,nShadowHeight,shadowDepthFormat.getZFormat(),Gnm::kStencilInvalid,shadowDepthTileMode,Gnm::kNumFragments1,NULL,&shadowHtileSizeAlign);

		void *shadowDepthCpuBasePtr,*shadowDepthCpuHtilePtr;
		framework.m_allocators.allocate(&shadowDepthCpuBasePtr,SCE_KERNEL_WC_GARLIC,shadowDepthTargetSizeAlign,Gnm::kResourceTypeDepthRenderTargetBaseAddress,"Buffer %d Shadow DepthRenderTarget DEPTH",buffer);
		framework.m_allocators.allocate(&shadowDepthCpuHtilePtr,SCE_KERNEL_WC_GARLIC,shadowHtileSizeAlign,Gnm::kResourceTypeDepthRenderTargetHTileAddress,"Buffer %d Shadow DepthRenderTarget HTILE",buffer);

		frame->m_shadowDepthTarget.setZReadAddress(shadowDepthCpuBasePtr);
		frame->m_shadowDepthTarget.setZWriteAddress(shadowDepthCpuBasePtr);
		frame->m_shadowDepthTarget.setHtileAddress(shadowDepthCpuHtilePtr);

		frame->m_aaTexture.initFromRenderTarget(&frame->m_aaTarget,false);
		frame->m_aaTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we won't ever bind this texture as RWTexture, so it's OK to mark it as read-only.

		char temp[64];
		snprintf(temp,64,"Buffer %d AA Texture",buffer);
		Gnm::registerResource(nullptr,framework.m_allocators.m_owner,frame->m_aaTexture.getBaseAddress(),aaSizeAlign.m_size,temp,Gnm::kResourceTypeTextureBaseAddress,0);

		frame->m_shadowMapTexture.initFromDepthRenderTarget(&frame->m_shadowDepthTarget,false);
		frame->m_shadowMapTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we won't ever bind this texture as RWTexture, so it's OK to mark it as read-only.

		snprintf(temp,64,"Buffer %d Shadow Texture",buffer);
		Gnm::registerResource(nullptr,framework.m_allocators.m_owner,frame->m_shadowMapTexture.getBaseAddress(),shadowDepthTargetSizeAlign.m_size,temp,Gnm::kResourceTypeTextureBaseAddress,0);

		frame->m_aaResourceBarrier.init(&frame->m_aaTarget,Gnmx::ResourceBarrier::kUsageRenderTarget,Gnmx::ResourceBarrier::kUsageRoTexture);
	}

	Init(&framework.m_allocators); 

	Gnm::Sampler resSampler, anisoSampler;
	resSampler.init();
	anisoSampler.init();
	anisoSampler.setXyFilterMode(Gnm::kFilterModeAnisoBilinear, Gnm::kFilterModeAnisoBilinear);
	anisoSampler.setAnisotropyRatio(Gnm::kAnisotropyRatio4);
	anisoSampler.setWrapMode(Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel);

	while( !framework.m_shutdownRequested )
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		m_angle = framework.GetSecondsElapsedApparent() * 0.3f;

		gfxc->pushMarker("Fast clear");	
		Gnm::Htile htile = {};
		htile.m_hiZ.m_zMask = 0; // tile is clear
		htile.m_hiZ.m_maxZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is > 1.f
		htile.m_hiZ.m_minZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is < 1.f
		Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(*gfxc, &frame->m_shadowDepthTarget, htile);
		gfxc->setDepthClearValue(1.f);
		gfxc->popMarker();

		gfxc->setupScreenViewport(0, 0, frame->m_shadowTarget.getWidth(), frame->m_shadowTarget.getHeight(), 0.5f, 0.5f);
		gfxc->setRenderTarget(0, &frame->m_shadowTarget);
		gfxc->setDepthRenderTarget(&frame->m_shadowDepthTarget);

		Matrix4 matrixLightView = framework.m_worldToLightMatrix;
		Matrix4 matrixLightProj = Matrix4::orthographic(-20, 20, -20, 20, 1.0, 200);
		ShadowPass(frame, matrixLightView, matrixLightProj);

		// Decompress the shadow map depth buffer in preparation for it to be used as a texture
		Gnmx::decompressDepthSurface(gfxc, &frame->m_shadowDepthTarget );

		gfxc->pushMarker("Color/Depth Clear");
			Gnm::RenderTarget *renderTarget = g_enableFXAA ? &frame->m_aaTarget : &bufferCpuIsWriting->m_renderTarget;

			Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, renderTarget, framework.getClearColor());

			gfxc->pushMarker("Fast Depth Clear");
				Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(*gfxc, &bufferCpuIsWriting->m_depthTarget, htile);			
				gfxc->setDepthClearValue(1.f);
            gfxc->popMarker();

			gfxc->setRenderTargetMask(0xF);

			gfxc->setRenderTarget(0, renderTarget);
			gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

			Gnm::PrimitiveSetup primSetupReg;
			primSetupReg.init();
			primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
			primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
			gfxc->setPrimitiveSetup(primSetupReg);

			const int32_t nWidth = renderTarget->getWidth();
			const int32_t nHeight = renderTarget->getHeight();
			gfxc->setupScreenViewport(0, 0, nWidth, nHeight, 0.5f, 0.5f);
		gfxc->popMarker();

		Gnm::ClipControl clipReg;
		clipReg.init();
		gfxc->setClipControl(clipReg);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);

		MainPass(frame, frame->m_shadowMapTexture, matrixLightProj, matrixLightView, framework.getLightColor(), framework.getAmbientColor());

		framework.RenderDebugObjects(*gfxc);

		if(g_enableFXAA)
		{
			gfxc->setDepthRenderTarget(0);
			frame->m_aaResourceBarrier.write(&gfxc->m_dcb);
			// Run AA onto the main render target (which must be untiled)
			gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
			gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);
			if (g_useSrt)
			{
				PSTexSamplerSrtData* srtBuffer = (PSTexSamplerSrtData*)gfxc->allocateFromCommandBuffer(sizeof(PSTexSamplerSrtData), Gnm::kEmbeddedDataAlignment16);
				srtBuffer->texture = frame->m_aaTexture;
				srtBuffer->sampler = anisoSampler;
				PSTexSamplerSrt srt = { srtBuffer };
				gfxc->setVsShader(shaderAsset[kAntialiasingPass].m_vsShaderSrt.m_shader, 0, (void*)0, &shaderAsset[kAntialiasingPass].m_vsShaderSrt.m_offsetsTable);
				gfxc->setPsShader(shaderAsset[kAntialiasingPass].m_psShaderSrt.m_shader, &shaderAsset[kAntialiasingPass].m_psShaderSrt.m_offsetsTable);
				gfxc->setUserSrtBuffer(Gnm::kShaderStagePs, &srt, sizeof(srt) / sizeof(uint32_t));
			}
			else
			{
				gfxc->setVsShader(shaderAsset[kAntialiasingPass].m_vsShader.m_shader, 0, (void*)0, &shaderAsset[kAntialiasingPass].m_vsShader.m_offsetsTable);
				gfxc->setPsShader(shaderAsset[kAntialiasingPass].m_psShader.m_shader, &shaderAsset[kAntialiasingPass].m_psShader.m_offsetsTable);
				gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &frame->m_aaTexture);
				gfxc->setSamplers(Gnm::kShaderStagePs, 3, 1, &anisoSampler);
			}
			gfxc->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
			gfxc->drawIndexAuto(3);
			numTris += 1;
			gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);
		}

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
	return 0;
}

