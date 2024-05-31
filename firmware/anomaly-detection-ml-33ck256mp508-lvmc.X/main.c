/*
© [2024] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.? 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
*/


#include <util.h>
#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdint.h>
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <stdio.h>
#include "ringbuffer.h"
#include <string.h>
#include <hal.h>
#include <libpic30.h>

#include "app_config.h"


#if STREAM_FORMAT_IS(NONE)

#include "knowledgepack/mplabml/inc/kb.h"
#include "knowledgepack/application/sml_recognition_run.h"
#endif 
// *****************************************************************************
// *****************************************************************************
// Section: Platform specific includes
// *****************************************************************************
// ***************************************************************************** 


#include "mcc_generated_files/motorBench/mcaf_main.h"
#include "mcc_generated_files/system/system.h"
#include "motorbench/system_state.h"
#include "motorbench/sat_PI.h"
#include "motorbench/filter.h"
#include "mcaf_sample_application.h"



// *****************************************************************************
// *****************************************************************************
// Section: Global variables
// *****************************************************************************
// *****************************************************************************

static sample_data_t motor_buffer_data[BUF_LEN][NUM_AXES];
static ringbuffer_t data_sample_buffer;
static volatile bool data_sample_buffer_overrun = false;

extern MCAF_MOTOR_DATA motor;
extern APPLICATION_DATA app;


uint16_t rpm=0;
volatile int8_t flag_read =0;
int8_t flag_print=1;



void sensor_read( sample_data_t *ptr, MCAF_MOTOR_DATA *pmotor, APPLICATION_DATA *appdata);


// *****************************************************************************
// Section: Platform specific stub definitions
// *****************************************************************************
// *****************************************************************************


bool USART_Write( void *buffer, const size_t size )
{
    bool writeStatus      = false;
    uint8_t *pu8Data      = (uint8_t*)buffer;
    uint32_t u32Index     = 0U;

    if(buffer != NULL)
    {
        /* Blocks while buffer is being transferred */
        while(u32Index < size)
        {
            /* Check if USART is ready for new data */
            while(U1STAHbits.UTXBF == 1)
             {
        
             }
            /* Write data to USART module */
              U1TXREG = pu8Data[u32Index];;    // Write the data byte to the USART.
            /* Increment index */
            u32Index++;
        }
        writeStatus = true;
    }

    return writeStatus;
}



size_t __attribute__(( unused )) UART_Write(uint8_t *ptr, const size_t nbytes) {
    return USART_Write(ptr, nbytes) ? nbytes : 0;
}

// *****************************************************************************
// *****************************************************************************
// Section: Generic stub definitions
// *****************************************************************************
// *****************************************************************************
void Null_Handler() {
    // Do nothing
}



// RPM measurement 
uint16_t speed_measurement_local(APPLICATION_DATA *appData)
{
    rpm = (float)(motor.apiData.velocityMeasured) / RPM_CONVERSION_FACTOR; 
    return rpm;
}

// For handling read of the motor IQ data
 void Read_iq_value() 
{
    ringbuffer_size_t wrcnt;
   
  if (data_sample_buffer_overrun)
     return;
  
    
    sample_data_t *ptr = ringbuffer_get_write_buffer(&data_sample_buffer, &wrcnt);
    
    if (wrcnt == 0)
        data_sample_buffer_overrun = true;
    else 
    {
        sensor_read( ptr, &motor, &app );
       ringbuffer_advance_write_index(&data_sample_buffer, 1);
    }
}    

 
 //Writing IQ and RPM value to ring buffer
 
void sensor_read( sample_data_t *ptr, MCAF_MOTOR_DATA *pmotor, APPLICATION_DATA *appData)
{
    sample_data_t * l_data_sample_buffer = ptr;
  
  *l_data_sample_buffer++ = (sample_data_t)abs(pmotor->idq.q);
  *l_data_sample_buffer++ = (sample_data_t) speed_measurement_local(&app);
   

}


