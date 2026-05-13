/*
 * file_transfer.c
 *
 *  Created on: May 7, 2026
 *      Author: Asus
 */


#include "file_transfer.h"
#include "main.h"
#include "lfs.h"
#include "string.h"
#include "stdio.h"

extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim6;
extern fileHandeler flhdl;


extern lfs_t lfs;

extern uint8_t writeFlag;

#define SOF             0xAA55
#define EOF_MARK        0x0D0A

#define TYPE_START      0x01
#define TYPE_DATA       0x02
#define TYPE_END        0x03
#define TYPE_REQUEST   	0x20

#define TYPE_ACK        0x10
#define TYPE_NACK       0x11

#define MAX_PAYLOAD     1024



typedef enum
{
    WAIT_SOF1,
    WAIT_SOF2,
    READ_HEADER,
    READ_PAYLOAD,
    READ_CRC1,
    READ_CRC2,
    READ_EOF1,
    READ_EOF2

}FT_State_t;

/* -------------------------------------------------- */

static FT_State_t state = WAIT_SOF1;

static uint8_t header[5];
static uint8_t header_index = 0;

static uint8_t payload[MAX_PAYLOAD];
static uint16_t payload_index = 0;

static uint8_t packet_type;
static uint16_t packet_seq;
static uint16_t payload_len;

static uint16_t rx_crc;
static uint16_t calc_crc;

static uint16_t expected_seq = 0;

static uint32_t file_size;
static uint32_t file_crc;
static uint8_t txTimeout = 0;

static char filename[64];

/* -------------------------------------------------- */
/* MicroSecond */
/* -------------------------------------------------- */
void delay_us(uint16_t us){
	TIM6->CNT = 0;
	while(TIM6->CNT < us);

}

void FT_ResetState(void)
{
    state = WAIT_SOF1;

    header_index = 0;
    payload_index = 0;

    packet_type = 0;
    packet_seq = 0;
    payload_len = 0;

    rx_crc = 0;
    calc_crc = 0;

    expected_seq = 0;
}
/* -------------------------------------------------- */
/* CRC16 */
/* -------------------------------------------------- */

