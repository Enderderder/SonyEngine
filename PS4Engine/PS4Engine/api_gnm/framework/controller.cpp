/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "controller.h"
#include <string>
#include <sceerror.h>
#include <pad.h>

#include <user_service.h>
#pragma comment(lib,"libSceUserService_stub_weak.a")

using namespace sce;

const float Framework::Input::ControllerContext::m_defaultDeadZone 	= 0.25;
const float Framework::Input::ControllerContext::m_recipMaxByteAsFloat	= 1.0f / 255.0f;

Framework::Input::ControllerContext::ControllerContext(void)
{

}

Framework::Input::ControllerContext::~ControllerContext(void)
{

}

int Framework::Input::ControllerContext::initialize()
{

	for(uint32_t button = 0; button < 16; ++button)
		m_previousSeconds[button] = 0;
	m_initialSecondsUntilNextRepeat = 0.25;
	m_finalSecondsUntilNextRepeat = 0.05;
	for(uint32_t button = 0; button < 16; ++button)
		m_secondsUntilNextRepeat[button] = m_initialSecondsUntilNextRepeat;

	m_deadZone = m_defaultDeadZone;

	memset(&m_temporaryPadData, 0, sizeof(m_temporaryPadData));
	m_temporaryPadData.leftStick.x = 128;
	m_temporaryPadData.leftStick.y = 128;
	m_temporaryPadData.rightStick.x = 128;
	m_temporaryPadData.rightStick.y = 128;

	memset(&m_currentPadData, 0x0, sizeof(Data));
	m_currentPadData.leftStick.x = 128;
	m_currentPadData.leftStick.y = 128;
	m_currentPadData.rightStick.x = 128;
	m_currentPadData.rightStick.y = 128;

	m_leftStickXY.setX(0.0f);
	m_leftStickXY.setY(0.0f);
	m_rightStickXY.setX(0.0f);
	m_rightStickXY.setY(0.0f);

	m_dummyStickXY.setX(0.0f);
	m_dummyStickXY.setY(0.0f);

	int ret = sceUserServiceInitialize(NULL);
	if(ret < 0){
		return ret;
	}

	SceUserServiceUserId initialUserId;
	ret = sceUserServiceGetInitialUser(&initialUserId);
	if(ret < 0)
	{
		return ret;
	}

	ret = scePadInit();
	if(ret < 0)
	{
		return ret;
	}

	m_handle = scePadOpen(initialUserId, SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);
	if(m_handle < 0) 
	{
		return m_handle;
	}
	
	ret = scePadSetMotionSensorState(m_handle, true);
	if(ret < 0)
	{
		return ret;
	}

	ScePadControllerInformation info;
	ret = scePadGetControllerInformation(m_handle, &info);
	if(ret < 0){
		return ret;
	}
	m_resolution.x = info.touchPadInfo.resolution.x;
	m_resolution.y = info.touchPadInfo.resolution.y;

	return SCE_OK;
}

int Framework::Input::ControllerContext::finalize()
{
	int ret = scePadClose(m_handle);
	if(ret < 0){
		return ret;
	}

	ret = sceUserServiceTerminate();

	return ret;
}

void Framework::Input::ControllerContext::update(double seconds)
{
	Data m_previousPadData;

	memcpy(&m_previousPadData, &m_currentPadData, sizeof(m_previousPadData));

	for(uint32_t button = 0; button < 16; ++button)
	{
		if(seconds - m_previousSeconds[button] > m_secondsUntilNextRepeat[button])
		{
			m_previousPadData.buttons &= ~(1<<button);
			m_previousSeconds[button] = seconds;
			m_secondsUntilNextRepeat[button] = m_secondsUntilNextRepeat[button] * 0.75 + m_finalSecondsUntilNextRepeat * 0.25;
		}
	}

	memset(&m_currentPadData, 0x0, sizeof(Data));
	updatePadData();

	for(uint32_t button = 0; button < 16; ++button)
	{
		if((m_currentPadData.buttons & (1<<button)) == 0)
		{
			m_previousSeconds[button] = seconds;
			m_secondsUntilNextRepeat[button] = m_initialSecondsUntilNextRepeat;
		}
	}

	float lX=0.0f, lY=0.0f, rX=0.0f, rY=0.0f;

	// ascertain button pressed / released events from the current & previous state
	m_pressedButtonData = (m_currentPadData.buttons & ~m_previousPadData.buttons);		///< pressed button event data
	m_releasedButtonData = (~m_currentPadData.buttons & m_previousPadData.buttons);		///< released button event data

	// Get the analogue stick values
	// Remap ranges from 0-255 to -1 > +1
	lX = (float)((int32_t)m_currentPadData.leftStick.x * 2 -256) * m_recipMaxByteAsFloat;
	lY = (float)((int32_t)m_currentPadData.leftStick.y * 2 -256) * m_recipMaxByteAsFloat;
	rX = (float)((int32_t)m_currentPadData.rightStick.x * 2 -256) * m_recipMaxByteAsFloat;
	rY = (float)((int32_t)m_currentPadData.rightStick.y * 2 -256) * m_recipMaxByteAsFloat;

	// store stick values adjusting for deadzone
	m_leftStickXY.setX((fabsf(lX) < m_deadZone) ? 0.0f : lX);
	m_leftStickXY.setY((fabsf(lY) < m_deadZone) ? 0.0f : lY);
	m_rightStickXY.setX((fabsf(rX) < m_deadZone) ? 0.0f : rX);
	m_rightStickXY.setY((fabsf(rY) < m_deadZone) ? 0.0f : rY);
}

