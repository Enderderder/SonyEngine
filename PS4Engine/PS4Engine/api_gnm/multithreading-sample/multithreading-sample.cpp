/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <kernel.h>
#include <sceerror.h>
#include <assert.h>
#include <x86intrin.h>
#include <pm4_dump.h>
#include "../framework/sample_framework.h"
#include "../framework/simple_mesh.h"
#include "../framework/gnf_loader.h"
#include "../framework/thread.h"
#include "std_cbuffer.h"
using namespace sce;


const int kThreadsPerFrame = 4;
const int kNumBuffers = 3;
const int kNumThreads = kThreadsPerFrame*kNumBuffers;

const int kNumHeaps = 2;


enum ThreadRequest
{
    kRequestIdle		= 1,
    kRequestWork		= 2
};

struct ThreadGlobals 
{
    Framework::GnmSampleFramework framework;
    Gnmx::VsShader * vertexShader;
	void *fetchShader;
	Gnmx::PsShader * pixelShader;

	Gnmx::InputOffsetsCache vertexShader_offsetsTable;
	Gnmx::InputOffsetsCache pixelShader_offsetsTable;

	
    Gnm::Texture textures[2];
    Gnm::Sampler sampler;

	uint8_t pad[8];
};

class ThreadLocals : public Framework::Thread
{
public:
	ThreadLocals()
	: Thread("renderThread")
	{
	}
	void join()
	{
		stop();
		int err = sceKernelSetEventFlag(event_flag,kRequestWork);
		SCE_GNM_UNUSED(err);
		Thread::join();
	}

                                    // Field access from:
                                    // Master thread    | Slave thread
	Matrix4 worldToViewMatrix;      //  W               |  R
	Matrix4 viewProjectionMatrix;   //  W               |  R
	Vector4 clearColor;             //  W               |  R
    Gnmx::GnmxGfxContext* gfxc;     //  none            |  R
    Framework::SimpleMesh* mesh;    //  none            |  R
    Gnm::RenderTarget* target;      //  none            |  R
    Gnm::DepthRenderTarget *zbuffer;//  none            |  R
    ThreadGlobals* globals;         //  none            |  R
	SceKernelEventFlag event_flag;	
	float secondsElapsed;           //  W               |  R
    unsigned thread_id;             //  R               |  R
    unsigned heap_id;               //  none            |  R
	unsigned flip_index;			//	none			|  R
    bool clear_screen;              //  none            |  R

	uint8_t pad[15];
    
private:
	virtual int32_t callback();
};

struct Frame
{
    Gnmx::GnmxGfxContext *contexts[kThreadsPerFrame];
    ThreadLocals *threads[kThreadsPerFrame];
    volatile uint64_t* labels[kThreadsPerFrame];
    Gnm::RenderTarget* target;
};

void fillCommandBuffer( ThreadLocals *descr )
{
    ThreadGlobals &globals = *descr->globals;

    descr->gfxc->reset();
	if(descr->clear_screen) 
	{
		// only the first packet in the frame can init hardware
		descr->gfxc->waitUntilSafeForRendering(globals.framework.GetVideoHandle(), descr->flip_index);
	}

	
    descr->gfxc->setDepthClearValue(1.f);
    Gnm::PrimitiveSetup primSetupReg;
	primSetupReg.init();
    primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
    primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
    descr->gfxc->setPrimitiveSetup(primSetupReg);
    if(descr->clear_screen)
    {
        // Clear
        Gnmx::Toolkit::SurfaceUtil::clearRenderTarget(*descr->gfxc, descr->target, descr->clearColor);
        Gnmx::Toolkit::SurfaceUtil::clearDepthTarget(*descr->gfxc, descr->zbuffer, 1.f);
    }

    descr->gfxc->setRenderTargetMask(0xF);

    descr->gfxc->setRenderTarget(0, descr->target);
    descr->gfxc->setDepthRenderTarget(descr->zbuffer);

    Gnm::DepthStencilControl dsc;
	dsc.init();
    dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLessEqual);
    dsc.setDepthEnable(true);
    descr->gfxc->setDepthStencilControl(dsc);
    descr->gfxc->setupScreenViewport(0, 0, descr->target->getWidth(), descr->target->getHeight(), 0.5f, 0.5f);

	descr->gfxc->setVsShader(globals.vertexShader, 0, globals.fetchShader, &globals.vertexShader_offsetsTable);
	descr->gfxc->setPsShader(globals.pixelShader, &globals.pixelShader_offsetsTable);

    descr->mesh->SetVertexBuffer( *descr->gfxc, Gnm::kShaderStageVs );

    cbSphereShaderConstants *sphereConst = static_cast<cbSphereShaderConstants*>( descr->gfxc->allocateFromCommandBuffer( sizeof(cbSphereShaderConstants), Gnm::kEmbeddedDataAlignment4 ) );
    Gnm::Buffer sphereConstBuffer;
    sphereConstBuffer.initAsConstantBuffer(sphereConst, sizeof(*sphereConst));
	sphereConstBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // it's a constant buffer, so read-only is OK

    descr->gfxc->setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &sphereConstBuffer);
    descr->gfxc->setConstantBuffers(Gnm::kShaderStagePs, 0, 1, &sphereConstBuffer);

    descr->gfxc->setTextures(Gnm::kShaderStagePs, 0, 2, globals.textures);
    descr->gfxc->setSamplers(Gnm::kShaderStagePs, 0, 1, &globals.sampler);

    const float radians = descr->secondsElapsed; 

    const Matrix4 m = Matrix4::translation(Vector3(0,(float)(descr->thread_id%kThreadsPerFrame)/kThreadsPerFrame,0.f))*Matrix4::rotationZYX(Vector3(radians,radians,0.f));

    sphereConst->m_modelView = transpose(descr->worldToViewMatrix*m);
    sphereConst->m_modelViewProjection = transpose(descr->viewProjectionMatrix*m);

    sphereConst->m_lightPosition = descr->worldToViewMatrix * Point3(-1.5f,4,9);

    descr->gfxc->setPrimitiveType(descr->mesh->m_primitiveType);
    descr->gfxc->setIndexSize(descr->mesh->m_indexType);
	
	descr->gfxc->drawIndex(descr->mesh->m_indexCount, descr->mesh->m_indexBuffer);
}

