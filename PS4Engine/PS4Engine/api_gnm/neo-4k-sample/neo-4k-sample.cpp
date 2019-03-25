/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include <libsysmodule.h>
#include <dbg_keyboard.h>
#include <vector>
#include <video_out.h>

using namespace sce;

#pragma comment(lib,"SceSysModule_stub_weak")
#pragma comment(lib,"SceDbgKeyboard_stub_weak")

enum ShaderType
{
	kShaderPhong,
	kShaderPhongAlphaTest,
	kShaderSkybox,
	kShaderAlphaTest,
	kShaderAlphaTestMask,
	kShaderShadow,
	kShaderFxaa,
};

enum MaterialType
{
	kMaterialTypeOpaque			= 1,
	kMaterialTypeAlphaTest		= 2,
	kMaterialTypeTranslucent	= 4,
};

enum RenderingMode
{
	kRenderingMode1080p,		// 1080p
	kRenderingMode4K,			// 4K
	kRenderingMode4KCB,			// 4K Checkerboard
	kRenderingMode4KG,			// 4K Geometry
};

enum AaMode
{
	kAaModeNone,
	kAaModeFxaa,
	kAaModeSmaa
};

struct VsShader
{
	sce::Gnmx::VsShader*	m_shader;
	Gnmx::InputOffsetsCache	m_offsetsTable;
	void*					m_fetchShader;
};

struct PsShader
{
	sce::Gnmx::PsShader*	m_shader;
	Gnmx::InputOffsetsCache	m_offsetsTable;
};

struct CsShader
{
	sce::Gnmx::CsShader*	m_shader;
	Gnmx::InputOffsetsCache	m_offsetsTable;
};

struct ShaderPair
{
	const char*			m_vertexShaderName;
	const char*			m_pixelShaderName;
	VsShader			m_vsShader;
	PsShader			m_psShader;
};

struct MaterialDesc
{
	const char*			m_name;
	const char*			m_textureDiffuse;
	const char*			m_textureSpecular;
	const char*			m_textureNormal;
	float				m_specularColor[3];
};

struct Material
{
	const char*			m_name;
	ShaderPair*			m_shader;
	Gnm::Texture		m_textureDiffuse;
	Gnm::Texture		m_textureSpecular;
	Gnm::Texture		m_textureNormal;
	Vector4				m_specularColor;
	MaterialType		m_type;
	bool isAlphaTest()		{return m_type == kMaterialTypeAlphaTest;}
	bool isTranslucent()	{return m_type == kMaterialTypeTranslucent;}
};

struct Texture
{
	const char*			m_name;
	Gnm::Texture		m_texture;
};

struct Mesh
{
	const char*			m_name;
	void*				m_vertexBuffer;
	void*				m_indexBuffer;

	uint32_t			m_numVertices;
	uint32_t			m_numIndices;
	uint32_t			m_stride;
	int					m_position;
	int					m_normal;
	int					m_tangent;
	int					m_texcoord;

	uint32_t			m_materialIndex;
	
	void*				m_dynamicIndexBuffer;

	Matrix4				m_matWorldViewProjPrev;
};

struct LinearAllocator
{
	void init(size_t size, Gnmx::Toolkit::Allocators *allocators)
	{
		m_base = (uintptr_t)allocators->m_onion.allocate(size, 1024);
		m_current = m_base;
		m_end = m_base + size;
	}

	void *allocate(size_t size, size_t align)
	{
		uintptr_t p = (m_current + align-1) & (~(align-1));
		if(p + size > m_end)
			return NULL;
		m_current = p + size;
		return (void*)p;
	}

	void reset()
	{
		m_current = m_base;
	}

	uintptr_t	m_base;
	uintptr_t	m_current;
	uintptr_t	m_end;
};

ShaderPair g_shaders[] =
{
	{"phong", "phong"},
	{"phong", "phong_alpha_test"},
	{"skybox", "skybox"},
	{"alpha_depth_only", "alpha_depth_only"},
	{"alpha_depth_only_mask", "alpha_depth_only_mask"},
	{"shadow", "shadow"},
	{"fxaa2", "fxaa2"},
};

struct {
	uint32_t width;
	uint32_t height;
	Gnm::NumSamples colorSampleCount;
	Gnm::NumFragments colorFragmentCount;
	Gnm::NumSamples idSampleCount;
	Gnm::NumFragments depthFragmentCount;
	bool fmask;
	const char* shader_name;
	CsShader resolve_shader;
} g_targetConfigs[] = {
	{1920, 1080, Gnm::kNumSamples1, Gnm::kNumFragments1, Gnm::kNumSamples1, Gnm::kNumFragments1, false, "/app0/neo-4k-sample/blit_c.sb"}, // 1080p
	{3840, 2160, Gnm::kNumSamples1, Gnm::kNumFragments1, Gnm::kNumSamples1, Gnm::kNumFragments1, false, "/app0/neo-4k-sample/blit_c.sb"}, // 4K
	{1920, 2160, Gnm::kNumSamples1, Gnm::kNumFragments1, Gnm::kNumSamples2, Gnm::kNumFragments2, false, "/app0/neo-4k-sample/checkerboard_resolve_c.sb"}, // 4KCB
	{1920, 1080, Gnm::kNumSamples1, Gnm::kNumFragments1, Gnm::kNumSamples4, Gnm::kNumFragments4, false, "/app0/neo-4k-sample/geometry_resolve_c.sb"}, // 4KG
};

const size_t g_numConfigs = sizeof(g_targetConfigs)/sizeof(g_targetConfigs[0]);

#include "data/scene.h"

static const uint32_t	kNumShaders			= sizeof(g_shaders) / sizeof(g_shaders[0]);
static const uint32_t	kNumMaterials		= sizeof(materialDesc) / sizeof(materialDesc[0]);
static const uint32_t	kNumMeshes			= sizeof(meshDesc) / sizeof(meshDesc[0]);
static const uint32_t	kShadowWidth		= 4096;
static const uint32_t	kShadowHeight		= 4096;
static const uint32_t	kNumFramesInFlight	= 3;
static const uint32_t	kNumBlurTargets		= 6;


Mesh					g_meshes[kNumMeshes];
Material				g_materials[kNumMaterials];
LinearAllocator			g_scratchBuffers[kNumFramesInFlight];
std::vector<Texture>	g_textures;
uint32_t				g_debugView = 0;
bool					g_temporalEnabled = true;
bool					g_gradientAdjustEnabled = true;
bool					g_psInvokeEnabled = true;
bool					g_alphaUnrollEnabled = true;
bool					g_checkerboardEnabled = true;
bool					g_bloomEnabled = true;
bool					g_maskExport = true;
AaMode					g_aaMode = kAaModeSmaa;
Gnm::Sampler			g_samplerPoint;
Gnm::Sampler			g_samplerLinear;
Gnm::Sampler			g_samplerAniso;
Gnm::Sampler			g_samplerShadow;
VsShader				g_fullScreenVertexShader;
PsShader				g_passThroughPixelShader;
PsShader				g_brightPassPixelShader;
PsShader				g_gaussianHPixelShader;
PsShader				g_gaussianVPixelShader;
PsShader				g_bloomCompositePixelShader;
PsShader				g_radialBlurPixelShader;
CsShader				g_checkerboardResolveShader;
CsShader				g_copyExpandedHTileShader;
RenderingMode           g_renderingMode = kRenderingMode4KCB;
uint32_t				g_checkerboardId = 0;

VsShader				g_smaaEdgeDetectionVertexShader;
PsShader				g_smaaEdgeDetectionPixelShader;
VsShader				g_smaaBlendWeightVertexShader;
PsShader				g_smaaBlendWeightPixelShader;
VsShader				g_smaaBlendVertexShader;
PsShader				g_smaaBlendPixelShader;

float					g_cubicB = 0.25;
float					g_cubicC = 0.0;

Gnm::DepthRenderTarget	g_shadowDepthTarget;
Gnm::Texture			g_shadowMapTexture;

Gnm::DepthRenderTarget	g_depthTarget[g_numConfigs];
Gnm::Texture			g_depthTexture[g_numConfigs];

Gnm::DepthRenderTarget	g_resolvedDepthTarget[g_numConfigs];
Gnm::Texture			g_resolvedDepthTexture[g_numConfigs];

Gnm::DepthRenderTarget	g_edgeStencilTarget;
Gnm::Texture			g_edgeStencilTexture;

Gnm::RenderTarget		g_colorCheckerboardRenderTarget[g_numConfigs][2];
Gnm::Texture			g_colorCheckerboardTexture[g_numConfigs][2];

Gnm::RenderTarget		g_velocityCheckerboardRenderTarget[g_numConfigs];
Gnm::Texture			g_velocityCheckerboardTexture[g_numConfigs];

Gnm::RenderTarget		g_primIdRenderTarget[g_numConfigs];
Gnm::Texture			g_primIdTexture[g_numConfigs];

Gnm::RenderTarget		g_colorHistoryRenderTarget[2];
Gnm::Texture			g_colorHistoryTexture[2];

Gnm::RenderTarget		g_blurRenderTarget[kNumBlurTargets][2];
Gnm::Texture			g_blurTexture[kNumBlurTargets][2];

Gnm::Texture			g_cubicWeightTexture;
Gnm::Sampler			g_cubicWeightSampler;

Gnm::Texture			g_smaaAreaTexture;
Gnm::Texture			g_smaaSearchTexture;
Gnm::RenderTarget		g_smaaEdgesRenderTarget;
Gnm::Texture			g_smaaEdgesTexture;
Gnm::RenderTarget		g_smaaBlendRenderTarget;
Gnm::Texture			g_smaaBlendTexture;

// appended menu items

Framework::MenuItemText const kaRenderingModeNeoText[] = {
	{ "1080p", "Render 1080p baseline" },
	{ "4K Native", "Render using 4K Native" },
	{ "4K Checkerboard", "Render using 4K Checkerboard technique" },
	{ "4K Geometry", "Render using 4K Geometry technique" },
};

Framework::MenuItemText const kaRenderingModeBaseText[] = {
	{ "1080p", "Render 1080p baseline" },
	{ "4K Native", "Render using 4K Native" },
};

Framework::MenuItemText const kaAaModeText[] = {
	{ "None", "No antialiasing" },
	{ "FXAA", "FXAA enabled" },
	{ "SMAA", "SMAA enabled" },
};

Framework::MenuCommandBool menuCommandBoolTemporal(&g_temporalEnabled);
Framework::MenuCommandBool menuCommandBoolGradientAdjust(&g_gradientAdjustEnabled);
Framework::MenuCommandBool menuCommandBoolPsInvoke(&g_psInvokeEnabled);
Framework::MenuCommandBool menuCommandBoolAlphaUnroll(&g_alphaUnrollEnabled);
Framework::MenuCommandEnum menuCommandEnumRenderingModeNeo(kaRenderingModeNeoText, (uint32_t)(sizeof(kaRenderingModeNeoText)/sizeof(kaRenderingModeNeoText[0])), &g_renderingMode);
Framework::MenuCommandEnum menuCommandEnumRenderingModeBase(kaRenderingModeBaseText, (uint32_t)(sizeof(kaRenderingModeBaseText)/sizeof(kaRenderingModeBaseText[0])), &g_renderingMode);
Framework::MenuCommandEnum menuCommandEnumAaMode(kaAaModeText, 3, &g_aaMode);
Framework::MenuCommandBool menuCommandBoolMask(&g_maskExport);

void loadVsShader(VsShader *shader, const char *filename, Gnmx::Toolkit::Allocators *allocators)
{
	shader->m_shader = Framework::LoadVsMakeFetchShader(&shader->m_fetchShader, filename, allocators);
	SCE_GNM_ASSERT(shader->m_shader);
	Gnmx::generateInputOffsetsCache(&shader->m_offsetsTable, Gnm::kShaderStageVs, shader->m_shader);
}

void loadPsShader(PsShader *shader, const char *filename, Gnmx::Toolkit::Allocators *allocators)
{
	shader->m_shader = Framework::LoadPsShader(filename, allocators);
	SCE_GNM_ASSERT(shader->m_shader);
	Gnmx::generateInputOffsetsCache(&shader->m_offsetsTable, Gnm::kShaderStagePs, shader->m_shader);
}

