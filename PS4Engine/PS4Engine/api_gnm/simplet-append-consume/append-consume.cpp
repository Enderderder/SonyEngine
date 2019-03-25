/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../toolkit/simplet-common.h"
using namespace sce;

const uint32_t kNumElements = 128;
const uint32_t kNumThreadGroups = kNumElements / 64;
const uint32_t zero[2] = {0};

static const unsigned s_append_c[] = {
	#include "append_c.h"
};
static const unsigned s_consume_c[] = {
	#include "consume_c.h"
};
// {
//   "title" : "simplet-append-consume",
//   "overview" : "This sample demonstrates the set up and use of append-consume buffers.",
//   "explanation" : "This sample launches a job to append data to a buffer, and then launches a job to consume data from the same buffer."
// }

// Uncomment to run the sample on a compute queue instead of the graphics ring
//#define USE_COMPUTE_QUEUE

static void Test()
{
#ifdef USE_COMPUTE_QUEUE

	const uint32_t kNumBatches = 2; // Estimated number of batches to support within the resource buffer
	const uint32_t resourceBufferSize = Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(Gnmx::LightweightConstantUpdateEngine::kResourceBufferCompute, kNumBatches);

	Gnmx::ComputeContext context;
	context.init(
		(uint32_t*)SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes/sizeof(uint32_t), // Dispatch command buffer
		(uint32_t*)SimpletUtil::allocateGarlicMemory(resourceBufferSize, Gnm::kAlignmentOfBufferInBytes), resourceBufferSize, // LCUE resource buffer
		(uint32_t*)SimpletUtil::allocateGarlicMemory(SCE_GNM_SHADER_GLOBAL_TABLE_SIZE, Gnm::kAlignmentOfBufferInBytes)
		);
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
#else
	// Create and initialize Gnmx::GfxContext
	const uint32_t	kNumRingEntries	   = 64;
	const uint32_t	cueHeapSize		   = Gnmx::ConstantUpdateEngine::computeHeapSize(kNumRingEntries);
	Gnmx::GfxContext context;
	context.init(SimpletUtil::allocateGarlicMemory(cueHeapSize, Gnm::kAlignmentOfBufferInBytes), kNumRingEntries,	// Constant Update Engine
		SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes,			// Draw command buffer
		SimpletUtil::allocateOnionMemory(Gnm::kIndirectBufferMaximumSizeInBytes, Gnm::kAlignmentOfBufferInBytes), Gnm::kIndirectBufferMaximumSizeInBytes			// Constant command buffer
		);
#endif

	Gnmx::CsShader *csAppend = SimpletUtil::LoadCsShaderFromMemory(s_append_c);
	Gnmx::CsShader *csConsume = SimpletUtil::LoadCsShaderFromMemory(s_consume_c);

	uint32_t *pDataIn    = (uint32_t*)SimpletUtil::allocateGarlicMemory(kNumElements*sizeof(uint32_t), 4);
	uint32_t *pDataInter = (uint32_t*)SimpletUtil::allocateGarlicMemory(kNumElements*sizeof(uint32_t), 4);
	uint32_t *pDataOut   = (uint32_t*)SimpletUtil::allocateGarlicMemory(kNumElements*sizeof(uint32_t), 4);
	Gnm::Buffer bufIn, bufInter, bufOut;
	bufIn.initAsRegularBuffer(pDataIn, sizeof(uint32_t), kNumElements);
	bufInter.initAsRegularBuffer(pDataInter, sizeof(uint32_t), kNumElements);
	bufOut.initAsRegularBuffer(pDataOut, sizeof(uint32_t), kNumElements);

	bufIn.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // we never bind as RWBuffer, so read-only is OK
	bufInter.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we bind as RWBuffer, so it's GPU-coherent
	bufOut.setResourceMemoryType(Gnm::kResourceMemoryTypeGC); // we bind as RWBuffer, so it's GPU-coherent

	volatile uint64_t *submitLabel  = (volatile uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));
	volatile uint64_t *appendLabel  = (volatile uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));
	volatile uint64_t *consumeLabel = (volatile uint64_t*)SimpletUtil::allocateOnionMemory(sizeof(uint64_t), sizeof(uint64_t));
	volatile uint32_t *bufInterCounts = (volatile uint32_t*)SimpletUtil::allocateOnionMemory(sizeof(uint32_t)*2, Gnm::kAlignmentOfBufferInBytes);
	volatile uint32_t *bufInterCountsCopy = (volatile uint32_t*)SimpletUtil::allocateOnionMemory(sizeof(uint32_t)*2, Gnm::kAlignmentOfBufferInBytes);
	*submitLabel = 2;
	*appendLabel = 2;
	*consumeLabel = 2;
	bufInterCounts[0] = bufInterCountsCopy[0] = 0;
	bufInterCounts[1] = bufInterCountsCopy[1] = 0;
	for (uint32_t i=0; i<kNumElements; ++i)
	{
		pDataIn[i] = i;
		pDataInter[i] = 0x01234567;
		pDataOut[i] = 0xDEADC0DE;
	}

	// Pass 1: produce data
	context.clearAppendConsumeCounters(0x0000, 0, 1, 0);
	context.setCsShader(csAppend);
