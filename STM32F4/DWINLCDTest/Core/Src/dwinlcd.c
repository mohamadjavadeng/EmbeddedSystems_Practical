/*
 * dwinlcd.c
 *
 *  Created on: Feb 16, 2026
 *      Author: Mohamad
 */
#include "dwinlcd.h"
#include <string.h>
#include <stdlib.h>

uint8_t day, month, year, hour, minute, second, weekday;
uint8_t backlightValue;
uint8_t backlightCurrent;


void DWINDriverInit(dwinDriver *driver, UART_HandleTypeDef *huart){
	driver->huart = huart;
	driver->resLength = 0;
	driver->rxComplete = 0;
}

HAL_StatusTypeDef DWINSendCommand(dwinDriver *driver, uint8_t *data, uint16_t size){
	HAL_StatusTypeDef status;
	status = HAL_UART_Transmit(driver->huart, data, size, 100);

	while(status == HAL_BUSY) HAL_Delay(5);
	return HAL_OK;
}

void DWINDriverListening(dwinDriver *driver){
	HAL_UARTEx_ReceiveToIdle_IT(driver->huart,
								driver->responseBuffer,
								RESPONSEBUFFER);
}

HAL_StatusTypeDef isConnected(dwinDriver *driver){
	uint8_t cmd[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x31, 0x01};
	driver->cmdType = wrData;
	if(DWINSendCommand(driver, cmd, sizeof(cmd)) != HAL_OK) return HAL_ERROR;

	if(driver->responseBuffer[0] != 0x5A)return HAL_ERROR;

	else if(driver->responseBuffer[1] != 0xA5){
		return HAL_ERROR;
	}
	else if(driver->responseBuffer[2] != 0x06){

		return HAL_ERROR;
	}
	else if(driver->responseBuffer[3] != 0x83){

		return HAL_ERROR;
	}
	else if(driver->responseBuffer[4] != 0x00){

		return HAL_ERROR;
	}
	return HAL_OK;

}

void nextPage(dwinDriver *driver){

	uint8_t currentPage[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x14, 0x01};
	driver->cmdType = pageChange;
	DWINSendCommand(driver, currentPage, sizeof(currentPage));
	uint8_t page1, page2;
	if(driver->responseBuffer[8] == 0xFF){
		page2 = driver->responseBuffer[7] + 1;
		page1 = 0;
	}
	else{
		page1 = driver->responseBuffer[8] + 1;
		page2 = driver->responseBuffer[7];
	}
	uint8_t nextpage[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, page2, page1};
	DWINSendCommand(driver, nextpage, sizeof(nextpage));

}
void previousPage(dwinDriver *driver){

	uint8_t currentPage[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x14, 0x01};
	driver->cmdType = pageChange;
	DWINSendCommand(driver, currentPage, sizeof(currentPage));
	uint8_t page1, page2;
	if(driver->responseBuffer[8] == 0x00 && driver->responseBuffer[7] != 0x00){
		page2 = driver->responseBuffer[7] - 1;
		page1 = 0xFF;
	}
	else if(driver->responseBuffer[8] == 0x00 && driver->responseBuffer[7] == 0x00){
		page2 = 0;
		page1 = 0;
	}
	else{
		page1 = driver->responseBuffer[8] + 1;
		page2 = driver->responseBuffer[7];
	}
	uint8_t previouspage[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, page2, page1};
	DWINSendCommand(driver, previouspage, sizeof(previouspage));

}
void gotoPage(dwinDriver *driver, const uint16_t page){
	driver->cmdType = pageChange;
	uint8_t page1 = page & 0xFF;
	uint8_t page2 = (page >> 8) & 0xFF;
	uint8_t command[] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, page2, page1};
	DWINSendCommand(driver, command, sizeof(command));
}
void writeSingleReg(dwinDriver *driver, const uint16_t registeraddress, const uint16_t value){
	driver->cmdType = wrData;
	uint8_t highAddress = (registeraddress >> 8) & 0xFF;
	uint8_t lowAddress = registeraddress & 0xFF;
	uint8_t highValue = (value >> 8) & 0xFF;
	uint8_t lowValue = value & 0xFF;
	uint8_t singleRegCommand[] = {0x5A, 0xA5, 0x05, 0x82, highAddress, lowAddress, highValue, lowValue};
	DWINSendCommand(driver, singleRegCommand, sizeof(singleRegCommand));

}
void writeData(dwinDriver *driver, const uint16_t registeraddress, const uint8_t *data, const uint16_t length){
	driver->cmdType = wrData;
	uint8_t highByte = (registeraddress >> 8) & 0xFF; // Extract high byte
	uint8_t lowByte = registeraddress & 0xFF;
	uint8_t* newData = malloc(length + 6);
    newData[0] = 0x5A;
    newData[1] = 0xA5;
    newData[2] = length + 3;
    newData[3] = 0x82;
    newData[4] = highByte;
    newData[5] = lowByte;
    for(int i = 0; i < length; i++){
        newData[i+6] = data[i];
    }
    DWINSendCommand(driver, newData, sizeof(newData));
    free(newData);
}
void setSingleBit(dwinDriver *driver, const uint16_t registeraddress, const uint16_t Bitnumber){
	driver->cmdType = wrData;
	uint16_t val = 1;
	val = val << Bitnumber;
	uint16_t readPV = readSingleReg(driver, registeraddress);
	readPV |= val;
	HAL_Delay(5);
	writeSingleReg(driver, registeraddress, readPV);

}
void resetSingleBit(dwinDriver *driver, const uint16_t registeraddress, const uint16_t Bitnumber){
	driver->cmdType = wrData;
	uint16_t val = 1;
	val = val << Bitnumber;
	uint16_t readPV = readSingleReg(driver, registeraddress);
	readPV &= ~val;
	HAL_Delay(5);
	writeSingleReg(driver, registeraddress, readPV);


}


