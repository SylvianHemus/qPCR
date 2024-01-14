
/****************************************************
This code controlls the 3D printed PCR device created
by the Bioelctronics Lab at UCF. 

Geoffrey Mulberry
Kevin A White
Dr. Brian N. Kim

****************************************************/

#include <SPI.h>
#include <MicroView.h>


//These are the pins

#define HEATER 5
#define FAN 6
#define LED 2
#define PHOTODIODE 0
#define THERMISTOR 1



// resistance at 25 degrees C
#define THERMISTORNOMINAL 10000
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 5
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3434
// the value of the 'other' resistor
#define SERIESRESISTOR 9990

// the reverse transcription time in seconds
 unsigned long int RT=600; //600
// the Pre-heating (95C) time in seconds
 unsigned long int PreHeat=180; //180

  int heaterstatus;
  int oldheaterstatus;
  
  double c;
  int oldtemps[64];
  int tempdelay = 0;

  // variables for reading out the results
  double readings[50];
  double times[50];
  int viewreadings = 0;
  int offset = 0; 
  int yposition = 0;
  int plotview = 0;
  
  unsigned long int printtime = 0;
  unsigned long int oldprinttime = 0;
  int cyclecount = 0;
  int shift = 0;
  int cycling = 0;
  int started = 0;
  
  int but1 = 0;
  int but2 = 0;
  int but3 = 0;
  int but4 = 0;

  int oldbut1 = 0;
  int oldbut2 = 0;
  int oldbut3 = 0;
  int oldbut4 = 0;

  int but1trig = 0;
  int but2trig = 0;
  int but3trig = 0;
  int but4trig = 0;

  double intensity = 0.0;
  int intensityflag = 0;

  int LEDstate = 0;

  int seconds = 0;
  int minutes = 0;

  double hightemp = 0;
  double lowlowtemp = 58.5;
  double lowtemp = 0;

  int numlowcycles = 2;
  int lowcycles = 0;
  int lowcycling = 0;

  unsigned long int runningtime = 0;
  unsigned long int startingtime = 0;
  unsigned long int currenttime = 0;

  int nocartridge = 1;
  


void serialdiagnostics()
{
  Serial.print(currenttime/1000.0);
  Serial.print("\t");  
  Serial.print(runningtime/1000.0);
  Serial.print("\t");
  Serial.print(c);
  Serial.print("\t");  
  Serial.print(cyclecount);

  // Print out the intensities
  Serial.print("\n");
  Serial.print("\t\t\t\t\t\t\t\t");
  
  int k;

  for (k = 0; k<50; k++)
  {
    Serial.print (readings[k]);
    Serial.print("\t"); 
  }
  Serial.print("\n");
  Serial.print("\t\t\t\t\t\t\t\t");
  for (k = 0; k<50; k++)
  {
    Serial.print (times[k]);
    Serial.print("\t"); 
  }
  Serial.print("\n");
}

double Thermistor(int RawADC) 
{
 // Inputs ADC Value from Thermistor and outputs Temperature in Celsius
 //  requires: include <math.h>
 // Utilizes the Steinhart-Hart Thermistor Equation:
 //    Temperature in Kelvin = 1 / {A + B[ln(R)] + C[ln(R)]^3}
 //    where A = 0.001129148, B = 0.000234125 and C = 8.76741E-08
 double Resistance;  double Temp;  // Dual-Purpose variable to save space.
 Resistance=10000.0*((1024.0/RawADC) - 1);  // Assuming a 10k Thermistor.  Calculation is actually: Resistance = (1024 /ADC -1) * BalanceResistor
// For a GND-Thermistor-PullUp--Varef circuit it would be Rtherm=Rpullup/(1024.0/ADC-1)

// Serial.print(Resistance);
// Serial.print("\t");
  double steinhart;
  steinhart = Resistance / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C
  Temp = steinhart;
  return Temp;  // Return the Temperature
}

void LEDon()
{
  digitalWrite(LED, HIGH);
}

void LEDoff()
{
  digitalWrite(LED, LOW);
}

void getIntensity()
{
  LEDon();                    // turn on the LED
  delay(1000);                  // wait a bit to make sure the LED has had time to turn on
  double intensities[5];
  int n;
  for(n = 0; n<5; n++)
     {
        intensities[n] = analogRead(PHOTODIODE); // Photodiode is now on A0
     }
  intensity = median(5, intensities) * 5.0 / 1024.0;
  LEDoff(); 
}

