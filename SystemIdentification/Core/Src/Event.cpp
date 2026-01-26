/*
 * Event.cpp
 *
 *  Created on: Dec 14, 2025
 *      Author: martin
 */

#include <cstdio>
#include "Event.h"
#include "main.h"
#include "SystemStateHandler.h"
#include "lcd.h"

// Definition threshold value (0,95*4095=3890,25)
#define THRESHOLD_3T 3890
#define SAMPLE_PERIOD_US 100
#define ADC_BUFFER_LEN 10000
#define NUM_MEASUREMENTS 3

extern std::queue<Event*> event_queue;
extern SystemStateHandler* sysHandler;
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;
extern LCD_HandleTypeDef lcd;
extern uint32_t stateTimestamp;

uint16_t adcBuffer[ADC_BUFFER_LEN];
static uint8_t currentMeasurement = 0;
static bool measurementActive = false;
std::vector<float> resultsBuffer;

volatile uint32_t bufferOverreached = 0;
volatile bool dataReady = false;

void SafePush(Event* e) {
    __disable_irq();
    event_queue.push(e);
    __enable_irq();
}

void DisplayEvent::vHandleEvent() {

	LCD_Clear(&lcd);
	LCD_SetCursor(&lcd, 0, 0);

	// Switch trough all possible states and set display output
	switch(CurrentState){

		case STATE_STARTING:
			LCD_Printf(&lcd, "Booting...");

			break;

		case STATE_IDLE :

			LCD_Printf(&lcd, "State: IDLE");
			LCD_SetCursor(&lcd, 1, 0);
			LCD_Printf(&lcd, "BLUE to START");

			break;

		case STATE_RUNNING:

			LCD_Printf(&lcd, "State: RUNNING");
			LCD_SetCursor(&lcd, 1, 0);
			LCD_Printf(&lcd, "BLUE to CANCEL");

			break;

		case STATE_CLEANUP:

			LCD_Printf(&lcd, "State: CANCELED");
			LCD_SetCursor(&lcd, 1, 0);
			LCD_Printf(&lcd, "CLEANUP ...");

			break;

		case STATE_RESULT:

			LCD_Printf(&lcd, "State: COMPLETED");
			LCD_SetCursor(&lcd, 1, 0);
			LCD_Printf(&lcd, "%s", message.c_str());

			break;

		case STATE_ERROR:

			LCD_Printf(&lcd, "State: ERROR");
			LCD_SetCursor(&lcd, 1, 0);
			LCD_Printf(&lcd, "Restart device");

			break;

		default:
			break;
	}
}

void SystemStartEvent::vHandleEvent() {
	// Startup Event

	sysHandler->vTransition(STATE_STARTING);
	DisplayEvent("").vHandleEvent();
	SafePush(new StartTimeoutEvent());
}

void StartTimeoutEvent::vHandleEvent() {
	// Start timeout without blocking

    if (HAL_GetTick() - stateTimestamp < 1100) {
    	SafePush(new StartTimeoutEvent());
    } else {
    	SafePush(new SystemIdleEvent());
    }
}

void SystemIdleEvent::vHandleEvent() {
	// Idle Event showing operational

    sysHandler->vTransition(STATE_IDLE);
    SafePush(new DisplayEvent("IDLE: Blau druecken"));
}

void BlueButtonEvent::vHandleEvent() {
	// React to pressing of blue button depending on current state

	if (CurrentState == STATE_CLEANUP) {
		return;
	}
	else if (CurrentState == STATE_IDLE) {
		SafePush(new StartMeasureEvent());
    }
    else if (CurrentState == STATE_RUNNING) {
    	SafePush(new SystemCancelEvent());
    }
    else if (CurrentState == STATE_RESULT) {
    	SafePush(new SystemCancelEvent());
    }
}

void StartMeasureEvent::vHandleEvent() {
	// Starting the measurement when blue button is pressed and STATE_IDLE

	sysHandler->vTransition(STATE_RUNNING);

    if (currentMeasurement == 0) {
        resultsBuffer.clear();
        SafePush(new DisplayEvent("Messung laeuft..."));
    }

    HAL_TIM_Base_Stop(&htim2);
	HAL_ADC_Stop_DMA(&hadc1);

	bufferOverreached = 0;
    dataReady = false;
    measurementActive = true;

    // Set timer counter to 0 and delete all flags
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_OVR | ADC_FLAG_AWD | ADC_FLAG_EOC);

    // Give power to testing circuit
    HAL_GPIO_WritePin(RC_INPUT_GPIO_Port, RC_INPUT_Pin, GPIO_PIN_SET);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuffer, ADC_BUFFER_LEN);
    HAL_TIM_Base_Start(&htim2);
}

