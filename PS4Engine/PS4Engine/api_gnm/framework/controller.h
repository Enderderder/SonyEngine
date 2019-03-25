/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_CONTROLLER_H_
#define _SCE_GNM_FRAMEWORK_CONTROLLER_H_

#include "../toolkit/geommath/geommath.h"
#include <user_service.h>
#include <scetypes.h>
#include <gnm.h>


namespace Framework
{
	namespace Input
	{
		enum Button
		{
			BUTTON_L3          = 0x00000002,
			BUTTON_R3          = 0x00000004,
			BUTTON_OPTIONS     = 0x00000008,
			BUTTON_UP          = 0x00000010,
			BUTTON_RIGHT       = 0x00000020,
			BUTTON_DOWN        = 0x00000040,
			BUTTON_LEFT        = 0x00000080,
			BUTTON_L2          = 0x00000100,
			BUTTON_R2          = 0x00000200,
			BUTTON_L1          = 0x00000400,
			BUTTON_R1          = 0x00000800,
			BUTTON_TRIANGLE    = 0x00001000,
			BUTTON_CIRCLE      = 0x00002000,
			BUTTON_CROSS       = 0x00004000,
			BUTTON_SQUARE      = 0x00008000,
			BUTTON_TOUCH_PAD   = 0x00100000,
			BUTTON_INTERCEPTED = 0x80000000,
		};

		enum ButtonEventPattern
		{
			PATTERN_ANY,
			PATTERN_ALL,
		};

		class ControllerContext {
			double m_initialSecondsUntilNextRepeat;
			double m_finalSecondsUntilNextRepeat;
			double m_previousSeconds[16];
			double m_secondsUntilNextRepeat[16];
		public:

			ControllerContext(void);
			~ControllerContext(void);

			int initialize(void);
			int finalize(void);
			void update(double seconds); 


			// check whether any or all of the specified button are down
			bool isButtonDown(Button buttons, ButtonEventPattern pattern=PATTERN_ALL) const;
			bool isButtonUp(Button buttons, ButtonEventPattern pattern=PATTERN_ALL) const;
			bool isButtonPressed(Button buttons, ButtonEventPattern pattern=PATTERN_ALL) const;
			bool isButtonReleased(Button buttons, ButtonEventPattern pattern=PATTERN_ALL) const;

			const Vector2& getLeftStick(void) const;
			const Vector2& getRightStick(void) const;

			Quat getOrientation() const;

			void setDeadZone(float deadZone);

			struct Finger
			{
				float x;
				float y;
				uint8_t id;
				uint8_t reserved[3];
			};
			Finger getFinger(uint32_t index) const;
			uint32_t getFingers() const;
			
			bool isConnected() const;
		private:
			struct Touch 
			{
				uint16_t  x;		
				uint16_t  y;		
				uint8_t   id;		
				uint8_t   reserved;
			};

			Touch m_resolution;

			struct AnalogStick
			{
				uint8_t x;
				uint8_t y;
			};

			struct AnalogButtons
			{
				uint8_t l2;
				uint8_t r2;
				uint8_t padding[2]; 
			};

			struct TouchData 
			{
				uint8_t touchNum;	
				uint8_t reserved;
				Touch touch[2];	
			} ;

			struct Data
			{
				uint32_t		buttons;		
				AnalogStick		leftStick;		
				AnalogStick		rightStick;		
				AnalogButtons	analogButtons;
				SceFQuaternion  orientation;                                       											
				SceFVector3		acceleration;	
				SceFVector3		angularVelocity;
				TouchData       touchData;	
				bool			connected;	
				uint8_t         reserved[5];
				uint64_t        timestamp;	
			};

		public:
			uint16_t m_reserved0;
		private:
			Data m_currentPadData;
			Data m_temporaryPadData;
			Vector2	m_leftStickXY;
			Vector2	m_rightStickXY;
			Vector2	m_dummyStickXY;

			uint32_t		m_pressedButtonData;		///< The "Pressed" button event data.
			uint32_t		m_releasedButtonData;		///< The "Released" button event data.

			float					m_deadZone;
			static const float		m_defaultDeadZone;
			static const float		m_recipMaxByteAsFloat;

			int32_t m_handle;


			void updatePadData(void);
		};
	}
}

#endif // _SCE_GNM_FRAMEWORK_CONTROLLER_H_
