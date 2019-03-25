/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "yif2gpu_reader.h"
#include "../../framework/sample_framework.h"
#include "../../framework/simple_mesh.h"
#include <assert.h>
using namespace sce;

struct SPatchGpuVertex
{
	Vector3Unaligned vPos;
};


struct SMainGpuVertex
{
	Vector3Unaligned vPos;
	Vector3Unaligned vNorm;
	Vector4Unaligned vTangent;
};

struct SSharedAttribs
{
	//unsigned int color;
	float s, t;
};

struct SSkinInfo
{
	unsigned int uWeights0;	// 10 bits per weight
	unsigned int uWeights1;
	unsigned int uMPal0;	// three 10 bit indices
	unsigned int uMPal1;	// three 10 bit indices
};


struct SSinglePassSkinnedGpuVertex
{
	SMainGpuVertex v;
	SSharedAttribs a;
	SSkinInfo s;
};

struct SDualPassSkinnedComputeVertexIn
{
	SMainGpuVertex v;
	SSkinInfo s;
};

struct SPatchesSkinnedVertex
{
	Vector3Unaligned vPos;
	SSkinInfo s;
};



int Max(const int a, const int b) { return a>b?a:b; }

struct SGPUMesh
{
	Framework::SimpleMesh simple;
	CYif2GpuReader::MemBlock m_patchIndexBuffer;
	int m_iNrPreValBuffers;
	uint8_t m_reserved0[4];
	CYif2GpuReader::MemBlock * m_pPreValBuffers;
	CYif2GpuReader::MemBlock m_lsbFetchShader;
};

void CYif2GpuReader::ToggleMode()
{
	m_bIsSinglePass = m_bIsSinglePass==true ? false : true;
	PrepStreams();
}

void CYif2GpuReader::SetGlobalTessFactor(const int iTessFactor)
{
	m_iTessFactor = iTessFactor;
}

void CYif2GpuReader::ToggleTangInDS()
{
	m_bGenTangsInDS = !m_bGenTangsInDS;
	m_iNrPatchesOffset = 0;
}

void CYif2GpuReader::ToggleReadVertsInHS()
{
	m_bReadVertsInHS = !m_bReadVertsInHS;
	if(m_bReadVertsInHS == true && m_bIsSinglePass == true)
		ToggleMode();
	m_iNrPatchesOffset = 0;
}

void CYif2GpuReader::SetTessSupported(const bool bEnable)
{
	if(bEnable!=m_bTessEnabled)
	{
		m_bTessEnabled = bEnable;
		PrepStreams();
		m_iNrPatchesOffset = 0;
	}
}

void CYif2GpuReader::SetAnimationSupported(const bool bEnable)
{
	if(bEnable!=m_bAnimEnabled)
	{
		m_bAnimEnabled = bEnable;
		PrepStreams();
		m_iNrPatchesOffset = 0;
	}
}


void CYif2GpuReader::TogglePatchesMethod()
{
	m_bIsGregoryPatches = !m_bIsGregoryPatches;

	m_iNrPatchesOffset = 0;
}

void CYif2GpuReader::ToggleNrThreadsPerRegularPatch()
{
	m_bIsFourThreadsPerRegPatch = !m_bIsFourThreadsPerRegPatch;
	m_iNrPatchesOffset = 0;
}

bool CYif2GpuReader::IsModeReadVertsInHS() const
{
	return m_bReadVertsInHS;
}

bool CYif2GpuReader::IsModeGenTangsInDS() const
{
	return m_bGenTangsInDS;
}

bool CYif2GpuReader::IsPatchModeGregory() const
{
	return m_bIsGregoryPatches;
}



int CYif2GpuReader::GetNrRegularPatches() const
{
	return m_iNrRegPatches;
}

int CYif2GpuReader::GetNrIrregularPatches() const
{
	return m_iNrIrregPatches;
}

int CYif2GpuReader::GetNrThreadsPerRegularPatch() const
{
	return m_bIsFourThreadsPerRegPatch ? 4 : 16;
}

void CYif2GpuReader::DecreaseNrPatchesOffset()
{
	if(m_iNrPatchesOffset > -5)
	--m_iNrPatchesOffset;
}

void CYif2GpuReader::IncreaseNrPatchesOffset()
{
	if(m_iNrPatchesOffset < 10)
		++m_iNrPatchesOffset;
}

