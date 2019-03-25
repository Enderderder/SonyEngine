/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "sample_framework.h"
#include <pm4_dump.h>
#include <unistd.h>
#include "../toolkit/embedded_shader.h"
#include "gnf_loader.h"
#include <video_out.h>
#include <sys/sce_errno.h>
#include "../toolkit/allocators.h"
#include "debug_objects.h"

using namespace sce;

#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
#include <libsysmodule.h>
#include <razor_gpu_thread_trace.h>
#pragma comment(lib,"libSceSysmodule_stub_weak.a")
#pragma comment(lib,"libSceRazorGpuThreadTrace_stub_weak.a")
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE

// Set default heap size
size_t sceLibcHeapSize = 512 * 1024 * 1024; // 512MB for Razor GPU
unsigned int sceLibcHeapExtendedAlloc = 1; // Enable

namespace // Private Functions
{
    static const uint32_t k4kWidth     = 3840;
    static const uint32_t k4kHeight    = 2160;

    static const uint32_t k1080pWidth  = 1920;
    static const uint32_t k1080pHeight = 1080;

    static const uint32_t k720pWidth   = 1280;
    static const uint32_t k720pHeight  =  720;

	enum FrameworkDisplayBuffer
	{
		kFrameworkDisplayBufferColor,
		kFrameworkDisplayBufferAlpha,
		kFrameworkDisplayBufferDepth,
		kFrameworkDisplayBufferStencil,
		kFrameworkDisplayBufferCount
	};

	uint32_t s_color[] =
	{
	  0xFF000000,
	  0xFF0000FF,
	  0xFF00FF00,
	  0xFF00FFFF,
	  0xFFFF0000,
	  0xFFFF00FF,
	  0xFFFFFF00,
	  0xFFFFFFFF,
	};

	uint64_t performanceCounter()
	{
		uint64_t result = 0;
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		result = (uint64_t)ts.tv_sec * 1000000000LL + (uint64_t)ts.tv_nsec;
		return result;
	}

	uint64_t performanceFrequency()
	{
		uint64_t result = 1;
		result = 1000000000LL;
		return result;
	}

	const int kMicrosecondsPerFrame = 0;

	enum Type
	{
		kUint,
		kVector3,
		kString,
		kBool,
		kFloat,
	};

	struct Variant
	{
		Type m_type;
		union Item
		{	
			const char *m_s;
			uint32_t m_u;
			struct {Vector3 m_v;};
			bool m_b;
			float m_f;
			Item() {}
		};
		union Member
		{
			const char *Framework::ConfigData::*m_s;
			uint32_t Framework::ConfigData::*m_u;
			Vector3 Framework::ConfigData::*m_v;
			bool Framework::ConfigData::*m_b;
			float Framework::ConfigData::*m_f;
		};
		uint8_t m_reserved0[4];
		Member m_member;
		Item m_default;
		Item m_min;
		Item m_max;
		Variant(const Variant &rhs)
		{
			memcpy(this, &rhs, sizeof(rhs));
		}
		Variant(const char *Framework::ConfigData::* m, const char *a)
		{
			m_type = kString;
			m_member.m_s = m;
			m_default.m_s = a;
		}
		Variant(uint32_t Framework::ConfigData::* m, uint32_t a, uint32_t b, uint32_t c)
		{
			m_type = kUint;
			m_member.m_u = m;
			m_default.m_u = a;
			m_min.m_u = b;
			m_max.m_u = c;
		}
		Variant(Vector3 Framework::ConfigData::* m, Vector3 a, Vector3 b, Vector3 c)
		{
			m_type = kVector3;
			m_member.m_v = m;
			m_default.m_v = a;
			m_min.m_v = b;
			m_max.m_v = c;
		}
		Variant(bool Framework::ConfigData::* m, bool a)
		{
			m_type = kBool;
			m_member.m_b = m;
			m_default.m_b = a;
		}
		Variant(float Framework::ConfigData::* m, float a, float b, float c)
		{
			m_type = kFloat;
			m_member.m_f = m;
			m_default.m_f = a;
			m_min.m_f = b;
			m_max.m_f = c;
		}
	};

	enum class Documented
	{
		kNo,
		kYes
	};

	enum class RelatedToLighting
	{
		kNo,
		kYes
	};

	enum class Hidden
	{
		kNo,
		kYes
	};

	class Option
	{
	public:
		const char* m_name;
		uint8_t m_reserved0[8];
		Variant m_variant;
		const char* m_description;
		Documented m_documented;
		RelatedToLighting m_relatedToLighting;
		Hidden m_hidden;
		uint8_t m_reserved1[12];
		void setAsUint32(Framework::ConfigData* configData,const uint32_t value) const
		{
			SCE_GNM_ASSERT(m_variant.m_type == kUint);
			configData->*m_variant.m_member.m_u = std::max(m_variant.m_min.m_u, std::min(m_variant.m_max.m_u, value));
		}
		void setAsVector3(Framework::ConfigData* configData,const Vector3 value) const
		{
			SCE_GNM_ASSERT(m_variant.m_type == kVector3);
			configData->*m_variant.m_member.m_v = maxPerElem(m_variant.m_min.m_v, minPerElem(m_variant.m_max.m_v, value));		
		}
		void setAsString(Framework::ConfigData* configData,const char *value) const
		{
			switch(m_variant.m_type)
			{
			case kString:
				configData->*m_variant.m_member.m_s = value;
				break;
			case kUint:
				setAsUint32(configData, atoi(value));
				break;
			case kVector3: 
				{
					float x,y,z;
					sscanf(value,"%f,%f,%f",&x,&y,&z);
					const Vector3 vector3(x,y,z);
					setAsVector3(configData, vector3);
				}
				break;
			case kBool:
				if(0 == strcasecmp(value, "true") || 0 == strcasecmp(value, "1"))
					setAsBool(configData, true);
				if(0 == strcasecmp(value, "false") || 0 == strcasecmp(value, "0"))
					setAsBool(configData, false);
				break;
			case kFloat:
				setAsFloat(configData, atof(value));
				break;
			}
		}
		void setAsBool(Framework::ConfigData* configData,const bool value) const
		{
			SCE_GNM_ASSERT(m_variant.m_type == kBool);
			configData->*m_variant.m_member.m_b = value;
		}
		void setAsFloat(Framework::ConfigData* configData,const float value) const
		{
			SCE_GNM_ASSERT(m_variant.m_type == kFloat);
			configData->*m_variant.m_member.m_f = std::max(m_variant.m_min.m_f, std::min(m_variant.m_max.m_f, value));
		}
		void loadDefault(Framework::ConfigData* configData) const
		{
			switch(m_variant.m_type)
			{
			case kString:
				configData->*m_variant.m_member.m_s = m_variant.m_default.m_s;
				break;
			case kUint:
				configData->*m_variant.m_member.m_u = m_variant.m_default.m_u;
				break;
			case kVector3:
				configData->*m_variant.m_member.m_v = m_variant.m_default.m_v;
				break;
			case kBool:
				configData->*m_variant.m_member.m_b = m_variant.m_default.m_b;
				break;
			case kFloat:
				configData->*m_variant.m_member.m_f = m_variant.m_default.m_f;
				break;
			}
		}
	};

	const Option g_options[] =
	{
		{"screenshotFilename",      {}, Variant(&Framework::ConfigData::m_screenshotFilename, "/hostapp/screenshot0.gnf"),"filename to use when taking a screenshot"}, 
		{"menuFilename",            {}, Variant(&Framework::ConfigData::m_menuFilename, "/app0/menu.json"),"filename to use when writing out menu documentation"}, 
        {"renderingMode",           {}, Variant(&Framework::ConfigData::m_renderingMode, nullptr),"rendering mode to override user preference."},
		{"targetWidth",             {}, Variant(&Framework::ConfigData::m_targetWidth, 0, 0, k4kWidth), "width in pixels of the window or fullscreen display mode"}, 
		{"targetHeight",            {}, Variant(&Framework::ConfigData::m_targetHeight, 0, 0, k4kHeight), "height in pixels of the window or fullscreen display mode"}, 
		{"fullScreen",              {}, Variant(&Framework::ConfigData::m_fullScreen, false), "false:display in a window, true:display as fullscreen"}, 
		{"frames",                  {}, Variant(&Framework::ConfigData::m_frames, 0xFFFFFFFF, 0, 0xFFFFFFFF), "number of frames to render before automatically quitting"},
		{"seconds",                 {}, Variant(&Framework::ConfigData::m_seconds, FLT_MAX, 0.f, FLT_MAX), "number of seconds to run before automatically quitting"},
		{"buffers",                 {}, Variant(&Framework::ConfigData::m_buffers, 3, 2, 3), "number of render targets to allocate for round-robin display", Documented::kYes},
		{"dcc",                     {}, Variant(&Framework::ConfigData::m_dcc, false), "is there a DCC buffer for the render targets?", Documented::kYes},
		{"stencil",                 {}, Variant(&Framework::ConfigData::m_stencil, false), "is there a stencil buffer for the depth render targets?", Documented::kYes},
		{"htile",                   {}, Variant(&Framework::ConfigData::m_htile, true), "is there an htile buffer for the depth render targets?", Documented::kYes},
		{"screenshot",              {}, Variant(&Framework::ConfigData::m_screenshot, false), "false = no screenshot on exit, true = take a screenshot on exit"}, 
		{"microSecondsPerFrame",    {}, Variant(&Framework::ConfigData::m_microSecondsPerFrame, kMicrosecondsPerFrame, 0, 100000), "0=use OS clock, >0=use fixed microseconds per frame", Documented::kYes},
		{"displayBuffer",           {}, Variant(&Framework::ConfigData::m_displayBuffer, kFrameworkDisplayBufferColor, kFrameworkDisplayBufferColor, kFrameworkDisplayBufferStencil), "which buffer to display: the RGB channels of color buffer, alpha channel of color buffer, depth buffer, or stencil buffer", Documented::kYes},
		{"cameraControls",          {}, Variant(&Framework::ConfigData::m_cameraControls, true), "false: camera controls are disabled, true: camera controls are enabled", Documented::kYes},
		{"cameraControlMode",       {}, Variant(&Framework::ConfigData::m_cameraControlMode,0,0,1), "false: default camera controls, true: alternative camera controls (yaw/pitch only)", Documented::kNo},
		{"depthBufferIsMeaningful", {}, Variant(&Framework::ConfigData::m_depthBufferIsMeaningful, true), "false: final contents of depth buffer are not meaningful, true: final contents of depth buffer are meaningful", Documented::kNo, RelatedToLighting::kNo, Hidden::kYes},
		{"displayTimings",          {}, Variant(&Framework::ConfigData::m_displayTimings, true), "false: do not display frame timings, true: display frame timings", Documented::kYes},
		{"clearColor",              {}, Variant(&Framework::ConfigData::m_clearColor, 0, 0, 7), "color to clear the backbuffer to", Documented::kYes},
		{"lightColor",              {}, Variant(&Framework::ConfigData::m_lightColor, 7, 0, 7), "color of the direct light in the scene", Documented::kYes, RelatedToLighting::kYes},
		{"ambientColor",            {}, Variant(&Framework::ConfigData::m_ambientColor, 1, 0, 7), "color of the ambient light in the scene", Documented::kYes, RelatedToLighting::kYes},
		{"lightingIsMeaningful",    {}, Variant(&Framework::ConfigData::m_lightingIsMeaningful, false), "does the light color mean anything in this sample?", Documented::kNo, RelatedToLighting::kYes, Hidden::kYes},
		{"clearColorIsMeaningful",  {}, Variant(&Framework::ConfigData::m_clearColorIsMeaningful, true), "does the clear color mean anything in this sample?", Documented::kNo, RelatedToLighting::kNo, Hidden::kYes},
		{"secondsAreMeaningful",    {}, Variant(&Framework::ConfigData::m_secondsAreMeaningful, true), "does elapsed seconds mean anything in this sample?", Documented::kNo, RelatedToLighting::kNo, Hidden::kYes},
		{"log2CharacterWidth",      {}, Variant(&Framework::ConfigData::m_log2CharacterWidth, 3, 3, 5), "log2 of the character width in pixels", Documented::kYes},
		{"log2CharacterHeight",     {}, Variant(&Framework::ConfigData::m_log2CharacterHeight, 4, 3, 5), "log2 of the character height in pixels", Documented::kYes},
		{"asynchronous",            {}, Variant(&Framework::ConfigData::m_asynchronous, true), "CPU and GPU run asynchronously with regard to each other"},
		{"renderText",              {}, Variant(&Framework::ConfigData::m_renderText, true), "false: do not render text, true: render text"},
		{"printMenus",              {}, Variant(&Framework::ConfigData::m_printMenus, false), "false: do not print menus as JSON, true: do so"},
		{"dumpPackets",             {}, Variant(&Framework::ConfigData::m_dumpPackets, false), "false: do not dump packets, true: dump packets"},
		{"displayDebugObjects",     {}, Variant(&Framework::ConfigData::m_displayDebugObjects, false), "false: don't display debug objects, true: display them"},
		{"enableInput",             {}, Variant(&Framework::ConfigData::m_enableInput, true), "false: disable input, true: enable input"},
		{"enableTouchCursors",      {}, Variant(&Framework::ConfigData::m_enableTouchCursors, true), "false: disable touch cursors, true: enable touch cursors"},
		{"onionMemoryInBytes",      {}, Variant(&Framework::ConfigData::m_onionMemoryInBytes,  0x40000000, 0x00000000, 0xFFFFFFFF), "number of bytes of onion memory to allocate"},
		{"garlicMemoryInBytes",     {}, Variant(&Framework::ConfigData::m_garlicMemoryInBytes, 0x7F000000, 0x00000000, 0xFFFFFFFF), "number of bytes of garlic memory to allocate"},
		{"whoQueuesFlips",          {}, Variant(&Framework::ConfigData::m_whoQueuesFlips, 1, 0, 2), "0=CPU queues flip in main thread 1=GPU queues flip 2=CPU queues flip in worker thread"},
		{"flipMode",                {}, Variant(&Framework::ConfigData::m_flipMode, 1, 1, 4), "1=SCE_VIDEO_OUT_FLIP_MODE_VSYNC 2=SCE_VIDEO_OUT_FLIP_MODE_HSYNC 3=SCE_VIDEO_OUT_FLIP_MODE_WINDOW 4=SCE_VIDEO_OUT_FLIP_MODE_VSYNC_MULTI"},
		{"lookAtPosition",          {}, Variant(&Framework::ConfigData::m_lookAtPosition, {0,0,2.5f}, {-10,-10,-10}, {10,10,10}), "position for camera to look from", Documented::kYes, RelatedToLighting::kYes},
		{"lookAtTarget",            {}, Variant(&Framework::ConfigData::m_lookAtTarget, {0,0,0}, {-10,-10,-10}, {10,10,10}), "position of target for camera to look at", Documented::kYes, RelatedToLighting::kYes},
		{"lookAtUp",                {}, Variant(&Framework::ConfigData::m_lookAtUp, {0,1,0}, {-1,-1,-1}, {1,1,1}), "direction that camera considers to be up", Documented::kYes, RelatedToLighting::kYes},
		{"lightPosition",           {}, Variant(&Framework::ConfigData::m_lightPositionX, {-1.5,4,9}, {-10,-10,-10}, {10,10,10}), "position of light in scene", Documented::kYes, RelatedToLighting::kYes},
		{"lightTarget",             {}, Variant(&Framework::ConfigData::m_lightTargetX, {0,0,0}, {-10,-10,-10}, {10,10,10}), "position of target for light in scene", Documented::kYes, RelatedToLighting::kYes},
		{"lightUp",                 {}, Variant(&Framework::ConfigData::m_lightUpX, {0.f, 1.f, 0.f}, {-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}), "direction that light considers to be up", Documented::kYes, RelatedToLighting::kYes},  
        {"interruptOnFlip",         {}, Variant(&Framework::ConfigData::m_interruptOnFlip, true), "false: raise interrupt somewhat before flip, true: raise interrupt on flip", Documented::kYes},
		{"userProvidedSubmitDone",	{}, Variant(&Framework::ConfigData::m_userProvidedSubmitDone,false), "false: the framework will issue submitDone, true: user is responsible for issueing submitDone()", Documented::kYes},
	};

