/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#define ENABLE_PERF_COUNTERS 1
#define ENABLE_USER_PERF_COUNTER 1
#define ENABLE_PERF_COUNTER_WINDOWING 1

#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/frame.h"
#include <gnmx/resourcebarrier.h>
#include "perfcountermanager.h"
#include <assert.h>

#include <gnm.h>
#include <gpu_address.h>
using namespace sce;
using namespace sce::Gnmx;

#include <stdio.h>
#include <stdlib.h>
#include "std_cbuffer.h"

#ifdef _MSC_VER
# define LL "I64"
# define snprintf _snprintf
#else
# define LL "ll"
#endif

namespace
{
	Framework::GnmSampleFramework framework;

	class RuntimeOptions
	{
	public:
		bool m_useDepthFastClear;
		bool m_useShadowFastClear;
		bool m_useHtile;
		RuntimeOptions()
		: m_useDepthFastClear(true)
		, m_useShadowFastClear(true)
		, m_useHtile(true)
		{
		}
		bool UseHtile() const
		{
			return m_useHtile;
		}
		bool UseDepthFastClear() const
		{
			return m_useHtile && m_useDepthFastClear;
		}
		bool UseShadowFastClear() const
		{
			return m_useHtile && m_useShadowFastClear;
		}
	};
	RuntimeOptions g_runtimeOptions;

	class MenuCommandBoolNeedsHTile : public Framework::MenuCommandBool
	{
	public:
		MenuCommandBoolNeedsHTile(bool* target)
		: Framework::MenuCommandBool(target)
		{
		}
		virtual bool isEnabled() const
		{
			return g_runtimeOptions.m_useHtile;
		}
	};

	double shadowClearMs = 0;
	double depthColorClearMs = 0;
	float shadowMapSize = 1.f;

	Framework::MenuCommandBool htile(&g_runtimeOptions.m_useHtile);
	MenuCommandBoolNeedsHTile depthFastClear(&g_runtimeOptions.m_useDepthFastClear);
	MenuCommandBoolNeedsHTile shadowFastClear(&g_runtimeOptions.m_useShadowFastClear);
	Framework::MenuCommandFloat menuShadowMapSize(&shadowMapSize, 0.5f, 4.f, 0.25f);

#if ENABLE_PERF_COUNTERS && ENABLE_PERF_COUNTER_WINDOWING
	enum WindowToDrawCall {
		kWindowDisable = 0,
		kWindowShadowClear,
		kWindowShadowMap,
		kWindowShadowDecompress,
		kWindowTargetClear,
		kWindowIceLogo,
		kWindowPlanes,
		kWindowFrame,
	};
	enum WindowMethod {
		kWindowMethodSetPerfmonEnable = 0,
		kWindowMethodSyncAndSample,
	};
	uint32_t s_windowToDrawCall = 0;
	uint32_t s_windowMethod = 0;
	Framework::MenuItemText const kaWindowToDrawCallText[] = {
		{ "Disable", "Disable windowing and profile entire frame" },
		{ "Shadow clear", "Window to shadow depth clear" },
		{ "Shadow map", "Window to shadow render" },
		{ "Shadow decompress", "Window to shadow depth decompress" },
		{ "Target clear", "Window to render target color/depth clear" },
		{ "ICE logo", "Window to ICE logo draw call" },
		{ "Planes", "Window to background plane draw calls" },
		{ "Frame", "Window to entire frame except Framework menu/debug rendering" },
	};
	Framework::MenuCommandEnum windowToDrawCall(kaWindowToDrawCallText, (uint32_t)(sizeof(kaWindowToDrawCallText)/sizeof(kaWindowToDrawCallText[0])), &s_windowToDrawCall);
	Framework::MenuItemText const kaWindowMethodText[] = {
		{ "SetPerfmonEnable", "Use dcb->setPerfmonEnable for windowing" },
		{ "Sync and Sample", "Synchronize and sample at draw call boundaries" },
	};
	Framework::MenuCommandEnum windowMethod(kaWindowMethodText, (uint32_t)(sizeof(kaWindowMethodText)/sizeof(kaWindowMethodText[0])), &s_windowMethod);
#endif

	bool s_bCounterSelectsDirty = false;
#if ENABLE_USER_PERF_COUNTER
	unsigned int perfCounterHwUnit = PerfCounters::kNumHwUnits, perfCounterSelect = 0;

	static const char* aszCpPerfCounterName[Gnm::kNumCpPerfCounters] = { NULL };
	static const char* aszCpcPerfCounterName[Gnm::kNumCpcPerfCounters] = { NULL };
	static const char* aszCpfPerfCounterName[Gnm::kNumCpfPerfCounters] = { NULL };
	static const char* aszWdPerfCounterName[Gnm::kNumWdPerfCounters] = { NULL };
	static const char* aszIaPerfCounterName[Gnm::kNumIaPerfCounters] = { NULL };
	static const char* aszGdsPerfCounterName[Gnm::kNumGdsPerfCounters] = { NULL };
	static const char* aszTcaPerfCounterName[Gnm::kNumTcaPerfCounters] = { NULL };
	static const char* aszTcsPerfCounterName[Gnm::kNumTcsPerfCounters] = { NULL };
	static const char* aszTccPerfCounterName[Gnm::kNumTccPerfCounters] = { NULL };
	static const char* aszVgtPerfCounterName[Gnm::kNumVgtPerfCounters] = { NULL };
	static const char* aszPaSuPerfCounterName[Gnm::kNumPaSuPerfCounters] = { NULL };
	static const char* aszPaScPerfCounterName[Gnm::kNumPaScPerfCounters] = { NULL };
	static const char* aszSpiPerfCounterName[Gnm::kNumSpiPerfCounters] = { NULL };
	static const char* aszSxPerfCounterName[Gnm::kNumSxPerfCounters] = { NULL };
	static const char* aszCbPerfCounterName[Gnm::kNumCbPerfCounters] = { NULL };
	static const char* aszDbPerfCounterName[Gnm::kNumDbPerfCounters] = { NULL };
	static const char* aszSqPerfCounterName[Gnm::kNumSqPerfCounters] = { NULL };
	static const char* aszTaPerfCounterName[Gnm::kNumTaPerfCounters] = { NULL };
	static const char* aszTdPerfCounterName[Gnm::kNumTdPerfCounters] = { NULL };
	static const char* aszTcpPerfCounterName[Gnm::kNumTcpPerfCounters] = { NULL };
	static const char* aszTcaPerfCounterVirtualizedName[Gnm::kVirtualizedRangeEndTcaPerfCounter - Gnm::kVirtualizedRangeStartTcaPerfCounter + 1] = {NULL};
	static const char* aszTcsPerfCounterVirtualizedName[Gnm::kVirtualizedRangeEndTcsPerfCounter - Gnm::kVirtualizedRangeStartTcsPerfCounter + 1] = {NULL};
	static const char* aszTccPerfCounterVirtualizedName[Gnm::kVirtualizedRangeEndTccPerfCounter - Gnm::kVirtualizedRangeStartTccPerfCounter + 1] = {NULL};