void getTemp() // Takes a temperature reading 3 times and returns the median
{
     double temps[3];
     int n;
     for(n = 0; n<3; n++)
     {
      temps[n] = Thermistor(analogRead(THERMISTOR));
     }
     c = median(3, temps);

     
}

double median(int n, double x[]) 
{
    double temp;
    int i, j;
    // the following two loops sort the array x in ascending order
    for(i=0; i<n-1; i++) {
        for(j=i+1; j<n; j++) {
            if(x[j] < x[i]) {
                // swap elements
                temp = x[i];
                x[i] = x[j];
                x[j] = temp;
            }
        }
    }

    if(n%2==0) {
        // if there is an even number of elements, return mean of the two elements in the middle
        return((x[n/2] + x[n/2 - 1]) / 2.0);
    } else {
        // else return the element in the middle
        return x[n/2];
    }
}

void readButtons()
{
  oldbut1 = but1;
  oldbut2 = but2;
  oldbut3 = but3;
  oldbut4 = but4;

    // read the button status 
   but1 = analogRead(2);
   but2 = analogRead(3);
   but3 = analogRead(4);
   but4 = analogRead(5);
  

   // convert them to simple boolean values (1 = pressed, 0 = not pressed)

   if (but1 < 100)
   {
      but1 = 1;
   }
   else
      but1 = 0;
    
   if (but2 < 100)
   {
      but2 = 1;
   }
   else
      but2 = 0;
    
   if (but3 < 100)
   {
      but3 = 1;
   }
   else
      but3 = 0;
    
   if (but4 < 100)
   {
      but4 = 1;
   }
   else
      but4 = 0;

// determine edge triggered values

   if (but1 == 1 && oldbut1 == 0)
   {
      but1trig = 1;
   }
   else 
   {
      but1trig = 0;
   }
   if (but2 == 1 && oldbut2 == 0)
   {
      but2trig = 1;
   }
   else 
   {
      but2trig = 0;
   }
   if (but3 == 1 && oldbut3 == 0)
   {
      but3trig = 1;
   }
   else 
   {
      but3trig = 0;
   }
   if (but4 == 1 && oldbut4 == 0)
   {
      but4trig = 1;
   }
   else 
   {
      but4trig = 0;
   }
}


void setup() {


// for Heater
 pinMode(HEATER, OUTPUT); // set the relay control as an output
 digitalWrite(HEATER, LOW);

// for Fan
 pinMode(FAN, OUTPUT); // set the relay control as an output
 digitalWrite(FAN, LOW);

// for Microview

 uView.begin();

// for LED

pinMode(LED, OUTPUT);

 Serial.begin(250000);

 
// clears the temps array
 int n;
 for (n = 0; n < 64; n++)
 {
  oldtemps[n] = 47;
 }
}

void cartridgecheck ()
{
  if (c < 0)
     {
      nocartridge = 1;
     }
  else 
     {
      nocartridge = 0;
     }
}

