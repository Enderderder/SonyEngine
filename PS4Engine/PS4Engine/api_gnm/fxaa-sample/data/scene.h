/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

//extern float16 mtx_BackGround_mesh[1];
//extern MeshDeclaration BackGround_mesh_Italy_Building_A_dirty_PNU;
//extern float16 mtx_Building_mesh[1];
//extern MeshDeclaration Building_mesh_Italy_Building_A_dirty_PNU;
//extern float16 mtx_Ground_mesh[1];
//extern MeshDeclaration Ground_mesh_Italy_Ground_metalL_PNU;
//extern float16 mtx_Macho_mesh[1];
//extern MeshDeclaration Macho_mesh_Italy_Macho_A_metalH_PNU;
//extern MeshDeclaration Macho_mesh_Italy_Macho_B_metalH_PNU;
//extern MeshDeclaration Macho_mesh_Italy_Macho_C_metalH_PNU;
//extern float16 mtx_OBJ_mesh[1];
//extern MeshDeclaration OBJ_mesh_Italy_Ground_metalL_PNU;
//extern MeshDeclaration OBJ_mesh_Italy_Obj_dirty_PNU;
//extern MeshDeclaration OBJ_mesh_Italy_Obj_MetalL_PNU;
//extern float16 mtx_Sky_mesh[1];
//extern MeshDeclaration Sky_mesh_Italy_sky_PU;
//extern float16 mtx_Water_mesh[1];
//extern MeshDeclaration Water_mesh_Italy_Water_metalH_PNU;
//extern float16 mtx_fan_mesh[1];
//extern MeshDeclaration fan_mesh_Italy_Macho_metalH_PNU;

struct float16
{
	float val[16];

	float &operator [](int i) { return val[i]; }
	const float &operator [](int i) const { return val[i]; }

	operator float*() { return val; }
	operator const float*() const { return val; }
};

struct Bone
{
	const char   *name;			
	int          animeOffset;	
	float16      bindMatrix;	
	float16		 poseMatrix;	
	uint8_t m_reserved0[4];
};

struct MeshDeclaration
{
	const char   *fileName;
	unsigned int numVertices;
	unsigned int numIndices;
	unsigned int stride;
	int			 position;
	int			 normal;
	int			 color;
	int			 texcoord;
	int			 tangent;
	int			 binormal;
	int			 index;
	int			 weight;

	int          numBones;
	Bone         *bones;

	float16      *mtxInstance;
	int          *animeOffsets;
	unsigned int numInstance;
	uint8_t m_reserved0[4];
};

struct Mesh
{
	Mesh() :
		meshDeclaration(NULL),
		materialIndex(-1)
	{
	}

	const MeshDeclaration	*meshDeclaration;

	void *m_vertexBuffer;

	void *m_indexBuffer;

	void *m_fetchShader;

	int					materialIndex;
	uint8_t m_reserved0[4];
};


float16 mtx_BackGround_mesh[1] =
{
	{
		{
		1.000000f, 0.000000f, 0.000000f, 0.000000f,
		0.000000f, 1.000000f, 0.000000f, 0.000000f,
		0.000000f, 0.000000f, 1.000000f, 0.000000f,
		0.000000f, -0.000000f, 0.000000f, 1.000000f,
		}
	}
};

int ofs_BackGround_mesh[1] =
{
	-1,
};

MeshDeclaration BackGround_mesh_Italy_Building_A_dirty_PNU =
{
	"BackGround_mesh_Italy_Building_A_dirty_PNU.mesh",
	20772,
	20772,
	40,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_BackGround_mesh,
	ofs_BackGround_mesh,
	1,
};

float16 mtx_Building_mesh[1] =
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

int ofs_Building_mesh[1] =
{
	-1,
};

MeshDeclaration Building_mesh_Italy_Building_A_dirty_PNU =
{
	"Building_mesh_Italy_Building_A_dirty_PNU.mesh",
	53022,
	70638,
	40,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_Building_mesh,
	ofs_Building_mesh,
	1,
};

float16 mtx_Ground_mesh[1] =
{
	{
		{
			0.000000f, 0.000000f, 1.000000f, 0.000000f,
			0.000000f, 1.000000f, 0.000000f, 0.000000f,
			-1.000000f, 0.000000f, 0.000000f, 0.000000f,
			0.000000f, 0.000000f, 0.000000f, 1.000000f,
		},
	}
};

int ofs_Ground_mesh[1] =
{
	-1,
};

MeshDeclaration Ground_mesh_Italy_Ground_metalL_PNU =
{
	"Ground_mesh_Italy_Ground_metalL_PNU.mesh",
	2910,
	4038,
	40,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_Ground_mesh,
	ofs_Ground_mesh,
	1,
};

float16 mtx_Macho_mesh[1] =
{
	{
		{
			1.000000f, 0.000000f, 0.000000f, 0.000000f,
			0.000000f, 1.000000f, 0.000000f, 0.000000f,
			0.000000f, 0.000000f, 1.000000f, 0.000000f,
			0.000000f, 0.000000f, -0.000000f, 1.000000f,
		}
	}
};