	static void init_perfcounter_names()
	{
#define ENUM_VALUE_CpPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumCpPerfCounters	)	aszCpPerfCounterName	[val] = "kCpPerfCounter"	#sym;
#define ENUM_VALUE_CpcPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumCpcPerfCounters	)	aszCpcPerfCounterName	[val] = "kCpcPerfCounter"	#sym;
#define ENUM_VALUE_CpfPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumCpfPerfCounters	)	aszCpfPerfCounterName	[val] = "kCpfPerfCounter"	#sym;
#define ENUM_VALUE_WdPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumWdPerfCounters	)	aszWdPerfCounterName	[val] = "kWdPerfCounter"	#sym;
#define ENUM_VALUE_IaPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumIaPerfCounters	)	aszIaPerfCounterName	[val] = "kIaPerfCounter"	#sym;
#define ENUM_VALUE_GdsPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumGdsPerfCounters	)	aszGdsPerfCounterName	[val] = "kGdsPerfCounter"	#sym;
#define ENUM_VALUE_TcaPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumTcaPerfCounters	)	aszTcaPerfCounterName	[val] = "kTcaPerfCounter"	#sym;
#define ENUM_VALUE_TcsPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumTcsPerfCounters	)	aszTcsPerfCounterName	[val] = "kTcsPerfCounter"	#sym;
#define ENUM_VALUE_TccPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumTccPerfCounters	)	aszTccPerfCounterName	[val] = "kTccPerfCounter"	#sym;
#define ENUM_VALUE_VgtPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumVgtPerfCounters	)	aszVgtPerfCounterName	[val] = "kVgtPerfCounter"	#sym;
#define ENUM_VALUE_PaSuPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumPaSuPerfCounters	)	aszPaSuPerfCounterName	[val] = "kPaSuPerfCounter"	#sym;
#define ENUM_VALUE_PaScPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumPaScPerfCounters	)	aszPaScPerfCounterName	[val] = "kPaScPerfCounter"	#sym;
#define ENUM_VALUE_SpiPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumSpiPerfCounters	)	aszSpiPerfCounterName	[val] = "kSpiPerfCounter"	#sym;
#define ENUM_VALUE_SxPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumSxPerfCounters	)	aszSxPerfCounterName	[val] = "kSxPerfCounter"	#sym;
#define ENUM_VALUE_CbPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumCbPerfCounters	)	aszCbPerfCounterName	[val] = "kCbPerfCounter"	#sym;
#define ENUM_VALUE_DbPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumDbPerfCounters	)	aszDbPerfCounterName	[val] = "kDbPerfCounter"	#sym;
#define ENUM_VALUE_SqPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumSqPerfCounters	)	aszSqPerfCounterName	[val] = "kSqPerfCounter"	#sym;
#define ENUM_VALUE_TaPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumTaPerfCounters	)	aszTaPerfCounterName	[val] = "kTaPerfCounter"	#sym;
#define ENUM_VALUE_TdPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumTdPerfCounters	)	aszTdPerfCounterName	[val] = "kTdPerfCounter"	#sym;
#define ENUM_VALUE_TcpPerfCounter(	sym, val, desc_string)	if (val < Gnm::kNumTcpPerfCounters	)	aszTcpPerfCounterName	[val] = "kTcpPerfCounter"	#sym;
#define ENUM_VALUE_TcaPerfCounterVirtualized(sym, val, desc_string) \
	if (val >= Gnm::kVirtualizedRangeStartTcaPerfCounter && val <= Gnm::kVirtualizedRangeEndTcaPerfCounter) \
		aszTcaPerfCounterVirtualizedName[val - Gnm::kVirtualizedRangeStartTcaPerfCounter] = "kTcaPerfCounterVirtualized"#sym;
#define ENUM_VALUE_TcsPerfCounterVirtualized(sym, val, desc_string) \
	if (val >= Gnm::kVirtualizedRangeStartTcsPerfCounter && val <= Gnm::kVirtualizedRangeEndTcsPerfCounter) \
		aszTcsPerfCounterVirtualizedName[val - Gnm::kVirtualizedRangeStartTcsPerfCounter] = "kTcsPerfCounterVirtualized"#sym;
#define ENUM_VALUE_TccPerfCounterVirtualized(sym, val, desc_string) \
	if (val >= Gnm::kVirtualizedRangeStartTccPerfCounter && val <= Gnm::kVirtualizedRangeEndTccPerfCounter) \
		aszTccPerfCounterVirtualizedName[val - Gnm::kVirtualizedRangeStartTccPerfCounter] = "kTccPerfCounterVirtualized"#sym;
#include "perfcounter_constants.inl"
#undef ENUM_VALUE_CpPerfCounter
#undef ENUM_VALUE_CpcPerfCounter
#undef ENUM_VALUE_CpfPerfCounter
#undef ENUM_VALUE_WdPerfCounter
#undef ENUM_VALUE_IaPerfCounter
#undef ENUM_VALUE_GdsPerfCounter
#undef ENUM_VALUE_TcaPerfCounter
#undef ENUM_VALUE_TcsPerfCounter
#undef ENUM_VALUE_TccPerfCounter
#undef ENUM_VALUE_VgtPerfCounter
#undef ENUM_VALUE_PaSuPerfCounter
#undef ENUM_VALUE_PaScPerfCounter
#undef ENUM_VALUE_SpiPerfCounter
#undef ENUM_VALUE_SxPerfCounter
#undef ENUM_VALUE_CbPerfCounter
#undef ENUM_VALUE_DbPerfCounter
#undef ENUM_VALUE_SqPerfCounter
#undef ENUM_VALUE_TaPerfCounter
#undef ENUM_VALUE_TdPerfCounter
#undef ENUM_VALUE_TcpPerfCounter
#undef ENUM_VALUE_TcaPerfCounterVirtualized
#undef ENUM_VALUE_TcsPerfCounterVirtualized
#undef ENUM_VALUE_TccPerfCounterVirtualized
	}

	unsigned int getMaxPerfCounterIndex(PerfCounters::HardwareUnit eHwUnit)
	{
		switch (eHwUnit) {
		case PerfCounters::kHwUnitCp:		return Gnm::kNumCpPerfCounters - 1;
		case PerfCounters::kHwUnitCpc:		return Gnm::kNumCpcPerfCounters - 1;
		case PerfCounters::kHwUnitCpf:		return Gnm::kNumCpfPerfCounters - 1;
		case PerfCounters::kHwUnitIa:		return Gnm::kNumIaPerfCounters - 1;
		case PerfCounters::kHwUnitWd:		return Gnm::kNumWdPerfCounters - 1;
		case PerfCounters::kHwUnitGds:		return Gnm::kNumGdsPerfCounters - 1;
		case PerfCounters::kHwUnitTca:		return Gnm::kVirtualizedRangeEndTcaPerfCounter;
		case PerfCounters::kHwUnitTcs:		return Gnm::kVirtualizedRangeEndTcsPerfCounter;
		case PerfCounters::kHwUnitTcc:		return Gnm::kVirtualizedRangeEndTccPerfCounter;
		case PerfCounters::kHwUnitVgt:		return Gnm::kNumVgtPerfCounters - 1;
		case PerfCounters::kHwUnitPaSu:		return Gnm::kNumPaSuPerfCounters - 1;
		case PerfCounters::kHwUnitPaSc:		return Gnm::kNumPaScPerfCounters - 1;
		case PerfCounters::kHwUnitPaScQuadPacker:	return Gnm::kNumPaScPerfCounters - 1;
		case PerfCounters::kHwUnitSpi:		return Gnm::kNumSpiPerfCounters - 1;
		case PerfCounters::kHwUnitSx:		return Gnm::kNumSxPerfCounters - 1;
		case PerfCounters::kHwUnitCb:		return Gnm::kNumCbPerfCounters - 1;
		case PerfCounters::kHwUnitDb:		return Gnm::kNumDbPerfCounters - 1;
		case PerfCounters::kHwUnitSq:		return Gnm::kNumSqPerfCounters - 1;
		case PerfCounters::kHwUnitTa:		return Gnm::kNumTaPerfCounters - 1;
		case PerfCounters::kHwUnitTd:		return Gnm::kNumTdPerfCounters - 1;
		case PerfCounters::kHwUnitTcp:		return Gnm::kNumTcpPerfCounters - 1;
		default:			break;
		}
		return -1;
	}
	char const* getPerfCounterName(PerfCounters::HardwareUnit eHwUnit, unsigned int select)
	{
		unsigned int val = select & 0x3FF;
		unsigned int virtVal;
		static const char* kszInvalid = "INVALID";
		switch (eHwUnit) {
		case PerfCounters::kHwUnitCp:		return (val < Gnm::kNumCpPerfCounters	&& aszCpPerfCounterName[val]) ? aszCpPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitCpc:		return (val < Gnm::kNumCpcPerfCounters	&& aszCpcPerfCounterName[val]) ? aszCpcPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitCpf:		return (val < Gnm::kNumCpfPerfCounters	&& aszCpfPerfCounterName[val]) ? aszCpfPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitIa:		return (val < Gnm::kNumIaPerfCounters	&& aszIaPerfCounterName[val]) ? aszIaPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitWd:		return (val < Gnm::kNumWdPerfCounters	&& aszWdPerfCounterName[val]) ? aszWdPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitGds:		return (val < Gnm::kNumGdsPerfCounters	&& aszGdsPerfCounterName[val]) ? aszGdsPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitTca: 
			virtVal = val - Gnm::kVirtualizedRangeStartTcaPerfCounter;
			if (val < Gnm::kNumTcaPerfCounters && aszTcaPerfCounterName[val])
				return aszTcaPerfCounterName[val];
			else if (virtVal < (Gnm::kVirtualizedRangeEndTcaPerfCounter - Gnm::kVirtualizedRangeStartTcaPerfCounter + 1))
				return aszTcaPerfCounterVirtualizedName[virtVal];
			return kszInvalid;
		case PerfCounters::kHwUnitTcs:
			virtVal = val - Gnm::kVirtualizedRangeStartTcsPerfCounter;
			if (val < Gnm::kNumTcsPerfCounters && aszTcsPerfCounterName[val])
				return aszTcsPerfCounterName[val];
			else if (virtVal < (Gnm::kVirtualizedRangeEndTcsPerfCounter - Gnm::kVirtualizedRangeStartTcsPerfCounter + 1))
				return aszTcsPerfCounterVirtualizedName[virtVal];
			return kszInvalid;
		case PerfCounters::kHwUnitTcc:
			virtVal = val - Gnm::kVirtualizedRangeStartTccPerfCounter;
			if (val < Gnm::kNumTccPerfCounters && aszTccPerfCounterName[val])
				return aszTccPerfCounterName[val];
			else if (virtVal < (Gnm::kVirtualizedRangeEndTccPerfCounter - Gnm::kVirtualizedRangeStartTccPerfCounter + 1))
				return aszTccPerfCounterVirtualizedName[virtVal];
			return kszInvalid;
		case PerfCounters::kHwUnitVgt:		return (val < Gnm::kNumVgtPerfCounters	&& aszVgtPerfCounterName[val]) ? aszVgtPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitPaSu:		return (val < Gnm::kNumPaSuPerfCounters	&& aszPaSuPerfCounterName[val]) ? aszPaSuPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitPaSc:		return (val < Gnm::kNumPaScPerfCounters	&& aszPaScPerfCounterName[val]) ? aszPaScPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitPaScQuadPacker:	return (val < Gnm::kNumPaScPerfCounters	&& aszPaScPerfCounterName[val]) ? aszPaScPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitSpi:		return (val < Gnm::kNumSpiPerfCounters	&& aszSpiPerfCounterName[val]) ? aszSpiPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitSx:		return (val < Gnm::kNumSxPerfCounters	&& aszSxPerfCounterName[val]) ? aszSxPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitCb:		return (val < Gnm::kNumCbPerfCounters	&& aszCbPerfCounterName[val]) ? aszCbPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitDb:		return (val < Gnm::kNumDbPerfCounters	&& aszDbPerfCounterName[val]) ? aszDbPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitSq:		return (val < Gnm::kNumSqPerfCounters	&& aszSqPerfCounterName[val]) ? aszSqPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitTa:		return (val < Gnm::kNumTaPerfCounters	&& aszTaPerfCounterName[val]) ? aszTaPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitTd:		return (val < Gnm::kNumTdPerfCounters	&& aszTdPerfCounterName[val]) ? aszTdPerfCounterName[val] : kszInvalid;
		case PerfCounters::kHwUnitTcp:		return (val < Gnm::kNumTcpPerfCounters	&& aszTcpPerfCounterName[val]) ? aszTcpPerfCounterName[val] : kszInvalid;
		default:			break;
		}
		return kszInvalid;
	}

