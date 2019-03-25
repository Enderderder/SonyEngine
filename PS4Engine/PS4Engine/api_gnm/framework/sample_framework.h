/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_SAMPLE_FRAMEWORK_H_
#define _SCE_GNM_FRAMEWORK_SAMPLE_FRAMEWORK_H_

#include <gnmx.h>
#include "../dbg_font/dbg_font.h"
#include "../toolkit/stack_allocator.h"
#include "controller.h"
#include "framework.h"
#include "sample_framework_menu.h"
#include "simple_mesh.h"
#include "thread.h"
#include "eopeventqueue.h"
#include "debug_objects.h"
#include "configdata.h"

namespace Framework
{
	enum WhoQueuesFlips
	{
		kCpuMainThread,
		kGpuEop,
		kCpuWorkerThread
	};

	class GnmSampleFramework
	{
		double m_secondsElapsedActualPerFrame;
		double m_secondsElapsedApparentPerFrame;

		sce::DbgFont::Screen::HardwareCursors m_hardwareCursors;

		uint32_t m_line;
		uint32_t m_reserved0;
		const char *m_windowTitle;
		const char *m_overview;
		const char *m_explanation;
		enum {kMaxPath = 512};
		char m_szExePath[kMaxPath];
		double   m_secondsElapsedActual;
		uint64_t m_lastClock;
		void DisplayEnum(const char *word[], uint32_t words, uint32_t value, const MenuItem* menuItem);
		MenuCommand* m_menuCommand;

		class FlipThread : public Thread
		{
		public:
			FlipThread(GnmSampleFramework *framework);
			SCE_GNM_API_DEPRECATED("These APIs are no longer supported.")
			void addPendingEvent() {}
		private:
			GnmSampleFramework *m_framework;
			virtual int32_t callback();
		};
		FlipThread m_flipThread;
		int32_t flipThreadCallback();

		friend class FlipThread;
		int32_t flipToBuffer(uint32_t bufferIndex);

		sce::Gnmx::Toolkit::StackAllocator m_garlicStackAllocator;
		sce::Gnmx::Toolkit::StackAllocator m_onionStackAllocator;
	public:
		// The cell to clear the screen with, for each of the following display buffer modes:
		// 0: kFrameworkDisplayBufferColor,
		// 1: kFrameworkDisplayBufferAlpha,
		// 2: kFrameworkDisplayBufferDepth,
		// 3: kFrameworkDisplayBufferStencil,
		static sce::DbgFont::Cell s_clearCell[4];

		sce::Gnmx::Toolkit::IAllocator m_garlicAllocator; // allocator for memory that is fastest for GPU
		sce::Gnmx::Toolkit::IAllocator m_onionAllocator; // allocator for memory that is for sharing between CPU and GPU
		sce::Gnmx::Toolkit::Allocators m_allocators; // contains references to the onion and garlic allocators.
		SCE_GNM_API_DEPRECATED_VAR_MSG("don't use this. use m_garlicAllocator.")
		sce::Gnmx::Toolkit::IAllocator m_allocator; // don't use this. use m_garlicAllocator.
		void print(FILE* file);
		bool m_menuIsEnabled;
		uint8_t m_reserved1[7];
		double actualSecondsPerFrame() const { return m_secondsElapsedActualPerFrame; }
		double apparentSecondsPerFrame() const { return m_secondsElapsedApparentPerFrame; }
		MenuHost m_menuHost;
		void removeAllMenuItems();
		void restoreDefaultMenuItems();
		void appendMenuItems(                uint32_t count, const MenuItem* menuItem);
		void insertMenuItems(uint32_t index, uint32_t count, const MenuItem* menuItem);
		void removeMenuItems(uint32_t index, uint32_t count                          );

		double   m_secondsElapsedApparent;
		bool m_paused;
		uint8_t m_reserved2[7];
		Input::ControllerContext m_controllerContext;

		GnmSampleFramework();
		~GnmSampleFramework();

		int32_t processCommandLine(int argc, const char* argv[]);
		int32_t initialize(const char* windowTitle, const char* overview, const char* explanation);
		int32_t initialize(const char* windowTitle, const char* overview, const char* explanation, int argc, const char *argv[]);
		void terminate(sce::Gnmx::GnmxGfxContext& gfxc);
	
		void displayTimer(sce::Gnmx::GnmxGfxContext &gfxc, uint32_t x, uint32_t y, void* timer, uint32_t characters, uint32_t decimal, float factor);
		void displayUnsigned(sce::Gnmx::GnmxGfxContext &gfxc,uint32_t x,uint32_t y,void* timer,uint32_t characters);

		void forceRedraw();
		void BeginFrame(sce::Gnmx::GnmxGfxContext& gfxc);
		bool m_alreadyPreparedHud;
		void PrepareHud(sce::Gnmx::GnmxGfxContext& gfxc);
		void RenderHud(sce::Gnmx::GnmxGfxContext& gfxc, int dbgFontHorizontalPixelOffset, int dbgFontVerticalPixelOffset);
		void EndFrame(sce::Gnmx::GnmxGfxContext& gfxc);

		void AppendEndFramePackets(sce::Gnmx::GnmxGfxContext& gfxc);
		void SubmitCommandBufferOnly(sce::Gnmx::GnmxGfxContext& gfxc);
		void SubmitCommandBuffer(sce::Gnmx::GnmxGfxContext& gfxc);
		void WaitForBufferAndFlip();
		void StallUntilGpuIsIdle();
		uint32_t GetVideoHandle();
		SceKernelEqueue GetEopEventQueue();

