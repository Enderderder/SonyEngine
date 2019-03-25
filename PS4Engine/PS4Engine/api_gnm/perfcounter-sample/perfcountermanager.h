/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef PERF_COUNTER_MANAGER_H
#define PERF_COUNTER_MANAGER_H

#include <gnm/perfcounter_constants.h>
#include "perfcounterselection.h"

using namespace sce;

namespace PerfCounters {

	class PerfCounterManager
	{
		typedef void* (*PfnAlignedMalloc)(void* pAllocatorContext, size_t size, size_t alignment);
		typedef void (*PfnAlignedFree)(void* pAllocatorContext, void* pAllocation);

		enum {
			kPtrAlignment = sizeof(void*),
			kMaskPtrAlign = kPtrAlignment-1,
		};
		enum {
			kMeasurementAllocationStep = 16,	//must be a power of 2
			kMeasurementAllocationMask = (kMeasurementAllocationStep-1),
			kExtraDataAllocationStep = 64,		//must be a power of 2
			kExtraDataAllocationMask = (kExtraDataAllocationStep-1),
		};

		typedef struct Measurement
		{
			PerfCounterSelection* pSelection;		// perf counter selection
			char** aszName;							// array of pSelection->getNumCounterSlots() names of this measurement
			uint32_t sizeofData;					// total size of pSelection->m_aCounterValue[] and aszName[] data
			uint32_t startOffset;					// starting byte offset of output data in counter value buffer
		} Measurement;

		static PfnAlignedMalloc ms_aligned_malloc;
		static PfnAlignedFree ms_aligned_free;
		static void* ms_allocator_context;
		
		Measurement* m_aMeasurements;		// base pointer to a buffer beginning with the array of Measurement[m_numMeasurements], followed immediately by space allocated to associated Measurement::pSelection, Measurement::pSelection->m_aCounterSelect[], and Measurement::szName[] data
		uint32_t m_numMeasurementsAlloc;	// allocated size of m_aMeasurements 
		uint32_t m_sizeofExtraDataAlloc;	// size of additional allocated space available after m_aMeasurements[m_numMeasurementsAlloc]
		uint32_t m_numMeasurements;			// used size of m_aMeasurements
		uint32_t m_sizeofExtraData;			// used portion of m_sizeofExtraData
		uint32_t m_maskHwUnits;				// bit mask of (1<<HardwareUnit) used by all m_aMeasurements[m_numMeasurements]
		uint32_t m_sizeofCounterValues;		// required space for output counter values in bytes
		Gnm::PerfCounterControl m_state;	// last start or stop state

		// Do not allow copy construction:
		PerfCounterManager(PerfCounterManager const&unused) { SCE_GNM_ERROR_INLINE("Copying forbidden"); SCE_GNM_UNUSED(unused);}
		PerfCounterManager const& operator=(PerfCounterManager const&unused) { SCE_GNM_ERROR_INLINE("Assignment forbidden"); return *this; SCE_GNM_UNUSED(unused);}

		bool realloc(uint32_t numPerfCounterMeasurementsAlloc, uint32_t sizeofExtraDataAlloc);

		int32_t addMeasurement(HardwareUnit unit, HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, void const* aCounterSelect, char const*const* aszName);

		int32_t addMeasurement(HardwareUnit unit, uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, void const* aCounterSelect, char const*const* aszName)
		{
			HwUnitTarget unitInstance = getUnitInstance(unit, instanceIndex);
			if (unitInstance.isInstanceBroadcast() || !(unitInstance.getInstanceIndex() < getNumHwUnitsPerParent(unit)))
				return -1;
			return addMeasurement(unit, unitInstance, counterSlotStart, numCounterSlots, aCounterSelect, aszName);
		}

