/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/frame.h"
#include "std_cbuffer.h"
using namespace sce;

namespace 
{
	uint32_t spinningTestVal;
	Matrix4 spinningRotation;

	void spin(float seconds)
	{
		Framework::Random random = {static_cast<uint64_t>(seconds)};
		spinningTestVal = random.GetUShort() & 0xFFF;
		const Vector3 axis[6] = {Vector3::xAxis(), Vector3::yAxis(), Vector3::zAxis(), -Vector3::xAxis(), -Vector3::yAxis(), -Vector3::zAxis()};
		const Vector3 spinningAxis = axis[random.GetUint() % 6];
		const float spinningAmount = seconds-(int)seconds;
		const float spinningRadians = float(Framework::smoothStep(0,1,Framework::smoothStep(0, 1, spinningAmount)) * 3.1415926535897932384626433832795*2);
		spinningRotation = Matrix4::rotation(spinningRadians, spinningAxis);
	}

	class Stencil
	{
	public:
		uint32_t m_compareFunc;
		uint32_t m_stencilFail;
		uint32_t m_stencilZPass;
		uint32_t m_stencilZFail;
		Gnm::StencilControl m_stencilControl;
		Stencil()
		: m_compareFunc(7)
		, m_stencilFail(0)
		, m_stencilZPass(0)
		, m_stencilZFail(0)
		{
			m_stencilControl.m_testVal = 1;
			m_stencilControl.m_opVal = 1;
			m_stencilControl.m_mask = 0xFF;
			m_stencilControl.m_writeMask = 0xFF;
		}		
	};
	
	Vector2 g_cubePosition = Vector2(0,0);
	int32_t g_clearVal = 0;
	bool g_separateStencilEnable;
	Stencil g_stencilFront;
	Stencil g_stencilBack;

	Framework::MenuItemText stencilOpText[16] = {
		{"Keep",        "Dest = src"},
		{"Zero",        "Dest = 0x00"},
		{"Ones",        "Dest = 0xFF"},
		{"ReplaceTest", "Dest = testVal"},
		{"ReplaceOp",   "Dest = opVal"},
		{"AddClamp",    "Dest = src + opVal (clamp)"},
		{"SubClamp",    "Dest = src - opVal (clamp)"},
		{"Invert",      "Dest = ~src"},
		{"AddWrap",     "Dest = src + opVal (wrap)"},
		{"SubWrap",     "Dest = src - opVal (wrap)"},
		{"And",         "Dest = src & opVal"},
		{"Or",          "Dest = src | opVal"},
		{"Xor",         "Dest = src ^ opVal"},
		{"Nand",        "Dest = ~(src & opVal)"},
		{"Nor",         "Dest = ~(src | opVal)"},
		{"Xnor",        "Dest = ~(src ^ opVal)"},
	};

	Framework::MenuItemText compareFuncText[8] = 
	{
		{"Never",    "Test never passes"}, 
		{"Less",     "Test passes if incoming value is <"}, 
		{"Equal",    "Test passes if incoming value is =="},
		{"LEqual",   "Test passes if incoming value is <="}, 
		{"Greater",  "Test passes if incoming value is >"}, 
		{"NotEqual", "Test passes if incoming value is !="}, 
		{"GEqual",   "Test passes if incoming value is >="}, 
		{"Always",   "Test always passes"},
	};

