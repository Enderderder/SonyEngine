/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "perfcounterselection.h"
#include <assert.h>

using namespace sce;

namespace PerfCounters {

char const* kaszHwUnitName[kNumHwUnits] = { 
	"CP", "CPC", "CPF", "IA", "WD", 
	"GDS", "TCA", "TCS", "TCC", 
	"VGT", "PA-SU", "PA-SC", "PA-SC-QP",
	"SPI", "SX", "CB", "DB", 
	"SQ", "TA", "TD", "TCP",
};

const uint32_t kaNumPerfCounterSlots[kNumHwUnits] = {
	Gnm::kNumCpPerfCounterSlots,	Gnm::kNumCpcPerfCounterSlots,	Gnm::kNumCpfPerfCounterSlots,	Gnm::kNumIaPerfCounterSlots,	Gnm::kNumWdPerfCounterSlots,
	Gnm::kNumGdsPerfCounterSlots,	Gnm::kNumTcaPerfCounterSlots,	Gnm::kNumTcsPerfCounterSlots,	Gnm::kNumTccPerfCounterSlots,
	Gnm::kNumVgtPerfCounterSlots,	Gnm::kNumPaSuPerfCounterSlots,	Gnm::kNumPaScPerfCounterSlots,	Gnm::kNumPaScPerfCounterSlots,	
	Gnm::kNumSpiPerfCounterSlots,	Gnm::kNumSxPerfCounterSlots,	Gnm::kNumCbPerfCounterSlots,	Gnm::kNumDbPerfCounterSlots,
	Gnm::kNumSqPerfCounterSlots,	Gnm::kNumTaPerfCounterSlots,	Gnm::kNumTdPerfCounterSlots,	Gnm::kNumTcpPerfCounterSlots,
};

uint32_t kNumSeUnits = 2 /*Gnm::kNumSeUnits*/;

uint32_t kaNumHwUnits[kNumHwUnits] = {
	Gnm::kNumCpUnits,	Gnm::kNumCpcUnits,	Gnm::kNumCpfUnits,	1 /*Gnm::kNumIaUnits*/,	Gnm::kNumWdUnits,
	Gnm::kNumGdsUnits,	2 /*Gnm::kNumTcaUnits*/,	Gnm::kNumTcsUnits,	8 /*Gnm::kNumTccUnits*/,
	Gnm::kNumVgtUnitsPerSe*kNumSeUnits, Gnm::kNumPaSuUnitsPerSe*kNumSeUnits, Gnm::kNumPaScUnitsPerSe*kNumSeUnits, Gnm::kNumQuadPackerUnitsPerSe*kNumSeUnits,
	Gnm::kNumSpiUnitsPerSe*kNumSeUnits, Gnm::kNumSxUnitsPerSe*kNumSeUnits, Gnm::kNumCbUnitsPerSe*kNumSeUnits, Gnm::kNumDbUnitsPerSe*kNumSeUnits,
	Gnm::kNumSqPerfCounterUnitsPerSe*kNumSeUnits, Gnm::kNumCusPerSe*kNumSeUnits, Gnm::kNumCusPerSe*kNumSeUnits, Gnm::kNumCusPerSe*kNumSeUnits,
};

uint32_t kaNumHwUnitsPerSe[kNumHwUnits] = {
	0,	0,	0,	0,	0,
	0,	0,	0,	0,
	Gnm::kNumVgtUnitsPerSe,		Gnm::kNumPaSuUnitsPerSe,	Gnm::kNumPaScUnitsPerSe,	Gnm::kNumQuadPackerUnitsPerSe,
	Gnm::kNumSpiUnitsPerSe,		Gnm::kNumSxUnitsPerSe,		Gnm::kNumCbUnitsPerSe,		Gnm::kNumDbUnitsPerSe,
	Gnm::kNumSqPerfCounterUnitsPerSe,			Gnm::kNumCusPerSe,			Gnm::kNumCusPerSe,			Gnm::kNumCusPerSe,
};

uint32_t kaNumHwUnitsPerParent[kNumHwUnits] = {
	Gnm::kNumCpUnits,	Gnm::kNumCpcUnits,	Gnm::kNumCpfUnits,	1 /*Gnm::kNumIaUnits*/,	Gnm::kNumWdUnits,
	Gnm::kNumGdsUnits,	2 /*Gnm::kNumTcaUnits*/,	Gnm::kNumTcsUnits,	8 /*Gnm::kNumTccUnits*/,
	Gnm::kNumVgtUnitsPerSe,		Gnm::kNumPaSuUnitsPerSe,	Gnm::kNumPaScUnitsPerSe,	Gnm::kNumQuadPackerUnitsPerSe,
	Gnm::kNumSpiUnitsPerSe,		Gnm::kNumSxUnitsPerSe,		Gnm::kNumCbUnitsPerSe,		Gnm::kNumDbUnitsPerSe,
	Gnm::kNumSqPerfCounterUnitsPerSe,			Gnm::kNumCusPerSe,			Gnm::kNumCusPerSe,			Gnm::kNumCusPerSe,
};

void setGpuMode()
{
	kNumSeUnits = Gnm::getNumShaderEngines();
	kaNumHwUnits[kHwUnitIa] = kaNumHwUnitsPerParent[kHwUnitIa] = Gnm::getNumIaUnits();
	kaNumHwUnits[kHwUnitTca] = kaNumHwUnitsPerParent[kHwUnitTca] = Gnm::getNumTcaUnits();
	kaNumHwUnits[kHwUnitTcc] = kaNumHwUnitsPerParent[kHwUnitTcc] = Gnm::getNumTccUnits();
	for (uint32_t hwUnit = kFirstHwUnitPerSe; hwUnit < kNumHwUnits; ++hwUnit)
		kaNumHwUnits[hwUnit] = kaNumHwUnitsPerSe[hwUnit] * kNumSeUnits;
}

uint32_t getNumHwUnits(HardwareUnit unit)
{
	if (unit >= kNumHwUnits)
		return 0;
	return kaNumHwUnits[unit];
}
uint32_t getNumHwUnitsPerSe(HardwareUnit unit)
{
	if (unit >= kNumHwUnits)
		return 0;
	return kaNumHwUnitsPerSe[unit];
}
uint32_t getNumHwUnitsPerParent(HardwareUnit unit)
{
	if (unit >= kNumHwUnits)
		return 0;
	return kaNumHwUnitsPerParent[unit];
}

bool isQuadPackerPerfCounter(sce::Gnm::PaScPerfCounter paScCounter)
{
	if (paScCounter >= sce::Gnm::kPaScPerfCounterStalledByDbTile && paScCounter <= sce::Gnm::kPaScPerfCounterStalledBySpi)
		return true;
	if (paScCounter >= sce::Gnm::kPaScPerfCounterTilePerPrimH0 && paScCounter <= sce::Gnm::kPaScPerfCounterTilePerPrimH16)
		return true;
	if (paScCounter >= sce::Gnm::kPaScPerfCounterTilePickedH1 && paScCounter <= sce::Gnm::kPaScPerfCounterGrp4DynSclkBusy)
		return true;
	return false;
}
bool isQuadPackerPerfCounterSet(sce::Gnm::PaScPerfCounter const *aCounters, uint32_t numCounters)
{
	bool bIsQuadPacker = isQuadPackerPerfCounter(aCounters[0]);
	for (uint32_t i = 1; i < numCounters; ++i) {
		assert(isQuadPackerPerfCounter(aCounters[i]) == bIsQuadPacker);
	}
	return bIsQuadPacker;
}

HwUnitTarget getUnitInstance(HardwareUnit unit, uint32_t globalLogicalInstance)
{
	if (!(unit < kNumHwUnits && globalLogicalInstance < kaNumHwUnits[unit]))
		return HwUnitTarget();
	if (kaNumHwUnitsPerSe[unit] != 0) {
		if (kaNumHwUnitsPerSe[unit] > 1) {
			uint32_t seIndex = globalLogicalInstance / kaNumHwUnitsPerSe[unit];
			uint8_t instanceIndex = (uint8_t)(globalLogicalInstance - seIndex * kaNumHwUnitsPerSe[unit]);
			assert(seIndex < kNumSeUnits);
			return HwUnitTarget((sce::Gnm::ShaderEngine)seIndex, instanceIndex);
		} else {
			assert(globalLogicalInstance < kNumSeUnits);
			return HwUnitTarget((sce::Gnm::ShaderEngineBroadcast)globalLogicalInstance);
		}
	}
	else {
		// For global units, kShaderEngine0 is just a dummy value.
		return HwUnitTarget(sce::Gnm::kShaderEngine0, (uint8_t)globalLogicalInstance);
	}
}

void HwUnitTarget::setSqInstanceMask(uint32_t const *aSqInstanceMaskBySe/*[kNumSeUnits]*/)
{
	m_target = kTargetSqInstanceMask;
	for (uint32_t se = 0; se < kNumSeUnits; ++se)
		m_target |= (uint64_t)(aSqInstanceMaskBySe[0] & ((1 << kTargetSqInstanceMaskShiftPerSe) - 1)) << (kTargetInstanceValueShift + kTargetSqInstanceMaskShiftPerSe*se);
}

void HwUnitTarget::setSqInstanceMask(sce::Gnm::ShaderEngine engine, uint32_t sqInstanceMaskForSe)
{
	m_target = kTargetSqInstanceMask;
	m_target |= (uint64_t)(sqInstanceMaskForSe & ((1 << kTargetSqInstanceMaskShiftPerSe) - 1)) << (kTargetInstanceValueShift + kTargetSqInstanceMaskShiftPerSe*(engine & 3));
}

uint32_t PerfCounterSelection::getNumPerfCounterValues() const
{
	if (!(m_unit < kNumHwUnits && m_numCounterSlots > 0 && m_counterSlotStart + m_numCounterSlots <= kaNumPerfCounterSlots[m_unit]))
		return 0;
	uint32_t numCounterValues = m_unitInstances.isInstanceBroadcast() ? kaNumHwUnitsPerParent[m_unit] : 1;
	if (m_unitInstances.isSeBroadcast() && kaNumHwUnitsPerSe[m_unit] != 0)
		numCounterValues *= kNumSeUnits;
	return numCounterValues * m_numCounterSlots;
}

uint32_t PerfCounterSelection::getNumPerfCounterValuesPerSlot() const
{
	if (!(m_unit < kNumHwUnits))
		return 0;
	uint32_t numCounterValues = m_unitInstances.isInstanceBroadcast() ? kaNumHwUnitsPerParent[m_unit] : 1;
	if (m_unitInstances.isSeBroadcast() && kaNumHwUnitsPerSe[m_unit] != 0)
		numCounterValues *= kNumSeUnits;
	return numCounterValues;
}

void PerfCounterSelection::selectPerfCounters(sce::Gnm::DrawCommandBuffer* dcb) const
{
#ifdef __ORBIS__
	if (!(m_unit < kNumHwUnits && m_numCounterSlots > 0 && m_counterSlotStart + m_numCounterSlots <= kaNumPerfCounterSlots[m_unit]))
		return;
	SCE_GNM_ASSERT_MSG(m_unit == kHwUnitSq || !m_unitInstances.isSqInstanceMask(), "PerfCounterSelection currently only supports instance masking for SQ perf counters");
	switch (m_unit) {
	case kHwUnitCp:		dcb->selectCpPerfCounters(		m_counterSlotStart, m_numCounterSlots, (Gnm::CpPerfCounter const*)m_aCounterSelect); break;
	case kHwUnitCpc:	dcb->selectCpcPerfCounters(		m_counterSlotStart, m_numCounterSlots, (Gnm::CpcPerfCounter const*)m_aCounterSelect); break;
	case kHwUnitCpf:	dcb->selectCpfPerfCounters(		m_counterSlotStart, m_numCounterSlots, (Gnm::CpfPerfCounter const*)m_aCounterSelect); break;
	case kHwUnitIa:
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectIaPerfCounters(m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::IaPerfCounterSelect const*)m_aCounterSelect);
		else
			dcb->selectIaPerfCounters(m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::IaPerfCounterSelect const*)m_aCounterSelect);
		break;
	case kHwUnitWd:		dcb->selectWdPerfCounters(		m_counterSlotStart, m_numCounterSlots, (Gnm::WdPerfCounterSelect const*)m_aCounterSelect); break;
	case kHwUnitGds:	dcb->selectGdsPerfCounters(		m_counterSlotStart, m_numCounterSlots, (Gnm::GdsPerfCounter const*)m_aCounterSelect); break;
	case kHwUnitTca:
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectTcaPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::TcaPerfCounterSelect const*)m_aCounterSelect);
		else
			dcb->selectTcaPerfCounters(		m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::TcaPerfCounterSelect const*)m_aCounterSelect);
		break;
	case kHwUnitTcs:	dcb->selectTcsPerfCounters(		m_counterSlotStart, m_numCounterSlots, (Gnm::TcsPerfCounterSelect const*)m_aCounterSelect); break;
	case kHwUnitTcc:	
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectTccPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::TccPerfCounterSelect const*)m_aCounterSelect);
		else
			dcb->selectTccPerfCounters(		m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::TccPerfCounterSelect const*)m_aCounterSelect);
		break;
	case kHwUnitVgt:	
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectVgtPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::VgtPerfCounterSelect const*)m_aCounterSelect);
		else
			dcb->selectVgtPerfCounters(		m_unitInstances.getSeIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::VgtPerfCounterSelect const*)m_aCounterSelect);
		break;
	case kHwUnitPaSu:	
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectPaSuPerfCounters(	m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::PaSuPerfCounter const*)m_aCounterSelect);
		else
			dcb->selectPaSuPerfCounters(	m_unitInstances.getSeIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::PaSuPerfCounter const*)m_aCounterSelect);
		break;
	case kHwUnitPaSc:	
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectPaScPerfCounters(	m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::PaScPerfCounter const*)m_aCounterSelect);
		else
			dcb->selectPaScPerfCounters(	m_unitInstances.getSeIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::PaScPerfCounter const*)m_aCounterSelect);
		break;
	case kHwUnitPaScQuadPacker:	
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectPaScPerfCounters(	m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::PaScPerfCounter const*)m_aCounterSelect);
		else
			dcb->selectPaScPerfCounters(	m_unitInstances.getSeIndex(), m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::PaScPerfCounter const*)m_aCounterSelect);
		break;
	case kHwUnitSpi:	
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectSpiPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::SpiPerfCounter const*)m_aCounterSelect);
		else
			dcb->selectSpiPerfCounters(		m_unitInstances.getSeIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::SpiPerfCounter const*)m_aCounterSelect);
		break;
	case kHwUnitSx:		
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectSxPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::SxPerfCounter const*)m_aCounterSelect);
		else
			dcb->selectSxPerfCounters(		m_unitInstances.getSeIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::SxPerfCounter const*)m_aCounterSelect);
		break;
	case kHwUnitCb:		
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectCbPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::CbPerfCounterSelect const*)m_aCounterSelect);
		else
			dcb->selectCbPerfCounters(		m_unitInstances.getSeIndex(), m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::CbPerfCounterSelect const*)m_aCounterSelect);
		break;
	case kHwUnitDb:		
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectDbPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::DbPerfCounterSelect const*)m_aCounterSelect);
		else
			dcb->selectDbPerfCounters(		m_unitInstances.getSeIndex(), m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::DbPerfCounterSelect const*)m_aCounterSelect);
		break;
	case kHwUnitSq:
		if (m_unitInstances.isSqInstanceMask()) {
			uint32_t seIndex0 = m_unitInstances.isSeBroadcast() ? 0 : m_unitInstances.getSeIndex();
			uint32_t seIndexEnd = m_unitInstances.isSeBroadcast() ? kNumSeUnits : seIndex0+1;
			for (uint32_t seIndex = seIndex0; seIndex < seIndexEnd; ++seIndex) {
				uint16_t cuMaskSe = m_unitInstances.getSqInstanceMask((sce::Gnm::ShaderEngine)seIndex);
				uint16_t aCuMaskSe[4] = { cuMaskSe, cuMaskSe, cuMaskSe, cuMaskSe };
				dcb->selectSqPerfCounters(		(sce::Gnm::ShaderEngine)seIndex, m_counterSlotStart, m_numCounterSlots, (Gnm::SqPerfCounterSelect const*)m_aCounterSelect, aCuMaskSe);
			}
		} else if (m_unitInstances.isInstanceBroadcast())
			dcb->selectSqPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::SqPerfCounterSelect const*)m_aCounterSelect);
		else
			dcb->selectSqPerfCounters(		m_unitInstances.getSeIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::SqPerfCounterSelect const*)m_aCounterSelect);
		break;
	case kHwUnitTa:		
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectTaPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::TaPerfCounterSelect const*)m_aCounterSelect);
		else
			dcb->selectTaPerfCounters(		m_unitInstances.getSeIndex(), m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::TaPerfCounterSelect const*)m_aCounterSelect);
		break;
	case kHwUnitTd:		
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectTdPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::TdPerfCounterSelect const*)m_aCounterSelect);
		else
			dcb->selectTdPerfCounters(		m_unitInstances.getSeIndex(), m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::TdPerfCounterSelect const*)m_aCounterSelect);
		break;
	case kHwUnitTcp:	
		if (m_unitInstances.isInstanceBroadcast())
			dcb->selectTcpPerfCounters(		m_unitInstances.getSeBroadcast(), m_counterSlotStart, m_numCounterSlots, (Gnm::TcpPerfCounterSelect const*)m_aCounterSelect);
		else
			dcb->selectTcpPerfCounters(		m_unitInstances.getSeIndex(), m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, (Gnm::TcpPerfCounterSelect const*)m_aCounterSelect);
		break;
	default:
		break;
	}
