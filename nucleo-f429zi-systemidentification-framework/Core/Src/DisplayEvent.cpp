/*
 * DisplayEvent.cpp
 *
 *  Created on: Dec 14, 2025
 *      Author: samay099
 */

#include "DisplayEvent.h"
#include "lcd.h"
#include "SystemState.h"
#include "stm32f4xx_hal.h"

I2C_HandleTypeDef hi2c2;
LCD_HandleTypeDef lcd = {LCD_DEFAULT_ADDR,&hi2c2,1};

DisplayEvent::DisplayEvent() {
	// TODO Auto-generated constructor stub

}

DisplayEvent::~DisplayEvent() {
	// TODO Auto-generated destructor stub
}

void DisplayEvent::vHandleEvent(){

		LCD_Begin(&lcd);
		LCD_Clear(&lcd);

	switch(CurrentState){

		case STATE_IDLE :

				LCD_Printf(&lcd, "Status: IDLE");
				LCD_Printf(&lcd, "Press the BLUE BUTTON to START the operation");

				break;

		case STATE_RUNNING:

				LCD_Printf(&lcd, "Status: OPERATION RUNNING");
				LCD_Printf(&lcd, "Press the BLUE BUTTON to CANCEL");

				break;
		case STATE_CLEANUP:

				LCD_Printf(&lcd, "Status: CANCELLED. CLEANUP IN PROGRESS");

				break;

		case STATE_RESULT:

				LCD_Printf(&lcd, "Status: OPERATION COMPLETE");
				LCD_Printf(&lcd, "Operation finished or successfully cancelled");

				break;
		default:
			break;

	}

}

DisplayEvent& DisplayEvent::getObject() {
    static DisplayEvent sObject;
    return sObject;
}