		int32_t addMeasurement(HardwareUnit unit, uint32_t const* aSqMaskForSe, uint32_t counterSlotStart, uint32_t numCounterSlots, void const* aCounterSelect, char const*const* aszName)
		{
			if (unit != kHwUnitSq)
				return -1;
			HwUnitTarget target(aSqMaskForSe);
			return addMeasurement(unit, target, counterSlotStart, numCounterSlots, aCounterSelect, aszName);
		}
		int32_t addMeasurement(HardwareUnit unit, sce::Gnm::ShaderEngine engine, uint32_t sqMaskForSe, uint32_t counterSlotStart, uint32_t numCounterSlots, void const* aCounterSelect, char const*const* aszName)
		{
			if (unit != kHwUnitSq)
				return -1;
			HwUnitTarget target;
			target.setSqInstanceMask(engine, sqMaskForSe);
			return addMeasurement(unit, target, counterSlotStart, numCounterSlots, aCounterSelect, aszName);
		}

		int32_t addMeasurement(HardwareUnit unit, HwUnitTarget unitInstances, uint32_t counterSlot, void const* pCounterSelect, char const* szName)
		{
			return addMeasurement(unit, unitInstances, counterSlot, 1, pCounterSelect, szName ? &szName : NULL);
		}

		int32_t addMeasurement(HardwareUnit unit, uint32_t instanceIndex, uint32_t counterSlot, void const* pCounterSelect, char const* szName)
		{
			HwUnitTarget unitInstance = getUnitInstance(unit, instanceIndex);
			if (unitInstance.isInstanceBroadcast() || !(unitInstance.getInstanceIndex() < getNumHwUnitsPerParent(unit)))
				return -1;
			return addMeasurement(unit, unitInstance, counterSlot, 1, pCounterSelect, szName ? &szName : NULL);
		}

		int32_t addMeasurement(HardwareUnit unit, uint32_t const* aSqMaskForSe, uint32_t counterSlot, void const* pCounterSelect, char const* szName)
		{
			if (unit != kHwUnitSq)
				return -1;
			HwUnitTarget target(aSqMaskForSe);
			return addMeasurement(unit, target, counterSlot, 1, pCounterSelect, szName ? &szName : NULL);
		}
		int32_t addMeasurement(HardwareUnit unit, sce::Gnm::ShaderEngine engine, uint16_t sqMaskForSe, uint32_t counterSlot, void const* pCounterSelect, char const* szName)
		{
			if (unit != kHwUnitSq)
				return -1;
			HwUnitTarget target;
			target.setSqInstanceMask(engine, sqMaskForSe);
			return addMeasurement(unit, target, counterSlot, 1, pCounterSelect, szName ? &szName : NULL);
		}

		void syncTheGpuIfNeeded(Gnm::ConstantCommandBuffer* ccb, Gnm::DrawCommandBuffer* dcb) const;

	protected:
		uint8_t m_reserved0[4];
	public:
		// Sets the allocation and free functions which will be used by PerfCounterManager.
		// These default to context free _aligned_malloc/memalign and _aligned_free/free.
		static void setAllocator(PfnAlignedMalloc pfnAlignedMalloc, PfnAlignedFree pfnAlignedFree, void* pAllocatorContext)
		{
			ms_aligned_malloc = pfnAlignedMalloc;
			ms_aligned_free = pfnAlignedFree;
			ms_allocator_context = pAllocatorContext;
		}

		// Construct with no memory allocated.
		PerfCounterManager()
			: m_aMeasurements(NULL)
			, m_numMeasurementsAlloc(0)
			, m_sizeofExtraDataAlloc(0)
			, m_numMeasurements(0)
			, m_sizeofExtraData(0)
			, m_maskHwUnits(0)
			, m_sizeofCounterValues(0)
			, m_state(Gnm::kPerfmonStateStopCounting, Gnm::kPerfmonEnableModeAlwaysCount, Gnm::kPerfmonNoSample)
		{
		}

		// Construct with memory allocated to allow for numMeasurementsAllocInitial measurements to be added, with a guess for how much space will be required for PerfCounterSelections, counter selects, and names.
		explicit PerfCounterManager(uint32_t numMeasurementsAllocInitial);

		// Construct with memory allocated to allow for numMeasurementsAllocInitial measurements to be added, with sizeofExtraDataAllocInitial bytes of additional space reserved for PerfCounterSelections, counter selects, and names.
		PerfCounterManager(uint32_t numMeasurementsAllocInitial, uint32_t sizeofExtraDataAllocInitial);