void loadCsShader(CsShader *shader, const char *filename, Gnmx::Toolkit::Allocators *allocators)
{
	shader->m_shader = Framework::LoadCsShader(filename, allocators);
	SCE_GNM_ASSERT(shader->m_shader);
	Gnmx::generateInputOffsetsCache(&shader->m_offsetsTable, Gnm::kShaderStageCs, shader->m_shader);
}

void loadShaderPair(ShaderPair *shader, Gnmx::Toolkit::Allocators *allocators)
{
	char filePath[256];
	strncpy(filePath, "/app0/neo-4k-sample/", 64);
	strncat(filePath, shader->m_vertexShaderName, 128);
	strncat(filePath, "_vv.sb", 8);
	loadVsShader(&shader->m_vsShader, filePath, allocators);

	strncpy(filePath, "/app0/neo-4k-sample/", 64);
	strncat(filePath, shader->m_pixelShaderName, 128);
	strncat(filePath, "_p.sb", 8);
	loadPsShader(&shader->m_psShader, filePath, allocators);
}

Texture *loadTexture(const char* filename, Gnmx::Toolkit::Allocators *allocators)
{
	for(uint32_t i=0; i<g_textures.size(); ++i)
	{
		if(g_textures[i].m_name == filename)
			return &g_textures[i];
	}

	Texture tex;

	char filePath[256];
	snprintf(filePath, 256, "/app0/neo-4k-sample/data/%s.gnf", filename);
	int res = Framework::loadTextureFromGnf(&tex.m_texture, filePath, 0, &allocators->m_garlic);
	SCE_GNM_ASSERT(res == 0);
	tex.m_texture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // won't ever bind this texture as RWTexture, so it's OK to mark it as read-only.
	tex.m_name = filename;

	g_textures.push_back(tex);
	return &g_textures.back();
}

bool loadMesh(Gnmx::Toolkit::Allocators *allocators, Mesh &mesh, const char *dataDirPath, const char *filename)
{
	char filePath[256];
	snprintf(filePath, 256, SAMPLE_FRAMEWORK_FSROOT "/neo-4k-sample/%s/%s", dataDirPath, filename);
	FILE *file = fopen(filePath, "rb");
	SCE_GNM_ASSERT(file);

	uint32_t stride = sizeof(float)*(3+2+3+4);   

	size_t n;

	uint32_t materialIndex;
	fread(&materialIndex, sizeof(uint32_t), 1, file);

	uint32_t numVertices;
	fread(&numVertices, sizeof(uint32_t), 1, file);

	uint32_t numIndices;
	fread(&numIndices, sizeof(uint32_t), 1, file);

	const uint32_t sizeVertexBuffer = numVertices * stride;
	mesh.m_vertexBuffer = allocators->m_onion.allocate(sizeVertexBuffer, Gnm::kAlignmentOfBufferInBytes);

	const uint32_t sizeIndexBuffer = numIndices * sizeof(uint32_t);
	mesh.m_indexBuffer = allocators->m_onion.allocate(sizeIndexBuffer, Gnm::kAlignmentOfBufferInBytes);

	mesh.m_dynamicIndexBuffer = NULL;

	n = fread(mesh.m_vertexBuffer, sizeVertexBuffer, 1, file);

	n = fread(mesh.m_indexBuffer, sizeIndexBuffer, 1, file);

	fclose(file);

	mesh.m_name = filename;
	mesh.m_numVertices = numVertices;   
	mesh.m_numIndices = numIndices;    
	mesh.m_stride = stride;        
	mesh.m_position = 0;
	mesh.m_texcoord = 12;
	mesh.m_normal = 20;
	mesh.m_tangent = 32;
	mesh.m_materialIndex = materialIndex;

	return true;
}

void loadAssets(Gnmx::Toolkit::Allocators *allocators) // load textures, meshes, and shaders.
{
	for(uint32_t s = 0; s < kNumShaders; ++s)
	{
		loadShaderPair(&g_shaders[s], allocators);
	}

	for(uint32_t i = 0; i < kNumMaterials; i++)
	{
		size_t l = strlen(materialDesc[i].m_name);
		bool alphaTest = (l > 2 && materialDesc[i].m_name[l - 2] == '_' && materialDesc[i].m_name[l - 1] == 'a');
		bool alphaBlend = (l > 2 && materialDesc[i].m_name[l - 2] == '_' && materialDesc[i].m_name[l - 1] == 'b');

		if(strcmp(materialDesc[i].m_name, "SG051") == 0)
		{
			// skybox
			g_materials[i].m_shader = &g_shaders[kShaderSkybox];
		}
		else if(alphaTest)
		{
			g_materials[i].m_shader = &g_shaders[kShaderPhongAlphaTest];
		}
		else
		{
			g_materials[i].m_shader = &g_shaders[kShaderPhong];
		}

		if(materialDesc[i].m_textureDiffuse)
		{
			Texture *tex = loadTexture(materialDesc[i].m_textureDiffuse, allocators);
			g_materials[i].m_textureDiffuse = tex->m_texture;
		}
		if(materialDesc[i].m_textureSpecular)
		{
			Texture *tex = loadTexture(materialDesc[i].m_textureSpecular, allocators);
			g_materials[i].m_textureSpecular = tex->m_texture;
		}
		if(materialDesc[i].m_textureNormal)
		{
			Texture *tex = loadTexture(materialDesc[i].m_textureNormal, allocators);
			g_materials[i].m_textureNormal = tex->m_texture;
		}

		float *ks = materialDesc[i].m_specularColor;
		g_materials[i].m_specularColor = Vector4(ks[0], ks[1], ks[2], 0) * 0.4f;

		if(alphaTest)
		{
			g_materials[i].m_type = kMaterialTypeAlphaTest;
		}
		else if(alphaBlend)
		{
			g_materials[i].m_type = kMaterialTypeTranslucent;
		}
		else
		{
			g_materials[i].m_type = kMaterialTypeOpaque;
		}
	}

	for(uint32_t m = 0; m < kNumMeshes; m++)
	{
		loadMesh(allocators, g_meshes[m], "data", meshDesc[m]);
	}
}

void drawMesh(Gnmx::GnmxGfxContext *gfxc, Mesh &mesh)
{
	Gnm::Buffer buffer[4];
	buffer[0].initAsVertexBuffer(static_cast<uint8_t*>(mesh.m_vertexBuffer) + mesh.m_position, Gnm::kDataFormatR32G32B32Float, mesh.m_stride, mesh.m_numVertices);
	buffer[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's read-only.
	buffer[1].initAsVertexBuffer(static_cast<uint8_t*>(mesh.m_vertexBuffer) + mesh.m_texcoord, Gnm::kDataFormatR32G32Float, mesh.m_stride, mesh.m_numVertices);
	buffer[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's read-only.
	buffer[2].initAsVertexBuffer(static_cast<uint8_t*>(mesh.m_vertexBuffer) + mesh.m_normal, Gnm::kDataFormatR32G32B32Float, mesh.m_stride, mesh.m_numVertices);
	buffer[2].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's read-only.
	buffer[3].initAsVertexBuffer(static_cast<uint8_t*>(mesh.m_vertexBuffer) + mesh.m_tangent, Gnm::kDataFormatR32G32B32Float, mesh.m_stride, mesh.m_numVertices);
	buffer[3].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's read-only.
	gfxc->setVertexBuffers( Gnm::kShaderStageVs, 0, 4, buffer );

	// disable back face culling for alpha blend / alpha test
	Material &material = g_materials[mesh.m_materialIndex];
	Gnm::PrimitiveSetup primSetupReg;
	primSetupReg.init();
	if(material.isTranslucent() || material.isAlphaTest())
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceNone);
	else
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
	primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
	gfxc->setPrimitiveSetup(primSetupReg);

	gfxc->setPrimitiveType(Gnm::kPrimitiveTypeTriList);
	gfxc->setIndexSize(Gnm::kIndexSize32);

	if(material.isTranslucent())
		gfxc->drawIndex(mesh.m_numIndices, mesh.m_dynamicIndexBuffer);
	else
		gfxc->drawIndex(mesh.m_numIndices, mesh.m_indexBuffer);
}

void alphaTestPrepass(Gnmx::GnmxGfxContext *gfxc,
   			const Matrix4 &matView,
			const Matrix4 &matProj)
{
	gfxc->pushMarker("alpha test prepass");

	if (g_maskExport && g_renderingMode == kRenderingMode4KCB)
	{
		// With mask export enabled, we run a shader at pixel rate that exports a coverage mask for the two samples. This can give higher performance over unrolling.
		gfxc->setVsShader(g_shaders[kShaderAlphaTestMask].m_vsShader.m_shader, 0, g_shaders[kShaderAlphaTestMask].m_vsShader.m_fetchShader, &g_shaders[kShaderAlphaTestMask].m_vsShader.m_offsetsTable);
		gfxc->setPsShader(g_shaders[kShaderAlphaTestMask].m_psShader.m_shader, &g_shaders[kShaderAlphaTestMask].m_psShader.m_offsetsTable);

		// mask export requires checkerboard ID
		struct PsConstants
		{
			uint32_t checkerboardId;
		};

		PsConstants* psConstants = (PsConstants*)(gfxc->allocateFromCommandBuffer(sizeof(PsConstants), Gnm::kEmbeddedDataAlignment4));
		psConstants->checkerboardId = g_checkerboardId;
		Gnm::Buffer psCb;
		psCb.initAsConstantBuffer(psConstants, sizeof(PsConstants));
		psCb.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &psCb);
	}
	else
	{
		// With mask export disabled, we unroll (i.e run at sample rate) a standard discard shader
		gfxc->setPsShaderRate(Gnm::kPsShaderRatePerSample);

		gfxc->setVsShader(g_shaders[kShaderAlphaTest].m_vsShader.m_shader, 0, g_shaders[kShaderAlphaTest].m_vsShader.m_fetchShader, &g_shaders[kShaderAlphaTest].m_vsShader.m_offsetsTable);
		gfxc->setPsShader(g_shaders[kShaderAlphaTest].m_psShader.m_shader, &g_shaders[kShaderAlphaTest].m_psShader.m_offsetsTable);
	}

	for(uint32_t m = 0; m < kNumMeshes; m++)
	{
		Mesh &mesh = g_meshes[m];
		Material &material = g_materials[mesh.m_materialIndex];

		if(!(material.m_type & kMaterialTypeAlphaTest))
			continue;

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &material.m_textureDiffuse);
		gfxc->setSamplers( Gnm::kShaderStagePs, 0, 1, &g_samplerAniso );

		struct VsConstants
		{
			Matrix4Unaligned m_matWorldViewProj;
		};
		VsConstants* vsConstants = (VsConstants*)gfxc->allocateFromCommandBuffer(sizeof(VsConstants), Gnm::kEmbeddedDataAlignment4);
		Gnm::Buffer vsCb;
		vsCb.initAsConstantBuffer(vsConstants, sizeof(VsConstants));
		vsCb.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &vsCb);

		Matrix4 matWorld = Matrix4::identity();
		Matrix4 matWorldView = matView * matWorld;
		Matrix4 matWorldViewProj = matProj * matWorldView;

		vsConstants->m_matWorldViewProj = transpose(matWorldViewProj);

		if(g_renderingMode == kRenderingMode4KCB || g_renderingMode == kRenderingMode4KG)
		{
			// use the index buffer address as the object ID
			uint32_t objectId = (uint32_t)((uint64_t)mesh.m_indexBuffer << 16);
			gfxc->m_dcb.setObjectId(objectId);
		}

		drawMesh(gfxc, mesh);
	}

	// restore state
	gfxc->setPsShaderRate(Gnm::kPsShaderRatePerPixel);

	gfxc->popMarker();
}

