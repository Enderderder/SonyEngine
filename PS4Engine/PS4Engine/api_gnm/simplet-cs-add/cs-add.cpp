/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

// Simplet #4
// This simplet shows how to setup and run a basic compute shader.

#include "../toolkit/simplet-common.h"
using namespace sce;

const uint32_t			NUM_THREADS	 = 64;
const uint32_t			NUM_GROUPS	 = 2;
const uint32_t			NUM_ELEMENTS = NUM_GROUPS * NUM_THREADS;

static const unsigned s_cs_add2_c[] = {
	#include "cs-add2_c.h"
};

#define USING_COMPUTE_QUEUES

// {
//   "title" : "simplet-cs-add",
//   "overview" : "This sample demonstrates the operation of a simple compute shader.",
//   "explanation" : "This sample uses a compute shader to read from two buffers, add their values, and write the result to a third buffer."
// }


#ifdef USING_COMPUTE_QUEUES
#define SHADER_STAGE
#else
#define SHADER_STAGE Gnm::kShaderStageCs, 
#endif


static void Test(void)
{
	// The compute shader used in this sample is very basic, it reads inputs from two different buffers (bufIn1 and bufIn2)
	// add them together then write the result in an output buffer (bufOut2).



	//
	// Load the compute shaders:
	//

	Gnmx::CsShader *add2Csb = SimpletUtil::LoadCsShaderFromMemory(s_cs_add2_c);



	//
	// Initialize the buffer:
	//

	void *memIn1 = SimpletUtil::allocateGarlicMemory(NUM_ELEMENTS*4, Gnm::kAlignmentOfBufferInBytes);
	void *memIn2 = SimpletUtil::allocateGarlicMemory(NUM_ELEMENTS*4, Gnm::kAlignmentOfBufferInBytes);
	void *memOut = SimpletUtil::allocateGarlicMemory(NUM_ELEMENTS*4, Gnm::kAlignmentOfBufferInBytes);

	uint32_t *in1 = (uint32_t *)memIn1;
	uint32_t *in2 = (uint32_t *)memIn2;
	uint32_t *out = (uint32_t *)memOut;

	for (uint32_t i=0; i<NUM_ELEMENTS; ++i)
	{
		in1[i]   = i;
		in2[i]   = i*2;
	}
	for (uint32_t i=0; i<NUM_ELEMENTS/2; ++i)
	{
		out[i*2]   = 0xcc;
		out[i*2+1] = 0xdd;
	}



	//
	// Setup Buffers:
	//

	// Input "Buffer"s and output "RWBuffer" in the shader are setup using Gnm::Buffer.
	Gnm::Buffer bufOut;
	Gnm::Buffer bufIn1, bufIn2;
	bufIn1.initAsDataBuffer(memIn1, Gnm::kDataFormatR32Uint, NUM_ELEMENTS);
	bufIn2.initAsDataBuffer(memIn2, Gnm::kDataFormatR32Uint, NUM_ELEMENTS);
	bufOut.initAsDataBuffer(memOut, Gnm::kDataFormatR32Uint, NUM_ELEMENTS);

	bufIn1.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we only read from this buffer
	bufIn2.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we only read from this buffer
	bufOut.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we write to this buffer

#ifdef USING_COMPUTE_QUEUES
	const uint32_t memRingSize = 4*1024;
	void *memRing = SimpletUtil::allocateOnionMemory(memRingSize, Gnm::kAlignmentOfComputeQueueRingBufferInBytes);
	void *memReadPtr = SimpletUtil::allocateGarlicMemory(4, 16);

	memset(memRing, 0, memRingSize);

	uint32_t uqdId;
	int ret = sce::Gnm::mapComputeQueue(&uqdId, 0,0,
										memRing, memRingSize/4,
										memReadPtr);
	SCE_GNM_ASSERT(ret == 0);
#endif // USING_COMPUTE_QUEUES

	//
	// Create a synchronization point:
	//

 	volatile uint64_t *label = (volatile uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));
	*label = 2;

	const uint64_t zero = 0;

	// The if statement will demonstrate the execution the compute shader
	// The following code will execute the compute shader using a instance
	// of the low-level object: Gnm::DispatchCommandBuffer

	// initialize the DispatchCommandBuffer
