/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "yif.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <gnm.h>

CYifMaterial::CYifMaterial()
{
	m_pMaterialName = NULL;
}

CYifMaterial::~CYifMaterial()
{
}


void CYifSurface::SetPrimType(eYifPrimType eType)
{
	m_ePrimType = eType;
}

void CYifSurface::SetNumPrims(int iNrNumPrims)
{
	m_iNumPrims = iNrNumPrims;
}

// only used for fans and strips
void CYifSurface::SetNumVertsPerPrim(int * piNumVerts)
{
	if(m_piNumVerts!=NULL) { delete [] m_piNumVerts; m_piNumVerts=NULL; }
	m_piNumVerts = piNumVerts;
}

void CYifSurface::SetIndexBufferOffset(int iOffset)
{
	m_iOffset = iOffset;
}

eYifPrimType CYifSurface::GetPrimType() const
{
	return m_ePrimType;
}

int CYifSurface::GetNumPrims() const
{
	return m_iNumPrims;
}

const int * CYifSurface::GetNumVertsBuffer() const
{
	bool bRequiresBuffer = GetPrimType()==YIF_FANS || GetPrimType()==YIF_STRIPS || GetPrimType()==YIF_CONNECTED_LINES;
	SCE_GNM_ASSERT((m_piNumVerts!=NULL && bRequiresBuffer) || (m_piNumVerts==NULL && !bRequiresBuffer));
	return m_piNumVerts;
}

int CYifSurface::GetNumVertsOnPrim(const int index) const
{
	int iRes = 0;
	switch( GetPrimType() )
	{
		case YIF_FANS:
		case YIF_STRIPS:
		case YIF_CONNECTED_LINES:
			if(m_piNumVerts!=NULL && index>=0 && index<GetNumPrims())
				iRes = m_piNumVerts[index];
			break;
		case YIF_TRIANGLES:
			iRes = 3;
			break;
		case YIF_QUADS:
			iRes = 4;
			break;
		case YIF_LINES:
			iRes = 2;
			break;
		default:
			assert(false);
	}
	
	return iRes;
}

int CYifSurface::GetIndexBufferOffset() const
{
	return m_iOffset;
}

int CYifSurface::GetMaterialIndex() const
{
	return m_iMatIndex;
}

void CYifSurface::SetRegularPatchIndicesOffset(const int offset)
{
	m_iIndicesOffsetRegularPatches = offset;
}

void CYifSurface::SetRegularPatchOffset(const int offset)
{
	m_iOffsetRegularPatches = offset;
}

void CYifSurface::SetIrregularPatchIndicesOffset(const int offset)
{
	m_iIndicesOffsetIrregularPatches = offset;
}

void CYifSurface::SetIrregularPatchOffset(const int offset)
{
	m_iOffsetIrregularPatches = offset;
}

void CYifSurface::SetNrIrregularPatches(const int iNrIrregularPatches)
{
	m_iNrIrregularPatches = iNrIrregularPatches;
}

void CYifSurface::SetNrRegularPatches(const int iNrRegularPatches)
{
	m_iNrRegularPatches = iNrRegularPatches;
}

int CYifSurface::GetRegularPatchIndicesOffset() const
{
	return m_iIndicesOffsetRegularPatches;
}

int CYifSurface::GetRegularPatchOffset() const
{
	return m_iOffsetRegularPatches;
}

int CYifSurface::GetIrregularPatchIndicesOffset() const
{
	return m_iIndicesOffsetIrregularPatches;
}

int CYifSurface::GetIrregularPatchOffset() const
{
	return m_iOffsetIrregularPatches;
}

int CYifSurface::GetNrIrregularPatches() const
{
	return m_iNrIrregularPatches;
}

int CYifSurface::GetNrRegularPatches() const
{
	return m_iNrRegularPatches;
}

