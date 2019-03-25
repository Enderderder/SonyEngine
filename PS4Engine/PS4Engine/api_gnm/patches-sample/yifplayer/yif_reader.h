/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __YIFREADER_H__
#define __YIFREADER_H__

#include "yif.h"


class CYifSceneReader : public CYifScene
{
public:
	bool ReadScene(const char file[]);

private:
	bool ParseTransformations(FILE * fptr);
	bool ParseTransformation(CYifTransformation * pTransf, FILE * fptr);

	bool ParseMeshInstances(FILE * fptr);
	bool ParseMeshInstance(CYifMeshInstance * pMeshInst, FILE * fptr);

	bool ParseMeshes(FILE * fptr);
	bool ParseMesh(CYifMesh * pMesh, FILE * fptr);
	bool ParseSurfaces(CYifMesh * pMesh, FILE * fptr);
	bool ParseSurface(CYifSurface * pSurf, FILE * fptr);
};

#endif