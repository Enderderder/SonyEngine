/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "yif_reader.h"
#include <string.h>
#include <assert.h>
#include <algorithm>
#include <gnm.h>

#define MAKE_ID(a,b,c,d)	( (((unsigned int) d)<<24) | (((unsigned int) c)<<16) | (((unsigned int) b)<<8) | (((unsigned int) a)<<0) )

enum eCHUNK_TYPE
{
	CHUNK_UNKNOWN	= 0,
	CHUNK_YIF_		= MAKE_ID('Y','I','F','_'),
	CHUNK_MSHS		= MAKE_ID('M','S','H','S'),
	CHUNK_MCNT		= MAKE_ID('M','C','N','T'),
	CHUNK_MESH		= MAKE_ID('M','E','S','H'),
	CHUNK_MSTS		= MAKE_ID('M','S','T','S'),
	CHUNK_MINS		= MAKE_ID('M','I','N','S'),
	CHUNK_TRNS		= MAKE_ID('T','R','N','S'),
	CHUNK_TCNT		= MAKE_ID('T','C','N','T'),
	CHUNK_TRAN		= MAKE_ID('T','R','A','N'),
	CHUNK_NAME		= MAKE_ID('N','A','M','E'),
	CHUNK_VRTS		= MAKE_ID('V','R','T','S'),
	CHUNK_WGTS		= MAKE_ID('W','G','T','S'),
	CHUNK_BCNT		= MAKE_ID('B','C','N','T'),
	CHUNK_MINV		= MAKE_ID('M','I','N','V'),
	CHUNK_BNME		= MAKE_ID('B','N','M','E'),
	CHUNK_INDX		= MAKE_ID('I','N','D','X'),
	CHUNK_SRFS		= MAKE_ID('S','R','F','S'),
	CHUNK_SCNT		= MAKE_ID('S','C','N','T'),
	CHUNK_SURF		= MAKE_ID('S','U','R','F'),
	CHUNK_PCNT		= MAKE_ID('P','C','N','T'),
	CHUNK_PTYP		= MAKE_ID('P','T','Y','P'),
	CHUNK_IOFF		= MAKE_ID('I','O','F','F'),
	CHUNK_NVRT		= MAKE_ID('N','V','R','T'),
	CHUNK_MAIX		= MAKE_ID('M','A','I','X'),
	CHUNK_MEIX		= MAKE_ID('M','E','I','X'),
	CHUNK_TRIX		= MAKE_ID('T','R','I','X'),
	CHUNK_BTIX		= MAKE_ID('B','T','I','X'),
	CHUNK_KCNT		= MAKE_ID('K','C','N','T'),
	CHUNK_PMTX		= MAKE_ID('P','M','T','X'),
	CHUNK_QBUF		= MAKE_ID('Q','B','U','F'),
	CHUNK_PBUF		= MAKE_ID('P','B','U','F'),
	CHUNK_SBUF		= MAKE_ID('S','B','U','F'),
	CHUNK_CHIX		= MAKE_ID('C','H','I','X'),
	CHUNK_SIIX		= MAKE_ID('S','I','I','X'),
	CHUNK_DURA		= MAKE_ID('D','U','R','A'),
	CHUNK_PAIN		= MAKE_ID('P','A','I','N'),
	CHUNK_PPFX		= MAKE_ID('P','P','F','X'),
	CHUNK_PVLC		= MAKE_ID('P','V','L','C'),
	CHUNK_PRIO		= MAKE_ID('P','R','I','O'),
	CHUNK_PROF		= MAKE_ID('P','R','O','F'),
	CHUNK_PIIO		= MAKE_ID('P','I','I','O'),
	CHUNK_PIOF		= MAKE_ID('P','I','O','F'),
	CHUNK_PRCT		= MAKE_ID('P','R','C','T'),
	CHUNK_PICT		= MAKE_ID('P','I','C','T')
};


#define MAX_CHUNK_DEPTH		1024

static int g_iCurDepth;
static int g_piChunkStack[MAX_CHUNK_DEPTH];

static eCHUNK_TYPE PushChunk(FILE * fptr, int * piChunk=NULL);
static void PopChunk(FILE * fptr);
static void SkipChunk(FILE * fptr);
static bool IsChunkDone(FILE * fptr);


