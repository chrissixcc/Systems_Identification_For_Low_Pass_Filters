/*
 * StartEvent.cpp
 *
 *  Created on: Dec 4, 2025
 *      Author: Chrissi
 */

#include "StartEvent.h"
#include "SystemStateHandler.h"
#include "SystemState.h"
#include "main.h"

extern SystemStateHandler stateHandler; //wie in main
extern SystemState CurrentState; //auch so in main

StartEvent::StartEvent() {
}


void StartEvent::vHandleEvent() {
	if (CurrentState != STATE_IDLE) {
		return; //nichts machen wenn nicht idle
	}

	CurrentState = STATE_OPERATING;
	//hier NICHT das blinken implementieren, muss in die main loop
	//da sonst die event queue blockiert wird
}