bool CYif2GpuReader::ReadScene(sce::Gnmx::Toolkit::Allocators* allocators, const char file[])
{
	bool bRes = false;
	if( CYifSceneReader::ReadScene(file) )
	{
		// load geometry shaders
		m_csb_skin = Framework::LoadCsShader("/app0/patches-sample/yifplayer/skinning_c.sb", allocators);
		m_csb_skinCC = Framework::LoadCsShader("/app0/patches-sample/yifplayer/skinningCC_c.sb", allocators);
		m_vsb_skin = Framework::LoadVsShader("/app0/patches-sample/yifplayer/skinning_vv.sb", allocators);
		m_vsb_noskin = Framework::LoadVsShader("/app0/patches-sample/yifplayer/shader_vv.sb", allocators);
		m_lsb_skin = Framework::LoadLsShader("/app0/patches-sample/yifplayer/LSskinning_vl.sb", allocators);
		m_lsb_noskin = Framework::LoadLsShader("/app0/patches-sample/yifplayer/LSshader_vl.sb", allocators);
		m_lsb_hsvert_noskin = Framework::LoadLsShader("/app0/patches-sample/yifplayer/LSshader_hsvert_vl.sb", allocators);

		// generate fetch shaders
		MemBlock * pBlockArray[] = {&m_vsb_noskin_fetch, &m_vsb_skin_fetch, &m_lsb_noskin_fetch, &m_lsb_skin_fetch};
		for(int f=0; f<4; f++)
		{
			MemBlock &fetch = *pBlockArray[f];
			uint32_t fssz = 0;

			if(f<2) 
			{
				fssz = Gnmx::computeVsFetchShaderSize(f==0 ? m_vsb_noskin : m_vsb_skin);
			}
			else 
			{
				fssz = Gnmx::computeLsFetchShaderSize(f==2 ? m_lsb_noskin : m_lsb_skin);
			}
			fetch = static_cast<uint32_t*>(allocators->m_garlic.allocate(fssz, Gnm::kAlignmentOfFetchShaderInBytes));
			if(f<2) 
			{
				uint32_t shaderModifier;
				Gnmx::VsShader *vsShader = f==0 ? m_vsb_noskin : m_vsb_skin;
				Gnmx::generateVsFetchShader(fetch, &shaderModifier, vsShader);
				vsShader->applyFetchShaderModifier(shaderModifier);
			}
			else 
			{
				uint32_t shaderModifier;
				Gnmx::LsShader *lsShader = f==2 ? m_lsb_noskin : m_lsb_skin;
				Gnmx::generateLsFetchShader(fetch, &shaderModifier, lsShader);
				lsShader->applyFetchShaderModifier(shaderModifier);
			}
		}

		//
		m_dsbirreg_dt_patch = Framework::LoadVsShader("/app0/patches-sample/yifplayer/acc_dtangents/ccpatchirreg_dv.sb", allocators);
		m_dsbirreg_ht_patch = Framework::LoadVsShader("/app0/patches-sample/yifplayer/acc_htangents/ccpatchirreg_dv.sb", allocators);
		m_dsb_bez_patch = Framework::LoadVsShader("/app0/patches-sample/yifplayer/reg_patches/patchbez_dv.sb", allocators);
		m_dsb_bez_patch_16threads = Framework::LoadVsShader("/app0/patches-sample/yifplayer/reg_patches/patchbez_16threads_dv.sb", allocators);
		m_dsb_greg_patch = Framework::LoadVsShader("/app0/patches-sample/yifplayer/greg_patches/gpatch_dv.sb", allocators);

		const char hs_irreg_files[][256] = {"acc_dtangents/ccpatchirreg", "acc_htangents/ccpatchirreg", "acc_dtangents/ccpatchirreg_hsvert", "acc_htangents/ccpatchirreg_hsvert"};
		const char   hs_reg_files[][256] = {"reg_patches/patchreg", "reg_patches/patchreg_hsvert", "reg_patches/patchreg_16threads", "reg_patches/patchreg_16threads_hsvert"};
		const char   hs_Greg_files[][256] = {"greg_patches/gpatchirreg", "greg_patches/gpatchirreg_hsvert"};

		for(int i=0; i<4; i++)
		{
			char temp[3][256];
			strncpy(temp[0], "/app0/patches-sample/yifplayer/", 64);
			strncat(temp[0], hs_irreg_files[i], 128);
			strncat(temp[0], "_h.sb", 8);

			strncpy(temp[1], "/app0/patches-sample/yifplayer/", 64);
			strncat(temp[1], hs_reg_files[i], 128);
			strncat(temp[1], "_h.sb", 8);

			if(i<2) 
			{
				strncpy(temp[2], "/app0/patches-sample/yifplayer/", 64);
				strncat(temp[2], hs_Greg_files[i], 128);
				strncat(temp[2], "_h.sb", 8);
			}

			m_hsbirreg_patch[i] = Framework::LoadHsShader(temp[0], allocators);
			m_hsbreg_patch[i] = Framework::LoadHsShader(temp[1], allocators);
			if(i<2) m_hsbGreg_patch[i] = Framework::LoadHsShader(temp[2], allocators);
		}

		for(int b=0; b<4; b++)
			m_params_HsTg[b].tessCnstMemBlock = static_cast<uint32_t*>(allocators->m_garlic.allocate(256, Gnm::kAlignmentOfBufferInBytes));

		//
		const int iNrTransf = Max(GetNumTransformations(),1);
		m_pMats = new Matrix4[iNrTransf];
		const int iNrGPUMeshes = Max(GetNumMeshes(),1);
		m_pGpuMeshes = new SGPUMesh[iNrGPUMeshes];
		const int iNrEntries = Max(GetNumMeshInstances(),1);
		m_psInstCbuf1 = new SInstCBufs[iNrEntries];
		memset(m_psInstCbuf1, 0, sizeof(SInstCBufs)*iNrEntries);
		m_psInstCbuf2 = new SInstCBufs[iNrEntries];
		memset(m_psInstCbuf2, 0, sizeof(SInstCBufs)*iNrEntries);
		if(m_pMats!=NULL && m_pGpuMeshes!=NULL && m_psInstCbuf1!=NULL && m_psInstCbuf2!=NULL)
		{
			uint32_t iMaxNrVerts = 0;
			for(int m=0; m<GetNumMeshes(); m++)
			{
				Mesh2GPU(allocators, &m_pGpuMeshes[m], *GetMeshRef(m));
				Framework::SimpleMesh &sime = m_pGpuMeshes[m].simple;
				if(iMaxNrVerts<sime.m_vertexCount) iMaxNrVerts=sime.m_vertexCount;
			}

			{
				const int iDwordsPerElemOut = sizeof(SMainGpuVertex) / 4;

				const int iNrEntries = 64*((iMaxNrVerts+63)/64);

				for(int b=0; b<2; b++)
				{
					void * cpuVm = (uint8_t*)allocators->m_onion.allocate(iNrEntries * iDwordsPerElemOut * sizeof(uint32_t), Gnm::kAlignmentOfBufferInBytes);		// cpu access
					if(b==0) m_uPtrGPUskinning1=cpuVm;
					else m_uPtrGPUskinning2=cpuVm;
				}
				m_uPtrGPUskinningCur = m_uPtrGPUskinning1;
			}

			const int iSizeMat = sizeof(float)*16;
			for(int m=0; m<GetNumMeshInstances(); m++)
			{
				int iSizeOfMeshInstCB = sizeof(cbMeshInstance), iSizeBonesPalette=0;
				CYifMeshInstance &meshinst = *GetMeshInstanceRef(m);
				const int mesh_index = meshinst.GetMeshIndex();
				if(mesh_index>=0)
				{
					CYifMesh &mesh = *GetMeshRef(mesh_index);
					const int iNrBones = mesh.GetNumBones();
					iSizeBonesPalette += iNrBones*iSizeMat;
				}

				// regular instance constant buffer
				for(int b=0; b<2; b++)
				{
					void * pInstCBuf = allocators->m_onion.allocate(iSizeOfMeshInstCB, Gnm::kAlignmentOfBufferInBytes);
					Gnm::Buffer * pBuf = b==0 ? &m_psInstCbuf1[m].m_sInstData : &m_psInstCbuf2[m].m_sInstData;
					pBuf->initAsConstantBuffer(pInstCBuf, iSizeOfMeshInstCB);
					pBuf->setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
					memset(pInstCBuf, 0, iSizeOfMeshInstCB);

					// bones palette
					if(iSizeBonesPalette>0)
					{
						Matrix4* pBonesMem =reinterpret_cast<Matrix4*>(allocators->m_garlic.allocate(iSizeBonesPalette, sizeof(Matrix4)));
						Gnm::Buffer * pBonesBuf = b==0 ? &m_psInstCbuf1[m].m_sBonesPalette : &m_psInstCbuf2[m].m_sBonesPalette;
						pBonesBuf->initAsConstantBuffer(pBonesMem, iSizeBonesPalette);
						pBonesBuf->setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
						memset(pBonesMem, 0, iSizeBonesPalette);
					}
				}
			}

			m_psInstCbuf = m_psInstCbuf1;
			bRes = true;
		}
		else
		{
			if(m_pMats!=NULL) { delete [] m_pMats; m_pMats=NULL; }
			if(m_pGpuMeshes!=NULL) { delete [] m_pGpuMeshes; m_pGpuMeshes=NULL; }
			if(m_psInstCbuf1!=NULL) { delete [] m_psInstCbuf1; m_psInstCbuf1=NULL; }
			if(m_psInstCbuf2!=NULL) { delete [] m_psInstCbuf2; m_psInstCbuf2=NULL; }
		}
	}

	return bRes;
}

// t \in [0;1]
void CYif2GpuReader::UpdateTransformationHierarchy(const float t, const Matrix4 &m44WorldToView, const Matrix4 &m44Proj)
{
	if(GetNumTransformations()>0)
	{
		const int node_index = 0;
		const int parent_index = -1;
		UpdateNodeTransformation(node_index, parent_index, t);
	}
	const int iMeshInsts = GetNumMeshInstances();
	const Matrix4 m44WorldToProj = m44Proj * m44WorldToView;

	for(int m=0; m<iMeshInsts; m++)
	{
		CYifMeshInstance &meshinst = *GetMeshInstanceRef(m);

		const int mesh_index = meshinst.GetMeshIndex();
		const int transf_index = meshinst.GetTransformIndex();

		const Matrix4 &obj2world = m_pMats[transf_index];

		const Matrix4 obj2view = m44WorldToView * obj2world;
		const Matrix4 obj2proj = m44WorldToProj * obj2world;

		// transpose for shader layout
		cbMeshInstance &mesh_inst = ((cbMeshInstance *) m_psInstCbuf[m].m_sInstData.getBaseAddress())[0];
		mesh_inst.g_mLocToProj = transpose(obj2proj);
		mesh_inst.g_mLocToView = transpose(obj2view);
		mesh_inst.g_fTessFactor = (float) m_iTessFactor;


		// setup bones
		CYifMesh &mesh = *GetMeshRef(mesh_index);

		const int iNrBones = mesh.GetNumBones();
		const Matrix4* pm44InvBones = mesh.GetInvMats();
		const int * piBoneToTransform = meshinst.GetBoneToTransformIndices();

		if(m_bAnimEnabled)
		{
			for(int b=0; b<iNrBones; b++)
			{
				assert(piBoneToTransform[b]<GetNumTransformations());
				const Matrix4 &bone = m_pMats[piBoneToTransform[b]];
				const Matrix4 &invbone = pm44InvBones[b];
				((Matrix4*)m_psInstCbuf[m].m_sBonesPalette.getBaseAddress())[b] = transpose(bone*invbone);
			}
		}
	}

	m_bNoUpdateYet=false;
}