bool CYifSceneReader::ReadScene(const char file[])
{
	bool bRes = false;
	FILE * fptr = fopen(file, "rb");
	if(fptr!=NULL)
	{
		// init chunk stack
		memset(g_piChunkStack, 0, sizeof(g_piChunkStack));
		g_iCurDepth = 0;

		// continue parsing
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		assert(eChunkID==CHUNK_YIF_);
		while(!IsChunkDone(fptr))
		{
			eChunkID = PushChunk(fptr);
			switch(eChunkID)
			{
				case CHUNK_MSHS:
					if(!ParseMeshes(fptr)) { CleanUp(); return false; }
					break;
				case CHUNK_MSTS:
					if(!ParseMeshInstances(fptr)) { CleanUp(); return false; }
					break;
				case CHUNK_TRNS:
					if(!ParseTransformations(fptr)) { CleanUp(); return false; }
					break;
				default:
					SkipChunk(fptr);
			}
			PopChunk(fptr);
		}
		PopChunk(fptr);

		bRes = true;
		fclose(fptr);
	}

	return bRes;
}

//---------------------------------------------------------------------
//------------------------ Parsing Meshes -----------------------------

bool CYifSceneReader::ParseMeshes(FILE * fptr)
{
	int iMeshIndex = 0;
	while(!IsChunkDone(fptr))
	{
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		switch(eChunkID)
		{
			case CHUNK_MCNT:
			{
				int iNumMeshes;
				fread(&iNumMeshes, sizeof(int), 1, fptr);
				if(!SetNumMeshes(iNumMeshes)) return false;
			}
			break;
			case CHUNK_MESH:
			{
				assert(iMeshIndex<GetNumMeshes());
				if(iMeshIndex<GetNumMeshes())
				{
					CYifMesh * pMesh = GetMeshRef(iMeshIndex);	// must be less than m_iNumMeshes
					if(!ParseMesh(pMesh, fptr)) return false;
					++iMeshIndex;
				}
			}
			break;
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}

	return true;
}

bool CYifSceneReader::ParseMesh(CYifMesh * pMesh, FILE * fptr)
{
	while(!IsChunkDone(fptr))
	{
		int iChunkSize = 0;
		eCHUNK_TYPE eChunkID = PushChunk(fptr, &iChunkSize);
		switch(eChunkID)
		{
			case CHUNK_SRFS:
			{
				if(!ParseSurfaces(pMesh, fptr)) return false;
			}
			break;
			case CHUNK_BCNT:
			{
				int iNumBones;
				fread(&iNumBones, sizeof(int), 1, fptr);
				pMesh->SetNumBones(iNumBones);
			}
			break;
			case CHUNK_VRTS:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrVerts = iChunkSize / sizeof(SYifVertex);
					SYifVertex * pvVerts = new SYifVertex[iNrVerts];
					if(pvVerts!=NULL)
					{
						fread(pvVerts, sizeof(SYifVertex), iNrVerts, fptr);

						// normalize the vertex positions to a bounding box from [-1,-1,-1] to [1,1,1]

						Vector3 mini = ToVector3(pvVerts[0].vPos);
						Vector3 maxi = ToVector3(pvVerts[0].vPos);
						for(int i=1; i<iNrVerts; ++i)
						{
							mini = minPerElem(mini, ToVector3(pvVerts[i].vPos));
							maxi = maxPerElem(mini, ToVector3(pvVerts[i].vPos));
						}
						const Vector3 size = maxi - mini;
						const float factor = 2.f / std::max(size.getX(), std::max(size.getY(), size.getZ()));
						for(int i=0; i<iNrVerts; ++i)
						{
							pvVerts[i].vPos = ToVector3(pvVerts[i].vPos) * factor;
						}

						// normalization done.

						pMesh->SetVertices(pvVerts);
						bRes=true;
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			case CHUNK_INDX:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrIndices = iChunkSize / sizeof(int);
					int * piIndices = new int[iNrIndices];
					if(piIndices!=NULL)
					{
						fread(piIndices, sizeof(int), iNrIndices, fptr);
						pMesh->SetIndices(piIndices, iNrIndices);
						bRes=true;
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			case CHUNK_NAME:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrCharacters = iChunkSize / sizeof(char);	// includes '\0'
					char * pcName = new char[iNrCharacters];
					if(pcName!=NULL)
					{
						fread(pcName, sizeof(char), iNrCharacters, fptr);
						pMesh->SetName(pcName);
						delete [] pcName;
						bRes=true;
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			case CHUNK_WGTS:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrVertices = iChunkSize / sizeof(SYifSkinVertInfo);
					SYifSkinVertInfo * pvWeightVerts = new SYifSkinVertInfo[iNrVertices];
					if(pvWeightVerts!=NULL)
					{
						fread(pvWeightVerts, sizeof(SYifSkinVertInfo), iNrVertices, fptr);
						pMesh->SetSkinVertInfo(pvWeightVerts);
						bRes=true;
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			case CHUNK_MINV:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrMats = iChunkSize / sizeof(Matrix4);
					Matrix4* pfInvMats = new Matrix4[iNrMats];
					if(pfInvMats!=NULL)
					{
						fread(pfInvMats, sizeof(Matrix4), iNrMats, fptr);
						pMesh->SetBonesInvMats(pfInvMats);
						bRes=true;
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			case CHUNK_BNME:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iStringSize = iChunkSize / sizeof(char);
					char * pcNames = new char[iStringSize];
					if(pcNames!=NULL)
					{
						// set names
						fread(pcNames, sizeof(char), iStringSize, fptr);
						pMesh->SetNamesData(pcNames);

						// Create offset to name table
						{
							int iNrNames, iOffset;
							iNrNames=0; iOffset = 0;
							while(iOffset<iChunkSize)
							{
								const int iNameLen = (const int) strlen(pcNames+iOffset)+1;
								iOffset += iNameLen; ++iNrNames;
							}
							assert(iOffset==iChunkSize);
							int * piOffsetToName = new int[iNrNames];
							if(piOffsetToName!=NULL)
							{
								iOffset = 0;
								for(int i=0; i<iNrNames; i++)
								{
									piOffsetToName[i]=iOffset;
									iOffset += ((const int) strlen(pcNames+iOffset)+1);
								}
								pMesh->SetBonesOffsToNameTable(piOffsetToName);
								const int iNrBones = pMesh->GetNumBones();
								SCE_GNM_ASSERT(iNrBones==0 || iNrBones==iNrNames);

								bRes=true;
							}
						}
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			case CHUNK_PAIN:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrPatchIndices = iChunkSize / sizeof(int);
					int * piPatchIndices = new int[iNrPatchIndices];
					if(piPatchIndices!=NULL)
					{
						fread(piPatchIndices, sizeof(int), iNrPatchIndices, fptr);
						pMesh->SetPatchIndices(piPatchIndices);
						bRes=true;
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			case CHUNK_PPFX:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrPatchPrefixes = iChunkSize / sizeof(unsigned int);
					unsigned int * puPatchPrefixes = new unsigned int[iNrPatchPrefixes];
					if(puPatchPrefixes!=NULL)
					{
						fread(puPatchPrefixes, sizeof(unsigned int), iNrPatchPrefixes, fptr);
						pMesh->SetPatchPrefixes(puPatchPrefixes);
						bRes=true;
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			case CHUNK_PVLC:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrPatchValences = iChunkSize / sizeof(unsigned int);
					unsigned int * puPatchValences = new unsigned int[iNrPatchValences];
					if(puPatchValences!=NULL)
					{
						fread(puPatchValences, sizeof(unsigned int), iNrPatchValences, fptr);
						pMesh->SetPatchValences(puPatchValences);
						bRes=true;
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}

	return true;
}

bool CYifSceneReader::ParseSurfaces(CYifMesh * pMesh, FILE * fptr)
{
	int iSurfIndex = 0;
	while(!IsChunkDone(fptr))
	{
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		switch(eChunkID)
		{
			case CHUNK_SCNT:
			{
				int iNumSurfaces;
				fread(&iNumSurfaces, sizeof(int), 1, fptr);
				if(!pMesh->SetNumSurfaces(iNumSurfaces)) return false;
			}
			break;
			case CHUNK_SURF:
			{
				assert(iSurfIndex<pMesh->GetNumSurfaces());
				if(iSurfIndex<pMesh->GetNumSurfaces())
				{
					CYifSurface * pSurf = pMesh->GetSurfRef(iSurfIndex);	// must be less than m_iNumMeshes
					if(!ParseSurface(pSurf, fptr)) return false;
					++iSurfIndex;
				}
			}
			break;
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}

	return true;
}

bool CYifSceneReader::ParseSurface(CYifSurface * pSurf, FILE * fptr)
{
	while(!IsChunkDone(fptr))
	{
		int iChunkSize = 0;
		eCHUNK_TYPE eChunkID = PushChunk(fptr, &iChunkSize);
		switch(eChunkID)
		{
			case CHUNK_PCNT:
			{
				int iPrimCount;
				fread(&iPrimCount, sizeof(int), 1, fptr);
				pSurf->SetNumPrims(iPrimCount);
			}
			break;
			case CHUNK_PTYP:
			{
				eYifPrimType prim_type;
				fread(&prim_type, sizeof(eYifPrimType), 1, fptr);
				pSurf->SetPrimType(prim_type);
			}
			break;
			case CHUNK_IOFF:
			{
				int iOffset;	// offset into mesh index buffer
				fread(&iOffset, sizeof(int), 1, fptr);
				pSurf->SetIndexBufferOffset(iOffset);
			}
			break;
			case CHUNK_MAIX:
			{
				int iMatIndex;	// material index
				fread(&iMatIndex, sizeof(int), 1, fptr);
				//pSurf->SetMaterialIndex();
			}
			break;
			case CHUNK_NVRT:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrNums = iChunkSize / sizeof(int);
					int * piNumVertsOnPrim = new int[iNrNums];
					if(piNumVertsOnPrim!=NULL)
					{
						fread(piNumVertsOnPrim, sizeof(int), iNrNums, fptr);
						pSurf->SetNumVertsPerPrim(piNumVertsOnPrim);
						bRes=true;
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			case CHUNK_PRIO:
			{
				int iOffset=0;
				fread(&iOffset, sizeof(int), 1, fptr);
				pSurf->SetRegularPatchIndicesOffset(iOffset);
			}
			break;
			case CHUNK_PROF:
			{
				int iOffset=0;
				fread(&iOffset, sizeof(int), 1, fptr);
				pSurf->SetRegularPatchOffset(iOffset);
			}
			break;
			case CHUNK_PIIO:
			{
				int iOffset=0;
				fread(&iOffset, sizeof(int), 1, fptr);
				pSurf->SetIrregularPatchIndicesOffset(iOffset);
			}
			break;
			case CHUNK_PIOF:
			{
				int iOffset=0;
				fread(&iOffset, sizeof(int), 1, fptr);
				pSurf->SetIrregularPatchOffset(iOffset);
			}
			break;
			case CHUNK_PRCT:
			{
				int iCount=0;
				fread(&iCount, sizeof(int), 1, fptr);
				pSurf->SetNrRegularPatches(iCount);
			}
			break;
			case CHUNK_PICT:
			{
				int iCount=0;
				fread(&iCount, sizeof(int), 1, fptr);
				pSurf->SetNrIrregularPatches(iCount);
			}
			break;
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}

	return true;
}

//---------------------------------------------------------------------
//-------------------- Parsing Mesh Instances -------------------------

bool CYifSceneReader::ParseMeshInstances(FILE * fptr)
{
	int iMeshInstIndex = 0;
	while(!IsChunkDone(fptr))
	{
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		switch(eChunkID)
		{
			case CHUNK_MCNT:
			{
				int iNumMeshInstances;
				fread(&iNumMeshInstances, sizeof(int), 1, fptr);
				if(!SetNumMeshInstances(iNumMeshInstances)) return false;
			}
			break;
			case CHUNK_MINS:
			{
				assert(iMeshInstIndex<GetNumMeshInstances());
				if(iMeshInstIndex<GetNumMeshInstances())
				{
					CYifMeshInstance * pMeshInst = GetMeshInstanceRef(iMeshInstIndex);	// must be less than m_iNumMeshes
					if(!ParseMeshInstance(pMeshInst, fptr)) return false;
					++iMeshInstIndex;
				}
			}
			break;
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}

	return true;
}

bool CYifSceneReader::ParseMeshInstance(CYifMeshInstance * pMeshInst, FILE * fptr)
{
	while(!IsChunkDone(fptr))
	{
		int iChunkSize=0;
		eCHUNK_TYPE eChunkID = PushChunk(fptr, &iChunkSize);
		switch(eChunkID)
		{
			case CHUNK_NAME:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrCharacters = iChunkSize / sizeof(char);	// includes '\0'
					char * pcName = new char[iNrCharacters];
					if(pcName!=NULL)
					{
						fread(pcName, sizeof(char), iNrCharacters, fptr);
						pMeshInst->SetMeshInstName(pcName);
						delete [] pcName;
						bRes=true;
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			case CHUNK_MEIX:
			{
				int iMeshIndex = 0;
				fread(&iMeshIndex, sizeof(int), 1, fptr);
				pMeshInst->SetMeshIndex(iMeshIndex);
			}
			break;
			case CHUNK_TRIX:
			{
				int iTransformIndex = 0;
				fread(&iTransformIndex, sizeof(int), 1, fptr);
				pMeshInst->SetNodeTransformIndex(iTransformIndex);
			}
			break;
			case CHUNK_BTIX:
			{
				bool bRes=false;
				if(iChunkSize>0)
				{
					const int iNrIndices = iChunkSize / sizeof(int);
					if(pMeshInst->SetNumBones(iNrIndices)==true)
					{
						const int iNR_MAX = 16;
						int piIndices[iNR_MAX];
						int iOffs = 0;
						while(iOffs<iNrIndices)
						{
							const int iReadIn = iNrIndices-iOffs;
							const int iClampedReadIn = iReadIn>iNR_MAX?iNR_MAX:iReadIn;
							fread(piIndices, sizeof(int), iClampedReadIn, fptr);
							
							for(int i=0; i<iClampedReadIn; i++)
								pMeshInst->SetBoneNodeTransformIndex(iOffs+i, piIndices[i]);

							iOffs += iClampedReadIn;
						}
					}
				}
				if(!bRes) SkipChunk(fptr);
			}
			break;
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}

	return true;
}

//---------------------------------------------------------------------
//---------------- Parsing Transformation Instances -------------------

bool CYifSceneReader::ParseTransformations(FILE * fptr)
{
	int iNodeIndex = 0;
	while(!IsChunkDone(fptr))
	{
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		switch(eChunkID)
		{
			case CHUNK_TCNT:
			{
				int iNumTransformationNodes;
				fread(&iNumTransformationNodes, sizeof(int), 1, fptr);
				if(!SetNumTransformations(iNumTransformationNodes)) return false;
			}
			break;
			case CHUNK_TRAN:
			{
				assert(iNodeIndex<GetNumTransformations());
				if(iNodeIndex<GetNumTransformations())
				{
					CYifTransformation * pTransf = GetTransformationRef(iNodeIndex);	// must be less than m_iNumMeshes
					if(!ParseTransformation(pTransf, fptr)) return false;
					++iNodeIndex;
				}
			}
			break;
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}

	return true;
}

bool CYifSceneReader::ParseTransformation(CYifTransformation * pTransf, FILE * fptr)
{
	bool bGotPositions = false;
	bool bGotQuaternions = false;
	bool bGotScaleValues = false;
	while(!IsChunkDone(fptr))
	{
		eCHUNK_TYPE eChunkID = PushChunk(fptr);
		switch(eChunkID)
		{
			case CHUNK_KCNT:
			{
				int iNumKeys;
				fread(&iNumKeys, sizeof(int), 1, fptr);
				if(!pTransf->SetNumKeys(iNumKeys)) return false;
			}
			break;
			case CHUNK_DURA:
			{
				float fDuration=0.0f;
				fread(&fDuration, sizeof(float), 1, fptr);
				pTransf->SetDuration(fDuration);
			}
			break;
			case CHUNK_PMTX:
			{
				assert(pTransf->m_pLocToParent!=NULL);
				if(pTransf->m_pLocToParent!=NULL)
					fread(pTransf->m_pLocToParent, sizeof(float)*16, 1, fptr);
			}
			break;
			case CHUNK_QBUF:
			{
				int iNumKeys = pTransf->GetNumKeys();
				assert(pTransf->m_pQuaternions!=NULL && iNumKeys>0);
				if(pTransf->m_pQuaternions!=NULL && iNumKeys>0)
				{
					fread(pTransf->m_pQuaternions, sizeof(Quat), iNumKeys, fptr);
					bGotQuaternions = true;
				}
			}
			break;
			case CHUNK_PBUF:
			{
				int iNumKeys = pTransf->GetNumKeys();
				assert(pTransf->m_pPositions!=NULL && iNumKeys>0);
				if(pTransf->m_pPositions!=NULL && iNumKeys>0)
				{
					fread(pTransf->m_pPositions, sizeof(Vector3Unaligned), iNumKeys, fptr);
					bGotPositions = true;
				}
			}
			break;
			case CHUNK_SBUF:
			{
				int iNumKeys = pTransf->GetNumKeys();
				assert(pTransf->m_pScaleValues!=NULL && iNumKeys>0);
				if(pTransf->m_pScaleValues!=NULL && iNumKeys>0)
				{
					fread(pTransf->m_pScaleValues, sizeof(Vector3Unaligned), iNumKeys, fptr);
					bGotScaleValues = true;
				}
			}
			break;
			case CHUNK_CHIX:
			{
				int iIndexToFirstChild = 0;
				fread(&iIndexToFirstChild, sizeof(int), 1, fptr);
				pTransf->SetFirstChild(iIndexToFirstChild);
			}
			break;
			case CHUNK_SIIX:
			{
				int iIndexToNextSibling = 0;
				fread(&iIndexToNextSibling, sizeof(int), 1, fptr);
				pTransf->SetNextSibling(iIndexToNextSibling);
			}
			break;
			default:
				SkipChunk(fptr);
		}
		PopChunk(fptr);
	}

	if(pTransf->GetNumKeys()>1)
	{
		if(!bGotPositions) pTransf->ClearPositions();
		if(!bGotQuaternions) pTransf->ClearRot();
		if(!bGotScaleValues) pTransf->ClearScaleValues();
	}

	return true;
}


//---------------------------------------------------------------------
eCHUNK_TYPE IdentifyChunk(const char cChunk[]);

static eCHUNK_TYPE PushChunk(FILE * fptr, int * piChunk)
{
	eCHUNK_TYPE eChunkID = CHUNK_UNKNOWN;

	assert(g_iCurDepth<MAX_CHUNK_DEPTH);
	if(g_iCurDepth<MAX_CHUNK_DEPTH)
	{
		char cChunk[4];
		fread(cChunk, 1, 4, fptr);

		// identify chunk
		eChunkID = IdentifyChunk(cChunk);

		int iChunkSize;		// not including the chunkID nor the 4 bytes to the size itself
		fread(&iChunkSize, 1, 4, fptr);
		if(piChunk!=NULL) *piChunk = iChunkSize;
		
		g_piChunkStack[g_iCurDepth]=ftell(fptr)+iChunkSize;		// predicted end of chunk
		++g_iCurDepth;
	}

	return eChunkID;
}

static void PopChunk(FILE * fptr)
{
	assert(g_iCurDepth>0);
	assert(IsChunkDone(fptr));
	if(g_iCurDepth>0)
	{
		--g_iCurDepth;
		int iPredictedLocation = g_piChunkStack[g_iCurDepth];
		SCE_GNM_ASSERT( iPredictedLocation == ftell(fptr) );
	}
}

static bool IsChunkDone(FILE * fptr)
{
	assert(g_iCurDepth>0);
	if(g_iCurDepth<1) return true;
	const int iEndOfChunk = g_piChunkStack[g_iCurDepth-1];
	const int iCurLocation = ftell(fptr);
	assert(iCurLocation<=iEndOfChunk);
	return iCurLocation>=iEndOfChunk;
}

static void SkipChunk(FILE * fptr)
{
	assert(g_iCurDepth>0);
	if(g_iCurDepth>0)
	{
		const int iEndOfChunk = g_piChunkStack[g_iCurDepth-1];
		fseek(fptr, iEndOfChunk, SEEK_SET);
	}
}


eCHUNK_TYPE IdentifyChunk(const char cChunk[])
{
	return (eCHUNK_TYPE) MAKE_ID(cChunk[0], cChunk[1], cChunk[2], cChunk[3]);
}
