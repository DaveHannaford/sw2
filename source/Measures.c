//***********************************************************************************************************************
//  © 2014 Kane International Ltd.                                                                                      *
//  INSTRUMENT      :KANE720 CO2 Leak Detector                                                                          *
//  FILE            :Timer.c                                                                                            *
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
#include <math.h>                                           // math routines are used in gas calculations
#include "..\header\Definitions.h"                          // Global definitions
#include "..\header\Structures.h"                           // structure variables definitions
//***********************************************************************************************************************
//**************************Variables************************************************************************************
const unsigned int uiClickTable10mS[11]={50,33,25,20,10,8,6,5,4,2,1};
const long lCTab[5][4];
//**************************External Variables***************************************************************************
extern struct InstrumentDisplay Led;
extern struct InstrumentTimers Timers;
extern struct InstrumentParameters Inst;
extern struct InstrumentMeasurements Meas;
//***********************************************************************************************************************
#include <math.h>                                           // math routines are used in gas calculations
#include "..\header\definitions.h"                          // definitions for this file only
#include "..\header\structures.h"                           // structure containing most variables
//**************************Function Prototypes****************************************************************************
void CalibrateCo2Zero(void);
unsigned char CalibrateCo2ZeroCheck(void);
void RefreshAllMeasurements(void);
void InitCo2(void);

