//***********************************************************************************************************************
//  © 2014 Kane International Ltd.                                                                                      *
//  INSTRUMENT   :KANE720 CO2 Leak Detector                                                                             *
//  FILE         :LED Display.c                                                                                         *
//  DESCRIPTION  :Source file for LED bar graph, alarm and battery LEDs                                                 *
//  CREATED DATE :12/05/2014                                                                                            *
//  CPU TYPE     :PIC24FJ32GA002                                                                                        *
//  COMPILER     :MPLAB X16  v1.21                                                                                      *
//  IDE          :MPLAB X IDE v2.05                                                                                     *
//----------------------------------------------------------------------------------------------------------------------*
//      REVISION HISTORY                                                                                                *
//----------------------------------------------------------------------------------------------------------------------*
//  CHANGE NOTE :First alpha
//  MODIFIER    :Dave Hannaford
//  VERSION     :0.1
//  DATE        :12 June 2014
//  COMMENTS    :See Main.C for details.
//-----------------------------------------------------------------------------------------------------------------------
//  CHANGE NOTE : ECN6303
//  MODIFIER    : Dave Hannaford
//  VERSION     :0.11
//  DATE        :11th February 2015
//  COMMENTS    :LED DRIVE CHANGED TO INCREASE BRIGHTNESS...see main.c for full details
//-----------------------------------------------------------------------------------------------------------------------
//  CHANGE NOTE : SAVE FILES TO GIT
//-----------------------------------------------------------------------------------------------------------------------
//***********************************************************************************************************************
//************************************Directives*************************************************************************
#include <xc.h>
#include <ctype.h>                                      // used for character manipulation
#include <stdio.h>                                      // used in sprintf statements
#include <stdlib.h>
#include <math.h>                                       // math routines are used in gas calculations
#include <string.h>                                     // string operators are used in Display/RS232 routines
#include "..\header\Definitions.h"                      // definitions for all files
#include "..\header\Structures.h"                       // structures containing most variables.
//***********************************************************************************************************************
//**************************Function protypes****************************************************************************
void InitLedDisplay(void);
void UpdateLedBarGraph(void);
void RefreshLedDisplay(void);
//***********************************Variables***************************************************************************
static unsigned char ucBits;
unsigned int uiPumpOutMask;
//**************************External functions***************************************************************************
//**************************External variables***************************************************************************
extern struct InstrumentDisplay Led;
extern struct InstrumentTimers Timers;
extern struct InstrumentParameters Inst;
extern struct InstrumentMeasurements Meas;
//=======================================================================================================================
// Function:    InitLedDisplay
// Description: Sets up display for 6 LEDs for display & 1 LED for battery & one LED for Alarm
//              Turns all LEDs off
//=======================================================================================================================
void InitLedDisplay(void)
{
#ifdef NEW_LED_DRIVE
    pbLedDr1=Low;
#else
    pbLedDr1=Low;
    pbLedDrive1=Low;                //physically set the drive lines for the LED bargraph to all off inc alarm and battery state
    pbLedDrive2=Low;
    pbLedDrive3=Low;

    pbLedCom1=Low;
    pbLedCom2=Low;
    pbLedCom3=Low;
#endif

    Led.ucLedPattern=0x00;          //All Leds Off
    return;
}
//=======================================================================================================================
// Function:    _ISR _T3Interrupt
// Description: ISR called once every 62.5uS. (IRQ8)
//              Updates the LEDs by pulsing each for an interrupt period in turn as they cannot be turned on statically
//=======================================================================================================================
void _ISR720 _T3Interrupt(void)
{
    //======================================================================
    //drive the pump from here
    if(uiPumpOutMask==0)
    {
        uiPumpOutMask=0x0001;
    }
    (uiPumpOutMask&Inst.iPumpOutputPwm)?(pbPumpControl=1):(pbPumpControl=0);
    uiPumpOutMask=uiPumpOutMask<<1;
    //=======================================================================
    pbLedDrive1=Low;            //physically set the drive lines for the LED bargraph to all off inc alarm and battery state
    pbLedDrive2=Low;
    pbLedDrive3=Low;

    pbLedCom1=Low;
    pbLedCom2=Low;
    pbLedCom3=Low;

    if(ucBits==0)               //this is the bit pattern template
        ucBits=0x01;

    switch(ucBits&Led.ucLedPattern)
    {
        case Led6:              //Just Led6 is turned on 0x01
        pbLedCom2=High;
        pbLedCom3=High;
        pbLedDrive3=High;
        break;

        case Led9:              //0x02
        pbLedDrive3=High;
        pbLedCom1=High;
        break;

        case Led1:              //0x04
        pbLedCom1=High;
        pbLedCom2=High;
        pbLedDrive2=High;
        break;

        case Led2:              //0x08
        pbLedCom1=High;
        pbLedCom3=High;
        pbLedDrive2=High;
        break;

        case Led5:              //0x10
        pbLedCom2=High;
        pbLedCom3=High;
        pbLedDrive2=High;
        break;

        case Led4:              //0x20
        pbLedCom1=High;
        pbLedCom2=High;
        pbLedDrive1=High;
        break;

        case Led7:              //0x40
        pbLedCom1=High;
        pbLedCom3=High;
        pbLedDrive1=High;
        pbLedDrive3=High;
        break;

        case Led8:              //0x80
        pbLedCom2=High;
        pbLedCom3=High;
        pbLedDrive1=High;
    }
    ucBits=ucBits<<1;           //next led on next interrupt

    IFS0bits.T3IF=False;        //reset interrupt
    return;
}
//=======================================================================================================================
// Function:    UpdateLedBarGraph
// Description: Sets up display for 6 LEDs for display on 6 LEDs..has a linear scale in 32 divisions
//=======================================================================================================================
const long lCO2divTable[32]=
{
    250,        //250ppm per LED...0
    403,        //1
    556,        //2
    710,        //3
    863,        //4
    1016,       //5
    1169,       //6
    1323,       //7
    1476,       //8
    1629,       //9
    1782,       //10
    1935,       //11
    2089,       //12
    2242,       //13
    2395,       //14
    2548,       //15
    2702,       //16
    2855,       //17
    3008,       //18
    3161,       //19
    3315,       //20
    3468,       //21
    3621,       //22
    3774,       //23
    3927,       //24
    4081,       //25
    4234,       //26
    4387,       //27
    4540,       //28
    4694,       //29
    4847,       //30
    5000        //31
};
//
void UpdateLedBarGraph(void)
{
    unsigned char ucLedPattern;
    long lCalc;
    int iTemp;

    if(Inst.uiInstStatus==(unsigned int)InitialisingBeep)           //during the start up bleeping turn all leds on
    {
        ucLedPattern=0xff;
    }
    else if(Inst.uiInstStatus==(unsigned int)PoweringUp)
    {
        Led.ucIndex=0x07;
    }
    else if(Inst.uiInstStatus==(unsigned int)AirZero)               //a phase when the CO2 system is zeroed
    {
        if(Led.ucIndex&0x04)                                        //every fourth visit here light alternate Leds in stack
        {
            ucLedPattern=Led1+Led5+Led7;
        }
        else
        {
            ucLedPattern=Led2+Led4+Led8;
        }
        if(Led.ucIndex)
        {
            Led.ucIndex--;
        }
        else
        {
            Led.ucIndex=0x07;
        }
    }
    else if(Inst.uiInstStatus==(unsigned int)AirZeroCheck)
    {
        if(Led.ucIndex&0x04)                                        //every fourth visit here light alternate Leds in stack
        {
            ucLedPattern=Led1+Led5+Led2;                            //change the pattern to bottom 3 Leds then top 3
        }
        else
        {
            ucLedPattern=Led4+Led7+Led8;
        }
        if(Led.ucIndex)
        {
            Led.ucIndex--;
        }
        else
        {
            Led.ucIndex=0x07;
        }
    }
    else if(Inst.uiInstStatus==(unsigned int)InSleepMode)
    {
        ucLedPattern=0x00;                                  //turn leds off
    }
    else    //Update The Leds Normally
    {
        /*Inst.lCo2Ppm holds the co2 value in ppm*/
        /*Meas.iThumbWheelReading holds the pot setting..the smaller the number the more clockwise the dial is*/
        iTemp=Meas.iThumbWheelReading;
        iTemp&=0x01ff;
        iTemp=iTemp>>5;                             //32 distinct bands
        iTemp&=0x001f;

        lCalc=Inst.lCo2Ppm/lCO2divTable[iTemp];     //a number 0 to 6
        if(lCalc>6)                                 //maximum allowed value
        {
            lCalc=6;
        }

        Inst.bAlarmSound=False;
        switch(lCalc)
        {
            case 6:
            ucLedPattern=Led8+Led7+Led4+Led5+Led2+Led1;            //set all bargraph leds on
            Inst.bAlarmSound=True;
            if(Inst.bAlarmFlasher)                                  //this bit is toggled in Timers.c
            {
                ucLedPattern|=Led6;
            }
            break;

            case 5:
            ucLedPattern=Led6+Led8+Led7+Led4+Led5+Led2;             //set lower 5 bargraph leds on..plus alarm
            break;

            case 4:
            ucLedPattern=Led8+Led7+Led4+Led5;                      //set lower 4 bargraph leds on
            break;

            case 3:
            ucLedPattern=Led8+Led7+Led4;
            break;

            case 2:
            ucLedPattern=Led8+Led7;
            break;

            case 1:
            ucLedPattern=Led8;                                     //set lower 1 bargraph leds on
            break;

            case 0:
            ucLedPattern=0x00;
        }

        //****show battery led as on until battery level falls****
        if(Meas.iBatteryLevelReading>(int)BatteryLow25Percent)
        {
            ucLedPattern|=Led9;
        }
        else if(Inst.bBatteryFlasher)                       //this bit is toggled in Timers.c
        {
            ucLedPattern|=Led9;
        }
    }

    Led.ucLedPattern=ucLedPattern;                          //copy the updated pattern to the control variable
}
//=======================================================================================================================
//  Function:       RefreshLedDisplay
//  Description:    On the expiry of the timer runs UpdateLedBarGraph() to refresh the data on the LEDs
//=======================================================================================================================
void RefreshLedDisplay(void)
{
    if(Timers.ucRefreshDisplay10mS==0)
    {
    Timers.ucRefreshDisplay10mS=(unsigned char)DisplayUpdate10ms;               //the period when next we come through here..currently 10 times a second
    UpdateLedBarGraph();                                                        //update LED Bar Graph
    }
    return;
}

