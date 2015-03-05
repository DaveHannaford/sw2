//***********************************************************************************************************************
//  © 2014 Kane International Ltd.                                                                                      *
//  INSTRUMENT      :KANE700 Leak Detector                                                                              *
//  FILE            :ADC.c                                                                                              *
//  DESCRIPTION     :Controlling ADC interface                                                                          *
//  CREATED DATE    :12/05/2014                                                                                         *
//  CPU TYPE        :PIC24FJ32GA002                                                                                     *
//  COMPILER        :MPLAB X16  v1.21                                                                                   *
//  IDE             :MPLAB X IDE v2.05                                                                                  *
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
unsigned char ucA2dIndex;
//**************************Function Prototypes****************************************************************************
void CalcBatteryVoltsFromAdc(void);
void AverageCO2Data(void);
void CalcDialOutput(void);
void CalcPumpCurrent(void);
void CalcThermistorTemperature(void);
void GetInternalAdcCount(void);
void Initialise10BitAdc(void);
void UpdateDialData(void);
//**************************External Variables***************************************************************************
extern struct InstrumentDisplay Led;
extern struct InstrumentTimers Timers;
extern struct InstrumentParameters Inst;
extern struct InstrumentMeasurements Meas;
//***********************************************************************************************************************
//=======================================================================================================================
// Function:    _ISR _ADC1Interrupt
// Description: ISR called every Five ADC conversions as there are five channels to measure
//              ADC1BUF0 will hold first conversion from AN2, POT o/p for pump speed control
//              ADC1BUF1 will hold the conversion from AN3, CO2 output
//              ADC1BUF2 will hold the conversion from AN5, the battery voltage VBAT ref
//              ADC1BUF3 will hold the conversion from AN11, the pump current
//              ADC1BUF4 will hold the conversion from AN12, temperature from a thermistor
//              All data is in 10 bit integer format
//=======================================================================================================================
void _ISR720 _ADC1Interrupt(void)
{
    IFS0bits.AD1IF=False;
    return;
}
//=======================================================================================================================
// Function:    Initialise10BitAdc
// Description: This initialises the 10 bit ADC. The ADC will be driven in a manual mode. Each analogue input is sampled
//              and averaged before moving on to the next required voltage to be measured.
//=======================================================================================================================
void Initialise10BitAdc(void)
{
    AD1CON1=0x2000;         //from example 17-1..Integer format(FORM)
                            //Manual conversion trigger(SSRC)
                            //Manual start of sampling(ASAM=0)
                            //No operation in Idle Mode(ADSIDL)

    AD1PCFG=0xe7d0;         // AD1PCFG A2D port config file...a 1 is in digital mode..a 0 is in analogue mode..POR value 0x0000..all analogue
                            // this means that AN0..3 are analogue
                            // AN5 is in analogue
                            // AN14..11 are also analogue, but physically only goes to 12??
    AD1CON2 = 0x6000;       // Configure A/D voltage reference
                            // and buffer fill modes...currently filling ADC1BUF0-BUF7
                            // Vr+ and Vr- from Ext Vref+ Vref- (VCFG<2:0>=011),
                            // Inputs are not scanned,
                            // Interrupt after every sample
                            // Buffer configured as one 16 word buffer(ADC1BUF0 to ADC1BUFF)
                            // Always use MUX A input multiplexer input
    AD1CON3 = 0x1f01;       // Configure A/D conversion clock as Tcy
                            // Autosample time bits 31Tad

    AD1CHS = 0x8080;        // Configure input channels,
                            // CH0+ input is AN0
                            // CH0- input is AN1
    AD1CSSL = 0;            // No inputs are scanned.
    IPC3|=0x0010;           //internal ADC interrupt priority level 1
    IFS0bits.AD1IF = 0;     // Clear A/D conversion interrupt.
    return;
}
//=======================================================================================================================
// Function:    GetInternalAdcCount
// Description: Sets next i/p to measure and performs a sample and conversion of analogue i/p
//              Called from main() this uses a timer to update all measurements but not final data in miliivolts,
//              or temperature or whatever the measurement is.
//=======================================================================================================================
void GetInternalAdcCount(void)
{
    unsigned char ucDelay;

    for(ucA2dIndex=A2D_POT_INPUT;ucA2dIndex!=(unsigned char)A2D_DATA_END;ucA2dIndex++)
    {

        switch(ucA2dIndex)          //select the appropriate input channel to be measured
        {
            case A2D_POT_INPUT:
            AD1CHS=0x8082;          // Channel 0 positive MUX A is AN2
            break;

            case A2D_CO2_INPUT:
            AD1CHS=0x8083;          // Channel 0 positive MUX A is AN3
            break;

            case A2D_BAT_INPUT:
            AD1CHS=0x8085;          // Channel 0 positive MUX A is AN5
            break;

            case A2D_PUMP_I_INPUT:
            AD1CHS=0x808b;          // Channel 0 positive MUX A is AN11
            break;

            case A2D_TEMP_INPUT:
            AD1CHS=0x808c;          // Channel 0 positive MUX A is AN12
        }

        AD1CSSL = 0x0004;

        AD1CON1bits.ADON = 1;       // Turn on A/D
        AD1CON1bits.SAMP = 1;       // Start A/D sampling
        ucDelay=0xff;               // load the delay timer for sampling the analogue voltage
        while(--ucDelay);
        AD1CON1bits.SAMP = 0;       // Stop A/D sampling start the conversion

        while(!AD1CON1bits.DONE);   // conversion done?

        Meas.uiAdcData[ucA2dIndex]=(unsigned int)ADC1BUF0;       // yes then get ADC value
        AD1CON1bits.ADON = 0;       // Turn off A/D
    }
    return;
}
//============================================================================================================
//  Function:       CalcBatteryVoltsFromAdc
//  Description:    Takes the integer 10 bit value from the A2D and converts it into battery voltage
//                  and a percentage value to determine when the battery supply is low and exhausted
//                  The A2D data gives 13mV resolution per bit. The A2D data is available in buffer ADC1BUF2.
//                  Function saves the current battery voltage and averaged level as 0-128 count
//
//============================================================================================================
void CalcBatteryVoltsFromAdc(void)
{
    int iValue;
    unsigned char ucI;

    iValue=(int)Meas.uiAdcData[A2D_BAT_INPUT];                                      //the processor A2D buffer holding latest 10bit data from battery supply
    if(iValue>(int)BatteryVoltsMaxA2dCount)
    {
        Meas.iBatteryMilliVolts=(int)BatteryVoltsMaxErrMilliV;                      //assign to a high fault value
    }
    else if(iValue<(int)BatteryVoltsMinA2dCount)
    {
        Meas.iBatteryMilliVolts=(int)BatteryVoltsMinErrMilliV;                      //assign to a low fault value
    }
    else
    {
        Meas.iBatteryMilliVolts=((int)BatteryScaleFactorMilliVolts)*iValue;         //the battery voltage in whole millivolts
        //calculate the 'percentage'
        iValue=Meas.iBatteryMilliVolts-(int)BatteryMilliVoltsMin;
        iValue=iValue>>BatteryLevelShiftNo;                                         //this holds 'percentage' divides by 16

        Meas.ucBattLevelIndex++;                                                    //increment the index into the averaging array
        Meas.ucBattLevelIndex&=(unsigned char)(BattLevelReadingArrayIndex-1);
        Meas.iBattLevelReadingArray[Meas.ucBattLevelIndex]=iValue;                  //save the latest data into the array

        for(ucI=0,iValue=0;ucI!=(unsigned char)BattLevelReadingArrayIndex;ucI++)    //sum the level data in the array
        {
            iValue+=Meas.iBattLevelReadingArray[ucI];
        }

        Meas.iBatteryLevelReading=iValue>>BattLevelReadingShiftValue;               //logical shift it to get the average value..divide by 8
    }
    return;
}
//============================================================================================================
//  Function:       UpdateDialData
//  Description:    Takes the integer 10 bit value from the A2D and saves it into an array for averaging.
//                  The A2D data is available in buffer ADC1BUF0.
//============================================================================================================
void UpdateDialData(void)
{
    unsigned int uiData;

    Meas.ucThumbWheelReadingArrayIndex++;
    Meas.ucThumbWheelReadingArrayIndex&=(unsigned char)(ThumbWheelReadingArray-1);
    uiData=(unsigned int)Meas.uiAdcData[A2D_POT_INPUT];                             //...the pot data 0 to 1023 saved to array
    uiData&=0x03ff;                                                                 //10bit data only is valid
    Meas.iThumbWheelReadingArray[Meas.ucThumbWheelReadingArrayIndex]=uiData;
    return;
}
//============================================================================================================
//  Function:       CalcDialOutput
//  Description:    Takes the integer 10 bit values in the array, adds them and divides by logic shifting to
//                  get the averaged value. Indicates the position of the dial and therefore the sensitivity
//                  of the CO2 measurement.
//============================================================================================================
void CalcDialOutput(void)
{
    int iValue;
    unsigned char ucI;
    
    for(iValue=0,ucI=0;ucI!=(unsigned char)ThumbWheelReadingArray;ucI++)    //sum the array
    {
        iValue+=Meas.iThumbWheelReadingArray[ucI];
    }
    Meas.iThumbWheelReading=iValue>>ThumbWheelAverageShift;                 //this is the average data from last set of array data (160ms)
    Meas.iThumbWheelReading&=0x03ff;        //Meas.iThumbWheelReading holds data 0 to 1023 to display increasing/decreasing sensivity on display led

    return;
}
//============================================================================================================
//  Function:       AverageCO2Data
//  Description:    Takes the integer 10 bit value from the A2D and converts it into a direct voltage.
//                  This is the voltage measured at TP4.
//                  The maximum value must be less than the A2D reference voltage which is defined by
//                  AnaloguePsuVolts*AnaPsuVoltsVrefPotDown. The minimum can be 0V though a practical
//                  level should be checked which would correspond to high levels of CO2. At low levels of
//                  CO2 the voltage is maximum.
//============================================================================================================
void AverageCO2Data(void)
{
    long lSum;
    unsigned char ucI;
    int iValue;

    iValue=(int)Meas.uiAdcData[A2D_CO2_INPUT];                      //grab the a2d data for the CO2 channel

    if(iValue>(int)Co2MaxA2dCount)                                  //test for high data
    {
        iValue=(int)Co2HiA2dCount;                                  //set to highest value possible
    }
    else if(iValue<(int)Co2MinA2dCount)                             //test for low data
    {
        iValue=(int)Co2LowA2dCount;                                 //set to lowest value possible
    }
    //save the data into array and average
    Meas.ucCo2DataIndex++;
    Meas.ucCo2DataIndex&=(unsigned char)(Co2DataArrayIndex-1);         //wrap the index within bounds
    Meas.iCo2DataArray[Meas.ucCo2DataIndex]=iValue;                         //save the data into array
    for(ucI=0,lSum=0;ucI!=(unsigned char)Co2DataArrayIndex;ucI++)      //sum the data in the array
    {
        lSum+=(long)Meas.iCo2DataArray[ucI];
    }
    lSum=lSum>>Co2MilliVoltAverageShift;                                    //average the reading
    Meas.iCo2DataAv=(int)lSum;                                              //save the average reading
    return;
}
//==============================================================================================================
//  Function:       CalcThermistorTemperature
//  Description:    This function measures and places limits on the temperature from the on board thermistor bridge.
//                  The data is held in the processor buffer ADC1BUF0 in succession with all other measurements.
//                  The data range for the thermistor is 908 to 151 over the temperature range -15ºC to 70ºC
//==============================================================================================================
void CalcThermistorTemperature(void)
{
    int iValue;
    float fValue,fVal;

    iValue=Meas.uiAdcData[A2D_TEMP_INPUT];
    if(iValue>ThermistorMaxA2dCount)
    {
        Meas.fThermistorTemp=ThermistorMinTemp;         //this is a temperature less than -15degC
    }
    else if(iValue<ThermistorMinA2dCount)
    {
        Meas.fThermistorTemp=ThermistorMaxTemp;         //this is a temperature greater than 75degC
    }
    else
    {
        fVal=(float)iValue;
        fValue=(float)ThermistorFactor1;
        fValue+=fVal*((float)ThermistorFactor2);
        fValue+=fVal*fVal*((float)ThermistorFactor3);
        fValue+=fVal*fVal*fVal*((float)ThermistorFactor4);
        if(!((fValue>ThermistorMaxTemp) || (fValue<ThermistorMinTemp))) //have a valid temperature to add to the array for averaging
        {
            Meas.ucThermTempIndex++;
            Meas.ucThermTempIndex&=(unsigned char)(ThermistorTempArray-1);
            Meas.fThermTempArray[Meas.ucThermTempIndex]=fValue;
            for(iValue=0,fValue=0.0;iValue!=(int)ThermistorTempArray;iValue++)
            {
                fValue+=Meas.fThermTempArray[iValue];
            }
            Meas.fThermistorTemp=fValue/((float)ThermistorTempArray);
        }
    }
    return;
}
//==============================================================================================================
//  Function:       CalcPumpCurrent
//  Description:    This function measures the voltage across the resistor in series with the pump and later
//                  modifies the PWM drive to keep air flow constant deleivered by the pump
//                  The data is held in the processor buffer ADC1BUF3.
//                  The current is measured across a low Ohmic resistor.
//                  Using integers as the A2D resolution means that 1 bit is 1.19mA and this is for a possible
//                  flow control only
//==============================================================================================================
void CalcPumpCurrent(void)
{
    int iValue;
    long lVal;
    unsigned char ucI;

    lVal=(long)Meas.uiAdcData[A2D_PUMP_I_INPUT];                                  //grab the current A2D count
    if(lVal>(long)MaxA2dPumpCurrentCount)                                      //check for high current...initially set to 250mA
    {
        Meas.iPumpCurrent=(int)MaxPumpCurrentMilliAmp;                          //limit it to a maximum 250mA
    }
    else                                                                        //current is within the expected range
    {
        lVal*=(long)AnalogueRefMilliVolts;
        lVal/=(long)A2dBitCount;                                                //the current in mA

        Meas.ucPumpCurrentIndex++;                                              //increment the index into the array of data stored
        Meas.ucPumpCurrentIndex&=(unsigned char)(PumpCurrentMilliAmpIndex-1);   //ensure it is bounded
        Meas.iPumpCurrentArray[Meas.ucPumpCurrentIndex]=(int)lVal;              //save the data

        for(ucI=0,iValue=0;ucI!=(unsigned char)PumpCurrentMilliAmpIndex;ucI++)
        {
            iValue+=Meas.iPumpCurrentArray[ucI];                                //sum the data in the array
        }
        Meas.iPumpCurrent=iValue>>PumpCurrentAverageShift;                      //shift the data right to divide and average
    }
}