CYifSurface::CYifSurface()
{
	m_iMatIndex = 0;			// reference to material
	m_piNumVerts = NULL;
	
	m_iNumPrims = 0;
	m_iOffset = 0;

	m_iIndicesOffsetRegularPatches=0;
	m_iOffsetRegularPatches=0;
	m_iNrRegularPatches=0;
	m_iIndicesOffsetIrregularPatches=0;
	m_iOffsetIrregularPatches=0;
	m_iNrIrregularPatches=0;
}

CYifSurface::~CYifSurface()
{
}

void CYifMesh::SetVertices(SYifVertex * pSrcVerts)
{
	m_pVertices = pSrcVerts;
}

void CYifMesh::SetSkinVertInfo(SYifSkinVertInfo * pWeightVertsInfo)
{
	m_pWeightVertsInfo = pWeightVertsInfo;
}

void CYifMesh::SetIndices(int * piIndices, const int iNrIndices)
{
	m_piIndices = piIndices;
	m_iNumOfIndices = iNrIndices;
}

bool CYifMesh::SetNumSurfaces(const int iNumSurfaces)
{
	assert(iNumSurfaces>0 && m_pSurfaces==NULL);
	if(iNumSurfaces<=0 || m_pSurfaces!=NULL)
	{ CleanUp(); return false; }
	m_pSurfaces = new CYifSurface[iNumSurfaces];
	if(m_pSurfaces==NULL)
	{ CleanUp(); return false; }
	m_iNumSurfaces = iNumSurfaces;
	return true;
}

CYifSurface * CYifMesh::GetSurfRef(const int index)		// must be less than m_iNumSurfaces
{
	assert(index>=0 && index<m_iNumSurfaces);
	if(index>=0 && index<m_iNumSurfaces)
		return &m_pSurfaces[index];
	else
		return NULL;
}

bool CYifMesh::SetName(const char * pName)
{
	bool bRes=false;
	if(pName!=NULL)
	{
		const size_t iLen = strlen(pName)+1;
		m_pMeshName = new char[iLen];
		if(m_pMeshName!=NULL)
		{
			strncpy(m_pMeshName, pName, iLen);
			bRes=true;
		}
	}
	return bRes;
}

int CYifMesh::GetNumIndices() const
{
	return m_iNumOfIndices;
}

int CYifMesh::GetNumSurfaces() const
{
	return m_iNumSurfaces;
}

const char * CYifMesh::GetName() const
{
	return m_pMeshName;
}

const SYifVertex * CYifMesh::GetVertices() const
{
	return m_pVertices;
}

const SYifSkinVertInfo * CYifMesh::GetSkinVertices() const
{
	return m_pWeightVertsInfo;
}

const int * CYifMesh::GetIndices() const
{
	return m_piIndices;
}

int CYifMesh::GetNumBones() const
{
	return m_iNrBones;
}

const char * CYifMesh::GetBoneName(const int index) const
{
	return m_pcNamesData+m_piOffsToNameTable[index];
}

const Matrix4* CYifMesh::GetInvMats() const
{
	return m_pfInvMats;
}

void CYifMesh::SetBonesInvMats(Matrix4* pfInvMats)
{
	assert(m_pfInvMats==NULL);
	m_pfInvMats = pfInvMats;
}

void CYifMesh::SetBonesOffsToNameTable(int * piOffsToNameTable)
{
	assert(m_piOffsToNameTable==NULL);
	m_piOffsToNameTable = piOffsToNameTable;
}

void CYifMesh::SetNamesData(char * pcNamesData)
{
	assert(m_pcNamesData==NULL);
	m_pcNamesData = pcNamesData;
}

void CYifMesh::SetNumBones(const int iNrBones)
{
	assert(m_iNrBones==0 && iNrBones>0);
	m_iNrBones = iNrBones;
}

void CYifMesh::SetPatchIndices(int * piPatchesIndices)
{
	m_piPatchesIndices = piPatchesIndices;
}

void CYifMesh::SetPatchPrefixes(unsigned int * puPrefixes)
{
	m_puPrefixes = puPrefixes;
}