void CYif2GpuReader::UpdateNodeTransformation(const int node_index, const int parent_index, const float t)
{
	CYifTransformation * pNode = GetTransformationRef(node_index);
	assert(pNode!=NULL && node_index>=0 && node_index<GetNumTransformations());

	m_pMats[node_index] = pNode->EvalTransform(t);
	if(parent_index>=0) m_pMats[node_index] = (Matrix4)m_pMats[parent_index] * (Matrix4)m_pMats[node_index];

	const int iIndexToFirstChild = pNode->GetFirstChild();
	const int iIndexToNextSibling = pNode->GetNextSibling();
	if(iIndexToFirstChild>=0)
		UpdateNodeTransformation(iIndexToFirstChild, node_index, t);
	if(iIndexToNextSibling>=0)
		UpdateNodeTransformation(iIndexToNextSibling, parent_index, t);
}




// pedantic implementation
unsigned int To8bitf(const float fX_in)
{
	const float fX = fX_in<0?0:(fX_in>1?1:fX_in);
	const int iX = (int) (fX*255+0.5f);
	return (unsigned int) (iX<0?0:(iX>255?255:iX));
}

// pedantic implementation
unsigned int To10bitf(const float fX_in)
{
	const float fX = fX_in<0?0:(fX_in>1?1:fX_in);
	const int iX = (int) (fX*1023+0.5f);
	return (unsigned int) (iX<0?0:(iX>1023?1023:iX));
}

unsigned int To10biti(const int iX_in)
{
	assert(iX_in>=0 && iX_in<1024);
	return (unsigned int) (iX_in&1023);
}

void SetMainVert(SMainGpuVertex * pV, const SYifVertex &v)
{
	pV->vPos = v.vPos; pV->vNorm = v.vNorm; pV->vTangent = v.vTangent;
}


void SetAttribs(SSharedAttribs * pA, const SYifVertex &v)
{
	pA->s = v.s; pA->t = v.t;
	//pA->color = (To10biti(To8bitf(v.vColor.w))<<24) | (To10biti(To8bitf(v.vColor.z))<<16) | (To8bitf(v.vColor.y)<<8) | To8bitf(v.vColor.x);
}

void SetSkinInfo(SSkinInfo * pS, const SYifSkinVertInfo &v)
{
	Vector4Unaligned vW0 = v.vWeights0;
	Vector4Unaligned vW1 = v.vWeights1;

	const float fD = vW0.x+vW0.y+vW0.z+vW0.w+vW1.x+vW1.y;//+vW1.z+vW1.w;
	pS->uWeights0 = (To10bitf(vW0.z/fD)<<20) | (To10bitf(vW0.y/fD)<<10) | To10bitf(vW0.x/fD);
	pS->uWeights1 = (To10bitf(vW1.y/fD)<<20) | (To10bitf(vW1.x/fD)<<10) | To10bitf(vW0.w/fD);
	pS->uMPal0 = (To10biti(v.bone_mats0[2])<<20) | (To10biti(v.bone_mats0[1])<<10) | To10biti(v.bone_mats0[0]);
	pS->uMPal1 = (To10biti(v.bone_mats1[1])<<20) | (To10biti(v.bone_mats1[0])<<10) | To10biti(v.bone_mats0[3]);

/*
	for(int i=0; i<4; i++)
	{
		pS->bone_mats0[i]=v.bone_mats0[i];
		pS->bone_mats1[i]=v.bone_mats1[i];
	}*/
}

bool CYif2GpuReader::HasPatches(CYifMesh &mesh) const
{
	if(!m_bTessEnabled)
		return false;
	else
	{
		bool bRes = false;
		for(int s=0; s<mesh.GetNumSurfaces(); s++)
		{
			CYifSurface &surf = *mesh.GetSurfRef(s);
			const bool bGotPatches = (surf.GetNrRegularPatches()+surf.GetNrIrregularPatches())>0;
			bRes |= bGotPatches;
		}
		return bRes;
	}
}

void CYif2GpuReader::PrepStreams()
{
	for(int m=0; m<GetNumMeshes(); m++)
		SetupStreams(&m_pGpuMeshes[m], *GetMeshRef(m));
	m_bNoUpdateYet = true;
}