static void CalcCO2Abs(void);
static void CalcCO2ppm(void);
static void RefreshBatteryStatus(void);
static void RefreshCO2(void);
static void RefreshDialOutput(void);
static void RefreshPumpCurrent(void);
static void RefreshThermistorTemerature(void);
//**************************External Function Prototypes***************************************************************************
//------------ADC.c------------
extern void CalcBatteryVoltsFromAdc(void);
extern void AverageCO2Data(void);
extern void CalcDialOutput(void);
extern void CalcPumpCurrent(void);
extern void CalcThermistorTemperature(void);
extern void UpdateDialData(void);
//---------Timer.c---------------------------------------
//---------LED Display.c----------
//**************************External Variables***************************************************************************
extern struct InstrumentDisplay Lcd;
extern struct InstrumentTimers Timers;
extern struct InstrumentParameters Inst;
extern struct InstrumentMeasurements Meas;
//**************************Constants************************************************************************************
#define X1 0
#define X2 1
#define Y1 2
#define Y2 3
//***********************************************************************************************************************
//=======================================================================================================================
// Function:    RefreshCO2
// Description: On the expiry of the timer calcs the CO2 sensor voltage and calcs a scaled absorption to be displayed
//      when needed...currently this is updated every 200ms and the data is averaged over 16 samples..measured
//          every 60ms..960ms cycle so lamp is flashed at this rate too
//=======================================================================================================================
static void RefreshCO2(void)
{
    if(Timers.ucRefreshCo2Data10ms==0)  //response of the CO2 sensor is slow but any changes need feeding back to user as is a sniffer
    {
        Timers.ucRefreshCo2Data10ms=(unsigned char)Co2MeasurementUpdate10ms;
        AverageCO2Data();
        CalcCO2Abs();
        CalcCO2ppm();
    }
    return;
}
//=======================================================================================================================
//  Function:       RefreshBatteryStatus
//  Description:    On the expiry of the timer calcs the battery voltage and checks if battery supply is low
//                  The data is updated every 0.5seconds and the data is averaged from 8 samples.
//=======================================================================================================================
static void RefreshBatteryStatus(void)
{
    if(Timers.ucRefreshBatteryStatus10ms==0)                                        //0.5sec timer expired
    {
        Timers.ucRefreshBatteryStatus10ms=(unsigned char)BatteryStatusUpdate10ms;   //battery status refresh period
        CalcBatteryVoltsFromAdc();                                                  //processes the battery voltage and life
    }
    return;
}
//=======================================================================================================================
//  Function:       RefreshThermistorTemerature
//  Description:    On the expiry of the timer runs CalcThermistorTemperature() to refresh the calculated data
//                  for the thermistor temperature...currently set to 1sec...it should not be changing that quickly!
//                  Note that the temperature readings are averaged on a true averaging scheme over 4 seconds
//=======================================================================================================================
static void RefreshThermistorTemerature(void)
{
    if(Timers.ucRefreshThermistorTemperature10ms==0)
    {
        Timers.ucRefreshThermistorTemperature10ms=(unsigned char)RefreshThermistorTemperature10ms;
        CalcThermistorTemperature();                        //calculates the local temperature in the range -15 to 70degC
    }
    return;
}
//=======================================================================================================================
// Function:    RefreshAllMeasurements
// Description: This calls all the routines to process all the instruments measurements on a timed basis
//=======================================================================================================================
void RefreshAllMeasurements(void)
{
    RefreshCO2();
    RefreshBatteryStatus();
    RefreshThermistorTemerature();
    RefreshDialOutput();
    RefreshPumpCurrent();
    return;
}
//=======================================================================================================================
//  Function:   RefreshDialOutput
//  Description: On the expiry of the timer runs CalcDialOutput() to refresh the calculated data
//              for the dial position...currently set to 0.1sec...
//=======================================================================================================================
static void RefreshDialOutput(void)
{
    if(Timers.ucRefreshPumpSpeed10ms==0)
    {
        Timers.ucRefreshPumpSpeed10ms=(unsigned char)RefreshPumpSpeed10ms;
        UpdateDialData();
        CalcDialOutput();   //calculates the position of the dial to select CO2 sensitivity
    }
    return;
}
//=======================================================================================================================
//  Function:       CalibrateCo2Zero
//  Description:    Performs a zero on fresh air and notes the local temperature as zero is temperature sensitive
//=======================================================================================================================
void CalibrateCo2Zero(void)
{
    Inst.iCo2ZeroData=Meas.iCo2DataAv;                      //the latest CO2 average data
    Inst.fThermistorTempAtZero=Meas.fThermistorTemp;        //grab the current temperature(degC)
}
//=======================================================================================================================
//  Function:       CalibrateCo2ZeroCheck
//  Description:    Performs a check on the CO2 zero level and returns True if the level has not changed much
//=======================================================================================================================
unsigned char CalibrateCo2ZeroCheck(void)
{
    unsigned char ucCheck=(unsigned char)True;

    if(Meas.iCo2DataAv>Inst.iCo2ZeroData)                                       //test which signal is greatest
    {
        if((Meas.iCo2DataAv-Inst.iCo2ZeroData)>((int)Co2ZeroStabLimitsData))    //get the change in reading and if too great clear flag
        {
            ucCheck=(unsigned char)False;
        }
    }
    else
    {
        if((Inst.iCo2ZeroData-Meas.iCo2DataAv)>((int)Co2ZeroStabLimitsData))    //get the change in reading and if too great clear flag..a count of 4
        {
            ucCheck=(unsigned char)False;
        }
    }


    if(Meas.fThermistorTemp>Inst.fThermistorTempAtZero)                                 //test which temperature is the greater
    {
        if((Meas.fThermistorTemp-Inst.fThermistorTempAtZero)>Co2ZeroTempStabLimitsDegC) //tests for 1degC
        {
            ucCheck=(unsigned char)False;
        }
    }
    else
    {
        if((Inst.fThermistorTempAtZero-Meas.fThermistorTemp)>Co2ZeroTempStabLimitsDegC)
        {
            ucCheck=(unsigned char)False;
        }
    }

    return(ucCheck);
}
//=======================================================================================================================
//  Function:   CalcCO2Abs
//  Description: Calculates the CO2 absorption in integer form to 0.1%, ie 100% is '1000'
//               Since the a2d is only ten bit there may be some missing ABS values as calculation is rounded up/down
//=======================================================================================================================
static void CalcCO2Abs(void)
{
    long lAbs,lData,lZero;

    if(Inst.iCo2ZeroData!=(int)Co2LowA2dCount && Inst.iCo2ZeroData!=(int)Co2HiA2dCount)
    {
        lAbs=(long)1000;
        lData=(long)Meas.iCo2DataAv;
        lZero=(long)Inst.iCo2ZeroData;
        lAbs*=lData;
        lAbs/=lZero;
        lAbs=(long)1000-lAbs;
        if(!(lAbs>1000||lAbs<0))
        {
            Meas.iCo2Abs=(int)lAbs;
        }
    }
    return;
}
//=======================================================================================================================
//  Function:   CalcCO2ppm
//  Desc:       Convert the calculated ABS into CO2 ppm
//=======================================================================================================================
const long lCTab[5][4]=
{
    //   X1          X2         Y1        Y2
    //lower abs, upper abs, lower ppm, upper ppm
    {0,              30,         0,      3800},
    {30,             83,         3800,   9500},
    {83,             219,        9500,   40000},
    {219,            364,        40000,  100000},
    {364,            523,       100000,  200000}
};
//=======================================================================================================================
static void CalcCO2ppm(void)
{
    long lPpm;
    unsigned char ucI;

    lPpm=(long)Meas.iCo2Abs;        //grab current ABS
    for(ucI=0;ucI!=5;ucI++)
    {
        if(lPpm<lCTab[ucI][X2])
        {
            break;
        }
    }
    if(ucI!=5)
    {
        Inst.lCo2Ppm=(lPpm*(lCTab[ucI][Y2]-lCTab[ucI][Y1])+lCTab[ucI][Y1]*lCTab[ucI][X2]-lCTab[ucI][X1]*lCTab[ucI][Y2])
                  /(lCTab[ucI][X2]-lCTab[ucI][X1]);
    }
}
//=======================================================================================================================
//  Function:       InitCo2
//  Description:    Initialise the CO2 data registers to a default level
//=======================================================================================================================
void InitCo2(void)
{
    Meas.lCo2Ppm=0;
    Meas.iCo2Abs=0;
    Meas.iCo2DataAv=Inst.iCo2ZeroData;
    Inst.fThermistorTempAtZero=(float)ThermistorDefTemp;                //set the thermistor temperature to a standard ambient
}
//=======================================================================================================================
//  Function:       RefreshPumpCurrent
//  Description:    On the expiry of the timer runs CalcPumpCurrent() to refresh the calculated data
//                  for the pump current...
//=======================================================================================================================
static void RefreshPumpCurrent(void)
{
    if(Timers.ucRefreshPumpCurrent10ms==0)
    {
        Timers.ucRefreshPumpCurrent10ms=(unsigned char)RefreshPumpCurrent10ms;
        CalcPumpCurrent();
    }
}