	void ReadConfigFile(Framework::ConfigData *config, int argc, const char* argv[])
	{	
		for(int arg=1; arg<argc; ++arg)
		{
			const char* src = argv[arg];
			while( *src == '-' )
				++src;
			if(arg < argc-1)
				for(unsigned i=0; i<sizeof(g_options)/sizeof(g_options[0]); ++i)
					if(g_options[i].m_hidden == Hidden::kNo && strcasecmp(src,g_options[i].m_name) == 0)
					{
						g_options[i].setAsString(config,argv[++arg]);
						break;
					}
		}
	}

	const uint32_t dbg_font_timer_c[] = {
#include "dbg_font_timer_c.h"
	};

	const uint32_t dbg_font_unsigned_c[] ={
#include "dbg_font_unsigned_c.h"
	};

	const uint32_t dbg_font_repair_c[] = {
#include "dbg_font_repair_c.h"
	};


	Gnmx::Toolkit::EmbeddedCsShader s_dbg_font_timer ={dbg_font_timer_c,"Debug Font Timer Compute Shader"};
	Gnmx::Toolkit::EmbeddedCsShader s_dbg_font_unsigned ={dbg_font_unsigned_c,"Debug Font Unsigned Compute Shader"};
	Gnmx::Toolkit::EmbeddedCsShader s_dbg_font_repair ={dbg_font_repair_c,"Debug Font Repairer Compute Shader"};

	Gnmx::Toolkit::EmbeddedCsShader *s_embeddedCsShader[] ={&s_dbg_font_timer,&s_dbg_font_unsigned,&s_dbg_font_repair};

	Gnmx::Toolkit::EmbeddedShaders s_embeddedShaders =
	{
		s_embeddedCsShader, sizeof(s_embeddedCsShader) / sizeof(s_embeddedCsShader[0]),
	};

	Framework::MenuItemText g_colorText[] =
	{
		{"Black", "Black"},
		{"Blue", "Blue"},
		{"Green", "Green"},
		{"Aqua", "Aqua"},
		{"Red", "Red"},
		{"Magenta", "Magenta"},
		{"LightGreen", "LightGreen"},
		{"LightGray", "LightGray"},
	};

	Framework::MenuItemText g_displayText[] = 
	{
		{"Color", "The RGB channels of the back buffer"},
		{"Alpha", "The alpha channel of the back buffer"},
		{"Depth", "Depth values from 0 to 1 (black to white)"},
		{"Stencil", "Integers from 0 to 255 (black to white)"},
	};
}

void Framework::GnmSampleFramework::print(FILE* file)
{
	fprintf(file, "{ \"title\" : \"%s\", \"overview\" : \"%s\", \"explanation\" : \"%s\", \"menu\" : ", m_windowTitle, m_overview, m_explanation);
	m_menuCommand->getMenu()->print(file);
	fprintf(file, ", \"option\" : [");
	unsigned items = 0;
	for(unsigned i=0; i<sizeof(g_options)/sizeof(g_options[0]); ++i)
	{
		const Option &o = g_options[i];
		if(o.m_documented == Documented::kNo)
			continue;
		if(o.m_hidden == Hidden::kYes)
			continue;
		if(!m_config.m_lightingIsMeaningful && o.m_relatedToLighting == RelatedToLighting::kYes)
			continue;
		++items;
	}
	unsigned item = 0;
	for(unsigned i=0; i<sizeof(g_options)/sizeof(g_options[0]); ++i)
	{
		const Option &o = g_options[i];
		if(o.m_documented == Documented::kNo)
			continue;
		if(o.m_hidden == Hidden::kYes)
			continue;
		if(!m_config.m_lightingIsMeaningful && o.m_relatedToLighting == RelatedToLighting::kYes)
			continue;
		fprintf(file,"{");
		fprintf(file,"\"name\" : \"%s\", ", o.m_name);
		fprintf(file,"\"description\" : \"%s\", ",o.m_description);
		switch(o.m_variant.m_type)
		{
		case kString:
			fprintf(file,"\"default\" : %s",o.m_variant.m_default.m_s);
			break;
		case kUint:
			fprintf(file,"\"default\" : %d, ",o.m_variant.m_default.m_u);
			fprintf(file,"\"min\" : %d, ",o.m_variant.m_min.m_u);
			fprintf(file,"\"max\" : %d",o.m_variant.m_max.m_u);
			break;
		case kVector3:
			{
				const Vector3 a = o.m_variant.m_default.m_v;
				const Vector3 b = o.m_variant.m_min.m_v;
				const Vector3 c = o.m_variant.m_max.m_v;
				fprintf(file,"\"default\" : \"%f %f %f\", ", (float)a.getX(), (float)a.getY(), (float)a.getZ());
				fprintf(file,"\"min\" : \"%f %f %f\", ", (float)b.getX(), (float)b.getY(), (float)b.getZ());
				fprintf(file,"\"max\" : \"%f %f %f\" ", (float)c.getX(), (float)c.getY(), (float)c.getZ());
				break;
			}
		case kBool:
			fprintf(file,"\"default\" : %s", o.m_variant.m_default.m_b ? "true" : "false");
			break;	
		case kFloat:
			fprintf(file,"\"default\" : %f, ",o.m_variant.m_default.m_f);
			fprintf(file,"\"min\" : %f, ",o.m_variant.m_min.m_f);
			fprintf(file,"\"max\" : %f",o.m_variant.m_max.m_f);
			break;
		}
		fprintf(file,"}");
		if(item != items - 1)
			fprintf(file,", ");
		++item;
	}
	fprintf(file, "]}");
}

class MenuCommandEnumDisplayValidator : public Framework::Validator
{
public:
	Framework::GnmSampleFramework* m_gsf;
	virtual bool isValid(uint32_t value) const;
};

class MenuCommandEnumDisplay : public Framework::MenuCommandEnum
{
	MenuCommandEnumDisplayValidator m_validator;
public:
	Framework::GnmSampleFramework* m_gsf;
	MenuCommandEnumDisplay(uint32_t* target)
	: MenuCommandEnum(g_displayText, sizeof(g_displayText)/sizeof(g_displayText[0]), target, &m_validator)
	{
	}
	virtual bool isEnabled() const;
	void SetFramework(Framework::GnmSampleFramework* gsf)
	{
		m_gsf = gsf;
		m_validator.m_gsf = gsf;
	}
};

class MenuCommandEnumClearColor : public Framework::MenuCommandEnum
{
public:
	bool m_isEnabled;
	uint8_t m_reserved[7];
	Framework::GnmSampleFramework* m_gsf;
	MenuCommandEnumClearColor(uint32_t* target)
	: MenuCommandEnum(g_colorText, sizeof(g_colorText)/sizeof(g_colorText[0]), target)
	, m_isEnabled(true)
	{
	}
	virtual bool isEnabled() const
	{
		return m_gsf->m_config.m_clearColorIsMeaningful;
	}
};

class MenuCommandEnumLightColor : public Framework::MenuCommandEnum
{
public:
	bool m_isEnabled;
	uint8_t m_reserved[7];
	Framework::GnmSampleFramework* m_gsf;
	MenuCommandEnumLightColor(uint32_t* target)
		: MenuCommandEnum(g_colorText, sizeof(g_colorText)/sizeof(g_colorText[0]), target)
		, m_isEnabled(true)
	{
	}
	virtual bool isEnabled() const
	{
		return m_gsf->m_config.m_lightingIsMeaningful;
	}
};

class MenuCommandQuit : public Framework::MenuCommand
{
public:
	Framework::GnmSampleFramework* m_gsf;
	virtual void adjust(int) const
	{
		m_gsf->RequestTermination();
	}
};

#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
class MenuCommandRazorGpuThreadTrace : public Framework::MenuCommand
{
public:
	Framework::GnmSampleFramework* m_gsf;
	virtual void adjust(int) const
	{
		m_gsf->RequestThreadTrace();
	}
	
	virtual bool isEnabled() const
	{
		return sce::Gnm::isUserPaEnabled();
	}  
};
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE

class MenuCommandMatrix : public Framework::MenuCommandMatrix
{
public:
	Framework::GnmSampleFramework *m_gsf;
	bool *m_enabled;

	MenuCommandMatrix(Matrix4 *target, bool useRightStick)
	: Framework::MenuCommandMatrix(target, useRightStick)
	{
	}

	bool isEnabled() const
	{
		return *m_enabled; 
	}
	void input(Framework::MenuHost *src)
	{
		m_step = (float)m_gsf->actualSecondsPerFrame() * 2.f;
		Framework::MenuCommandMatrix::input(src);
		if (updated_by_input_since_last_call())
			m_gsf->UpdateMatrices();
	}
};