int ofs_Macho_mesh[1] =
{
	-1,
};

MeshDeclaration Macho_mesh_Italy_Macho_A_metalH_PNU =
{
	"Macho_mesh_Italy_Macho_A_metalH_PNU.mesh",
	7688,
	11004,
	40,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_Macho_mesh,
	ofs_Macho_mesh,
	1,
};

MeshDeclaration Macho_mesh_Italy_Macho_B_metalH_PNU =
{
	"Macho_mesh_Italy_Macho_B_metalH_PNU.mesh",
	1984,
	2880,
	40,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_Macho_mesh,
	ofs_Macho_mesh,
	1,
};

MeshDeclaration Macho_mesh_Italy_Macho_C_metalH_PNU =
{
	"Macho_mesh_Italy_Macho_C_metalH_PNU.mesh",
	3312,
	5628,
	40,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_Macho_mesh,
	ofs_Macho_mesh,
	1,
};

float16 mtx_OBJ_mesh[1] =
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

int ofs_OBJ_mesh[1] =
{
	-1,
};

MeshDeclaration OBJ_mesh_Italy_Ground_metalL_PNU =
{
	"OBJ_mesh_Italy_Ground_metalL_PNU.mesh",
	2328,
	3096,
	40,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_OBJ_mesh,
	ofs_OBJ_mesh,
	1,
};

MeshDeclaration OBJ_mesh_Italy_Obj_dirty_PNU =
{
	"OBJ_mesh_Italy_Obj_dirty_PNU.mesh",
	1372,
	6048,
	40,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_OBJ_mesh,
	ofs_OBJ_mesh,
	1,
};

MeshDeclaration OBJ_mesh_Italy_Obj_MetalL_PNU =
{
	"OBJ_mesh_Italy_Obj_MetalL_PNU.mesh",
	13392,
	19680,
	40,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_OBJ_mesh,
	ofs_OBJ_mesh,
	1,
};

float16 mtx_Sky_mesh[1] =
{
	{
		{
			1.000000f, 0.000000f, 0.000000f, 0.000000f,
			0.000000f, 1.000000f, 0.000000f, 0.000000f,
			0.000000f, 0.000000f, 1.000000f, 0.000000f,
			0.000000f, 0.000000f, 0.000001f, 1.000000f,
		}
	}
};

int ofs_Sky_mesh[1] =
{
	-1,
};

MeshDeclaration Sky_mesh_Italy_sky_PU =
{
	"Sky_mesh_Italy_sky_PU.mesh",
	425,
	1728,
	20,
	0,
	-1,
	-1,
	12,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_Sky_mesh,
	ofs_Sky_mesh,
	1,
};

float16 mtx_Water_mesh[1] =
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

int ofs_Water_mesh[1] =
{
	-1,
};

MeshDeclaration Water_mesh_Italy_Water_metalH_PNU =
{
	"Water_mesh_Italy_Water_metalH_PNU.mesh",
	252,
	252,
	32,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_Water_mesh,
	ofs_Water_mesh,
	1,
};

float16 mtx_fan_mesh[1] =
{
	{
		{
			1.000000f, 0.000000f, 0.000000f, 0.000000f,
			0.000000f, 1.000000f, 0.000000f, 0.000000f,
			0.000000f, 0.000000f, 1.000000f, 0.000000f,
			0.000000f, 4.125000f, 0.000000f, 1.000000f,
		}
	}
};

int ofs_fan_mesh[1] =
{
	-1,
};

MeshDeclaration fan_mesh_Italy_Macho_metalH_PNU =
{
	"fan_mesh_Italy_Macho_metalH_PNU.mesh",
	720,
	1080,
	32,
	0,
	12,
	-1,
	24,
	-1,
	-1,
	-1,
	-1,
	0,
	NULL,
	mtx_fan_mesh,
	ofs_fan_mesh,
	1,
};

MeshDeclaration *gScene_MeshDecTable[] = {
	&BackGround_mesh_Italy_Building_A_dirty_PNU,
	&Building_mesh_Italy_Building_A_dirty_PNU,
	&Ground_mesh_Italy_Ground_metalL_PNU,
	&Macho_mesh_Italy_Macho_A_metalH_PNU,
	&Macho_mesh_Italy_Macho_B_metalH_PNU,
	&Macho_mesh_Italy_Macho_C_metalH_PNU,
	&OBJ_mesh_Italy_Ground_metalL_PNU,
	&OBJ_mesh_Italy_Obj_MetalL_PNU,
	&OBJ_mesh_Italy_Obj_dirty_PNU,
	&Sky_mesh_Italy_sky_PU,
	&Water_mesh_Italy_Water_metalH_PNU,
	&fan_mesh_Italy_Macho_metalH_PNU,
};
static const int gScene_NumMeshes = sizeof(gScene_MeshDecTable)/sizeof(gScene_MeshDecTable[0]);
