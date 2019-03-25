/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef PERF_COUNTER_SELECTION_H
#define PERF_COUNTER_SELECTION_H

#include "../framework/sample_framework.h"
#include "../toolkit/toolkit.h"

#include <gnm.h>
#include <gnm/perfcounter_regs.h>

namespace PerfCounters {

	enum HardwareUnit
	{
		kHwUnitCp = 0,	// command processor graphics (CPG, aka CP), 1 per GPU
		kHwUnitCpc,		// command processor compute (CPC), 1 per GPU
		kHwUnitCpf,		// command processor fetch (CPF), 1 per GPU
		kHwUnitIa,		// input assembler (IA), getNumIaUnits(getGpuMode()) per GPU
		kHwUnitWd,		// work distributor (WD), 1 per GPU
		kHwUnitGds,		// global data store (GDS), 1 per GPU
		kHwUnitTca,		// texture cache arbiter (TCA), 2 per GPU; aka L2 cache controller
		kHwUnitTcs,		// texture cache ? (TCS), 1 per GPU; aka L2 cache bypass
		kHwUnitTcc,		// texture cache controller (TCC), getNumTccUnits(getGpuMode()) per GPU; aka L2 cache bank
		// per SE (shader engine) units:
		kHwUnitVgt,		// vertex geometry tessellator (VGT), 1 per SE
		kHwUnitPaSu,	// primitive assembler setup unit (PA-SU), 1 per SE
		kHwUnitPaSc,	// primitive assembler scan converter (PA-SC), 1 per SE
		kHwUnitPaScQuadPacker,	// primitive assembler scan converter (PA-SC) quad packer counter, 2 per SE
		kHwUnitSpi,		// shader processor input (SPI), 1 per SE
		kHwUnitSx,		// shader export (SX), 1 per SE
		kHwUnitCb,		// color buffer (CB), 4 per SE
		kHwUnitDb,		// depth buffer (DB), 4 per SE
		kHwUnitSq,		// sequencer (SQ), 1 set of perf counter values per SE; includes sequencer cache (SQC), aka L1 instruction and constant caches, I-cache and K-cache
		// per CU (compute unit) units: (CUs are also per SE)
		kHwUnitTa,		// texture address (TA), 1 per CU
		kHwUnitTd,		// texture data (TD), 1 per CU
		kHwUnitTcp,		// texture cache pipe (TCP), 1 per CU; aka L1 texture cache

		kNumHwUnits,
		kFirstHwUnitPerSe = kHwUnitVgt,
		kFirstHwUnitPerCu = kHwUnitTa,
	};

	extern char const* kaszHwUnitName[kNumHwUnits];				// short unit acronym string ("CP", "IA", etc.) by HardwareUnit
	extern const uint32_t kaNumPerfCounterSlots[kNumHwUnits];	// total number of performance counter slots available by HardwareUnit

	void setGpuMode();											// initialize system
	uint32_t getNumHwUnits(HardwareUnit unit);					// total number of logical hardware unit instances by HardwareUnit
	uint32_t getNumHwUnitsPerSe(HardwareUnit unit);				// total number of logical hardware unit instances per shader engine (SE) by HardwareUnit; 0 for units which do not reside inside an SE
	uint32_t getNumHwUnitsPerParent(HardwareUnit unit);			// total number of logical hardware unit instances per shader engine (SE) if in an SE, or in the GPU if not, by HardwareUnit
	bool isQuadPackerPerfCounter(sce::Gnm::PaScPerfCounter paScCounter);
	bool isQuadPackerPerfCounterSet(sce::Gnm::PaScPerfCounter const *aCounters, uint32_t numCounters);

	class HwUnitTarget
	{
		uint64_t m_target;

		static const uint64_t kTargetMaskSeIndex =			0x03;
		static const uint64_t kTargetSeBroadcast =			0x10;
		static const uint64_t kTargetInstanceBroadcast =	0x20;
		static const uint64_t kTargetSqInstanceMask =		0x40;
		static const uint32_t kTargetInstanceValueShift	=		 8;
		static const uint32_t kTargetSqInstanceMaskShiftPerSe =	12;

