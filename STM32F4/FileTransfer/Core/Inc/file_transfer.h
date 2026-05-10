/*
 * file_transfer.h
 *
 *  Created on: May 7, 2026
 *      Author: Asus
 */

#ifndef INC_FILE_TRANSFER_H_
#define INC_FILE_TRANSFER_H_

#include "main.h"
#include "stm32f4xx.h"


void FT_processByte(uint8_t byte);
void FT_processPacket(uint8_t *packet);
void FT_SendPacket(uint8_t type,
                   uint16_t seq,
                   uint8_t *payload,
                   uint16_t length);

void FT_SendFile(char *filename);

#endif /* INC_FILE_TRANSFER_H_ */