void mainPass(Gnmx::GnmxGfxContext *gfxc,
			const Gnm::Texture* shadowMapTexture,
			const Matrix4 &matView,
			const Matrix4 &matProj,
			const Matrix4 &matShadow,
			const Vector3 lightViewPos,
			const Vector4 lightColor,
			const Vector4 ambientColor,
			uint32_t materialFilter)
{
	for(uint32_t m = 0; m < kNumMeshes; m++)
	{
		Mesh &mesh = g_meshes[m];
		Material &material = g_materials[mesh.m_materialIndex];

		if(!(material.m_type & materialFilter))
			continue;

		// when alpha unroll is enabled the discard is moved to the alpha_depth_only shader, so for performance we disable discard in the main pass.
		PsShader* psShader = &material.m_shader->m_psShader;
		if(g_alphaUnrollEnabled && psShader->m_shader == g_shaders[kShaderPhongAlphaTest].m_psShader.m_shader)
			psShader = &g_shaders[kShaderPhong].m_psShader;

		gfxc->setVsShader(material.m_shader->m_vsShader.m_shader, 0, material.m_shader->m_vsShader.m_fetchShader, &material.m_shader->m_vsShader.m_offsetsTable);
		gfxc->setPsShader(psShader->m_shader, &psShader->m_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &material.m_textureDiffuse);
		gfxc->setTextures(Gnm::kShaderStagePs, 1, 1, shadowMapTexture);
		gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &material.m_textureSpecular);
		gfxc->setTextures(Gnm::kShaderStagePs, 3, 1, &material.m_textureNormal);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &g_samplerAniso );
		gfxc->setSamplers(Gnm::kShaderStagePs, 1, 1, &g_samplerShadow );

		struct VsConstants
		{
			Matrix4Unaligned m_matWorldView;
			Matrix4Unaligned m_matWorldViewProj;
			Matrix4Unaligned m_matWorldViewProjPrev;
			Matrix4Unaligned m_matWorldShadow;
		};
		VsConstants* vsConstants = (VsConstants*)gfxc->allocateFromCommandBuffer(sizeof(VsConstants), Gnm::kEmbeddedDataAlignment4);
		Gnm::Buffer vsCb;
		vsCb.initAsConstantBuffer(vsConstants, sizeof(VsConstants));
		vsCb.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &vsCb);

		Matrix4 matWorld = Matrix4::identity();
		Matrix4 matWorldView = matView * matWorld;
		Matrix4 matWorldViewProj = matProj * matWorldView;
		Matrix4 matWorldShadow = matShadow * matWorld;

		vsConstants->m_matWorldView = transpose(matWorldView);
		vsConstants->m_matWorldViewProj = transpose(matWorldViewProj);
		vsConstants->m_matWorldViewProjPrev = transpose(mesh.m_matWorldViewProjPrev);
		vsConstants->m_matWorldShadow = transpose(matWorldShadow);

		struct PsConstants
		{
			Vector4Unaligned m_lightPosition;
			Vector4Unaligned m_lightColor;
			Vector4Unaligned m_ambientColor;
			Vector4Unaligned m_specularColor;
			bool			 m_alphaTest;
			uint32_t		 m_debugView;
		};
		PsConstants *psConstants = static_cast<PsConstants*>(gfxc->allocateFromCommandBuffer(sizeof(PsConstants), Gnm::kEmbeddedDataAlignment4));
		Gnm::Buffer psCb;
		psCb.initAsConstantBuffer(psConstants, sizeof(*psConstants));
		psCb.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
		psConstants->m_lightPosition = ToVector4Unaligned( (Vector4)(lightViewPos) );
		psConstants->m_lightColor = ToVector4Unaligned(lightColor);
		psConstants->m_ambientColor = ToVector4Unaligned(ambientColor);
		psConstants->m_specularColor = ToVector4Unaligned(material.m_specularColor);
		psConstants->m_alphaTest = (!g_alphaUnrollEnabled) && material.isAlphaTest();
		psConstants->m_debugView = g_debugView;
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &psCb);

		if(g_renderingMode == kRenderingMode4KCB || g_renderingMode == kRenderingMode4KG)
		{
			// use the index buffer address as the object ID
			uint32_t objectId = (uint32_t)((uint64_t)mesh.m_indexBuffer << 16);
			gfxc->m_dcb.setObjectId(objectId);
		}

		drawMesh(gfxc, mesh);

		// save worldViewProj matrix so we can calculate velocities next frame
		mesh.m_matWorldViewProjPrev = matWorldViewProj;
	}
}

void shadowPass(Gnmx::GnmxGfxContext *gfxc,
			const Matrix4 *matrixView,
			const Matrix4 *matrixProjection)
{
	gfxc->pushMarker("shadow pass");
	for(uint32_t m = 0; m < kNumMeshes; m++)
	{
		Mesh &mesh = g_meshes[m];
		Material &material = g_materials[mesh.m_materialIndex];

		gfxc->setVsShader(g_shaders[kShaderShadow].m_vsShader.m_shader, 0, g_shaders[kShaderShadow].m_vsShader.m_fetchShader, &g_shaders[kShaderShadow].m_vsShader.m_offsetsTable);

		if(material.isAlphaTest() || material.isTranslucent())
		{
			gfxc->setPsShader(g_shaders[kShaderShadow].m_psShader.m_shader, &g_shaders[kShaderShadow].m_psShader.m_offsetsTable);
			gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &material.m_textureDiffuse);
			gfxc->setSamplers( Gnm::kShaderStagePs, 0, 1, &g_samplerAniso );
		}
		else
		{
			gfxc->setPsShader(NULL, NULL);
		}

		struct VsConstants
		{
			Matrix4Unaligned m_world;
			Matrix4Unaligned m_view;
			Matrix4Unaligned m_projection;
		};

		VsConstants* vsConstants = (VsConstants*)gfxc->allocateFromCommandBuffer(sizeof(VsConstants), Gnm::kEmbeddedDataAlignment4);
		Gnm::Buffer vsCb;
		vsCb.initAsConstantBuffer(vsConstants, sizeof(VsConstants));
		vsCb.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so it's read-only.
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &vsCb);

		vsConstants->m_view = transpose(*matrixView);
		vsConstants->m_projection = transpose(*matrixProjection);
		vsConstants->m_world = transpose(Matrix4::identity());

		drawMesh(gfxc, mesh);
	}
	gfxc->popMarker();
}

// transparency sorting
struct TriangleDist
{
	uint32_t	index;
	float		dist;
};

int compareTriangleDist(const void *a, const void* b)
{
	float d = ((TriangleDist*)b)->dist - ((TriangleDist*)a)->dist;
	return d < 0 ? -1 : 1;
}

void sortTrisBackToFront(uint32_t *indicesOut, const uint32_t *indices, const uint8_t *vertices, uint32_t numIndices, uint32_t stride, Vector3 eyePos)
{
	static const uint32_t kMaxTrisPerMesh = 50000;
	static TriangleDist tris[kMaxTrisPerMesh];

	uint32_t numTris = numIndices/3;
	SCE_GNM_ASSERT(numTris < kMaxTrisPerMesh);
	SCE_GNM_ASSERT(numIndices % 3 == 0);
	SCE_GNM_ASSERT(stride % 16 == 0);

	// get per triangle distance from camera
	for(uint32_t i=0; i<numTris; ++i)
	{
		uint32_t i0 = indices[i*3 + 0];
		uint32_t i1 = indices[i*3 + 1];
		uint32_t i2 = indices[i*3 + 2];
		Vector3 p0 = *(Vector3*)(vertices + i0*stride);
		Vector3 p1 = *(Vector3*)(vertices + i1*stride);
		Vector3 p2 = *(Vector3*)(vertices + i2*stride);
		Vector3 c = (p0+p1+p2)/3;
		float d = lengthSqr(c - eyePos);
		tris[i].index = i;
		tris[i].dist = d;
	}

	// sort
	qsort(tris, numTris, sizeof(TriangleDist), compareTriangleDist);

	// re-index
	for(uint32_t i=0; i<numTris; ++i)
	{
		uint32_t tri = tris[i].index;
		indicesOut[i*3 + 0] = indices[tri*3 + 0];
		indicesOut[i*3 + 1] = indices[tri*3 + 1];
		indicesOut[i*3 + 2] = indices[tri*3 + 2];
	}
}

void initRenderTarget(uint32_t width, uint32_t height, Gnm::DataFormat format, Gnm::NumSamples numSamples, Gnm::NumFragments numFragments, Framework::GnmSampleFramework *framework, Gnm::RenderTarget *renderTarget, Gnm::Texture *tex)
{
	SCE_GNM_ASSERT(renderTarget);

	Gnm::TileMode tileMode;
	GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeColorTarget, format, 1 << (uint32_t)numFragments);

	Gnm::RenderTargetSpec spec;
	spec.init();
	spec.m_width = width;
	spec.m_height = height;
	spec.m_pitch = 0;
	spec.m_numSlices = 1;
	spec.m_colorFormat = format;
	spec.m_colorTileModeHint = tileMode;
	spec.m_minGpuMode = Gnm::getGpuMode();
	spec.m_numSamples = numSamples;
	spec.m_numFragments = numFragments;

	renderTarget->init(&spec);
	renderTarget->setAddresses(framework->m_garlicAllocator.allocate(renderTarget->getColorSizeAlign()), 0, 0);

	memset(renderTarget->getBaseAddress(), 0x7f, renderTarget->getColorSizeAlign().m_size);

	if(tex)
	{
		tex->initFromRenderTarget(renderTarget, false);
		tex->setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	}
}

void initDepthRenderTarget(uint32_t width, uint32_t height, Gnm::ZFormat zFormat, Gnm::StencilFormat stencilFormat, Gnm::NumFragments numFragments, bool htile, bool texHtile, Framework::GnmSampleFramework *framework, Gnm::DepthRenderTarget *depthTarget, Gnm::Texture *depthTex, Gnm::Texture *stencilTex)
{
	bool depthEnabled = zFormat != Gnm::kZFormatInvalid;
	bool stencilEnabled = stencilFormat != Gnm::kStencilInvalid;

	SCE_GNM_ASSERT(depthTarget);
	SCE_GNM_ASSERT(depthEnabled || stencilEnabled);
	SCE_GNM_ASSERT(depthEnabled || depthTex == NULL);
	SCE_GNM_ASSERT(stencilEnabled || stencilTex == NULL);

	Gnm::TileMode tileMode;

	if(depthEnabled)
	{
		Gnm::DataFormat depthFormat = Gnm::DataFormat::build(zFormat);
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeDepthOnlyTarget, depthFormat, 1 << (uint32_t)numFragments);
	}
	
	if(stencilEnabled)
	{
		// force kTileModeDepth_2dThin_64 if stencil is enabled (https://ps4.siedev.net/technotes/view/677/1)
		tileMode = Gnm::kTileModeDepth_2dThin_64;
	}

	Gnm::DepthRenderTargetSpec spec;
	spec.init();
	spec.m_width = width;
	spec.m_height = height;
	spec.m_pitch = 0;
	spec.m_numSlices = 1;
	spec.m_zFormat = zFormat;
	spec.m_stencilFormat = stencilTex ? Gnm::kStencil8 : Gnm::kStencilInvalid;
	spec.m_tileModeHint = tileMode;
	spec.m_minGpuMode = Gnm::getGpuMode();
	spec.m_numFragments = numFragments;
	spec.m_flags.enableHtileAcceleration = htile ? 1 : 0;
	spec.m_flags.enableTextureWithoutDecompress = texHtile ? 1 : 0;
 
	depthTarget->init(&spec);

	if(depthEnabled)
	{
		void *baseAddr = framework->m_garlicAllocator.allocate(depthTarget->getZSizeAlign());
		depthTarget->setZReadAddress(baseAddr);
		depthTarget->setZWriteAddress(baseAddr);
	}

	if(stencilEnabled)
	{
		void *stencilAddr = framework->m_garlicAllocator.allocate(depthTarget->getStencilSizeAlign());
		depthTarget->setStencilReadAddress(stencilAddr);
		depthTarget->setStencilWriteAddress(stencilAddr);
	}
	
	if(htile)
	{
		void *htileAddr = framework->m_garlicAllocator.allocate(depthTarget->getHtileSizeAlign());
		depthTarget->setHtileAddress(htileAddr);
	}

	if(depthTex)
	{
		depthTex->initFromDepthRenderTarget(depthTarget, false);
		depthTex->setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
		if(texHtile)
		{
			depthTex->setMetadataAddress(depthTarget->getHtileAddress());
			depthTex->setMetadataCompressionEnable(true);
		}
	}

	if(stencilTex)
	{
		stencilTex->initFromStencilTarget(depthTarget, Gnm::kTextureChannelTypeUInt, false);
		stencilTex->setResourceMemoryType(Gnm::kResourceMemoryTypeRO);
	}
}