int32_t ThreadLocals::callback()
{
	while(isRunning())
	{
		sceKernelWaitEventFlag(event_flag,kRequestWork,SCE_KERNEL_EVF_WAITMODE_AND | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL,NULL);

		fillCommandBuffer(this);

		//////////////////////////////////////////////////////////////////////////
		//
		//  _mm_mfence() is very important here.
		// Because of the interaction between caches and read/write queues in the CPU the read/write results issued on one CPU core 
		// may appear in different orders as observed on another core. 
		//  _mm_mfence() ensures that the read and write queues are flushed and also prevents the compiler from reordering instructions around itself.
		// Without this barrier it's possible that the thread's status will be updated before the command buffer and the other thread may execute an uninitialized/stale command buffer.
		//

		_mm_mfence();
		sceKernelSetEventFlag(event_flag,kRequestIdle);
	}
	return 0;
}

int main(int argc, const char *argv[])
{
    ThreadGlobals globals;
	globals.framework.processCommandLine(argc, argv);
	globals.framework.m_config.m_htile = false;
    globals.framework.initialize( "Multithreading", 
        "Sample code to demonstrate rendering objects with multiple threads.",
		"This sample shows how to use multiple CPU threads for drawing toruses of the basic-sample.<br><br>"
		"In detail, it first generates multiple GfxContext instances by using multiple CPU threads.  Then a manager thread performs multiple submits of all the instances.  This demonstrates an efficient use of multiple CPU cores.");

	enum {kCommandBufferSizeInBytes = 1 * 1024 * 1024};

    //
    // We only need two CUE heaps for all our contexts because the command buffer hardware cannot process more than two Gnm::submitCommandBuffers()s simultaneously.
	// Note that it does not matter how many individual buffers are supplied with the Gnm::submitCommandBuffers() it's the Gnm::submitCommandBuffers() itself that matters.
	// The GPU commands in the third Gnm::submitCommandBuffers() will always wait until the first one has finished executing.
	// The DCBs and CCBs within one Gnm::submitCommandBuffers() do not do that. If there are three DrawCommandBuffers d0, d1 and d2 and 
	// you submit them all together inside a single Gnm::submitCommandBuffers() call then it's possible that the commands in d2 
	// will begin executing while commands in d0 still have not finished.
	// If there are three GfxContexts a, b and c and we call a.submit(); b.submit(); c.submit(); the commands in the context c are guaranteed to execute only 
	// when all the commands in the context a have already finished (because the GfxContext::submit() calls Gnm::submitCommandBuffers() and there cannot be more than two of these in flight)
    // 
    // This allows us to use just two heaps for the multiple graphics contexts.
    // Here is how the command buffers in this sample are executed:
    // 
    // Frame 0 : Thread 0   (Heap 0)
    // Frame 0 : Thread 1   (Heap 1)
	// Frame 0 : Thread 2	(Heap 0)
    // etc
    // Frame 0 : Thread n   (Heap n%2)
    // >>>>>>>>>>>> flip
    // Frame 1 : Thread k   (Heap 0)
    // Frame 1 : Thread k+1 (Heap 1)
    // etc
    // >>>>>>>>>>>> sync gpu
    // Frame 2 : Thread 0 (Heap 0)
    // etc
	// See how the threads are alternating contexts that use a different heap but there are only two heaps in total used for any number of threads.

	Gnmx::Toolkit::IAllocator *allocator = &globals.framework.m_garlicAllocator;

#if  !SCE_GNMX_ENABLE_GFX_LCUE
	const uint32_t kNumRingEntries = 64;
	const uint32_t cueHeapSize = Gnmx::ConstantUpdateEngine::computeHeapSize(kNumRingEntries);

    void *heapAddrs[kNumHeaps];
    for( unsigned i=0; i!=kNumHeaps; ++i)
    {
        heapAddrs[i] = allocator->allocate(cueHeapSize, Gnm::kAlignmentOfBufferInBytes);
    }
#endif

    // Create a contexts for each thread, share two heapAddrs between them.
    Gnmx::GnmxGfxContext contexts[kNumThreads];

    for( unsigned i=0; i!=kNumThreads; ++i)
    {
#if  SCE_GNMX_ENABLE_GFX_LCUE
		const uint32_t kNumBatches = 100; // Estimated number of batches to support within the resource buffer
		const uint32_t resourceBufferSize = Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(Gnmx::LightweightConstantUpdateEngine::kResourceBufferGraphics, kNumBatches);
		contexts[i].init
			(		
			globals.framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes), kCommandBufferSizeInBytes,		// Draw command buffer
			globals.framework.m_garlicAllocator.allocate(resourceBufferSize, Gnm::kAlignmentOfBufferInBytes), resourceBufferSize,					// Resource buffer
			NULL																																	// Global resource table 
			);
#else
		contexts[i].init(heapAddrs[i%kNumHeaps], kNumRingEntries,		// select one of the two heapAddrs for the constant update engine alternatively
			globals.framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes), kCommandBufferSizeInBytes,		// Draw command buffer
			globals.framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes), kCommandBufferSizeInBytes		// Constant command buffer
			);