void CalculationEvent::vHandleEvent() {
	// Measure time to reach 3T

	if (!dataReady) {
		return; // Shouldnt be reached
	}

	// Search for threshold point
    int32_t foundIndex = -1;
    for (int i = 0; i < ADC_BUFFER_LEN; i++) {
        if (adcBuffer[i] >= THRESHOLD_3T) {
            foundIndex = i;
            break;
        }
    }

    int32_t index = bufferOverreached + foundIndex;

	// time of tau = index * period duration (100µs)
	float timeTau = (float)index * (float)SAMPLE_PERIOD_US;

	resultsBuffer.push_back(timeTau);

    currentMeasurement++;

    if (currentMeasurement < NUM_MEASUREMENTS) {
    	SafePush(new DischargeEvent());
    } else {
    	SafePush(new FinalCalculationEvent(0));
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc1) {
	// Callback for full ADC DMA register to count buffer overflows

    if (hadc1->Instance == ADC1 && !dataReady) {
    	bufferOverreached += ADC_BUFFER_LEN;
    }
}

void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef* hadc1) {
	// Callback for exceeding threshold

	if (hadc1->Instance == ADC1 && !dataReady) {
        TIM2->CR1 &= ~TIM_CR1_CEN; // Timer Register Stop
        HAL_ADC_Stop_DMA(hadc1);

        dataReady = true;

        HAL_GPIO_WritePin(RC_INPUT_GPIO_Port, RC_INPUT_Pin, GPIO_PIN_RESET);

        SafePush(new CalculationEvent());
    }
}

void DischargeEvent::vHandleEvent() {
    // Check if capacitor is discharged

    HAL_ADC_Start(&hadc1);
    HAL_TIM_Base_Start(&htim2);
    uint32_t val = HAL_ADC_GetValue(&hadc1);
    HAL_TIM_Base_Stop(&htim2);
    HAL_ADC_Stop(&hadc1);

    // Discharge condensator before next measurement
    if (val < 50) {
    	// Set Pin to Input to have high resistance
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = Discharge_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(Discharge_GPIO_Port, &GPIO_InitStruct);
        HAL_Delay(10);
        SafePush(new StartMeasureEvent());
        return;
    } else {
    	// Set Pin to Output to discharge condensator
    	GPIO_InitTypeDef GPIO_InitStruct = {0};
		GPIO_InitStruct.Pin = Discharge_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(Discharge_GPIO_Port, &GPIO_InitStruct);
		HAL_GPIO_WritePin(Discharge_GPIO_Port, Discharge_Pin, GPIO_PIN_RESET);
		HAL_Delay(5);
		SafePush(new DischargeEvent());
        return;
    }
}

void FinalCalculationEvent::vHandleEvent() {
	// Calculate the time constant and send it to the display

	// Stop power to testing circuit
    HAL_GPIO_WritePin(RC_INPUT_GPIO_Port, RC_INPUT_Pin, GPIO_PIN_RESET);

    uint64_t sum = 0;
    for (auto t : resultsBuffer)
		sum += t;

	float avgTau = sum / (float)NUM_MEASUREMENTS;

	currentMeasurement = 0;
	bufferOverreached = 0;
	measurementActive = false;
	dataReady = false;
	resultsBuffer.clear();

    sysHandler->vTransition(STATE_RESULT);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "T=%.1f ms", avgTau / 3000.0f);
    SafePush(new DisplayEvent(buffer));
    SafePush(new ResultTimeoutEvent());
}

void ResultTimeoutEvent::vHandleEvent() {
	// Give user enough time to read result without blocking

    if (HAL_GetTick() - stateTimestamp >= 5000) {
    	SafePush(new SystemIdleEvent());
    }
    else {
    	SafePush(new ResultTimeoutEvent());
    }
}

void SystemCancelEvent::vHandleEvent() {
	// Handle cancellation trough blue button press

    sysHandler->vTransition(STATE_CLEANUP);

    // Block interrupts to prevent race conditions
    __disable_irq();

    TIM2->CR1 &= ~TIM_CR1_CEN;
	HAL_ADC_Stop_DMA(&hadc1);
	HAL_GPIO_WritePin(RC_INPUT_GPIO_Port, RC_INPUT_Pin, GPIO_PIN_RESET);

	currentMeasurement = 0;
	bufferOverreached = 0;
	measurementActive = false;
	dataReady = false;
	resultsBuffer.clear();

	// Set Pin to Input to have high resistance
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = Discharge_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(Discharge_GPIO_Port, &GPIO_InitStruct);

	// Clear event_queue
	while(!event_queue.empty()) {
		delete event_queue.front();
		event_queue.pop();
	}

    event_queue.push(new DisplayEvent(""));
    event_queue.push(new CancelTimeoutEvent());

    __enable_irq();
}

void CancelTimeoutEvent::vHandleEvent() {
	// Give user time to read cleanup done without blocking

    if (HAL_GetTick() - stateTimestamp > 1000) {
    	SafePush(new SystemIdleEvent());
    }
    else {
    	HAL_Delay(5);
    	SafePush(new CancelTimeoutEvent());
    }
}
