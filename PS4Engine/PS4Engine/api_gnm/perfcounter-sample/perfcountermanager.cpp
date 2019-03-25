/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "perfcountermanager.h"
#include "gnm/perfcounter_regs.h"
#include "gnm/platform.h"
#include <stdlib.h>

using namespace sce;

namespace PerfCounters {

#ifdef _MSC_VER
static void* aligned_malloc(void*, size_t size, size_t alignment)
{
	return _aligned_malloc(size, alignment);
}
static void aligned_free(void*, void* pAllocation)
{
	return _aligned_free(pAllocation);
}
#else
static void* aligned_malloc(void*, size_t size, size_t alignment)
{
	return memalign(alignment, size);
}
static void aligned_free(void*, void* pAllocation)
{
	return free(pAllocation);
}
#endif

PerfCounterManager::PfnAlignedMalloc PerfCounterManager::ms_aligned_malloc = aligned_malloc;
PerfCounterManager::PfnAlignedFree PerfCounterManager::ms_aligned_free = aligned_free;
void* PerfCounterManager::ms_allocator_context = NULL;


bool PerfCounterManager::realloc(uint32_t numPerfCounterMeasurementsAlloc, uint32_t sizeofExtraDataAlloc)
{
	void* pAllocNew = (*ms_aligned_malloc)(ms_allocator_context, ((numPerfCounterMeasurementsAlloc * sizeof(Measurement) + kMaskPtrAlign)&~kMaskPtrAlign) + sizeofExtraDataAlloc, kPtrAlignment);
	if (pAllocNew == NULL)
		return false;
	Measurement* aPerfCounterMeasurementsNew = (Measurement*)pAllocNew;
	if (m_aMeasurements != NULL) {
		uint8_t* pExtraDataNew = (uint8_t*)pAllocNew + ((numPerfCounterMeasurementsAlloc * sizeof(Measurement) + kMaskPtrAlign)&~kMaskPtrAlign);
		uint32_t sizeofExtraData = 0;
		for (uint32_t i = 0; i < m_numMeasurements; ++i) {
			sizeofExtraData = ((sizeofExtraData + kMaskPtrAlign)&~kMaskPtrAlign);
			void* pAllocSelection = pExtraDataNew + sizeofExtraData;
			void* pAllocExtraData = pExtraDataNew + sizeofExtraData + sizeof(PerfCounterSelection);
			uint32_t sizeofExtraDataElem = m_aMeasurements[i].sizeofData;
			memcpy(pAllocExtraData, m_aMeasurements[i].pSelection->getCounterSelects(), sizeofExtraDataElem);
			aPerfCounterMeasurementsNew[i].pSelection = new(pAllocSelection) PerfCounterSelection(m_aMeasurements[i].pSelection->getHardwareUnit(), m_aMeasurements[i].pSelection->getUnitInstances(), m_aMeasurements[i].pSelection->getCounterSlotStart(), m_aMeasurements[i].pSelection->getNumCounterSlots(), pAllocExtraData);
			if (m_aMeasurements[i].aszName != NULL) {
				aPerfCounterMeasurementsNew[i].aszName = (char**)((uintptr_t)pAllocExtraData + ((uintptr_t)m_aMeasurements[i].aszName - (uintptr_t)m_aMeasurements[i].pSelection->getCounterSelects()));
				for (uint32_t iSlot = 0, numSlots = m_aMeasurements[i].pSelection->getNumCounterSlots(); iSlot < numSlots; ++iSlot)
					aPerfCounterMeasurementsNew[i].aszName[iSlot] = (char*)((uintptr_t)pAllocExtraData + ((uintptr_t)m_aMeasurements[i].aszName[iSlot] - (uintptr_t)m_aMeasurements[i].pSelection->getCounterSelects()));
			} else
				aPerfCounterMeasurementsNew[i].aszName = NULL;
			aPerfCounterMeasurementsNew[i].sizeofData = sizeofExtraDataElem;
			aPerfCounterMeasurementsNew[i].startOffset = m_aMeasurements[i].startOffset;
			sizeofExtraData += sizeof(PerfCounterSelection) + sizeofExtraDataElem;
		}
		SCE_GNM_ASSERT(sizeofExtraData == m_sizeofExtraData);
		(*ms_aligned_free)(ms_allocator_context, m_aMeasurements);
	}
	m_aMeasurements = aPerfCounterMeasurementsNew;
	m_numMeasurementsAlloc = numPerfCounterMeasurementsAlloc;
	m_sizeofExtraDataAlloc = sizeofExtraDataAlloc;
	return true;
}

int32_t PerfCounterManager::addMeasurement(HardwareUnit unit, HwUnitTarget unitInstances, uint32_t counterSlotStart, uint32_t numCounterSlots, void const* aCounterSelect, char const*const* aszName)
{
	if (unit >= kNumHwUnits)
		return -1;
	if (!(numCounterSlots > 0 && counterSlotStart + numCounterSlots <= kaNumPerfCounterSlots[unit]))
		return -1;
	SCE_GNM_ASSERT(numCounterSlots <= 16);
	uint32_t sizeofNames = 0;
	uint32_t aSizeofName[16];
	uint32_t sizeofCounterSelects = numCounterSlots * (unit == kHwUnitCb ? 4*2 : 4);
	uint32_t sizeofNamesAlign;
	if (aszName != NULL) {
		sizeofNamesAlign = ((0 - sizeof(PerfCounterSelection) - sizeofCounterSelects)&kMaskPtrAlign);
		sizeofNames += sizeof(char*)*numCounterSlots;
		for (uint32_t iSlot = 0; iSlot < numCounterSlots; ++iSlot) {
			aSizeofName[iSlot] = (uint32_t)(strlen(aszName[iSlot]) + 1);
			sizeofNames += aSizeofName[iSlot];
		}
	} else
		sizeofNamesAlign = 0;
	uint32_t sizeofExtraDataNew = ((m_sizeofExtraData + kMaskPtrAlign)&~kMaskPtrAlign) + sizeof(PerfCounterSelection) + sizeofCounterSelects + sizeofNamesAlign + sizeofNames;
	if (m_numMeasurements + 1 > m_numMeasurementsAlloc || sizeofExtraDataNew > m_sizeofExtraDataAlloc) {
		uint32_t numMeasurementsAllocNew = (m_numMeasurements + 1 + kMeasurementAllocationMask)&~kMeasurementAllocationMask;
		uint32_t sizeofExtraDataAllocNew = numMeasurementsAllocNew*(sizeof(PerfCounterSelection) + 2*sizeof(uint32_t)  );// + (aszName ? 64 : 0));
		if (sizeofExtraDataAllocNew < sizeofExtraDataNew)
			sizeofExtraDataAllocNew = sizeofExtraDataNew;
		sizeofExtraDataAllocNew = (sizeofExtraDataAllocNew + kExtraDataAllocationMask) &~kExtraDataAllocationMask;
		if (!realloc(numMeasurementsAllocNew, sizeofExtraDataAllocNew))
			return -1;
	}
	void* pAllocSelection = (uint8_t*)((uintptr_t)m_aMeasurements + ((m_numMeasurementsAlloc * sizeof(Measurement) + kMaskPtrAlign)&~kMaskPtrAlign) + ((m_sizeofExtraData + kMaskPtrAlign)&~kMaskPtrAlign));
	void* pAllocExtraData = (uint8_t*)((uintptr_t)pAllocSelection + sizeof(PerfCounterSelection));
	m_sizeofExtraData = sizeofExtraDataNew;
	memcpy(pAllocExtraData, aCounterSelect, sizeofCounterSelects);
	char** aszNameCopy;
	if (aszName != NULL) {
		aszNameCopy = (char**)((uintptr_t)pAllocExtraData + sizeofCounterSelects + sizeofNamesAlign);
		char* szNameCopy = (char*)(aszNameCopy + numCounterSlots);
		for (uint32_t iSlot = 0; iSlot < numCounterSlots; ++iSlot) {
			memcpy(szNameCopy, aszName[iSlot], aSizeofName[iSlot]);
			aszNameCopy[iSlot] = szNameCopy;
			szNameCopy += aSizeofName[iSlot];
		}
	} else
		aszNameCopy = NULL;
	Measurement* pMeasurement = m_aMeasurements + m_numMeasurements++;
	pMeasurement->pSelection = new(pAllocSelection) PerfCounterSelection(unit, unitInstances, counterSlotStart, numCounterSlots, pAllocExtraData);
	pMeasurement->aszName = aszNameCopy;
	pMeasurement->sizeofData = sizeofCounterSelects + sizeofNamesAlign + sizeofNames;
	pMeasurement->startOffset = m_sizeofCounterValues;
	m_sizeofCounterValues += 8*pMeasurement->pSelection->getNumPerfCounterValues();
	m_maskHwUnits |= (1<<unit);
	return (int32_t)pMeasurement->startOffset;
}

PerfCounterManager::PerfCounterManager(uint32_t numPerfCounterMeasurementsAllocInitial, uint32_t sizeofExtraDataAllocInitial)
	: m_aMeasurements(NULL)
	, m_numMeasurementsAlloc(0)
	, m_sizeofExtraDataAlloc(0)
	, m_numMeasurements(0)
	, m_sizeofExtraData(0)
	, m_maskHwUnits(0)
	, m_sizeofCounterValues(0)
{
	void* pAllocNew = (*ms_aligned_malloc)(ms_allocator_context, ((numPerfCounterMeasurementsAllocInitial * sizeof(Measurement) + kMaskPtrAlign)&~kMaskPtrAlign) + sizeofExtraDataAllocInitial, kPtrAlignment);
	if (pAllocNew != NULL) {
		m_aMeasurements = (Measurement*)pAllocNew;
		m_numMeasurementsAlloc = numPerfCounterMeasurementsAllocInitial;
		m_sizeofExtraDataAlloc = sizeofExtraDataAllocInitial;
	}
}

PerfCounterManager::PerfCounterManager(uint32_t numPerfCounterMeasurementsAllocInitial)
	: m_aMeasurements(NULL)
	, m_numMeasurementsAlloc(0)
	, m_sizeofExtraDataAlloc(0)
	, m_numMeasurements(0)
	, m_sizeofExtraData(0)
	, m_maskHwUnits(0)
	, m_sizeofCounterValues(0)
{
	uint32_t sizeofExtraDataAllocInitial = numPerfCounterMeasurementsAllocInitial * (sizeof(PerfCounterSelection) + 2*sizeof(uint32_t) + 64);
	void* pAllocNew = (*ms_aligned_malloc)(ms_allocator_context, ((numPerfCounterMeasurementsAllocInitial * sizeof(Measurement) + kMaskPtrAlign)&~kMaskPtrAlign) + sizeofExtraDataAllocInitial, kPtrAlignment);
	if (pAllocNew != NULL) {
		m_aMeasurements = (Measurement*)pAllocNew;
		m_numMeasurementsAlloc = numPerfCounterMeasurementsAllocInitial;
		m_sizeofExtraDataAlloc = sizeofExtraDataAllocInitial;
	}
}

PerfCounterManager::~PerfCounterManager()
{
	if (m_aMeasurements != NULL)
		(*ms_aligned_free)(ms_allocator_context, m_aMeasurements);
}

void PerfCounterManager::resetAndFree()
{
	if (m_aMeasurements != NULL) {
		(*ms_aligned_free)(ms_allocator_context, m_aMeasurements);
		m_aMeasurements = NULL;
		m_numMeasurementsAlloc = 0;
		m_sizeofExtraDataAlloc = 0;
	}
	reset();
}

void PerfCounterManager::syncTheGpuIfNeeded(Gnm::ConstantCommandBuffer* ccb, Gnm::DrawCommandBuffer* dcb) const
{
	if (m_maskHwUnits & ((1<<kNumHwUnits) - (1<<kHwUnitCpc))) {
		//NOTE: setPerfCounterControl() immediately changes the perfcounter state accross the entire GPU
		// as soon as the CP processes it.  To prevent the changes from occurring in the middle of any
		// draw calls which haven't completed, we need to issue a label to flush the GPU pipeline.
		//CP perfcounters don't need a pipeline flush because the CP is controlling the counters.
		uint64_t *pLabel = (uint64_t*)dcb->allocateFromCommandBuffer(8, Gnm::kEmbeddedDataAlignment8);
		*pLabel = 0;
		dcb->writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, pLabel, Gnm::kEventWriteSource32BitsImmediate, 1, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
		dcb->waitOnAddress(pLabel, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);
		if ((m_maskHwUnits & (1<<kHwUnitCpf)) && ccb != NULL) {
			//FIXME: CPF perfcounters should also require the ccb to wait for the previous draw call to finish.  The following hangs the system, though:
			//ccb->waitOnDeCounterDiff(0);
		}
	} else if (m_maskHwUnits & (1<<kHwUnitCpc)) {
		//FIXME: CPC perfcounters are related to asynchronous compute; Synchronizing compute pipes to the graphics pipe would require higher level knowledge
	}
}

void PerfCounterManager::startPerfCounters(Gnm::ConstantCommandBuffer* ccb, Gnm::DrawCommandBuffer* dcb, Gnm::PerfmonSample doSample, Gnm::PerfmonEnableMode windowing)
{
	//NOTE: while triggerEvent packets flow down the pipeline as context state and automatically
	// start counters between draw calls, but only affect some subset of IA, PA-SC, CB, and TCS counters.
	dcb->triggerEvent(Gnm::kEventTypePerfCounterStart);

	// Even with windowing enabled, we can't be sure that previous windowed draw calls have completed unless we sync here:
	syncTheGpuIfNeeded(ccb, dcb);
	m_state.setPerfmonState(Gnm::kPerfmonStateStartCounting);
	m_state.setPerfmonEnableMode(windowing);
	Gnm::PerfCounterControl stateSample = m_state;
	stateSample.setPerfmonSample(doSample);
	dcb->setPerfCounterControl(stateSample);
}

void PerfCounterManager::stopPerfCounters(Gnm::ConstantCommandBuffer* ccb, Gnm::DrawCommandBuffer* dcb, Gnm::PerfmonSample doSample)
{
	dcb->triggerEvent(Gnm::kEventTypePerfCounterStop);

	// Even with windowing enabled, we can't be sure that previous windowed draw calls have completed unless we sync here:
	syncTheGpuIfNeeded(ccb, dcb);
	m_state.setPerfmonState(Gnm::kPerfmonStateStopCounting);
	Gnm::PerfCounterControl stateSample = m_state;
	stateSample.setPerfmonSample(doSample);
	dcb->setPerfCounterControl(stateSample);
}

void PerfCounterManager::resetPerfCounters(Gnm::ConstantCommandBuffer* ccb, Gnm::DrawCommandBuffer* dcb)
{
	dcb->triggerEvent(Gnm::kEventTypePerfCounterStop);

	syncTheGpuIfNeeded(ccb, dcb);
	m_state.setPerfmonState(Gnm::kPerfmonStateStopCounting);
	Gnm::PerfCounterControl stateSample = m_state;
	stateSample.setPerfmonState(Gnm::kPerfmonStateDisableAndReset);
	dcb->setPerfCounterControl(stateSample);
}

void PerfCounterManager::samplePerfCounters(Gnm::ConstantCommandBuffer* ccb, Gnm::DrawCommandBuffer* dcb) const
{
	dcb->triggerEvent(Gnm::kEventTypePerfCounterSample);

	// Even with windowing enabled, we can't be sure that previous windowed draw calls have completed unless we sync here:
	syncTheGpuIfNeeded(ccb, dcb);
	Gnm::PerfCounterControl stateSample = m_state;
	stateSample.setPerfmonSample(Gnm::kPerfmonSample);
	dcb->setPerfCounterControl(stateSample);
}

void PerfCounterManager::selectPerfCounters(Gnm::DrawCommandBuffer* dcb) const
{
	for (uint32_t iMeasurement = 0; iMeasurement < m_numMeasurements; ++iMeasurement)
		m_aMeasurements[iMeasurement].pSelection->selectPerfCounters(dcb);
	if (m_maskHwUnits & ((1<<kHwUnitSq)|(1<<kHwUnitTa))) {
		dcb->setSqPerfCounterControl(Gnm::kShaderEngineBroadcastAll, Gnm::SqPerfCounterControl(0x7F, 0));
	} else {
		dcb->setSqPerfCounterControl(Gnm::kShaderEngineBroadcastAll, Gnm::SqPerfCounterControl());
	}
}

bool PerfCounterManager::readPerfCounterValues(Gnm::DrawCommandBuffer* dcb, uint64_t const* pPerfCounterValueBuffer, uint32_t iSample) const
{
	if (((uintptr_t)pPerfCounterValueBuffer) & (Gnm::kAlignmentOfBufferInBytes-1))
		return false;

	uint64_t* pgpuCounterValues = (uint64_t*)((uintptr_t)pPerfCounterValueBuffer + m_sizeofCounterValues*iSample);
	for (uint32_t iMeasurement = 0; iMeasurement < m_numMeasurements; ++iMeasurement)
		pgpuCounterValues = (uint64_t*)((uintptr_t)pgpuCounterValues + m_aMeasurements[iMeasurement].pSelection->readPerfCounters(dcb, pgpuCounterValues));
	SCE_GNM_ASSERT((uintptr_t)pgpuCounterValues == (uintptr_t)pPerfCounterValueBuffer + m_sizeofCounterValues*iSample + m_sizeofCounterValues);
	return true;
}

uint64_t PerfCounterManager::getPerfCounterValue(uint64_t const* pPerfCounterValueBuffer, uint32_t iMeasurement, uint32_t iCounterValue, uint32_t iSample) const
{
	if (!(iMeasurement < m_numMeasurements))
		return 0;
	uint32_t numCounterValues = m_aMeasurements[iMeasurement].pSelection->getNumPerfCounterValues();
	if (!(iCounterValue < numCounterValues))
		return 0;
	uint64_t const* pValue = (uint64_t const*)((uintptr_t)pPerfCounterValueBuffer + iSample*m_sizeofCounterValues + m_aMeasurements[iMeasurement].startOffset);
	return pValue[iCounterValue];
}

int64_t PerfCounterManager::getPerfCounterValueDelta(uint64_t const* pPerfCounterValueBuffer, uint32_t iMeasurement, uint32_t iCounterValue, uint32_t iSample) const
{
	if (!(iMeasurement < m_numMeasurements || iSample == 0))
		return 0;
	uint32_t numCounterValues = m_aMeasurements[iMeasurement].pSelection->getNumPerfCounterValues();
	if (!(iCounterValue < numCounterValues))
		return 0;
	uint64_t const* pValue = (uint64_t const*)((uintptr_t)pPerfCounterValueBuffer + iSample*m_sizeofCounterValues + m_aMeasurements[iMeasurement].startOffset);
	uint64_t const* pValuePrev = (uint64_t const*)((uintptr_t)pValue - m_sizeofCounterValues);
	return (int64_t)( pValue[iCounterValue] - pValuePrev[iCounterValue] );
}

uint64_t PerfCounterManager::getPerfCounterTotalValue(uint64_t const* pPerfCounterValueBuffer, uint32_t iMeasurement, uint32_t iCounterSlot, uint32_t iSample) const
{
	if (!(iMeasurement < m_numMeasurements))
		return 0;
	uint64_t const* pCounterValues = (uint64_t const*)((uintptr_t)pPerfCounterValueBuffer + iSample*m_sizeofCounterValues + m_aMeasurements[iMeasurement].startOffset);
	return m_aMeasurements[iMeasurement].pSelection->getPerfCounterTotalValue(iCounterSlot, pCounterValues);
}

int64_t PerfCounterManager::getPerfCounterTotalValueDelta(uint64_t const* pPerfCounterValueBuffer, uint32_t iMeasurement, uint32_t iCounterSlot, uint32_t iSample) const
{
	if (!(iMeasurement < m_numMeasurements || iSample == 0))
		return 0;
	uint64_t const* pCounterValues = (uint64_t const*)((uintptr_t)pPerfCounterValueBuffer + iSample*m_sizeofCounterValues + m_aMeasurements[iMeasurement].startOffset);
	uint64_t const* pCounterValuesPrev = (uint64_t const*)((uintptr_t)pCounterValues - m_sizeofCounterValues);
	return (int64_t)(m_aMeasurements[iMeasurement].pSelection->getPerfCounterTotalValue(iCounterSlot, pCounterValues) - m_aMeasurements[iMeasurement].pSelection->getPerfCounterTotalValue(iCounterSlot, pCounterValuesPrev));
}

}	//namespace PerfCounters