class MenuCommandResetCamera : public Framework::MenuCommand
{
public:
	Framework::GnmSampleFramework* m_gsf;
	virtual void adjust(int) const
	{
		m_gsf->UpdateViewToWorldMatrix(m_gsf->m_originalViewMatrix);
	}
	virtual bool isEnabled() const
	{
		return m_gsf->m_config.m_cameraControls;
	}
};

class MenuCommandSecondsElapsed : public Framework::MenuCommandDouble
{
public:
	Framework::GnmSampleFramework* m_gsf;
	MenuCommandSecondsElapsed(double* target, double min, double max, double step)
	: MenuCommandDouble(target, min, max, step)
	{
	}
	virtual bool isEnabled() const
	{
		return m_gsf->m_config.m_secondsAreMeaningful;
	}
};

class LightingMenu : public Framework::MenuCommand
{
	Framework::GnmSampleFramework* m_gsf;
	Framework::Menu m_menu;
	MenuCommandMatrix         m_menuCommandLightPosition;
	MenuCommandEnumClearColor m_menuCommandEnumClearColor;
	MenuCommandEnumLightColor m_menuCommandEnumLightColor;
	MenuCommandEnumLightColor m_menuCommandEnumAmbientColor;
public:
	LightingMenu(Framework::GnmSampleFramework *gsf)
	: m_gsf(gsf)
	, m_menuCommandLightPosition(&gsf->m_lightToWorldMatrix, false)
	, m_menuCommandEnumClearColor(&gsf->m_config.m_clearColor)
	, m_menuCommandEnumLightColor(&gsf->m_config.m_lightColor)
	, m_menuCommandEnumAmbientColor(&gsf->m_config.m_ambientColor)
	{
		m_menuCommandLightPosition.m_gsf = gsf;
		m_menuCommandLightPosition.m_enabled = &gsf->m_config.m_lightingIsMeaningful;
		m_menuCommandEnumClearColor.m_gsf = gsf;
		m_menuCommandEnumLightColor.m_gsf = gsf;
		m_menuCommandEnumAmbientColor.m_gsf = gsf;
		const Framework::MenuItem menuItem[] =
		{
			{{"Light Position", "Left stick moves"}, &m_menuCommandLightPosition},
			{{"Clear Color", "Color to clear the back buffer"}, &m_menuCommandEnumClearColor},
			{{"Light Color", "Color of directional light"}, &m_menuCommandEnumLightColor},
			{{"Ambient Color", "Color of ambient light"}, &m_menuCommandEnumAmbientColor},
		};
		m_menu.appendMenuItems(sizeof(menuItem)/sizeof(menuItem[0]), &menuItem[0]);
	}
	virtual Framework::Menu* getMenu()
	{
		return &m_menu;
	}
	virtual void adjust(int) const {}
	virtual bool isEnabled() const
	{
		return m_gsf->m_config.m_lightingIsMeaningful || m_gsf->m_config.m_clearColorIsMeaningful;
	}
};

class CommonMenu : public Framework::MenuCommand
{
	Framework::Menu              m_menu;
	MenuCommandQuit              m_menuCommandQuit;
	Framework::MenuCommandBool   m_menuCommandPause;
	MenuCommandEnumDisplay       m_menuCommandEnumDisplay;
	MenuCommandMatrix            m_menuCommandCamera;
	MenuCommandResetCamera       m_menuCommandResetCamera;
	MenuCommandSecondsElapsed    m_menuCommandSecondsElapsed;
	LightingMenu				 m_menuCommandLightingMenu;
#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
	MenuCommandRazorGpuThreadTrace	m_menuThreadTrace;
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE

public:
	CommonMenu(Framework::GnmSampleFramework* gsf)	
	: m_menuCommandPause(&gsf->m_paused)
	, m_menuCommandEnumDisplay(&gsf->m_config.m_displayBuffer)
	, m_menuCommandCamera(&gsf->m_viewToWorldMatrix, true)
	, m_menuCommandSecondsElapsed(&gsf->m_secondsElapsedApparent, 0, 0, 1/30.f)
	, m_menuCommandLightingMenu(gsf)
	{
		m_menuCommandQuit.m_gsf = gsf;
		m_menuCommandCamera.m_gsf = gsf;
		m_menuCommandCamera.m_enabled = &gsf->m_config.m_cameraControls;
		m_menuCommandResetCamera.m_gsf = gsf;
		m_menuCommandSecondsElapsed.m_gsf = gsf;
		m_menuCommandEnumDisplay.SetFramework(gsf);
#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
		m_menuThreadTrace.m_gsf = gsf;
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
		restoreDefaultMenuItems();
	}
	void removeAllMenuItems()
	{
		m_menu.removeAllMenuItems();
	}
	void restoreDefaultMenuItems()
	{
		m_menu.removeAllMenuItems();
		Framework::MenuItem menuItem[] =
		{
			{{"Pause", "Pause or unpause simulation"}, &m_menuCommandPause},
			{{"Buffer", "Select a buffer to display"}, &m_menuCommandEnumDisplay},
			{{"Camera Controls", "Left stick moves, right stick rotates"}, &m_menuCommandCamera},
			{{"Reset Camera", "Bring camera to original orientation"}, &m_menuCommandResetCamera},
			{{"Seconds Elapsed", "This is the apparent time since the start of the simulation. It can be adjusted with either the left stick or d-pad"}, &m_menuCommandSecondsElapsed},
			{{"Lighting", "Options pertaining to lighting"}, &m_menuCommandLightingMenu},
#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
			{{"Thread Trace", "Trace a frame of the sample"}, &m_menuThreadTrace},
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
		};
		m_menu.appendMenuItems(sizeof(menuItem)/sizeof(menuItem[0]), menuItem);
	}
	virtual Framework::Menu* getMenu()
	{
		return &m_menu;
	}
	virtual void adjust(int) const {}
};

void Framework::GnmSampleFramework::removeAllMenuItems()
{
	((CommonMenu*)m_menuCommand)->removeAllMenuItems();
}

void Framework::GnmSampleFramework::restoreDefaultMenuItems()
{
	((CommonMenu*)m_menuCommand)->restoreDefaultMenuItems();
}

bool MenuCommandEnumDisplay::isEnabled() const
{
	return true; //m_gsf->m_config.m_depthBufferIsMeaningful || m_gsf->m_config.m_stencil;
}

bool MenuCommandEnumDisplayValidator::isValid(uint32_t value) const
{
	switch(value)
	{
		case kFrameworkDisplayBufferColor:
			return true;
			break;
		case kFrameworkDisplayBufferAlpha:
			return m_gsf->m_backBuffer->m_renderTarget.getDataFormat().getNumComponents() == 4;
			break;
		case kFrameworkDisplayBufferDepth:
			return m_gsf->m_config.m_depthBufferIsMeaningful;
			return true;
			break;
		case kFrameworkDisplayBufferStencil:
			return m_gsf->m_config.m_stencil;
			break;
		default:
			return false;
	}
}

Framework::GnmSampleFramework::GnmSampleFramework()
: m_secondsElapsedActualPerFrame(0)
, m_secondsElapsedApparentPerFrame(0)
, m_flipThread(this)
, m_menuIsEnabled(true)
, m_alreadyPreparedHud(false)
, m_eopEventQueue("Framework Main Thread Queue")
, m_originalViewMatrix(Matrix4::identity())
, m_menuAdjustmentValidator(0)
{	
	m_onionAllocator = GetInterface(&m_onionStackAllocator);
	m_garlicAllocator = GetInterface(&m_garlicStackAllocator);
	m_allocators.m_onion = m_onionAllocator;
	m_allocators.m_garlic = m_garlicAllocator;
#ifdef _MSC_VER
#             pragma warning(push)
#             pragma warning(disable:4996) // use deprecated class
#elif defined(__ORBIS__)
#             pragma clang diagnostic push
#             pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
	m_allocator = m_garlicAllocator;
#ifdef _MSC_VER
#             pragma warning(pop)
#elif defined(__ORBIS__)
#             pragma clang diagnostic pop
#endif

	m_menuCommand = new CommonMenu(this);
	MenuStackEntry menuStackEntry;
	menuStackEntry.m_menu = m_menuCommand->getMenu();
	menuStackEntry.m_x = 0;
	menuStackEntry.m_y = 7;
	m_menuHost.m_menuStack.push(menuStackEntry);
	restoreDefaultMenuItems();

	memset( m_szExePath, 0, sizeof(m_szExePath) );
	m_secondsElapsedApparent = 0;
	m_secondsElapsedActual = 0;
	m_paused = false;
	
	m_shutdownRequested = 0;
	m_frameCount = 0;
	m_windowHandle = 0;

	for(unsigned i=0; i<sizeof(g_options)/sizeof(g_options[0]); ++i)
		g_options[i].loadDefault(&m_config);
}

Framework::GnmSampleFramework::~GnmSampleFramework()
{
	delete (CommonMenu*)m_menuCommand;
	// Assert: check that the memory has been freed before leaving.
	SCE_GNM_ASSERT(0 == m_buffer);
}

uint32_t Framework::GnmSampleFramework::GetVideoHandle()
{
	return m_videoInfo.handle;
}

SceKernelEqueue Framework::GnmSampleFramework::GetEopEventQueue()
{
	return m_eopEventQueue.getEventQueue();
}

DbgFont::Cell Framework::GnmSampleFramework::s_clearCell[4] =
{
	{0, DbgFont::kWhite, 15, DbgFont::kBlack, 3}, // kFrameworkDisplayBufferColor,
	{0, DbgFont::kWhite, 15, DbgFont::kBlack, 7}, // kFrameworkDisplayBufferAlpha,
	{0, DbgFont::kWhite, 15, DbgFont::kBlack, 7}, // kFrameworkDisplayBufferDepth,
	{0, DbgFont::kWhite, 15, DbgFont::kBlack, 7}, // kFrameworkDisplayBufferStencil,
};

int32_t Framework::GnmSampleFramework::processCommandLine(int argc, const char* argv[])
{
	{
		const char root_fs[] = "/app0/";
		strncpy(m_szExePath,root_fs, sizeof(m_szExePath));
		if( argv[0]) 
		{
			const char *start = argv[0];
			const char *file_name_start = start + strlen(start);
			while( file_name_start != start && *file_name_start != '\\' && *file_name_start != '/')
				--file_name_start;
			
			if( file_name_start != start )
			{
				const char* dir_start = file_name_start - 1;
				while( dir_start != start && *dir_start != '\\' && *dir_start != '/')
					--dir_start;
				char* dest = m_szExePath + sizeof(root_fs) - 1;
				const char* src = dir_start+1;
				for( unsigned n = sizeof(root_fs); 
					src != file_name_start && n+2 < sizeof(m_szExePath); 
					++n, ++src, ++dest)
				{
					*dest = *src;
				}
				*dest++ = '/';
				*dest = '\0';
				
			}
		}
	}
	ReadConfigFile(&m_config, argc, argv);
	return 0;
}

int32_t Framework::GnmSampleFramework::initialize(const char* windowTitle, const char* overview, const char* explanation, int argc, const char *argv[])
{
	processCommandLine(argc, argv);
	return initialize(windowTitle, overview, explanation);
}

