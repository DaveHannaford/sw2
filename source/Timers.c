//***********************************************************************************************************************
//  © 2014 Kane International Ltd.                                                                                      *
//  INSTRUMENT  :KANE720 CO2 Leak Detector                                                                              *
//  FILE        :Timer.c                                                                                                *
//  DESCRIPTION :Controlling ADC interface                                                                              *
//  CREATED DATE:12/05/2014                                                                                             *
//  CPU TYPE    :PIC24FJ32GA002                                                                                         *
//  COMPILER    :MPLAB X16  v1.21                                                                                       *
//  IDE         :MPLAB X IDE v2.05                                                                                      *
//----------------------------------------------------------------------------------------------------------------------*
//      REVISION HISTORY                                                                                                *
//----------------------------------------------------------------------------------------------------------------------*
//  CHANGE NOTE :First alpha
//  MODIFIER    :Dave Hannaford
//  VERSION     :0.1
//  DATE        :12 June 2014
//  COMMENTS    :See Main.C for details.
//***********************************************************************************************************************

//*****************************Directives********************************************************************************
#include <xc.h>
#include "..\header\Definitions.h"                          // Global definitions
#include "..\header\Structures.h"                           // structure variables definitions
//***********************************************************************************************************************
//**************************Variables************************************************************************************
//**************************Function protypes****************************************************************************
void InitTimers(void);
void ProcessTenmsTimer(void);
//*************************External Function Prototypes******************************************************************
//-------------------------------------ADC.C---------------------------------------------
//-------------------------------------LED Display.C---------------------------------------------
extern void LedDriveUpdate(void);
//**************************External variables***************************************************************************
extern struct InstrumentDisplay Led;
extern struct InstrumentTimers Timers;
extern struct InstrumentParameters Inst;
extern struct InstrumentMeasurements Meas;
//***********************************************************************************************************************
//=======================================================================================================================
// Function:    InitTimers
// Description: This part has 5 16bit timers...see datasheet for info.  Tcy =Fosc/2 = 8MHz.
//=======================================================================================================================
void InitTimers (void)
{
                                                                    //**** 500uS timer ****
        TMR1=0;                                                     //reset counter
        PR1=4000;                                                   //timer 1 compare value (8MHz/1/4000=2KHz) (tuned on scope)
        T1CONbits.TCKPS=0;                                          //set timer prescaler to 1:1
        T1CONbits.TON = True;                                       //enable timer 1
        IPC0|=0x3000;                                               //timer 1 priority level 3
        IEC0bits.T1IE=False;                                        //disable timer 1 interrupt - wait for key press

        TMR2=0;                                                     //reset counter
        PR2=0x0200;                                                 //timer 2 compare value (8MHz/1/512=15.625KHz) (tuned on scope)
        T2CONbits.TCKPS=0;                                          //set timer prescaler to 1:1
        T2CONbits.TON = True;                                       //enable timer 2
    //  IPC1|=0x5000;                                               //timer 2 priority level 4
    //  IEC0bits.T2IE=True;                                         //enable timer 2 interrupt

        TMR3=0;                                                     //reset counter
        PR3=0x0200;                                                 //timer 3 compare value (8MHz/1/512=15.625KHz) (tuned on scope)
        T3CONbits.TCKPS0=0;                                          //set timer prescaler to 1:1
        T3CONbits.TCKPS1=0;
        T3CONbits.TON = True;                                       //enable timer 3
        T3CONbits.TSIDL=1;
        IPC2|=0x0001;                                               //timer 3 priority level 1
        IEC0bits.T3IE=True;                                         //enable timer 3 interrupt

                                                                    //**** 5mS timer ****
        TMR4=0;                                                     //reset counter
        PR4=10000;                                                  //timer 4 compare value (8MHz/8/10000=100Hz) (tuned on scope)
        T4CONbits.TCKPS=1;                                          //set timer prescaler to 1:8
        T4CONbits.TON = True;                                       //enable timer 4
        IPC6|=0x5000;                                               //timer 4 priority level 5 - highest
        IEC1bits.T4IE=True;                                         //enable timer 4 interrupt

                                                                    //**** 1S timer ****
        TMR5=0;                                                     //reset counter
        PR5=31250;                                                  //timer 5 compare value (8MHz/256/32250=1Hz) (tuned on scope)
        T5CONbits.TCKPS=3;                                          //set timer prescaler to 1:256
        T5CONbits.TON = True;                                       //enable timer 5
        IPC7|=0x0001;                                               //timer 5 priority level 1
        IEC1bits.T5IE=True;                                         //enable timer 5 interrupt
    return;
}
//=======================================================================================================================
//  Function:       ProcessTenmsTimer
//  Description:    process all the 10ms timer dependent timers
//=======================================================================================================================
void ProcessTenmsTimer(void)
{
    if(Inst.bTenmsTimerFlag==True)                                          //a 10ms interrupt has occurred
    {
        Inst.bTenmsTimerFlag=False;                                         //clear the flag
        ClrWdt();                                                           //wdt is setup in configure... 8 sec

        //the Zero CO2 key has been pressed and any previous keys have been actioned
        if(pbZeroCo2Input==False && Inst.bZeroCo2KeyPress==False && Inst.uiInstStatus==(unsigned int)NormalOperation)
        {
            if(Inst.ucZeroCo2KeyDebounceCount!=0)
                Inst.ucZeroCo2KeyDebounceCount--;
            else
            {
                Inst.bZeroCo2KeyPress=True;                                 //have the key press debounced
            }
        }
        else
        {
            Inst.ucZeroCo2KeyDebounceCount=(unsigned char)Debounce50ms;     //initialise a 50ms debounce Zero CO2 key press
        }

        if(Timers.ucLampFlash10ms!=0)                                       //ten millisecond timer to determine next lamp flash
        {
            Timers.ucLampFlash10ms--;
        }

        if(Timers.ucRefreshA2D10ms!=0)                                      //10ms counter used to update all the analogue measurements
        {
            Timers.ucRefreshA2D10ms--;
        }

        if(Timers.ucRefreshInitialise10ms!=0)                               //10ms counter for initialising time at power up
        {
            Timers.ucRefreshInitialise10ms--;
        }

        if(Timers.ucRefreshDisplay10mS!=0)                                  //display refresh timer
        {
            Timers.ucRefreshDisplay10mS--;
        }

        if(Timers.ucRefreshCo2Data10ms!=0)                                  //this timer is to update the CO2 data
        {
            Timers.ucRefreshCo2Data10ms--;
        }

        if(Timers.ucRefreshBatteryStatus10ms!=0)                            //This Timer Updates The Battery Data And Status
        {
            Timers.ucRefreshBatteryStatus10ms--;
        }

        if(Timers.ucRefreshProcessAllMeasurements10ms!=0)                   //Timer To Update All Calculated Data From Raw ADC
        {
            Timers.ucRefreshProcessAllMeasurements10ms--;
        }

        if(Timers.ucRefreshThermistorTemperature10ms!=0)                    //timer to update the pcb temperature using sm0603 thermistor
        {
            Timers.ucRefreshThermistorTemperature10ms--;
        }

        if(Timers.ucRefreshPumpCurrent10ms!=0)                              //timer used to update the pump current measurement and averaging
        {
            Timers.ucRefreshPumpCurrent10ms--;
        }

        if(Timers.ucRefreshPumpSpeed10ms!=0)                                //timer to update the pump speed setting from the measured data
        {
            Timers.ucRefreshPumpSpeed10ms--;
        }

        if(Timers.ucAlarmBuzzDelay10ms!=0)                                  //timer for delay in alarm buzzer
        {
            Timers.ucAlarmBuzzDelay10ms--;
        }


        if(Timers.uiBeepCounter10mS!=0)
        {
            Timers.uiBeepCounter10mS--;
        }
        else if(Inst.ucNoOfBeeps!=0)
        {
            Timers.uiBeepCounter10mS=Inst.uiCounterbewtweenBeeps10mS;       //load counter
            Timers.ucBuzzer500uS=Inst.ucBuzzer500uS;                        //load buzzer
            Inst.ucNoOfBeeps--;                                             //done one beep
            IEC0bits.T1IE=True;                                             //enable timer 1 interrupt for buzzer
        }
    }
    return;
}
//=======================================================================================================================
// Function:    _ISR _T1Interrupt
// Description: ISR called once every 500uS. (IRQ3)
//=======================================================================================================================
void _ISR720 _T1Interrupt(void)
{
#ifdef DAVE
    pbSpeakerControl= ~pbSpeakerControl;
#else

    if(Timers.ucBuzzer500uS!=0)
    {
        Timers.ucBuzzer500uS--;                     //buzzer on
        pbSpeakerControl= ~pbSpeakerControl;
    }
    else
    {
        pbSpeakerControl=Low;                       //ensure the drive to the speaker is off..minimum current
        IEC0bits.T1IE=False;                        //disable timer 1 interrupt
    }
#endif
    IFS0bits.T1IF=False;                            //reset interrupt
    return;
}
//=======================================================================================================================
// Function:    _ISR _T2Interrupt
// Description: ISR called once every 62.5uS. (IRQ7)
//              Control the pump speed by driving with a pwm signal, 16 times the period of this interrupt.
//=======================================================================================================================
void _ISR720 _T2Interrupt(void)
{
    (Inst.iPumpOutputPwmMask & Inst.iPumpOutputPwm) ? (pbPumpControl=True) : (pbPumpControl=False); //turn pump on if bit set
    Inst.iPumpOutputPwmMask=Inst.iPumpOutputPwmMask<<1;                                             //move the bit mask left
    if(Inst.iPumpOutputPwmMask==0x0000)                                                             //when zero
        Inst.iPumpOutputPwmMask|=0x0001;                                                            //set the least significant bit

    IFS0bits.T2IF=False;                                                                            //reset interrupt
    return;
}
//=======================================================================================================================
// Function:    _ISR _T3Interrupt
// Description: ISR called once every 62.5uS. (IRQ8)
//=======================================================================================================================
#ifdef DAVE_WANTS
void _ISR720 _T3Interrupt(void)
{
    LedDriveUpdate();
    IFS0bits.T3IF=False;                                        //reset interrupt
    return;
}
#endif
//=======================================================================================================================
// Function:    _ISR _T4Interrupt
// Description: ISR called once every 10mS. (IRQ27)
//=======================================================================================================================
void _ISR720 _T4Interrupt(void)
{
    Inst.bTenmsTimerFlag=True;              //flag a 10ms interrupt has occurred
    IFS1bits.T4IF=False;                    //reset interrupt
    return;                                 //exit
}
//=======================================================================================================================
// Function:    _ISR _T5Interrupt
// Description: ISR called once every 1S.
//=======================================================================================================================
void _ISR720 _T5Interrupt(void)
{
    Inst.bISRok=True;

    Inst.bBatteryFlasher=~Inst.bBatteryFlasher;                 //toggle the every time
    Inst.bAlarmFlasher=~Inst.bAlarmFlasher;                     //toggle the alarm led every time

    if(Timers.uiAirZeroS!=0)                                    //update air zero
    {
        Timers.uiAirZeroS--;
        if(Inst.uiInstStatus==AirZero)                          //only use beeper on air zero status
        {
            Timers.ucBuzzer500uS=BuzzerOn30mS;                  //load buzzer timer
            IEC0bits.T1IE=True;                                 //enable timer 1 interrupt for buzzer - debug
        }
    }

    IFS1bits.T5IF=False;                                        //reset interrupt

    return;
}

