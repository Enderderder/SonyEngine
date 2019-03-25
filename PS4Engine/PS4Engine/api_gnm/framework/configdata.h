/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_CONFIGDATA_H_
#define _SCE_GNM_FRAMEWORK_CONFIGDATA_H_

#include "../toolkit/geommath/geommath.h"

namespace Framework
{
	class ConfigData
	{
	public:
		Vector3 m_lookAtPosition;
		Vector3 m_lookAtTarget;
		Vector3 m_lookAtUp;
		Vector3 m_lightPositionX;
		Vector3 m_lightTargetX;
		Vector3 m_lightUpX;
		const char *m_screenshotFilename;
		const char *m_menuFilename;
        const char *m_renderingMode;
		uint32_t m_targetWidth;
		uint32_t m_targetHeight;
		uint32_t m_frames;
		float    m_seconds;
		uint32_t m_buffers;
		uint32_t m_microSecondsPerFrame;
		uint32_t m_displayBuffer;
		uint32_t m_lightColor;
		uint32_t m_ambientColor;
		uint32_t m_clearColor;
		uint32_t m_log2CharacterWidth;
		uint32_t m_log2CharacterHeight;
		uint32_t m_onionMemoryInBytes;
		uint32_t m_garlicMemoryInBytes;
		uint32_t m_whoQueuesFlips;
		uint32_t m_flipMode;
        uint32_t m_cameraControlMode;
		bool     m_cameraControls;
		bool     m_fullScreen;
        bool     m_dcc;
		bool	 m_stencil;
		bool	 m_htile;
		bool	 m_screenshot;
		bool	 m_depthBufferIsMeaningful;
		bool	 m_displayTimings;
		bool	 m_lightingIsMeaningful;
		bool	 m_clearColorIsMeaningful;
		bool	 m_secondsAreMeaningful;
		bool	 m_asynchronous;
		bool	 m_renderText;
		bool	 m_printMenus;
		bool	 m_dumpPackets;
		bool	 m_displayDebugObjects;
		bool	 m_enableInput;
		bool	 m_enableTouchCursors;
        bool     m_interruptOnFlip;
		bool	 m_userProvidedSubmitDone;
		//uint8_t  m_reserved0[0];
	};
}

#endif // _SCE_GNM_FRAMEWORK_CONFIGDATA_H_