#endif	//#ifdef __ORBIS__
}

size_t PerfCounterSelection::readPerfCounters(Gnm::DrawCommandBuffer* dcb, uint64_t* pgpuCounterValueStart) const
{
#ifdef __ORBIS__
	if (!(m_unit < kNumHwUnits && m_numCounterSlots > 0 && m_counterSlotStart + m_numCounterSlots <= kaNumPerfCounterSlots[m_unit]))
		return 0;
	switch (m_unit) {
	case kHwUnitCp:		dcb->readCpPerfCounters(	m_counterSlotStart, m_numCounterSlots, pgpuCounterValueStart); return 8 * m_numCounterSlots;
	case kHwUnitCpc:	dcb->readCpcPerfCounters(	m_counterSlotStart, m_numCounterSlots, pgpuCounterValueStart); return 8 * m_numCounterSlots;
	case kHwUnitCpf:	dcb->readCpfPerfCounters(	m_counterSlotStart, m_numCounterSlots, pgpuCounterValueStart); return 8 * m_numCounterSlots;
	case kHwUnitIa:
		if (m_unitInstances.isInstanceBroadcast()) {
			uint64_t* pgpuCounterValue = pgpuCounterValueStart;
			for (uint32_t instanceIndex = 0; instanceIndex < kaNumHwUnits[m_unit]; ++instanceIndex, pgpuCounterValue += m_numCounterSlots) {
				dcb->readIaPerfCounters(instanceIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue);
			}
			return ((uintptr_t)pgpuCounterValue - (uintptr_t)pgpuCounterValueStart);
		} else {
			dcb->readIaPerfCounters(m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, pgpuCounterValueStart);
			return 8 * m_numCounterSlots;
		}
	case kHwUnitWd:		dcb->readWdPerfCounters(	m_counterSlotStart, m_numCounterSlots, pgpuCounterValueStart); return 8 * m_numCounterSlots;
	case kHwUnitGds:	dcb->readGdsPerfCounters(	m_counterSlotStart, m_numCounterSlots, pgpuCounterValueStart); return 8 * m_numCounterSlots;
	case kHwUnitTca:
		if (m_unitInstances.isInstanceBroadcast()) {
			uint64_t* pgpuCounterValue = pgpuCounterValueStart;
			for (uint32_t instanceIndex = 0; instanceIndex < kaNumHwUnits[m_unit]; ++instanceIndex, pgpuCounterValue += m_numCounterSlots) {
				dcb->readTcaPerfCounters(instanceIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue);
			}
			return ((uintptr_t)pgpuCounterValue - (uintptr_t)pgpuCounterValueStart);
		} else {
			dcb->readTcaPerfCounters(m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, pgpuCounterValueStart);
			return 8 * m_numCounterSlots;
		}
	case kHwUnitTcs:	dcb->readTcsPerfCounters(	m_counterSlotStart, m_numCounterSlots, pgpuCounterValueStart); return 8 * m_numCounterSlots;
	case kHwUnitTcc:
		if (m_unitInstances.isInstanceBroadcast()) {
			uint64_t* pgpuCounterValue = pgpuCounterValueStart;
			for (uint32_t instanceIndex = 0; instanceIndex < kaNumHwUnits[m_unit]; ++instanceIndex, pgpuCounterValue += m_numCounterSlots) {
				dcb->readTccPerfCounters(instanceIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue);
			}
			return ((uintptr_t)pgpuCounterValue - (uintptr_t)pgpuCounterValueStart);
		} else {
			dcb->readTccPerfCounters(m_unitInstances.getInstanceIndex(), m_counterSlotStart, m_numCounterSlots, pgpuCounterValueStart);
			return 8 * m_numCounterSlots;
		}
	default:
		{
			uint32_t const kNumHwUnitsPerParent = kaNumHwUnitsPerParent[m_unit];
			uint32_t seIndex0 = m_unitInstances.isSeBroadcast() ? 0 : m_unitInstances.getSeIndex();
			uint32_t seIndexEnd = m_unitInstances.isSeBroadcast() ? kNumSeUnits : seIndex0+1;
			uint64_t* pgpuCounterValue = pgpuCounterValueStart;
			if (kNumHwUnitsPerParent == 1) {
				for (uint32_t seIndex = seIndex0; seIndex < seIndexEnd; ++seIndex, pgpuCounterValue += m_numCounterSlots) {
					switch (m_unit) {
					case kHwUnitVgt:	dcb->readVgtPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
					case kHwUnitPaSu:	dcb->readPaSuPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
					case kHwUnitPaSc:	dcb->readPaScPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
					case kHwUnitSpi:	dcb->readSpiPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
					case kHwUnitSx:		dcb->readSxPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
					case kHwUnitSq:		dcb->readSqPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
					default:			break;
					}
				}
			} else {
				uint32_t instanceIndex0 = m_unitInstances.isInstanceBroadcast() ? 0 : m_unitInstances.getInstanceIndex();
				uint32_t instanceIndexEnd = m_unitInstances.isInstanceBroadcast() ? kNumHwUnitsPerParent : instanceIndex0+1;
				for (uint32_t seIndex = seIndex0; seIndex < seIndexEnd; ++seIndex) {
					for (uint32_t instanceIndex = instanceIndex0; instanceIndex < instanceIndexEnd; ++instanceIndex, pgpuCounterValue += m_numCounterSlots) {
						switch (m_unit) {
						case kHwUnitPaScQuadPacker:	dcb->readPaScPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, instanceIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
						case kHwUnitCb:		dcb->readCbPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, instanceIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
						case kHwUnitDb:		dcb->readDbPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, instanceIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
						case kHwUnitTa:		dcb->readTaPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, instanceIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
						case kHwUnitTd:		dcb->readTdPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, instanceIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
						case kHwUnitTcp:	dcb->readTcpPerfCounters(	(sce::Gnm::ShaderEngine)seIndex, instanceIndex, m_counterSlotStart, m_numCounterSlots, pgpuCounterValue); break;
						default:			break;
						}
					}
				}
			}
			return ((uintptr_t)pgpuCounterValue - (uintptr_t)pgpuCounterValueStart);
		}
	}
#endif	//#ifdef __ORBIS__
}