	Framework::MenuItemText aMenuHwUnits[PerfCounters::kNumHwUnits + 1] = {
		{	"CP",	"Command processor graphics (CPG, aka CP), 1 per GPU" },
		{	"CPC",	"Command processor compute (CPC), 1 per GPU" },
		{	"CPF",	"Command processor fetch (CPF), 1 per GPU" },
		{	"IA",	"Input assembler (IA), 1 per GPU" },
		{	"WD",	"Work distributor (WD), 1 per GPU" },
		{	"GDS",	"Global data store (GDS), 1 per GPU" },
		{	"TCA",	"Texture cache arbiter (TCA), 1 per GPU; aka L2 cache controller" },
		{	"TCS",	"Texture cache ? (TCS), 1 per GPU; aka L2 cache bypass" },
		{	"TCC",	"Texture cache controller (TCC), 8 per GPU; aka L2 cache bank" },
		{	"VGT",	"Vertex geometry tessellator (VGT), 1 per SE" },
		{	"PA_SU","Primitive assembler setup unit (PA-SU), 1 per SE" },
		{	"PA_SC","Primitive assembler scan converter (PA-SC), 1 per SE" },
		{	"PA_SC(QP)",	"Primitive assembler scan converter (PA-SC) quad packer counter, 2 per SE" },
		{	"SPI",	"Shader processor input (SPI), 1 per SE" },
		{	"SX",	"Shader export (SX), 1 per SE" },
		{	"CB",	"Color buffer (CB), 4 per SE" },
		{	"DB",	"Depth buffer (DB), 4 per SE" },
		{	"SQ",	"Sequencer (SQ), 1 per CU; includes sequencer cache (SQC), aka L1 instruction and constant caches, I-cache and K-cache" },
		{	"TA",	"Texture address (TA), 1 per CU" },
		{	"TD",	"Texture data (TD), 1 per CU" },
		{	"TCP",	"Texture cache pipe (TCP), 1 per CU; aka L1 texture cache" },
		{	"None", "No user performance counter selected" },
	};
	class MenuCommandPerfCounterSelect : public Framework::MenuCommandUint
	{
		uint32_t *m_pHwUnit;
	public:
		MenuCommandPerfCounterSelect(uint32_t *pSelect, uint32_t *pHwUnit)
			: Framework::MenuCommandUint(pSelect, 0, 0, (const char*)NULL)
			, m_pHwUnit(pHwUnit)
		{
			m_wrapMode = kWrap;
		}
		virtual void adjust(int delta) const
		{
			Framework::MenuCommandUint::adjust(delta);
			s_bCounterSelectsDirty = true;
			if (*m_pHwUnit == PerfCounters::kHwUnitPaSc) {
				if (PerfCounters::isQuadPackerPerfCounter((sce::Gnm::PaScPerfCounter)*m_target))
					*m_pHwUnit = PerfCounters::kHwUnitPaScQuadPacker;
			} else if (*m_pHwUnit == PerfCounters::kHwUnitPaScQuadPacker) {
				if (!PerfCounters::isQuadPackerPerfCounter((sce::Gnm::PaScPerfCounter)*m_target))
					*m_pHwUnit = PerfCounters::kHwUnitPaSc;
			} else if (*m_pHwUnit == PerfCounters::kHwUnitTca) {
				// skip long interval between physical and virtual counters
				if (*m_target == Gnm::kNumTcaPerfCounters)
					*m_target = Gnm::kVirtualizedRangeStartTcaPerfCounter;
				else if (*m_target == Gnm::kVirtualizedRangeStartTcaPerfCounter - 1)
					*m_target = Gnm::kNumTcaPerfCounters - 1;
			} else if (*m_pHwUnit == PerfCounters::kHwUnitTcs) {
				// skip long interval between physical and virtual counters
				if (*m_target == Gnm::kNumTcsPerfCounters)
					*m_target = Gnm::kVirtualizedRangeStartTcsPerfCounter;
				else if (*m_target == Gnm::kVirtualizedRangeStartTcsPerfCounter - 1)
					*m_target = Gnm::kNumTcsPerfCounters - 1;
			} else if (*m_pHwUnit == PerfCounters::kHwUnitTcc) {
				// skip long interval between physical and virtual counters
				if (*m_target == Gnm::kNumTccPerfCounters)
					*m_target = Gnm::kVirtualizedRangeStartTccPerfCounter;
				else if (*m_target == Gnm::kVirtualizedRangeStartTccPerfCounter - 1)
					*m_target = Gnm::kNumTccPerfCounters - 1;
			}
		}
		virtual bool isDocumented() const {return true;}
		virtual bool isEnabled() const { return m_max != 0xFFFFFFFF; }
		unsigned int getValue() const { return *m_target; }
		void setMax(unsigned int maxValue) 
		{ 
			m_max = maxValue; 
			if (maxValue == 0xFFFFFFFF) {
				*m_target = 0;
				s_bCounterSelectsDirty = true;
			} else if (*m_target > m_max) { 
				*m_target = m_max; 
				s_bCounterSelectsDirty = true;
			}
			if (*m_pHwUnit == PerfCounters::kHwUnitPaSc) {
				if (PerfCounters::isQuadPackerPerfCounter((sce::Gnm::PaScPerfCounter)*m_target))
					*m_pHwUnit = PerfCounters::kHwUnitPaScQuadPacker;
			} else if (*m_pHwUnit == PerfCounters::kHwUnitPaScQuadPacker) {
				if (!PerfCounters::isQuadPackerPerfCounter((sce::Gnm::PaScPerfCounter)*m_target))
					*m_pHwUnit = PerfCounters::kHwUnitPaSc;
			} else if (*m_pHwUnit == PerfCounters::kHwUnitTca) {
				if (*m_target >= Gnm::kNumTcaPerfCounters && *m_target < Gnm::kVirtualizedRangeStartTcaPerfCounter)
					*m_target = Gnm::kNumTcaPerfCounters - 1;
			} else if (*m_pHwUnit == PerfCounters::kHwUnitTcs) {
				if (*m_target >= Gnm::kNumTcsPerfCounters && *m_target < Gnm::kVirtualizedRangeStartTcsPerfCounter)
					*m_target = Gnm::kNumTcsPerfCounters - 1;
			} else if (*m_pHwUnit == PerfCounters::kHwUnitTcc) {
				if (*m_target >= Gnm::kNumTccPerfCounters && *m_target < Gnm::kVirtualizedRangeStartTccPerfCounter)
					*m_target = Gnm::kNumTccPerfCounters - 1;
			}
		}
	};
	class MenuCommandPerfCounterHwUnit : public Framework::MenuCommandEnum
	{
		MenuCommandPerfCounterSelect* m_pSelect;
	public:
		MenuCommandPerfCounterHwUnit(uint32_t *pHwUnit, MenuCommandPerfCounterSelect *pSelect)
			: Framework::MenuCommandEnum(aMenuHwUnits, PerfCounters::kNumHwUnits + 1, pHwUnit, NULL)
			, m_pSelect(pSelect)
		{
			unsigned int maxIndex = getMaxPerfCounterIndex((PerfCounters::HardwareUnit)*m_target);
			m_pSelect->setMax(maxIndex);
		}
		virtual void adjust(int delta) const
		{
			if ((*m_target > PerfCounters::kHwUnitPaSc) != (*m_target + delta > PerfCounters::kHwUnitPaSc))
				delta += (delta > 0) ? 1 : -1;
			Framework::MenuCommandEnum::adjust(delta);
			unsigned int maxIndex = getMaxPerfCounterIndex((PerfCounters::HardwareUnit)*m_target);
			m_pSelect->setMax(maxIndex);
			s_bCounterSelectsDirty = true;
		}
	};
	MenuCommandPerfCounterSelect menuPerfCounterIndex(&perfCounterSelect, &perfCounterHwUnit);
	MenuCommandPerfCounterHwUnit menuPerfCounterHwUnit(&perfCounterHwUnit, &menuPerfCounterIndex);
#endif	//#if ENABLE_USER_PERF_COUNTER