void CYif2GpuReader::SetupStreams(SGPUMesh * pGPUMesh, CYifMesh &yifmesh)
{
	/////////////////////////
	// if any surface is found on this mesh containing patch primitives
	// then normals and tangents are removed from the vertex stream
	const bool bFoundPatches = HasPatches(yifmesh);

	const bool bGotBones = m_bAnimEnabled && yifmesh.GetNumBones()>0 && yifmesh.GetSkinVertices()!=NULL;
	const bool bIsSinglePass = m_bIsSinglePass || (!bGotBones);

	// watch out for two streams mode
	const bool bTwoStreamsMode = !bIsSinglePass || bFoundPatches;


	Framework::SimpleMesh &sime = pGPUMesh->simple;

	if(bFoundPatches) sime.m_vertexStride = sizeof(Vector3Unaligned);
	// if this is dual pass then sizeof(SSharedAttribs) becomes the stride of the secondary buffer
	else sime.m_vertexStride = sizeof(SMainGpuVertex) + ((!bTwoStreamsMode) ? sizeof(SSharedAttribs) : 0);
	if(bGotBones && bIsSinglePass)
		sime.m_vertexStride += sizeof(SSkinInfo);



	// position buffer and skinning weights if these are in use
	void* pVoidPtrVertsSepStream  =  (void *) (((unsigned char *) sime.m_vertexBuffer) + ((sime.m_vertexCount * sizeof(SSharedAttribs)+1023) & (~1023)));

	// gpu pointer to position stream
	uint8_t* uPosBuffer = static_cast<uint8_t*>(bTwoStreamsMode ? pVoidPtrVertsSepStream : sime.m_vertexBuffer);

	// 5 attributes to a SYifVertex
	// 4 attributes to a SYifSkinVertInfo
	sime.m_vertexAttributeCount = 1;
	uint32_t uPos_offs = bFoundPatches ? ATTR_OFFS(SPatchesSkinnedVertex, vPos) : ATTR_OFFS(SSinglePassSkinnedGpuVertex, v.vPos);
	sime.m_buffer[0].initAsVertexBuffer((void*)(uPosBuffer+uPos_offs), Gnm::kDataFormatR32G32B32Float, sime.m_vertexStride, sime.m_vertexCount);
	sime.m_buffer[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's OK to be read only.
	// only regular geometry needs tangent and normal
	const bool bIsSkinInfoInVertStream = bGotBones && bIsSinglePass;
	if(bIsSkinInfoInVertStream)
	{
		sime.m_vertexAttributeCount += 4;

		uint32_t uWgts0offs = bFoundPatches ? ATTR_OFFS(SPatchesSkinnedVertex, s.uWeights0) : ATTR_OFFS(SSinglePassSkinnedGpuVertex, s.uWeights0);
		uint32_t uWgts1offs = bFoundPatches ? ATTR_OFFS(SPatchesSkinnedVertex, s.uWeights1) : ATTR_OFFS(SSinglePassSkinnedGpuVertex, s.uWeights1);
		uint32_t uMPal0offs = bFoundPatches ? ATTR_OFFS(SPatchesSkinnedVertex, s.uMPal0) : ATTR_OFFS(SSinglePassSkinnedGpuVertex, s.uMPal0);
		uint32_t uMPal1offs = bFoundPatches ? ATTR_OFFS(SPatchesSkinnedVertex, s.uMPal1) : ATTR_OFFS(SSinglePassSkinnedGpuVertex, s.uMPal1);

		sime.m_buffer[1].initAsVertexBuffer((void*)(uPosBuffer+uWgts0offs), Gnm::kDataFormatR10G10B10A2Unorm, sime.m_vertexStride, sime.m_vertexCount);
		sime.m_buffer[2].initAsVertexBuffer((void*)(uPosBuffer+uWgts1offs), Gnm::kDataFormatR10G10B10A2Unorm, sime.m_vertexStride, sime.m_vertexCount);
		sime.m_buffer[3].initAsVertexBuffer((void*)(uPosBuffer+uMPal0offs), Gnm::kDataFormatR10G10B10A2Uint, sime.m_vertexStride, sime.m_vertexCount);
		sime.m_buffer[4].initAsVertexBuffer((void*)(uPosBuffer+uMPal1offs), Gnm::kDataFormatR10G10B10A2Uint, sime.m_vertexStride, sime.m_vertexCount);

		sime.m_buffer[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's OK to be read only.
		sime.m_buffer[2].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's OK to be read only.
		sime.m_buffer[3].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's OK to be read only.
		sime.m_buffer[4].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's OK to be read only.
	}

	if(!bFoundPatches)
	{
		int off = sime.m_vertexAttributeCount;
		sime.m_vertexAttributeCount += 3;
		sime.m_buffer[0+off].initAsVertexBuffer((void*)(uPosBuffer+ATTR_OFFS(SMainGpuVertex, vNorm)), Gnm::kDataFormatR32G32B32Float, sime.m_vertexStride, sime.m_vertexCount);
		sime.m_buffer[1+off].initAsVertexBuffer((void*)(uPosBuffer+ATTR_OFFS(SMainGpuVertex, vTangent)), Gnm::kDataFormatR32G32B32A32Float, sime.m_vertexStride, sime.m_vertexCount);

		sime.m_buffer[0+off].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's OK to be read only.
		sime.m_buffer[1+off].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's OK to be read only.

		const uint32_t uSharedAttribsOffs = bIsSinglePass ? ( sizeof(SMainGpuVertex) + ATTR_OFFS(SSharedAttribs, s)) : 0;
		const uint32_t uAttribsStride = bTwoStreamsMode ? sizeof(SSharedAttribs) : sime.m_vertexStride;
		sime.m_buffer[2+off].initAsVertexBuffer(static_cast<uint8_t*>(sime.m_vertexBuffer) + uSharedAttribsOffs, Gnm::kDataFormatR32G32Float, uAttribsStride, sime.m_vertexCount);

		sime.m_buffer[2+off].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so it's OK to be read only.
	}

	// patches and compute based skinning
	if(bTwoStreamsMode)
	{
		assert(bGotBones || bFoundPatches);

		// separate buffer for the texture coordinates
		SSharedAttribs * pDualAttribs= (SSharedAttribs *) sime.m_vertexBuffer;

		// setup texture coordinates in a separate buffer
		for(uint32_t i=0; i<sime.m_vertexCount; i++)
			SetAttribs(&pDualAttribs[i], yifmesh.GetVertices()[i]);

		//
		if(bFoundPatches)
		{
			if(!bIsSinglePass)
			{
				// if skinning is done in compute then only the position is read in the vertex shader
				assert(bGotBones);
				sime.m_vertexAttributeCount = 1;
				assert(sime.m_vertexStride == sizeof(Vector3Unaligned));
			}

			if(bGotBones)
			{
				SPatchesSkinnedVertex * pDualVerts  =  (SPatchesSkinnedVertex *) pVoidPtrVertsSepStream;

				for(uint32_t i=0; i<sime.m_vertexCount; i++)
				{
					pDualVerts[i].vPos = yifmesh.GetVertices()[i].vPos;
					SetSkinInfo(&pDualVerts[i].s, yifmesh.GetSkinVertices()[i]);
				}
			}
			else
			{
				for(uint32_t i=0; i<sime.m_vertexCount; i++)
					((Vector3Unaligned *) pVoidPtrVertsSepStream)[i] = yifmesh.GetVertices()[i].vPos;
			}
		}
		else
		{
			// skinning in compute (regular geometry)
			SDualPassSkinnedComputeVertexIn * pDualVerts  =  (SDualPassSkinnedComputeVertexIn *) pVoidPtrVertsSepStream;

			for(uint32_t i=0; i<sime.m_vertexCount; i++)
			{
				SetMainVert(&pDualVerts[i].v, yifmesh.GetVertices()[i]);
				SetSkinInfo(&pDualVerts[i].s, yifmesh.GetSkinVertices()[i]);
			}
		}
	}
	// regular non patch based geometry (and if skinning is enabled this is done in the vertex shader).
	else
	{
		const int iSingleStride = bGotBones ? sizeof(SSinglePassSkinnedGpuVertex) : (sizeof(SMainGpuVertex)+sizeof(SSharedAttribs));

		for(uint32_t i=0; i<sime.m_vertexCount; i++)
		{
			if(bGotBones)
			{
				SSinglePassSkinnedGpuVertex &vert = static_cast<SSinglePassSkinnedGpuVertex *>(static_cast<void *>(static_cast<unsigned char *>(sime.m_vertexBuffer)+i*iSingleStride))[0];
				SetMainVert(&vert.v, yifmesh.GetVertices()[i]);
				SetAttribs(&vert.a, yifmesh.GetVertices()[i]);
				SetSkinInfo(&vert.s, yifmesh.GetSkinVertices()[i]);
			}
			else
			{
				SMainGpuVertex &vert = static_cast<SMainGpuVertex *>(static_cast<void *>(static_cast<unsigned char *>(sime.m_vertexBuffer)+i*iSingleStride))[0];
				SSharedAttribs &attrb = static_cast<SSharedAttribs *>(static_cast<void *>(static_cast<unsigned char *>(sime.m_vertexBuffer)+i*iSingleStride + sizeof(SMainGpuVertex)))[0];
				SetMainVert(&vert, yifmesh.GetVertices()[i]);
				SetAttribs(&attrb, yifmesh.GetVertices()[i]);
			}
		}
	}
}



void CYif2GpuReader::Mesh2GPU(Gnmx::Toolkit::Allocators *allocators, SGPUMesh * pGPUMesh, CYifMesh &yifmesh)
{
	int iMaxIndex = -1;
	for(int i=0; i<yifmesh.GetNumIndices(); i++)
		iMaxIndex = Max(iMaxIndex, yifmesh.GetIndices()[i]);
	const int iNrVertices = iMaxIndex+1;
	Framework::SimpleMesh &sime = pGPUMesh->simple;

	sime.m_vertexCount = iNrVertices;
	//const int max_attribs = 9;
	//const uint32_t fetchBufferSize = Gnm::GetFetchShaderRequiredBufferSize(max_attribs);
	//Toolkit::allocateMemory(&sime.m_fetchShader, fetchBufferSize, 256);

	const bool bIndicesAsShorts = sime.m_vertexCount < 0xffff;

	sime.m_indexCount = yifmesh.GetNumIndices();
	sime.m_indexType = bIndicesAsShorts ? Gnm::kIndexSize16 : Gnm::kIndexSize32;

	const int iSizeIndex = bIndicesAsShorts ? sizeof(unsigned short) : sizeof(unsigned int);
	sime.m_indexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(sime.m_indexCount * iSizeIndex, Gnm::kAlignmentOfBufferInBytes));

	if(bIndicesAsShorts)
		for(uint32_t i=0; i<sime.m_indexCount; i++) ((unsigned short *) sime.m_indexBuffer)[i]=(unsigned short) yifmesh.GetIndices()[i];
	else
		for(uint32_t i=0; i<sime.m_indexCount; i++) ((unsigned int *) sime.m_indexBuffer)[i]=(unsigned int) yifmesh.GetIndices()[i];

	// build vertex stream (+64 to align second buffer for dual pass)
	sime.m_vertexBuffer = static_cast<uint8_t*>(allocators->m_garlic.allocate(sime.m_vertexCount * sizeof(SSinglePassSkinnedGpuVertex)+1024, Gnm::kAlignmentOfBufferInBytes));
	sime.m_primitiveType = GetGpuPrimType(yifmesh, 0);

	//const int iNrBones = yifmesh.GetNumBones();
	SetupStreams(pGPUMesh, yifmesh);


	// setup patches index buffer
	int iNumRegIndicesAlloc = 0;
	int iNumIrregIndicesAlloc = 0;
	const int * piPatchIndices = yifmesh.GetPatchIndices();
	int nr_irreg = 0, nr_reg = 0;
	for(int s=0; s<yifmesh.GetNumSurfaces(); s++)
	{
		CYifSurface &surf = *yifmesh.GetSurfRef(s);
		iNumRegIndicesAlloc += surf.GetNrRegularPatches()*16;
		iNumIrregIndicesAlloc += surf.GetNrIrregularPatches()*32;

		nr_reg += (surf.GetNrRegularPatches()>0?1:0);
		nr_irreg += (surf.GetNrIrregularPatches()>0?1:0);
	}

	if((iNumRegIndicesAlloc+iNumIrregIndicesAlloc)>0)
	{
		pGPUMesh->m_iNrPreValBuffers = nr_irreg;
		pGPUMesh->m_pPreValBuffers = new MemBlock[nr_irreg];
		pGPUMesh->m_patchIndexBuffer = static_cast<uint32_t*>(allocators->m_garlic.allocate((iNumRegIndicesAlloc+iNumIrregIndicesAlloc) * iSizeIndex, Gnm::kAlignmentOfBufferInBytes));
		int index = 0, preval_index = 0, hsvert_indices_index = 0;
		for(int s=0; s<yifmesh.GetNumSurfaces(); s++)
		{
			CYifSurface &surf = *yifmesh.GetSurfRef(s);

			if(surf.GetNrRegularPatches()>0)
			{
				for(int p=0; p<surf.GetNrRegularPatches(); p++)
				{
					if(bIndicesAsShorts)
					{
						for(int k=0; k<16; k++)
						{
							const short data_index = (short) piPatchIndices[surf.GetRegularPatchIndicesOffset()+p*16+k];
							((short *) pGPUMesh->m_patchIndexBuffer)[index++] = data_index;

							//verts[k]=yifmesh.GetVertices()[piPatchIndices[surf.GetRegularPatchIndicesOffset()+p*16+k]].vPos;
						}
					}
					else
						for(int k=0; k<16; k++)
						{
							const int data_index = (int) piPatchIndices[surf.GetRegularPatchIndicesOffset()+p*16+k];
							((int *) pGPUMesh->m_patchIndexBuffer)[index++] = data_index;
						}

					//unsigned int uPref = yifmesh.GetPatchPrefixes()[surf.GetRegularPatchOffset()+p];
					//unsigned int uVal = yifmesh.GetPatchValences()[surf.GetRegularPatchOffset()+p];
					//((unsigned int *) pGPUMesh->m_pPreValBuffers[preval_index].m_ptr)[2*p+0]=uPref;
					//((unsigned int *) pGPUMesh->m_pPreValBuffers[preval_index].m_ptr)[2*p+1]=uVal;
				}

				//++preval_index;
				++hsvert_indices_index;
			}

			if(surf.GetNrIrregularPatches()>0)
			{
				pGPUMesh->m_pPreValBuffers[preval_index] = static_cast<uint32_t*>(allocators->m_garlic.allocate(surf.GetNrIrregularPatches()*sizeof(unsigned int)*2, Gnm::kAlignmentOfBufferInBytes));


				for(int p=0; p<surf.GetNrIrregularPatches(); p++)
				{
					if(bIndicesAsShorts)
						for(int k=0; k<32; k++)
						{
							const short data_index = (short) piPatchIndices[surf.GetIrregularPatchIndicesOffset()+p*32+k];
							((short *) pGPUMesh->m_patchIndexBuffer)[index++] = data_index;
						}
					else
						for(int k=0; k<32; k++)
						{
							const int data_index = (int) piPatchIndices[surf.GetIrregularPatchIndicesOffset()+p*32+k];
							((int *) pGPUMesh->m_patchIndexBuffer)[index++] = data_index;

						}
					unsigned int uPref = yifmesh.GetPatchPrefixes()[surf.GetIrregularPatchOffset()+p];
					unsigned int uVal = yifmesh.GetPatchValences()[surf.GetIrregularPatchOffset()+p];
					((unsigned int *) pGPUMesh->m_pPreValBuffers[preval_index])[2*p+0]=uPref;
					((unsigned int *) pGPUMesh->m_pPreValBuffers[preval_index])[2*p+1]=uVal;
				}

				++preval_index;
				++hsvert_indices_index;
			}

		}
	}

}

// will replace this with a look-up table later
Gnm::PrimitiveType CYif2GpuReader::GetGpuPrimType(CYifMesh &yifmesh, const int iSurf) const
{
	Gnm::PrimitiveType res = Gnm::kPrimitiveTypeTriList;
	CYifSurface &surf = *yifmesh.GetSurfRef(iSurf);

	switch(surf.GetPrimType())
	{
		case YIF_QUADS:
			res = Gnm::kPrimitiveTypeQuadList;
			break;
		case YIF_TRIANGLES:
			res = Gnm::kPrimitiveTypeTriList;
			break;
		case YIF_FANS:
			res = Gnm::kPrimitiveTypeTriFan;
			break;
		case YIF_STRIPS:
			res = Gnm::kPrimitiveTypeTriStrip;
			break;
		case YIF_LINES:
			res = Gnm::kPrimitiveTypeLineList;
			break;
		case YIF_CONNECTED_LINES:
			res = Gnm::kPrimitiveTypeLineStrip;
			break;
	}

	return res;
}

int CYif2GpuReader::GetNrIndices(CYifMesh &yifmesh, const int iSurf) const
{
	CYifSurface &surf = *yifmesh.GetSurfRef(iSurf);
	int iNrIndices;

	switch(surf.GetPrimType())
	{
		case YIF_QUADS:
			iNrIndices = 4*surf.GetNumPrims();
			break;
		case YIF_FANS:
		case YIF_STRIPS:
		case YIF_CONNECTED_LINES:
		{
			iNrIndices = 0;
			for(int p=0; p<surf.GetNumPrims(); p++)
				iNrIndices += surf.GetNumVertsOnPrim(p);
		}
		break;
		case YIF_LINES:
			iNrIndices = 2*surf.GetNumPrims();
			break;
		default:
			iNrIndices = 3*surf.GetNumPrims();
	}

	return iNrIndices;
}

void CYif2GpuReader::PrepHullShaders()
{
	bool bGotBonesGlobal = false;
	int m=0;
	while(m<GetNumMeshes() && !bGotBonesGlobal)
	{
		CYifMesh &mesh = *GetMeshRef(m);
		bGotBonesGlobal |= (mesh.GetNumBones()>0 && mesh.GetSkinVertices()!=NULL);
		if(!bGotBonesGlobal) ++m;
	}
	bGotBonesGlobal &= m_bAnimEnabled;

	// find minimum patch size (in LDS) to be used.
	int index = (m_bReadVertsInHS ? 2 : 0) + (m_bGenTangsInDS ? 0 : 1);

	const int reg_index = m_bReadVertsInHS ? 1 : 0;
	const int nr_threads_index = m_bIsFourThreadsPerRegPatch ? 0 : 2;
	Gnmx::HsShader * hsb_irreg_noskin = m_bIsGregoryPatches ? m_hsbGreg_patch[reg_index] : m_hsbirreg_patch[index];
	Gnmx::HsShader * hsb_irreg_skin = m_bIsGregoryPatches ? m_hsbGreg_patch[0] : m_hsbirreg_patch[(m_bGenTangsInDS ? 0 : 1)];

	Gnmx::HsShader * hsb_reg_noskin = m_hsbreg_patch[nr_threads_index+reg_index];
	Gnmx::HsShader * hsb_reg_skin = m_hsbreg_patch[nr_threads_index+0];

	Gnmx::LsShader * lsb_noskin = m_bReadVertsInHS ? m_lsb_hsvert_noskin : m_lsb_noskin;
	Gnmx::LsShader * lsb_skin = m_lsb_skin;

	const int iIrregHSPatchsize_noskin = GetPatchSize(hsb_irreg_noskin, lsb_noskin->m_lsStride);
	const int iRegHSPatchsize_noskin = GetPatchSize(hsb_reg_noskin, lsb_noskin->m_lsStride);

	const int iIrregHSPatchsize_skin = GetPatchSize(hsb_irreg_skin, lsb_skin->m_lsStride);
	const int iRegHSPatchsize_skin = GetPatchSize(hsb_reg_skin, lsb_skin->m_lsStride);

	// report either skinned or non skinned numPatches
	const bool bGotSomeBones = m_bIsSinglePass && bGotBonesGlobal;

	// find hull shader with min. patch size.
	Gnmx::HsShader * hsb_min;
	int iMinPatchSize;
	if(!bGotSomeBones)
	{
		if(iRegHSPatchsize_noskin<iIrregHSPatchsize_noskin)
		{
			iMinPatchSize = iRegHSPatchsize_noskin;
			hsb_min = hsb_reg_noskin;
		}
		else
		{
			iMinPatchSize = iIrregHSPatchsize_noskin;
			hsb_min = hsb_irreg_noskin;
		}
	}
	else
	{
		if(iRegHSPatchsize_skin<iIrregHSPatchsize_skin)
		{
			iMinPatchSize = iRegHSPatchsize_skin;
			hsb_min = hsb_reg_skin;
		}
		else
		{
			iMinPatchSize = iIrregHSPatchsize_skin;
			hsb_min = hsb_irreg_skin;
		}
	}

	int num_shaders = bGotSomeBones ? 4 : 2;
	Gnmx::HsShader * hsb_shaders[4] = {hsb_reg_noskin, hsb_irreg_noskin, hsb_reg_skin, hsb_irreg_skin};

	bool bFoundMinHs = false;
	int h = 0;
	while(h<num_shaders && !bFoundMinHs)
	{
		bFoundMinHs = hsb_shaders[h]==hsb_min;
		if(!bFoundMinHs) ++h;
	}

	//int nextPatchesOffset = m_iNrPatchesOffset;
	for(int s2=0; s2<num_shaders; s2++)
	{
		const int mask = num_shaders-1;
		const int s = (s2+h)&mask;	// start from min entry
		Gnmx::LsShader * localShader = s<2 ? lsb_noskin : lsb_skin;
		Gnmx::HsShader * hullShader = hsb_shaders[s];
		uint32_t iCurPatchSize = GetPatchSize(hullShader, localShader->m_lsStride);

		uint32_t hs_vgpr_limit=hullShader->m_hsStageRegisters.getNumVgprs();
		uint32_t lds_size=0x1000;	// 4kB
		//Gnm::TessellationDataConstantBuffer tessConstants;

		// find max nr_patches per threadgroup
		uint32_t numPatches = computeNumPatches(&hullShader->m_hsStageRegisters, &hullShader->m_hullStateConstants, localShader->m_lsStride, hs_vgpr_limit, lds_size);

		if(numPatches>16) { numPatches=16; lds_size = numPatches * iCurPatchSize; }

		if(m_iNrPatchesOffset!=0)
		{
			const uint32_t iMaxLDS = 4*0x1000;		// 16 kB (actual maximum is 32KB per threadgroup)
			lds_size += (m_iNrPatchesOffset*iMinPatchSize);
			lds_size = lds_size<iCurPatchSize ? iCurPatchSize : (lds_size>iMaxLDS ? iMaxLDS : lds_size);
			int next_nr_patches = computeNumPatches(&hullShader->m_hsStageRegisters, &hullShader->m_hullStateConstants, localShader->m_lsStride, hs_vgpr_limit, lds_size);
			// update offset based on regular patches
			if(s2==0)	// we set minhsb to first entry
			{
				SCE_GNM_ASSERT(hsb_min == hullShader);
				m_iNrPatchesOffset = next_nr_patches - numPatches;
			}
			numPatches = next_nr_patches;
		}

		// THIS CRASH, only happens on A0 cards so I am disabling this clamp on numPatches.
		// we start getting crashes when numPatches>=16.
		// For instance TFs all 8.0 on a quad domain, integer tess and with numPatches==16
		// triggers a crash in every example we've tried so far.
		//if(numPatches>=16) numPatches=15;

		uint32_t vgt_prims;
		// We don't call computeVgtPrimitiveAndPatchCounts() here, because the whole point of this code is to experiment with different patch counts.
		// instead, we inline the relevant logic to compute an appropriate value for vgt_prims, and adjust numPatches as necessary.

		// Determine how many patches to send through each VGT (no more than 256 vertices is recommended)
		vgt_prims = 256 / hullShader->m_hullStateConstants.m_numInputCP;

		// Make sure the number of primitives we pass through each VGT is a multiple
		// of the patch count per HS threadgroup, to avoid partially filled threadgroups.
		if(vgt_prims > numPatches)
		{
			vgt_prims -= (vgt_prims % numPatches); // closest multiple
		}
		else
		{
			numPatches = vgt_prims; // if numPatches>vgtPrimCount, reduse LDS consumption by adjusting patch count
		}

		// record values
		m_params_HsTg[s].iVgtPrims = vgt_prims;
		m_params_HsTg[s].iNumPatches = numPatches;
		m_params_HsTg[s].hullShader = hullShader;
		if(s<2)
		{
			m_pLsbShaders[s&1] = (s&1)!=0 ? lsb_skin : lsb_noskin;
			m_pLsbFetchShaders[s&1] = (s&1)!=0 ? &m_lsb_skin_fetch : &m_lsb_noskin_fetch;
		}

		Gnm::TessellationDataConstantBuffer tessConstants;
		tessConstants.init(&hullShader->m_hullStateConstants, localShader->m_lsStride, numPatches, 512.f);
		memcpy(m_params_HsTg[s].tessCnstMemBlock, &tessConstants, sizeof(tessConstants));
	}

	// report either skinned or non skinned numPatches
	const int offs = bGotSomeBones ? 2 : 0;
	m_iNrRegPatches = m_params_HsTg[0+offs].iNumPatches;
	m_iNrIrregPatches = m_params_HsTg[1+offs].iNumPatches;
}

void CYif2GpuReader::DrawScene(Gnmx::GnmxGfxContext &gfxc, cbExtraInst *constantBuffer, int iNrRepeats)
{
	const int iMeshInsts = GetNumMeshInstances();

	// let's start fresh
	Framework::waitForGraphicsWrites(&gfxc);

	// bones must have been updated at least once.
	if(!m_bNoUpdateYet)
	{
		// prep hull shaders
		PrepHullShaders();

		// not using actual hw instancing here
		for(int i=0; i<iNrRepeats; i++)
		{
			cbExtraInst * cnstBuf = constantBuffer + i; 
			cnstBuf[0].g_fScale = GEN_REP_UNI_SCALE(iNrRepeats);
			cnstBuf[0].g_fOffs = GEN_REP_X_OFFS(iNrRepeats);
			
			Gnm::Buffer sConstBuffer;
			sConstBuffer.initAsConstantBuffer( cnstBuf, sizeof(cbExtraInst) );
			sConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

			// set replica scale+add
			gfxc.setConstantBuffers(Gnm::kShaderStageVs, 1, 1, &sConstBuffer);
			gfxc.setConstantBuffers(Gnm::kShaderStageLs, 0, 1, &sConstBuffer);
			gfxc.setConstantBuffers(Gnm::kShaderStageHs, 1, 1, &sConstBuffer);

			for(int m=0; m<iMeshInsts; m++)
			{
				CYifMeshInstance &meshinst = *GetMeshInstanceRef(m);
				const int mesh_index = meshinst.GetMeshIndex();
				CYifMesh &mesh = *GetMeshRef(mesh_index);

				gfxc.setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &m_psInstCbuf[m].m_sInstData);
				gfxc.setConstantBuffers(Gnm::kShaderStageHs, 0, 1, &m_psInstCbuf[m].m_sInstData);

				const bool bGotBones = m_bAnimEnabled && mesh.GetNumBones()>0 && mesh.GetSkinVertices()!=NULL;
				if(bGotBones)
				{
					const bool bIsSinglePass = m_bIsSinglePass || (!bGotBones);
					//const int iCBSize = mesh.GetNumBones()*sizeof(Matrix4);
					if(!bIsSinglePass)
						gfxc.setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &m_psInstCbuf[m].m_sBonesPalette);
					else
					{
						gfxc.setConstantBuffers(Gnm::kShaderStageVs, 2, 1, &m_psInstCbuf[m].m_sBonesPalette);
						gfxc.setConstantBuffers(Gnm::kShaderStageLs, 1, 1, &m_psInstCbuf[m].m_sBonesPalette);
					}
				}


				// swap skinned vertices buffers
				if(m_uPtrGPUskinningCur==m_uPtrGPUskinning1) m_uPtrGPUskinningCur=m_uPtrGPUskinning2;
				else m_uPtrGPUskinningCur=m_uPtrGPUskinning1;

				DrawMesh(gfxc, mesh_index);
			}
		}
	}

	if(m_psInstCbuf == m_psInstCbuf1) m_psInstCbuf = m_psInstCbuf2;
	else  m_psInstCbuf = m_psInstCbuf1;
}