#ifdef USE_COMPUTE_QUEUE
	context.setBuffers(0, 1, &bufIn);
	context.setRwBuffers(0, 1, &bufInter);
	context.setAppendConsumeCounterRange(0, 4);
#else
	context.setBuffers(Gnm::kShaderStageCs, 0, 1, &bufIn);
	context.setRwBuffers(Gnm::kShaderStageCs, 0, 1, &bufInter);
	context.setAppendConsumeCounterRange(Gnm::kShaderStageCs, 0, 4);
#endif
	context.dispatch(kNumThreadGroups, 1, 1);

	// Redundant read to test readDataFromGds()
	// Note: Due to a GPU bug, a call to readDataFromGds() must immediately follow a dispatch call if the former is used with Gnm::kEosCsDone as its argument.
#ifdef USE_COMPUTE_QUEUE
	context.readDataFromGds((void*)&bufInterCountsCopy[0], 0x0000, 1);
#else
	context.readDataFromGds(Gnm::kEosCsDone, (void*)&bufInterCountsCopy[0], 0x0000, 1);
#endif

	context.triggerEvent(Gnm::kEventTypeCacheFlushAndInvEvent);

	// Wait for append job to complete
#ifdef USE_COMPUTE_QUEUE
	context.writeReleaseMemEvent(Gnm::kReleaseMemEventCsDone, Gnm::kEventWriteDestMemory, (void*)appendLabel, Gnm::kEventWriteSource32BitsImmediate,
		1, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
#else
	context.writeAtEndOfShader(Gnm::kEosCsDone, (void*)appendLabel , 0x1);
#endif
	context.waitOnAddress((void*)appendLabel, 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);

	// Read the element count of bufInter so we can verify it later
	context.readAppendConsumeCounters((void*)&bufInterCounts[0], 0x0000, 0, 1);
	
	// Pass 2: consume data
	context.setCsShader(csConsume);
#ifdef USE_COMPUTE_QUEUE
	context.setRwBuffers(0, 1, &bufInter); // Redundant, but listed for clarity
	context.setRwBuffers(1, 1, &bufOut);
	context.setAppendConsumeCounterRange(0, 4); // Redundant
#else
	context.setRwBuffers(Gnm::kShaderStageCs, 0, 1, &bufInter); // Redundant, but listed for clarity
	context.setRwBuffers(Gnm::kShaderStageCs, 1, 1, &bufOut);
	context.setAppendConsumeCounterRange(Gnm::kShaderStageCs, 0, 4); // Redundant
#endif
	context.dispatch(kNumThreadGroups, 1, 1);

	// Redundant read to test readDataFromGds()
	// Note: Due to a GPU bug, a call to readDataFromGds() must immediately follow a dispatch call if the former is used with Gnm::kEosCsDone as its argument.
#ifdef USE_COMPUTE_QUEUE
	context.readDataFromGds((void*)&bufInterCountsCopy[1], 0x0000, 1);
#else
	context.readDataFromGds(Gnm::kEosCsDone, (void*)&bufInterCountsCopy[1], 0x0000, 1);
#endif

	context.triggerEvent(Gnm::kEventTypeCacheFlushAndInvEvent);

	// Wait for consume job to complete
#ifdef USE_COMPUTE_QUEUE
	context.writeReleaseMemEvent(Gnm::kReleaseMemEventCsDone, Gnm::kEventWriteDestMemory, (void*)consumeLabel, Gnm::kEventWriteSource32BitsImmediate,
		1, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
#else
	context.writeAtEndOfShader(Gnm::kEosCsDone, (void*)consumeLabel , 0x1);
#endif
	context.waitOnAddress((void*)consumeLabel, 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);

	// Read the element count of bufInter so we can verify it later
	context.readAppendConsumeCounters((void*)&bufInterCounts[1], 0x0000, 0, 1);

	// Flush the L2$ and signal the CPU it's done:
#ifdef USE_COMPUTE_QUEUE
	context.flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, 0);
#else
	context.flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, 0, Gnm::kStallCommandBufferParserDisable);