int main ( void )
{
    int8_t app_failed = 1;

   /* Initialize all modules */
    SYSTEM_Initialize ();
    MCAF_MainInit();
 
    
    while (1)
    {
   
        /* Initialize the sensor data buffer */
        if (ringbuffer_init(&data_sample_buffer, motor_buffer_data, sizeof(motor_buffer_data) / sizeof(motor_buffer_data[0]), sizeof(motor_buffer_data[0])))
            break;
    
        //Checks for motor operational faults
         if(motor.state == MCSM_FAULT)
        {
            printf("\r\033[K  ERROR: Got a bad motor status");
            break;
        }

#if STREAM_FORMAT_IS(NONE)        
      
        /* Initialize MPLABML Knowledge Pack */
        
        kb_model_init();
        
        const uint8_t *ptr = kb_get_model_uuid_ptr(0);
        printf("\n");
        printf("    Running MPLABML Anomaly Detection Demo \r\n\n ");
        printf(" Knowledge pack uuid: ");
        printf("%02x", *ptr++); 
        for (int i=1; i < 15; i++) {
            if ((i%4) == 0)
                printf("-");
            printf("%02x", *ptr++); 
        }
        printf("%02x", *ptr++); 
        printf("\r\n\n");
        printf("*********************************************************** \r\n");
        printf("\r\n\n");        
#endif

        app_failed = 0;
        break;
    }
    
#if STREAM_FORMAT_IS(NONE)
    int clsid = -1;
#endif
    
    while (!app_failed)
    {
        
        MCAF_MainLoop();
              
        if (flag_read == 1)
        {
            Read_iq_value();
            flag_read = 0;
        }
        
        if (data_sample_buffer_overrun == true) {
            printf("\n\n\nOverrun!\n\n\n");
            ringbuffer_reset(&data_sample_buffer);
            data_sample_buffer_overrun = false;
     
            continue;
        }
#if !STREAM_FORMAT_IS(NONE)
        else if(ringbuffer_get_read_items(&data_sample_buffer) >= SAMPLES_PER_PACKET) {
            ringbuffer_size_t rdcnt;
            sample_dataframe_t const *ptr = ringbuffer_get_read_buffer(&data_sample_buffer, &rdcnt);
            while (rdcnt >= SAMPLES_PER_PACKET) {
    #if STREAM_FORMAT_IS(ASCII)
                sample_data_t const *scalarptr = (sample_data_t const *) ptr;
                printf("%d", *scalarptr++);
                for (int j=1; j < sizeof(sample_datapacket_t) / sizeof(sample_data_t); j++) {
                    printf(" %d", *scalarptr++);
                }
                printf("\r\n");
    #elif STREAM_FORMAT_IS(MDV)
                uint8_t headerbyte = MDV_START_OF_FRAME;
                UART_Write(&headerbyte, 1);
                UART_Write((uint8_t *) ptr, sizeof(sample_datapacket_t));
                headerbyte = ~headerbyte;
                UART_Write(&headerbyte, 1);
    
    #endif //STREAM_FORMAT_IS(ASCII)
                ptr += SAMPLES_PER_PACKET;
                rdcnt -= SAMPLES_PER_PACKET;
                ringbuffer_advance_read_index(&data_sample_buffer, SAMPLES_PER_PACKET);
            }
        }
#else   
        else {
            
            if (motor.state == MCSM_RUNNING )
            {
            
                ringbuffer_size_t rdcnt;
                sample_dataframe_t const *ptr = ringbuffer_get_read_buffer(&data_sample_buffer, &rdcnt);
                 while (rdcnt--) 
                {
                   int ret = sml_recognition_run((sample_data_t *) ptr++, NUM_AXES);
                    ringbuffer_advance_read_index(&data_sample_buffer, 1);
                    __delay_us(500);


                    /*Class ID
                      1 - Normal_operation
                      2 - Unbalanced_load
                      0 - unknown*/

                    if (ret >=0 && ret != clsid)
                    {
                        clsid = ret;
                        
                        if (clsid == 1 )
                            printf("\r\033[K   Normal_Operation");
                        else if (clsid == 2) 
                            printf("\r\033[K   Unbalanced_load");
                        else if (clsid == 0) {
                            printf("\r\033[K   Unknown");
                        }
                     
                    }

                }
                flag_print =1;
            }
        
            else
            {
                if(motor.state == MCSM_STOPPED && flag_print == 1)
                {
                printf("\r\033[K   Motor is in idle state! ");
                flag_print=0;
                clsid = -1;
                }
                
                ringbuffer_reset(&data_sample_buffer);
                
            }
        }
#endif //!STREAM_FORMAT_IS(NONE)



    }

    
    /* Loop forever on error */
    while (1) {};

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/
