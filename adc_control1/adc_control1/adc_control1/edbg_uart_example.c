/**
 * \file
 *
 * \brief Application implement
 *
 * Copyright (c) 2015-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */

#include "atmel_start.h"
#include "atmel_start_pins.h"
#include "driver_init.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

volatile static uint32_t data_arrived = 0;

#define DATA_NUM	1024

int dac_out0=0xCFF, dac_out1=0xDFF, dac_out2=0xEFF ;

int adc_count = 0 ;
int adc_data[DATA_NUM] = {0};
int lower_discri_count = 0 ;
int zero_cross_count = 0 ;
int upper_discri_count = 0 ;
int upper_discri_time[DATA_NUM] ={0};


static void tx_cb_EDBG_COM(const struct usart_async_descriptor *const io_descr)
{
	/* Transfer completed */
	gpio_toggle_pin_level(LED0);
}

static void rx_cb_EDBG_COM(const struct usart_async_descriptor *const io_descr)
{
	/* Receive completed */
	data_arrived = 1;
}

static void err_cb_EDBG_COM(const struct usart_async_descriptor *const io_descr)
{
	/* error handle */
//	io_write(&EDBG_COM.io, example_hello_world, 12);
}

void put_char( char c )
{
	while (io_write(&EDBG_COM.io, &c, 1) != 1) {
	}
	while( 1 )
		if( usart_async_get_status(&EDBG_COM.io, NULL) == ERR_NONE)
			break ;
}


void print( char *msg)
{
	for(int i = 0 ; i < strlen(msg); i++ )
		put_char( msg[i] );
}

uint8_t get_ch( void )
{
	uint8_t recv_char;
	while( data_arrived == 0 )
		return 0;
	while (io_read(&EDBG_COM.io, &recv_char, 1) == 1)
		break ;
	data_arrived = 0 ;
	
	return recv_char ;
}

uint8_t get_char( void )
{
	uint8_t recv_char;
	while( data_arrived == 0 )
		;
	while (io_read(&EDBG_COM.io, &recv_char, 1) == 1)
		break ;
	put_char( recv_char );
	data_arrived = 0 ;
	
	return recv_char ;
}

void get_str( char *ss )
{
	uint8_t c ;
	int i = 0 ;

	ss[0] = 0 ;
	while( 1 ){
		c = get_char();
		if( c == '\n'){
			ss[i] = 0 ;
			return ;
		}else if( c == '\b' && i != 0 ){
			i--;
			print("\b \b");
		}else{
			ss[i++] = c ;
		}
	}
}

void dump_data( void )
{
	char ss[128];
	int i;
	
	sprintf( ss, "ADC COUNT = %d\r\n", adc_count);
	print( ss );

adc_count = 32 ;
	print("ADC DATA :\r\n");
	for( i = 0 ; i < adc_count ; i++ ){
		sprintf( ss, "%04X ", adc_data[i] );
		print(ss);
		if((i%8) == 7) print("\r\n");
		if((i%8) == 3) print(" ");
	}
	print("\r\n");

	sprintf( ss, "Lower Discri Count = %d\r\n", lower_discri_count);
	print( ss );
	sprintf( ss, "Zero Cross Count = %d\r\n", zero_cross_count);
	print( ss );
	sprintf( ss, "Upper Discri Count = %d\r\n", upper_discri_count);
	print( ss );

upper_discri_count = 45 ;
	print("Upper Discri Time(msec) :\r\n");
	for( i = 0 ; i < upper_discri_count ; i++ ){
		sprintf( ss, "%4d ", upper_discri_time[i] );
		print(ss);
		if((i%8) == 7) print("\r\n");
		if((i%8) == 3) print(" ");
	}
	print("\r\n");

}

void dac_write( unsigned char *ss )
{
	struct io_descriptor *io;

	spi_m_sync_get_io_descriptor(&SPI_0, &io);
	
	gpio_set_pin_level(DAC_CSn_3V, false);

	spi_m_sync_enable(&SPI_0);
	io_write(io, ss, 2);

	gpio_set_pin_level(DAC_CSn_3V, true);
}