void CYifMesh::SetPatchValences(unsigned int * puValences)
{
	m_puValences = puValences;
}

int * CYifMesh::GetPatchIndices() const
{
	return m_piPatchesIndices;
}

unsigned int * CYifMesh::GetPatchPrefixes() const
{
	return m_puPrefixes;
}

unsigned int * CYifMesh::GetPatchValences() const
{
	return m_puValences;
}

void CYifMesh::CleanUp()
{
	if(m_pfInvMats!=NULL)
	{ delete [] m_pfInvMats; m_pfInvMats=NULL; }

	if(m_piOffsToNameTable!=NULL)
	{ delete [] m_piOffsToNameTable; m_piOffsToNameTable=NULL; }

	if(m_pcNamesData!=NULL)
	{ delete [] m_pcNamesData; m_pcNamesData=NULL; }

	m_iNrBones = 0;
}

CYifMesh::CYifMesh()
{
	m_pMeshName = NULL;
	m_iNumSurfaces = 0;
	m_pSurfaces = NULL;

	m_pVertices = NULL;			// array of unique vertices
	m_pWeightVertsInfo = NULL;
	m_iNumOfIndices = 0;
	m_piIndices = NULL;

	m_piPatchesIndices = NULL;
	m_puPrefixes = NULL;
	m_puValences = NULL;

	m_pfInvMats = NULL;
	m_piOffsToNameTable = NULL;
	m_pcNamesData = NULL;
	m_iNrBones = 0;
}

CYifMesh::~CYifMesh()
{
}

void CYifMeshInstance::SetMeshIndex(const int iMeshIndex)
{
	m_iMeshIndex = iMeshIndex;
}

void CYifMeshInstance::SetNodeTransformIndex(const int iTNodeIndex)
{
	m_iTransformIndex = iTNodeIndex;
}

bool CYifMeshInstance::SetMeshInstName(const char pcName[])
{
	bool bRes = false;

	if(pcName!=NULL && m_pMeshInstName==NULL)
	{
		const size_t iLen = strlen(pcName)+1;
		m_pMeshInstName = new char[iLen];
		if(m_pMeshInstName!=NULL)
		{
			strncpy(m_pMeshInstName, pcName, iLen);
			bRes = true;
		}
	}
	return bRes;
}

bool CYifMeshInstance::SetNumBones(const int iNrBones)
{
	bool bRes = false;
	assert(m_piBoneToTransform==NULL);
	m_piBoneToTransform = new int[iNrBones];
	if(m_piBoneToTransform!=NULL)
	{
		for(int i=0; i<iNrBones; i++)
			m_piBoneToTransform[i]=-1;
		bRes = true;
	}
	return bRes;
}

void CYifMeshInstance::SetBoneNodeTransformIndex(const int bone_index, const int iTNodeIndex)
{
	m_piBoneToTransform[bone_index] = iTNodeIndex;
}

int CYifMeshInstance::GetBoneNodeTransformIndex(const int bone_index) const
{
	return m_piBoneToTransform[bone_index];
}

const char * CYifMeshInstance::GetName() const
{
	return m_pMeshInstName;
}

int CYifMeshInstance::GetMeshIndex() const
{
	return m_iMeshIndex;
}

int CYifMeshInstance::GetTransformIndex() const
{
	return m_iTransformIndex;
}

const int * CYifMeshInstance::GetBoneToTransformIndices() const
{
	return m_piBoneToTransform;
}


void CYifMeshInstance::CleanUp()
{
	if(m_pMeshInstName!=NULL)
	{ delete [] m_pMeshInstName; m_pMeshInstName=NULL; }

	if(m_piBoneToTransform!=NULL)
	{ delete [] m_piBoneToTransform; m_piBoneToTransform=NULL; }

	m_iMeshIndex = 0;
	m_iTransformIndex = 0;
}