void copyTexture(Gnmx::GnmxGfxContext *gfxc, Gnm::Texture *src, Gnm::RenderTarget *tgt)
{
	gfxc->setDepthRenderTarget(0);
	gfxc->setRenderTarget(0, tgt);
	gfxc->setupScreenViewport(0, 0, tgt->getWidth(), tgt->getHeight(), 0.5f, 0.5f);
	gfxc->setVsShader(g_fullScreenVertexShader.m_shader, 0, (void*)0, &g_fullScreenVertexShader.m_offsetsTable);
	gfxc->setPsShader(g_passThroughPixelShader.m_shader, &g_passThroughPixelShader.m_offsetsTable);
	gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, src);
	gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &g_samplerLinear);
	gfxc->setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
	gfxc->drawIndexAuto(4);
	gfxc->waitForGraphicsWrites(tgt->getBaseAddress256ByteBlocks(), tgt->getSliceSizeInBytes()*1/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);
}

void applyFxaa(Gnmx::GnmxGfxContext *gfxc, Gnm::Texture *src, Gnm::RenderTarget *tgt)
{
	gfxc->pushMarker("FXAA");
	gfxc->setRenderTarget(0, tgt);
	gfxc->setDepthRenderTarget(0);
	gfxc->setupScreenViewport(0, 0, tgt->getWidth(), tgt->getHeight(), 0.5f, 0.5f);
	gfxc->setVsShader(g_shaders[kShaderFxaa].m_vsShader.m_shader, 0, (void*)0, &g_shaders[kShaderFxaa].m_vsShader.m_offsetsTable);
	gfxc->setPsShader(g_shaders[kShaderFxaa].m_psShader.m_shader, &g_shaders[kShaderFxaa].m_psShader.m_offsetsTable);
	gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, src);
	gfxc->setSamplers(Gnm::kShaderStagePs, 3, 1, &g_samplerLinear);
	gfxc->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
	gfxc->drawIndexAuto(3);
	gfxc->popMarker();
}

void applySmaa(Gnmx::GnmxGfxContext *gfxc, Gnm::Texture *src, Gnm::RenderTarget *tgt)
{
	gfxc->pushMarker("SMAA");

	Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &g_smaaEdgesRenderTarget, Vector4(0,0,0,0));
	Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &g_smaaBlendRenderTarget, Vector4(0,0,0,0));

	// edge detection
	{
		gfxc->pushMarker("edge detection");

		// In 4KCB mode, the checkerboard resolve shader is configured to output primitive edges to the stencil buffer.
		// We use this to accelerate the SMAA edge detection pass by configuring stencil to only pass for these primitive edges (where stencil value = 0xff).
		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthEnable(false);
		dsc.setStencilEnable(true);
		dsc.setStencilFunction(g_renderingMode == kRenderingMode4KCB ? Gnm::kCompareFuncEqual : Gnm::kCompareFuncAlways);
		gfxc->setDepthStencilControl(dsc);

		Gnm::StencilOpControl stencilOpControl;
		stencilOpControl.init();
		stencilOpControl.setStencilOps(Gnm::kStencilOpKeep, Gnm::kStencilOpReplaceOp, Gnm::kStencilOpKeep);
		gfxc->setStencilOpControl( stencilOpControl );

		Gnm::StencilControl stencilControl;
		stencilControl.m_testVal = 0xff;
		stencilControl.m_mask = 0xff;
		stencilControl.m_writeMask = 0xff;
		stencilControl.m_opVal = 1;
		gfxc->setStencil(stencilControl);

		auto srcNoSrgb = *src;
		srcNoSrgb.setChannelType(Gnm::kTextureChannelTypeUNorm);

		Gnm::RenderTarget *renderTarget = &g_smaaEdgesRenderTarget;
		gfxc->setRenderTarget(0, renderTarget);
		gfxc->setupScreenViewport(0, 0, renderTarget->getWidth(), renderTarget->getHeight(), 0.5f, 0.5f);
		gfxc->setDepthRenderTarget(&g_edgeStencilTarget);
		gfxc->setVsShader(g_smaaEdgeDetectionVertexShader.m_shader, 0, (void*)0, &g_smaaEdgeDetectionVertexShader.m_offsetsTable);
		gfxc->setPsShader(g_smaaEdgeDetectionPixelShader.m_shader, &g_smaaEdgeDetectionPixelShader.m_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &srcNoSrgb);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &g_samplerLinear);
		gfxc->setSamplers(Gnm::kShaderStagePs, 1, 1, &g_samplerPoint);
		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeTriList);
		gfxc->setIndexSize(Gnm::kIndexSize16);
		gfxc->drawIndexAuto(3);
		gfxc->waitForGraphicsWrites(renderTarget->getBaseAddress256ByteBlocks(), renderTarget->getSliceSizeInBytes()*1/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);

		gfxc->popMarker();
	}

	// blend weight
	{
		gfxc->pushMarker("blend weight");

		Gnm::StencilOpControl stencilOpControl;
		stencilOpControl.init();
		stencilOpControl.setStencilOps(Gnm::kStencilOpKeep, Gnm::kStencilOpKeep, Gnm::kStencilOpKeep);
		gfxc->setStencilOpControl( stencilOpControl );

		Gnm::StencilControl stencilControl;
		stencilControl.m_testVal = 1;
		stencilControl.m_mask = 0xff;
		stencilControl.m_writeMask = 0;
		stencilControl.m_opVal = 0;
		gfxc->setStencil(stencilControl);

		Gnm::RenderTarget *renderTarget = &g_smaaBlendRenderTarget;
		gfxc->setRenderTarget(0, renderTarget);
		gfxc->setupScreenViewport(0, 0, renderTarget->getWidth(), renderTarget->getHeight(), 0.5f, 0.5f);
		gfxc->setDepthRenderTarget(&g_edgeStencilTarget);
		gfxc->setVsShader(g_smaaBlendWeightVertexShader.m_shader, 0, (void*)0, &g_smaaBlendWeightVertexShader.m_offsetsTable);
		gfxc->setPsShader(g_smaaBlendWeightPixelShader.m_shader, &g_smaaBlendWeightPixelShader.m_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &g_smaaEdgesTexture);
		gfxc->setTextures(Gnm::kShaderStagePs, 1, 1, &g_smaaAreaTexture);
		gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &g_smaaSearchTexture);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &g_samplerLinear);
		gfxc->setSamplers(Gnm::kShaderStagePs, 1, 1, &g_samplerPoint);
		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeTriList);
		gfxc->setIndexSize(Gnm::kIndexSize16);
		gfxc->drawIndexAuto(3);
		gfxc->waitForGraphicsWrites(renderTarget->getBaseAddress256ByteBlocks(), renderTarget->getSliceSizeInBytes()*1/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);

		gfxc->popMarker();
	}

	// blend
	{
		gfxc->pushMarker("blend");

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthEnable(false);
		dsc.setStencilEnable(false);
		gfxc->setDepthStencilControl(dsc);

		Gnm::RenderTarget *renderTarget = tgt;
		gfxc->setRenderTarget(0, renderTarget);
		gfxc->setupScreenViewport(0, 0, renderTarget->getWidth(), renderTarget->getHeight(), 0.5f, 0.5f);
		gfxc->setDepthRenderTarget(NULL);
		gfxc->setVsShader(g_smaaBlendVertexShader.m_shader, 0, (void*)0, &g_smaaBlendVertexShader.m_offsetsTable);
		gfxc->setPsShader(g_smaaBlendPixelShader.m_shader, &g_smaaBlendPixelShader.m_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, src);
		gfxc->setTextures(Gnm::kShaderStagePs, 1, 1, &g_smaaBlendTexture);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &g_samplerLinear);
		gfxc->setSamplers(Gnm::kShaderStagePs, 1, 1, &g_samplerPoint);
		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeTriList);
		gfxc->setIndexSize(Gnm::kIndexSize16);
		gfxc->drawIndexAuto(3);
		gfxc->waitForGraphicsWrites(renderTarget->getBaseAddress256ByteBlocks(), renderTarget->getSliceSizeInBytes()*1/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);

		gfxc->popMarker();
	}

	gfxc->popMarker();
}

void applyBloom(Gnmx::GnmxGfxContext *gfxc, Matrix4 matView, Matrix4 matProj)
{
	if(!g_bloomEnabled)
	{
		copyTexture(gfxc, &g_colorCheckerboardTexture[g_renderingMode][0], &g_colorCheckerboardRenderTarget[g_renderingMode][1]);
		return;
	}

	Matrix4 matWorldViewProj = matProj * matView;
	Vector4 sunPos(-4,14,14,1);
	Vector4 sunPosNdc = matWorldViewProj * sunPos;
	sunPosNdc /= sunPosNdc.getW();

	gfxc->pushMarker("bloom");

	// disable EQAA during downsample / blur
	gfxc->m_dcb.setScanModeControl(Gnm::kScanModeControlAaDisable, Gnm::kScanModeControlViewportScissorEnable);

	struct BloomConstants
	{
		uint32_t debugView;
		float sunPos[2];
	};

	BloomConstants* bloomConstants = (BloomConstants*)(gfxc->allocateFromCommandBuffer(sizeof(BloomConstants), Gnm::kEmbeddedDataAlignment4));
	bloomConstants->debugView = g_debugView;
	bloomConstants->sunPos[0] = sunPosNdc.getX();
	bloomConstants->sunPos[1] = sunPosNdc.getY();
	Gnm::Buffer bloomCb;
	bloomCb.initAsConstantBuffer(bloomConstants, sizeof(BloomConstants));
	bloomCb.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

	// perform bright pass on checkerboard color buffer, and downsample to 1/2 width * 1/4 height
	{
		gfxc->pushMarker("bright pass");
		Gnm::RenderTarget *renderTarget = &g_blurRenderTarget[0][0];
		gfxc->setRenderTarget(0, renderTarget);
		gfxc->setupScreenViewport(0, 0, renderTarget->getWidth(), renderTarget->getHeight(), 0.5f, 0.5f);
		gfxc->setDepthRenderTarget(NULL);
		gfxc->setVsShader(g_fullScreenVertexShader.m_shader, 0, (void*)0, &g_fullScreenVertexShader.m_offsetsTable);
		gfxc->setPsShader(g_brightPassPixelShader.m_shader, &g_brightPassPixelShader.m_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &g_colorCheckerboardTexture[g_renderingMode][0]);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &g_samplerLinear);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &bloomCb);
		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
		gfxc->setIndexSize(Gnm::kIndexSize16);
		gfxc->drawIndexAuto(4);
		gfxc->waitForGraphicsWrites(renderTarget->getBaseAddress256ByteBlocks(), renderTarget->getSliceSizeInBytes()*1/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);
		gfxc->popMarker();
	}

	// blur + downsample
	for(uint32_t i=0; i<kNumBlurTargets; ++i)
	{
		// horizontal blur
		{
			gfxc->pushMarker("blur h");
			Gnm::RenderTarget *renderTarget = &g_blurRenderTarget[i][1];
			gfxc->setRenderTarget(0, renderTarget);
			gfxc->setupScreenViewport(0, 0, renderTarget->getWidth(), renderTarget->getHeight(), 0.5f, 0.5f);
			gfxc->setDepthRenderTarget(NULL);
			gfxc->setVsShader(g_fullScreenVertexShader.m_shader, 0, (void*)0, &g_fullScreenVertexShader.m_offsetsTable);
			gfxc->setPsShader(g_gaussianHPixelShader.m_shader, &g_gaussianHPixelShader.m_offsetsTable);
			gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &g_blurTexture[i][0]);
			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &g_samplerLinear);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &bloomCb);
			gfxc->setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
			gfxc->setIndexSize(Gnm::kIndexSize16);
			gfxc->drawIndexAuto(4);
			gfxc->waitForGraphicsWrites(renderTarget->getBaseAddress256ByteBlocks(), renderTarget->getSliceSizeInBytes()*1/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);
			gfxc->popMarker();
		}

		// vertical blur
		{
			gfxc->pushMarker("blur v");
			Gnm::RenderTarget *renderTarget = &g_blurRenderTarget[i][0];
			gfxc->setRenderTarget(0, renderTarget);
			gfxc->setupScreenViewport(0, 0, renderTarget->getWidth(), renderTarget->getHeight(), 0.5f, 0.5f);
			gfxc->setDepthRenderTarget(NULL);
			gfxc->setVsShader(g_fullScreenVertexShader.m_shader, 0, (void*)0, &g_fullScreenVertexShader.m_offsetsTable);
			gfxc->setPsShader(g_gaussianVPixelShader.m_shader, &g_gaussianVPixelShader.m_offsetsTable);
			gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &g_blurTexture[i][1]);
			gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &g_samplerLinear);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &bloomCb);
			gfxc->setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
			gfxc->setIndexSize(Gnm::kIndexSize16);
			gfxc->drawIndexAuto(4);
			gfxc->waitForGraphicsWrites(renderTarget->getBaseAddress256ByteBlocks(), renderTarget->getSliceSizeInBytes()*1/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);
			gfxc->popMarker();
		}

		// downsample
		if(i < kNumBlurTargets-1)
		{
			gfxc->pushMarker("downsample");
			copyTexture(gfxc, &g_blurTexture[i][0], &g_blurRenderTarget[i+1][0]);
			gfxc->popMarker();
		}
	}

	// radial blur
	{
		gfxc->pushMarker("radial blur");
		Gnm::RenderTarget *renderTarget = &g_blurRenderTarget[1][1];
		gfxc->setRenderTarget(0, renderTarget);
		gfxc->setupScreenViewport(0, 0, renderTarget->getWidth(), renderTarget->getHeight(), 0.5f, 0.5f);
		gfxc->setDepthRenderTarget(NULL);
		gfxc->setVsShader(g_fullScreenVertexShader.m_shader, 0, (void*)0, &g_fullScreenVertexShader.m_offsetsTable);
		gfxc->setPsShader(g_radialBlurPixelShader.m_shader, &g_radialBlurPixelShader.m_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &g_blurTexture[0][0]);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &g_samplerLinear);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &bloomCb);
		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
		gfxc->setIndexSize(Gnm::kIndexSize16);
		gfxc->drawIndexAuto(4);
		gfxc->waitForGraphicsWrites(renderTarget->getBaseAddress256ByteBlocks(), renderTarget->getSliceSizeInBytes()*1/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);
		gfxc->popMarker();
	}

	// composite bloom with the checkerboard color target
	// note: want to sample the bloom textures at sample location, so enable EQAA
	gfxc->m_dcb.setScanModeControl(Gnm::kScanModeControlAaEnable, Gnm::kScanModeControlViewportScissorEnable);
	{
		gfxc->pushMarker("composite");
		Gnm::RenderTarget *renderTarget = &g_colorCheckerboardRenderTarget[g_renderingMode][1];
		gfxc->setRenderTarget(0, renderTarget);
		gfxc->setupScreenViewport(0, 0, renderTarget->getWidth(), renderTarget->getHeight(), 0.5f, 0.5f);
		gfxc->setDepthRenderTarget(NULL);
		gfxc->setVsShader(g_fullScreenVertexShader.m_shader, 0, (void*)0, &g_fullScreenVertexShader.m_offsetsTable);
		gfxc->setPsShader(g_bloomCompositePixelShader.m_shader, &g_bloomCompositePixelShader.m_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &g_blurTexture[0][0]);
		gfxc->setTextures(Gnm::kShaderStagePs, 1, 1, &g_blurTexture[1][0]);
		gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &g_blurTexture[2][0]);
		gfxc->setTextures(Gnm::kShaderStagePs, 3, 1, &g_blurTexture[3][0]);
		gfxc->setTextures(Gnm::kShaderStagePs, 4, 1, &g_blurTexture[4][0]);
		gfxc->setTextures(Gnm::kShaderStagePs, 5, 1, &g_blurTexture[5][0]);
		gfxc->setTextures(Gnm::kShaderStagePs, 6, 1, &g_blurTexture[1][1]);
		gfxc->setTextures(Gnm::kShaderStagePs, 7, 1, &g_colorCheckerboardTexture[g_renderingMode][0]);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &g_samplerPoint);
		gfxc->setSamplers(Gnm::kShaderStagePs, 1, 1, &g_samplerLinear);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &bloomCb);
		gfxc->setPrimitiveType(Gnm::kPrimitiveTypeQuadList);
		gfxc->setIndexSize(Gnm::kIndexSize16);
		gfxc->drawIndexAuto(4);
		gfxc->waitForGraphicsWrites(renderTarget->getBaseAddress256ByteBlocks(), renderTarget->getSliceSizeInBytes()*1/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);
		gfxc->popMarker();
	}

	gfxc->popMarker();
}