	const Framework::MenuItem menuItem[] =
	{
		{{"Fast depth clear", "Use HTILE to clear scene depth buffer"}, &depthFastClear},
		{{"Shadow fast clear", "Use HTILE to clear shadow depth buffer"}, &shadowFastClear},
		{{"HTILE", "Are HTILE buffers used?"}, &htile},
		{{"Shadow map size", "Length of shadow map edge in world space"}, &menuShadowMapSize},
#if ENABLE_PERF_COUNTERS && ENABLE_PERF_COUNTER_WINDOWING
		{{"Counter window", "Draw call window to collect counters from"}, &windowToDrawCall},
		{{"Window method", "What method to use to window counters"}, &windowMethod},
#endif
#if ENABLE_PERF_COUNTERS && ENABLE_USER_PERF_COUNTER
		{{"Counter unit", "Hardware unit to collect custom counter on"}, &menuPerfCounterHwUnit},
		{{"Counter select", "Hardware unit counter to collect"}, &menuPerfCounterIndex},
#endif	//#if ENABLE_PERF_COUNTERS && ENABLE_USER_PERF_COUNTER
	};
	enum { kMenuItems = sizeof(menuItem)/sizeof(menuItem[0]) };
}

#if ENABLE_PERF_COUNTERS
# if ENABLE_PERF_COUNTER_WINDOWING
#  define BEGIN_PERF_COUNTER_FRAME() \
	if (s_windowToDrawCall == kWindowDisable || s_windowMethod == kWindowMethodSetPerfmonEnable) { \
		perfCounters.samplePerfCounters(gfxc); \
		perfCounters.readPerfCounterValues(gfxc, aPerfCounterValues, 0); \
	}
#  define END_PERF_COUNTER_FRAME() \
	if (s_windowToDrawCall == kWindowDisable || s_windowMethod == kWindowMethodSetPerfmonEnable) { \
		perfCounters.samplePerfCounters(gfxc); \
		perfCounters.readPerfCounterValues(gfxc, aPerfCounterValues, 1); \
	}
#  define BEGIN_PERF_COUNTER_WINDOW(window) \
	if (s_windowToDrawCall == window) { \
		if (s_windowMethod == kWindowMethodSetPerfmonEnable) { \
			gfxc->m_dcb.setPerfmonEnable(Gnm::kPerfmonEnable); \
		} else { \
			perfCounters.samplePerfCounters(gfxc); \
			perfCounters.readPerfCounterValues(gfxc, aPerfCounterValues, 0); \
			gfxc->writeTimestampAtEndOfPipe(Gnm::kEopFlushAndInvalidateCbDbCaches, aPerfCounterValues + (kNumPerfCountersTotal*2 + 2), Gnm::kCacheActionNone); \
		} \
	}
#  define END_PERF_COUNTER_WINDOW(window) \
	if (s_windowToDrawCall == window) { \
		if (s_windowMethod == kWindowMethodSetPerfmonEnable) { \
			gfxc->m_dcb.setPerfmonEnable(Gnm::kPerfmonDisable); \
		} else { \
			gfxc->writeTimestampAtEndOfPipe(Gnm::kEopFlushAndInvalidateCbDbCaches, aPerfCounterValues + (kNumPerfCountersTotal*2 + 3), Gnm::kCacheActionNone); \
			perfCounters.samplePerfCounters(gfxc); \
			perfCounters.readPerfCounterValues(gfxc, aPerfCounterValues, 1); \
		} \
	}
# else
#  define BEGIN_PERF_COUNTER_FRAME() \
	perfCounters.samplePerfCounters(gfxc); \
	perfCounters.readPerfCounterValues(gfxc, aPerfCounterValues, 0);
#  define END_PERF_COUNTER_FRAME() \
	perfCounters.samplePerfCounters(gfxc); \
	perfCounters.readPerfCounterValues(gfxc, aPerfCounterValues, 1);
#  define BEGIN_PERF_COUNTER_WINDOW(window)
#  define END_PERF_COUNTER_WINDOW(window)
# endif
#else
# define BEGIN_PERF_COUNTER_FRAME()
# define END_PERF_COUNTER_FRAME()
# define BEGIN_PERF_COUNTER_WINDOW(window) 
# define END_PERF_COUNTER_WINDOW(window) 
#endif