CYifMeshInstance::CYifMeshInstance()
{
	m_pMeshInstName = NULL;
	m_piBoneToTransform = NULL;
	m_iMeshIndex = 0;
	m_iTransformIndex = 0;
}

CYifMeshInstance::~CYifMeshInstance()
{

}

const Matrix4 CYifTransformation::EvalTransform(const float t) const
{
	if(m_iNrKeys<=1)
	{
		return m_pLocToParent[0];
	}
	else
	{
		Matrix4 mat;
		Vector3 vP(0,0,0);
		Vector3 vS(1,1,1);
		Quat Q(Vector3(0,0,0), 1);

		// let's locate keys
		const float t_clmp = (t < 0) ? 0 : ((t > 1) ? 1 : t);
		const int iNumKeysSubOne = GetNumKeys()-1;

		int i2 = (int) (t_clmp * iNumKeysSubOne);
		if(i2 == iNumKeysSubOne) --i2;		// too far, rewind	
		int i3 = i2+1;
		int i1 = (i2 == 0) ? i2 : i2-1;
		int i4 = (i3 == iNumKeysSubOne) ? i3 : i3+1;
		
		// generate interpolated result (Hermite)
		const float t1 = t*iNumKeysSubOne - i2;	// time, relative to chosen polynomial
		const float t2 = t1*t1;
		const float t3 = t1*t2;

		const float b = 3*t2 - 2*t3;
		const float d = t3 - t2;
		const float a = 1 - b;
		const float c = d - t2 + t1;

		if(GetQuaternionBuffer()!=NULL)
		{
			const Quat Q1 = GetQuaternion(i2);
			const Quat Q2 = GetQuaternion(i3);
			Q = slerp(t1, Q1, Q2);
		}

		if(GetPositionBuffer()!=NULL)
		{
			const Vector3 &P1 = GetPosition(i2);
			const Vector3 &P2 = GetPosition(i3);
			const Vector3 R1 = P2 - GetPosition(i1);
			const Vector3 R2 = GetPosition(i4) - P1;
			vP = a*P1 + b*P2 + c*R1 + d*R2;
		}

		if(GetScaleValueBuffer()!=NULL)
		{
			const Vector3 &S1 = GetScaleValue(i2);
			const Vector3 &S2 = GetScaleValue(i3);
			const Vector3 R1 = S2 - GetScaleValue(i1);
			const Vector3 R2 = GetScaleValue(i4) - S1;
			vS = a*S1 + b*S2 + c*R1 + d*R2;
		}

		mat = Matrix4::rotation(Q);
		mat.setCol3(Vector4(vP,1.f));
		for(int i=0; i<3; i++)
		{
			const float fS = i==0 ? vS.getX() : (i==1 ? vS.getY() : vS.getZ());
			mat.setCol(i, fS*mat.getCol(i));
		}

		return mat;
	}
}

void CYifTransformation::CleanUp()
{
	ClearRot();
	ClearPositions();
	ClearScaleValues();

	if(m_pLocToParent!=NULL)
	{ delete [] m_pLocToParent; m_pLocToParent = NULL; }

	m_fDuration=0.0f;
	m_iNrKeys = 1;
	m_iIndexToFirstChild=-1;		// -1 means no child
	m_iIndexToNextSibling=-1;		// -1 means no siblings
}

bool CYifTransformation::SetNumKeys(const int iNrKeys)	// allocates memory
{
	assert(iNrKeys>0 && m_pQuaternions==NULL && m_pPositions==NULL && m_pScaleValues==NULL && m_pLocToParent==NULL);
	if(!(iNrKeys>0 && m_pQuaternions==NULL && m_pPositions==NULL && m_pScaleValues==NULL && m_pLocToParent==NULL))
	{ CleanUp(); return false; }

	if(iNrKeys==1)
	{
		m_pLocToParent = new Matrix4[1];
		if(m_pLocToParent==NULL)
		{ CleanUp(); return false; }
	}
	else
	{
		m_pQuaternions = new Quat[iNrKeys];
		m_pPositions = new Vector3Unaligned[iNrKeys];
		m_pScaleValues = new Vector3Unaligned[iNrKeys];
		if(m_pQuaternions==NULL || m_pPositions==NULL || m_pScaleValues==NULL)
		{ CleanUp(); return false; }
	}
	m_iNrKeys = iNrKeys;
	return true;
}