bool Framework::Input::ControllerContext::isButtonDown(Button buttons, ButtonEventPattern pattern) const
{
	if (pattern == PATTERN_ANY)
	{
		if ((m_currentPadData.buttons & buttons) != 0)
		{	
			return true;
		} else {
			return false;
		}
	} else if (pattern == PATTERN_ALL) {
		if ((m_currentPadData.buttons & buttons) == buttons)
		{
			return true;
		} else {
			return false;
		}
	}
	return false;
}


bool Framework::Input::ControllerContext::isButtonUp(Button buttons, ButtonEventPattern pattern) const
{
	if (pattern == PATTERN_ANY)
	{
		if ((m_currentPadData.buttons & buttons) != 0)
		{	
			return false;
		} else {
			return true;
		}
	} else if (pattern == PATTERN_ALL) {
		if ((m_currentPadData.buttons & buttons) == buttons)
		{
			return false;
		} else {
			return true;
		}
	}
	return true;
}

bool Framework::Input::ControllerContext::isButtonPressed(Button buttons, ButtonEventPattern pattern) const
{
	if (pattern == PATTERN_ANY)
	{
		if ((m_pressedButtonData & buttons) != 0)
		{	
			return true;
		} else {
			return false;
		}
	} else if (pattern == PATTERN_ALL) {
		if ((m_pressedButtonData & buttons) == buttons)
		{
			return true;
		} else {
			return false;
		}
	}
	return false;
}

bool Framework::Input::ControllerContext::isButtonReleased(Button buttons, ButtonEventPattern pattern) const
{
	if (pattern == PATTERN_ANY)
	{
		if ((m_releasedButtonData & buttons) != 0)
		{	
			return true;
		} else {
			return false;
		}
	} else if (pattern == PATTERN_ALL) {
		if ((m_releasedButtonData & buttons) == buttons)
		{
			return true;
		} else {
			return false;
		}
	}
	return false;
}

const Vector2& Framework::Input::ControllerContext::getLeftStick(void) const
{
	return m_leftStickXY;
}

const Vector2& Framework::Input::ControllerContext::getRightStick(void) const
{
	return m_rightStickXY;
}

void Framework::Input::ControllerContext::setDeadZone(float deadZone)
{
	m_deadZone = deadZone;
}

Quat Framework::Input::ControllerContext::getOrientation() const
{
  Vector4 v(m_currentPadData.orientation.x, m_currentPadData.orientation.y, m_currentPadData.orientation.z, m_currentPadData.orientation.w);
  v = normalize(v);
  return Quat(v.getX(), v.getY(), v.getZ(), v.getW());
}

uint32_t Framework::Input::ControllerContext::getFingers() const
{
	return m_currentPadData.touchData.touchNum;
}

bool Framework::Input::ControllerContext::isConnected() const
{
	return m_currentPadData.connected;
}

Framework::Input::ControllerContext::Finger Framework::Input::ControllerContext::getFinger(uint32_t index) const
{
	Framework::Input::ControllerContext::Finger result;
	result.x = m_currentPadData.touchData.touch[index].x / (float)m_resolution.x;
	result.y = m_currentPadData.touchData.touch[index].y / (float)m_resolution.y;
	return result;
}


void Framework::Input::ControllerContext::updatePadData(void)
{

	ScePadData data;
	int ret = scePadReadState(m_handle, &data);
	if(ret != SCE_OK)
		return;
	int scePadSetLightBar(int32_t handle, const ScePadLightBarParam *pParam);

	m_currentPadData.buttons = data.buttons;
	m_currentPadData.leftStick.x = data.leftStick.x;
	m_currentPadData.leftStick.y = data.leftStick.y;
	m_currentPadData.rightStick.x = data.rightStick.x;
	m_currentPadData.rightStick.y = data.rightStick.y;
	m_currentPadData.analogButtons.l2 = data.analogButtons.l2;
	m_currentPadData.analogButtons.r2 = data.analogButtons.r2;
	m_currentPadData.orientation = data.orientation;
	m_currentPadData.acceleration = data.acceleration;
	m_currentPadData.angularVelocity = data.angularVelocity;
	m_currentPadData.touchData.touchNum = data.touchData.touchNum;
	for(uint32_t i = 0; i < 2; ++i)
	{
		m_currentPadData.touchData.touch[i].id = data.touchData.touch[i].id;
		m_currentPadData.touchData.touch[i].x = data.touchData.touch[i].x;
		m_currentPadData.touchData.touch[i].y = data.touchData.touch[i].y;
	}
	m_currentPadData.connected = data.connected;
	m_currentPadData.timestamp = data.timestamp;


}