int32_t Framework::GnmSampleFramework::initialize(const char* windowTitle, const char* overview, const char* explanation)
{
	SCE_GNM_ASSERT_MSG(overview && overview[strlen(overview)-1] == '.', "framework sample requires overview text, and requires it to end in a period.");
	SCE_GNM_ASSERT_MSG(explanation && explanation[strlen(explanation)-1] == '.', "framework sample requires explanation text, and requires it to end in a period.");

	Gnm::registerOwner(&m_allocators.m_owner, windowTitle);
	const unsigned kPlayerId = 0;
	m_videoInfo.handle = sceVideoOutOpen(kPlayerId, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
	SCE_GNM_ASSERT_MSG(m_videoInfo.handle >= 0, "sceVideoOutOpen() returned error code %d.", m_videoInfo.handle);

	if(m_config.m_targetWidth == 0 && m_config.m_targetHeight == 0)
	{
		m_config.m_targetWidth  = k1080pWidth;
		m_config.m_targetHeight = k1080pHeight;
        if(sceKernelIsNeoMode())
        {
		    SceVideoOutResolutionStatus status;
		    const int ret = sceVideoOutGetResolutionStatus(m_videoInfo.handle, &status);
		    SCE_GNM_ASSERT_MSG(ret == 0, "sceVideoOutGetResolutionStatus() returned error code %d.", ret);
		    if(ret == 0 && (status.fullWidth > k1080pWidth || status.fullHeight > k1080pHeight)) // If VideoOut signal Resolution is higher than 1080p, please render 4K resolution.
		    {
    			m_config.m_targetWidth  = k4kWidth;
			    m_config.m_targetHeight = k4kHeight;
		    }
		}
	}

    if(sceKernelIsNeoMode())
    {
        m_config.m_targetWidth  = std::max(k1080pWidth , std::min(k4kWidth,  m_config.m_targetWidth ));
        m_config.m_targetHeight = std::max(k1080pHeight, std::min(k4kHeight, m_config.m_targetHeight));
    }
    else
    {
        m_config.m_targetWidth  = std::max(k720pWidth , std::min(k1080pWidth,  m_config.m_targetWidth ));
        m_config.m_targetHeight = std::max(k720pHeight, std::min(k1080pHeight, m_config.m_targetHeight));
    }


	memset(&m_hardwareCursors, 0, sizeof(m_hardwareCursors));			

	m_menuHost.m_controllerContext = &m_controllerContext;
	m_menuHost.m_actualSecondsPerFrame = &m_secondsElapsedActualPerFrame;
    m_menuHost.m_cameraControlMode = m_config.m_cameraControlMode;

	m_windowTitle = windowTitle;
	m_overview = overview;
	m_explanation = explanation;

	m_line = 0;

	m_lastClock = performanceCounter();

	int32_t ret = SCE_GNM_OK;

	m_lightToWorldMatrix = inverse(Matrix4::lookAt((Point3)m_config.m_lightPositionX, (Point3)m_config.m_lightTargetX, m_config.m_lightUpX));

	m_depthNear = 1.f;
	m_depthFar = 100.f;
	const float aspect = (float)m_config.m_targetWidth / (float)m_config.m_targetHeight;
	m_projectionMatrix = Matrix4::frustum( -aspect, aspect, -1, 1, m_depthNear, m_depthFar );
	SetViewToWorldMatrix(inverse(Matrix4::lookAt((Point3)m_config.m_lookAtPosition, (Point3)m_config.m_lookAtTarget, m_config.m_lookAtUp)));

	m_onionStackAllocator.init(SCE_KERNEL_WB_ONION, m_config.m_onionMemoryInBytes);
	m_garlicStackAllocator.init(SCE_KERNEL_WC_GARLIC, m_config.m_garlicMemoryInBytes);

	DebugObjects::initializeWithAllocators(&m_allocators);

	{
		s_embeddedShaders.initializeWithAllocators(&m_allocators);
		for(unsigned i = 0; i < s_embeddedShaders.m_embeddedVsShaders; ++i)
		{
			Gnmx::Toolkit::EmbeddedVsShader *embeddedVsShader = s_embeddedShaders.m_embeddedVsShader[i];
			m_allocators.allocate((void**)&embeddedVsShader->m_fetchShader, SCE_KERNEL_WC_GARLIC, Gnmx::computeVsFetchShaderSize(embeddedVsShader->m_shader), Gnm::kAlignmentOfBufferInBytes, Gnm::kResourceTypeFetchShaderBaseAddress, "Framework Fetch Shader %d", i);
			uint32_t shaderModifier;
			Gnmx::generateVsFetchShader(embeddedVsShader->m_fetchShader, &shaderModifier, embeddedVsShader->m_shader);
			embeddedVsShader->m_shader->applyFetchShaderModifier(shaderModifier);
		}
	}

	m_controllerContext.initialize();

	Gnmx::Toolkit::initializeWithAllocators(&m_allocators);
	DbgFont::initializeWithAllocators(&m_allocators);

	Gnm::SizeAlign timerSizeAlign = Gnmx::Toolkit::Timers::getRequiredBufferSizeAlign(kTimers*m_config.m_buffers);

	void *timerPtr = nullptr;
	m_allocators.allocate(&timerPtr, SCE_KERNEL_WB_ONION, timerSizeAlign, Gnm::kResourceTypeLabel, "Framework Timers");

	BuildDebugConeMesh(&m_garlicAllocator, &m_coneMesh, 1.f, 1.f, 16);
	BuildDebugCylinderMesh(&m_garlicAllocator, &m_cylinderMesh, 1.f, 1.f, 16);
	BuildDebugSphereMesh(&m_garlicAllocator, &m_sphereMesh, 1.f, 16, 16);

	if(kCpuWorkerThread == m_config.m_whoQueuesFlips)
		m_flipThread.run();

	m_allocators.allocate((void**)&m_label,               SCE_KERNEL_WB_ONION, sizeof(uint64_t) * m_config.m_buffers, sizeof(uint64_t), Gnm::kResourceTypeLabel, "Framework Labels");
	m_allocators.allocate((void**)&m_labelForPrepareFlip, SCE_KERNEL_WB_ONION, sizeof(uint64_t) * m_config.m_buffers, sizeof(uint64_t), Gnm::kResourceTypeLabel, "Framework Labels For PrepareFlip");

	m_buffer = new Buffer[m_config.m_buffers];
	m_backBufferIndex = 0;
	m_backBuffer = &m_buffer[m_backBufferIndex];
	m_indexOfLastBufferCpuWrote = m_config.m_buffers - 1;
	m_indexOfBufferCpuIsWriting = 0;

	const unsigned characterWidth = 1 << m_config.m_log2CharacterWidth;
	const unsigned characterHeight = 1 << m_config.m_log2CharacterHeight;
	const unsigned screenCharactersWide = (m_config.m_targetWidth + characterWidth - 1) / characterWidth;
	const unsigned screenCharactersHigh = (m_config.m_targetHeight + characterHeight - 1) / characterHeight;

	void* buffer_address[8];
	for( unsigned buffer=0; buffer<m_config.m_buffers; ++buffer )
	{
		m_label[buffer] = 0xFFFFFFFF;
		m_buffer[buffer].m_label = m_label + buffer;
		m_buffer[buffer].m_expectedLabel = 0xFFFFFFFF;

		m_labelForPrepareFlip[buffer] = 0xFFFFFFFF;
		m_buffer[buffer].m_labelForPrepareFlip = m_labelForPrepareFlip + buffer;

        {
		    const Gnm::DataFormat colorFormat = Gnm::kDataFormatB8G8R8A8UnormSrgb;
		    Gnm::TileMode tileMode;
		    GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &tileMode, GpuAddress::kSurfaceTypeColorTargetDisplayable, colorFormat, 1);

	        sce::Gnm::RenderTargetSpec spec;
	        spec.init();
	        spec.m_width = m_config.m_targetWidth;
	        spec.m_height = m_config.m_targetHeight;
	        spec.m_pitch = 0;
	        spec.m_numSlices = 1;
	        spec.m_colorFormat = colorFormat;
	        spec.m_colorTileModeHint = tileMode;
	        spec.m_minGpuMode = sce::Gnm::getGpuMode();
	        spec.m_numSamples = Gnm::kNumSamples1;
	        spec.m_numFragments = Gnm::kNumFragments1;
	        spec.m_flags.enableCmaskFastClear   = 0;
	        spec.m_flags.enableFmaskCompression = 0;
            spec.m_flags.enableDccCompression   = m_config.m_dcc ? 1 : 0;
	        int32_t status = m_buffer[buffer].m_renderTarget.init(&spec);
            SCE_GNM_ASSERT(status == SCE_GNM_OK);
		    void *colorPointer = nullptr;
		    m_allocators.allocate(&colorPointer, SCE_KERNEL_WC_GARLIC, m_buffer[buffer].m_renderTarget.getColorSizeAlign(), Gnm::kResourceTypeRenderTargetBaseAddress, "Framework Buffer %d Color", buffer);
            buffer_address[buffer] = colorPointer;
            m_buffer[buffer].m_renderTarget.setAddresses(colorPointer, nullptr, nullptr);
	        if(m_config.m_dcc)
            {
                void *dccPointer = nullptr;
		        m_allocators.allocate(&dccPointer, SCE_KERNEL_WC_GARLIC, m_buffer[buffer].m_renderTarget.getDccSizeAlign(), Gnm::kResourceTypeGenericBuffer, "Framework Buffer %d DCC", buffer);
                m_buffer[buffer].m_renderTarget.setDccAddress(dccPointer);
            }
        }

		Gnm::SizeAlign stencilSizeAlign;
		Gnm::SizeAlign htileSizeAlign;
		const Gnm::StencilFormat stencilFormat = (m_config.m_stencil) ? Gnm::kStencil8 : Gnm::kStencilInvalid;
		Gnm::TileMode depthTileMode;
		Gnm::DataFormat depthFormat = Gnm::DataFormat::build(Gnm::kZFormat32Float);
        const GpuAddress::SurfaceType surfaceType = m_config.m_stencil ? GpuAddress::kSurfaceTypeDepthTarget : GpuAddress::kSurfaceTypeDepthOnlyTarget;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(), &depthTileMode, surfaceType, depthFormat, 1);

		const Gnm::SizeAlign depthTargetSizeAlign = Gnmx::Toolkit::init(&m_buffer[buffer].m_depthTarget, m_config.m_targetWidth, m_config.m_targetHeight, depthFormat.getZFormat(), stencilFormat, depthTileMode, Gnm::kNumFragments1, (m_config.m_stencil)?&stencilSizeAlign:0, (m_config.m_htile)?&htileSizeAlign:0 );

		void *stencilPtr = nullptr;
		void *htilePtr = nullptr;
		if(m_config.m_stencil)
		{
			m_allocators.allocate(&stencilPtr, SCE_KERNEL_WC_GARLIC, stencilSizeAlign, Gnm::kResourceTypeDepthRenderTargetStencilAddress, "Framework Buffer %d Stencil", buffer);
		}
		if(m_config.m_htile)
		{
			m_allocators.allocate(&htilePtr, SCE_KERNEL_WC_GARLIC, htileSizeAlign, Gnm::kResourceTypeDepthRenderTargetHTileAddress, "Framework Buffer %d HTile", buffer);
			m_buffer[buffer].m_depthTarget.setHtileAddress(htilePtr);
		}

		void *depthPtr = nullptr;
		m_allocators.allocate(&depthPtr, SCE_KERNEL_WC_GARLIC, depthTargetSizeAlign, Gnm::kResourceTypeDepthRenderTargetBaseAddress, "Framework Buffer %d Depth", buffer);
		m_buffer[buffer].m_depthTarget.setAddresses(depthPtr, stencilPtr);

		Gnm::SizeAlign screenSizeAlign = m_buffer[buffer].m_screen.calculateRequiredBufferSizeAlign(screenCharactersWide, screenCharactersHigh);
		void *screenPtr = nullptr;
		m_allocators.allocate(&screenPtr, SCE_KERNEL_WB_ONION, screenSizeAlign, Gnm::kResourceTypeBufferBaseAddress, "Framework Buffer %d Screen", buffer);

		m_buffer[buffer].m_screen.initialize(screenPtr, screenCharactersWide, screenCharactersHigh);
		enum {kOverscan = 2};
		DbgFont::Window fullScreenWindow;
		fullScreenWindow.initialize(&m_buffer[buffer].m_screen);
		m_buffer[buffer].m_window.initialize(&fullScreenWindow, kOverscan, kOverscan, screenCharactersWide - kOverscan * 2, screenCharactersHigh - kOverscan * 2);

		m_buffer[buffer].m_timers.initialize(static_cast<uint8_t*>(timerPtr) + timerSizeAlign.m_size / m_config.m_buffers * buffer, kTimers);

		m_buffer[buffer].m_debugObjects.Init(&m_onionAllocator, &m_sphereMesh, &m_coneMesh, &m_cylinderMesh);
	}

	// Create Attribute
	m_videoInfo.flip_index_base = 0;
	m_videoInfo.buffer_num = m_config.m_buffers;

	SceVideoOutBufferAttribute attribute;
	sceVideoOutSetBufferAttribute(&attribute,
		SCE_VIDEO_OUT_PIXEL_FORMAT_B8_G8_R8_A8_SRGB,
		SCE_VIDEO_OUT_TILING_MODE_TILE,
		SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
		m_buffer[0].m_renderTarget.getWidth(),
		m_buffer[0].m_renderTarget.getHeight(),
		m_buffer[0].m_renderTarget.getPitch());
	ret= sceVideoOutRegisterBuffers(m_videoInfo.handle, 0, buffer_address, m_config.m_buffers, &attribute );
	SCE_GNM_ASSERT_MSG(ret >= 0, "sceVideoOutRegisterBuffers() returned error code %d.", ret);
	m_shutdownRequested = 0;

	m_renderedDebugObjectsAlreadyInThisFrame = false;

	return -1;
}