int CYifTransformation::GetNumKeys() const
{
	return m_iNrKeys;
}

const Quat * CYifTransformation::GetQuaternionBuffer() const
{
	return m_pQuaternions;
}

const Vector3Unaligned * CYifTransformation::GetPositionBuffer() const
{
	return m_pPositions;
}

const Vector3Unaligned * CYifTransformation::GetScaleValueBuffer() const
{
	return m_pScaleValues;
}

void CYifTransformation::SetQuaternion(const int key, const Quat &quat)
{
	assert(m_pQuaternions!=NULL && key>=0 && key<m_iNrKeys);
	m_pQuaternions[key]=quat;
}

Quat CYifTransformation::GetQuaternion(const int key) const
{
	assert(m_pQuaternions!=NULL && key>=0 && key<m_iNrKeys);
	return m_pQuaternions[key];
}

void CYifTransformation::SetPosition(const int key, const Vector3 &v0)
{
	assert(m_pPositions!=NULL && key>=0 && key<m_iNrKeys);
	m_pPositions[key] = v0;
}

Vector3 CYifTransformation::GetPosition(const int key) const
{
	assert(m_pPositions!=NULL && key>=0 && key<m_iNrKeys);
	return ToVector3( m_pPositions[key] );
}

void CYifTransformation::SetScaleValue(const int key, const Vector3 &s0)
{
	assert(m_pScaleValues!=NULL && key>=0 && key<m_iNrKeys);
	m_pScaleValues[key] = s0;
}

Vector3 CYifTransformation::GetScaleValue(const int key) const
{
	assert(m_pScaleValues!=NULL && key>=0 && key<m_iNrKeys);
	return ToVector3( m_pScaleValues[key] );
}

void CYifTransformation::SetDuration(const float fDuration)
{
	m_fDuration = fDuration;
}

float CYifTransformation::GetDuration() const
{
	return m_fDuration;
}

void CYifTransformation::SetLocalToParent(const Matrix4 &mat)
{
	assert(m_pLocToParent!=NULL);
	*m_pLocToParent = mat;
}

Matrix4 CYifTransformation::GetLocalToParent() const
{
	return *m_pLocToParent;
}

void CYifTransformation::SetFirstChild(const int index)
{
	m_iIndexToFirstChild = index;
}

void CYifTransformation::SetNextSibling(const int index)
{
	m_iIndexToNextSibling = index;
}

int CYifTransformation::GetFirstChild() const
{
	return m_iIndexToFirstChild;
}

int CYifTransformation::GetNextSibling() const
{
	return m_iIndexToNextSibling;
}

void CYifTransformation::ClearRot()
{
	if(m_pQuaternions!=NULL)
	{ delete [] m_pQuaternions; m_pQuaternions=NULL; }
}

void CYifTransformation::ClearPositions()
{
	if(m_pPositions!=NULL)
	{ delete [] m_pPositions; m_pPositions=NULL; }
}

void CYifTransformation::ClearScaleValues()
{
	if(m_pScaleValues!=NULL)
	{ delete [] m_pScaleValues; m_pScaleValues=NULL; }
}

CYifTransformation::CYifTransformation()
{
	m_fDuration=0.0f;
	m_pQuaternions=NULL;
	m_pPositions=NULL;
	m_pScaleValues=NULL;
	m_pLocToParent=NULL;
	m_iNrKeys=0;
	m_iIndexToFirstChild=-1;		// -1 means no child
	m_iIndexToNextSibling=-1;		// -1 means no siblings
}

CYifTransformation::~CYifTransformation()
{
}

