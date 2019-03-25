/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

// Simplet #4
// This sample demonstrates how to use the Gnmx layer for the compute queues.

#include "../toolkit/simplet-common.h"
using namespace sce;

static const unsigned s_cs_add2_c[] = {
	#include "cs-add2_c.h"
};

const uint32_t			NUM_THREADS	 = 64;
const uint32_t			NUM_GROUPS	 = 2;
const uint32_t			NUM_ELEMENTS = NUM_GROUPS * NUM_THREADS;

// {
//   "title" : "simplet-cmpc-test",
//   "overview" : "This sample demonstrates how to use the Gnmx layer for the compute queues.",
//   "explanation" : "This sample builds two dispatch command buffers using: Gnmx::ComputeContext and then uses an instance of Gnmx::ComputeQueue to run them back to back."
// }

static void Test(void)
{
	// The compute shader used in this sample is very basic, it reads inputs from two different buffers (bufIn1 and bufIn2)
	// add them together then write the result in an output buffer (bufOut2).


	//
	// Load the compute shaders:
	//

	Gnmx::CsShader *add2Csb = SimpletUtil::LoadCsShaderFromMemory(s_cs_add2_c);

	// In this part of the simplet (the same way it was done for the single-fs simplet),
	// the setup of this resources is hardcoded.
	// The following code make sure the hardcoded settings match the expected input to avoid blue screening.
	const Gnm::InputUsageSlot *inputUsageSlots = add2Csb->getInputUsageSlotTable();

	const uint32_t bufIn1StartRegister = SimpletUtil::getStartRegister(inputUsageSlots, add2Csb->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageImmResource, 0);
	const uint32_t bufIn2StartRegister = SimpletUtil::getStartRegister(inputUsageSlots, add2Csb->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageImmResource, 1);
	const uint32_t bufOutStartRegister = SimpletUtil::getStartRegister(inputUsageSlots, add2Csb->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageImmRwResource, 0);

	SCE_GNM_ASSERT(bufIn1StartRegister < 16);
	SCE_GNM_ASSERT(bufIn2StartRegister < 16);
	SCE_GNM_ASSERT(bufOutStartRegister < 16);

	//
	// Initialize the buffer:
	//

	void *memIn1 = SimpletUtil::allocateGarlicMemory(NUM_ELEMENTS*4, Gnm::kAlignmentOfBufferInBytes);
	void *memIn2 = SimpletUtil::allocateGarlicMemory(NUM_ELEMENTS*4, Gnm::kAlignmentOfBufferInBytes);
	void *memOut1 = SimpletUtil::allocateGarlicMemory(NUM_ELEMENTS*4, Gnm::kAlignmentOfBufferInBytes);
	void *memOut2 = SimpletUtil::allocateGarlicMemory(NUM_ELEMENTS*4, Gnm::kAlignmentOfBufferInBytes);

	uint32_t *in1  = (uint32_t *)memIn1;
	uint32_t *in2  = (uint32_t *)memIn2;
	uint32_t *out1 = (uint32_t *)memOut1;
	uint32_t *out2 = (uint32_t *)memOut2;

	for (uint32_t i=0; i<NUM_ELEMENTS; ++i)
	{
		in1[i]   = i;
		in2[i]   = i*2;
	}
	for (uint32_t i=0; i<NUM_ELEMENTS/2; ++i)
	{
		out1[i*2]   = 0xcc;
		out1[i*2+1] = 0xdd;
		out2[i*2]   = 0xcc;
		out2[i*2+1] = 0xdd;
	}

	// Input "Buffer"s and output "RWBuffer" in the shader are setup using Gnm::Buffer.
	Gnm::Buffer bufIn1,  bufIn2;
	Gnm::Buffer bufOut1, bufOut2;
	bufIn1.initAsDataBuffer(memIn1, Gnm::kDataFormatR32Uint, NUM_ELEMENTS);
	bufIn2.initAsDataBuffer(memIn2, Gnm::kDataFormatR32Uint, NUM_ELEMENTS);
	bufOut1.initAsDataBuffer(memOut1, Gnm::kDataFormatR32Uint, NUM_ELEMENTS);
	bufOut2.initAsDataBuffer(memOut2, Gnm::kDataFormatR32Uint, NUM_ELEMENTS);

	bufIn1.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we don't write to it, so read-only is OK
	bufIn2.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we don't write to it, so read-only is OK
	bufOut1.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we write to it, so it's GPU coherent
	bufOut2.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we write to it, so it's GPU coherent

	//
	// Setup a Compute Queue:
	//

	const uint32_t memRingSize = 4 * 1024;
	void *memRing = SimpletUtil::allocateOnionMemory(memRingSize, 256);
	void *memReadPtr = SimpletUtil::allocateGarlicMemory(4, 16);

	memset(memRing, 0, memRingSize);

	Gnmx::ComputeQueue cq;
	
	cq.initialize(0,3);
	cq.map(memRing, memRingSize/4, memReadPtr);



	//
	// Create a synchronization point:
	//

	volatile uint64_t *label1 = (volatile uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));
	volatile uint64_t *label2 = (volatile uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));
	*label1 = 3;
	*label2 = 3;

	const uint64_t zero = 0;


	// Initialize the first DispatchCommandBuffer
	Gnmx::ComputeContext cmpc1;
	cmpc1.init(SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes);
	cmpc1.setCsShader(&add2Csb->m_csStageRegisters);
	cmpc1.setVsharpInUserData(bufIn1StartRegister, &bufIn1);
	cmpc1.setVsharpInUserData(bufIn2StartRegister, &bufIn2);
	cmpc1.setVsharpInUserData(bufOutStartRegister, &bufOut1);
	cmpc1.dispatch(NUM_GROUPS, 1, 1);
	cmpc1.writeReleaseMemEvent(Gnm::kReleaseMemEventCbDbReadsDone,
							  Gnm::kEventWriteDestMemory, (void*)label1,
							  Gnm::kEventWriteSource64BitsImmediate, 0x1,
							  Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
	cmpc1.waitOnAddress((void*)label1, 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
	cmpc1.writeDataInline((void*)label1, &zero, 2, Gnm::kWriteDataConfirmEnable);

	//

	// Initialize the second DispatchCommandBuffer
	Gnmx::ComputeContext cmpc2;
	cmpc2.init(SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes);
	cmpc2.setCsShader(&add2Csb->m_csStageRegisters);
	cmpc2.setVsharpInUserData(bufIn1StartRegister, &bufIn1);
	cmpc2.setVsharpInUserData(bufIn2StartRegister, &bufIn2);
	cmpc2.setVsharpInUserData(bufOutStartRegister, &bufOut2);
	cmpc2.dispatch(NUM_GROUPS, 1, 1);
	cmpc2.writeReleaseMemEvent(Gnm::kReleaseMemEventCbDbReadsDone,
							  Gnm::kEventWriteDestMemory, (void*)label2,
							  Gnm::kEventWriteSource64BitsImmediate, 0x1,
							  Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
	cmpc2.waitOnAddress((void*)label2, 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
	cmpc2.writeDataInline((void*)label2, &zero, 2, Gnm::kWriteDataConfirmEnable);

	//
	// Submit the dispatch command buffer:
	//

	cq.submit(&cmpc1);
	cq.submit(&cmpc2);

	//
	// Wait for the execution to be complete:
	//
	uint32_t wait = 0;
	while (*label1 != 0 || *label2 != 0)
		wait++;

	//
	// Display the result of compute job
	//
	bool success = true;
	uint32_t *result1 = (uint32_t *)memOut1;
	uint32_t *result2 = (uint32_t *)memOut2;
	
	for (uint32_t i=0; i<NUM_ELEMENTS; ++i)
	{
		if (result1[i] != in1[i]+in2[i])
		{
			success = false;
			printf("%3i = %3i + %3i (%3i%s)\n", result1[i], in1[i], in2[i], in1[i]+in2[i], result1[i] == in1[i]+in2[i] ? "" : " ***");
		}
	}
	for (uint32_t i=0; i<NUM_ELEMENTS; ++i)
	{
		if (result2[i] != in1[i]+in2[i])
		{
			success = false;
			printf("%3i = %3i + %3i (%3i%s)\n", result2[i], in1[i], in2[i], in1[i]+in2[i], result2[i] == in1[i]+in2[i] ? "" : " ***");
		}
	}

	if (success)
		printf("SUCCESS: All sums are correct!\n");
	else
		printf("ERROR: Marked sums are incorrect!\n");

	cq.unmap();
}


int main()
{
	SimpletUtil::Init();

	Test();

	return 0;
}