void Framework::GnmSampleFramework::terminate(sce::Gnmx::GnmxGfxContext& /*gfxc*/)
{
	if(kCpuWorkerThread == m_config.m_whoQueuesFlips)
		m_flipThread.join();
	StallUntilGpuIsIdle();
	if(m_config.m_printMenus)
	{
		FILE* file = fopen(m_config.m_menuFilename, "wb");
		SCE_GNM_ASSERT(file);
		print(file);
		fclose(file);
	}
	if(m_config.m_screenshot)
	{
		TakeScreenshot(m_config.m_screenshotFilename);
	}

	m_garlicStackAllocator.deinit();
	m_onionStackAllocator.deinit();

	delete[] m_buffer;
	m_buffer = 0;
}


void Framework::GnmSampleFramework::forceRedraw()
{
}

void Framework::GnmSampleFramework::RenderDebugObjects(sce::Gnmx::GnmxGfxContext& gfxc)
{
	if(m_renderedDebugObjectsAlreadyInThisFrame)
		return;
	if(m_config.m_displayDebugObjects)
		m_backBuffer->m_debugObjects.Render(gfxc, m_viewProjectionMatrix);
	m_backBuffer->m_debugObjects.Clear();
	m_renderedDebugObjectsAlreadyInThisFrame = true;
}

void Framework::GnmSampleFramework::displayTimer(sce::Gnmx::GnmxGfxContext &gfxc, uint32_t x, uint32_t y, void* timer, uint32_t characters, uint32_t decimal, float factor)
{
	Gnmx::Toolkit::synchronizeGraphicsToCompute(&gfxc.m_dcb);
	uint32_t* label = static_cast<uint32_t*>(gfxc.allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8));
	uint32_t zero = 0;
	gfxc.writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
	gfxc.writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, label, 1, Gnm::kCacheActionNone);
	gfxc.waitOnAddress(label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);

	struct Constants
	{
		int32_t m_cursorX;
		int32_t m_cursorY;
		int32_t m_characters;
		int32_t m_decimal;
		int32_t m_screenWidthInCharacters;
		float m_multiplier;
		int32_t m_windowX;
		int32_t m_windowY;
		int32_t m_windowWidth;
		int32_t m_windowHeight;
	};
	Constants* constants = (Constants*)gfxc.allocateFromCommandBuffer(sizeof(Constants), Gnm::kEmbeddedDataAlignment4);
	constants->m_cursorX = x; // x position of first character
	constants->m_cursorY = y; // y position of first character
	constants->m_screenWidthInCharacters = m_backBuffer->m_window.m_screen->m_widthInCharacters;
	constants->m_characters = characters; // count of characters to print
	constants->m_decimal = decimal; // where the decimal point goes
	constants->m_multiplier = factor; // factor to multiply clock difference by, in order to get printable value
	constants->m_windowX = m_backBuffer->m_window.m_positionXInCharactersRelativeToScreen;
	constants->m_windowY = m_backBuffer->m_window.m_positionYInCharactersRelativeToScreen;
	constants->m_windowWidth = m_backBuffer->m_window.m_widthInCharacters;
	constants->m_windowHeight = m_backBuffer->m_window.m_heightInCharacters;

	Gnm::Buffer buffer, constantBuffer;
	buffer.initAsDataBuffer(timer, Gnm::kDataFormatR32Uint, 4);
	buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never bind it as RWBuffer, so read-only is OK
	constantBuffer.initAsConstantBuffer(constants, sizeof(Constants));
	constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

	gfxc.setCsShader(s_dbg_font_timer.m_shader, &s_dbg_font_timer.m_offsetsTable);
	gfxc.setRwBuffers(Gnm::kShaderStageCs, 0, 1, &m_backBuffer->m_screen.m_cellBuffer);
	gfxc.setBuffers(Gnm::kShaderStageCs, 0, 1, &buffer);
	gfxc.setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &constantBuffer);
	gfxc.dispatch(1,1,1);
	Gnmx::Toolkit::synchronizeComputeToCompute(&gfxc.m_dcb);
}

void Framework::GnmSampleFramework::PrepareHud(sce::Gnmx::GnmxGfxContext& gfxc)
{
	SCE_GNM_ASSERT_MSG(m_alreadyPreparedHud == false, "Can't prepare HUD twice in one frame.");
	m_alreadyPreparedHud = true;

	DisplayKeyboardOptions();

	double actualSeconds = 0;
	if(m_config.m_microSecondsPerFrame == 0)
	{
		const uint64_t clock = performanceCounter();
		const uint64_t frequency = performanceFrequency();
		uint64_t clocks;
		if(clock >= m_lastClock)
			clocks = clock - m_lastClock;
		else
			clocks = clock + ( std::numeric_limits<uint64_t>::max() - m_lastClock ) + 1;
		actualSeconds = clocks / (double)frequency;
		m_lastClock = clock;
	}
	else
	{
		actualSeconds = m_config.m_microSecondsPerFrame / 1000000.0;
	}
	m_secondsElapsedActual += actualSeconds;
	m_secondsElapsedApparent += actualSeconds * (m_paused?0:1);

	{
		volatile uint64_t *label = (volatile uint64_t*)gfxc.allocateFromCommandBuffer(sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8);
		uint32_t zero = 0;
		gfxc.writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
		gfxc.writeAtEndOfShader(Gnm::kEosCsDone, (void*)label, 0x1); // tell the CP to write a 1 into the memory only when all compute shaders have finished
		gfxc.waitOnAddress((void*)label, 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1); // tell the CP to wait until the memory has the val 1
		m_backBuffer->m_timers.end(gfxc.m_dcb, kFrameTimer);
	}

	RenderDebugObjects(gfxc);

	m_backBuffer->m_window.setCursor(0, m_line);
	m_backBuffer->m_window.m_cell = s_clearCell[m_config.m_displayBuffer];

#if !SCE_GNMX_ENABLE_GFX_LCUE
	m_backBuffer->m_window.printf("libgnm - %s Sample\n%s", m_windowTitle, m_overview);
#else
	m_backBuffer->m_window.printf("libgnm - %s Sample (w/LightweightGfxContext)\n%s", m_windowTitle, m_overview);
#endif

	if(m_config.m_displayTimings)
	{
		const unsigned fps = static_cast<unsigned>(1.0 / actualSeconds + 0.5f);
		const float cpuExcludingFrameworkInMs = static_cast<float>(m_cpuTimerExcludingFramework.seconds() * 1000);
		m_cpuTimerIncludingFramework.stop();
		const float cpuIncludingFrameworkInMs = static_cast<float>(m_cpuTimerIncludingFramework.seconds() * 1000);
		m_cpuTimerIncludingFramework.start();
		m_backBuffer->m_window.setCursor(0, m_line+3);
		m_backBuffer->m_window.printf("FPS: %3d CPU: build command buffer=%09.4f total=%09.4fms GPU: 0000.0000ms", fps, cpuExcludingFrameworkInMs, cpuIncludingFrameworkInMs);
		// have the GPU compute the time in milliseconds between the begin and end timers,
		// and have the GPU print the resulting ASCII string, without help from the CPU.
		displayTimer(gfxc, 68, m_line + 3, m_backBuffer->m_timers.m_addr, 9, 4, 1000.0 / SCE_GNM_GPU_CORE_CLOCK_FREQUENCY);
	}

	if(m_config.m_displayBuffer != kFrameworkDisplayBufferColor)
	{
		Gnmx::decompressDepthSurface(&gfxc, &m_backBuffer->m_depthTarget);
		Gnm::DbRenderControl dbRenderControl;
		dbRenderControl.init();
		gfxc.setDbRenderControl(dbRenderControl);

		// Before we blit, we need to flush/invalidate all outstanding CB/DB/SX writes
		gfxc.triggerEvent(Gnm::kEventTypeCacheFlushAndInvEvent);

		Gnm::Texture srcTexture;
		switch(m_config.m_displayBuffer)
		{
		case kFrameworkDisplayBufferAlpha:
			srcTexture.initFromRenderTarget(&m_backBuffer->m_renderTarget, false);
			srcTexture.setChannelOrder(Gnm::kTextureChannelW, Gnm::kTextureChannelW, Gnm::kTextureChannelW, Gnm::kTextureChannelW);
			srcTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we won't bind this texture as an RWTexture, so it's safe to declare it read-only.
			Gnmx::Toolkit::SurfaceUtil::copyTextureToRenderTarget(gfxc, &m_backBuffer->m_renderTarget, &srcTexture);
			Gnmx::Toolkit::synchronizeRenderTargetGraphicsToCompute(&gfxc.m_dcb, &m_backBuffer->m_renderTarget);
			break;
		case kFrameworkDisplayBufferStencil:
			srcTexture.initFromStencilTarget(&m_backBuffer->m_depthTarget, Gnm::kTextureChannelTypeUNorm, false);
			srcTexture.setChannelOrder(Gnm::kTextureChannelX, Gnm::kTextureChannelX, Gnm::kTextureChannelX, Gnm::kTextureChannelX);
			srcTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we won't bind this texture as an RWTexture, so it's safe to declare it read-only.
			Gnmx::Toolkit::SurfaceUtil::copyTextureToRenderTarget(gfxc, &m_backBuffer->m_renderTarget, &srcTexture);
			Gnmx::Toolkit::synchronizeRenderTargetGraphicsToCompute(&gfxc.m_dcb, &m_backBuffer->m_renderTarget);
			break;
		case kFrameworkDisplayBufferDepth:
			srcTexture.initFromDepthRenderTarget(&m_backBuffer->m_depthTarget, false);
			srcTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we won't bind this texture as an RWTexture, so it's safe to declare it read-only.
			Gnmx::Toolkit::SurfaceUtil::copyTextureToRenderTarget(gfxc, &m_backBuffer->m_renderTarget, &srcTexture);
			Gnmx::Toolkit::synchronizeRenderTargetGraphicsToCompute(&gfxc.m_dcb, &m_backBuffer->m_renderTarget);
			break;
		}
	}
	if(m_config.m_renderText)	// repair broken line-draw characters
	{
		struct Constants
		{
			uint32_t m_screenWidthInCharacters;
			uint32_t m_screenHeightInCharacters;
		};
		Constants* constants = (Constants*)gfxc.allocateFromCommandBuffer(sizeof(Constants), Gnm::kEmbeddedDataAlignment4);
		constants->m_screenWidthInCharacters = m_backBuffer->m_screen.m_widthInCharacters;
		constants->m_screenHeightInCharacters = m_backBuffer->m_screen.m_heightInCharacters;
		Gnm::Buffer constantBuffer;
		constantBuffer.initAsConstantBuffer(constants, sizeof(Constants));
		constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

		gfxc.setCsShader(s_dbg_font_repair.m_shader, &s_dbg_font_repair.m_offsetsTable);
		gfxc.setRwBuffers(Gnm::kShaderStageCs, 0, 1, &m_backBuffer->m_screen.m_cellBuffer);
		gfxc.setConstantBuffers(Gnm::kShaderStageCs, 0, 1, &constantBuffer);
		gfxc.dispatch(((m_backBuffer->m_screen.m_widthInCharacters - 2) + 7) / 8, ((m_backBuffer->m_screen.m_heightInCharacters - 2) + 7) / 8, 1);

		volatile uint64_t *label = (volatile uint64_t*)gfxc.allocateFromCommandBuffer(sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8);
		uint32_t zero = 0;
		gfxc.writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
		gfxc.writeAtEndOfShader(Gnm::kEosCsDone, (void*)label, 0x1); // tell the CP to write a 1 into the memory only when all compute shaders have finished
		gfxc.waitOnAddress((void*)label, 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1); // tell the CP to wait until the memory has the val 1
		Gnmx::Toolkit::synchronizeComputeToGraphics(&gfxc.m_dcb);
	}
}