	class MenuCommandStencil : public Framework::MenuCommand
	{
		Stencil* m_stencil;
		Framework::Menu m_menu;
		Framework::MenuCommandEnum m_compareFunc;
		Framework::MenuCommandUint8 m_testVal;
		Framework::MenuCommandUint8 m_opVal;
		Framework::MenuCommandUint8 m_mask;
		Framework::MenuCommandUint8 m_writeMask;
		Framework::MenuCommandEnum m_stencilFail;
		Framework::MenuCommandEnum m_stencilZPass;
		Framework::MenuCommandEnum m_stencilZFail;
	public:
		MenuCommandStencil(Stencil* stencil)
		: m_stencil(stencil)
		, m_compareFunc(compareFuncText, sizeof(compareFuncText)/sizeof(compareFuncText[0]), &m_stencil->m_compareFunc)
		, m_testVal(&m_stencil->m_stencilControl.m_testVal, 0, 0xFF, "0x%02X")
		, m_opVal(&m_stencil->m_stencilControl.m_opVal, 0, 0xFF, "0x%02X")
		, m_mask(&m_stencil->m_stencilControl.m_mask, 0, 0xFF, "0x%02X")
		, m_writeMask(&m_stencil->m_stencilControl.m_writeMask, 0, 0xFF, "0x%02X")
		, m_stencilFail(stencilOpText, sizeof(stencilOpText)/sizeof(stencilOpText[0]), &m_stencil->m_stencilFail)
		, m_stencilZPass(stencilOpText, sizeof(stencilOpText)/sizeof(stencilOpText[0]), &m_stencil->m_stencilZPass)
		, m_stencilZFail(stencilOpText, sizeof(stencilOpText)/sizeof(stencilOpText[0]), &m_stencil->m_stencilZFail)
		{
			const Framework::MenuItem menuItem[] =
			{
				{{"CompareFunc", "Comparison function for stencil test"}, &m_compareFunc},
				{{"TestVal", "TestVal for stencil test"}, &m_testVal},
				{{"OpVal", "OpVal for stencil test"}, &m_opVal},
				{{"Mask", "Mask for stencil test"}, &m_mask},
				{{"WriteMask", "Mask for stencil write"}, &m_writeMask},
				{{"StencilFail", "Op for when stencil fails"}, &m_stencilFail},
				{{"StencilZPass", "Op for when Z passes"}, &m_stencilZPass},
				{{"StencilZFail", "Op for when Z fails"}, &m_stencilZFail},
			};
			m_menu.appendMenuItems(sizeof(menuItem)/sizeof(menuItem[0]), &menuItem[0]);
		}
		virtual Framework::Menu* getMenu()
		{
			return &m_menu;
		}
		virtual void adjust(int) const {}
	};

	Framework::MenuCommandVector2 iceCube(&g_cubePosition, Vector2(-1,-1), Vector2(1,1), Vector2(1,-1));
	Framework::MenuCommandInt clearVal(&g_clearVal, 0, 0xFF, "0x%02X");
	Framework::MenuCommandBool separateStencilEnable(&g_separateStencilEnable);
	MenuCommandStencil stencilFront(&g_stencilFront);
	MenuCommandStencil stencilBack(&g_stencilBack);

	const Framework::MenuItem menuItem[] =
	{
		{{"ICE Cube Controls", "Use the left analog stick to move the ICE cube around the world. As the ICE cube passes in front of stencil cubes, it interacts with them according to the rules of stencil testing hardware"}, &iceCube},
		{{"ClearVal", "Adjust the value which the stencil buffer is cleared to, before either the stencil cubes or ICE cubes are rendered"}, &clearVal},
		{{"Separate Front and Back", "It is possible to specify different stencil options for front-facing primitives and back-facing primitives. Unless this option is enabled, the stencil test options for front-facing primitives will be used for back-facing primitives, too"}, &separateStencilEnable},
		{{"Front-Facing Options", "These are the stencil options for front-facing primitives (or all primitives, if separate front and back options are not enabled.) They include stencil test AND stencil write options"}, &stencilFront},
		{{"Back-Facing Options", "These are the stencil options for back-facing primitives (only if separate front and back options are enabled.) They include stencil test AND stencil write options"}, &stencilBack},
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };

	Framework::GnmSampleFramework framework;
}

