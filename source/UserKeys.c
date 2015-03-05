//***********************************************************************************************************************
// 	© 2014 Kane International Ltd.																						*
//  INSTRUMENT      :KANE720 Leak Detector																				*
//	FILE            :UserKeys.c                                        													*
//	DESCRIPTION     :All activity to do with user switches				             									*
//	CREATED DATE    :12/05/2014																							*
//	CPU TYPE        :PIC24FJ32GA002																						*
//	COMPILER		:MPLAB X16	v1.21																					*
//	IDE				:MPLAB X IDE v2.05																					*
//----------------------------------------------------------------------------------------------------------------------*
//		REVISION HISTORY																								*
//----------------------------------------------------------------------------------------------------------------------*
//	CHANGE NOTE	:First alpha
//	MODIFIER	:Dave Hannaford
//  VERSION		:0.1
//	DATE		:01 October 2014
//	COMMENTS	:This is the beginnings of the source code for the Kane720 CO2 sniffer.
//***********************************************************************************************************************
//*****************************Directives********************************************************************************
#include <xc.h>
#include "..\header\Definitions.h"							// global definitions
#include "..\header\Structures.h"							// structure variables definitions
//***********************************************************************************************************************
//**************************Variables************************************************************************************
//**************************Function Prototypes**************************************************************************
void InitUserKeys(unsigned char ucSleepMode);
//**************************External Function Prototypes*****************************************************************
//**********LED Display.c*************************
//************Timer.c***********
//*************Measures.c**************************
//*********ADC.c************
//**************************External variables***************************************************************************
extern struct InstrumentDisplay Led;
extern struct InstrumentTimers Timers;
extern struct InstrumentParameters Inst;
extern struct InstrumentMeasurements Meas;
//=======================================================================================================================
// Function:	InitUserKeys
// Description:	This part sets the 8 bits of change notification up as interrupts.
//				Modified from Kane700. The only active key is the ZERO CO2 key
//=======================================================================================================================
void InitUserKeys(unsigned char ucSleepMode)
{
    CNEN1=0x0000;							//CN15- 0 - disable all
    CNEN2=0x0000;							//CN31-16 - disable all

    if(ucSleepMode==True)
    {
	CNPU1=0x0000;							//CN15- 0 - disable all pullups
    	CNPU2=0x0000;							//CN31-16 - disable all pullups
    }
    else
    {
	CNPU1=0x0002;							//CN15- 0 - disable all pullups except the switch pins ZERO CO2
    	CNPU2=0x0000;							//CN31-16 - disable all pullups
    }

    IPC4&=~0x7000;								//Change notification priority level==0 disables the interrupt source
    IFS1bits.CNIF=False;						//clear change notification interrupt
    IEC1bits.CNIE=False;						//disable change notification interrupt

    Inst.ucZeroCo2KeyDebounceCount=(unsigned char)Debounce50ms;	//initialise a 50ms debounce Zero CO2 key press
    Inst.bZeroCo2KeyPress=False;				//
    return;
}
//=======================================================================================================================
// Function:	_ISR _CNInterrupt()
// Description:	Change notification interrupt (IRQ19)
//				The only active key is the ZERO CO2 key..CN1..key will be polled in main loop.this int is not active
//=======================================================================================================================
void _ISR720 _CNInterrupt(void)
{
    IFS1bits.CNIF=False;						//clear change notification interrupt
    return;
}