void Framework::GnmSampleFramework::RenderHud(sce::Gnmx::GnmxGfxContext& gfxc, int dbgFontHorizontalPixelOffset, int dbgFontVerticalPixelOffset)
{
	if(m_config.m_renderText)
	{
		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl( Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways );
		dsc.setDepthEnable(false);
		dsc.setStencilEnable(false);
		gfxc.setDepthStencilControl( dsc );
		m_backBuffer->m_screen.render(gfxc, &m_backBuffer->m_renderTarget, m_config.m_log2CharacterWidth, m_config.m_log2CharacterHeight, dbgFontHorizontalPixelOffset, dbgFontVerticalPixelOffset);
	}
}

void Framework::GnmSampleFramework::AppendEndFramePackets(sce::Gnmx::GnmxGfxContext& gfxc)
{
	m_cpuTimerExcludingFramework.stop();
	if(false == m_alreadyPreparedHud)
	{
		PrepareHud(gfxc);
		RenderHud(gfxc, 0, 0);
	}
	m_alreadyPreparedHud = false;

	const unsigned indexOfBufferCpuIsWriting = getIndexOfBufferCpuIsWriting();
    
    if(m_buffer[indexOfBufferCpuIsWriting].m_renderTarget.getDccCompressionEnable())
	    Gnmx::decompressDccSurface(&gfxc, &m_buffer[indexOfBufferCpuIsWriting].m_renderTarget); // implicit fast-clear as well.

	m_buffer[indexOfBufferCpuIsWriting].m_expectedLabel = m_frameCount;

	if(kGpuEop == m_config.m_whoQueuesFlips && m_config.m_interruptOnFlip)
	    gfxc.writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, const_cast<uint64_t*>(m_buffer[indexOfBufferCpuIsWriting].m_label), m_buffer[indexOfBufferCpuIsWriting].m_expectedLabel, Gnm::kCacheActionNone);
    else
	    gfxc.writeImmediateAtEndOfPipeWithInterrupt(Gnm::kEopFlushCbDbCaches, const_cast<uint64_t*>(m_buffer[indexOfBufferCpuIsWriting].m_label), m_buffer[indexOfBufferCpuIsWriting].m_expectedLabel, Gnm::kCacheActionNone);
}

void Framework::GnmSampleFramework::SubmitCommandBufferOnly(sce::Gnmx::GnmxGfxContext& gfxc)
{
	if(0 == m_frameCount)
	{
		if(m_config.m_dumpPackets)
		{
			// Dump the PM4 packet for the first frame:
			char fileName[kMaxPath];
			strncpy(fileName, m_szExePath, kMaxPath);
			strncat(fileName, "dlog.pm4", kMaxPath-strlen(fileName)-1);
			FILE *dlog = fopen(fileName, "w");
			if( dlog )
			{
				if (gfxc.m_submissionCount > 0) {
					Gnm::Pm4Dump::SubmitRange aSubmit[Gnmx::BaseGfxContext::kMaxNumStoredSubmissions + 1];
					for (unsigned iSubmit = 0; iSubmit < gfxc.m_submissionCount; ++iSubmit) {
						aSubmit[iSubmit].m_startDwordOffset = gfxc.m_submissionRanges[iSubmit].m_dcbStartDwordOffset;
						aSubmit[iSubmit].m_sizeInDwords = gfxc.m_submissionRanges[iSubmit].m_dcbSizeInDwords;
					}
					aSubmit[gfxc.m_submissionCount].m_startDwordOffset = gfxc.m_currentDcbSubmissionStart - gfxc.m_dcb.m_beginptr;
					aSubmit[gfxc.m_submissionCount].m_sizeInDwords = gfxc.m_dcb.m_cmdptr - gfxc.m_currentDcbSubmissionStart;
					Gnm::Pm4Dump::dumpPm4PacketStream(dlog, gfxc.m_dcb.m_beginptr, gfxc.m_submissionCount + 1, aSubmit);
				} else
					Gnm::Pm4Dump::dumpPm4PacketStream(dlog, &gfxc.m_dcb);
				fclose(dlog);
			}

			if (gfxc.m_acb.m_cmdptr != gfxc.m_acb.m_beginptr)
			{
				strncpy(fileName, m_szExePath, kMaxPath);
				strncat(fileName, "alog.pm4", kMaxPath-strlen(fileName)-1);
				FILE *alog = fopen(fileName, "w");
				if ( alog )
				{
					if (gfxc.m_submissionCount > 0) {
						Gnm::Pm4Dump::SubmitRange aSubmit[Gnmx::BaseGfxContext::kMaxNumStoredSubmissions + 1];
						for (unsigned iSubmit = 0; iSubmit < gfxc.m_submissionCount; ++iSubmit) {
							aSubmit[iSubmit].m_startDwordOffset = gfxc.m_submissionRanges[iSubmit].m_acbStartDwordOffset;
							aSubmit[iSubmit].m_sizeInDwords = gfxc.m_submissionRanges[iSubmit].m_acbSizeInDwords;
						}   
						aSubmit[gfxc.m_submissionCount].m_startDwordOffset = gfxc.m_currentAcbSubmissionStart - gfxc.m_acb.m_beginptr;
						aSubmit[gfxc.m_submissionCount].m_sizeInDwords = gfxc.m_acb.m_cmdptr - gfxc.m_currentAcbSubmissionStart;
						Gnm::Pm4Dump::dumpPm4PacketStream(alog, gfxc.m_acb.m_beginptr, gfxc.m_submissionCount + 1, aSubmit);
					} else
						Gnm::Pm4Dump::dumpPm4PacketStream(alog, &gfxc.m_acb);
					fclose(alog);
				}
			}

			strncpy(fileName, m_szExePath, kMaxPath);
			strncat(fileName, "clog.pm4", kMaxPath-strlen(fileName)-1);
			FILE *clog = fopen(fileName, "w");
			if ( clog )
			{
				if (gfxc.m_submissionCount > 0) {
					Gnm::Pm4Dump::SubmitRange aSubmit[Gnmx::BaseGfxContext::kMaxNumStoredSubmissions + 1];
					for (unsigned iSubmit = 0; iSubmit < gfxc.m_submissionCount; ++iSubmit) {
						aSubmit[iSubmit].m_startDwordOffset = gfxc.m_submissionRanges[iSubmit].m_ccbStartDwordOffset;
						aSubmit[iSubmit].m_sizeInDwords = gfxc.m_submissionRanges[iSubmit].m_ccbSizeInDwords;
					}
					aSubmit[gfxc.m_submissionCount].m_startDwordOffset = gfxc.m_currentCcbSubmissionStart - gfxc.m_ccb.m_beginptr;
					aSubmit[gfxc.m_submissionCount].m_sizeInDwords = gfxc.m_ccb.m_cmdptr - gfxc.m_currentCcbSubmissionStart;
					Gnm::Pm4Dump::dumpPm4PacketStream(clog, gfxc.m_ccb.m_beginptr, gfxc.m_submissionCount + 1, aSubmit);
				} else
					Gnm::Pm4Dump::dumpPm4PacketStream(clog, &gfxc.m_ccb);
				fclose(clog);
			}
		}
	} 
	else
	{
		m_secondsElapsedActualPerFrame = m_secondsElapsedActual / m_frameCount;
		m_secondsElapsedApparentPerFrame = m_paused ? 0 : m_secondsElapsedActualPerFrame;
	}

	const unsigned indexOfBufferCpuIsWriting = getIndexOfBufferCpuIsWriting();

	//
	// Submit the command buffer:
	//
	int32_t state;
	if(kGpuEop == m_config.m_whoQueuesFlips)
    {
        if(m_config.m_interruptOnFlip)
            state = gfxc.submitAndFlipWithEopInterrupt(m_videoInfo.handle, m_videoInfo.flip_index_base + indexOfBufferCpuIsWriting, m_config.m_flipMode, 0, sce::Gnm::kEopFlushCbDbCaches, (void *)m_buffer[indexOfBufferCpuIsWriting].m_labelForPrepareFlip, m_buffer[indexOfBufferCpuIsWriting].m_expectedLabel, sce::Gnm::kCacheActionWriteBackAndInvalidateL1andL2);
	    else
		    state = gfxc.submitAndFlip(m_videoInfo.handle, m_videoInfo.flip_index_base + indexOfBufferCpuIsWriting, m_config.m_flipMode, 0, (void *)m_buffer[indexOfBufferCpuIsWriting].m_labelForPrepareFlip, m_buffer[indexOfBufferCpuIsWriting].m_expectedLabel);
    }
    else
        state = gfxc.submit();   

	SCE_GNM_ASSERT_MSG(state == sce::Gnm::kSubmissionSuccess, "Command buffer validation error.");
	if (!m_config.m_userProvidedSubmitDone)
		Gnm::submitDone();
}

void Framework::GnmSampleFramework::displayUnsigned(sce::Gnmx::GnmxGfxContext &gfxc,uint32_t x,uint32_t y,void* value,uint32_t characters)
{
	Gnmx::Toolkit::synchronizeGraphicsToCompute(&gfxc.m_dcb);
	uint32_t* label = static_cast<uint32_t*>(gfxc.allocateFromCommandBuffer(sizeof(uint32_t),Gnm::kEmbeddedDataAlignment8));
	uint32_t zero = 0;
	gfxc.writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
	gfxc.writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches,label,1,Gnm::kCacheActionNone);
	gfxc.waitOnAddress(label,0xFFFFFFFF,Gnm::kWaitCompareFuncEqual,1);

	struct Constants
	{
		int m_cursorX;
		int m_cursorY;
		int m_characters;
		int m_screenWidthInCharacters;
		int m_windowX;
		int m_windowY;
		int m_windowWidth;
		int m_windowHeight;
	};
	Constants* constants = (Constants*)gfxc.allocateFromCommandBuffer(sizeof(Constants),Gnm::kEmbeddedDataAlignment4);
	constants->m_cursorX = x; // x position of first character
	constants->m_cursorY = y; // y position of first character
	constants->m_screenWidthInCharacters = m_backBuffer->m_window.m_screen->m_widthInCharacters;
	constants->m_characters = characters; // count of characters to print
	constants->m_windowX = m_backBuffer->m_window.m_positionXInCharactersRelativeToScreen;
	constants->m_windowY = m_backBuffer->m_window.m_positionYInCharactersRelativeToScreen;
	constants->m_windowWidth = m_backBuffer->m_window.m_widthInCharacters;
	constants->m_windowHeight = m_backBuffer->m_window.m_heightInCharacters;

	Gnm::Buffer buffer,constantBuffer;
	buffer.initAsDataBuffer(value,Gnm::kDataFormatR32Uint,1);
	buffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never bind it as RWBuffer, so read-only is OK
	constantBuffer.initAsConstantBuffer(constants,sizeof(Constants));
	constantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

	gfxc.setCsShader(s_dbg_font_unsigned.m_shader, &s_dbg_font_unsigned.m_offsetsTable);
	gfxc.setRwBuffers(Gnm::kShaderStageCs,0,1,&m_backBuffer->m_screen.m_cellBuffer);
	gfxc.setBuffers(Gnm::kShaderStageCs,0,1,&buffer);
	gfxc.setConstantBuffers(Gnm::kShaderStageCs,0,1,&constantBuffer);
	gfxc.dispatch(1,1,1);
	Gnmx::Toolkit::synchronizeComputeToCompute(&gfxc.m_dcb);
}

void Framework::GnmSampleFramework::SubmitCommandBuffer(sce::Gnmx::GnmxGfxContext& gfxc)
{
	AppendEndFramePackets(gfxc);
	SubmitCommandBufferOnly(gfxc);
}

void Framework::GnmSampleFramework::TakeScreenshot(const char *path)
{
	const auto bufferIndex = getIndexOfBufferCpuIsWriting();
	sce::Gnm::Texture texture;
	texture.initFromRenderTarget(&m_buffer[bufferIndex].m_renderTarget,false);
	Framework::saveTextureToGnf(path, &texture);
}

void Framework::GnmSampleFramework::TakeScreenshot(unsigned number, Gnm::Texture *texture)
{
	char path[kMaxPath];
	snprintf(path, kMaxPath, "/hostapp/screenshot%d.gnf", number);
	Framework::saveTextureToGnf(path,texture);
}

void Framework::GnmSampleFramework::ExecuteKeyboardOptions()
{
	if(!m_config.m_enableInput)
		return;

	m_controllerContext.update(m_secondsElapsedActual);


	if(m_controllerContext.isButtonPressed(static_cast<Input::Button>(Input::BUTTON_OPTIONS)))
		m_menuIsEnabled ^= true;
	if(m_menuIsEnabled)
	{
	  m_menuHost.m_menuStack.top()->m_menu->menuInput(&m_menuHost);
	}
}

