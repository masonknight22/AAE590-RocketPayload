/*
    Simple sketch to start a motor connected to a relay when a light sensor transitioned to 
    the correfct states using trending and statistical calculations.

    Ball Aerospace
    BIRST Rocket Team PAPA
    Author: Mason Knight
    Date: July 2023

    Sources: 
    - Calculuating data treand (https://rfix.me/arduino-trend)

    Arduino Settings:
        Board: Arduino Uno
        Libraries (Install by searching the Arduion Library Manager):
        - SparkFun AMbient Light Sensor
        - Sparkfun Qwicc Relay

    Hardware:
    - Sparkfun Redboard QWIIC
    - Sparkfun  VELM6030 Ambient Lilght Sensor
    - Sparkfun Qwiic Single Relay

*/

#include <Wire.h>
#include "SparkFun_VEML6030_Ambient_Light_Sensor.h"
#include "Sparkfun_Qwiic_Relay.h"

#define AL_ADDR 0x48
#define RELAY_ADDR 0x18 // Alternate address 0x19

// Possible values: .125, .25, 1, 2
// Both .125 and .25 should be used in most cases except darker rooms.
// A gain of 2 should only be used if the sensor will be covered by a dark glass
float gain = .125;

// Possible integration times in milliseconds: 800, 400, 200, 100, 50, 25
// Higher times give higher resolutions and should be used in darker light
int time = 50;
long luxVal = 0;

const int PayloadTrendTrainingKey     = 0;
const int PayloadTrendFallingKey      = 1;
const int PayloadTrendSteadyKey       = 2;
const int PayloadTrendRisingKey       = 3;
 
 const int LowLux = 10;
 
 const int LoadingPayload = 0;
 const int WaitingPayload = 1;
 const int DeployingPayload = 2;
 const int Finished = 3;
 int PayloadState = LoadingPayload;

 void setup() {
    Wire.begin();
    Serial.begin(9600);

    if ( light.begin()) {
        Serial.println("Ready to sense some light!");

        // Again the gain and integration times determine the resolution of the lux
        // value, and give different reanges of possibel ilght readings.
        light.setGain(gain);
        light.setIntegTime(time);

        Serial.println("Reading settings...");
        Serial.print("Gain: ");
        float gainVal = light.readGain();
        Serial.print(gainVal, 3);
        Serial.print("Integreation Time: ");
        int timeVal = light.readIntegTime();
        Serial.println(timeVal);
        
    }
    else {
        Serial.println("Could not communicate with the sensor!");
    }

    if  (relay.begin()) {
        Serial.println("ready to flip some switches.");
        float version = relay.singleRelayVersion();
        Serial.print("Firmware Version: ");
        Serial.println(version);
    }
    else {
        Serial.println("Check connections to Qwiic Relay.");
    }
   
 }

 void loop() {
    luxVal = light.readLight();
    Serial.print("Ambient Light Reading: ")
    Serial.pprint(luxVal);
    Serial.println(" Lux");
    delay(2000);

    int trendKey = luxAnalysisIncluding(luxVa);
    Serial.print("Trend Key: ");
    Serial.println(trendKey);

    if (PayloadState == LoadingPayload && luxV al < LowLux && trendKey == 2) {
        PayloadState = WaitingPayload;
        Serial.println("Waiting for Deployoment");
    }
    
    if (PayloadState == WaitingPayload && luxVal > LowLux && trendKey == 2) {
        PayloadState = DeployingPayload;
        Serial.println("Payload Deployed");
    }

    if (PayloadState == DeployingPayload && luxVal > LowLux && trendKey == 2) {
        PayloadState = Finished;
    }
 }

 void startMotor() {
    Serial.println("Start Motor");
    relay.turnRelayOn();
 }


 

/*
 *  Note: the size of the sensor data history array and the
 *  Critical value of t are related. If you change the size
 *  of the array, you must recalculate Critical_t_value.
 *  
 *  Given:
 *  
 *      Let α = 0.05    (5%)
 *      Let ν = (LuxHistorySize - 2) = (6 - 2) = 4
 *      
 *  Then use one of the following:
 *  
 *      Excel:      =T.INV(α/2,ν)  
 *      
 *      TINspire:   invt(α/2,ν)
 *      
 *  Given those inputs, the answer is:
 *  
 *      -2.776445105
 *      
 *  Critical_t_value is the absolute value of that.
 *  
 */
const size_t LuxHistorySize = 6;
const double Critical_t_value = 2.776445105;
 
// the array of pressures
int luxs[LuxHistorySize] = { };
 
// the number of elements currently in the array
size_t luxHistoryCount = 0;
 