int main(int argc, const char *argv[])
{
	framework.processCommandLine(argc, argv);

	framework.initialize( "Perfcounter", 
		"Sample code to illustrate how to select and read out performance counters.", 
		"This sample program collects a selection of performance counters before and after rendering an ICE logo and two planes with shadow map, and calculates and displays the total counter deltas on screen on the following frame. The following setting needs to be done before running the sample program to display performance counters. In Neighborhood for PlayStation(R)4, set [Target Settings] -> [Debug Settings] -> [Graphics] -> [PA Debug] to YES.");

	framework.appendMenuItems( kMenuItems, menuItem );

	class Frame
	{
	public:
		sce::Gnmx::GnmxGfxContext m_commandBuffer;
		Matrix4Unaligned* m_shadowMapConstants;
		Constants* m_iceLogoConstants;
		Constants* m_quadConstants;
		Constants* m_quad2Constants;
		Gnm::DepthRenderTarget m_shadowDepthTarget;
		Gnm::Texture m_shadowTexture;
		Gnmx::ResourceBarrier m_shadowBarrier;
	};
	Frame frames[3];
	SCE_GNM_ASSERT(framework.m_config.m_buffers <= 3);
	for(unsigned buffer = 0; buffer < framework.m_config.m_buffers; ++buffer)
	{
		Frame *frame = &frames[buffer];
		createCommandBuffer(&frame->m_commandBuffer,&framework,buffer);
		framework.m_allocators.allocate((void**)&frame->m_shadowMapConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_shadowMapConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Shadow Map Constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_iceLogoConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_iceLogoConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d ICE Logo Constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_quadConstants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_quadConstants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Quad Constants",buffer);
		framework.m_allocators.allocate((void**)&frame->m_quad2Constants,SCE_KERNEL_WB_ONION,sizeof(*frame->m_quad2Constants),4,Gnm::kResourceTypeConstantBufferBaseAddress,"Buffer %d Quad 2 Constants",buffer);

		const uint32_t shadowWidth = 512;
		const uint32_t shadowHeight = 512;
		Gnm::SizeAlign shadowHtileSizeAlign;
		Gnm::DataFormat shadowDepthFormat = Gnm::DataFormat::build(Gnm::kZFormat32Float);
		Gnm::TileMode shadowDepthTileMode;
		GpuAddress::computeSurfaceTileMode(Gnm::getGpuMode(),&shadowDepthTileMode,GpuAddress::kSurfaceTypeDepthOnlyTarget,shadowDepthFormat,1);
		Gnm::SizeAlign shadowDepthTargetSizeAlign = Gnmx::Toolkit::init(&frame->m_shadowDepthTarget,shadowWidth,shadowHeight,shadowDepthFormat.getZFormat(),Gnm::kStencilInvalid,shadowDepthTileMode,Gnm::kNumFragments1,NULL,&shadowHtileSizeAlign);

		void *shadowZ;
		framework.m_allocators.allocate(&shadowZ,SCE_KERNEL_WC_GARLIC,shadowDepthTargetSizeAlign,Gnm::kResourceTypeDepthRenderTargetBaseAddress,"Buffer %d Depth Render Target",buffer);
		frame->m_shadowDepthTarget.setZReadAddress(shadowZ);
		frame->m_shadowDepthTarget.setZWriteAddress(shadowZ);

		void *shadowHtile;
		framework.m_allocators.allocate(&shadowHtile,SCE_KERNEL_WC_GARLIC,shadowHtileSizeAlign,Gnm::kResourceTypeDepthRenderTargetHTileAddress,"Buffer %d Htile",buffer);
		frame->m_shadowDepthTarget.setHtileAddress(shadowHtile);

		frame->m_shadowTexture.initFromDepthRenderTarget(&frame->m_shadowDepthTarget,false);
		frame->m_shadowTexture.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as RWTexture, so it's OK to declare read-only.

		frame->m_shadowBarrier.init(&frame->m_shadowDepthTarget, Gnmx::ResourceBarrier::kUsageDepthSurface, Gnmx::ResourceBarrier::kUsageRoTexture);
	}

#if ENABLE_PERF_COUNTERS
	bool const bIsUserPaEnabled = sce::Gnm::isUserPaEnabled();
#endif

	void *fetchShader, *shadowFetchShader;
	Gnmx::VsShader *vertexShader = Framework::LoadVsMakeFetchShader(&fetchShader, "/app0/perfcounter-sample/shader_vv.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&vertexShader_offsetsTable, Gnm::kShaderStageVs, vertexShader);

	Gnmx::VsShader *shadowVertexShader = Framework::LoadVsMakeFetchShader(&shadowFetchShader, "/app0/perfcounter-sample/basic_vv.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache shadowVertexShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&shadowVertexShader_offsetsTable, Gnm::kShaderStageVs, shadowVertexShader);

	Gnmx::PsShader *logoPixelShader = Framework::LoadPsShader("/app0/perfcounter-sample/logo_p.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache logoPixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&logoPixelShader_offsetsTable, Gnm::kShaderStagePs, logoPixelShader);

	Gnmx::PsShader *planePixelShader = Framework::LoadPsShader("/app0/perfcounter-sample/plane_p.sb", &framework.m_allocators);
	Gnmx::InputOffsetsCache planePixelShader_offsetsTable;
	Gnmx::generateInputOffsetsCache(&planePixelShader_offsetsTable, Gnm::kShaderStagePs, planePixelShader);

	Framework::SimpleMesh shadowCasterMesh, quadMesh;
	LoadSimpleMesh(&framework.m_allocators, &shadowCasterMesh, "/app0/assets/icetext.mesh");
	BuildQuadMesh(&framework.m_allocators, "Quad", &quadMesh, 10);

	Gnm::Sampler samplers[3];
	for(int s=0; s<2; s++)
	{
		samplers[s].init();
		samplers[s].setMipFilterMode(Gnm::kMipFilterModeLinear);
		samplers[s].setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);
	}
	samplers[2].init();
	samplers[2].setDepthCompareFunction(Gnm::kDepthCompareLessEqual);
	samplers[2].setWrapMode(Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel, Gnm::kWrapModeClampLastTexel);

	Gnm::Texture textures[2];
	Framework::GnfError loadError = Framework::kGnfErrorNone;
	loadError = Framework::loadTextureFromGnf(&textures[0], "/app0/assets/icelogo-color.gnf", 0, &framework.m_allocators);
	SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );
	loadError = Framework::loadTextureFromGnf(&textures[1], "/app0/assets/icelogo-normal.gnf", 0, &framework.m_allocators);
	SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );

	textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as RWTexture, so it's OK to declare read-only.
	textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as RWTexture, so it's OK to declare read-only.

	// Projection Matrix
	framework.SetViewToWorldMatrix(inverse(Matrix4::lookAt(Point3(0,0,0), Point3(0,-0.5f,-1), Vector3(0,1,0))));

	// Set up frame timing
	enum { kShadowTimer, kDepthColorClearTimer, kTimers };
	const Gnm::SizeAlign timerSizeAlign = Gnmx::Toolkit::Timers::getRequiredBufferSizeAlign(kTimers);
	void *timerPtr = framework.m_garlicAllocator.allocate(timerSizeAlign);
	Gnmx::Toolkit::Timers timers;
	timers.initialize(timerPtr, kTimers);

	const Matrix4 clipSpaceToTextureSpace = Matrix4::translation(Vector3(0.5f, 0.5f, 0.5f)) * Matrix4::scale(Vector3(0.5f, -0.5f, 0.5f));

	// Allocate space for reading out performance counters
#if ENABLE_PERF_COUNTERS
	PerfCounters::setGpuMode();
#if ENABLE_USER_PERF_COUNTER
	init_perfcounter_names();
#endif	//#if ENABLE_USER_PERF_COUNTER
	PerfCounters::PerfCounterManager perfCounters;
	const uint32_t kNumPerfCountersTotal = 256;
	uint64_t* aPerfCounterValues = (uint64_t*)framework.m_onionAllocator.allocate((kNumPerfCountersTotal + 2)*2*8, 8);	//technically, only Gnm::kMinimumBufferAlignmentInBytes is required by the GPU, but reading out uint64_t from the CPU is more efficient with 8-byte alignment
	memset(aPerfCounterValues, 0xDBDBDBDB, (kNumPerfCountersTotal + 2)*2*8);
	s_bCounterSelectsDirty = true;
#endif	//#if ENABLE_PERF_COUNTERS

	while( !framework.m_shutdownRequested )
	{
		Framework::GnmSampleFramework::Buffer *bufferCpuIsWriting = framework.m_buffer + framework.getIndexOfBufferCpuIsWriting();
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		GnmxGfxContext *gfxc = &frame->m_commandBuffer;

		gfxc->reset();
#if ENABLE_PERF_COUNTERS
		gfxc->writeTimestampAtEndOfPipe(Gnm::kEopFlushAndInvalidateCbDbCaches, aPerfCounterValues + (kNumPerfCountersTotal*2 + 0), Gnm::kCacheActionNone);
#endif	//#if ENABLE_PERF_COUNTERS

#if ENABLE_PERF_COUNTERS
		bool bCountersSelected = s_bCounterSelectsDirty;
		perfCounters.stopPerfCounters(gfxc);
		if (s_bCounterSelectsDirty && bIsUserPaEnabled) {
			s_bCounterSelectsDirty = false;
			// select performance counters to collect
			PerfCounters::HwUnitTarget broadcast;
			perfCounters.reset();
			//			perfCounters.addMeasurement(0, Gnm::IaPerfCounterSelect(Gnm::kIaPerfCounterIaDmaReturn, Gnm::kPerfmonCounterModeAccum), "IA dma count (32 bytes each)");
			// 			perfCounters.addMeasurement(0, Gnm::TcaPerfCounterSelect(Gnm::kTcaPerfCounterBusy, Gnm::kPerfmonCounterModeAccum), "TCA busy cycles (x2)");
			perfCounters.addMeasurement(broadcast, 0, Gnm::TccPerfCounterSelect(Gnm::kTccPerfCounterReq, Gnm::kPerfmonCounterModeAccum), "TCC L1 texture cache misses to L2 (64 bytes each)");
			perfCounters.addMeasurement(broadcast, 1, Gnm::TccPerfCounterSelect(Gnm::kTccPerfCounterMiss, Gnm::kPerfmonCounterModeAccum), "TCC L2 cache misses to memory (64 bytes each)");
			perfCounters.addMeasurement(0, Gnm::TcsPerfCounterSelect(Gnm::kTcsPerfCounterReq, Gnm::kPerfmonCounterModeAccum), "TCS (Onion+) requests to memory (64 bytes each)");
			// 			perfCounters.addMeasurement(broadcast, 0, Gnm::VgtPerfCounterSelect(Gnm::kVgtPerfCounterVgtBusy, Gnm::kPerfmonCounterModeAccum), "VGT busy cycles (x2)");
			// 			Gnm::PaSuPerfCounter aPaSuCounterSelect[] =	{ Gnm::kPaSuPerfCounterPaInputPrim,		Gnm::kPaSuPerfCounterSuOutputPrim,			};
			// 			char const* aszPaSuCounterName[] =			{ "PA-SU input prim count",				"PA-SU output prim count", };
			// 			perfCounters.addMeasurementSet(broadcast, 0, (uint32_t)(sizeof(aPaSuCounterSelect)/sizeof(aPaSuCounterSelect[0])), aPaSuCounterSelect, aszPaSuCounterName);
			// 			Gnm::PaScPerfCounter aPaScCounterSelect[] =	{ Gnm::kPaScPerfCounterQz0QuadCount,			Gnm::kPaScPerfCounterQz1QuadCount,			Gnm::kPaScPerfCounterP0HizQuadCount,		Gnm::kPaScPerfCounterP1HizQuadCount,		Gnm::kPaScPerfCounterP0DetailQuadCount,				Gnm::kPaScPerfCounterP1DetailQuadCount,				Gnm::kPaScPerfCounterEarlyzQuadCount,	};
			// 			char const* aszPaScCounterName[] =			{ "PA-SC pre HiZ quad count (quad-z pipe 0)",	"PA-SC pre HiZ quad count (quad-z pipe 1)",	"PA-SC post HiZ quad count (DB pipe 0)",	"PA-SC post HiZ quad count (DB pipe 1)",	"PA-SC post detail quad count (DB pipe 0 detail)",	"PA-SC post detail quad count (DB pipe 1 detail)",	"PA-SC post EarlyZ quad count",			};
			// 			perfCounters.addMeasurementSet(broadcast, 0, (uint32_t)(sizeof(aPaScCounterSelect)/sizeof(aPaScCounterSelect[0])), aPaScCounterSelect, aszPaScCounterName);
			// 			perfCounters.addMeasurement(0, Gnm::kCpPerfCounterAlwaysCount, "CP total cycles");
			// 			char szNameBuf[256];
			// 			uint32_t numCus = Gnm::kNumCusPerSe*Gnm::kNumSeUnits;
			// 			snprintf(szNameBuf, sizeof(szNameBuf)-1, "SQ total cycles (x%u)", numCus);
			// 			perfCounters.addMeasurement(broadcast, 0, Gnm::SqPerfCounterSelect(Gnm::kSqPerfCounterCycles, Gnm::kPerfmonCounterModeAccum, Gnm::kPerfmonStreamingCounterModeOff, 0xF, 0xF, 0xF), szNameBuf);
			// 			snprintf(szNameBuf, sizeof(szNameBuf)-1, "TA busy cycles (x%u)", numCus);
			// 			perfCounters.addMeasurement(broadcast, 0, Gnm::TaPerfCounterSelect(Gnm::kTaPerfCounterTaBusy, Gnm::kPerfmonCounterModeAccum), szNameBuf);
			// 			snprintf(szNameBuf, sizeof(szNameBuf)-1, "TD busy cycles (x%u)", numCus);
			// 			perfCounters.addMeasurement(broadcast, 0, Gnm::TdPerfCounterSelect(Gnm::kTdPerfCounterTdBusy, Gnm::kPerfmonCounterModeAccum), szNameBuf);
#if ENABLE_USER_PERF_COUNTER
			switch (perfCounterHwUnit) {
			case PerfCounters::kHwUnitCp:		perfCounters.addMeasurement(PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, (Gnm::CpPerfCounter)perfCounterSelect);	break;
			case PerfCounters::kHwUnitCpc:		perfCounters.addMeasurement(PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, (Gnm::CpcPerfCounter)perfCounterSelect);	break;
			case PerfCounters::kHwUnitCpf:		perfCounters.addMeasurement(PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, (Gnm::CpfPerfCounter)perfCounterSelect);	break;
			case PerfCounters::kHwUnitIa:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::IaPerfCounterSelect((Gnm::IaPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum));	break;
			case PerfCounters::kHwUnitWd:		perfCounters.addMeasurement(PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::WdPerfCounterSelect((Gnm::WdPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum));	break;
			case PerfCounters::kHwUnitGds:		perfCounters.addMeasurement(PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, (Gnm::GdsPerfCounter)perfCounterSelect);	break;
			case PerfCounters::kHwUnitTca: {
				Gnm::MappedTcaPerfCounter mapped;
				int32_t ret = Gnm::getMappedTcaPerfCounterFromVirtual((Gnm::TcaPerfCounter)perfCounterSelect, &mapped);
                SCE_GNM_ASSERT_MSG(ret == SCE_GNM_OK, "Invalid TCA perfcounter");
                SCE_GNM_UNUSED(ret);
				if (mapped.instance == -1)
					perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::TcaPerfCounterSelect((Gnm::TcaPerfCounter)mapped.counter, Gnm::kPerfmonCounterModeAccum));
				else
					perfCounters.addMeasurement(mapped.instance, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::TcaPerfCounterSelect((Gnm::TcaPerfCounter)mapped.counter, Gnm::kPerfmonCounterModeAccum));
				break;
			}
			case PerfCounters::kHwUnitTcs:		perfCounters.addMeasurement(PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::TcsPerfCounterSelect((Gnm::TcsPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum));	break;
			case PerfCounters::kHwUnitTcc:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::TccPerfCounterSelect((Gnm::TccPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum));	break;
			case PerfCounters::kHwUnitVgt:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::VgtPerfCounterSelect((Gnm::VgtPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum));	break;
			case PerfCounters::kHwUnitPaSu:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, (Gnm::PaSuPerfCounter)perfCounterSelect);	break;
			case PerfCounters::kHwUnitPaSc:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, (Gnm::PaScPerfCounter)perfCounterSelect);	break;
			case PerfCounters::kHwUnitPaScQuadPacker:	perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, (Gnm::PaScPerfCounter)perfCounterSelect);	break;
			case PerfCounters::kHwUnitSpi:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, (Gnm::SpiPerfCounter)perfCounterSelect);	break;
			case PerfCounters::kHwUnitSx:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, (Gnm::SxPerfCounter)perfCounterSelect);	break;
			case PerfCounters::kHwUnitCb:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::CbPerfCounterSelect((Gnm::CbPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum));	break;
			case PerfCounters::kHwUnitDb:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::DbPerfCounterSelect((Gnm::DbPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum));	break;
			case PerfCounters::kHwUnitSq:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::SqPerfCounterSelect((Gnm::SqPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum, 0xF, 0xF, 0xF)); break;
			case PerfCounters::kHwUnitTa:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::TaPerfCounterSelect((Gnm::TaPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum));	break;
			case PerfCounters::kHwUnitTd:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::TdPerfCounterSelect((Gnm::TdPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum));	break;
			case PerfCounters::kHwUnitTcp:		perfCounters.addMeasurement(broadcast, PerfCounters::kaNumPerfCounterSlots[perfCounterHwUnit] - 1, Gnm::TcpPerfCounterSelect((Gnm::TcpPerfCounter)perfCounterSelect, Gnm::kPerfmonCounterModeAccum));	break;
			default:			break;
			}