		enum 
		{ 
			kFrameTimer = 0,
			kTimers = 1,
		};

		struct Buffer
		{
			sce::Gnm::RenderTarget       m_renderTarget;
			sce::Gnm::DepthRenderTarget  m_depthTarget;
			uint8_t						 m_reserved0[4];
			sce::DbgFont::Screen         m_screen;
			sce::DbgFont::Window         m_window;
			volatile uint64_t           *m_label;
			volatile uint64_t           *m_labelForPrepareFlip;
			uint32_t                     m_expectedLabel;
			uint32_t                     m_reserved;
			sce::Gnmx::Toolkit::Timers   m_timers;
			DebugObjects                 m_debugObjects;
		};

		uint8_t m_reserved3[7];
		Buffer* m_buffer;
		Buffer* m_backBuffer;
	private:
		EopEventQueue m_eopEventQueue;

		struct VideoInfo
		{
			int32_t handle;
			uint32_t flip_index_base;
			uint32_t buffer_num;
		};
		VideoInfo m_videoInfo;
	public:
		uint32_t m_backBufferIndex;

		struct CpuTimer
		{
			uint64_t m_begin;
			uint64_t m_end;
			uint64_t m_clocks;
			CpuTimer();
			void stop();
			void start();
			uint64_t clocks() const;
			double seconds() const;
		};
		CpuTimer m_cpuTimerIncludingFramework;
		CpuTimer m_cpuTimerExcludingFramework;

		uint32_t          m_shutdownRequested;
		uint32_t          m_frameCount;

		uint32_t	      m_reserved4[2];
		ConfigData        m_config;

		uintptr_t         m_windowHandle;
		uint32_t          m_dcpId;

		bool m_renderedDebugObjectsAlreadyInThisFrame;
		void RenderDebugObjects(sce::Gnmx::GnmxGfxContext& gfxc);

		void WaitUntilBufferFinishedRendering(Buffer* buffer);
		void WaitUntilGPUIsFinishedWithBuffer(Buffer* buffer);

		float GetSecondsElapsedApparent() const;
		uint32_t GetFrame() const { return m_frameCount; }

		void UpdateMatrices();
		void UpdateViewToWorldMatrix(const Matrix4&);

		void SetProjectionMatrix( const Matrix4& );
		void SetViewToWorldMatrix( const Matrix4& );
		void SetDepthNear( float value );
		void SetDepthFar( float value );

		float GetDepthNear() const;
		float GetDepthFar() const;

		struct Plane
		{
			Vector4 m_equation;
			Vector4 m_vertex[4];
		};

		uint8_t m_reserved5[3];
		enum {kPlanes = 6};
		Plane m_plane[kPlanes];
		enum {kCorners = 8};
		Vector4 m_clipCorner[kCorners];

		Matrix4 m_worldToLightMatrix;
		Matrix4 m_lightToWorldMatrix;
		Matrix4 m_projectionMatrix;
		Matrix4 m_worldToViewMatrix;
		Matrix4 m_viewToWorldMatrix;
		Matrix4 m_viewProjectionMatrix;
		Matrix4 m_originalViewMatrix;
		float m_depthNear;
		float m_depthFar;
		void (*m_menuAdjustmentValidator)();

		void RequestTermination();

#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
		void RequestThreadTrace();
#endif //#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE

		Vector4 getCameraPositionInWorldSpace() const;
		Vector4 getLightPositionInWorldSpace() const;
		Vector4 getLightPositionInViewSpace() const;
		Vector4 getClearColor() const;
		Vector4 getLightColor() const;
		Vector4 getAmbientColor() const;

		// DEPRECATED STUFF BELOW -------------------------------------------

		// DEPRECATED STUFF ABOVE -------------------------------------------

		uint32_t getIndexOfLastBufferCpuWrote() const {return m_indexOfLastBufferCpuWrote;}
		uint32_t getIndexOfBufferCpuIsWriting() const {return m_indexOfBufferCpuIsWriting;}

	private:
		void StallUntilGPUIsNotUsingBuffer(EopEventQueue *eopEventQueue, uint32_t bufferIndex);
		void advanceCpuToNextBuffer();

		Mesh m_sphereMesh;
		Mesh m_coneMesh;
		Mesh m_cylinderMesh;

		void DisplayKeyboardOptions();
		void ExecuteKeyboardOptions();

		volatile uint64_t *m_label;
		volatile uint64_t *m_labelForPrepareFlip;

		SCE_GNM_API_DEPRECATED_MSG("please use the version of TakeScreenshot that takes only 'path' as an argument")
		void TakeScreenshot(unsigned number, sce::Gnm::Texture *texture);
		void TakeScreenshot(const char *path);

#ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
		bool m_traceFrame = false;
#endif // #ifdef ENABLE_RAZOR_GPU_THREAD_TRACE
		uint8_t m_reserved6[3];
		uint32_t m_indexOfLastBufferCpuWrote;
		uint32_t m_indexOfBufferCpuIsWriting;
	};

	void createCommandBuffer(sce::Gnmx::GnmxGfxContext *commandBuffer,GnmSampleFramework *framework,unsigned buffer);
}

#endif // _SCE_GNM_FRAMEWORK_SAMPLE_FRAMEWORK_H_