#endif
    }

	Gnmx::GnmxGfxContext *commandBuffers = new Gnmx::GnmxGfxContext[globals.framework.m_config.m_buffers];

	for(uint32_t bufferIndex = 0; bufferIndex < globals.framework.m_config.m_buffers; ++bufferIndex)
	{
#if  SCE_GNMX_ENABLE_GFX_LCUE
		const uint32_t kNumBatches = 100; // Estimated number of batches to support within the resource buffer
		const uint32_t resourceBufferSize = Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(Gnmx::LightweightConstantUpdateEngine::kResourceBufferGraphics, kNumBatches);
		Gnmx::GnmxGfxContext *gfxc = &commandBuffers[bufferIndex];
		gfxc->init(globals.framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes), kCommandBufferSizeInBytes,	// Draw command buffer
			globals.framework.m_garlicAllocator.allocate(resourceBufferSize, Gnm::kAlignmentOfBufferInBytes), resourceBufferSize,						// Resource buffer
			NULL																																		// Global resource table 
			);
#else
		Gnmx::GnmxGfxContext *gfxc = &commandBuffers[bufferIndex];
		gfxc->init(globals.framework.m_garlicAllocator.allocate(cueHeapSize, Gnm::kAlignmentOfBufferInBytes), kNumRingEntries,					// Constant Update Engine
			globals.framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes), kCommandBufferSizeInBytes,	// Draw command buffer
			globals.framework.m_onionAllocator.allocate(kCommandBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes), kCommandBufferSizeInBytes	// Constant command buffer
			);