	public:
		// broadcast to all instances on all shader engines
		HwUnitTarget() : m_target(kTargetSeBroadcast | kTargetInstanceBroadcast) {}
		// broadcast to all instances on one or all shader engines
		explicit
		HwUnitTarget(sce::Gnm::ShaderEngineBroadcast broadcast) : m_target((broadcast == sce::Gnm::kShaderEngineBroadcastAll ? kTargetSeBroadcast : (broadcast & kTargetMaskSeIndex)) | kTargetInstanceBroadcast) {}
		// target to one (logically indexed) instance on one shader engine
		HwUnitTarget(sce::Gnm::ShaderEngine engine, uint8_t instanceIndex) : m_target((engine & kTargetMaskSeIndex) | (instanceIndex << kTargetInstanceValueShift)) {}

		// target to multiple (logically indexed) SQ instances across both shader engines
		explicit
		HwUnitTarget(uint32_t const* aSqInstanceMaskForSe/*[kNumSeUnits]*/)
		{
			setSqInstanceMask(aSqInstanceMaskForSe);
		}

		// Returns true if broadcasting to all shader engines 
		bool isSeBroadcast() const { return (m_target & kTargetSeBroadcast) != 0; }
		// Returns the target shader engine if !isSeBroadcast()
		sce::Gnm::ShaderEngine getSeIndex() const { return (sce::Gnm::ShaderEngine)(m_target & kTargetMaskSeIndex); }
		// Returns true if broadcasting to all instances on any target shader engines
		bool isInstanceBroadcast() const { return (m_target & kTargetInstanceBroadcast) != 0; }
		// Returns the target (logical) instance index on the targeted shader engine, if !isInstanceBroadcast()
		uint8_t getInstanceIndex() const { return (uint8_t)(m_target >> kTargetInstanceValueShift); }
		// Returns true if targeting a subset of instances on any target shader engines
		bool isSqInstanceMask() const { return (m_target & kTargetSqInstanceMask) != 0; }
		// Returns the target SQ instance mask if isInstanceMask() with the mask for SE 0 in bits [0:getNumHwUnitsPerSe(HwUnit)-1], the mask for SE 1 in bits [getNumHwUnitsPerSe(HwUnit):getNumHwUnitsPerSe(HwUnit)*2-1], etc.
		uint32_t getSqInstanceMask() const { return (uint32_t)(m_target >> kTargetInstanceValueShift); }
		// Returns the target SQ instance mask if isSqInstanceMask() for the specified shader engine
		uint32_t getSqInstanceMask(sce::Gnm::ShaderEngine engine) const
		{
			return (uint32_t)(m_target >> (kTargetInstanceValueShift + kTargetSqInstanceMaskShiftPerSe * engine)) & ((1 << kTargetSqInstanceMaskShiftPerSe) - 1);
		}
		// Returns the targeted shader engines for an instance broadcast, if isInstanceBroadcast()
		sce::Gnm::ShaderEngineBroadcast getSeBroadcast() const { return isSeBroadcast() ? sce::Gnm::kShaderEngineBroadcastAll : (sce::Gnm::ShaderEngineBroadcast)getSeIndex(); }

		// broadcast to all instances on one or all shader engines
		void setSeBroadcast(sce::Gnm::ShaderEngineBroadcast broadcast) { m_target &= ~(kTargetSeBroadcast | kTargetMaskSeIndex); if (broadcast == sce::Gnm::kShaderEngineBroadcastAll) m_target |= kTargetSeBroadcast; else m_target |= (broadcast & kTargetMaskSeIndex); }
		// target to one (logically indexed) instance on one shader engine
		void setInstance(sce::Gnm::ShaderEngine engine, uint8_t instanceIndex) { m_target = ((uint32_t)engine & kTargetMaskSeIndex) | ((uint32_t)instanceIndex << kTargetInstanceValueShift); }