void adc_read( unsigned char *ss )
{
	struct io_descriptor *io;

	spi_m_sync_get_io_descriptor(&SPI_0, &io);
	
	spi_m_sync_enable(&SPI_0);
	io_read(io, ss, 2);

}


void main_proc_start(void)
{
	char ss[64];
	unsigned short h ;


	convert_complete_flag = 0 ;
	ss[0] = dac_out0 >> 8 ; ss[1] = dac_out0 ; dac_write( ss );
	ss[0] = dac_out1 >> 8 ; ss[1] = dac_out1 ; dac_write( ss );
	ss[0] = dac_out2 >> 8 ; ss[1] = dac_out2 ; dac_write( ss );

	gpio_set_pin_level(DAC_LDACn, false);
	gpio_set_pin_level(ADC_CSn_3V, false);
	gpio_set_pin_level(FF_Enable_5V, true);

	while( 1 ){
		while( convert_complete_flag == 0 )
			if(get_ch()) goto LLL;
        adc_read( ss );
		h = ((unsigned short)(ss[0]) << 8) | ss[1] ;
		h >>= 3 ;
		sprintf( ss, "ADC_Value = %04X\n", (unsigned)h );
		print( ss );
		gpio_set_pin_level(FF_Enable_5V, false);
		gpio_set_pin_level(FF_Enable_5V, true);
		convert_complete_flag = 0;
		if( get_ch() ) break ;
	}
LLL:;
	gpio_set_pin_level(FF_Enable_5V, false);
	gpio_set_pin_level(DAC_CSn_3V, true);
	gpio_set_pin_level(DAC_LDACn, true);

}


int main(void)
{
	uint8_t recv_char;
	char buf[80];
	int dac_out0=0xCFF, dac_out1=0xDFF, dac_out2=0xEFF ;
	char c ;
	int i;
	int start = 0 ;

	atmel_start_init();

	usart_async_register_callback(&EDBG_COM, USART_ASYNC_TXC_CB, tx_cb_EDBG_COM);
	usart_async_register_callback(&EDBG_COM, USART_ASYNC_RXC_CB, rx_cb_EDBG_COM);
	usart_async_register_callback(&EDBG_COM, USART_ASYNC_ERROR_CB, err_cb_EDBG_COM);
	usart_async_enable(&EDBG_COM);

	while (1) {
		print("\r\n\r\n-----adc_control menu---\r\n");
		sprintf(buf, "1:set DAC OUT0(ZeroCross):  0x%04X\r\n", dac_out0);
		print(buf);
		sprintf(buf, "2:set DAC OUT1(LowerDescri):0x%04X\r\n", dac_out1);
		print(buf);
		sprintf(buf, "3:set DAC OUT2(UpperDescri):0x%04X\r\n", dac_out2);
		print(buf);
		if( start == 1 ){
			print( "4:*start\r\n");
			print( "5:stop\r\n" );
		}else{
			print( "4:start\r\n");
			print( "5:*stop\r\n" );
		}
		print( "6:Data Read\r\n");
		print( "7:Reset\r\n");
		buf[0] = 0 ;
		print( "> ");
		get_str(buf);
		switch( buf[0] ){
		case '1':
			if( start ){
				print("must when stop");
				continue ;
			}
			sscanf( buf, "%d %x", &i, &dac_out0);
			break ;
		case '2':
			if( start ){
				print("must when stop");
				continue ;
			}
			sscanf( buf, "%d %x", &i, &dac_out1);
			break ;
		case '3':
			if( start ){
				print("must when stop");
				continue ;
			}
			sscanf( buf, "%d %x", &i, &dac_out2);
			break ;
		case '4':
			start = 1; 
			main_proc_start();
			start = 0;
			break ;
		case '5':
			start = 0; 
			break ;
		case '6':
			if( start ){
				print("must when stop");
				continue ;
			}
			dump_data();
		case '7':
			if( start ){
				print("must when stop");
				continue ;
			}
			adc_count = 0 ;
			memset(adc_data, 0, sizeof(int) * DATA_NUM);
			lower_discri_count = 0 ;
			zero_cross_count = 0 ;
			upper_discri_count = 0 ;
			memset(upper_discri_time, 0, sizeof(int) * DATA_NUM);
			break ;
		}
	}
}