void Framework::GnmSampleFramework::StallUntilGPUIsNotUsingBuffer(EopEventQueue *eopEventQueue, uint32_t bufferIndex)
{
	SCE_GNM_ASSERT_MSG(bufferIndex < m_config.m_buffers, "bufferIndex must be between 0 and %d.", m_config.m_buffers - 1);
    if(kGpuEop == m_config.m_whoQueuesFlips && m_config.m_interruptOnFlip)
    {
    	while(static_cast<uint32_t>(*m_buffer[bufferIndex].m_labelForPrepareFlip) != m_buffer[bufferIndex].m_expectedLabel)
	    	eopEventQueue->waitForEvent();
        return;
    }
	while(static_cast<uint32_t>(*m_buffer[bufferIndex].m_label) != m_buffer[bufferIndex].m_expectedLabel)
		eopEventQueue->waitForEvent();
    if(kGpuEop == m_config.m_whoQueuesFlips)
    {
	    volatile uint32_t spin = 0;
	    while(static_cast<uint32_t>(*m_buffer[bufferIndex].m_labelForPrepareFlip) != m_buffer[bufferIndex].m_expectedLabel)
		    ++spin;
    }
}

void Framework::GnmSampleFramework::WaitUntilBufferFinishedRendering(Buffer* buffer)
{
	StallUntilGPUIsNotUsingBuffer(&m_eopEventQueue, buffer - m_buffer);
}

void Framework::GnmSampleFramework::StallUntilGpuIsIdle()
{
	for(unsigned buffer = 0; buffer < m_config.m_buffers; ++buffer)
		StallUntilGPUIsNotUsingBuffer(&m_eopEventQueue, buffer);
}

void Framework::GnmSampleFramework::WaitForBufferAndFlip()
{
	++m_frameCount;
	if(m_frameCount >= m_config.m_frames)
		RequestTermination();
	if(m_secondsElapsedActual >= m_config.m_seconds)
		RequestTermination();

	if(!m_config.m_asynchronous)
	{
		StallUntilGPUIsNotUsingBuffer(&m_eopEventQueue, getIndexOfBufferCpuIsWriting());
		if(kCpuMainThread == m_config.m_whoQueuesFlips)
		{
			const int32_t ret = sceVideoOutSubmitFlip(m_videoInfo.handle, m_videoInfo.flip_index_base + getIndexOfBufferCpuIsWriting(), m_config.m_flipMode, 0);
			SCE_GNM_ASSERT(ret >= 0);
		}
		advanceCpuToNextBuffer(); 
		StallUntilGPUIsNotUsingBuffer(&m_eopEventQueue, getIndexOfBufferCpuIsWriting());
	}
	else
	{
		advanceCpuToNextBuffer(); 
		StallUntilGPUIsNotUsingBuffer(&m_eopEventQueue, getIndexOfBufferCpuIsWriting());
		if((kCpuMainThread == m_config.m_whoQueuesFlips) && (m_frameCount >= m_config.m_buffers))
		{
			const int32_t ret = sceVideoOutSubmitFlip(m_videoInfo.handle, m_videoInfo.flip_index_base + getIndexOfBufferCpuIsWriting(), m_config.m_flipMode, 0);
			SCE_GNM_ASSERT(ret >= 0);
		}
	}

}

void Framework::GnmSampleFramework::EndFrame(sce::Gnmx::GnmxGfxContext& gfxc)
{
#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
	// end trace of this frame.
	if(m_traceFrame)
		sceRazorGpuThreadTraceStop(&gfxc.m_dcb);

	// create a sync point
	// the GPU will write "1" to a label to signify that the trace has finished
	volatile uint64_t *label = (uint64_t*)gfxc.m_dcb.allocateFromCommandBuffer(8, Gnm::kEmbeddedDataAlignment8);
	*label = 0;
	gfxc.writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)label, 1, Gnm::kCacheActionNone);
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE

	SubmitCommandBuffer(gfxc);
	WaitForBufferAndFlip();

#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
	// wait for the trace to finish
	uint32_t wait = 0;
	while(*label != 1)
		++wait;
		
	// save the thread trace file for the frame and reset ready to perform another trace.
	if(m_traceFrame) {
		SceRazorGpuErrorCode errorCode = SCE_OK;
		errorCode = sceRazorGpuThreadTraceSave(RAZOR_GPU_THREAD_TRACE_FILENAME);
		if(errorCode != SCE_OK) {
			m_backBuffer->m_window.printf("Unable to save Razor GPU Thread Trace file. [%h]\n", errorCode);
		} else {
			m_backBuffer->m_window.printf("Saved Razor GPU Thread Trace file to %s.\n", RAZOR_GPU_THREAD_TRACE_FILENAME);
		}

		sceRazorGpuThreadTraceShutdown();
		m_traceFrame = false;
	}
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
}

void Framework::GnmSampleFramework::DisplayKeyboardOptions()
{
	m_backBuffer->m_window.setCursor(0, m_line+5);
	if(m_menuIsEnabled)
	{
		m_backBuffer->m_window.printf("%s", "Press and hold [FC 27][TRIANGLE][/] to display context-sensitive help\n\n");
		for(unsigned i=0; i<m_menuHost.m_menuStack.m_count; ++i )
		{
			MenuStackEntry* menuStackEntry = m_menuHost.m_menuStack.getEntry(i);
			menuStackEntry->m_menu->menuDisplay(&m_backBuffer->m_window, menuStackEntry->m_x, menuStackEntry->m_y);
		}
		if(m_controllerContext.isButtonDown(Input::BUTTON_TRIANGLE))
			m_menuHost.m_menuStack.top()->m_menu->menuDisplayDocumentation(&m_backBuffer->m_window);
	}
	else
		m_backBuffer->m_window.printf("Press the OPTIONS button to open the menu.\n\n");
}

void Framework::GnmSampleFramework::BeginFrame(sce::Gnmx::GnmxGfxContext& gfxc)
{
	DbgFont::Window fullScreenWindow;
	fullScreenWindow.initialize(&m_backBuffer->m_screen);
	fullScreenWindow.clear(s_clearCell[m_config.m_displayBuffer]);

	ExecuteKeyboardOptions();

	if(m_menuAdjustmentValidator != 0)
		m_menuAdjustmentValidator();

	gfxc.waitUntilSafeForRendering(m_videoInfo.handle, m_videoInfo.flip_index_base + getIndexOfBufferCpuIsWriting());

	m_backBuffer->m_timers.begin(gfxc.m_dcb, kFrameTimer);

	m_backBuffer->m_debugObjects.AddDebugSphere(getLightPositionInWorldSpace().getXYZ(), 0.1f, getLightColor(), m_viewProjectionMatrix);

	const float lineWidth = 0.025f;
	const float arrowWidth = 0.05f;
	const float arrowLength = 0.2f;

	if(m_config.m_enableTouchCursors && m_controllerContext.getFingers() > 0)
	{
		m_hardwareCursors.m_position[0][0] = m_controllerContext.getFinger(0).x * m_config.m_targetWidth;
		m_hardwareCursors.m_position[0][1] = m_controllerContext.getFinger(0).y * m_config.m_targetHeight;
		m_hardwareCursors.m_color[0][0] = 0xf;
		m_hardwareCursors.m_color[0][1] = 0xf;
		m_hardwareCursors.m_size[0][0] = 16;
		m_hardwareCursors.m_size[0][1] = 32;
	}
	else
	{
		m_hardwareCursors.m_size[0][0] *= 0.9f;
		m_hardwareCursors.m_size[0][1] *= 0.9f;
	}

	if(m_config.m_enableTouchCursors && m_controllerContext.getFingers() > 1)
	{
		m_hardwareCursors.m_position[1][0] = m_controllerContext.getFinger(1).x * m_config.m_targetWidth;
		m_hardwareCursors.m_position[1][1] = m_controllerContext.getFinger(1).y * m_config.m_targetHeight;
		m_hardwareCursors.m_color[1][0] = 0xf;
		m_hardwareCursors.m_color[1][1] = 0xf;
		m_hardwareCursors.m_size[1][0] = 16;
		m_hardwareCursors.m_size[1][1] = 32;
	}
	else
	{
		m_hardwareCursors.m_size[1][0] *= 0.9f;
		m_hardwareCursors.m_size[1][1] *= 0.9f;
	}

	m_backBuffer->m_screen.m_hardwareCursors = m_hardwareCursors;

	const Vector3 x = m_lightToWorldMatrix.getCol0().getXYZ();
	const Vector3 y = m_lightToWorldMatrix.getCol1().getXYZ();
	const Vector3 z = m_lightToWorldMatrix.getCol2().getXYZ();
	const Vector3 w = m_lightToWorldMatrix.getCol3().getXYZ();

	m_backBuffer->m_debugObjects.AddDebugArrow(w, w + x, lineWidth, arrowWidth, arrowLength, Vector4(1.f, 0.f, 0.f, 0.25f), m_viewProjectionMatrix);
	m_backBuffer->m_debugObjects.AddDebugArrow(w, w + y, lineWidth, arrowWidth, arrowLength, Vector4(0.f, 1.f, 0.f, 0.25f), m_viewProjectionMatrix);
	m_backBuffer->m_debugObjects.AddDebugArrow(w, w + z, lineWidth, arrowWidth, arrowLength, Vector4(0.f, 0.f, 1.f, 0.25f), m_viewProjectionMatrix);

	m_renderedDebugObjectsAlreadyInThisFrame = false;

#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
	// begin trace of this frame.
	if(m_traceFrame) {
		
		// populate parameters structure for Razor GPU Thread Trace.
		SceRazorGpuThreadTraceParams threadTraceParams;
		memset(&threadTraceParams, 0, sizeof(SceRazorGpuThreadTraceParams));
		threadTraceParams.sizeInBytes = sizeof(SceRazorGpuThreadTraceParams);

		// set up SQ counters for this trace.
		threadTraceParams.numCounters = 0;
		threadTraceParams.counterRate = SCE_RAZOR_GPU_THREAD_TRACE_COUNTER_RATE_HIGH;

		// set up instruction issue tracing, 
		// NOTE: change this to true and set the shaderEngine0ComputeUnitIndex below to enable instruction issue tracing.
		threadTraceParams.enableInstructionIssueTracing = false;

		// initialize thread trace.
		sceRazorGpuThreadTraceInit(&threadTraceParams);
		sceRazorGpuThreadTraceStart(&gfxc.m_dcb);
	}
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE

	m_cpuTimerExcludingFramework.start();
}

float Framework::GnmSampleFramework::GetSecondsElapsedApparent() const
{
	return static_cast<float>( m_secondsElapsedApparent );
}

void Framework::GnmSampleFramework::SetDepthNear( float value )
{
	m_depthNear = value;
}

void Framework::GnmSampleFramework::SetDepthFar( float value )
{
	m_depthFar = value;
}

void Framework::GnmSampleFramework::UpdateViewToWorldMatrix( const Matrix4& m )
{
	m_viewToWorldMatrix = m;
	UpdateMatrices();
}

void Framework::GnmSampleFramework::SetViewToWorldMatrix( const Matrix4& m )
{
	m_originalViewMatrix = m;
	m_viewToWorldMatrix = m;
	UpdateMatrices();
}

void Framework::GnmSampleFramework::SetProjectionMatrix( const Matrix4& m )
{
	m_projectionMatrix = m;
	UpdateMatrices();
}

#define EPSILON 0.0001

void IntersectPlanes(Vector4& p, const Vector4& p1, const Vector4& p2, const Vector4& p3)
{
	const Vector3 u = cross(p2.getXYZ(), p3.getXYZ());
	const float denom = dot(p1.getXYZ(), u);
	SCE_GNM_ASSERT(fabsf(denom) > EPSILON);
	p.setXYZ((-p1.getW() * u + cross(p1.getXYZ(), -p3.getW() * p2.getXYZ() - -p2.getW() * p3.getXYZ())) / denom);
	p.setW(1.f);
	const float a = fabsf(dot(p, p1));
	const float b = fabsf(dot(p, p2));
	const float c = fabsf(dot(p, p3));
	SCE_GNM_ASSERT(a < EPSILON);
	SCE_GNM_ASSERT(b < EPSILON);
	SCE_GNM_ASSERT(c < EPSILON);
}

