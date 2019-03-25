/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __YIF_H__
#define __YIF_H__

#include "../../toolkit/geommath/geommath.h"
#include <stdio.h>

struct SYifVertex
{
	Vector3Unaligned vPos;
	Vector3Unaligned vNorm;
	Vector4Unaligned vTangent;
	Vector4Unaligned vColor;
	float s, t;
};

struct SYifSkinVertInfo
{
	Vector4Unaligned vWeights0;
	Vector4Unaligned vWeights1;
	unsigned short bone_mats0[4];
	unsigned short bone_mats1[4];
};


class CYifMaterial
{
public:
	CYifMaterial();
	~CYifMaterial();

private:
	char * m_pMaterialName;
};

enum eYifPrimType
{
	YIF_TRIANGLES			=	1,
	YIF_QUADS				=	2,
	YIF_FANS				=	3,
	YIF_STRIPS				=	4,
	YIF_LINES				=	5,
	YIF_CONNECTED_LINES		=	6
};

class CYifSurface
{
public:
	void SetPrimType(eYifPrimType eType);
	void SetNumPrims(int iNrNumPrims);
	void SetNumVertsPerPrim(int * piNumVerts);	// only used for fans and strips
	void SetIndexBufferOffset(int iOffset);

	eYifPrimType GetPrimType() const;
	int GetNumPrims() const;
	int GetMaterialIndex() const;
	const int * GetNumVertsBuffer() const;
	int GetNumVertsOnPrim(const int index) const;
	int GetIndexBufferOffset() const;

	void SetRegularPatchIndicesOffset(const int offset);
	void SetRegularPatchOffset(const int offset);
	void SetIrregularPatchIndicesOffset(const int offset);
	void SetIrregularPatchOffset(const int offset);
	void SetNrIrregularPatches(const int iNrIrregularPatches);
	void SetNrRegularPatches(const int iNrRegularPatches);

	int GetRegularPatchIndicesOffset() const;
	int GetRegularPatchOffset() const;
	int GetIrregularPatchIndicesOffset() const;
	int GetIrregularPatchOffset() const;
	int GetNrIrregularPatches() const;
	int GetNrRegularPatches() const;

	CYifSurface();
	~CYifSurface();

private:
	// type
	int m_iNumPrims;
protected:
	uint8_t m_reserved0[4];
private:
	int * m_piNumVerts;			// only set for fans and strips
	int m_iOffset;				// offset into the index list
	eYifPrimType m_ePrimType;

	int m_iMatIndex;			// index to a material

	// patch stuff
private:
	int m_iIndicesOffsetRegularPatches, m_iOffsetRegularPatches, m_iNrRegularPatches;
	int m_iIndicesOffsetIrregularPatches, m_iOffsetIrregularPatches, m_iNrIrregularPatches;
protected:
	uint8_t m_reserved1[4];
};

class CYifMesh
{
public:
	bool SetNumSurfaces(const int iNumSurfaces);
	CYifSurface * GetSurfRef(const int index);	// must be less than m_iNumSurfaces

	void SetVertices(SYifVertex * pSrcVerts);
	void SetSkinVertInfo(SYifSkinVertInfo * pWeightVertsInfo);
	void SetIndices(int * piIndices, const int iNrIndices);
	bool SetName(const char * pName);

	int GetNumIndices() const;
	int GetNumSurfaces() const;
	const char * GetName() const;
	const SYifVertex * GetVertices() const;
	const SYifSkinVertInfo * GetSkinVertices() const;
	const int * GetIndices() const;

	int GetNumBones() const;
	const char * GetBoneName(const int index) const;
	const Matrix4* GetInvMats() const;

	void SetBonesInvMats(Matrix4* pfInvMats);
	void SetBonesOffsToNameTable(int * piOffsToNameTable);
	void SetNamesData(char * pcNamesData);
	void SetNumBones(const int iNrBones);
	void SetPatchIndices(int * piPatchesIndices);
	void SetPatchPrefixes(unsigned int * puPrefixes);
	void SetPatchValences(unsigned int * puValences);
	int * GetPatchIndices() const;
	unsigned int * GetPatchPrefixes() const;
	unsigned int * GetPatchValences() const;

	void CleanUp();

	CYifMesh();
	~CYifMesh();

private:
	char * m_pMeshName;

	int m_iNumSurfaces;
protected:
	uint8_t m_reserved0[4];
private:
	CYifSurface * m_pSurfaces;