#endif
	context.writeDataInline((void*)submitLabel, zero, 2, Gnm::kWriteDataConfirmEnable);

#ifdef USE_COMPUTE_QUEUE
	int32_t state = cq.submit(&context);
#else
	int32_t state = context.submit();
#endif
	SCE_GNM_ASSERT(state == sce::Gnm::kSubmissionSuccess);


	//
	// Wait for the execution to be complete:
	//
	uint32_t wait = 0;
	while (*submitLabel != 0)
		wait++;

	// Check results:
	// The first 64 elements of pDataInter should be the first 64 multiples of 2, but not necessarily in sorted order
	// (due to unpredictable wavefront scheduling).
	bool foundElem[kNumElements/2];
	memset(foundElem, 0, sizeof(foundElem));
	for(uint32_t iBufElem=0; iBufElem<kNumElements/2; ++iBufElem)
	{
		SCE_GNM_ASSERT_MSG(pDataInter[iBufElem] < kNumElements, "out of range value in pDataInter[%d]: 0x%08X", iBufElem, pDataInter[iBufElem]);
		SCE_GNM_ASSERT_MSG(pDataInter[iBufElem] % 2 == 0, "pDataInter[%d] is not a multiple of 2: 0x%08X", iBufElem, pDataInter[iBufElem]);
		SCE_GNM_ASSERT_MSG(foundElem[pDataInter[iBufElem]/2] == false, "Duplicate value found at pDataInter[%d]: 0x%08X", iBufElem, pDataInter[iBufElem]);
		foundElem[pDataInter[iBufElem]/2] = true;
	}
	// The odd-numbers elements of pDataOut should be 0xFFFFFFFF; the even-numbered elements are the same multiples of 2 from bufInter.
	memset(foundElem, 0, sizeof(foundElem));
	for(uint32_t iBufElem=0; iBufElem<kNumElements; ++iBufElem)
	{
		if (iBufElem % 2 == 0)
		{
			SCE_GNM_ASSERT_MSG(pDataOut[iBufElem] < kNumElements, "out of range value in pDataOut[%d]: 0x%08X", iBufElem, pDataOut[iBufElem]);
			SCE_GNM_ASSERT_MSG(pDataOut[iBufElem] % 2 == 0, "pDataOut[%d] is not a multiple of 2: 0x%08X", iBufElem, pDataOut[iBufElem]);
			SCE_GNM_ASSERT_MSG(foundElem[pDataOut[iBufElem]/2] == false, "Duplicate value found at pDataOut[%d]: 0x%08X", iBufElem, pDataOut[iBufElem]);
			foundElem[pDataOut[iBufElem]/2] = true;
		}
		else
		{
			SCE_GNM_ASSERT_MSG(pDataOut[iBufElem] == 0xFFFFFFFF, "Bad value at pDataOut[%d]: expected 0xFFFFFFFF, found 0x%08X", iBufElem, pDataOut[iBufElem]);
		}
	}

	printf("     %10s %10s %10s\n", "in:", "inter:", "out:\n");
	for (uint32_t i=0; i<kNumElements; ++i)
	{
		printf("%3d: 0x%08X 0x%08X 0x%08X\n", i, pDataIn[i], pDataInter[i], pDataOut[i]);
	}
	printf("bufInter element count after appends:  %3d\n", bufInterCounts[0]);
	printf("bufInter element count after consumes: %3d\n", bufInterCounts[1]);
	SCE_GNM_ASSERT_MSG(bufInterCounts[0] == bufInterCountsCopy[0], "dmaData() from GDS gave different results from readFromGds()!");
	SCE_GNM_ASSERT_MSG(bufInterCounts[1] == bufInterCountsCopy[1], "dmaData() from GDS gave different results from readFromGds()!");
	printf("Test completed successfully! [assuming validation is enabled...]\n");
}


int main()
{
	//
	// Initializes system and video memory and memory allocators.
	//
	SimpletUtil::Init();

	{
		Test();
	}

	return 0;
}