void Framework::GnmSampleFramework::UpdateMatrices()
{
	m_worldToViewMatrix = inverse(m_viewToWorldMatrix);
	m_worldToLightMatrix = inverse(m_lightToWorldMatrix);

	enum {X, Y, Z, W};
	enum {Xp, Xm, Yp, Ym, Zp, Zm};
	enum {
		XpYpZp, XmYpZp, 
		XpYmZp, XmYmZp, 
		XpYpZm, XmYpZm, 
		XpYmZm, XmYmZm, 
	};
	m_viewProjectionMatrix = m_projectionMatrix * m_worldToViewMatrix;
	static bool doit = true;
	if(doit == false)
		return;
#if 1 // OGL_CLIP_SPACE
	m_plane[Xp].m_equation = m_viewProjectionMatrix.getRow(W) + m_viewProjectionMatrix.getRow(X);
	m_plane[Xm].m_equation = m_viewProjectionMatrix.getRow(W) - m_viewProjectionMatrix.getRow(X);
	m_plane[Yp].m_equation = m_viewProjectionMatrix.getRow(W) + m_viewProjectionMatrix.getRow(Y);
	m_plane[Ym].m_equation = m_viewProjectionMatrix.getRow(W) - m_viewProjectionMatrix.getRow(Y);
	m_plane[Zp].m_equation = m_viewProjectionMatrix.getRow(W) + m_viewProjectionMatrix.getRow(Z);
	m_plane[Zm].m_equation = m_viewProjectionMatrix.getRow(W) - m_viewProjectionMatrix.getRow(Z);
#else
	m_plane[Xp].m_equation = m_viewProjectionMatrix.getRow(W) + m_viewProjectionMatrix.getRow(X);
	m_plane[Xm].m_equation = m_viewProjectionMatrix.getRow(W) - m_viewProjectionMatrix.getRow(X);
	m_plane[Yp].m_equation = m_viewProjectionMatrix.getRow(W) - m_viewProjectionMatrix.getRow(Y);
	m_plane[Ym].m_equation = m_viewProjectionMatrix.getRow(W) + m_viewProjectionMatrix.getRow(Y);
	m_plane[Zp].m_equation =                                    m_viewProjectionMatrix.getRow(Z);
	m_plane[Zm].m_equation = m_viewProjectionMatrix.getRow(W) - m_viewProjectionMatrix.getRow(Z);
#endif
	for(unsigned i = 0; i < kPlanes; ++i)
	{
		const float f = 1.f / length(m_plane[i].m_equation.getXYZ());
		m_plane[i].m_equation *= f;
	}
	IntersectPlanes(m_clipCorner[XpYpZp], m_plane[Zp].m_equation, m_plane[Yp].m_equation, m_plane[Xp].m_equation);
	IntersectPlanes(m_clipCorner[XmYpZp], m_plane[Zp].m_equation, m_plane[Yp].m_equation, m_plane[Xm].m_equation);
	IntersectPlanes(m_clipCorner[XpYmZp], m_plane[Zp].m_equation, m_plane[Ym].m_equation, m_plane[Xp].m_equation);
	IntersectPlanes(m_clipCorner[XmYmZp], m_plane[Zp].m_equation, m_plane[Ym].m_equation, m_plane[Xm].m_equation);
	IntersectPlanes(m_clipCorner[XpYpZm], m_plane[Zm].m_equation, m_plane[Yp].m_equation, m_plane[Xp].m_equation);
	IntersectPlanes(m_clipCorner[XmYpZm], m_plane[Zm].m_equation, m_plane[Yp].m_equation, m_plane[Xm].m_equation);
	IntersectPlanes(m_clipCorner[XpYmZm], m_plane[Zm].m_equation, m_plane[Ym].m_equation, m_plane[Xp].m_equation);
	IntersectPlanes(m_clipCorner[XmYmZm], m_plane[Zm].m_equation, m_plane[Ym].m_equation, m_plane[Xm].m_equation);

	const unsigned u[kPlanes][4] =
	{
		{XpYpZp, XpYmZp, XpYpZm, XpYmZm}, // Xp
		{XmYpZp, XmYmZp, XmYpZm, XmYmZm}, // Xm
		{XpYpZp, XmYpZp, XpYpZm, XmYpZm}, // Yp
		{XpYmZp, XmYmZp, XpYmZm, XmYmZm}, // Ym
		{XpYpZp, XmYpZp, XpYmZp, XmYmZp}, // Zp
		{XpYpZm, XmYpZm, XpYmZm, XmYmZm}, // Zm
	};
	for(unsigned p = 0; p < kPlanes; ++p)
		for(unsigned v = 0; v < 4; ++v)
			m_plane[p].m_vertex[v] = m_clipCorner[u[p][v]];
	doit = false;
}

float Framework::GnmSampleFramework::GetDepthNear() const
{
	return m_depthNear;
}

float Framework::GnmSampleFramework::GetDepthFar() const
{
	return m_depthFar;
}

void Framework::GnmSampleFramework::appendMenuItems(uint32_t count, const MenuItem* menuItem)
{
	((CommonMenu*)m_menuCommand)->getMenu()->appendMenuItems(count, menuItem);
}

void Framework::GnmSampleFramework::insertMenuItems(uint32_t index, uint32_t count, const MenuItem* menuItem)
{
	((CommonMenu*)m_menuCommand)->getMenu()->insertMenuItems(index, count, menuItem);
}

void Framework::GnmSampleFramework::removeMenuItems(uint32_t index, uint32_t count)
{
	((CommonMenu*)m_menuCommand)->getMenu()->removeMenuItems(index, count);
}

void Framework::GnmSampleFramework::RequestTermination()
{
}

#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
void Framework::GnmSampleFramework::RequestThreadTrace()
{
	m_traceFrame = true;
}
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE

Vector4 Framework::GnmSampleFramework::getClearColor() const
{
	const uint32_t color = s_color[m_config.m_clearColor]; 
	Vector4 dest;
	dest.setX(static_cast<float>((color >> 16) & 0xFF));
	dest.setY(static_cast<float>((color >>  8) & 0xFF));
	dest.setZ(static_cast<float>((color >>  0) & 0xFF));
	dest.setW(static_cast<float>((color >> 24) & 0xFF));
	return dest / 255.f * 0.25f;
}

Vector4 Framework::GnmSampleFramework::getCameraPositionInWorldSpace() const
{
	return m_viewToWorldMatrix * Vector4(0.f, 0.f, 0.f, 1.f);
}

Vector4 Framework::GnmSampleFramework::getLightPositionInWorldSpace() const
{
	return m_lightToWorldMatrix * Vector4(0.f, 0.f, 0.f, 1.f);
}

Vector4 Framework::GnmSampleFramework::getLightPositionInViewSpace() const
{
	return m_worldToViewMatrix * getLightPositionInWorldSpace();
}

Vector4 Framework::GnmSampleFramework::getLightColor() const
{
	const uint32_t color = s_color[m_config.m_lightColor]; 
	Vector4 dest;
	dest.setX(static_cast<float>((color >> 16) & 0xFF));
	dest.setY(static_cast<float>((color >>  8) & 0xFF));
	dest.setZ(static_cast<float>((color >>  0) & 0xFF));
	dest.setW(static_cast<float>((color >> 24) & 0xFF));
	return (dest / 255.f) / 2;
}

Vector4 Framework::GnmSampleFramework::getAmbientColor() const
{
	const uint32_t color = s_color[m_config.m_ambientColor]; 
	Vector4 dest;
	dest.setX(static_cast<float>((color >> 16) & 0xFF));
	dest.setY(static_cast<float>((color >>  8) & 0xFF));
	dest.setZ(static_cast<float>((color >>  0) & 0xFF));
	dest.setW(static_cast<float>((color >> 24) & 0xFF));
	return (dest / 255.f) / 128;
}

Framework::GnmSampleFramework::CpuTimer::CpuTimer() 
: m_clocks(0) 
{
}

void Framework::GnmSampleFramework::CpuTimer::stop()
{
	m_end = performanceCounter();
	m_clocks = m_end - m_begin;
}

void Framework::GnmSampleFramework::CpuTimer::start()
{
	m_begin = performanceCounter();
}

uint64_t Framework::GnmSampleFramework::CpuTimer::clocks() const
{
	return m_clocks;
}

double Framework::GnmSampleFramework::CpuTimer::seconds() const
{
	return (double)clocks() / (double)performanceFrequency();
}

void Framework::GnmSampleFramework::advanceCpuToNextBuffer()
{
	m_indexOfLastBufferCpuWrote = m_indexOfBufferCpuIsWriting;
	m_indexOfBufferCpuIsWriting = (m_indexOfBufferCpuIsWriting + 1) % m_config.m_buffers;

	m_backBufferIndex = m_indexOfBufferCpuIsWriting;
	m_backBuffer = m_buffer + m_backBufferIndex;
}

int32_t Framework::GnmSampleFramework::flipToBuffer(uint32_t bufferIndex)
{
	return sceVideoOutSubmitFlip(m_videoInfo.handle, m_videoInfo.flip_index_base + bufferIndex, m_config.m_flipMode, 0);
}

int32_t Framework::GnmSampleFramework::FlipThread::callback()
{
	unsigned m_bufferToFlipTo = 0;
	EopEventQueue eopEventQueue("Framework Worker Thread Queue");
	while(isRunning())
	{
		m_framework->StallUntilGPUIsNotUsingBuffer(&eopEventQueue, m_bufferToFlipTo);
		m_framework->flipToBuffer(m_bufferToFlipTo);
		m_bufferToFlipTo = (m_bufferToFlipTo + 1) % m_framework->m_config.m_buffers;
	}
	return SCE_OK;
}

Framework::GnmSampleFramework::FlipThread::FlipThread(GnmSampleFramework *framework)
: Thread("Framework Worker Thread")
, m_framework(framework)
{
}

void Framework::createCommandBuffer(sce::Gnmx::GnmxGfxContext *commandBuffer,GnmSampleFramework *framework,unsigned buffer)
{
	enum { kCommandBufferSizeInBytes = 1 * 1024 * 1024 };
#if  SCE_GNMX_ENABLE_GFX_LCUE
	const uint32_t kNumBatches = 100; // Estimated number of batches to support within the resource buffer
	const uint32_t resourceBufferSize = Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(Gnmx::LightweightConstantUpdateEngine::kResourceBufferGraphics,kNumBatches);
	void *drawCommandBuffer;
	void *resourceBuffer;
	framework->m_allocators.allocate(&drawCommandBuffer,SCE_KERNEL_WB_ONION,kCommandBufferSizeInBytes,4,Gnm::kResourceTypeDrawCommandBufferBaseAddress,"Buffer %d Draw Command Buffer",buffer);
	framework->m_allocators.allocate(&resourceBuffer,SCE_KERNEL_WC_GARLIC,resourceBufferSize,4,Gnm::kResourceTypeGenericBuffer,"Buffer %d Resource Buffer",buffer);
	commandBuffer->init
		(
		drawCommandBuffer,kCommandBufferSizeInBytes,		// Draw command buffer
		resourceBuffer,resourceBufferSize,					// Resource buffer
		NULL																															// Global resource table 
		);
#else
	const uint32_t kNumRingEntries = 64;
	const uint32_t cueHeapSize = Gnmx::ConstantUpdateEngine::computeHeapSize(kNumRingEntries);
	void *constantUpdateEngine;
	void *drawCommandBuffer;
	void *constantCommandBuffer;
	framework->m_allocators.allocate(&constantUpdateEngine,SCE_KERNEL_WC_GARLIC,cueHeapSize,4,Gnm::kResourceTypeGenericBuffer,"Buffer %d Constant Update Engine",buffer);
	framework->m_allocators.allocate(&drawCommandBuffer,SCE_KERNEL_WB_ONION,kCommandBufferSizeInBytes,4,Gnm::kResourceTypeDrawCommandBufferBaseAddress,"Buffer %d Draw Command Buffer",buffer);
	framework->m_allocators.allocate(&constantCommandBuffer,SCE_KERNEL_WB_ONION,kCommandBufferSizeInBytes,4,Gnm::kResourceTypeConstantCommandBufferBaseAddress,"Buffer %d Constant Command Buffer",buffer);
	commandBuffer->init(constantUpdateEngine,kNumRingEntries,drawCommandBuffer,kCommandBufferSizeInBytes,constantCommandBuffer,kCommandBufferSizeInBytes);
#endif
}