int CYif2GpuReader::GetPatchSize(const Gnmx::HsShader * pHS_shader, const int ls_stride)
{
	int patchTotalSizeInBytes = pHS_shader->m_hullStateConstants.m_numInputCP * ls_stride +
															 pHS_shader->m_hullStateConstants.m_numOutputCP * pHS_shader->m_hullStateConstants.m_cpStride +
															 pHS_shader->m_hullStateConstants.m_numPatchConst*16;
	return patchTotalSizeInBytes;
}

#define RENDERED_PATCHES		1
#define RENDERED_GEOMETRY		2

void CYif2GpuReader::DrawMesh(Gnmx::GnmxGfxContext &gfxc, const int mesh_index)
{
#if SCE_GNMX_ENABLE_GFX_LCUE	// patches-sample does not support the LCUE
	SCE_GNM_UNUSED(gfxc);
	SCE_GNM_UNUSED(mesh_index);
#else
	CYifMesh &mesh = *GetMeshRef(mesh_index);
	const bool bGotBones = m_bAnimEnabled && mesh.GetNumBones()>0 && mesh.GetSkinVertices()!=NULL;
	const bool bIsSinglePass = m_bIsSinglePass || (!bGotBones);
	Framework::SimpleMesh &sime = m_pGpuMeshes[mesh_index].simple;

	// check if we have patches on this mesh
	bool bFoundPatches = HasPatches(mesh);

	void * pVoidPtrVertsSepStream  =  (void *) (((unsigned char *) sime.m_vertexBuffer) + ((sime.m_vertexCount * sizeof(SSharedAttribs)+1023) & (~1023)));

	if(bIsSinglePass)
	{
		gfxc.setVertexBuffers(Gnm::kShaderStageVs, 0, sime.m_vertexAttributeCount, sime.m_buffer);
		gfxc.setVertexBuffers(Gnm::kShaderStageLs, 0, sime.m_vertexAttributeCount, sime.m_buffer);
	}
	else				// dual pass
	{
		const int iNrVertices = sime.m_vertexCount;
		const int iBSizeVert = bFoundPatches ? sizeof(SPatchesSkinnedVertex) : sizeof(SDualPassSkinnedComputeVertexIn);
		Gnm::Buffer verts_buf;
		verts_buf.initAsRegularBuffer(pVoidPtrVertsSepStream, iBSizeVert, iNrVertices);
		verts_buf.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never bind as RWBuffer, so read-only is OK
		gfxc.setBuffers(Gnm::kShaderStageCs, 0, 1, &verts_buf);

		//m_prwbuffer
		const int iBSizeVert_out = bFoundPatches ? sizeof(Vector3Unaligned) : sizeof(SMainGpuVertex);
		Gnm::Buffer verts_buf_out;
		if(!bFoundPatches)
			verts_buf_out.initAsRegularBuffer(m_uPtrGPUskinningCur, iBSizeVert_out, iNrVertices);
		else
			verts_buf_out.initAsDataBuffer(m_uPtrGPUskinningCur, Gnm::kDataFormatR32G32B32Float, iNrVertices);
		verts_buf_out.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we bind as RWBuffer, so it's GPU-coherent

		gfxc.setRwBuffers(Gnm::kShaderStageCs, 0, 1, &verts_buf_out);

		gfxc.setCsShader(bFoundPatches ? m_csb_skinCC : m_csb_skin);

		gfxc.dispatch((iNrVertices+63)/64, 1, 1);

		//gfxc.waitForGraphicsWrites((uint32_t) (m_uPtrGPUskinningCur>>8), (iBSizeVert_out*iNrVertices+255)>>8, Gnm::kCoherTcActionEnable);
		Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc.m_dcb);


		// use output from compute
		Gnm::Buffer verts_bufs[4];
		uint32_t uOffs = bFoundPatches ? 0 : ATTR_OFFS(SMainGpuVertex, vPos);
		uint32_t uStride = bFoundPatches ? sizeof(Vector3Unaligned) : sizeof(SMainGpuVertex);
		verts_bufs[0].initAsVertexBuffer((void*)((uint8_t*)m_uPtrGPUskinningCur+uOffs), Gnm::kDataFormatR32G32B32Float, uStride, sime.m_vertexCount);
		verts_bufs[1].initAsVertexBuffer((void*)((uint8_t*)m_uPtrGPUskinningCur+ATTR_OFFS(SMainGpuVertex, vNorm)), Gnm::kDataFormatR32G32B32Float, sizeof(SMainGpuVertex), sime.m_vertexCount);
		verts_bufs[2].initAsVertexBuffer((void*)((uint8_t*)m_uPtrGPUskinningCur+ATTR_OFFS(SMainGpuVertex, vTangent)), Gnm::kDataFormatR32G32B32A32Float, sizeof(SMainGpuVertex), sime.m_vertexCount);
		verts_bufs[3].initAsVertexBuffer(sime.m_vertexBuffer, Gnm::kDataFormatR32G32Float, sizeof(SSharedAttribs), sime.m_vertexCount);

		verts_bufs[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
		verts_bufs[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
		verts_bufs[2].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK
		verts_bufs[3].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a vertex buffer, so read-only is OK

		gfxc.setVertexBuffers(Gnm::kShaderStageVs, 0, bFoundPatches ? 1 : 4, verts_bufs);
		gfxc.setVertexBuffers(Gnm::kShaderStageLs, 0, 1, verts_bufs);

		// in case of bReadVertsInHS==true
		pVoidPtrVertsSepStream = m_uPtrGPUskinningCur;
	}

	// prepare for rendering
	const bool bUseSkinningVShader = m_bIsSinglePass && bGotBones;
	const bool bReadVertsInHS = m_bReadVertsInHS && (!bUseSkinningVShader);
	const bool bGenTangsInDS = m_bGenTangsInDS;

	gfxc.setIndexSize(sime.m_indexType);

	Gnm::Buffer buffers[4];
	if(bFoundPatches)
	{
		buffers[0].initAsDataBuffer(sime.m_vertexBuffer, Gnm::kDataFormatR32G32Float, sime.m_vertexCount);	// texture coordinates
		buffers[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's never bound as RWBuffer, so read-only is OK
		if(bReadVertsInHS)
		{
			buffers[1].initAsDataBuffer(pVoidPtrVertsSepStream, Gnm::kDataFormatR32G32B32Float, sime.m_vertexCount);
			buffers[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's never bound as RWBuffer, so read-only is OK
		}
	}


	int preval_index = 0, hsvert_ibuffer_index = 0;
	int iRenderMode = 0;
	int iPatchesIndexOffset = 0;
	for(int s=0; s<mesh.GetNumSurfaces(); s++)
	{
		CYifSurface &surf = *mesh.GetSurfRef(s);

		// setup material
		const bool bGotPatches = m_bTessEnabled ? (surf.GetNrRegularPatches()+surf.GetNrIrregularPatches())>0 : false;
		if(bGotPatches)
		{
			Gnmx::LsShader * localShader = m_pLsbShaders[bUseSkinningVShader ? 1 : 0];
			MemBlock * fetchShader = bReadVertsInHS ? 0 : m_pLsbFetchShaders[bUseSkinningVShader ? 1 : 0];

			if((iRenderMode&RENDERED_PATCHES)==0)
			{
				// reset states if previous patch was not tessellation
				if(!bReadVertsInHS)
					gfxc.setIndexBuffer(m_pGpuMeshes[mesh_index].m_patchIndexBuffer);

				gfxc.setActiveShaderStages(Gnm::kActiveShaderStagesLsHsVsPs);
				gfxc.setPrimitiveType(Gnm::kPrimitiveTypePatch);


				iRenderMode = RENDERED_PATCHES;
			}

			int piNrPatches[] = {surf.GetNrRegularPatches(), surf.GetNrIrregularPatches()};


			// k==0 for regular patches and k==1 for irregular patches
			for(int k=0; k<2; k++)
			{
				const int iNrPatches = piNrPatches[k];
				if(iNrPatches>0)
				{
					// set vertex shader
					const int index = (bUseSkinningVShader ? 2 : 0) + k;
					Gnmx::HsShader * hullShader = m_params_HsTg[index].hullShader;			// hull shader
					int numPatches = m_params_HsTg[index].iNumPatches;			// number of patches per tg
					int vgt_prims = m_params_HsTg[index].iVgtPrims;				// number of patches per vgt

					// set number of patches per vgt
					uint8_t numPatchesMinus1 = (uint8_t)(vgt_prims-1);
					gfxc.setVgtControl(numPatchesMinus1);

					// set internal constant buffer expected by compiler
					MemBlock * pTessblock = &m_params_HsTg[index].tessCnstMemBlock;
					gfxc.setTessellationDataConstantBuffer(*pTessblock, Gnm::kShaderStageVs);

					// set Hull shader
					gfxc.setLsHsShaders(localShader, 0, fetchShader==NULL ? 0 : *fetchShader,
										hullShader, numPatches);
					// set Domain shader
					Gnmx::VsShader * dsb_irreg = m_bIsGregoryPatches ? m_dsb_greg_patch : (bGenTangsInDS ? m_dsbirreg_dt_patch : m_dsbirreg_ht_patch);
					Gnmx::VsShader * dsb_reg = m_bIsFourThreadsPerRegPatch ? m_dsb_bez_patch : m_dsb_bez_patch_16threads;
					gfxc.setVsShader(k==0 ? dsb_reg : dsb_irreg, 0, (void*)0);

					//
					const int iNrVertsOnPatch = (k+1)*16;
					const int iNrIndices = iNrVertsOnPatch*iNrPatches;

					// if the vertices are read in the hull shader then we need to set the index buffer to a buffer slot
					if(bReadVertsInHS)
					{
						const int iBSizeIndex = sime.m_indexType==Gnm::kIndexSize16 ? 2 : 4;
						void* uGPUIndices = (void*)  (((unsigned char *) m_pGpuMeshes[mesh_index].m_patchIndexBuffer)+iBSizeIndex*iPatchesIndexOffset);
						buffers[2].initAsDataBuffer(uGPUIndices, sime.m_indexType==Gnm::kIndexSize16 ? Gnm::kDataFormatR16Uint : Gnm::kDataFormatR32Uint, iNrIndices);
						buffers[2].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's never bound as RWBuffer, so read-only is OK
						++hsvert_ibuffer_index;
					}

					// set prefix-valence buffer
					if(k>0)
					{
						const int off = bReadVertsInHS ? 3 : 1;

						buffers[off].initAsDataBuffer(m_pGpuMeshes[mesh_index].m_pPreValBuffers[preval_index], Gnm::kDataFormatR32G32Uint, iNrPatches);
						buffers[off].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's never bound as RWBuffer, so read-only is OK

						//
						++preval_index;
					}

					// set accumulated list of buffers
					const int iNrSlots = 1 + (bReadVertsInHS ? 2 : 0) + (k==1 ? 1 : 0);
					gfxc.setBuffers(Gnm::kShaderStageHs, 0, iNrSlots, buffers);

					gfxc.setPrimitiveType(Gnm::kPrimitiveTypePatch);
					//if(k==1)
					{
						if(!bReadVertsInHS)
							gfxc.drawIndexOffset(iPatchesIndexOffset, iNrIndices);
						else
						{
							gfxc.drawIndexAuto(iNrPatches);
						}
					}

					iPatchesIndexOffset += iNrIndices;
				}
			}
		}
		else		// If rendering non tessellated geometry
		{
			if((iRenderMode&RENDERED_GEOMETRY)==0)
			{
				// reset states if previous patch was tessellation
				gfxc.setIndexBuffer(sime.m_indexBuffer);
				gfxc.setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
				gfxc.setVgtControl(255);

				Gnmx::VsShader * vertexShader = bUseSkinningVShader ? m_vsb_skin : m_vsb_noskin;
				MemBlock * fetchShader = bUseSkinningVShader ? &m_vsb_skin_fetch : &m_vsb_noskin_fetch;
				gfxc.setVsShader(vertexShader, 0, *fetchShader);
				iRenderMode = RENDERED_GEOMETRY;
			}

			// render surface
			const int offset = surf.GetIndexBufferOffset();
			Gnm::PrimitiveType prim_type = GetGpuPrimType(mesh, s);
			gfxc.setPrimitiveType(prim_type);
			const int iNrIndices = GetNrIndices(mesh, s);

			gfxc.drawIndexOffset(offset, iNrIndices);
		}
	}

	if(!bIsSinglePass)
	{
		gfxc.setVertexBuffers(Gnm::kShaderStageVs, 0, 0, 0);
		gfxc.setVertexBuffers(Gnm::kShaderStageLs, 0, 0, 0);
	}

	if((iRenderMode&RENDERED_GEOMETRY)==0)
	{
		gfxc.setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
		gfxc.setVgtControl(255);
	}
#endif // SCE_GNMX_ENABLE_GFX_LCUE 
}

CYif2GpuReader::CYif2GpuReader()
{
	m_pMats=NULL;
	m_pGpuMeshes=NULL;
	m_psInstCbuf=NULL;
	m_psInstCbuf1=NULL;
	m_psInstCbuf2=NULL;
	m_bIsSinglePass=true;
	m_bNoUpdateYet = true;
	m_bTessEnabled = true;
	m_bAnimEnabled = true;

	m_csb_skin=NULL;
	m_vsb_skin=NULL;
	m_vsb_noskin=NULL;

	m_iTessFactor = 1;
	m_bGenTangsInDS = true;
	m_bReadVertsInHS = false;
	m_bIsGregoryPatches = true;
	m_bIsFourThreadsPerRegPatch = true;

	m_iNrRegPatches = 0;
	m_iNrIrregPatches = 0;
	m_iNrPatchesOffset = 0;

	for(int s=0; s<4; s++)
		memset(&m_params_HsTg[s], 0, sizeof(sParamsHsTG));
}

CYif2GpuReader::~CYif2GpuReader()
{

}