float cubicFilter(float x, float B, float C)
{
    x = abs(x);
    const auto x2 = x * x;
    const auto x3 = x * x * x;

    if (x < 1.0)
    {
        return ((12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B));
    }
    else if (x < 2.0)
    {
        return ((-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C));
    }
    else
    {
        return 0.01;
    }
}

void updateCubicWeights(const Gnm::Texture& tex)
{
	__fp16* data = (__fp16*)tex.getBaseAddress();

	const auto w = tex.getWidth();
	const auto h = tex.getHeight();
	const float scale = 3;

	for(uint32_t y = 0; y < h; ++y)
	{
		for(uint32_t x = 0; x < w; ++x)
		{
			// we limit the wait to 0.02 here so that we don't get into trouble later when summing up in half precision.
			// the maximum value here is 36 so a minimum of 0.02 is about 11b range.
			data[x] = std::max(0.02f,cubicFilter(scale * x / (float)w,g_cubicB,g_cubicC) * cubicFilter(scale * y / (float)h,g_cubicB,g_cubicC));
		}
		data += tex.getPitch();
	}
}

void setGradientAdjust(Gnmx::GnmxGfxContext* gfxc, float factor0, float factor1, float factor2, float factor3, bool signNegationEnabled)
{
	auto f0 = sce::Gnmx::convertF32ToU2_6(g_gradientAdjustEnabled ? factor0 : 1.0);
	auto f1 = sce::Gnmx::convertF32ToS2_6(g_gradientAdjustEnabled ? factor1 : 0.0);
	auto f2 = sce::Gnmx::convertF32ToS2_6(g_gradientAdjustEnabled ? factor2 : 0.0);
	auto f3 = sce::Gnmx::convertF32ToU2_6(g_gradientAdjustEnabled ? factor3 : 1.0);

	// The gradient adjust factors are global registers, so we need to ensure that all texture reads
	// have finished before we're changing them.

	// First we get a label and initialise it from the GPU, which avoids the need for CPU interaction.
	uint32_t* label = (uint32_t*)gfxc->allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
	gfxc->m_dcb.dmaData(Gnm::kDmaDataDstMemory, (uint64_t)label, Gnm::kDmaDataSrcData, 0, 4, Gnm::kDmaDataBlockingEnable);
			
	// Then we wait only until all pixel shader wavefronts have completed. There is no need to flush anything else,
	// since we only care about shaders that use gradient adjust instructions, which should only be the pixel shaders.
	gfxc->writeAtEndOfShader(Gnm::kEosPsDone, label, 1);
	gfxc->waitOnAddress(label, ~0, Gnm::kWaitCompareFuncEqual, 1);

	// Now that we know no texture queries with gradient computation are outstanding, we change the register.
	gfxc->setTextureGradientFactors(f0, f1, f2, f3, 
		Gnm::kTextureGradientFactor01SignNegationBehaviorDisabled, 
		signNegationEnabled ? Gnm::kTextureGradientFactor10SignNegationBehaviorEnabled : Gnm::kTextureGradientFactor10SignNegationBehaviorDisabled);			// The gradient factor 10 is negated for sample 1
}

// Copies the HTILE buffer from 'source' to 'destination', replacing the ZMASK field with 0xf ("expanded")
void copyExpandedHTile(Gnmx::GnmxGfxContext& gfxc, sce::Gnm::DepthRenderTarget *src, sce::Gnm::DepthRenderTarget *dst)
{
	Gnm::Buffer htileBufferSrc;
	const unsigned dwordsSrc = src->getHtileSliceSizeInBytes() / sizeof(uint32_t);
	htileBufferSrc.initAsRegularBuffer(src->getHtileAddress(), sizeof(uint32_t), dwordsSrc);
	htileBufferSrc.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

	Gnm::Buffer htileBufferDst;
	const unsigned dwordsDst = dst->getHtileSliceSizeInBytes() / sizeof(uint32_t);
	htileBufferDst.initAsRegularBuffer(dst->getHtileAddress(), sizeof(uint32_t), dwordsDst);
	htileBufferDst.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);

	SCE_GNM_ASSERT(dwordsDst == dwordsSrc);

	gfxc.pushMarker("Copy Expanded HTile");

	gfxc.setCsShader(g_copyExpandedHTileShader.m_shader, &g_copyExpandedHTileShader.m_offsetsTable);

	gfxc.setBuffers(Gnm::kShaderStageCs, 0, 1, &htileBufferSrc);
	gfxc.setRwBuffers(Gnm::kShaderStageCs, 0, 1, &htileBufferDst);

	gfxc.dispatch(dwordsSrc/64, 1, 1);

	Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc.m_dcb);

	gfxc.popMarker();
}