#ifndef USING_COMPUTE_QUEUES
	Gnm::DrawCommandBuffer dcb;
	dcb.init(SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes, NULL, NULL);
#else
	Gnm::DispatchCommandBuffer dcb;
	dcb.init(memRing, memRingSize, NULL, NULL);
#endif // USING_COMPUTE_QUEUES

	// Set the compute shader:
	dcb.setCsShader(&add2Csb->m_csStageRegisters);

	// In this part of the simplet (the same way it was done for the single-fs simplet),
	// the setup of this resources is hardcoded.
	// The following code make sure the hardcoded settings match the expected input to avoid blue screening.
	const Gnm::InputUsageSlot *inputUsageSlots = add2Csb->getInputUsageSlotTable();

	const uint32_t bufIn1StartRegister = SimpletUtil::getStartRegister(inputUsageSlots, add2Csb->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageImmResource, 0);
	const uint32_t bufIn2StartRegister = SimpletUtil::getStartRegister(inputUsageSlots, add2Csb->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageImmResource, 1);
	const uint32_t bufOutStartRegister = SimpletUtil::getStartRegister(inputUsageSlots, add2Csb->m_common.m_numInputUsageSlots, Gnm::kShaderInputUsageImmRwResource, 0);

	// Manually set the input of the Compute Shader:
	dcb.setVsharpInUserData(SHADER_STAGE bufIn1StartRegister, &bufIn1);
	dcb.setVsharpInUserData(SHADER_STAGE bufIn2StartRegister, &bufIn2);
	dcb.setVsharpInUserData(SHADER_STAGE bufOutStartRegister, &bufOut);

	// Add the dispatch command and synchronization write:
	dcb.dispatch(NUM_GROUPS, 1, 1);

	// Make sure the shader task is complete:
#ifdef USING_COMPUTE_QUEUES
	dcb.writeReleaseMemEvent(Gnm::kReleaseMemEventCsDone,
							 Gnm::kEventWriteDestMemory, (void*)label,
							 Gnm::kEventWriteSource64BitsImmediate, 0x1,
							 Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
#else // USING_COMPUTE_QUEUES
	dcb.writeAtEndOfPipe(Gnm::kEopCsDone, 
						 Gnm::kEventWriteDestMemory, (void*)label,
						 Gnm::kEventWriteSource64BitsImmediate, 0x1,
						 Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
#endif // USING_COMPUTE_QUEUES

	dcb.waitOnAddress((void*)label, 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);

	dcb.writeDataInline((void*)label, &zero, 2, Gnm::kWriteDataConfirmEnable);

	// Submit the dispatch command buffer:
	 uint32_t cbSize = static_cast<uint32_t>(dcb.m_cmdptr - dcb.m_beginptr)*4;

#ifdef USING_COMPUTE_QUEUES
	sce::Gnm::dingDong(uqdId, cbSize/4);
#else // USING_COMPUTE_QUEUES
	void *ccbAddr = NULL;
	uint32_t ccbSize = 0;
	int32_t state = Gnm::submitCommandBuffers(1, (void**)&dcb.m_beginptr, &cbSize, &ccbAddr, &ccbSize);
	SCE_GNM_ASSERT(state == sce::Gnm::kSubmissionSuccess);
#endif // USING_COMPUTE_QUEUES

	//
	// Wait for the execution to be complete:
	//
	uint32_t wait = 0;
	while (*label != 0)
		wait++;

	//
	// Display the result of compute job
	//
	uint32_t *result = (uint32_t *)memOut;
	bool success = true;
	for (uint32_t i=0; i<NUM_ELEMENTS; ++i)
	{
		printf("%3i = %3i + %3i (%3i%s)\n", result[i], in1[i], in2[i], in1[i]+in2[i], result[i] == in1[i]+in2[i] ? "" : " ***");
		if (result[i] != in1[i]+in2[i])
			success = false;
	}
	if (success)
		printf("SUCCESS: All sums are correct!\n");
	else
		printf("ERROR: Marked sums are incorrect!\n");

#ifdef USING_COMPUTE_QUEUES
	sce::Gnm::unmapComputeQueue(uqdId);
#endif // USING_COMPUTE_QUEUES
}


int main()
{
	//
	// Initializes system and video memory and memory allocators.
	//
	SimpletUtil::Init();

	{
		//
		// Run the test
		//
		Test();
	}

	return 0;
}