#endif	//#if ENABLE_USER_PERF_COUNTER
			assert(perfCounters.getRequiredSizeofCounterValueBuffer(2) <= kNumPerfCountersTotal*2*8);

			perfCounters.resetPerfCounters(gfxc);
			perfCounters.selectPerfCounters(gfxc);
		}
# if ENABLE_PERF_COUNTER_WINDOWING
		perfCounters.startPerfCounters(gfxc, Gnm::kPerfmonNoSample, (s_windowToDrawCall && s_windowMethod == kWindowMethodSetPerfmonEnable) ? Gnm::kPerfmonEnableModeCountContextTrue : Gnm::kPerfmonEnableModeAlwaysCount);
# else
		perfCounters.startPerfCounters(gfxc, Gnm::kPerfmonNoSample, Gnm::kPerfmonEnableModeAlwaysCount);
# endif
#endif	//#if ENABLE_PERF_COUNTERS

		framework.BeginFrame(*gfxc);
		srand((int)framework.GetSecondsElapsedApparent());

#if ENABLE_PERF_COUNTERS
		if (!bIsUserPaEnabled) {
			bufferCpuIsWriting->m_window.printf("\nPA Debug is not enabled in target settings\n");
		} else if (!bCountersSelected) {
			// parse and print counter values from previous frame
			bufferCpuIsWriting->m_window.setCursor(0, 24);
			int64_t delta_timestamp = aPerfCounterValues[kNumPerfCountersTotal*2 + 1] - aPerfCounterValues[kNumPerfCountersTotal*2 + 0];
			float const fGpuCodeClockFrequency = Gnm::getGpuCoreClockFrequency();
			float const fTimestampToMs = 1000.0f / fGpuCodeClockFrequency;
# if ENABLE_PERF_COUNTER_WINDOWING
			int64_t delta_timestamp_window = delta_timestamp;
			if (s_windowToDrawCall && s_windowMethod != kWindowMethodSetPerfmonEnable) {
				delta_timestamp_window = aPerfCounterValues[kNumPerfCountersTotal*2 + 3] - aPerfCounterValues[kNumPerfCountersTotal*2 + 2]; 
				bufferCpuIsWriting->m_window.printf("%12" LL "d %.3fms (%7.2ffps) EOP_FRAME_TIME %12" LL "d %.3fms EOP_WINDOW_TIME\n", delta_timestamp, (float)delta_timestamp*fTimestampToMs, (delta_timestamp*9999.99f > fGpuCodeClockFrequency) ? fGpuCodeClockFrequency / (float)delta_timestamp : 9999.99f, delta_timestamp_window, (float)delta_timestamp_window*fTimestampToMs);
			} else
# endif
				bufferCpuIsWriting->m_window.printf("%12" LL "d %.3fms (%7.2ffps) EOP_FRAME_TIME\n", delta_timestamp, (float)delta_timestamp*fTimestampToMs, (delta_timestamp*9999.99f > fGpuCodeClockFrequency) ? fGpuCodeClockFrequency / (float)delta_timestamp : 9999.99f);
			for (uint32_t iMeasurement = 0, numMeasurements = perfCounters.getNumMeasurements(); iMeasurement < numMeasurements; ++iMeasurement) {
				uint32_t numCounterValuesPerSlot = perfCounters.getMeasurementNumCounterValuesPerSlot(iMeasurement);
				PerfCounters::HardwareUnit eHwUnit = perfCounters.getMeasurementSelection(iMeasurement)->getHardwareUnit();
				float fDeltaToPercentFrame = (delta_timestamp > 0) ? 100.0f/((float)delta_timestamp*(float)getNumHwUnits(eHwUnit)) : 0.0f;
				for (uint32_t iSlot = 0, numSlots = perfCounters.getMeasurementSelection(iMeasurement)->getNumCounterSlots(); iSlot < numSlots; ++iSlot) {
					int64_t delta = perfCounters.getPerfCounterTotalValueDelta(aPerfCounterValues, iMeasurement, iSlot, 1);
					if (!framework.m_config.m_displayTimings)
						delta = rand()%65536;
					if (perfCounters.isMeasurementNamed(iMeasurement, iSlot)) {
						bufferCpuIsWriting->m_window.printf("%12" LL "d %7.3f%%F %s\n", delta, (float)delta*fDeltaToPercentFrame, perfCounters.getMeasurementName(iMeasurement, iSlot));
					} else {
						uint32_t select = menuPerfCounterIndex.getValue();
#if ENABLE_USER_PERF_COUNTER
						bufferCpuIsWriting->m_window.printf("%12" LL "d %7.3f%%F SELECT:0x%08x %s\n", delta, (float)delta*fDeltaToPercentFrame, select, getPerfCounterName(eHwUnit, select));
#else	//#if ENABLE_USER_PERF_COUNTER
						bufferCpuIsWriting->m_window.printf("%12" LL "d %7.3f%%F SELECT:0x%08x\n", delta, (float)delta*fDeltaToPercentFrame, select);
#endif	//#if ENABLE_USER_PERF_COUNTER ... #else
					}
					if (numCounterValuesPerSlot > 1) {
						for (uint32_t iCounterValue = 0; iCounterValue < numCounterValuesPerSlot; ++iCounterValue) {
							int64_t deltaUnit = perfCounters.getPerfCounterValueDelta(aPerfCounterValues, iMeasurement, iCounterValue*numSlots + iSlot, 1);
							if (!framework.m_config.m_displayTimings)
								deltaUnit = rand()%65536;
							bufferCpuIsWriting->m_window.printf("%s%7" LL "d", (iCounterValue == 0 ? "  {" : ","), deltaUnit);
						}
						bufferCpuIsWriting->m_window.printf(" }\n");
					}
				}
			}
		}