#endif
	}

    const unsigned kEventSize = (unsigned)sizeof(uint64_t);
    volatile char *labels = (volatile char*)allocator->allocate(kEventSize*kNumThreads, sizeof(uint64_t));
    for(unsigned i=0; i!=kNumThreads; ++i)
    {
        volatile uint64_t* label = reinterpret_cast<volatile uint64_t*>(labels + i*kEventSize);
        *label = 0xdeadbeef;
    }

    globals.vertexShader = Framework::LoadVsMakeFetchShader(&globals.fetchShader, "/app0/multithreading-sample/shader_vv.sb", &globals.framework.m_allocators);
	Gnmx::generateInputOffsetsCache(&globals.vertexShader_offsetsTable, Gnm::kShaderStageVs, globals.vertexShader);

    globals.pixelShader = Framework::LoadPsShader("/app0/multithreading-sample/shader_p.sb", &globals.framework.m_allocators);
	Gnmx::generateInputOffsetsCache(&globals.pixelShader_offsetsTable, Gnm::kShaderStagePs, globals.pixelShader);

    Framework::GnfError loadError = Framework::kGnfErrorNone;
    loadError = Framework::loadTextureFromGnf(&globals.textures[0], "/app0/assets/icelogo-color.gnf", 0, &globals.framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );
    loadError = Framework::loadTextureFromGnf(&globals.textures[1], "/app0/assets/icelogo-normal.gnf", 0, &globals.framework.m_allocators);
    SCE_GNM_ASSERT(loadError == Framework::kGnfErrorNone );

	globals.textures[0].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as RWTexture, so it's safe to declare read-only.
	globals.textures[1].setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // this texture is never bound as RWTexture, so it's safe to declare read-only.

    globals.sampler.init();
    globals.sampler.setMipFilterMode(Gnm::kMipFilterModeLinear);
    globals.sampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);


    Framework::SimpleMesh simpleMesh;
    Framework::BuildTorusMesh(allocator, &simpleMesh, 0.8f*0.2f, 0.2f*0.2f, 32, 16, 4, 1);

    //
    // Initialize threads
    //

    ThreadLocals threads[kNumThreads];
    for(unsigned i = 0; i!=kNumThreads; ++i) {
        const unsigned heapId = i%kNumHeaps;
        ThreadLocals &thread = threads[i];

        thread.globals = &globals;
        thread.gfxc = &contexts[i];
        thread.mesh = &simpleMesh;
        thread.heap_id = heapId;
        thread.clear_screen = false;
        thread.thread_id = i;

		sceKernelCreateEventFlag(&thread.event_flag,"event",SCE_KERNEL_EVF_ATTR_MULTI,kRequestWork,NULL);
    }
    // Each Frame structure contains threads and other structures needed to render a frame
    // We have a structure per a back buffer so we can flip between these frames instead of changing some back buffer pointers
    Frame frames[kNumBuffers];
    for( int f=0; f != kNumBuffers; ++f)
    {
        Frame& frame = frames[f];

        Gnm::RenderTarget *target = &globals.framework.m_buffer[f].m_renderTarget;
		Gnm::DepthRenderTarget *zbuffer = &globals.framework.m_buffer[f].m_depthTarget;

        for( int t = 0; t!=kThreadsPerFrame; ++t)
        {
            unsigned threadId = f*kThreadsPerFrame + t;
            ThreadLocals & thread = threads[ threadId ];
            frame.contexts[t] = &contexts[threadId];
            frame.target = target;

            thread.target = target;
			thread.zbuffer = zbuffer;
			thread.flip_index = f;
            thread.clear_screen = t == 0;
			thread.viewProjectionMatrix = globals.framework.m_viewProjectionMatrix;
			thread.worldToViewMatrix = globals.framework.m_worldToViewMatrix;
			thread.secondsElapsed = globals.framework.GetSecondsElapsedApparent();
			thread.clearColor = globals.framework.getClearColor();
			
            frame.threads[t] = &thread;
            frame.labels[t] = reinterpret_cast<volatile uint64_t*>( labels + kEventSize*threadId );
			_mm_mfence();
			thread.run();
        }
    }
    
    //
    // Main Loop:
    //
    while (!globals.framework.m_shutdownRequested)
    {
		const unsigned currentFrame = globals.framework.m_backBufferIndex;
		Gnmx::GnmxGfxContext &uic = commandBuffers[currentFrame]; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.

        uic.reset();
        globals.framework.BeginFrame(uic);

        Frame& frame = frames[currentFrame % kNumBuffers];

        for( int t=0; t!=kThreadsPerFrame; ++t)
        {
            ThreadLocals *thread = frame.threads[t];
            // Wait for thread to finish creating the command buffer
			sceKernelWaitEventFlag(thread->event_flag,kRequestIdle,SCE_KERNEL_EVF_WAITMODE_AND | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL,NULL);

			Gnmx::GnmxGfxContext *gfxc = frame.contexts[t];
            gfxc->writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void *)frame.labels[t], currentFrame, Gnm::kCacheActionNone);
            
           gfxc->submit();
        }

        for(int t=0; t!=kThreadsPerFrame; ++t)
        {
            unsigned wait = 0;
            while(*frame.labels[t] != currentFrame)
            {
                scePthreadYield();
                ++wait; // spin till the submission has completed
            }
			ThreadLocals *thread = frame.threads[t];
            
			thread->viewProjectionMatrix = globals.framework.m_viewProjectionMatrix;
			thread->worldToViewMatrix = globals.framework.m_worldToViewMatrix;
			thread->secondsElapsed = globals.framework.GetSecondsElapsedApparent();
			thread->clearColor = globals.framework.getClearColor();
			sceKernelSetEventFlag(thread->event_flag,kRequestWork);
        }
        
        globals.framework.EndFrame(uic);          
    }

    for(unsigned i=0; i!=kNumThreads; ++i)
    {
        ThreadLocals &thread = threads[i];
		thread.join();
    }
	const unsigned currentFrame = globals.framework.m_backBufferIndex;
	Gnmx::GnmxGfxContext &uic = commandBuffers[currentFrame]; // one GfxContext (command buffer) object is required for each frame that can simultaneously be in flight.
    globals.framework.terminate(uic);
    return 0;
}