		// Destruct
		~PerfCounterManager();

		// Reset to a state with no measurements, but maintain existing allocated memory.
		void reset()
		{
			m_numMeasurements = 0;
			m_sizeofExtraData = 0;
			m_maskHwUnits = 0;
			m_sizeofCounterValues = 0;
		}

		// Reset to a pristine state with no measurements, as constructed by PerfCounterManager(), with no memory allocated.
		void resetAndFree();

		// Add a measurement of a single performance counter counterSelect in counter slot counterSlot of one instance of the hardware unit associated to that counter, by global instance index.
		// If specified, also stores a copy of szName associated with the counter select data.
		// The global instance index is a logical index for counters on CUs, in the range [0:23], rather than physical in the range [0:kNumCuUnits-1].
		// Returns the byte offset reserved for data output by readPerfCounters() for the counter values, or -1 on error.
		int32_t addMeasurement(uint32_t counterSlot, Gnm::CpPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitCp, HwUnitTarget(), counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t counterSlot, Gnm::CpcPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitCpc, HwUnitTarget(), counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t counterSlot, Gnm::CpfPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitCpf, HwUnitTarget(), counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t counterSlot, Gnm::WdPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitWd, HwUnitTarget(), counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t counterSlot, Gnm::GdsPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitGds, HwUnitTarget(), counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t counterSlot, Gnm::TcsPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTcs, HwUnitTarget(), counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::IaPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitIa, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::TcaPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTca, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::TccPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTcc, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::VgtPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitVgt, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::PaSuPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitPaSu, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::PaScPerfCounter counterSelect, char const* szName = NULL)	{ return addMeasurement(isQuadPackerPerfCounter(counterSelect) ? kHwUnitPaScQuadPacker : kHwUnitPaSc, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::SpiPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitSpi, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::SxPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitSx, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::CbPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitCb, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::DbPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitDb, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::SqPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitSq, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::TaPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTa, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::TdPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTd, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t instanceIndex, uint32_t counterSlot, Gnm::TcpPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTcp, instanceIndex, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(uint32_t const* aSqMaskForSe/*[getNumShaderEngines(GpuMode)]*/, uint32_t counterSlot, Gnm::SqPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitSq, aSqMaskForSe, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(sce::Gnm::ShaderEngine engine, uint32_t sqMaskForSe, uint32_t counterSlot, Gnm::SqPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitSq, engine, sqMaskForSe, counterSlot, &counterSelect, szName); }

		// Add a measurement of a set of performance counters aCounterSelect[numCounterSlots] in counter slots counterSlotStart..counterSlotStart+numCounterSlots-1 of one instance of the hardware unit associated to that counter, by global instance index.  
		// If specified, also stores copies of all aszName[numCounterSlots] with the counter select data.
		// The global instance index is a logical index for counters on CUs, in the range [0:23], rather than physical in the range [0:kNumCuUnits-1].
		// Returns the byte offset reserved for data output by readPerfCounters() for the counter values, or -1 on error.
		int32_t addMeasurementSet(uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::CpPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitCp, HwUnitTarget(), counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::CpcPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitCpc, HwUnitTarget(), counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::CpfPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitCpf, HwUnitTarget(), counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::WdPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitWd, HwUnitTarget(), counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::GdsPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitGds, HwUnitTarget(), counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TcsPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTcs, HwUnitTarget(), counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::IaPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitIa, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TcaPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTca, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TccPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTcc, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::VgtPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitVgt, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::PaSuPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitPaSu, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::PaScPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(isQuadPackerPerfCounter(aCounterSelect[0]) ? kHwUnitPaScQuadPacker : kHwUnitPaSc, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::SpiPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitSpi, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::SxPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitSx, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::CbPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitCb, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::DbPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitDb, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::SqPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitSq, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TaPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTa, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TdPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTd, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t instanceIndex, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TcpPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTcp, instanceIndex, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(uint32_t const* aSqMaskForSe/*[getNumShaderEngines(GpuMode)]*/, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::SqPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitSq, aSqMaskForSe, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(sce::Gnm::ShaderEngine engine, uint32_t sqMaskForSe, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::SqPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitSq, engine, sqMaskForSe, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }

		// Add a measurement of a single performance counter counterSelect in counter slot counterSlot of multiple instances of the hardware unit associated to that counter, based on the broadcast register setting unitInstances.
		// If specified, also stores a copy of szName associated with the counter select data.
		// For counters on CUs, unitInstances should have setInstanceBroadcast(), as broadcasts to select CUs by physical instance index are not generally portable.
		// Returns the byte offset reserved for data output by readPerfCounters() for the counter values, or -1 on error.
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::IaPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitIa, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::TcaPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTca, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::TccPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTcc, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::VgtPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitVgt, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::PaSuPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitPaSu, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::PaScPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(isQuadPackerPerfCounter(counterSelect) ? kHwUnitPaScQuadPacker : kHwUnitPaSc, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::SpiPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitSpi, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::SxPerfCounter counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitSx, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::CbPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitCb, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::DbPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitDb, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::SqPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitSq, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::TaPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTa, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::TdPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTd, unitInstances, counterSlot, &counterSelect, szName); }
		int32_t addMeasurement(HwUnitTarget unitInstances, uint32_t counterSlot, Gnm::TcpPerfCounterSelect counterSelect, char const* szName = NULL) { return addMeasurement(kHwUnitTcp, unitInstances, counterSlot, &counterSelect, szName); }

		// Add a measurement of a set of performance counters aCounterSelect[numCounterSlots] in counter slots counterSlotStart..counterSlotStart+numCounterSlots-1 of multiple instances of the hardware unit associated to that counter, based on the broadcast register setting unitInstances. 
		// If specified, also stores copies of all aszName[numCounterSlots] with the counter select data.
		// For counters on CUs, unitInstances should have setInstanceBroadcast(), as broadcasts to select CUs by physical instance index are not generally portable.
		// Returns the byte offset reserved for data output by readPerfCounters() for the counter values, or -1 on error.
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TcaPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTca, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TccPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTcc, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::VgtPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitVgt, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::PaSuPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitPaSu, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::PaScPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(isQuadPackerPerfCounter(aCounterSelect[0]) ? kHwUnitPaScQuadPacker : kHwUnitPaSc, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::SpiPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitSpi, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::SxPerfCounter const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitSx, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::CbPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitCb, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::DbPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitDb, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::SqPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitSq, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TaPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTa, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TdPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTd, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }
		int32_t addMeasurementSet(HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, Gnm::TcpPerfCounterSelect const* aCounterSelect, char const*const* aszName = NULL) { return addMeasurement(kHwUnitTcp, unitInstances, counterSlotStart, numCounterSlots, aCounterSelect, aszName); }

		// Returns the number of measurements added so far to this manager.
		uint32_t getNumMeasurements() const { return m_numMeasurements; }
		// Returns the total size of the counter value buffer required to store numSamples samples of all measurements currently added
		uint32_t getRequiredSizeofCounterValueBuffer(uint32_t numSamples = 1) const { return m_sizeofCounterValues*numSamples; }

		// Returns the PerfCounterSelection for the iMeasurement'th added measurement or set
		PerfCounterSelection const* getMeasurementSelection(uint32_t iMeasurement) const { return (iMeasurement < m_numMeasurements) ? m_aMeasurements[iMeasurement].pSelection : NULL; }
		// Returns true if slot iCounterSlot of the iMeasurement'th added measurement or set has a name
		bool isMeasurementNamed(uint32_t iMeasurement, uint32_t iCounterSlot = 0) const { return (iMeasurement < m_numMeasurements && m_aMeasurements[iMeasurement].aszName != NULL && iCounterSlot < m_aMeasurements[iMeasurement].pSelection->getNumCounterSlots()); }
		// Returns the name of slot iCounterSlot of the iMeasurement'th added measurement or set
		char const* getMeasurementName(uint32_t iMeasurement, uint32_t iCounterSlot = 0) const { return (iMeasurement < m_numMeasurements && m_aMeasurements[iMeasurement].aszName != NULL && iCounterSlot < m_aMeasurements[iMeasurement].pSelection->getNumCounterSlots()) ? m_aMeasurements[iMeasurement].aszName[iCounterSlot] : "<UNNAMED>"; }
		// Returns true if slot iCounterSlot of the iMeasurement'th added measurement or set has a name
		uint32_t getMeasurementSelect(uint32_t iMeasurement, uint32_t iCounterSlot = 0) const { return (iMeasurement < m_numMeasurements && iCounterSlot < m_aMeasurements[iMeasurement].pSelection->getNumCounterSlots()) ? ((uint32_t*)m_aMeasurements[iMeasurement].pSelection->getCounterSelects())[iCounterSlot] : (uint32_t)-1; }
		// Returns the name array for the iMeasurement'th added measurement or set
		char const*const* getMeasurementNames(uint32_t iMeasurement) const { return (iMeasurement < m_numMeasurements) ? m_aMeasurements[iMeasurement].aszName : NULL; }
		// Returns the byte offset of data output by readPerfCounters() for the iMeasurement'th added measurement or set
		int32_t getMeasurementCounterValueOffset(uint32_t iMeasurement) const { return (iMeasurement < m_numMeasurements) ? (int32_t)m_aMeasurements[iMeasurement].startOffset : -1; }
		// Returns the number of uint64_t counter values which will be output by readPerfCounters() for the iMeasurement'th added measurement or set
		uint32_t getMeasurementNumCounterValues(uint32_t iMeasurement) const { return (iMeasurement < m_numMeasurements) ? m_aMeasurements[iMeasurement].pSelection->getNumPerfCounterValues() : 0; }
		// Returns the number of uint64_t counter values which will be output by readPerfCounters() for the iMeasurement'th added measurement or set
		uint32_t getMeasurementNumCounterValuesPerSlot(uint32_t iMeasurement) const { return (iMeasurement < m_numMeasurements) ? m_aMeasurements[iMeasurement].pSelection->getNumPerfCounterValuesPerSlot() : 0; }

		// Appends packets to dcb to start accumulation of all performance counters, and optionally sample them.
		// Note that this requires a label to make the GPU idle itself between draw calls.  
		// Some subset of PA-SC, CB, IA, and TCS counters may be sampled accurately without a label by simply calling 
		// dcb->triggerEvent(Gnm::kEventTypePerfcounterStart), dcb->triggerEvent(Gnm::kEventTypePerfcounterSample).
		// Note that some CU perfcounters also require a call to Gnm::setSpiEnableCuCounterWindowingEvents(Gnm::kBroadcastAll, true)
		// to enable the collection of statistics which depend on SPI events, such as those windowed by draw call.
		void startPerfCounters(Gnm::ConstantCommandBuffer* ccb, Gnm::DrawCommandBuffer* dcb, Gnm::PerfmonSample doSample = Gnm::kPerfmonNoSample, Gnm::PerfmonEnableMode windowing = Gnm::kPerfmonEnableModeAlwaysCount);
		inline void startPerfCounters(Gnmx::GnmxGfxContext* gfxc, Gnm::PerfmonSample doSample = Gnm::kPerfmonNoSample, Gnm::PerfmonEnableMode windowing = Gnm::kPerfmonEnableModeAlwaysCount) { startPerfCounters(&gfxc->m_ccb, &gfxc->m_dcb, doSample, windowing); }

		// Appends packets to dcb to stop accumulation of all performance counters, and optionally sample them.
		// Note that this requires a label to make the GPU idle itself between draw calls.  
		// Some subset of PA-SC, CB, IA, and TCS counters may be sampled accurately without a label by simply calling 
		// dcb->triggerEvent(Gnm::kEventTypePerfcounterStop), dcb->triggerEvent(Gnm::kEventTypePerfcounterSample).
		void stopPerfCounters(Gnm::ConstantCommandBuffer* ccb, Gnm::DrawCommandBuffer* dcb, Gnm::PerfmonSample doSample = Gnm::kPerfmonNoSample);
		inline void stopPerfCounters(Gnmx::GnmxGfxContext* gfxc, Gnm::PerfmonSample doSample = Gnm::kPerfmonNoSample) { stopPerfCounters(&gfxc->m_ccb, &gfxc->m_dcb, doSample); }

		// Appends packets to dcb to stop and reset all performance counters to zero.
		// Note that this requires a label to make the GPU idle itself between draw calls.  
		void resetPerfCounters(Gnm::ConstantCommandBuffer* ccb, Gnm::DrawCommandBuffer* dcb);
		inline void resetPerfCounters(Gnmx::GnmxGfxContext* gfxc) { resetPerfCounters(&gfxc->m_ccb, &gfxc->m_dcb); }

		// Appends packets to dcb to sample the performance counters without changing whether they are currently started.
		// Note that this requires a label to make the GPU idle itself between draw calls.  
		// Some subset of PA-SC, CB, IA, and TCS counters may be sampled accurately without a label by simply calling 
		// dcb->triggerEvent(Gnm::kEventTypePerfcounterSample).
		void samplePerfCounters(Gnm::ConstantCommandBuffer* ccb, Gnm::DrawCommandBuffer* dcb) const;
		inline void samplePerfCounters(Gnmx::GnmxGfxContext* gfxc) { samplePerfCounters(&gfxc->m_ccb, &gfxc->m_dcb); }

		// Appends packets to dcb to select the performance counters for all measurements which have been added.
		void selectPerfCounters(Gnm::DrawCommandBuffer* dcb) const;
		inline void selectPerfCounters(Gnmx::GnmxGfxContext* gfxc) { selectPerfCounters(&gfxc->m_dcb); }

		// Appends packets to dcb to DMA the last sampled values for all added measurements to the space reserved in pPerfCounterValueBuffer for sample iSample.
		// Returns false if pPerfCounterValueBuffer is not a Gnm::kAlignmentBuffer aligned GPU visible buffer.
		bool readPerfCounterValues(Gnm::DrawCommandBuffer* dcb, uint64_t const* pPerfCounterValueBuffer, uint32_t iSample = 0) const;
		inline bool readPerfCounterValues(Gnmx::GnmxGfxContext* gfxc, uint64_t const* pPerfCounterValueBuffer, uint32_t iSample = 0) { return readPerfCounterValues(&gfxc->m_dcb, pPerfCounterValueBuffer, iSample); }

		// Returns the value of sample iSample of a single counter value iCounterValue of the iMeasurement'th added measurement.
		// getMeasurementSelection(iMeasurement)->getPerfCounterValueInfo(iCounterValue, &unitInstance, &iCounterSlot) can be used to determine the assignment of the specific counter value.
		uint64_t getPerfCounterValue(uint64_t const* pPerfCounterValueBuffer, uint32_t iMeasurement, uint32_t iCounterValue, uint32_t iSample = 0) const;

		// Returns the delta between sample (iSample-1) and sample iSample of a single counter value iCounterValue of the iMeasurement'th added measurement.
		// getMeasurementSelection(iMeasurement)->getPerfCounterValueInfo(iCounterValue, &unitInstance, &iCounterSlot) can be used to determine the assignment of the specific counter value.
		int64_t getPerfCounterValueDelta(uint64_t const* pPerfCounterValueBuffer, uint32_t iMeasurement, uint32_t iCounterValue, uint32_t iSample = 1) const;

		// Returns the summed values for sample iSample of the counter values from all hardware unit instances associated with slot iCounterSlot of the iMeasurement'th added measurement.
		uint64_t getPerfCounterTotalValue(uint64_t const* pPerfCounterValueBuffer, uint32_t iMeasurement, uint32_t iCounterSlot = 0, uint32_t iSample = 0) const;

		// Returns the delta from sample iSample-1 to iSample of the summed values of the counter values from all hardware unit instances associated with slot iCounterSlot of the iMeasurement'th added measurement.
		int64_t getPerfCounterTotalValueDelta(uint64_t const* pPerfCounterValueBuffer, uint32_t iMeasurement, uint32_t iCounterSlot = 0, uint32_t iSample = 1) const;
	};

}	//namespace PerfCounters

#endif	//#ifndef PERF_COUNTER_MANAGER_H