#endif	//#if ENABLE_PERF_COUNTERS

		BEGIN_PERF_COUNTER_FRAME();
		BEGIN_PERF_COUNTER_WINDOW(kWindowFrame);

		const float d = shadowMapSize * 0.5f;
		const Matrix4 shadowTargetProjection = Matrix4::frustum(-d, d, -d, d, 1, 100.0f);
		const Matrix4 shadowTargetViewProjection = shadowTargetProjection * framework.m_worldToLightMatrix;
		const Matrix4 shadowMapViewProjection = clipSpaceToTextureSpace * shadowTargetViewProjection;

		frame->m_shadowDepthTarget.setHtileAccelerationEnable(g_runtimeOptions.UseHtile());
		bufferCpuIsWriting->m_depthTarget.setHtileAccelerationEnable(g_runtimeOptions.UseHtile());

		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc->setPrimitiveSetup(primSetupReg);

		const float radians = framework.GetSecondsElapsedApparent();
		const Matrix4 logoMatrix = Matrix4::translation(Vector3(0,0,-5)) * Matrix4::rotationZYX(Vector3(-radians,-radians,0));
		const Matrix4 quadMatrix = Matrix4::translation(Vector3(0,-5,-5)) * Matrix4::rotationX(-1.57f);

		// Render Shadow Map
		timers.begin(gfxc->m_dcb, kShadowTimer);
		BEGIN_PERF_COUNTER_WINDOW(kWindowShadowClear);
		if(g_runtimeOptions.UseShadowFastClear())
		{
			Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(*gfxc, &frame->m_shadowDepthTarget);
			gfxc->setDepthClearValue(1.f);
		}
		else
			Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &frame->m_shadowDepthTarget, 1.f);
		gfxc->setRenderTargetMask(0x0); // we're about to render a shadow map, so we should turn off rasterization to color buffers.
		END_PERF_COUNTER_WINDOW(kWindowShadowClear);
		timers.end(gfxc->m_dcb, kShadowTimer);

		BEGIN_PERF_COUNTER_WINDOW(kWindowShadowMap);
		gfxc->setDepthRenderTarget(&frame->m_shadowDepthTarget);
		gfxc->setupScreenViewport(0, 0, frame->m_shadowDepthTarget.getWidth(), frame->m_shadowDepthTarget.getHeight(), 0.5f, 0.5f);
		{
			Gnm::ClipControl clipReg;
			clipReg.init();
			gfxc->setClipControl(clipReg);
		}

		gfxc->setVsShader(shadowVertexShader, 0, shadowFetchShader, &shadowVertexShader_offsetsTable);
		shadowCasterMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		gfxc->setRenderTargetMask(0x0);
		gfxc->setPsShader((Gnmx::PsShader*)NULL, NULL);

		Gnm::Buffer shadowMapConstantBuffer;
		shadowMapConstantBuffer.initAsConstantBuffer(frame->m_shadowMapConstants, sizeof(*frame->m_shadowMapConstants));
		shadowMapConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &shadowMapConstantBuffer);
		*frame->m_shadowMapConstants = transpose(shadowTargetViewProjection * logoMatrix);

		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		dsc.setDepthEnable(true);
		gfxc->setDepthStencilControl(dsc);

		primSetupReg.setPolygonOffsetEnable(Gnm::kPrimitiveSetupPolygonOffsetEnable, Gnm::kPrimitiveSetupPolygonOffsetEnable);
		gfxc->setPrimitiveSetup(primSetupReg);
		gfxc->setPolygonOffsetFront(5.0f, 6000.0f);
		gfxc->setPolygonOffsetBack(5.0f, 6000.0f);
		gfxc->setPolygonOffsetZFormat(Gnm::kZFormat32Float);

		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceFront);
		gfxc->setPrimitiveSetup(primSetupReg);

		gfxc->setPrimitiveType(shadowCasterMesh.m_primitiveType);
		gfxc->setIndexSize(shadowCasterMesh.m_indexType);
		gfxc->drawIndex(shadowCasterMesh.m_indexCount, shadowCasterMesh.m_indexBuffer);
		END_PERF_COUNTER_WINDOW(kWindowShadowMap);

		BEGIN_PERF_COUNTER_WINDOW(kWindowShadowDecompress);
		primSetupReg.setPolygonOffsetEnable(Gnm::kPrimitiveSetupPolygonOffsetDisable, Gnm::kPrimitiveSetupPolygonOffsetDisable);
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		gfxc->setPrimitiveSetup(primSetupReg);

		// Decompress shadow surfaces before we sample from them
		Gnmx::decompressDepthSurface(gfxc, &frame->m_shadowDepthTarget);
		Gnm::DbRenderControl dbRenderControl;
		dbRenderControl.init();
		gfxc->setDbRenderControl(dbRenderControl);		
		frame->m_shadowBarrier.write(&gfxc->m_dcb);
		END_PERF_COUNTER_WINDOW(kWindowShadowDecompress);

		BEGIN_PERF_COUNTER_WINDOW(kWindowTargetClear);
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		gfxc->setPrimitiveSetup(primSetupReg);
		// Render Main View
		{
			Gnm::ClipControl clipReg;
			clipReg.init();
			clipReg.setClipEnable(false);
			gfxc->setClipControl(clipReg);
		}

		dbRenderControl.init();
		dbRenderControl.setDepthClearEnable(true);
		dbRenderControl.setDepthTileWriteBackPolicy(Gnm::kDbTileWriteBackPolicyCompressionAllowed);
		gfxc->setDbRenderControl(dbRenderControl);
		timers.begin(gfxc->m_dcb, kDepthColorClearTimer);
		{
			Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*gfxc, &bufferCpuIsWriting->m_renderTarget, framework.getClearColor());
			if( g_runtimeOptions.UseDepthFastClear() )
			{
				Gnmx::Toolkit::SurfaceUtil::clearHtileSurface(*gfxc, &bufferCpuIsWriting->m_depthTarget);
				gfxc->setDepthClearValue(1.f);
			}
			else
			{
				Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*gfxc, &bufferCpuIsWriting->m_depthTarget, 1.f);
			}
		}
		timers.end(gfxc->m_dcb, kDepthColorClearTimer);
		END_PERF_COUNTER_WINDOW(kWindowTargetClear);

		BEGIN_PERF_COUNTER_WINDOW(kWindowIceLogo);
		gfxc->setRenderTarget(0, &bufferCpuIsWriting->m_renderTarget);
		gfxc->setDepthRenderTarget(&bufferCpuIsWriting->m_depthTarget);
		gfxc->setupScreenViewport(0, 0, bufferCpuIsWriting->m_renderTarget.getWidth(), bufferCpuIsWriting->m_renderTarget.getHeight(), 0.5f, 0.5f);
		dbRenderControl.setDepthClearEnable(false);
		gfxc->setDbRenderControl(dbRenderControl);
		gfxc->setRenderTargetMask(0xF);
		{
			Gnm::ClipControl clipReg;
			clipReg.init();
			gfxc->setClipControl(clipReg);
		}
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
		gfxc->setDepthStencilControl(dsc);

		gfxc->setVsShader(vertexShader, 0, fetchShader, &vertexShader_offsetsTable);
		shadowCasterMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		gfxc->setPsShader(logoPixelShader, &logoPixelShader_offsetsTable);

		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
		gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &frame->m_shadowTexture);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 3, samplers);

		Gnm::Buffer iceLogoConstantBuffer;
		iceLogoConstantBuffer.initAsConstantBuffer(frame->m_iceLogoConstants, sizeof(*frame->m_iceLogoConstants));
		iceLogoConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &iceLogoConstantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &iceLogoConstantBuffer);

		frame->m_iceLogoConstants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*logoMatrix);
		frame->m_iceLogoConstants->m_lightPosition = framework.getLightPositionInViewSpace();
		frame->m_iceLogoConstants->m_shadow_mvp = transpose(shadowMapViewProjection * logoMatrix);
		frame->m_iceLogoConstants->m_modelView = transpose(framework.m_worldToViewMatrix*logoMatrix);
		frame->m_iceLogoConstants->m_lightColor = framework.getLightColor();
		frame->m_iceLogoConstants->m_ambientColor = framework.getAmbientColor();
		frame->m_iceLogoConstants->m_lightAttenuation = Vector4(1, 0, 0, 0);

		gfxc->setPrimitiveType(shadowCasterMesh.m_primitiveType);
		gfxc->setIndexSize(shadowCasterMesh.m_indexType);
		gfxc->drawIndex(shadowCasterMesh.m_indexCount, shadowCasterMesh.m_indexBuffer);
		END_PERF_COUNTER_WINDOW(kWindowIceLogo);

		BEGIN_PERF_COUNTER_WINDOW(kWindowPlanes);
		gfxc->setPsShader(planePixelShader, &planePixelShader_offsetsTable);
		gfxc->setVsShader(vertexShader, 0, fetchShader, &vertexShader_offsetsTable);
		gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, textures);
		gfxc->setTextures(Gnm::kShaderStagePs, 2, 1, &frame->m_shadowTexture);
		gfxc->setSamplers(Gnm::kShaderStagePs, 0, 3, samplers);

		Gnm::Buffer quadConstantBuffer;
		quadConstantBuffer.initAsConstantBuffer(frame->m_quadConstants, sizeof(*frame->m_quadConstants));
		quadConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
		gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &quadConstantBuffer);
		gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &quadConstantBuffer);

		frame->m_quadConstants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*quadMatrix);
		frame->m_quadConstants->m_lightPosition = framework.getLightPositionInViewSpace();
		frame->m_quadConstants->m_shadow_mvp = transpose(shadowMapViewProjection * quadMatrix);
		frame->m_quadConstants->m_modelView = transpose(framework.m_worldToViewMatrix*quadMatrix);
		frame->m_quadConstants->m_lightColor = framework.getLightColor();
		frame->m_quadConstants->m_ambientColor = framework.getAmbientColor();
		frame->m_quadConstants->m_lightAttenuation = Vector4(1, 0, 0, 0);

		quadMesh.SetVertexBuffer(*gfxc, Gnm::kShaderStageVs);

		
		samplers[0].setMipFilterMode(Gnm::kMipFilterModePoint);
		gfxc->setPrimitiveType(quadMesh.m_primitiveType);
		gfxc->setIndexSize(quadMesh.m_indexType);
		gfxc->drawIndex(quadMesh.m_indexCount, quadMesh.m_indexBuffer);

		{
			const Matrix4 quad2Matrix = Matrix4::translation(Vector3(  0.0f, -2.5f,-10.0f));// * Matrix4::rotationX(-1.57f);
			Gnm::Buffer quad2ConstantBuffer;
			quad2ConstantBuffer.initAsConstantBuffer(frame->m_quad2Constants, sizeof(*frame->m_quad2Constants));
			quad2ConstantBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK
			gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &quad2ConstantBuffer);
			gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &quad2ConstantBuffer);

			frame->m_quad2Constants->m_modelViewProjection = transpose(framework.m_viewProjectionMatrix*quad2Matrix);
			frame->m_quad2Constants->m_lightPosition = framework.getLightPositionInViewSpace();
			frame->m_quad2Constants->m_shadow_mvp = transpose(shadowMapViewProjection * quad2Matrix);
			frame->m_quad2Constants->m_modelView = transpose(framework.m_worldToViewMatrix*quad2Matrix);
			frame->m_quad2Constants->m_lightColor = framework.getLightColor();
			frame->m_quad2Constants->m_ambientColor = framework.getAmbientColor();
			frame->m_quad2Constants->m_lightAttenuation = Vector4(1, 0, 0, 0);

			gfxc->drawIndex(quadMesh.m_indexCount, quadMesh.m_indexBuffer);
		}
		END_PERF_COUNTER_WINDOW(kWindowPlanes);

		END_PERF_COUNTER_WINDOW(kWindowFrame);
		END_PERF_COUNTER_FRAME();
#if ENABLE_PERF_COUNTERS
		gfxc->writeTimestampAtEndOfPipe(Gnm::kEopFlushAndInvalidateCbDbCaches, aPerfCounterValues + (kNumPerfCountersTotal*2 + 1), Gnm::kCacheActionNone);
#endif	//#if ENABLE_PERF_COUNTERS
		framework.EndFrame(*gfxc);

		shadowClearMs = timers.readTimerInMilliseconds(kShadowTimer);
		depthColorClearMs = timers.readTimerInMilliseconds(kDepthColorClearTimer);
	}

#if ENABLE_PERF_COUNTERS
	{
		Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
		GnmxGfxContext *gfxc = &frame->m_commandBuffer;
		gfxc->reset();
		framework.BeginFrame(*gfxc);
		perfCounters.resetPerfCounters(gfxc);
		framework.EndFrame(*gfxc);
	}
#endif	//#if ENABLE_PERF_COUNTERS

	Frame *frame = frames + framework.getIndexOfBufferCpuIsWriting();
	GnmxGfxContext *gfxc = &frame->m_commandBuffer;
	framework.terminate(*gfxc);
	return 0;
}
