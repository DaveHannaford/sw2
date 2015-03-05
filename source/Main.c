//***********************************************************************************************************************
//      © 2014 Kane International Ltd.                                                                                  *
//      INSTRUMENT  :KANE720 Leak Detector                                                                              *
//      FILE        :Main.c                                                                                             *
//      DESCRIPTION :Main source file                                                                                   *
//      CREATED DATE:12/05/2014                                                                                         *
//      CPU TYPE    :PIC24FJ32GA002                                                                                     *
//      COMPILER    :MPLAB X16  v1.21                                                                                   *
//      IDE         :MPLAB X IDE v2.05                                                                                  *
//----------------------------------------------------------------------------------------------------------------------*
//      REVISION HISTORY                                                                                                *
//----------------------------------------------------------------------------------------------------------------------*
//      CHANGE NOTE :First alpha
//      MODIFIER    :Dave Hannaford
//      VERSION     :0.1
//      DATE        :1st October 2014
//      COMMENTS    :This is the beginnings of the source code for the Kane720 CO2 sniffer.
//
//      CHANGE NOTE :Second alpha
//      MODIFIER    :Dave Hannaford
//      VERSION     :0.1
//      DATE        :23rd October 2014
//      COMMENTS    :This version has 250ppm to 5000ppm per led sensitivity.
//                  Has a 30 second warm up time.
//                  External zero switch not active, little point.
//                  Pot adjusts the senitivity linearly around 150ppm per segment, of which there are 32 so about 9degrees
//                  rotation per segment.
//
//      CHANGE NOTE:Add to Git for testing(going poorly).
//
//***********************************************************************************************************************
//************************************Directives*************************************************************************
#include "..\header\Main.h"
#include "..\header\config.h"
//**************************Function prototypes**************************************************************************
//**************************External functions***************************************************************************
//**************************External variables***************************************************************************
//**************************Variables************************************************************************************
//********define the instrument status modes***********
void (*const MainOpStatus[])(void)=
{
    InitialisingStatus,                 //0
    InitialisingBeepStatus,             //1
    PoweringUpStatus,                   //2
    AirZeroStatus,                      //3
    AirZeroCheckStatus,                 //4
    NormalOperationStatus,              //5
    NormalOperationBeepStatus,          //6
    PowerDownStatus,                    //7
    PowerDownBeepStatus,                //8
    InSleepModeStatus,                  //9
    FaultModeStatus,                    //10
    LowBatAtStartUpStatus,              //11
    LowBatInNormOpStatus,               //12
    TemperatureSensorFaultModeStatus,   //13
    Co2SensorFaultModeStatus            //14
};
//***********define the states where measurements needed*****
void (*const MainProcessData[])(void)=
{
    NoMeasurements,                 //0
    NoMeasurements,                 //1
    RefreshAllMeasurements,         //2
    RefreshAllMeasurements,         //3
    RefreshAllMeasurements,         //4
    RefreshAllMeasurements,         //5
    RefreshAllMeasurements,         //6
    NoMeasurements,                 //7
    NoMeasurements,                 //8
    NoMeasurements,                 //9
    NoMeasurements,                 //10
    NoMeasurements,                 //11
    NoMeasurements,                 //12
    NoMeasurements,                 //13
    NoMeasurements                  //14
};
//=======================================================================================================================
void NoMeasurements(void)
{
    return;
}
//=======================================================================================================================
//
//  The Main Function
//  -----------------
//
int main(void)
{
    Initialise();                   //set up the processor ports etc
    InitTimers();                   //system timers
    Initialise10BitAdc();
    InitMeas();

    Inst.uiInstStatus=(unsigned int)Initialising;

    while(1)
    {
        ProcessTenmsTimer();                                                //the 10ms interrupt sets a flag and all 10ms dependent timer functions are dealt with here
        if(Timers.ucLampFlash10ms==0)                                       //ten millisecond timer to determine next lamp flash
        {
            Timers.ucLampFlash10ms=(unsigned char)LampFlashInterval10ms;    //currently 0.48sec interval
            if(Timers.ucLampStatus==(unsigned char)High)
            {
                Timers.ucLampStatus=(unsigned char)Low;
                pbFlash=Low;
            }
            else
            {
                Timers.ucLampStatus=(unsigned char)High;
                pbFlash=High;
            }
        }

        if(Timers.ucRefreshA2D10ms==0)                                      //A2D timer has expired...
        {
            GetInternalAdcCount();                                          //grab all the A2D channel data
            Timers.ucRefreshA2D10ms=(unsigned char)RefreshA2Ddata10ms;      //load the timer..every 60ms
        }

        //Process the measured data
        MainProcessData[Inst.uiInstStatus]();
        //Process the intrument state
        MainOpStatus[Inst.uiInstStatus]();
        //Update the LED display
        RefreshLedDisplay();
    }
    return(0x0000);
}
//=======================================================================================================================
// Function:    InitialisingStatus
// Description: Within the instrument operating status this intialises the system
//=======================================================================================================================
void InitialisingStatus(void)
{
    Inst.ucSystemErr=Ok;                                    //reset error counter
    InitUserKeys(False);                                    //set up all change notification keys
    InitCo2();
    InitLedDisplay();                                       //Turns all LEDs off
    Inst.iPumpOutputPwm=0x00ff;                             //50:50 duty cycle for pump speed
    //*****************************************************************
    //****Show Unit Alive by beeping buzzer****************************
    //*****************************************************************
    Inst.ucNoOfBeeps=(unsigned char)(StartingUp-Faults);    //10 beeps to say am alive
    Inst.uiCounterbewtweenBeeps10mS=BeepEvery500mS;         //every half second beeps
    Inst.ucBuzzer500uS=BuzzerOn100mS;                       //load buzzer timer..total time here 6secs
    Inst.uiInstStatus=(unsigned int)InitialisingBeep;
    Inst.uiLastInstStatus=(unsigned int)Initialising;
    return;
}
//=======================================================================================================================
// Function:    InitialisingBeepStatus
// Description:
//=======================================================================================================================
void InitialisingBeepStatus(void)
{
    if(Inst.ucNoOfBeeps==0)                                 //wait for beeps to finish...6secs
    {
        Inst.uiInstStatus=(unsigned int)PoweringUp;
        Inst.uiLastInstStatus=(unsigned int)InitialisingBeep;
    }
    return;
}
//=======================================================================================================================
// Function:    PoweringUpStatus
// Description: Test the battery status and if ok proceed to zero the CO2 measurement
//=======================================================================================================================
void PoweringUpStatus(void)
{
    //at this stage been up for 6 secs bleeping with pump running and measurements going on
    if(Meas.iBatteryLevelReading<((int)BatteryLow5Percent))     //!!!Emergency - unit battery lower than 5%- shut down!!!!
    {
        Inst.uiInstStatus=(unsigned int)LowBatAtStartUp;
        Inst.uiLastInstStatus=(unsigned int)PoweringUp;         //save last state
    }
    else                                                        //battery good for operation for a short time
    {
        Inst.uiInstStatus=(unsigned int)AirZero;                //air zero operation
        Inst.uiLastInstStatus=(unsigned int)PoweringUp;         //save last state
        Timers.uiAirZeroS=(unsigned int)AirZero15S;             //wait for warm up
        Inst.ucCo2BadZeroCount=(unsigned char)Co2BadZeroCount;  //the maximum number of times get to find a stable CO2 zero
    }
    return;
}
//=======================================================================================================================
// Function:    AirZeroStatus
// Description: Zero the CO2 system
//=======================================================================================================================
void AirZeroStatus(void)
{
    //---------------CHECK FOR FAULTS IN CO2 AND THERMISTOR DATA--------------------------
    if(Meas.fThermistorTemp==ThermistorMaxTemp || Meas.fThermistorTemp==ThermistorMinTemp)
    {
        Inst.uiInstStatus=(unsigned int)TemperatureSensorFaultMode; //faulty temperature sensor &or temperature measurement.....cannot go on
        return;
    }

    if(Meas.iCo2DataAv==(int)Co2HiA2dCount || Meas.iCo2DataAv==(int)Co2LowA2dCount)
    {
        Inst.uiInstStatus=(unsigned int)Co2SensorFaultMode;         //faulty CO2 sensor &/or CO2 measurement.....cannot go on
        return;
    }

    if(Timers.uiAirZeroS==0)                                        //once timer has expired calibrate CO2 zero...30seconds
    {
        if(Meas.iCo2DataAv>Co2LoA2dCountAtZero && Meas.iCo2DataAv<(int)Co2HiA2dCountAtZero) //need the measured data to be within limits
        {
            CalibrateCo2Zero();                                         //do a zero..save the current temperature and CO2 reading
            Inst.uiInstStatus=(unsigned int)AirZeroCheck;               //check the zero is stable for operation
            Inst.uiLastInstStatus=(unsigned int)AirZero;                //save last state
            Timers.uiAirZeroS=(unsigned int)AirZero15S;                 //re-zero if signal greater after first 15 seconds
        }
        else
        {
            Inst.uiInstStatus=(unsigned int)Co2SensorFaultMode;         //faulty CO2 sensor &/or CO2 measurement.....cannot go on
        }
    }
    return;
}
//=======================================================================================================================
// Function:    AirZeroCheckStatus
// Description:
//=======================================================================================================================
void AirZeroCheckStatus(void)
{
    /*This is belt and braces but just in case*/

    if(Meas.iCo2DataAv==(int)Co2HiA2dCount || Meas.iCo2DataAv==(int)Co2LowA2dCount)
    {
        Inst.uiInstStatus=(unsigned int)Co2SensorFaultMode;         //faulty CO2 sensor &or CO2 measurement.....cannot go on
        Inst.uiLastInstStatus=(unsigned int)AirZeroCheck;           //save last status
        return;
    }

    //--------------------------------------------------------------------------------------------------------------
    //------------------Check the air zero after the timer has expired to be certain it has not drifted-------------
    //--------------------------------------------------------------------------------------------------------------
    if(Timers.uiAirZeroS==0)
    {
        if(CalibrateCo2ZeroCheck()==(unsigned char)False)
        {
            if(Inst.ucCo2BadZeroCount!=0)
            {
                Inst.ucCo2BadZeroCount--;                   //decrement counter
                Timers.uiAirZeroS=AirZero15S;               //wait for warm up
                Inst.uiInstStatus=AirZero;                  //air zero operation
                Inst.uiLastInstStatus=AirZeroCheck;         //save last state
            }
            else
            {
                Inst.uiInstStatus=(unsigned int)Co2SensorFaultMode;     //faulty CO2 sensor &or CO2 measurement.....cannot go on
                Inst.uiLastInstStatus=(unsigned int)AirZeroCheck;       //save last status
            }
        }
        else                                                        //check of CO2 zero is good!
        {
            Inst.uiInstStatus=(unsigned int)NormalOperation;        //normal operation
            Inst.uiLastInstStatus=(unsigned int)AirZeroCheck;       //save last state
        }
    }
    return;
}
//=======================================================================================================================
// Function:    NormalOperationStatus
// Description: Normal measuring function
//=======================================================================================================================
void NormalOperationStatus(void)
{
    if(Meas.iBatteryLevelReading<BatteryLow1Percent)                //!!!Emergency - unit battery lower than 1%- shut down!!!!
    {
        Inst.uiInstStatus=LowBatInNormOp;
        Inst.uiLastInstStatus=(unsigned int)NormalOperation;
    }
    else if(Inst.bAlarmSound==True && Timers.ucAlarmBuzzDelay10ms==0)
    {
        Inst.ucNoOfBeeps=(unsigned char)(CO2AlarmState-Faults);     //5 beeps to say am in alarm
        Inst.uiCounterbewtweenBeeps10mS=BeepEvery500mS;             //every half second beeps
        Inst.ucBuzzer500uS=BuzzerOn100mS;                           //load buzzer timer
        Inst.uiInstStatus=(unsigned int)NormalOperationBeep;
        Inst.uiLastInstStatus=(unsigned int)NormalOperation;
    }
    return;
}
//=======================================================================================================================
// Function:    NormalOperationBeepStatus
// Description:
//=======================================================================================================================
void NormalOperationBeepStatus(void)
{
    if(Inst.ucNoOfBeeps==0)                                     //wait for beeps to finish
    {
        Inst.uiInstStatus=(unsigned int)NormalOperation;
        Inst.uiLastInstStatus=(unsigned int)NormalOperationBeep;
        Timers.ucAlarmBuzzDelay10ms=(unsigned char)AlarmBuzzDelay10ms;
    }
    return;
}
//=======================================================================================================================
// Function:    PowerDownBeepStatus
// Description:
//=======================================================================================================================
void PowerDownBeepStatus(void)
{
    if(Inst.ucNoOfBeeps==0)
    {
        Inst.uiInstStatus=(unsigned int)PowerDown;
        Inst.uiLastInstStatus=(unsigned int)PowerDownBeep;
    }
    return;
}
//=======================================================================================================================
// Function:    PowerDownStatus
// Description:
//=======================================================================================================================
void PowerDownStatus(void)                         //prepare to go into sleep mode
{
    InitUserKeys(True);                     //set up keys for sleep mode
    InitLedDisplay();                       //blank display
    Inst.uiInstStatus=(unsigned int)InSleepMode;
    Inst.uiLastInstStatus=(unsigned int)PowerDown;
    return;
}
//=======================================================================================================================
// Function:    LowBatInNormOpStatus
// Description:
//=======================================================================================================================
void LowBatInNormOpStatus(void)
{
    Inst.ucNoOfBeeps=(unsigned char)(BatteryFlat-Faults);           //3 beeps to say battery flat
    Inst.uiCounterbewtweenBeeps10mS=BeepEvery1S;                    //every second beeps
    Inst.ucBuzzer500uS=BuzzerOn100mS;                               //load buzzer timer
    Inst.uiInstStatus=(unsigned int)PowerDownBeep;                  //shut down
    Inst.uiLastInstStatus=(unsigned int)LowBatInNormOp;
    return;
}
//=======================================================================================================================
// Function:    FaultModeStatus
// Description:
//=======================================================================================================================
void FaultModeStatus(void)
{
    Inst.ucNoOfBeeps=(unsigned char)(BatteryFlat-Faults);       //3 beeps to say battery flat
    Inst.uiCounterbewtweenBeeps10mS=BeepEvery500mS;             //every second beeps
    Inst.ucBuzzer500uS=BuzzerOn100mS;                           //load buzzer timer
    Inst.uiInstStatus=(unsigned int)PowerDownBeep;              //shut down
    Inst.uiLastInstStatus=(unsigned int)LowBatAtStartUp;
    return;
}
void LowBatAtStartUpStatus(void)
{
    FaultModeStatus();
}
//=======================================================================================================================
// Function:    TemperatureSensorFaultModeStatus
// Description:
//=======================================================================================================================
void TemperatureSensorFaultModeStatus(void)
{
    Inst.ucNoOfBeeps=(unsigned char)(TemperatureSensorFault-Faults);
    Inst.uiCounterbewtweenBeeps10mS=BeepEvery500mS;             //every second beeps
    Inst.ucBuzzer500uS=BuzzerOn100mS;                           //load buzzer timer
    Inst.uiInstStatus=(unsigned int)PowerDownBeep;              //shut down
    Inst.uiLastInstStatus=(unsigned int)TemperatureSensorFaultMode;
    return;
}
//=======================================================================================================================
// Function:    Co2SensorFaultModeStatus
// Description:
//=======================================================================================================================
void Co2SensorFaultModeStatus(void)
{
    Inst.ucNoOfBeeps=(unsigned char)(Co2SensorFaultMode-Faults);
    Inst.uiCounterbewtweenBeeps10mS=BeepEvery500mS;             //every second beeps
    Inst.ucBuzzer500uS=BuzzerOn100mS;                           //load buzzer timer
    Inst.uiInstStatus=(unsigned int)PowerDownBeep;              //shut down
    Inst.uiLastInstStatus=(unsigned int)Co2SensorFaultMode;
    return;
}
//=======================================================================================================================
// Function:    InSleepModeStatus
// Description:
//=======================================================================================================================
void InSleepModeStatus(void)
{
    return;
}
//
//  Initialise The Proceesor Ports
//  ------------------------------
//
void Initialise(void)
{

    AD1PCFG=0xFFFF;                 //Pin for corresponding analogue channel is in digital mode
    RCON=0x000;                     //reset the reset register
    CLKDIV=0x1900;                  //Using PLL (16Mhz) - change CPU ratio to divide by 2 (8MHz)& leave FRC post scale to divide by 2 (4MHz)
    //PORT A
    pbA2dPosRefDataDir=Input;       //+Vref for the A2D
    pbA2dNegRefDataDir=Input;       //-Vref for the A2D
    TRISAbits.TRISA2=Input;         //bits 2 & 3 are used for the ext osc
    TRISAbits.TRISA3=Output;        //this is shared with OSC0 o/p of inv buffer
    TRISAbits.TRISA4=Output;        //bit 4 drives the lamp flash

    //PORT B
    pbPotInputDataDir=Input;        //the analogue input from the pump pot
    pbCo2InputDataDir=Input;        //the CO2 analogue input
    TRISBbits.TRISB2=Output;        //LED COM 2 o/p
    pbBatVoltsInputDataDir=Input;   //the analogue input from the batterysupply
                                    //pot down through R11 , 470k, and R10, 47k
                                    //supply is four AA cells, non rechargeable..6V nom
    pbZeroCo2InputDataDir=Input;    //this is a push button input so needs internal
                                    //pullup turned on
    TRISBbits.TRISB5=Input;         //programming port PGD3
    TRISBbits.TRISB6=Input;         //programming port PGC3
    TRISBbits.TRISB7=Output;        //LED COM 3 o/p
    TRISBbits.TRISB8=Output;        //LED DRIVE 3 o/p
    TRISBbits.TRISB9=Output;        //LED COM 1 o/p
    TRISBbits.TRISB10=Output;       //LED DRIVE 1 o/p
    TRISBbits.TRISB11=Output;       //LED DRIVE 2 o/p
    pbTemperatureInputDataDir=Input;    //analogue i/p temperature
    pbPumpCurrentInputDataDir=Input;    //analogue i/p pump current
    TRISBbits.TRISB14=Output;       //pump control o/p
    TRISBbits.TRISB15=Output;       //speaker control o/p

    pbSpeakerControl= ~pbSpeakerControl;
    pbSpeakerControl= ~pbSpeakerControl;
    Timers.uiBeepCounter10mS=(unsigned int)BeepEvery2S;

    CNPU1bits.CN0PUE=On;            //weak pull up..what tho??

    // set o/ps to appropriate levels
    pbPumpControl=On;               //this will be PWM o/p
    pbFlash=Low;                    //sets the lamp flash drive off
    pbSpeakerControl=Low;

    IEC0=0;             //Disable all interrupts - we'll enable the
    IEC1=0;     //one(s) we need later.
    IEC2=0;     //
    IEC3=0;     //
    IEC4=0;     //

}
//=======================================================================================================================
// Function:    InitMeas
// Description: This function initialises the structure InstrumentMeasurements and its only declared type 'Meas'
//              This is essential until the true measurements are running and updating data about the battery in particular
//              else the instrument will never power up
//=======================================================================================================================
void InitMeas(void)
{
    //---------Set Up The Initial Data For The Temperature Measurement----------------------------------------------------------
    Meas.fThermistorTemp=(float)ThermistorDefTemp;                                                                  //the current thermistor temperature in ºC
    for(Meas.ucThermTempIndex=0; Meas.ucThermTempIndex!=(unsigned char)ThermistorTempArray;Meas.ucThermTempIndex++)
    {
        Meas.fThermTempArray[Meas.ucThermTempIndex]=(float)ThermistorDefTemp;
    }
    Meas.ucThermTempIndex=0;

    //---------Set Up The Initial Data For The Battery Voltage Measurement-------------------------------------------------------
    Meas.iBatteryMilliVolts=(int)BatteryMilliVoltsMax;                                                                  //set up as though the batteries are new
    Meas.iBatteryLevelReading=(int)BatteryLevelDefaultPercent;                                                          //to tie in with a high battery voltage
    for(Meas.ucBattLevelIndex=0;Meas.ucBattLevelIndex!=(unsigned char)BattLevelReadingArrayIndex;Meas.ucBattLevelIndex++)
    {
        Meas.iBattLevelReadingArray[Meas.ucBattLevelIndex]=(int)BatteryLevelDefaultPercent;
    }
    Meas.ucBattLevelIndex=0;

    //---------Set Up The Initial Data For The CO2 Measurement----------------------------------------------------------------
    Meas.iCo2DataAv=(int)Co2MaxA2dCount;
    for(Meas.ucCo2DataIndex=0;Meas.ucCo2DataIndex!=(unsigned char)Co2DataArrayIndex;Meas.ucCo2DataIndex++)
    {
        Meas.iCo2DataArray[Meas.ucCo2DataIndex]=(int)Co2MaxA2dCount;
    }
    Meas.ucCo2DataIndex=0;                        //the index used to control data entry into the array

    //---------Set Up The Initial Data For The Pump Speed Potentiometer Measurement----------------------------------------------
    Meas.iThumbWheelReading=(int)ThumbWheelDefReading;      //thumb wheel pot level 0-100%
    for(Meas.ucThumbWheelReadingArrayIndex=0;Meas.ucThumbWheelReadingArrayIndex!=(unsigned char)ThumbWheelReadingArray;Meas.ucThumbWheelReadingArrayIndex++)
    {
        Meas.iThumbWheelReadingArray[Meas.ucThumbWheelReadingArrayIndex]=(int)ThumbWheelDefReading;         //the array of data used for averaging
    }
    Meas.iThumbWheelReading=0;                              //the thumb wheel pot array index

    //---------Set Up The Initial Data For The Pump Current Measurement----------------------------------------------------------
    Meas.iPumpCurrent=(int)NomPumpCurrentMilliAmp;
    for(Meas.ucPumpCurrentIndex=0;Meas.ucPumpCurrentIndex!=(unsigned char)PumpCurrentMilliAmpIndex;Meas.ucPumpCurrentIndex++)
    {
        Meas.iPumpCurrentArray[Meas.ucPumpCurrentIndex]=(int)NomPumpCurrentMilliAmp;
    }
    Meas.ucPumpCurrentIndex=0;                              //the pump current array index

    //---------Set Up The Initial Data For The ADC Array Of Measurements----------------------------------------------------------
    Meas.uiAdcData[A2D_POT_INPUT]=(unsigned int)ThumbWheelDefData;
    Meas.uiAdcData[A2D_CO2_INPUT]=(unsigned int)Co2MaxA2dCount;
    Meas.uiAdcData[A2D_BAT_INPUT]=(unsigned int)BatteryVoltsMaxA2dCount;
    Meas.uiAdcData[A2D_PUMP_I_INPUT]=(unsigned int)MaxA2dPumpCurrentCount;
    Meas.uiAdcData[A2D_TEMP_INPUT]=(unsigned int)ThermistorMaxA2dCount;
}