	SYifVertex * m_pVertices;	// array of unique vertices
	SYifSkinVertInfo * m_pWeightVertsInfo;
	int m_iNumOfIndices;
protected:
	uint8_t m_reserved1[4];
private:
	int * m_piIndices;

private:
	int * m_piPatchesIndices;		// contains both regular and irregular patches of all surfaces
	unsigned int * m_puPrefixes;	// 4 per uint
	unsigned int * m_puValences;	// 4 per uint

private:
	Matrix4* m_pfInvMats;
	int * m_piOffsToNameTable;
	char * m_pcNamesData;
	int m_iNrBones;
protected:
	uint8_t m_reserved2[4];
};

class CYifTransformation
{
	friend class CYifSceneReader;

public:
	void CleanUp();

	bool SetNumKeys(const int iNrKeys);	// allocates memory
	int GetNumKeys() const;

	const Quat * GetQuaternionBuffer() const;
	const Vector3Unaligned * GetPositionBuffer() const;
	const Vector3Unaligned * GetScaleValueBuffer() const;

	const Matrix4 EvalTransform(const float t) const;

	void SetQuaternion(const int key, const Quat &quat);
	Quat GetQuaternion(const int key) const;
	void SetPosition(const int key, const Vector3 &v0);
	Vector3 GetPosition(const int key) const;
	void SetScaleValue(const int key, const Vector3 &s0);
	Vector3 GetScaleValue(const int key) const;
	void SetDuration(const float fDuration);
	float GetDuration() const;

	// only works when m_iNrKeys==1
	void SetLocalToParent(const Matrix4 &mat);
	Matrix4 GetLocalToParent() const;

	void ClearRot();
	void ClearPositions();
	void ClearScaleValues();

	void SetFirstChild(const int index);
	void SetNextSibling(const int index);

	int GetFirstChild() const;
	int GetNextSibling() const;

	CYifTransformation();
	~CYifTransformation();

private:
	float m_fDuration;
	int m_iNrKeys;
	Matrix4 * m_pLocToParent;	// only initialized when m_iNrKeys==1
	Quat * m_pQuaternions;
	Vector3Unaligned * m_pPositions;
	Vector3Unaligned * m_pScaleValues;

private:
	int m_iIndexToFirstChild;	// -1 means no child
	int m_iIndexToNextSibling;	// -1 means no siblings
};

class CYifMeshInstance
{
public:
	// must have some reference to an animation
	void CleanUp();
	void SetMeshIndex(const int iMeshIndex);
	void SetNodeTransformIndex(const int iTNodeIndex);
	bool SetMeshInstName(const char pcName[]);
	bool SetNumBones(const int iNrBones);
	void SetBoneNodeTransformIndex(const int bone_index, const int iTNodeIndex);

	int GetBoneNodeTransformIndex(const int bone_index) const;
	const char * GetName() const;
	int GetMeshIndex() const;
	int GetTransformIndex() const;
	const int * GetBoneToTransformIndices() const;

	CYifMeshInstance();
	~CYifMeshInstance();

private:
	char * m_pMeshInstName;
	int m_iMeshIndex;
	int m_iTransformIndex;

	// length is determined by GetNumBones() on the referenced mesh
	int * m_piBoneToTransform;					// reference to a transformation node
};

class CYifScene
{
public:
	virtual int GetNumMeshes() const;
	virtual int GetNumMeshInstances() const;
	virtual int GetNumTransformations() const;
	virtual int GetNumCameras() const;
	virtual int GetNumLights() const;

	bool SetNumMeshes(const int iNrMeshes);	// allocates memory
	CYifMesh * GetMeshRef(const int index);	// must be less than m_iNumMeshes

	bool SetNumTransformations(const int iNumTransformations);	// allocates memory
	CYifTransformation * GetTransformationRef(const int index);	// must be less than m_iNumTransformations

	bool SetNumMeshInstances(const int iNrMeshes);	// allocates memory
	CYifMeshInstance * GetMeshInstanceRef(const int index);	// must be less than m_iNumMeshInstances

	void CleanUp();

	CYifScene();
	~CYifScene();

private:
	int m_iNumMeshes;
	int m_iNumMeshInstances;
	int m_iNumTransformations;
	int m_iNumLights;
	int m_iNumCameras;

protected:
	uint8_t m_reserved0[4];
private:
	CYifMesh * m_pMeshes;
	CYifMeshInstance * m_pMeshInstances;

	CYifTransformation * m_pTransformations;	// transformation tree stored in pre-order
};

#endif