void PCR()
{
  //////////////////////////////////////////
if (started == 0)
{
  //do nothing
  hightemp = 0.0;
  lowtemp = 0.0;
  cycling = 0;
  digitalWrite(FAN, LOW);
  digitalWrite(HEATER, LOW);
  runningtime = 0;
}
else
{
 if (runningtime <= RT*1000)
{
  hightemp = 56.00;
  lowtemp = 54.00;
}

 if (runningtime > RT*1000 && runningtime < (RT*1000 + PreHeat*1000))
{
  hightemp = 97.00;
  lowtemp = 96.00;
}


 if (runningtime >= RT*1000 + PreHeat*1000) // this is the main cycling
{
  hightemp = 125.0;
  lowtemp = 59;
  cycling = 1;
}
}

if (cyclecount >= 51)//51)
{
  startingtime = 0;
  hightemp = 0.0;
  lowtemp = 0.0;
  cycling = 0;
  started = 0;
}


  getTemp();


   oldheaterstatus = heaterstatus;

if (started == 1)
  {
if (lowcycling == 0)
{
   if (c >= hightemp)
   {
      digitalWrite(HEATER,LOW); // if it is too hot turn off the heater 
      if(cycling==1)
      {
      delay(10);
      digitalWrite(FAN,HIGH); // also turn on the fan
      }
      
      heaterstatus = 0;
   }

   else if (c <= lowtemp)
   {
      heaterstatus = 1;
      digitalWrite(FAN,LOW); // turn off the fan
      
      if (oldheaterstatus == 0 && heaterstatus == 1 && cycling == 1)
      {
          //cyclecount ++;
          lowcycling = 1;
      }
      delay(10);
      digitalWrite(HEATER,HIGH); // turn on the heater 
   }
     //for cyclecount = 0, skip heating and go right to cooling, in order to include preheating setup as first cycling
   if (c > lowtemp && cyclecount == 0 && cycling == 1)
   {
      digitalWrite(HEATER,LOW); // if it is too hot turn off the heater 
      delay(10);
      digitalWrite(FAN,HIGH); // also turn on the fan
                 
      heaterstatus = 0;

   }
}

else // this is to hold the temperature low for a little bit
{
  
   if (c >= lowtemp)
   {
      digitalWrite(HEATER,LOW); // if it is too hot turn off the heater
      heaterstatus = 0;
   }

   else if (c <= lowlowtemp)
   {
      heaterstatus = 1;
      if (oldheaterstatus == 0 && heaterstatus == 1)
      {
          lowcycles ++;

          if (intensityflag==1)
          {
              cyclecount ++;
              getIntensity();
              readings[cyclecount-1]=intensity; // save the intensity
              times[cyclecount-1] = runningtime / 1000.0; // save the time
          }
          intensityflag ++; // increment the flag so that we read the next time through
          if(intensityflag == 2) // reset the flag if we have just read it
              intensityflag = 0;
      }
      digitalWrite(HEATER,HIGH); // turn on the heater 
      delay(100);
      digitalWrite(HEATER,LOW);// pulsing for 100ms
   }

   if (lowcycles >= numlowcycles)
   {
      lowcycling = 0;
      lowcycles = 0;
   }
}
  }
///////////////////////////////////////////////////
}

void plotTemp(int plotenable)
{
  int plotheight = 20;
  int temp;
  int n;
  shift = 0;
  tempdelay++;
  if (tempdelay >= 75) // if it has been a reasonable amount of time
  {
    tempdelay = 0;
    //oldprinttime = currenttime; // set the old time to the current time
    // calculate the new temperature
    temp = 47 - map(c, 30, 100, 0, plotheight);
    oldtemps[64] = temp;
    //shift = 1;
  //  if (shift==1) // shift
  //{
    //shift = 0;
    // shift the old temperatures left
    for (n = 0; n < 64; n++)
    {
      oldtemps[n] = oldtemps[n+1];
    }
  //}
  }
  if (plotenable == 1)
  {
  // print the temperatures
  for (n = 0; n < 65; n++)
  {
    uView.pixel(n, oldtemps[n]);
  }
  }
}

void plotData(void)
{
  
  int plotheight = 30;
  int temp[50];
  int n;

    for (n = 0; n < 50; n++)
    {
      temp[n] = 47 - map(readings[n]*1000, 0, 5000, 0, plotheight);
    }
  
  
  // print the data
  for (n = 0; n < 50; n++)
  {
    uView.pixel(n+7, temp[n]);
  }

  
}


   int mode = 0;
   // mode 0 is main menu
   // mode 1 is PCR viewing
   // mode 2 is intensity data review
   // mode 3 is about

   int pointer = 0; // points at mode selections
   