const int luxAnalysisIncluding(double newlux) {
 
    // have we filled the array?
    if (luxHistoryCount < LuxHistorySize) {
 
        // no! add this observation to the array
        luxs[luxHistoryCount] = newlux;
 
        // bump n
        luxHistoryCount++;
        
    } else {
 
        // yes! the array is full so we have to make space
        for (size_t i = 1; i < LuxHistorySize; i++) {
 
            luxs[i-1] = luxs[i];
 
        }
 
        // now we can fill in the last slot
        luxs[LuxHistorySize-1] = newlux;
        
    }
 
    // is the array full yet?
    if (luxHistoryCount < LuxHistorySize) {
 
        // no! we are still training
        return PayloadTrendTrainingKey;
        
    }
 
    /*
     * Step 1 : calculate the straight line of best fit
     *          (least-squares linear regression)
     */
 
    double sum_x = 0.0;     // ∑(x)
    double sum_xx = 0.0;    // ∑(x²)
    double sum_y = 0.0;     // ∑(y)
    double sum_xy = 0.0;    // ∑(xy)
    
    // we need n in lots of places and it's convenient as a double
    double n = 1.0 * LuxHistorySize;
 
    // iterate to calculate the above values
    for (size_t i = 0; i < LuxHistorySize; i++) {
 
        double x = 1.0 * i;
        double y = luxs[i];
 
        sum_x = sum_x + x;
        sum_xx = sum_xx + x * x;
        sum_y = sum_y + y;
        sum_xy = sum_xy + x * y;
        
    }
 
    // calculate the slope and intercept
    double slope = (sum_x*sum_y - n*sum_xy) / (sum_x*sum_x - n*sum_xx);
    double intercept = (sum_y -slope*sum_x) / n;
 
    /*
     * Step 2 : Perform an hypothesis test on the equation of the linear
     *          model to see whether, statistically, the available data
     *          contains sufficient evidence to conclude that the slope
     *          is non-zero.
     *          
     *          Let beta1 = the slope of the regression line between
     *          fixed time intervals and lux observations.
     *          
     *          H0: β₁ = 0    (the slope is zero)
     *          H1: β₁ ≠ 0    (the slope is not zero)
     *          
     *          The level of significance: α is 5% (0.05)
     *          
     *          The test statistic is:
     *          
     *              tObserved = (b₁ - β₁) / s_b₁
     *              
     *          In this context, b₁ is the estimated slope of the linear
     *          model and β₁ the reference value from the hypothesis
     *          being tested. s_b₁ is the standard error of b₁.
     *
     *          From H0, β₁ = 0 so the test statistic simplifies to:
     * 
     *              tObserved = b₁ / s_b₁
     *      
     *          This is a two-tailed test so half of α goes on each side
     *          of the T distribution.
     *          
     *          The degrees-of-freedom, ν, for the test is:
     *          
     *              ν = n-2 = 6 - 2 = 4
     *              
     *          The critical value (calculated externally using Excel or
     *          a graphics calculator) is:
     * 
     *              -tCritical = invt(0.05/2,4) = -2.776445105
     *      
     *          By symmetry:
     * 
     *              +tCritical = abs(-tCritical)
     *              
     *          The decision rule is:
     * 
     *              reject H0 if tObserved < -tCritical or 
     *                           tObserved > +tCritical
     *      
     *          which can be simplified to:
     * 
     *              reject H0 if abs(tObserved) > +tCritical
     * 
     *          Note that the value of +tCritical is carried in the
     *          global variable:
     *          
     *              Critical_t_value
     *              
     *          The next step is to calculate the test statistic but one
     *          of the inputs to that calculation is SSE, so we need
     *          that first.
     *      
     */
 
    double SSE = 0.0;        // ∑((y-ŷ)²)
 
    // iterate
    for (size_t i = 0; i < LuxHistorySize; i++) {
 
        double y = luxs[i];
        double residual = y - (intercept + slope * i);
        SSE = SSE + residual * residual;
 
    }
 
    /*    
     *          Now we can calculate the test statistic. Note the use
     *          of the fabs() function below to force the result into
     *          the positive domain for comparison with Critical_t_value
     */
 
    double tObserved =
        fabs(
           slope/(sqrt(SSE / (n-2.0)) / sqrt(sum_xx - sum_x*sum_x/n))
        );
 
    /*    
     *          Finally, make the decision and return a string
     *          summarising the conclusion.
     */
 
    // is tObserved further to the left or right than tCritical?
    if (tObserved > Critical_t_value) {
        // yes! what is the sign of the slope?
        if (slope < 0.0) {
            return PayloadTrendFallingKey;            
        } else {
            return PayloadTrendRisingKey;            
        }
    }
 
    // otherwise, the slope may be zero (statistically)
    return PayloadTrendSteadyKey;
 
}