		// target to multiple (logically indexed) SQ instances across both shader engines
		void setSqInstanceMask(uint32_t const *aSqInstanceMaskBySe/*[kNumSeUnits]*/);
		// target to multiple (logically indexed) SQ instances on one shader engine
		void setSqInstanceMask(sce::Gnm::ShaderEngine engine, uint32_t sqInstanceMaskForSe);
	};

	// converts a global logical hardware unit index (from 0 to kNum<unit>Units-1) into the equivalent HwUnitTarget
	HwUnitTarget getUnitInstance(HardwareUnit unit, uint32_t globalLogicalInstance);

	// holds a full set of parameters to a call to DrawCommandBuffer|DispatchCommandBuffer::select<unit>PerfCounters()
	class PerfCounterSelection
	{
		HardwareUnit m_unit;					// hardware unit type which should collect this counter
	protected:
		uint8_t m_reserved0[4];
	private:
		HwUnitTarget m_unitInstances;			// hardware unit instances which should collect this counter
		uint32_t m_counterSlotStart;			// index of the first performance counter slot to use
		uint32_t m_numCounterSlots;				// number of performance counter slots
		uint32_t const* m_aCounterSelect;		// pointer to an array of numCounterSlots counter select register values ([numCounterSlots][2] for kHwUnitCb, which has 2 select registers per slot);  NOTE: this memory is not managed by PerfCounterSelection!

		void init(HardwareUnit unit, HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, void const* aCounterSelect)
		{
			m_unit = unit;
			m_unitInstances = unitInstances;
			m_counterSlotStart = counterSlotStart;
			m_numCounterSlots = numCounterSlots;
			m_aCounterSelect = (uint32_t const*)aCounterSelect;
		}

	public:
		// General constructor
		PerfCounterSelection(HardwareUnit unit, HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, void const* aCounterSelect)		{ init(unit, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect); }