void loop() {//////////////////////////////////////////////////////////////////////////////////////////////

  //getIntensity();
  readButtons();
  currenttime = millis();
  runningtime = currenttime - startingtime;
  cartridgecheck(); // check if the cartidge is present

  //Serial.println(intensity);
  serialdiagnostics();

  PCR();// function contains thermocycling code
 
  //plotTemp(0);
  
  uView.clear(PAGE);

if (mode == 0)
{
  uView.setCursor(0,0);
  uView.print(" Main Menu");
  uView.line(0, 9, 64, 9);
  uView.setCursor(7,12);
  if (started == 0)
  {
    uView.print("New PCR");
  }
  else
  {
    uView.print("PCR");
  }
  uView.setCursor(7,22);
  uView.print("View Data");
  uView.setCursor(7,32);
  uView.print("About");

  if(pointer == 0)
  {
    uView.setCursor(0,12);
    uView.print(">");
    if (but2trig)
    {
      pointer = 1;
    }
    if (but3trig)
    {
      pointer = 2;
    }

    if (but1trig == 1 && started == 0) // start PCR if button 1 is pressed
  {
    startingtime = currenttime;
    runningtime = 0;
    started = 1;
    mode = 1;
  }
  if (but1trig == 1) // start PCR if button 1 is pressed
  {
    mode = 1;
  }
    
  }
  
  else if(pointer == 1)
  {
    uView.setCursor(0,22);
    uView.print(">");
    if (but2trig)
    {
      pointer = 2;
    }
    if (but3trig)
    {
      pointer = 0;
    }
    if (but1trig == 1) // go to data
    {
      but1trig = 0; // just reset this
      yposition = 0;
      mode = 2;
    }
  }
  else if(pointer == 2)
  {
    uView.setCursor(0,32);
    uView.print(">");
    if (but2trig)
    {
      pointer = 0;
    }
    if (but3trig)
    {
      pointer = 1;
    }
    if (but1trig == 1) // go to about
    {
      yposition = 11;
      mode = 3;
    }
  }
}


if (mode == 1) // PCR
{
  if (but4trig)
  {
    mode = 0;
  }
    // /*
  uView.setCursor(0,0);
  if(nocartridge == 0)
  {
    uView.print(c);
  }
  else
  {
    uView.print("~~"); // display this instead of temp if no cartridge
  }
  
  plotTemp(1); // plot the temperature
     
  uView.setCursor(50,0);
  uView.print(cyclecount);

  uView.setCursor(0,10);
  uView.print(intensity);

  
//*/
}

if (mode == 2)// data
{
  if (plotview == 0)
  {
  if (but4trig)
  {
    mode = 0;
  }
  if (but1trig)
  {
    plotview = 1;
  }
  
  if(but2)
  {
    yposition +=1; // move the text up the screen
  }
  else if (but3)
  {
    yposition -=1; // move the text up the screen
  }

  if(yposition == -1)
  {
    yposition = 0;
  }
  if(yposition == 47)
  {
    yposition = 46;
  }

  uView.setCursor(0,11); // the index
  uView.print(yposition+1);
  uView.setCursor(20,11); // the reading
  uView.print(readings[yposition]);
  uView.setCursor(0,21); // the index
  uView.print(yposition+2);
  uView.setCursor(20,21); // the reading
  uView.print(readings[yposition+1]);
  uView.setCursor(0,31); // the index
  uView.print(yposition+3);
  uView.setCursor(20,31); // the reading
  uView.print(readings[yposition+2]);
  uView.setCursor(0,41); // the index
  uView.print(yposition+4);
  uView.setCursor(20,41); // the reading
  uView.print(readings[yposition+3]);
  // this is at the bottom so it isn't covered
  uView.rectFill(0,0,64,10,BLACK,NORM);
  uView.setCursor(0,0);
  uView.print("   Data");
  uView.line(0, 9, 64, 9);
  }

  else
  {
    if(but4trig)
    {
      plotview = 0;
    }
    plotData();
    // this is at the bottom so it isn't covered
  uView.rectFill(0,0,64,10,BLACK,NORM);
  uView.setCursor(0,0);
  uView.print(" Data Plot");
  uView.line(0, 9, 64, 9);
  }

  
  
}

if (mode == 3)// about
{
  if (but4trig)
  {
    mode = 0;
  }
  if(but2)
  {
    yposition -=1; // move the text up the screen
  }
  else if (but3)
  {
    yposition +=1; // move the text up the screen
  }
  uView.setCursor(0,yposition);
  uView.print("Made by: \n\nGeoffrey \n Mulberry\n\nBrian \n Kim \n\nKevin \n  White\n\nAt UCF");

// this is at the bottom so it isn't covered
  uView.rectFill(0,0,64,10,BLACK,NORM);
  uView.setCursor(0,0);
  uView.print("   About");
  uView.line(0, 9, 64, 9);
}



     
   uView.display();        // display current page buffer
   //LEDon();
}