uint16_t readSingleReg(dwinDriver *driver, const uint16_t registeraddress){
	driver->cmdType = rdSingleReg;
	uint8_t highByte = (registeraddress >> 8) & 0xFF; // Extract high byte
    uint8_t lowByte = registeraddress & 0xFF;
    uint8_t command[] = {0x5A, 0xA5, 0x04, 0x83, highByte, lowByte, 0x01};
    DWINSendCommand(driver, command, sizeof(command));
    uint16_t highres = driver->responseBuffer[7];
	uint16_t result = (highres << 8) + driver->responseBuffer[8];
    return result;
}
HAL_StatusTypeDef readSingleBit(dwinDriver *driver, const uint16_t registeraddress, const uint16_t Bitnumber){
	driver->cmdType = rdSingleBit;
	uint8_t highByte = (registeraddress >> 8) & 0xFF; // Extract high byte
	uint8_t lowByte = registeraddress & 0xFF;
	uint8_t command[] = {0x5A, 0xA5, 0x04, 0x83, highByte, lowByte, 0x01};
	DWINSendCommand(driver, command, sizeof(command));
	uint16_t highres = driver->responseBuffer[7];
	uint16_t result = (highres << 8) + driver->responseBuffer[8];
	uint16_t bitStatus = (1 << Bitnumber) & result;
	if(bitStatus != 0 ) return SET;
	return RESET;

}
void readReg(dwinDriver *driver, const uint16_t registeraddress, uint8_t numberOfBytes){

	driver->cmdType = rdRegisters;
    uint8_t highByte = (registeraddress >> 8) & 0xFF; // Extract high byte
    uint8_t lowByte = registeraddress & 0xFF;
    uint8_t command[] = {0x5A, 0xA5, 0x04, 0x83, highByte, lowByte, numberOfBytes};
    DWINSendCommand(driver, command, sizeof(command));

}
void readRTC(dwinDriver *driver){
	driver->cmdType = rdRTC;
	uint8_t command[] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x10, 0x04};
	DWINSendCommand(driver, command, sizeof(command));
	year = driver->responseBuffer[7];
	month = driver->responseBuffer[8];
	day = driver->responseBuffer[9];
	weekday = driver->responseBuffer[10];
	hour = driver->responseBuffer[11];
	minute = driver->responseBuffer[12];
	second = driver->responseBuffer[13];
}
void writeRTC(dwinDriver *driver, uint8_t dayrtc, uint8_t monthrtc, uint8_t yearrtc, uint8_t hourrtc, uint8_t minutertc, uint8_t secondrtc, weekdays weekdayrtc){
	driver->cmdType = wrData;
	uint8_t command[] = {0x5A, 0xA5, 0x0B, 0x82, 0x00, 0x10, yearrtc, monthrtc, dayrtc, weekdayrtc, hourrtc, minutertc, secondrtc, 0x00};
	 DWINSendCommand(driver, command, sizeof(command));
}
void buzzer(dwinDriver *driver, buzzer_duration buzzer){
	driver->cmdType = buzOn;
	uint8_t duration;
	if(buzzer == BUZZ_1SEC){
		duration = 0x7D;
	}
	else if(buzzer == BUZZ_500MSEC){
		duration = 0x3E;
	}
	else if(buzzer == BUZZ_250MSEC){
		duration = 0x20;
	}
	else{
		duration = 0x7D;
	}
	uint8_t command[] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0xA0, 0x00, duration};
	DWINSendCommand(driver, command, sizeof(command));
}

// Handling Functions
HAL_StatusTypeDef writeDataHandler(dwinDriver *driver, uint16_t len){
	if(driver->responseBuffer[0] != 0x5A) return HAL_ERROR;
	if(driver->responseBuffer[1] != 0xA5) return HAL_ERROR;
	if(driver->responseBuffer[2] != 0x03) return HAL_ERROR;
	if(driver->responseBuffer[3] != 0x82) return HAL_ERROR;
	if(driver->responseBuffer[4] != 0x4F) return HAL_ERROR;
	if(driver->responseBuffer[5] != 0x4F) return HAL_ERROR;

	return HAL_OK;
}

HAL_StatusTypeDef readRTCHandler(dwinDriver *driver, uint8_t year,
									uint8_t month, uint8_t day, uint8_t weekday,
									uint8_t hour, uint8_t minute, uint8_t second){

}