uint16_t FT_CRC16(uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;

    for(uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for(uint8_t j = 0; j < 8; j++)
        {
            if(crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }

    return crc;
}

/* -------------------------------------------------- */
/* SEND RESPONSE */
/* -------------------------------------------------- */

void FT_SendResponse(uint8_t type, uint16_t seq)
{
    uint8_t packet[11];

    uint16_t sof = SOF;
    uint16_t eof = EOF_MARK;

    memcpy(&packet[0], &sof, 2);

    packet[2] = type;

    memcpy(&packet[3], &seq, 2);

    uint16_t len = 0;

    memcpy(&packet[5], &len, 2);

    uint16_t crc = FT_CRC16(&packet[2], 5);

    memcpy(&packet[7], &crc, 2);

    memcpy(&packet[9], &eof, 2);

    HAL_UART_Transmit(&huart3, packet, 11, HAL_MAX_DELAY);
}

/* -------------------------------------------------- */
/* PROCESS PACKET */
/* -------------------------------------------------- */

void FT_ProcessPacket()
{
    uint8_t crc_buffer[5 + MAX_PAYLOAD];

    memcpy(crc_buffer, &packet_type, 1);
    memcpy(&crc_buffer[1], &packet_seq, 2);
    memcpy(&crc_buffer[3], &payload_len, 2);
    memcpy(&crc_buffer[5], payload, payload_len);

    calc_crc = FT_CRC16(crc_buffer, payload_len + 5);

    if(calc_crc != rx_crc)
    {
        FT_SendResponse(TYPE_NACK, packet_seq);
        return;
    }

    if(packet_seq < expected_seq)
    {
        FT_SendResponse(TYPE_ACK, packet_seq);
        return;
    }

    if(packet_seq != expected_seq)
    {
        FT_SendResponse(TYPE_NACK, packet_seq);
        return;
    }

    /* START PACKET */

    if(packet_type == TYPE_START)
    {
        memcpy(&file_size, &payload[0], 4);
        memcpy(&file_crc, &payload[4], 4);

        uint8_t name_len = payload[8];

        memcpy(filename, &payload[9], name_len);

        filename[name_len] = 0;


        printf("START FILE: %s\r\n", filename);
        strcpy((char*)flhdl.filenameWrite,
            	       filename);
        HAL_UART_Transmit(&huart1, (uint8_t *)filename, 30, 1000);
        HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n",2, 1000);
        printf("SIZE: %lu\r\n", file_size);

        expected_seq++;

        FT_SendResponse(TYPE_ACK, packet_seq);
    }

    /* DATA PACKET */

    else if(packet_type == TYPE_DATA)
    {
        while((flhdl.fsBusy == 1) && txTimeout < 100){
    		txTimeout++;
    		delay_us(5000);
    	}
        flhdl.fsBusy = 0;
    	txTimeout = 0;
        __disable_irq();
        flhdl.payloadWrite = payload_len;

        flhdl.sequenceWrite = expected_seq;

    	memcpy((void*)flhdl.writeBuffer,
    			payload,
				payload_len);

    	flhdl.writeFlag = 1;
        __enable_irq();
        expected_seq++;

        FT_SendResponse(TYPE_ACK, packet_seq);
    }

    /* END PACKET */

    else if(packet_type == TYPE_END)
    {
    	flhdl.fileClose = 1;
        expected_seq = 0;
        flhdl.sequenceWrite = 0;
        FT_SendResponse(TYPE_ACK, packet_seq);
        FT_ResetState();

    }
    else if(packet_type == TYPE_REQUEST)
    {
        memcpy((void*)flhdl.requestName,
               payload,
               payload_len);

        flhdl.requestName[payload_len] = 0;
        flhdl.fileClose = 0;
        flhdl.requestFile = 1;
    }
}

/*-------------------------------
 Processing received Byte
 ------------------------------*/
void FT_processByte(uint8_t byte){
	switch(state)
	{
		case WAIT_SOF1:
			if(byte == 0x55){
				state = WAIT_SOF2;
				flhdl.writeFlag = 0;
			}
		break;
        case WAIT_SOF2:

            if(byte == 0xAA)
            {
                state = READ_HEADER;
                header_index = 0;
            }
            else
            {
                state = WAIT_SOF1;
            }

        break;
        case READ_HEADER:

            header[header_index++] = byte;

            if(header_index >= 5)
            {
                packet_type = header[0];

                memcpy(&packet_seq, &header[1], 2);


                payload_len = (header[4] << 8) + header[3];

                if(payload_len > MAX_PAYLOAD)
                {
                    state = WAIT_SOF1;
                    break;
                }
                payload_index = 0;

                if(payload_len == 0)
                    state = READ_CRC1;
                else
                    state = READ_PAYLOAD;
            }

        break;
        case READ_PAYLOAD:

            payload[payload_index++] = byte;

            if(payload_index >= payload_len)
            {
                state = READ_CRC1;
            }

        break;

        case READ_CRC1:

            rx_crc = byte;

            state = READ_CRC2;

        break;

        case READ_CRC2:

            rx_crc |= (byte << 8);

            state = READ_EOF1;

        break;

        case READ_EOF1:
            if(byte == 0x0D)
                state = READ_EOF2;
            else
                state = WAIT_SOF1;

        break;

        case READ_EOF2:

            if(byte == 0x0A)
            {
                FT_ProcessPacket();
            }

            state = WAIT_SOF1;

        break;

        default:
            state = WAIT_SOF1;
        break;
    }

}

/* Sending File Functions*/
void FT_SendPacket(uint8_t type,
                   uint16_t seq,
                   uint8_t *payload,
                   uint16_t length)
{
    uint8_t packet[1160];

    uint16_t sof = SOF;

    uint16_t eof = EOF_MARK;

    memcpy(&packet[0], &sof, 2);

    packet[2] = type;

    memcpy(&packet[3], &seq, 2);

    memcpy(&packet[5], &length, 2);

    if(length > 0)
    {
        memcpy(&packet[7],
               payload,
               length);
    }

    uint16_t crc =
        FT_CRC16(&packet[2],
                 5 + length);

    memcpy(&packet[7 + length],
           &crc,
           2);

    memcpy(&packet[9 + length],
           &eof,
           2);

    HAL_UART_Transmit(&huart3,
                      packet,
                      length + 11,
                      HAL_MAX_DELAY);
}


/*
 * Sending File from NOR Flash to PC
 */void FT_SendFile(char *filename)
{
    lfs_file_t file;

    static uint8_t buffer[MAX_PAYLOAD];

    uint16_t seq = 0;

    int size;

    uint32_t file_size;

    /* OPEN FILE */

    if(lfs_file_open(&lfs,
                     &file,
                     filename,
                     LFS_O_RDONLY) < 0)
    {
//        printf("OPEN FAILED\r\n");
        return;
    }

    /* GET FILE SIZE */

    file_size =
        lfs_file_size(&lfs,
                      &file);

    /* START PAYLOAD */

    static uint8_t start_payload[128];

    uint32_t file_crc = 0;

    uint8_t name_len =
        strlen(filename);

    memcpy(&start_payload[0],
           &file_size,
           4);

    memcpy(&start_payload[4],
           &file_crc,
           4);

    start_payload[8] = name_len;

    memcpy(&start_payload[9],
           filename,
           name_len);

    /* SEND START */

    FT_SendPacket(TYPE_START,
                  seq,
                  start_payload,
                  9 + name_len);

    seq++;


    /* SEND FILE DATA */

    while(1)
    {
        size =
            lfs_file_read(&lfs,
                          &file,
                          buffer,
                          MAX_PAYLOAD);

        if(size <= 0)
            break;

        FT_SendPacket(TYPE_DATA,
                      seq,
                      buffer,
                      size);

        seq++;

        HAL_Delay(5);
    }

    /* SEND END */

    FT_SendPacket(TYPE_END,
                  seq,
                  NULL,
                  0);

    lfs_file_close(&lfs,
                   &file);

    flhdl.requestFile = 0;
}