		// Constructors matching the various DrawCommandBuffer::select<unit>PerfCounters() calls:
		PerfCounterSelection(uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::CpPerfCounter const* aCounterSelect)		{ init(kHwUnitCp,	HwUnitTarget(), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::CpcPerfCounter const* aCounterSelect)		{ init(kHwUnitCpc,	HwUnitTarget(), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::CpfPerfCounter const* aCounterSelect)		{ init(kHwUnitCpf,	HwUnitTarget(), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::IaPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitIa,	HwUnitTarget(), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::WdPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitWd,	HwUnitTarget(), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::GdsPerfCounter const* aCounterSelect)		{ init(kHwUnitGds,	HwUnitTarget(), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::TcaPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitTca,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::TcsPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitTcs,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::TccPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitTcc,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::VgtPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitVgt,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::PaSuPerfCounter const* aCounterSelect)		{ init(kHwUnitPaSu, broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::PaScPerfCounter const* aCounterSelect)
		{
			HardwareUnit unit = isQuadPackerPerfCounterSet(aCounterSelect, numCounterSlots) ? kHwUnitPaScQuadPacker : kHwUnitPaSc;
			init(unit, broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect);
		}
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::SpiPerfCounter const* aCounterSelect)		{ init(kHwUnitSpi,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::SxPerfCounter const* aCounterSelect)		{ init(kHwUnitSx,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::CbPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitCb,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::DbPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitDb,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::SqPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitSq,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::TaPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitTa,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::TdPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitTd,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(HwUnitTarget broadcast, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::TcpPerfCounterSelect const* aCounterSelect)	{ init(kHwUnitTcp,	broadcast, counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::TccPerfCounterSelect const* aCounterSelect)			{ init(kHwUnitTcc,	getUnitInstance(kHwUnitTcc,		globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::VgtPerfCounterSelect const* aCounterSelect)			{ init(kHwUnitVgt,	getUnitInstance(kHwUnitVgt,		globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::PaSuPerfCounter const* aCounterSelect)					{ init(kHwUnitPaSu, getUnitInstance(kHwUnitPaSu,	globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::PaScPerfCounter const* aCounterSelect)
		{
			HardwareUnit unit = isQuadPackerPerfCounterSet(aCounterSelect, numCounterSlots) ? kHwUnitPaScQuadPacker : kHwUnitPaSc;
			init(unit, getUnitInstance(unit,	globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect);
		}
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::SpiPerfCounter const* aCounterSelect)					{ init(kHwUnitSpi,	getUnitInstance(kHwUnitSpi,		globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::SxPerfCounter const* aCounterSelect)					{ init(kHwUnitSx,	getUnitInstance(kHwUnitSx,		globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::CbPerfCounterSelect const* aCounterSelect)				{ init(kHwUnitCb,	getUnitInstance(kHwUnitCb,		globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::DbPerfCounterSelect const* aCounterSelect)				{ init(kHwUnitDb,	getUnitInstance(kHwUnitDb,		globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::SqPerfCounterSelect const* aCounterSelect)				{ init(kHwUnitSq,	getUnitInstance(kHwUnitSq,		globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::TaPerfCounterSelect const* aCounterSelect)				{ init(kHwUnitTa,	getUnitInstance(kHwUnitTa,		globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::TdPerfCounterSelect const* aCounterSelect)				{ init(kHwUnitTd,	getUnitInstance(kHwUnitTd,		globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t globalInstance, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::TcpPerfCounterSelect const* aCounterSelect)			{ init(kHwUnitTcp,	getUnitInstance(kHwUnitTcp,		globalInstance), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }
		PerfCounterSelection(uint32_t const* aSqMaskForSe, uint32_t counterSlotStart, uint32_t numCounterSlots, sce::Gnm::SqPerfCounterSelect const* aCounterSelect)		{ init(kHwUnitSq,	HwUnitTarget(aSqMaskForSe), counterSlotStart, numCounterSlots, (void const*)aCounterSelect); }

		// Accessors
		HardwareUnit getHardwareUnit() const { return m_unit; }
		HwUnitTarget getUnitInstances() const { return m_unitInstances; }
		uint32_t getCounterSlotStart() const { return m_counterSlotStart; }
		uint32_t getNumCounterSlots() const { return m_numCounterSlots; }
		void const* getCounterSelects() const { return m_aCounterSelect; }

		// Returns the number of sequential 8-byte counter values that will be required to collect a sample from all hardware unit instances for all performance counters selected by this PerfCounterSelection.
		// The space required by readPerfCounters() at gpuaddrCounterValueStart will be 8*getNumPerfCounterValues()
		uint32_t getNumPerfCounterValues() const;
		// Returns the number of sequential 8-byte counter values that will be required to collect a sample from all hardware unit instances for one performance counter slot selected by this PerfCounterSelection.
		uint32_t getNumPerfCounterValuesPerSlot() const;

		// Issues command buffer packets to select performance counters.
		void selectPerfCounters(sce::Gnm::DrawCommandBuffer* dcb) const;

		// Issues DMA packets to dcb to read the last sampled value from all hardware unit instances for all performance counters selected by this PerfCounterSelection.
		// Returns the size used for all counter values read to gpuaddrCounterValueStart, which is always equal to 8*getNumPerfCounterValues()
		size_t readPerfCounters(sce::Gnm::DrawCommandBuffer* dcb, uint64_t* pgpuCounterValueStart) const;

		// Fills out pUnitInstance with a HwUnitTarget containing the SE index (if the counter's unit is per SE) and logical instance index on that SE
		// and pCounterSlot with the counter slot index for the given counter value iCounterValue in the uint64_t[] array collected by readPerfCounters() for this PerfCounterSelection.
		// Returns true on success, or false if pSelection is invalid.
		bool getPerfCounterValueInfo(uint32_t iCounterValue, HwUnitTarget* pUnitInstance, uint32_t* pCounterSlot) const;

		// Returns the sum of the elements of pCounterValueStart which were read from counter slot iCounterSlot from all hardware unit instances selected by this PerfCounterSelection.
		// pCounterValueStart is the CPU address of the gpuaddrCounterValueStart passed to readCounterValues().
		uint64_t getPerfCounterTotalValue(uint32_t iCounterSlot, uint64_t const* pCounterValueStart) const;
	};

}	//namespace PerfCounters

#endif	//#ifndef PERF_COUNTER_SELECTION_H
