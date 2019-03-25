/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

extern float16 mtx_axis_mesh[1];
extern MeshDeclaration axis_mesh_mate_PC;
extern float16 mtx_compass_mesh[1];
extern MeshDeclaration compass_mesh_mate_PC;


float16 mtx_axis_mesh[1] =
{
	{
		{
			1.000000f, 0.000000f, 0.000000f, 0.000000f,
			0.000000f, 1.000000f, 0.000000f, 0.000000f,
			0.000000f, 0.000000f, 1.000000f, 0.000000f,
			0.000000f, 0.000000f, 0.000000f, 1.000000f,
		}
	}
};

int ofs_axis_mesh[1] =
{
	-1,
};

MeshDeclaration axis_mesh_mate_PC =
{
	"axis_mesh_mate_PC.bin",
	432,
	432,
	16,
	0,
	-1,
	12,
	-1,
	-1,
	-1,
	-1,
	-1,
	NULL,
	0,
	mtx_axis_mesh,
	ofs_axis_mesh,
	1,
};

float16 mtx_compass_mesh[1] =
{
	{
		{
			1.000000f, 0.000000f, 0.000000f, 0.000000f,
			0.000000f, 1.000000f, 0.000000f, 0.000000f,
			0.000000f, 0.000000f, 1.000000f, 0.000000f,
			3.000000f, 0.000000f, 0.000000f, 1.000000f,
		}
	}
};

int ofs_compass_mesh[1] =
{
	-1,
};

MeshDeclaration compass_mesh_mate_PC =
{
	"compass_mesh_mate_PC.bin",
	234,
	234,
	16,
	0,
	-1,
	12,
	-1,
	-1,
	-1,
	-1,
	-1,
	NULL,
	0,
	mtx_compass_mesh,
	ofs_compass_mesh,
	1,
};

MeshDeclaration *gAxis_MeshDecTable[] = {
	&axis_mesh_mate_PC,
	&compass_mesh_mate_PC,
};
const int gAxis_NumMeshes = sizeof(gAxis_MeshDecTable)/sizeof(gAxis_MeshDecTable[0]);