bool PerfCounterSelection::getPerfCounterValueInfo(uint32_t iCounterValue, HwUnitTarget* pUnitInstance, uint32_t* pCounterSlot) const
{
	uint32_t numCounterValues = getNumPerfCounterValues();
	if (!(iCounterValue < numCounterValues))
		return false;
	if (pCounterSlot)
		*pCounterSlot = iCounterValue % m_numCounterSlots;
	if (!pUnitInstance)
		return true;
	uint32_t iIndex = iCounterValue / m_numCounterSlots;
	if (m_unitInstances.isSqInstanceMask()) {
		uint32_t aSqInstanceMaskForSe[4] = { 0, 0, 0, 0 };
		for (uint32_t se = 0; se < kNumSeUnits; ++se)
			aSqInstanceMaskForSe[se] = m_unitInstances.getSqInstanceMask((sce::Gnm::ShaderEngine)se);
		pUnitInstance->setSqInstanceMask(aSqInstanceMaskForSe);
	} else if (!m_unitInstances.isInstanceBroadcast()) {
		*pUnitInstance = HwUnitTarget(m_unitInstances.getSeIndex(), m_unitInstances.getInstanceIndex());
	} else if (!m_unitInstances.isSeBroadcast()) {
		*pUnitInstance = HwUnitTarget(m_unitInstances.getSeIndex(), (uint8_t)iIndex);
	} else {
		uint32_t seIndex = iIndex / kaNumHwUnitsPerParent[m_unit];
		uint32_t instanceIndex = (iIndex - seIndex * kaNumHwUnitsPerParent[m_unit]);
		assert(seIndex < (uint32_t)(kaNumHwUnitsPerSe[m_unit] != 0 ? kNumSeUnits : 1));
		*pUnitInstance = HwUnitTarget((sce::Gnm::ShaderEngine)seIndex, (uint8_t)instanceIndex);
	}
	return true;
}

uint64_t PerfCounterSelection::getPerfCounterTotalValue(uint32_t iCounterSlot, uint64_t const* pCounterValueStart) const
{
	uint32_t numCounterValues = getNumPerfCounterValues();
	if (!(iCounterSlot < m_numCounterSlots))
		return 0;
	uint64_t perfCounterTotalValue = 0;
	for (uint32_t iCounterValue = iCounterSlot; iCounterValue < numCounterValues; iCounterValue += m_numCounterSlots)
		perfCounterTotalValue += pCounterValueStart[iCounterValue];
	return perfCounterTotalValue;
}

}	//namespace PerfCounters