int main(int argc, const char *argv[])
{
	framework.m_config.m_lightPositionX = Vector3(0,0,1.3f);
	const Vector3 ideal(1.f, 0.f, 0.f);
	const Vector3 forward = normalize(framework.m_config.m_lightTargetX - framework.m_config.m_lightPositionX);
	framework.m_config.m_lightUpX = normalize(ideal - forward * dot(forward, ideal));

	framework.processCommandLine(argc, argv);

	framework.m_config.m_stencil = true;
	framework.m_config.m_lightingIsMeaningful = true;

	framework.initialize( "Stencil", 
		"Sample code to illustrate the operation of stencil hardware functions.",
		"This sample program renders 256 cubes, each with a different stencil value. Then, another cube "
		"is rendered on top of them with user-specified stencil testing parameters.");

	framework.appendMenuItems(kMenuItems, menuItem);

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Constants *m_box;
		Constants *m_iceCube;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_box,SCE_KERNEL_WB_ONION,sizeof(*frame->m_box)*256,4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d box constant buffers",buffer);
		framework.m_allocators.allocate((void**)&frame->m_iceCube,SCE_KERNEL_WB_ONION,sizeof(*frame->m_iceCube),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d ICE cube constant buffer",buffer);
	}

	int error = 0;

	Framework::VsShader vertexShader;
	Framework::PsShader pixelShader, simplePixelShader;

	error = Framework::LoadVsMakeFetchShader(&vertexShader, Framework::ReadFile("/app0/stencil-sample/shader_vv.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&pixelShader, Framework::ReadFile("/app0/stencil-sample/shader_p.sb"), &framework.m_allocators);
	error = Framework::LoadPsShader(&simplePixelShader, Framework::ReadFile("/app0/stencil-sample/simple_p.sb"), &framework.m_allocators);

	const Vector3 scale( (float)framework.m_config.m_targetWidth/2, -(float)framework.m_config.m_targetHeight/2, 0.5f );
	const Vector3 offset( (float)framework.m_config.m_targetWidth/2, (float)framework.m_config.m_targetHeight/2, 0.5f );

	Gnm::Texture textures[2];
	error = Framework::loadTextureFromGnf(&textures[0], Framework::ReadFile("/app0/assets/icelogo-color.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );
	error = Framework::loadTextureFromGnf(&textures[1], Framework::ReadFile("/app0/assets/icelogo-normal.gnf"), 0, &framework.m_allocators);
    SCE_GNM_ASSERT(error == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare as read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as an RWTexture, so it's safe to declare as read-only.

	Gnm::Sampler samplers[2];
	for(uint32_t i=0; i<2; ++i)
	{
		samplers[i].init();
		samplers[i].setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);	
		samplers[i].setMipFilterMode(Gnm::kMipFilterModeLinear);
		samplers[i].setAnisotropyRatio(Gnm::kAnisotropyRatio16);
	}

	Framework::SimpleMesh cubeMesh;
	BuildCubeMesh(&framework.m_allocators, "Cube", &cubeMesh, 1.0f);

	enum {kCharactersPerCellWide = 4};
	enum {kCharactersPerCellHigh = 3};
	const uint32_t kNumberTextureCellWidth = (1 << framework.m_config.m_log2CharacterWidth) * kCharactersPerCellWide;
	const uint32_t kNumberTextureCellHeight = (1 << framework.m_config.m_log2CharacterHeight) * kCharactersPerCellHigh;
	const uint32_t kNumberTextureWidth = kNumberTextureCellWidth * 16;
	const uint32_t kNumberTextureHeight = kNumberTextureCellHeight * 16;

	// Make a texture to hold an image of the hex values 00 to FF, for use in labeling objects that write stencil with the stencil value they write

	Gnm::Texture numberTexture;
	Gnm::TileMode numberTextureTileMode;
	Gnm::DataFormat numberTextureFormat = Gnm::kDataFormatR8G8B8A8Unorm;

	uint32_t mips = 1;
	uint32_t pixels = std::max(uint32_t(kNumberTextureWidth), uint32_t(kNumberTextureHeight));
	while(pixels >>= 1)
		++mips;

	GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &numberTextureTileMode, GpuAddress::kSurfaceTypeRwTextureFlat, numberTextureFormat, 1);
	Gnm::SizeAlign sizeAlign = Gnmx::Toolkit::initAs2d(&numberTexture, kNumberTextureWidth, kNumberTextureHeight, mips, numberTextureFormat, numberTextureTileMode, Gnm::kNumFragments1);
	void *numberTextureAddress;
	framework.m_allocators.allocate(&numberTextureAddress, SCE_KERNEL_WC_GARLIC, sizeAlign, Gnm::kResourceTypeTextureBaseAddress, "Number Texture");
	numberTexture.setBaseAddress(numberTextureAddress);
	numberTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // this texture is bound as an RWTexture, so GPU coherent it is.

	uint32_t screenWidthInCharacters = 1024;
	uint32_t screenHeightInCharacters = 1024;
	DbgFont::Screen screen;
	Gnm::SizeAlign screenSizeAlign = screen.calculateRequiredBufferSizeAlign(screenWidthInCharacters, screenHeightInCharacters);
	void *screenAddr;
	framework.m_allocators.allocate(&screenAddr, SCE_KERNEL_WB_ONION, screenSizeAlign, Gnm::kResourceTypeBufferBaseAddress, "Character Cell Screen");
	screen.initialize(screenAddr, screenWidthInCharacters, screenHeightInCharacters);
	DbgFont::Window window;
	window.initialize(&screen);

	DbgFont::Cell clearCell;
	clearCell.m_character       = 0;
	clearCell.m_foregroundColor = DbgFont::kWhite;
	clearCell.m_foregroundAlpha = 15;
	clearCell.m_backgroundColor = DbgFont::kBlack;
	clearCell.m_backgroundAlpha = 0;
	window.clear(clearCell);
	for(uint32_t y=0; y<16; ++y)
		for(uint32_t x=0; x<16; ++x)
		{
			window.setCursor(x * kCharactersPerCellWide + 1, y * kCharactersPerCellHigh + 1);
			window.printf("%02X", y*16+x);
		}

	const Vector4 darkOliveGreen = Vector4(0x55, 0x6B, 0x2F, 0xFF) / 255.f;

	{
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
		gfxc->reset();
		Gnmx::Toolkit::SurfaceUtil::clearTexture(*gfxc, &numberTexture, darkOliveGreen);
		screen.render(*gfxc, &numberTexture, framework.m_config.m_log2CharacterWidth, framework.m_config.m_log2CharacterHeight);
		Gnmx::Toolkit::SurfaceUtil::generateMipMaps(*gfxc, &numberTexture);
		Gnmx::Toolkit::submitAndStall(*gfxc);
	}

	framework.SetViewToWorldMatrix(inverse(Matrix4::lookAt(Point3(0,0,1.3f), Point3(0,0,0), Vector3(-1,0,0))));

	while (!framework.m_shutdownRequested)
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
		framework.BeginFrame(*gfxc);

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace( Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace( Gnm::kPrimitiveSetupFrontFaceCcw );
		gfxc->setPrimitiveSetup( primSetupReg );

		Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
		Gnmx::Toolkit::SurfaceUtil::clearDepthStencilTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f, (uint8_t)g_clearVal);
		gfxc->setRenderTargetMask(0xF);

		gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
		gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);

		gfxc->setupScreenViewport( 0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f );

		gfxc->setVsShader(vertexShader.m_shader, 0, vertexShader.m_fetchShader, &vertexShader.m_offsetsTable);
		gfxc->setPsShader(simplePixelShader.m_shader, &simplePixelShader.m_offsetsTable);

		cubeMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);
		gfxc->setPrimitiveType(cubeMesh.m_primitiveType);
		gfxc->setIndexSize(cubeMesh.m_indexType);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 1, &numberTexture);
		gfxc->setTextures(Gnm::kShaderStagePs, 1, 1, &textures[1]);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 2, samplers);

		Gnm::DepthStencilControl depthControl;
		depthControl.init();
		Gnm::StencilOpControl stencilControl;
		stencilControl.init();

		depthControl.setDepthControl( Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual );
		depthControl.setDepthEnable(true);
		depthControl.setStencilEnable(true);
		stencilControl.setStencilOps(Gnm::kStencilOpReplaceTest, Gnm::kStencilOpReplaceTest, Gnm::kStencilOpReplaceTest);
		depthControl.setStencilFunction( Gnm::kCompareFuncAlways );
		gfxc->setDepthStencilControl( depthControl );
		gfxc->setStencilOpControl( stencilControl );

		spin(framework.GetSecondsElapsedApparent());

		for( uint32_t testVal=0; testVal<256; ++testVal )
		{
			Constants *constants = &frame->m_box[testVal];
			Gnm::Buffer constantBuffer;
			constantBuffer.initAsConstantBuffer(constants, sizeof(*constants));
			constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);
		
			const Matrix4 translation = Matrix4::translation(Vector3((testVal/16)/7.5f-1.f,(testVal%16)/7.5f-1.f,0));
			const Matrix4 scale = Matrix4::scale( Vector3(0.1f,0.1f,0.1f) );

			Matrix4 rotation = Matrix4::identity();
			if(testVal == spinningTestVal)
				rotation = spinningRotation;

			const Matrix4 model = translation * scale * rotation;

			constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*model);
			constants->m_modelView = transpose(framework.m_worldToViewMatrix*model);
			constants->m_lightColor = framework.getLightColor();
			constants->m_ambientColor = framework.getAmbientColor();
			constants->m_lightAttenuation = Vector4(1, 0, 0, 0);
			constants->m_lightPosition = framework.getLightPositionInViewSpace();			
			float testValX = float((testVal>>0) & 0xF);
			float testValY = float((testVal>>4) & 0xF);
			constants->m_uv = transpose(Matrix4::scale(Vector3(kNumberTextureCellWidth/(float)kNumberTextureWidth, kNumberTextureCellHeight/(float)kNumberTextureHeight,1)) * Matrix4::translation(Vector3(testValX, testValY, 0)));

			Gnm::StencilControl stencilControl;
			stencilControl.m_testVal = static_cast<uint8_t>(testVal);
			stencilControl.m_mask = 0xFF;
			stencilControl.m_writeMask = 0xFF;
			stencilControl.m_opVal = static_cast<uint8_t>(testVal);
			gfxc->setStencil(stencilControl);
			gfxc->drawIndex(cubeMesh.m_indexCount, cubeMesh.m_indexBuffer);
		}

		gfxc->setPsShader(pixelShader.m_shader, &pixelShader.m_offsetsTable);			
		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);

		Constants *constants = frame->m_iceCube;
		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(constants, sizeof(*constants) );
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &constantBuffer);
		
		const Matrix4 translation = Matrix4::translation(Vector3(-g_cubePosition.getY(), g_cubePosition.getX(), 0.f));
		const Matrix4 scale = Matrix4::scale( Vector3(0.2f,0.2f,0.2f) );
		const Matrix4 model = translation * scale;

		constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*model);
		constants->m_modelView = transpose(framework.m_worldToViewMatrix*model);
		constants->m_lightColor = framework.getLightColor();
		constants->m_ambientColor = framework.getAmbientColor();
		constants->m_lightAttenuation = Vector4(1, 0, 0, 0);
		constants->m_lightPosition = framework.m_worldToViewMatrix * Vector4(-framework.m_worldToLightMatrix.getCol3().getXYZ(),1.f);
		constants->m_uv = Matrix4::identity();

		gfxc->setStencilSeparate(g_stencilFront.m_stencilControl, g_stencilBack.m_stencilControl);

		stencilControl.setStencilOps((Gnm::StencilOp)g_stencilFront.m_stencilFail, (Gnm::StencilOp)g_stencilFront.m_stencilZPass, (Gnm::StencilOp)g_stencilFront.m_stencilZFail);
		stencilControl.setStencilOpsBack((Gnm::StencilOp)g_stencilBack.m_stencilFail, (Gnm::StencilOp)g_stencilBack.m_stencilZPass, (Gnm::StencilOp)g_stencilBack.m_stencilZFail);
		depthControl.setStencilFunction((Gnm::CompareFunc)g_stencilFront.m_compareFunc);
		depthControl.setStencilFunctionBack((Gnm::CompareFunc)g_stencilBack.m_compareFunc);
		depthControl.setSeparateStencilEnable(g_separateStencilEnable);
		gfxc->setDepthStencilControl( depthControl );
		gfxc->setStencilOpControl( stencilControl );

		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceNone);
		gfxc->setPrimitiveSetup(primSetupReg);
		gfxc->drawIndex(cubeMesh.m_indexCount, cubeMesh.m_indexBuffer);

		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		gfxc->setPrimitiveSetup(primSetupReg);

		{
			Gnm::DepthStencilControl depthControl;
			depthControl.init();
			depthControl.setDepthControl( Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual );
			depthControl.setDepthEnable(true);
			gfxc->setDepthStencilControl( depthControl );
		}

		framework.EndFrame(*gfxc);
	}

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	Gnmx::GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
    return 0;
}