// allocates memory
bool CYifScene::SetNumMeshes(const int iNrMeshes)
{
	assert(iNrMeshes>0 && m_pMeshes==NULL);
	if(iNrMeshes<=0 || m_pMeshes!=NULL)
	{ CleanUp(); return false; }
	m_pMeshes = new CYifMesh[iNrMeshes];
	if(m_pMeshes==NULL)
	{ CleanUp(); return false; }
	m_iNumMeshes = iNrMeshes;
	return true;
}

CYifMesh * CYifScene::GetMeshRef(const int index)
{
	assert(index>=0 && index<m_iNumMeshes);
	if(index>=0 && index<m_iNumMeshes)
		return &m_pMeshes[index];
	else
		return NULL;
}

bool CYifScene::SetNumMeshInstances(const int iNumMeshInstances)
{
	assert(iNumMeshInstances>0 && m_pMeshInstances==NULL);
	if(iNumMeshInstances<=0 || m_pMeshInstances!=NULL)
	{ CleanUp(); return false; }
	m_pMeshInstances = new CYifMeshInstance[iNumMeshInstances];
	if(m_pMeshInstances==NULL)
	{ CleanUp(); return false; }
	m_iNumMeshInstances = iNumMeshInstances;
	return true;
}

CYifMeshInstance * CYifScene::GetMeshInstanceRef(const int index)
{
	assert(index>=0 && index<m_iNumMeshInstances);
	if(index>=0 && index<m_iNumMeshInstances)
		return &m_pMeshInstances[index];
	else
		return NULL;
}

bool CYifScene::SetNumTransformations(const int iNumTransformations)
{
	assert(iNumTransformations>0 && m_pTransformations==NULL);
	if(iNumTransformations<=0 || m_pTransformations!=NULL)
	{ CleanUp(); return false; }
	m_pTransformations = new CYifTransformation[iNumTransformations];
	if(m_pTransformations==NULL)
	{ CleanUp(); return false; }
	m_iNumTransformations = iNumTransformations;
	return true;
}

CYifTransformation * CYifScene::GetTransformationRef(const int index)
{
	assert(index>=0 && index<m_iNumTransformations);
	if(index>=0 && index<m_iNumTransformations)
		return &m_pTransformations[index];
	else
		return NULL;
}


int CYifScene::GetNumMeshes() const
{
	return m_iNumMeshes;
}

int CYifScene::GetNumMeshInstances() const
{
	return m_iNumMeshInstances;
}

int CYifScene::GetNumTransformations() const
{
	return m_iNumTransformations;
}

int CYifScene::GetNumCameras() const
{
	return m_iNumCameras;
}

int CYifScene::GetNumLights() const
{
	return m_iNumLights;
}

void CYifScene::CleanUp()
{
	for(int m=0; m<m_iNumMeshes; m++)
		m_pMeshes[m].CleanUp();

	for(int m=0; m<m_iNumMeshInstances; m++)
		m_pMeshInstances[m].CleanUp();

	if(m_pMeshes!=NULL)
	{ delete [] m_pMeshes; m_pMeshes=NULL; }

	if(m_pMeshInstances!=NULL)
	{ delete [] m_pMeshInstances; m_pMeshInstances=NULL; }

	if(m_pTransformations!=NULL)
	{ delete [] m_pTransformations; m_pTransformations=NULL; }


	//
	m_iNumMeshes = 0;
	m_iNumMeshInstances = 0;
	m_iNumLights = 0;
	m_iNumCameras = 0;
	m_iNumTransformations = 0;
}

CYifScene::CYifScene()
{
	m_iNumMeshes = 0;
	m_iNumMeshInstances = 0;
	m_iNumLights = 0;
	m_iNumCameras = 0;
	m_iNumTransformations = 0;

	m_pMeshes = NULL;
	m_pMeshInstances = NULL;
	m_pTransformations = NULL;
}

CYifScene::~CYifScene()
{
}
