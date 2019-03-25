/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __YIFGPUREADER_H__
#define __YIFGPUREADER_H__

#include "yif_reader.h"
#include "../../toolkit/toolkit.h"
#include "../../toolkit/allocator.h"
#include "../../toolkit/geommath/geommath.h"
#include "std_cbuffer.h"

#define GEN_REP_UNI_SCALE(n)		(1.f / (n+1))
#define GEN_REP_X_OFFS(n)			((float) (21.0f/n)*(i-((n-1)/2.0f)))

struct SGPUMesh;

class CYif2GpuReader : public CYifSceneReader
{
public:
	typedef void* MemBlock;
	virtual bool ReadScene(sce::Gnmx::Toolkit::Allocators *allocators, const char file[]);
	void UpdateTransformationHierarchy(const float t, const Matrix4 &m44WorldToView, const Matrix4 &m44Proj);		// t \in [0;1]
	void DrawScene(sce::Gnmx::GnmxGfxContext &gfxc, cbExtraInst *constantBuffer, int iNrRepeats);
	void ToggleMode();

	void SetGlobalTessFactor(const int iTessFactor);
	void ToggleTangInDS();
	void ToggleReadVertsInHS();
	void SetTessSupported(const bool bEnable);
	void SetAnimationSupported(const bool bEnable);
	void TogglePatchesMethod();
	void ToggleNrThreadsPerRegularPatch();

	bool IsModeReadVertsInHS() const;
	bool IsModeGenTangsInDS() const;
	bool IsPatchModeGregory() const;
	int GetNrRegularPatches() const;
	int GetNrIrregularPatches() const;
	int GetNrThreadsPerRegularPatch() const;
	void DecreaseNrPatchesOffset();
	void IncreaseNrPatchesOffset();

	CYif2GpuReader();
	~CYif2GpuReader();

	int m_iNrPatchesOffset;
private:
	void DrawMesh(sce::Gnmx::GnmxGfxContext &gfxc, const int mesh_index);
	bool HasPatches(CYifMesh &mesh) const;

	sce::Gnm::PrimitiveType GetGpuPrimType(CYifMesh &yifmesh, const int iSurf) const;
	int GetNrIndices(CYifMesh &yifmesh, const int iSurf) const;
	void UpdateNodeTransformation(const int node_index, const int parent_index, const float t);

	void PrepStreams();
	void SetupStreams(SGPUMesh * pGPUMesh, CYifMesh &yifmesh);
	void Mesh2GPU(sce::Gnmx::Toolkit::Allocators *allocators, SGPUMesh * pGPUMesh, CYifMesh &yifmesh);

protected:
	uint8_t m_reserved0[4];
private:
	Matrix4 * m_pMats;	// length is equal to GetNumTransformations()
	SGPUMesh * m_pGpuMeshes;
	struct SInstCBufs
	{
		sce::Gnm::Buffer m_sBonesPalette;
		sce::Gnm::Buffer m_sInstData;
	};

	SInstCBufs * m_psInstCbuf, * m_psInstCbuf1, * m_psInstCbuf2;
	bool m_bIsSinglePass;

protected:
	uint8_t m_reserved1[7];
private:
	void *m_uPtrGPUskinning1, *m_uPtrGPUskinning2, *m_uPtrGPUskinningCur;
	sce::Gnmx::CsShader * m_csb_skin, * m_csb_skinCC;
	sce::Gnmx::VsShader * m_vsb_skin, * m_vsb_noskin;
	sce::Gnmx::LsShader * m_lsb_skin, * m_lsb_noskin, * m_lsb_hsvert_noskin;		// _hsvert_ does not need a fetch shader

	MemBlock m_vsb_skin_fetch, m_vsb_noskin_fetch;
	MemBlock m_lsb_skin_fetch, m_lsb_noskin_fetch;

	// domain shaders
	sce::Gnmx::VsShader * m_dsb_bez_patch, * m_dsb_bez_patch_16threads, * m_dsb_greg_patch, * m_dsbirreg_dt_patch, * m_dsbirreg_ht_patch;
	

	sce::Gnmx::HsShader * m_hsbreg_patch[4], * m_hsbGreg_patch[2], * m_hsbirreg_patch[4];
	bool m_bNoUpdateYet;	// must update bone mats at least once to draw

private:
	int GetPatchSize(const sce::Gnmx::HsShader * pHS_shader, const int ls_stride);

protected:
	uint8_t m_reserved2[3];
private:
	int m_iTessFactor;
	bool m_bGenTangsInDS, m_bReadVertsInHS, m_bIsGregoryPatches;
protected:
	uint8_t m_reserved3[1];
private:
	int m_iNrRegPatches, m_iNrIrregPatches;		// number of regular and irregular patches per tg

	bool m_bIsFourThreadsPerRegPatch;			// using 4 threads per regular patch

	bool m_bTessEnabled, m_bAnimEnabled;

private:
	void PrepHullShaders();

protected:
	uint8_t m_reserved4[1];
private:
	struct sParamsHsTG
	{
		int iNumPatches, iVgtPrims;
		sce::Gnmx::HsShader *  hullShader;
		MemBlock tessCnstMemBlock;
	} m_params_HsTg[4];

	sce::Gnmx::LsShader * m_pLsbShaders[2];
	MemBlock * m_pLsbFetchShaders[2];
};

#endif
