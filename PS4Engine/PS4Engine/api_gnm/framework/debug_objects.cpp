/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "debug_objects.h"
#include "simple_mesh.h"
#include <algorithm>
#include "../toolkit/embedded_shader.h"

namespace Framework {

namespace {

const uint32_t debug_vertex_p[] ={
#include "debug_vertex_p.h"
};

const uint32_t debug_vertex_vv[] ={
#include "debug_vertex_vv.h"
};

Gnmx::Toolkit::EmbeddedPsShader s_debug_vertex_p ={debug_vertex_p,"Debug Geometry Pixel Shader"};
Gnmx::Toolkit::EmbeddedVsShader s_debug_vertex_vv ={debug_vertex_vv,"Debug Geometry Vertex Shader"};
Gnmx::Toolkit::EmbeddedPsShader *s_embeddedPsShader[] ={&s_debug_vertex_p};
Gnmx::Toolkit::EmbeddedVsShader *s_embeddedVsShader[] ={&s_debug_vertex_vv};

Gnmx::Toolkit::EmbeddedShaders s_embeddedShaders =
{
	0, 0,
	s_embeddedPsShader,sizeof(s_embeddedPsShader) / sizeof(s_embeddedPsShader[0]),
	s_embeddedVsShader,sizeof(s_embeddedVsShader) / sizeof(s_embeddedVsShader[0]),
};

}

void DebugObjects::initializeWithAllocators(Gnmx::Toolkit::Allocators *allocators)
{
	s_embeddedShaders.initializeWithAllocators(allocators);
	for(unsigned i = 0; i < s_embeddedShaders.m_embeddedVsShaders; ++i)
	{
		Gnmx::Toolkit::EmbeddedVsShader *embeddedVsShader = s_embeddedShaders.m_embeddedVsShader[i];
		allocators->allocate((void**)&embeddedVsShader->m_fetchShader,SCE_KERNEL_WC_GARLIC,Gnmx::computeVsFetchShaderSize(embeddedVsShader->m_shader),Gnm::kAlignmentOfBufferInBytes,Gnm::kResourceTypeFetchShaderBaseAddress,"Framework Fetch Shader %d",i);
		uint32_t shaderModifier;
		Gnmx::generateVsFetchShader(embeddedVsShader->m_fetchShader,&shaderModifier,embeddedVsShader->m_shader);
		embeddedVsShader->m_shader->applyFetchShaderModifier(shaderModifier);
	}
}

void DebugObjects::Init(Gnmx::Toolkit::Allocators *allocators,const char *name,Mesh *sphereMesh,Mesh *coneMesh,Mesh *cylinderMesh)
{
	m_sphereMesh = sphereMesh;
	m_coneMesh = coneMesh;
	m_cylinderMesh = cylinderMesh;

	allocators->allocate((void**)&m_debugVertex,SCE_KERNEL_WB_ONION,sizeof(DebugVertex)    * kDebugVertices,4,Gnm::kResourceTypeVertexBufferBaseAddress,"%s Debug Vertex",name);
	allocators->allocate((void**)&m_debugConstants,SCE_KERNEL_WB_ONION,sizeof(DebugConstants) * kDebugObjects,16,Gnm::kResourceTypeConstantBufferBaseAddress,"%s Debug Constants",name);
	allocators->allocate((void**)&m_debugMeshConstants,SCE_KERNEL_WB_ONION,sizeof(DebugConstants) * kDebugMeshes,16,Gnm::kResourceTypeConstantBufferBaseAddress,"%s Debug MeshConstants",name);

	Clear();
}

void DebugObjects::Init(Gnmx::Toolkit::IAllocator *allocator,Mesh *sphereMesh,Mesh *coneMesh,Mesh *cylinderMesh)
{
	Gnmx::Toolkit::Allocators allocators(*allocator,*allocator);
	return Init(&allocators,0,sphereMesh,coneMesh,cylinderMesh);
}

void DebugObjects::BeginDebugMesh()
{
	SCE_GNM_ASSERT(m_debugObjects < kDebugObjects);
	m_debugObject[m_debugObjects].m_vertex = m_debugVertex + m_debugVertices;
	m_debugObject[m_debugObjects].m_vertices = 0;
}

void DebugObjects::AddDebugTriangle(const DebugVertex &a,const DebugVertex &b,const DebugVertex &c)
{
	SCE_GNM_ASSERT(m_debugVertices < kDebugVertices);
	m_debugVertex[m_debugVertices++] = a;
	SCE_GNM_ASSERT(m_debugVertices < kDebugVertices);
	m_debugVertex[m_debugVertices++] = b;
	SCE_GNM_ASSERT(m_debugVertices < kDebugVertices);
	m_debugVertex[m_debugVertices++] = c;
	m_debugObject[m_debugObjects].m_vertices += 3;
}

void DebugObjects::AddDebugQuad(const DebugVertex &a,const DebugVertex &b,const DebugVertex &c,const DebugVertex &d)
{
	AddDebugTriangle(a,b,c);
	AddDebugTriangle(d,a,c);
}

void DebugObjects::AddDebugTriangle(const Vector4 &color,const Vector3 &p0,const Vector3 &p1,const Vector3 &p2)
{
	DebugVertex debugVertex[3];
	debugVertex[0].m_color = color;
	debugVertex[1].m_color = color;
	debugVertex[2].m_color = color;
	debugVertex[0].m_position = p0;
	debugVertex[1].m_position = p1;
	debugVertex[2].m_position = p2;
	AddDebugTriangle(debugVertex[0],debugVertex[1],debugVertex[2]);
}

void DebugObjects::AddDebugQuad(const Vector4 &color,const Vector3 &p0,const Vector3 &p1,const Vector3 &p2,const Vector3 &p3)
{
	AddDebugTriangle(color,p0,p1,p2);
	AddDebugTriangle(color,p2,p3,p0);
}

void DebugObjects::EndDebugMesh(const Vector4 &color,const Matrix4 &modelViewProjection)
{
	DebugConstants &debugConstants = m_debugConstants[m_debugObjects];
	debugConstants.m_modelViewProjection = modelViewProjection;
	debugConstants.m_color = color;
	++m_debugObjects;
}

void DebugObjects::AddDebugShape(Mesh *mesh,const Vector4 &color,const Matrix4 &modelViewProjection)
{
	SCE_GNM_ASSERT(m_debugMeshes < kDebugMeshes);
	DebugConstants &debugConstants = m_debugMeshConstants[m_debugMeshes];
	debugConstants.m_modelViewProjection = modelViewProjection;
	debugConstants.m_color = color;
	m_debugMesh[m_debugMeshes] = mesh;
	++m_debugMeshes;
}

void DebugObjects::AddDebugShape(Mesh *mesh,const Vector3 &a,const Vector3 &b,float radius,const Vector4 &color,const Matrix4 &viewProjection)
{
	const Vector3 y = normalize(b - a);
	const float height = length(b - a);
	const Vector3 absY = maxPerElem(y,-y);
	const float smallest = std::min(absY.getX(),std::min(absY.getY(),absY.getZ()));

	Vector3 x(0.f,0.f,0.f);
	if(absY.getX() == smallest)
		x = Vector3(1.f,0.f,0.f);
	if(absY.getY() == smallest)
		x = Vector3(0.f,1.f,0.f);
	if(absY.getZ() == smallest)
		x = Vector3(0.f,0.f,1.f);
	x = normalize(x - y * dot(y,x));
	const Vector3 z = normalize(cross(x,y));
	Matrix4 model;
	model.setCol0(Vector4(x * radius,0.f));
	model.setCol1(Vector4(y * height,0.f));
	model.setCol2(Vector4(z * radius,0.f));
	model.setCol3(Vector4(a,1.f));
	const Matrix4 modelViewProjection = viewProjection * model;
	AddDebugShape(mesh,color,modelViewProjection);
}

void DebugObjects::AddDebugSphere(const Vector3 &position,float radius,const Vector4 &color,const Matrix4 &viewProjection)
{
	const Matrix4 model = Matrix4::translation(position) * Matrix4::scale(Vector3(radius,radius,radius));
	AddDebugShape(m_sphereMesh,color,viewProjection * model);
}

void DebugObjects::AddDebugCylinder(const Vector3 &a,const Vector3 &b,float radius,const Vector4 &color,const Matrix4 &viewProjection)
{
	AddDebugShape(m_cylinderMesh,a,b,radius,color,viewProjection);
}

void DebugObjects::AddDebugCone(const Vector3 &a,const Vector3 &b,float radius,const Vector4 &color,const Matrix4 &viewProjection)
{
	AddDebugShape(m_coneMesh,a,b,radius,color,viewProjection);
}

void DebugObjects::AddDebugArrow(const Vector3 &a,const Vector3 &b,float lineRadius,float arrowRadius,float arrowLength,const Vector4 &color,const Matrix4 &viewProjection)
{
	const Vector3 direction = normalize(b - a);
	const Vector3 middle = b - direction * arrowLength;
	AddDebugCylinder(a,middle,lineRadius,color,viewProjection);
	AddDebugCone(middle,b,arrowRadius,color,viewProjection);
}

void DebugObjects::RenderObject(sce::Gnmx::GnmxGfxContext &gfxc,uint32_t objectIndex)
{
	Gnm::Buffer vertexBuffer[2];
	vertexBuffer[0].initAsVertexBuffer(static_cast<uint8_t*>(static_cast<void*>(m_debugObject[objectIndex].m_vertex)) + offsetof(DebugVertex,m_position),Gnm::kDataFormatR32G32B32Float,sizeof(DebugVertex),m_debugObject[objectIndex].m_vertices);
	vertexBuffer[1].initAsVertexBuffer(static_cast<uint8_t*>(static_cast<void*>(m_debugObject[objectIndex].m_vertex)) + offsetof(DebugVertex,m_color),Gnm::kDataFormatR32G32B32A32Float,sizeof(DebugVertex),m_debugObject[objectIndex].m_vertices);
	vertexBuffer[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
	vertexBuffer[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
	gfxc.setVertexBuffers(Gnm::kShaderStageVs,0,2,vertexBuffer);

	const void *constants = static_cast<const void*>(&m_debugConstants[objectIndex]);
	Gnm::Buffer constantBuffer;
	constantBuffer.initAsConstantBuffer(const_cast<void*>(constants),sizeof(DebugConstants));
	constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
	gfxc.setConstantBuffers(Gnm::kShaderStageVs,0,1,&constantBuffer);

	gfxc.drawIndexAuto(m_debugObject[objectIndex].m_vertices);
}

void DebugObjects::RenderMesh(sce::Gnmx::GnmxGfxContext &gfxc,uint32_t meshIndex)
{
	Mesh *mesh = m_debugMesh[meshIndex];
	mesh->SetVertexBuffer(gfxc,Gnm::kShaderStageVs);

	const void *constants = static_cast<const void*>(&m_debugMeshConstants[meshIndex]);
	Gnm::Buffer constantBuffer;
	constantBuffer.initAsConstantBuffer(const_cast<void*>(constants),sizeof(DebugConstants));
	constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
	gfxc.setConstantBuffers(Gnm::kShaderStageVs,0,1,&constantBuffer);

	gfxc.drawIndex(mesh->m_indexCount,mesh->m_indexBuffer,0,0,Gnm::kDrawModifierDefault);
}

void DebugObjects::Render(sce::Gnmx::GnmxGfxContext &gfxc,const Matrix4 & /*viewProjectionMatrix*/)
{
	Gnm::PrimitiveSetup primSetupReg;
	primSetupReg.init();
	primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceNone);
	gfxc.setPrimitiveSetup(primSetupReg);

	Gnm::BlendControl blendControl;
	blendControl.init();
	blendControl.setBlendEnable(true);
	blendControl.setColorEquation(Gnm::kBlendMultiplierSrcAlpha,Gnm::kBlendFuncAdd,Gnm::kBlendMultiplierOneMinusSrcAlpha);
	blendControl.setSeparateAlphaEnable(false);
	gfxc.setBlendControl(0,blendControl);

	Gnm::DepthStencilControl dsc;
	dsc.init();
	dsc.setDepthControl(Gnm::kDepthControlZWriteDisable,Gnm::kCompareFuncLessEqual);
	dsc.setDepthEnable(true);
	gfxc.setDepthStencilControl(dsc);

	gfxc.setPsShader(s_debug_vertex_p.m_shader,&s_debug_vertex_p.m_offsetsTable);
	gfxc.setVsShader(s_debug_vertex_vv.m_shader,0,s_debug_vertex_vv.m_fetchShader,&s_debug_vertex_vv.m_offsetsTable); // passing 0 as modifier since the shader has already been patched.
	gfxc.setPrimitiveType(Gnm::kPrimitiveTypeTriList);
	gfxc.setIndexSize(Gnm::kIndexSize16);

	for(unsigned o = 0; o < m_debugObjects; ++o)
		RenderObject(gfxc,o);
	for(unsigned o = 0; o < m_debugMeshes; ++o)
		RenderMesh(gfxc,o);

	dsc.init();
	dsc.setDepthControl(Gnm::kDepthControlZWriteEnable,Gnm::kCompareFuncLessEqual);
	dsc.setDepthEnable(true);
	gfxc.setDepthStencilControl(dsc);

	blendControl.init();
	blendControl.setBlendEnable(false);
	gfxc.setBlendControl(0,blendControl);

	primSetupReg.init();
	primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
	primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
	gfxc.setPrimitiveSetup(primSetupReg);
}

void DebugObjects::Clear()
{
	m_debugVertices = 0;
	m_debugObjects = 0;
	m_debugMeshes = 0;
}

void BuildDebugConeMesh(Gnmx::Toolkit::IAllocator *allocator,Framework::Mesh *destMesh,float /*radius*/,float /*height*/,unsigned div)
{
	enum { kPosition,kColor,kAttributes };
	const Gnm::DataFormat dataFormat[kAttributes] =
	{
		Gnm::kDataFormatR8G8B8A8Snorm,
		Gnm::kDataFormatR8G8B8A8Unorm
	};
	destMesh->allocate(dataFormat,kAttributes,div + 1,3 * div,allocator);

	// Everything else is just filling in the vertex and index buffer.
	uint16_t *outI = static_cast<uint16_t*>(destMesh->m_indexBuffer);

	const float g = 2 * M_PI / div;
	uint32_t ov = 0;
	const Vector4 white(1.f,1.f,1.f,1.f);
	for(uint32_t i = 0; i < div; ++i)
	{
		const float theta = (float)i * g;
		destMesh->setElement(kColor,ov,white);
		const Vector4 position(cosf(theta),0.f,sinf(theta),1.f);
		destMesh->setElement(kPosition,ov,position);
		++ov;
	}
	destMesh->setElement(kColor,ov,white);
	const Vector4 position(0.f,1.f,0.f,1.f);
	destMesh->setElement(kPosition,ov,position);
	++ov;
	SCE_GNM_ASSERT(ov == destMesh->m_vertexCount);

	uint32_t oi = 0;
	for(uint32_t i = 0; i < div; ++i)
	{
		outI[oi++] = i;
		outI[oi++] = (i + 1) % div;
		outI[oi++] = div;
	}
	SCE_GNM_ASSERT(oi == destMesh->m_indexCount);
}

void BuildDebugCylinderMesh(Gnmx::Toolkit::IAllocator *allocator,Framework::Mesh *destMesh,float /*radius*/,float /*height*/,unsigned div)
{
	enum { kPosition,kColor,kAttributes };
	const Gnm::DataFormat dataFormat[kAttributes] =
	{
		Gnm::kDataFormatR8G8B8A8Snorm,
		Gnm::kDataFormatR8G8B8A8Unorm
	};
	destMesh->allocate(dataFormat,kAttributes,div * 2,((div - 2) * 2 + div * 2) * 3,allocator);

	// Everything else is just filling in the vertex and index buffer.
	uint16_t *outI = static_cast<uint16_t*>(destMesh->m_indexBuffer);

	const float g = 2 * M_PI / div;
	uint32_t ov = 0;
	const Vector4 white(1.f,1.f,1.f,1.f);
	for(uint32_t i = 0; i < div; ++i)
	{
		const float theta = (float)i * g;
		destMesh->setElement(kColor,ov,white);
		const Vector4 position(cosf(theta),0.f,sinf(theta),1.f);
		destMesh->setElement(kPosition,ov,position);
		++ov;
	}
	for(uint32_t i = div; i < div * 2; ++i)
	{
		const float theta = (float)i * g;
		destMesh->setElement(kColor,ov,white);
		const Vector4 position(cosf(theta),1.f,sinf(theta),1.f);
		destMesh->setElement(kPosition,ov,position);
		++ov;
	}
	SCE_GNM_ASSERT(ov == destMesh->m_vertexCount);

	uint32_t oi = 0;
	for(uint32_t i = 0; i < div; ++i)
	{
		const uint32_t index[4] ={i,(i + 1) % div,div + i,div + (i + 1) % div};
		outI[oi++] = index[0];
		outI[oi++] = index[1];
		outI[oi++] = index[2];
		outI[oi++] = index[2];
		outI[oi++] = index[1];
		outI[oi++] = index[3];
	}
	for(uint32_t i = 0; i < div - 2; ++i)
	{
		outI[oi++] = 0;
		outI[oi++] = i;
		outI[oi++] = (i + 1) % div;
	}
	for(uint32_t i = 0; i < div - 2; ++i)
	{
		outI[oi++] = div + 0;
		outI[oi++] = div + i;
		outI[oi++] = div + (i + 1) % div;
	}
	SCE_GNM_ASSERT(oi == destMesh->m_indexCount);
}

void BuildDebugSphereMesh(Gnmx::Toolkit::IAllocator *allocator,Framework::Mesh *destMesh,float /*radius*/,int xdiv,int ydiv)
{
	enum { kPosition,kColor,kAttributes };
	const Gnm::DataFormat dataFormat[kAttributes] =
	{
		Gnm::kDataFormatR8G8B8A8Snorm,
		Gnm::kDataFormatR8G8B8A8Unorm
	};
	destMesh->allocate(dataFormat,kAttributes,(xdiv + 1) * (ydiv + 1),(xdiv * (ydiv - 1) * 2) * 3,allocator);

	// Everything else is just filling in the vertex and index buffer.
	uint16_t *outI = static_cast<uint16_t*>(destMesh->m_indexBuffer);

	const float gx = 2*M_PI / xdiv;
	const float gy = M_PI / ydiv;

	const Vector4 white(1.f,1.f,1.f,1.f);
	for(long i = 0; i < xdiv; ++i)
	{
		const float theta = (float)i * gx;
		const float ct = cosf(theta);
		const float st = sinf(theta);

		const long k = i * (ydiv + 1);
		for(long j = 1; j < ydiv; ++j)
		{
			const float phi = (float)j * gy;
			const float sp = sinf(phi);
			const float x = ct * sp;
			const float y = st * sp;
			const float z = cosf(phi);

			destMesh->setElement(kPosition,k+j,Vector4(x,y,z,1.f));
			destMesh->setElement(kColor,k+j,white);
		}
	}

	const long kk = xdiv * (ydiv + 1);
	for(long j = 1; j < ydiv; ++j)
	{
		const float phi = (float)j * gy;
		const float x = sinf(phi);
		const float z = cosf(phi);

		destMesh->setElement(kPosition,kk+j,Vector4(x,0.f,z,1.f));
		destMesh->setElement(kColor,kk+j,white);
	}

	for(long i = 0; i < xdiv; i++)
	{
		const long k1 = i * (ydiv + 1) + 1;

		destMesh->setElement(kPosition,k1-1,Vector4(0.f,0.f,1.f,1.f));
		destMesh->setElement(kColor,k1-1,white);

		destMesh->setElement(kPosition,k1+ydiv-1,Vector4(0.f,0.f,-1.f,1.f));
		destMesh->setElement(kColor,k1+ydiv-1,white);
	}

	destMesh->setElement(kPosition,xdiv*(ydiv+1),destMesh->getElement(kPosition,0));
	destMesh->setElement(kColor,xdiv*(ydiv+1),white);

	destMesh->setElement(kPosition,xdiv*(ydiv+1)+ydiv,destMesh->getElement(kPosition,ydiv));
	destMesh->setElement(kColor,xdiv*(ydiv+1)+ydiv,white);

	long ii = 0;
	for(long i = 0; i < xdiv; ++i)
	{
		const long k = i * (ydiv + 1);

		outI[ii+0] = (uint16_t)k;
		outI[ii+1] = (uint16_t)(k + 1);
		outI[ii+2] = (uint16_t)(k + ydiv + 2);
		ii += 3;

		for(long j = 1; j < ydiv - 1; ++j)
		{
			outI[ii+0] = (uint16_t)(k + j);
			outI[ii+1] = (uint16_t)(k + j + 1);
			outI[ii+2] = (uint16_t)(k + j + ydiv + 2);
			outI[ii+3] = (uint16_t)(k + j);
			outI[ii+4] = (uint16_t)(k + j + ydiv + 2);
			outI[ii+5] = (uint16_t)(k + j + ydiv + 1);
			ii += 6;
		}

		outI[ii+0] = (uint16_t)(k + ydiv - 1);
		outI[ii+1] = (uint16_t)(k + ydiv);
		outI[ii+2] = (uint16_t)(k + ydiv * 2);
		ii += 3;
	}
}

}