int main(int argc, const char *argv[])
{
	Vector3 lightPos = Vector3(-4, 10, 3);
	Vector3 lightTgt = Vector3(0, 6, 0);
	Vector3 lightUp = Vector3(0, 1, 0);

	Framework::GnmSampleFramework framework;

	framework.m_config.m_lightPositionX = lightPos;
	framework.m_config.m_lightTargetX = lightTgt;
	framework.m_config.m_lightUpX = lightUp;
	framework.m_config.m_ambientColor = 0; // black
	framework.m_config.m_lookAtPosition = (Vector3)Point3(-9.5f, 2.2f, 4.5f);
	framework.m_config.m_lookAtTarget = (Vector3)Point3(0,3,0);
	framework.m_config.m_lookAtUp = (Vector3)Vector3(0,1,0);

	framework.processCommandLine(argc, argv);

	framework.m_config.m_clearColorIsMeaningful = 0;
	framework.m_config.m_lightingIsMeaningful = 0;
	framework.m_config.m_depthBufferIsMeaningful = 0;
	framework.m_config.m_garlicMemoryInBytes = 1024U * 1024U * 2048U;
	framework.m_config.m_buffers = kNumFramesInFlight;
	framework.m_config.m_cameraControlMode = 1;

	bool is4KTV = false;
	int handle = sceVideoOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
	if (handle > 0){
		SceVideoOutResolutionStatus status;
		if (SCE_OK == sceVideoOutGetResolutionStatus(handle, &status) && status.fullHeight > 1080){
			is4KTV = true;
		}
		sceVideoOutClose(handle);
	}

	const char k4kOverview[] = "Sample code to demonstrate 4k rendering on NEO.";
	const char k2kOverview[] = "Sample code to demonstrate 4k rendering on NEO. Note: Not currently running in 4K mode - results will be downscaled.";
	framework.initialize( "NEO 4k", 
		is4KTV ? k4kOverview : k2kOverview,
		"This sample demonstrates NEO features for 4k rendering. It renders a scene to a 2xEQAA checkerboard color buffer and full resolution ID buffer, and resolves to 4k resolution in a separate compute pass with an optional temporal component. Gradient adjust is configured to correct the texture gradients in checkerboard space, and PS Invoke is configured to avoid unnecessary shading of samples. For more details on the techniques used in this sample please refer to the 'NEO High Resolution Technologies' document.");

	const Framework::MenuItem menuItem[] = {
		{{"Toggle Temporal ('T')", "Toggle temporal reconstruction on/off"}, &menuCommandBoolTemporal},
		{{"Toggle Gradient Adjust ('G')", "Toggle gradient adjust on/off"}, &menuCommandBoolGradientAdjust},
		{{"Toggle PS Invoke ('P')", "Toggle PS invoke on/off"}, &menuCommandBoolPsInvoke},
		{{"Toggle Alpha Unroll ('A')", "Toggle alpha unroll on/off"}, &menuCommandBoolAlphaUnroll},
		{{"Rendering Mode", "Toggle rendering mode"}, Gnm::getGpuMode() == Gnm::kGpuModeNeo ? &menuCommandEnumRenderingModeNeo : &menuCommandEnumRenderingModeBase},
		{{"Anti-aliasing Mode", "Select anti-aliasing mode"}, &menuCommandEnumAaMode},
		{{"Mask Export", "Enable coverage mask export in alpha prepass"}, &menuCommandBoolMask},
	};

	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };

	framework.appendMenuItems(kMenuItems, menuItem);

	// set default rendering mode
	if(Gnm::getGpuMode() == Gnm::kGpuModeBase)
		g_renderingMode = kRenderingMode1080p;
	else
		g_renderingMode = kRenderingMode4KCB;


	// init keyboard
	int ret = 0;
	SceUserServiceUserId initialUserId;
	ret = sceUserServiceGetInitialUser(&initialUserId);
	ret = sceSysmoduleLoadModule( SCE_SYSMODULE_DEBUG_KEYBOARD );
	ret = sceDbgKeyboardInit();
	int keyboardHandle = sceDbgKeyboardOpen(initialUserId, SCE_DBG_KEYBOARD_PORT_TYPE_STANDARD, 0, NULL);
	
	static uint8_t keyCodes[256];
	memset(keyCodes, 0, sizeof(keyCodes));

	enum {kCommandBufferSizeInBytes = 2 * 1024 * 1024};

	Gnmx::GnmxGfxContext *commandBuffers = static_cast<Gnmx::GnmxGfxContext*>(framework.m_onionAllocator.allocate(sizeof(Gnmx::GnmxGfxContext) * framework.m_config.m_buffers, 16)); // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	for(uint32_t bufferIndex = 0; bufferIndex < framework.m_config.m_buffers; ++bufferIndex)
	{
		createCommandBuffer(&commandBuffers[bufferIndex], &framework, bufferIndex);
	}

	loadAssets(&framework.m_allocators); 


	// load shaders
	loadVsShader(&g_fullScreenVertexShader, "/app0/neo-4k-sample/fullscreen_vv.sb", &framework.m_allocators);
	loadPsShader(&g_passThroughPixelShader, "/app0/neo-4k-sample/passthrough_p.sb", &framework.m_allocators);
	loadPsShader(&g_brightPassPixelShader, "/app0/neo-4k-sample/bright_pass_p.sb", &framework.m_allocators);
	loadPsShader(&g_gaussianHPixelShader, "/app0/neo-4k-sample/gaussian_blur_h_p.sb", &framework.m_allocators);
	loadPsShader(&g_gaussianVPixelShader, "/app0/neo-4k-sample/gaussian_blur_v_p.sb", &framework.m_allocators);
	loadPsShader(&g_bloomCompositePixelShader, "/app0/neo-4k-sample/bloom_composite_p.sb", &framework.m_allocators);
	loadPsShader(&g_radialBlurPixelShader, "/app0/neo-4k-sample/radial_blur_p.sb", &framework.m_allocators);
	loadCsShader(&g_checkerboardResolveShader, "/app0/neo-4k-sample/checkerboard_resolve_c.sb", &framework.m_allocators);
	loadCsShader(&g_copyExpandedHTileShader, "/app0/neo-4k-sample/copy_expanded_htile_c.sb", &framework.m_allocators);

	loadVsShader(&g_smaaEdgeDetectionVertexShader, "/app0/neo-4k-sample/smaa_edge_detection_vv.sb", &framework.m_allocators);
	loadPsShader(&g_smaaEdgeDetectionPixelShader, "/app0/neo-4k-sample/smaa_edge_detection_p.sb", &framework.m_allocators);
	loadVsShader(&g_smaaBlendWeightVertexShader, "/app0/neo-4k-sample/smaa_blend_weight_vv.sb", &framework.m_allocators);
	loadPsShader(&g_smaaBlendWeightPixelShader, "/app0/neo-4k-sample/smaa_blend_weight_p.sb", &framework.m_allocators);
	loadVsShader(&g_smaaBlendVertexShader, "/app0/neo-4k-sample/smaa_blend_vv.sb", &framework.m_allocators);
	loadPsShader(&g_smaaBlendPixelShader, "/app0/neo-4k-sample/smaa_blend_p.sb", &framework.m_allocators);

	// init samplers
	g_samplerPoint.init();
	g_samplerPoint.setMipFilterMode(Gnm::kMipFilterModeNone);
	g_samplerPoint.setXyFilterMode(Gnm::kFilterModePoint, Gnm::kFilterModePoint);

	g_samplerLinear.init();
	g_samplerLinear.setMipFilterMode(Gnm::kMipFilterModeLinear);
	g_samplerLinear.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
	g_samplerLinear.setWrapMode(Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel);
	g_samplerLinear.setBorderColor(Gnm::kBorderColorTransBlack);

	g_samplerAniso.init();
	g_samplerAniso.setMipFilterMode(Gnm::kMipFilterModeLinear);
	g_samplerAniso.setXyFilterMode(Gnm::kFilterModeAnisoBilinear, Gnm::kFilterModeAnisoBilinear);
	g_samplerAniso.setAnisotropyRatio(Gnm::kAnisotropyRatio8);

	g_samplerShadow.init();
	g_samplerShadow.setDepthCompareFunction(Gnm::kDepthCompareLessEqual);
	g_samplerShadow.setWrapMode(Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel);
	g_samplerShadow.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
	g_samplerShadow.setZFilterMode(Gnm::kZFilterModeLinear);

	const uint32_t kTargetWidth = 3840;
	const uint32_t kTargetHeight = 2160;
	const uint32_t numSlices = 1;

	// Initialize shadow depth render target. 
	// For NEO we enable texture compatible depth compression, to avoid having to decompress before sampling the shadow map texture in the main pass.
	bool texHtile = Gnm::getGpuMode() == Gnm::kGpuModeNeo;
	initDepthRenderTarget(kShadowWidth, kShadowHeight, Gnm::kZFormat32Float, Gnm::kStencilInvalid, Gnm::kNumFragments1, true, texHtile, &framework, &g_shadowDepthTarget, &g_shadowMapTexture, NULL);	

	for(size_t i = 0; i < g_numConfigs; ++i)
	{
		auto& c = g_targetConfigs[i];

		// load resolve shader
		loadCsShader(&c.resolve_shader, c.shader_name, &framework.m_allocators);

		// init half res, 2 fragment depth render target (for checkerboard rendering)
		initDepthRenderTarget(c.width, c.height, Gnm::kZFormat32Float, Gnm::kStencilInvalid, c.depthFragmentCount, true, false, &framework, &g_depthTarget[i], &g_depthTexture[i], NULL);

		// init half res, single fragment depth render target
		initDepthRenderTarget(c.width, c.height, Gnm::kZFormat32Float, Gnm::kStencilInvalid, sce::Gnm::kNumFragments1, true, false, &framework, &g_resolvedDepthTarget[i], &g_resolvedDepthTexture[i], NULL);

		// init checkerboard color render targets
		initRenderTarget(c.width, c.height, Gnm::kDataFormatR8G8B8A8UnormSrgb, c.colorSampleCount, c.colorFragmentCount, &framework, &g_colorCheckerboardRenderTarget[i][0], &g_colorCheckerboardTexture[i][0]);
		initRenderTarget(c.width, c.height, Gnm::kDataFormatR8G8B8A8UnormSrgb, c.colorSampleCount, c.colorFragmentCount, &framework, &g_colorCheckerboardRenderTarget[i][1], &g_colorCheckerboardTexture[i][1]);

		
		// init checkerboard velocity render target
		initRenderTarget(c.width, c.height, Gnm::kDataFormatR16G16Float, c.colorSampleCount, c.colorFragmentCount, &framework, &g_velocityCheckerboardRenderTarget[i], &g_velocityCheckerboardTexture[i]);

		// init object/prim ID render target
		initRenderTarget(c.width, c.height, Gnm::kDataFormatR32Uint, c.idSampleCount, c.depthFragmentCount, &framework, &g_primIdRenderTarget[i], &g_primIdTexture[i]);
	}


	// init color history render targets
	initRenderTarget(kTargetWidth, kTargetHeight, Gnm::kDataFormatR8G8B8A8UnormSrgb, Gnm::kNumSamples1, Gnm::kNumFragments1, &framework, &g_colorHistoryRenderTarget[0], &g_colorHistoryTexture[0]);
	initRenderTarget(kTargetWidth, kTargetHeight, Gnm::kDataFormatR8G8B8A8UnormSrgb, Gnm::kNumSamples1, Gnm::kNumFragments1, &framework, &g_colorHistoryRenderTarget[1], &g_colorHistoryTexture[1]);

	// init blur render targets
	uint32_t w = kTargetWidth/4;
	uint32_t h = kTargetHeight/4;
	for(uint32_t i=0; i<kNumBlurTargets; ++i)
	{
		initRenderTarget(w, h, Gnm::kDataFormatR8G8B8A8UnormSrgb, Gnm::kNumSamples1, Gnm::kNumFragments1, &framework, &g_blurRenderTarget[i][0], &g_blurTexture[i][0]);
		initRenderTarget(w, h, Gnm::kDataFormatR8G8B8A8UnormSrgb, Gnm::kNumSamples1, Gnm::kNumFragments1, &framework, &g_blurRenderTarget[i][1], &g_blurTexture[i][1]);
		w /= 2;
		h /= 2;
	}

	// init SMAA render targets
	initRenderTarget(kTargetWidth, kTargetHeight, Gnm::kDataFormatR8G8Unorm, Gnm::kNumSamples1, Gnm::kNumFragments1, &framework, &g_smaaEdgesRenderTarget, &g_smaaEdgesTexture);
	initRenderTarget(kTargetWidth, kTargetHeight, Gnm::kDataFormatR8G8B8A8Unorm, Gnm::kNumSamples1, Gnm::kNumFragments1, &framework, &g_smaaBlendRenderTarget, &g_smaaBlendTexture);
	initRenderTarget(kTargetWidth, kTargetHeight, Gnm::kDataFormatR8G8B8A8Unorm, Gnm::kNumSamples1, Gnm::kNumFragments1, &framework, &g_smaaBlendRenderTarget, &g_smaaBlendTexture);

	// stencil only target for SMAA edges
	initDepthRenderTarget(kTargetWidth, kTargetHeight, Gnm::kZFormatInvalid, Gnm::kStencil8, Gnm::kNumFragments1, false, false, &framework, &g_edgeStencilTarget, NULL, &g_edgeStencilTexture);

	// load SMAA textures
	Texture *areaTex = loadTexture("AreaTex", &framework.m_allocators);
	Texture *searchTex = loadTexture("SearchTex", &framework.m_allocators);
	g_smaaAreaTexture = areaTex->m_texture;
	g_smaaSearchTexture = searchTex->m_texture;

	// init cubic filtering weights for the 4K Geometry resolve
	{
		Gnm::TextureSpec spec;
		spec.init();
		spec.m_textureType = Gnm::kTextureType2d;
		spec.m_width = 6;
		spec.m_height = 6;
		spec.m_depth = 1;
		spec.m_pitch = 0;
		spec.m_numMipLevels = 1;
		spec.m_numSlices = 1;
		spec.m_format = sce::Gnm::kDataFormatR16Float;
		spec.m_tileModeHint = sce::Gnm::kTileModeDisplay_LinearAligned;
		spec.m_minGpuMode = Gnm::getGpuMode();
		spec.m_numFragments = Gnm::kNumFragments1;
		g_cubicWeightTexture.init(&spec);

		auto sizeAlign = g_cubicWeightTexture.getSizeAlign();
		void* memory = framework.m_garlicAllocator.allocate(sizeAlign);
		g_cubicWeightTexture.setBaseAddress(memory);
		g_cubicWeightTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

		updateCubicWeights(g_cubicWeightTexture);

		g_cubicWeightSampler.init();
		g_cubicWeightSampler.setWrapMode(Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel);
		g_cubicWeightSampler.setXyFilterMode(Gnm::kFilterModePoint, Gnm::kFilterModePoint);
	}

	// init scratch buffers (used for dynamically generated GPU data)
	for(uint32_t i=0; i< kNumFramesInFlight; ++i)
		g_scratchBuffers[i].init(2*1024*1024, &framework.m_allocators);

	const float depthNear = 0.1f;
	const float depthFar = 250.0f;
	const float aspect = (float)framework.m_config.m_targetWidth / (float)framework.m_config.m_targetHeight;
	framework.m_projectionMatrix = Matrix4::perspective(45.0f * M_PI/180.0f, aspect, depthNear, depthFar );

	while( !framework.m_shutdownRequested )
	{
		Gnmx::GnmxGfxContext *gfxc = &commandBuffers[framework.m_backBufferIndex]; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.

		gfxc->reset();
		gfxc->initializeDefaultHardwareState();

		framework.BeginFrame(*gfxc);

		g_scratchBuffers[framework.m_backBufferIndex].reset();

		// rotate light
		float lightAngle = framework.GetSecondsElapsedApparent() * 0.1f;
		lightPos = Vector3(-5*cosf(lightAngle), 12, 5*sinf(lightAngle));
		framework.m_worldToLightMatrix = Matrix4::lookAt((Point3)lightPos, (Point3)lightTgt, lightUp);
		framework.m_lightToWorldMatrix = inverse(framework.m_worldToLightMatrix);

		// Sort triangles back to front for translucent meshes.
		// Note: this is purely to eliminate translucency artifacts due to rendering order, it is not required for any of the 4k rendering techniques used in this sample. 
		Vector3 eyePos = framework.getCameraPositionInWorldSpace().getXYZ();
		for(uint32_t i=0; i<kNumMeshes; ++i)
		{
			if(g_materials[g_meshes[i].m_materialIndex].isTranslucent())
			{
				Mesh &mesh = g_meshes[i];
				mesh.m_dynamicIndexBuffer = g_scratchBuffers[framework.m_backBufferIndex].allocate(mesh.m_numIndices * sizeof(uint32_t), Gnm::kAlignmentOfBufferInBytes);
				SCE_GNM_ASSERT(mesh.m_dynamicIndexBuffer);
				sortTrisBackToFront((uint32_t*)mesh.m_dynamicIndexBuffer, (uint32_t*)mesh.m_indexBuffer, (uint8_t*)mesh.m_vertexBuffer, mesh.m_numIndices, mesh.m_stride, eyePos);
			}
		}

		Matrix4 matWorldView = framework.m_worldToViewMatrix;
		Matrix4 matProj = framework.m_projectionMatrix;
		Vector3 lighPos = framework.getLightPositionInViewSpace().getXYZ();
		Vector4 lightColor = framework.getLightColor();
		Vector4 ambientColor = framework.getAmbientColor();

		///////////////////////////////////////////////////////////////////
		// shadow pass
		///////////////////////////////////////////////////////////////////

		Gnm::Htile htile = {};
		htile.m_hiZ.m_zMask = 0; // tile is clear
		htile.m_hiZ.m_maxZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is > 1.f
		htile.m_hiZ.m_minZ = Gnm::Htile::kMaximumZValue; // no depth sample in tile is < 1.f
		Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(*gfxc, &g_shadowDepthTarget, htile);
		gfxc->setDepthClearValue(1.f);
		gfxc->setupScreenViewport(0, 0, g_shadowDepthTarget.getWidth(), g_shadowDepthTarget.getHeight(), 0.5f, 0.5f);
		gfxc->setDepthRenderTarget(&g_shadowDepthTarget);
		gfxc->setRenderTargetMask(0);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);

		Matrix4 matLightView = framework.m_worldToLightMatrix;
		Matrix4 matLightProj = Matrix4::orthographic(-30, 30, -30, 30, 1.0, 200);

		shadowPass(gfxc, &matLightView, &matLightProj);

		if(!g_shadowMapTexture.getMetadataCompressionEnable())
		{
			// Decompress the shadow map depth buffer in preparation for it to be used as a texture
			Gnmx::decompressDepthSurface(gfxc, &g_shadowDepthTarget );

			// decompressDepthSurface leaves the Htile mode as "uncompressed", which is not intended here.
			sce::Gnm::DbRenderControl drc;
			drc.init();
			gfxc->setDbRenderControl(drc);
		}
		else
		{
			// shadow map has texture compatible compression, so a decompress is not required. We must synchronise however.
			gfxc->waitForGraphicsWrites(
				g_shadowDepthTarget.getZWriteAddress256ByteBlocks(), 
				g_shadowDepthTarget.getZSliceSizeInBytes()/256, 
				Gnm::kWaitTargetSlotDb, 
				Gnm::kCacheActionNone, 
				Gnm::kExtendedCacheActionFlushAndInvalidateDbCache, 
				Gnm::kStallCommandBufferParserDisable);

			gfxc->triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta);

			gfxc->waitForGraphicsWrites(
				g_shadowDepthTarget.getHtileAddress256ByteBlocks(), 
				g_shadowDepthTarget.getHtileSliceSizeInBytes()/256, 
				Gnm::kWaitTargetSlotDb, 
				Gnm::kCacheActionNone, 
				Gnm::kExtendedCacheActionFlushAndInvalidateDbCache, 
				Gnm::kStallCommandBufferParserDisable);
		}

		///////////////////////////////////////////////////////////////////
		// setup checkerboard rendering
		///////////////////////////////////////////////////////////////////

		// checkerboard index, alternated per frame when temporal is enabled
		g_checkerboardId = g_temporalEnabled ? framework.m_frameCount & 1 : 0;
			
		// Enable 2xEQAA checkerboard.
		// Each pixel has two samples at locations (-4, 0) and (4, 0)
		// The sample ordering defines the checkerboard layout (alternated per frame)
		//
		//  ---------------------
		// |  s0  s1  |  s0  s1  |		kQuadPixelUpperLeft, kQuadPixelUpperRight
		// |----------+----------|
		// |  s1  s0  |  s1  s0  |		kQuadPixelLowerLeft, kQuadPixelLowerRight
		//  ---------------------

		Gnm::AaSampleLocationControl locationControl;
		locationControl.init();

		sce::Gnm::DepthEqaaControl dec;
		dec.init();

		if(g_renderingMode == kRenderingMode4KCB)
		{
			int sampleOffsetX = g_checkerboardId == 0 ? -4 :  4;

			locationControl.setSampleLocationForPixel(Gnm::kQuadPixelUpperLeft,	 0,  sampleOffsetX, 0);
			locationControl.setSampleLocationForPixel(Gnm::kQuadPixelUpperLeft,  1, -sampleOffsetX, 0);
			locationControl.setSampleLocationForPixel(Gnm::kQuadPixelUpperRight, 0,  sampleOffsetX, 0);
			locationControl.setSampleLocationForPixel(Gnm::kQuadPixelUpperRight, 1, -sampleOffsetX, 0);

			locationControl.setSampleLocationForPixel(Gnm::kQuadPixelLowerLeft,  0, -sampleOffsetX, 0);
			locationControl.setSampleLocationForPixel(Gnm::kQuadPixelLowerLeft,  1,  sampleOffsetX, 0);
			locationControl.setSampleLocationForPixel(Gnm::kQuadPixelLowerRight, 0, -sampleOffsetX, 0);
			locationControl.setSampleLocationForPixel(Gnm::kQuadPixelLowerRight, 1,  sampleOffsetX, 0);
			gfxc->m_dcb.setAaSampleCount(Gnm::kNumSamples2, 4);  // maxSampleDistance is the greatest absolute distance from the pixel center on any axis.
			dec.setPsSampleIterationCount(sce::Gnm::kNumSamples2);

			if (g_maskExport)
				dec.setMaskExportNumSamples(Gnm::kNumSamples2);

			// enable PS invoke
			if(g_psInvokeEnabled)
				gfxc->setPsShaderSampleExclusionMask(0xfffe);

			// enable object/prim IDs
			gfxc->m_dcb.setObjectIdMode(Gnm::kObjectIdSourceRegister, Gnm::kAddPrimitiveIdEnable);
			Gnm::DrawPayloadControl dpc;
			dpc.init();
			dpc.setObjectPrimIdPropagation(Gnm::kObjectPrimIdEnable);
			gfxc->m_dcb.setDrawPayloadControl(dpc);
		}
		else if(g_renderingMode == kRenderingMode4KG)
		{
			// Look for slide 3-78 of the GPU Walkthrough for how to interpret the location values.
			locationControl.setSampleLocationForQuad(0, -4, -4);
			locationControl.setSampleLocationForQuad(1,  4, -4);
			locationControl.setSampleLocationForQuad(2, -4,  4);
			locationControl.setSampleLocationForQuad(3,  4,  4);
			gfxc->m_dcb.setAaSampleCount(Gnm::kNumSamples4, 4); // maxSampleDistance is the greatest absolute distance from the pixel center on any axis.
			dec.setPsSampleIterationCount(sce::Gnm::kNumSamples4);

			// enable PS invoke
			if(g_psInvokeEnabled)
				gfxc->setPsShaderSampleExclusionMask(0xfffe);

			// enable object/prim IDs
			gfxc->m_dcb.setObjectIdMode(Gnm::kObjectIdSourceRegister, Gnm::kAddPrimitiveIdEnable);
			Gnm::DrawPayloadControl dpc;
			dpc.init();
			dpc.setObjectPrimIdPropagation(Gnm::kObjectPrimIdEnable);
			gfxc->m_dcb.setDrawPayloadControl(dpc);
		}

		gfxc->m_dcb.setAaSampleLocationControl(&locationControl);		
		gfxc->m_dcb.setScanModeControl(Gnm::kScanModeControlAaEnable, Gnm::kScanModeControlViewportScissorDisable);
		gfxc->setDepthEqaaControl(dec);
		
		Gnm::RenderTarget *renderTarget = &g_colorCheckerboardRenderTarget[g_renderingMode][0];

		// clear targets
		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, renderTarget, framework.getClearColor());
		Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(*gfxc, &g_depthTarget[g_renderingMode], htile);
		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &g_velocityCheckerboardRenderTarget[g_renderingMode], Vector4(0,0,0,0));

		// set render targets
		// ID buffer must be bound to MRT 7
		gfxc->setRenderTarget(0, renderTarget);
		gfxc->setRenderTarget(1, &g_velocityCheckerboardRenderTarget[g_renderingMode]);
		gfxc->setRenderTarget(7, &g_primIdRenderTarget[g_renderingMode]);
		gfxc->setDepthRenderTarget(&g_depthTarget[g_renderingMode]);

		gfxc->setupScreenViewport(0, 0, renderTarget->getWidth(), renderTarget->getHeight(), 0.5f, 0.5f);

		Gnm::DbRenderControl dbRenderControl;
		dbRenderControl.init();
		gfxc->setDbRenderControl(dbRenderControl);

		///////////////////////////////////////////////////////////////////////////////////////
		// Pre-pass for alpha tested materials.
		// Writes depth and IDs only. The shader is unrolled to sample rate in order to get
		// correct depth/IDs for all samples.
		///////////////////////////////////////////////////////////////////////////////////////

		if(g_alphaUnrollEnabled)
		{
			gfxc->pushMarker("Alpha test prepass");

			// IDs only
			gfxc->setRenderTargetMask(0xF0000000);

			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
			dsc.setDepthEnable(true);
			gfxc->setDepthStencilControl(dsc);

			if(g_renderingMode == kRenderingMode4KCB)
			{
				// Set gradient adjust for checkerboard rendering.
				// Note: for checkerboard rendering at sample rate, gradient adjust sign negation must be enabled.
				setGradientAdjust(gfxc, 0.5f, 0.0f, g_checkerboardId == 0 ? -0.5f : 0.5f, 1.0f, true);
			}
			else if(g_renderingMode == kRenderingMode4KG)
			{
				// For the alphaTestPrepass, we half the gradients to match native 4K. 
				// Doing this with a negative LOD bias doesn't have quite the same result.
				setGradientAdjust(gfxc, 0.5f, 0.0f, 0.0f, 0.5f, false);
			}

			alphaTestPrepass(gfxc, matWorldView, matProj);

			gfxc->popMarker();
		}

		if(g_renderingMode == kRenderingMode4KCB)
		{
			// For checkerboard rendering at pixel rate, gradient adjust sign negation must be disabled.
			setGradientAdjust(gfxc, 0.5f, 0.0f, g_checkerboardId == 0 ? -0.5f : 0.5f, 1.0f, false);
		}
		else if(g_renderingMode == kRenderingMode4KG)
		{
			// Since we're done with alpha tested geometry, we now revert back to unadjusted gradients.
			setGradientAdjust(gfxc, 1.0f, 0.0f, 0.0f, 1.0f, false);
		}

		///////////////////////////////////////////////////////////////////////////////////////
		// Main opaque pass (no alpha test)
		// Writes checkerboard color+velocity, and full res depth+IDs
		///////////////////////////////////////////////////////////////////////////////////////

		// colour + velocity + IDs
		gfxc->setRenderTargetMask(0xF00000FF);

		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);

		Matrix4 matClipSpaceToTextureSpace = Matrix4::translation(Vector3(0.5f, 0.5f, 0.5f)) * Matrix4::scale(Vector3(0.5f, -0.5f, 0.5f));
		Matrix4 matShadow = matClipSpaceToTextureSpace * matLightProj * matLightView;

		uint32_t materialFilter = g_alphaUnrollEnabled ? kMaterialTypeOpaque : kMaterialTypeOpaque | kMaterialTypeAlphaTest;

		gfxc->pushMarker("Opaque");
		mainPass(gfxc, &g_shadowMapTexture, matWorldView, matProj, matShadow, lighPos, lightColor, ambientColor, materialFilter);
		gfxc->popMarker();

		if(g_renderingMode == kRenderingMode4KCB || g_renderingMode == kRenderingMode4KG)
		{
			// remaining passes do not write IDs, so disable them here
			gfxc->m_dcb.setObjectIdMode(Gnm::kObjectIdSourceRegister, Gnm::kAddPrimitiveIdDisable);
			Gnm::DrawPayloadControl dpc;
			dpc.init();
			dpc.setObjectPrimIdPropagation(Gnm::kObjectPrimIdDisable);
			gfxc->m_dcb.setDrawPayloadControl(dpc);
		}
		
		///////////////////////////////////////////////////////////////////////////////////////
		// Resolve depth to single sample. 
		// This code shows how to quickly resolve a multisample depth target to a single sample
		// depth target, and reuse the z range information from the original HTILE buffer. 
		// This is purely an optimisation for passes that don't require multisample depth or IDs.
		///////////////////////////////////////////////////////////////////////////////////////

		if(g_depthTarget[g_renderingMode].getNumFragments() != Gnm::kNumFragments1)
		{
			gfxc->pushMarker("Depth resolve");

			gfxc->m_dcb.triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta); 

			// Decompress and copy depth sample 0 to the resolved depth target.
			Gnmx::copyOneFragment(&gfxc->m_dcb, g_depthTarget[g_renderingMode], g_resolvedDepthTarget[g_renderingMode], Gnmx::kBuffersToCopyDepth, 0);

			// Copy HTILE buffer, preserving z ranges but setting zmask to 'expanded'
			gfxc->waitForGraphicsWrites(
				g_depthTarget[g_renderingMode].getHtileAddress256ByteBlocks(), 
				g_depthTarget[g_renderingMode].getHtileSizeAlign().m_size/256, 
				Gnm::kWaitTargetSlotDb, 
				Gnm::kCacheActionNone, 
				Gnm::kExtendedCacheActionFlushAndInvalidateDbCache, 
				Gnm::kStallCommandBufferParserDisable);

			copyExpandedHTile(*gfxc, &g_depthTarget[g_renderingMode], &g_resolvedDepthTarget[g_renderingMode]);

			// restore state
			gfxc->m_dcb.setScanModeControl(Gnm::kScanModeControlAaEnable, Gnm::kScanModeControlViewportScissorDisable);

			gfxc->setRenderTarget(0, renderTarget);
			gfxc->setRenderTarget(1, &g_velocityCheckerboardRenderTarget[g_renderingMode]);

			// use resolved depth for the remaining passes
			gfxc->setDepthRenderTarget(&g_resolvedDepthTarget[g_renderingMode]);

			gfxc->popMarker();
		}
		
		///////////////////////////////////////////////////////////////////////////////////////
		// Opaque pass with alpha test.
		// Writes checkerboard color+velocity only, with depth and IDs coming from the pre-pass.
		///////////////////////////////////////////////////////////////////////////////////////

		if(g_alphaUnrollEnabled)
		{
			gfxc->pushMarker("Alpha test");

			// colour + velocity
			gfxc->setRenderTargetMask(0x000000FF);

			dsc.init();
			dsc.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncEqual);
			dsc.setDepthEnable(true);
			gfxc->setDepthStencilControl(dsc);

			mainPass(gfxc, &g_shadowMapTexture, matWorldView, matProj, matShadow, lighPos, lightColor, ambientColor, kMaterialTypeAlphaTest);

			gfxc->popMarker();
		}

		///////////////////////////////////////////////////////////////////////////////////////
		// Translucent pass.
		// Writes color only.
		///////////////////////////////////////////////////////////////////////////////////////

		Gnm::BlendControl bc;
		bc.init();
		bc.setBlendEnable(true);
		bc.setColorEquation(Gnm::kBlendMultiplierSrcAlpha, Gnm::kBlendFuncAdd, Gnm::kBlendMultiplierOneMinusSrcAlpha);
		bc.setSeparateAlphaEnable(false);
		gfxc->setBlendControl(0, bc);

		// colour only
		gfxc->setRenderTargetMask(0x0000000F);

		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncLessEqual);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);

		gfxc->pushMarker("Translucent");
		mainPass(gfxc, &g_shadowMapTexture, matWorldView, matProj, matShadow, lighPos, lightColor, ambientColor, kMaterialTypeTranslucent);
		gfxc->popMarker();

		bc.setBlendEnable(false);
		gfxc->setBlendControl(0, bc);

		gfxc->waitForGraphicsWrites(renderTarget->getBaseAddress256ByteBlocks(), renderTarget->getSliceSizeInBytes()*numSlices/256, Gnm::kWaitTargetSlotCb0, Gnm::kCacheActionNone, Gnm::kExtendedCacheActionFlushAndInvalidateCbCache, Gnm::kStallCommandBufferParserDisable);

		///////////////////////////////////////////////////////////////////////////////////////
		// post processing (checkerboard)
		///////////////////////////////////////////////////////////////////////////////////////

		applyBloom(gfxc, matWorldView, matProj);

		///////////////////////////////////////////////////////////////////////////////////////
		// disable checkerboard rendering
		///////////////////////////////////////////////////////////////////////////////////////

		if(g_renderingMode == kRenderingMode4KCB || g_renderingMode == kRenderingMode4KG)
		{
			// disable EQAA
			gfxc->m_dcb.setScanModeControl(Gnm::kScanModeControlAaDisable, Gnm::kScanModeControlViewportScissorEnable);

			// disable gradient adjust
			setGradientAdjust(gfxc, 1.0f, 0.0f, 0.0f, 1.0f, false);

			// disable PS invoke
			gfxc->setPsShaderSampleExclusionMask(0);
		}

		///////////////////////////////////////////////////////////////////////////////////////
		// resolve to 4K
		///////////////////////////////////////////////////////////////////////////////////////

		if (g_renderingMode == kRenderingMode4KG)
		{
			// Decompress the depth buffer in preparation for it to be used as a texture
			Gnmx::decompressDepthSurface(gfxc, &g_depthTarget[g_renderingMode]);
		}

		struct ResolveConstants
		{
			uint32_t checkerboardId;
			uint32_t debugView;
			uint32_t temporal;
			float historyWeight;
		};

		ResolveConstants* resolveConstants = (ResolveConstants*)(gfxc->allocateFromCommandBuffer(sizeof(ResolveConstants), Gnm::kEmbeddedDataAlignment4));
		resolveConstants->checkerboardId = g_checkerboardId;
		resolveConstants->debugView = g_debugView;
		resolveConstants->temporal = g_temporalEnabled;
		resolveConstants->historyWeight = 0.6f;
		Gnm::Buffer resolveCb;
		resolveCb.initAsConstantBuffer(resolveConstants, sizeof(ResolveConstants));
		resolveCb.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);

		gfxc->pushMarker("4K resolve");

		auto writeable_dst_texture = g_colorHistoryTexture[g_checkerboardId];
		writeable_dst_texture.setResourceMemoryType(Gnm::kResourceMemoryTypePV, Gnm::kL1CachePolicyLru); 

		// cannot write to srgb textures from compute, so pretend it's unorm and do the srgb conversion in the shader
		writeable_dst_texture.setChannelType(Gnm::kTextureChannelTypeUNorm);

		auto writeable_dst_edge_texture = g_edgeStencilTexture;
		writeable_dst_edge_texture.setResourceMemoryType(Gnm::kResourceMemoryTypePV, Gnm::kL1CachePolicyLru);

		// Checkerboard resolve reads single sample depth (if SCE_CHECKERBOARD_RESOLVE_USE_FRONTMOST_VELOCITY is enabled)
		// Geometry resolve reads multisample depth.
		auto depth_texture = g_renderingMode == kRenderingMode4KCB ? &g_resolvedDepthTexture[g_renderingMode] : &g_depthTexture[g_renderingMode];

		auto resolve_shader = &g_targetConfigs[g_renderingMode].resolve_shader;

		gfxc->setCsShader(resolve_shader->m_shader, &resolve_shader->m_offsetsTable);

		gfxc->setTextures       ( Gnm::kShaderStageCs, 0, 1, &g_colorCheckerboardTexture[g_renderingMode][1] );
		gfxc->setTextures       ( Gnm::kShaderStageCs, 2, 1, &g_primIdTexture[g_renderingMode] );
		gfxc->setTextures       ( Gnm::kShaderStageCs, 4, 1, &g_velocityCheckerboardTexture[g_renderingMode] );
		gfxc->setTextures       ( Gnm::kShaderStageCs, 5, 1, &g_colorHistoryTexture[g_checkerboardId^1] );
		gfxc->setTextures       ( Gnm::kShaderStageCs, 6, 1, depth_texture );
		gfxc->setTextures       ( Gnm::kShaderStageCs, 7, 1, &g_cubicWeightTexture );

		gfxc->setRwTextures     ( Gnm::kShaderStageCs, 0, 1, &writeable_dst_texture);
		gfxc->setRwTextures     ( Gnm::kShaderStageCs, 1, 1, &writeable_dst_edge_texture);
		gfxc->setSamplers       ( Gnm::kShaderStageCs, 0, 1, &g_samplerPoint );
		gfxc->setSamplers       ( Gnm::kShaderStageCs, 1, 1, &g_samplerLinear );
		gfxc->setSamplers       ( Gnm::kShaderStageCs, 2, 1, &g_cubicWeightSampler );
		gfxc->setConstantBuffers( Gnm::kShaderStageCs, 0, 1, &resolveCb);

		auto dispatch_width = kTargetWidth / resolve_shader->m_shader->m_csStageRegisters.m_computeNumThreadX;
		auto dispatch_height = kTargetHeight / resolve_shader->m_shader->m_csStageRegisters.m_computeNumThreadY;

		if(g_renderingMode == kRenderingMode4KCB) 
		{
			dispatch_width >>= 1; // 4KCB does two pixels per thread.
		}

		gfxc->dispatch(dispatch_width, dispatch_height ,1);

		Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc->m_dcb);

		gfxc->popMarker();

		/////////////////////////////////////////////////////////////////////////////////////////////////////////
		// post processing (full res)
		/////////////////////////////////////////////////////////////////////////////////////////////////////////

		if(g_aaMode == kAaModeFxaa)
		{
			applyFxaa(gfxc, &g_colorHistoryTexture[g_checkerboardId], &framework.m_backBuffer->m_renderTarget);
		}
		else if (g_aaMode == kAaModeSmaa)
		{
			applySmaa(gfxc, &g_colorHistoryTexture[g_checkerboardId], &framework.m_backBuffer->m_renderTarget);
		}
		else
		{
			copyTexture(gfxc, &g_colorHistoryTexture[g_checkerboardId], &framework.m_backBuffer->m_renderTarget);
		}

		// get keyboard state
		SceDbgKeyboardData keyData;
		sceDbgKeyboardReadState(keyboardHandle, &keyData);

		if(keyData.length == 1)
		{
			if(keyData.keyCode[0] == 0)
				memset(keyCodes, 0, sizeof(keyCodes));
			else
				keyCodes[keyData.keyCode[0]]++;
		}

		if(keyCodes[SCE_DBG_KEYBOARD_CODE_T] == 1)
			g_temporalEnabled = !g_temporalEnabled;

		if(keyCodes[SCE_DBG_KEYBOARD_CODE_G] == 1)
			g_gradientAdjustEnabled = !g_gradientAdjustEnabled;

		if(keyCodes[SCE_DBG_KEYBOARD_CODE_P] == 1)
			g_psInvokeEnabled = !g_psInvokeEnabled;

		if(keyCodes[SCE_DBG_KEYBOARD_CODE_A] == 1)
			g_alphaUnrollEnabled = !g_alphaUnrollEnabled;

#if 0
		// debug only
		for(uint32_t i=0; i<12; ++i)
		{
			if(keyCodes[SCE_DBG_KEYBOARD_CODE_F1+i] == 1)
				g_debugView = g_debugView == i+1 ? 0 : i+1;
		}

		if(keyCodes[SCE_DBG_KEYBOARD_CODE_ESC] == 1)
			g_debugView = 0;

		framework.m_backBuffer->m_window.setCursor(0, 24);
		framework.m_backBuffer->m_window.printf("debugView = %d\n", g_debugView);
		framework.m_backBuffer->m_window.printf("checkerboard id = %d\n", g_checkerboardId);
#endif

		framework.EndFrame(*gfxc);
	}

	Gnmx::GnmxGfxContext *gfxc = &commandBuffers[framework.m_backBufferIndex]; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
	framework.terminate(*gfxc);
	return 0;
}

