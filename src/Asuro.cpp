/*
 
 Modified version of v0.3 of the Asurino lib inside the AsuroLib at SourceForge. 
 It corrects some errors and adds some small enhancements.
 
 Changes:
    -   Added setFrontLED
    -   Corrected readOdometry and readLinesensor
    -   Added some comments
    -   Rearranged constants
    -   New Arduino library format
    -   Added interrupt example
 
 Modifications by Dirk Froehling, November 2015
 
 License: GNU General Public License version 2.0 (GPLv2)
 
 */

#include "Asuro.h"
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
extern "C" {
#include <inttypes.h>
#include <avr/interrupt.h>
}

// ------------ Internal constants

// Pin numbers for usage with analog/digitalRead/Write
// These numbers correspond to the Arduino board, see
// https://www.arduino.cc/en/Hacking/PinMapping and
// http://www.asurowiki.de/pmwiki/pmwiki.php/Main/Prozessor
#define PB5 13
#define PB4 12
#define PD5 5
#define PD4 4
#define PWM_MOTOR_L 9
#define PWM_MOTOR_R 10

#define statusledred 2
#define frontled 6
#define irtxled 11
#define lphotores 3
#define rphotores 2
#define statusledgreen 8
#define odometricled 7
#define lodometric 1
#define rodometric 0
#define switches 4
#define battery 5
#define lbackled 1
#define rbackled 0


#define IR_CLOCK_RATE    36000L


// -------------

void (*ISRfunction)();


// Asuro infrared UART interfaces uses Timer2 with 72kHz
// counts falling and rising edge => 36kHz*2 = 72kHz
#if defined(__AVR_ATmega168__)
SIGNAL(SIG_OUTPUT_COMPARE2A) {
}
#else
//SIGNAL(SIG_OUTPUT_COMPARE2) {
ISR(TIMER2_COMP_vect) {
}
#endif


#if defined (__AVR_ATmega8__)
ISR (TIMER1_COMPA_vect)
{
    (*ISRfunction)();
}
#else
#error CPU type not yet supported
#endif


Asuro::Asuro(void)
{
    // Do nothing. We could call Init() here, but we will adhere to the ASURO standard.
}


void Asuro::Init(void)
{
    // Ports for motor control
    pinMode(PB4, OUTPUT);
    pinMode(PB5, OUTPUT);
    pinMode(PD4, OUTPUT);
    pinMode(PD5, OUTPUT);
    
    pinMode(frontled, OUTPUT);
    pinMode(statusledred, OUTPUT);
    pinMode(statusledgreen, OUTPUT);
    pinMode(odometricled, OUTPUT);
    pinMode(irtxled, OUTPUT);
    
    // for PWM (8-Bit PWM) on OC1A & OC1B
    TCCR1A = (1 << WGM10) | (1 << COM1A1) | (1 << COM1B1);

    // fix analog-to-digital converter timing (for 8 MHz clock)
    ADCSRA &= ~ADPS0;
    setTimer2();
}


void Asuro::startTimer1(unsigned long ms, void (*isrfunction)())
{
#if defined (__AVR_ATmega8__)
    ISRfunction = isrfunction;
    
    TCCR1A = 0x00;      // Timer benutzt kein PWM
    
    // ASURO-Taktfrequenz (F_CPU) ist 8 MHz. Prescaler von 1024 sorgt für 7812,5 Ticks pro Sekunde
    TCCR1B = (1<<CS10) | (1<<CS12) | (1<<WGM12); // Prescale = 1024, CTC ("Clear Timer on Compare")
    
    // Hier wird der Vergleichswert gesetzt
    unsigned long compare = F_CPU / 1024L * ms  / 1000L;
    OCR1A = compare;
    
    TIMSK |= (1<<OCIE1A); // Compare A Match Interrupt aktivieren
#else
#error CPU type not yet supported
#endif
}

void Asuro::stopTimer1()
{
#if defined (__AVR_ATmega8__)
    // Timer über das Timer/Counter Interrupt Mask Register disablen
    TIMSK &= ~(1<<OCIE1A);
#else
#error CPU type not yet supported
#endif
}


// Set Timer2 for infrared transmitter
void Asuro::setTimer2(void)
{
    /* Asuro infrared UART interfaces uses Timer2 with 72kHz */
#if defined(__AVR_ATmega168__)
    // toggle on compare, clk/1
    TCCR2A = _BV(WGM21) | _BV(COM2A0);
    TCCR2B = _BV(CS20);
    // 36kHz carrier/timer
    OCR2A = 0x6e;    //(F_CPU/(IR_CLOCK_RATE*2L)-1);
#else
    // toggle on compare, clk/1
    TCCR2 = _BV(WGM21) | _BV(CS20) | _BV(COM20);
    // 36kHz carrier/timer
    OCR2 = 0x6e;    //(F_CPU/(IR_CLOCK_RATE*2L)-1);
#endif
    DDRB |= _BV(PB3);
}


//taillight LEDs
void Asuro::setBackLED(unsigned char left, unsigned char right)
{
	if (left) {
		PORTD &= ~(1 << PD7); // Wheel LED OFF
		DDRC |= (1 << PC1); // Output => no odometrie
		PORTC |= (1 << PC1);
	}
	else {
		PORTD |= (1 << PD7); 
		DDRC &=~ (1 << PC1); 
		PORTC &=~ (1 << PC1);
	}
	if (right) {
		PORTD &= ~(1 << PD7); // Wheel LED OFF
		DDRC |= (1 << PC0); // Output => no odometrie
		PORTC |= (1 << PC0);
	}
	else {
		PORTD |= (1 << PD7); 
		DDRC &=~ (1 << PC0); 
		PORTC &=~ (1 << PC0);
	}	
}


//
void Asuro::setFrontLED(unsigned char status)
{
    digitalWrite(frontled, status);
}


// Bicolor Status LED
void Asuro::setStatusLED(unsigned char color)
{
    if (color == OFF)    {digitalWrite(statusledgreen, LOW); digitalWrite(statusledred, LOW);}
    if (color == GREEN)  {digitalWrite(statusledgreen, HIGH); digitalWrite(statusledred, LOW);}
    if (color == YELLOW) {digitalWrite(statusledgreen, HIGH); digitalWrite(statusledred, HIGH);}
    if (color == RED)    {digitalWrite(statusledgreen, LOW); digitalWrite(statusledred, HIGH);}
}


//read front switches
int Asuro::readSwitches(void)
{
    long tmp;
    pinMode(3, OUTPUT);
    digitalWrite(3, HIGH);
    delayMicroseconds(10);
    tmp = analogRead(switches);
    digitalWrite(3, LOW);
    return ((10240000L/tmp-10000L)*MY_SWITCH_VALUE+5000L)/10000;
}


//read battery
int Asuro::readBattery(void)
{
    int tmp;
    uint8_t oldadmux = (ADMUX & (unsigned int) 0xf0);
    ADMUX = (1 << REFS0) | (1 << REFS1);
    delayMicroseconds(10);
    tmp = analogRead(battery);
    ADMUX = oldadmux;
    return tmp;
}


//read odometry
void Asuro::readOdometry(int *data)
{
    digitalWrite(odometricled, HIGH);
    data[LEFT] = analogRead(lodometric);
    data[RIGHT] = analogRead(rodometric);
}


//read line sensors
void Asuro::readLinesensor(int *data)
{
    uint8_t oldadmux = (ADMUX & (unsigned int) 0xf0);
    ADMUX = (1 << REFS0) ;
    data[LEFT] = analogRead(lphotores);
    data[RIGHT] = analogRead(rphotores);
    ADMUX = oldadmux;
}


//motor direction
void Asuro::setMotorDirection (int left, int right)
{
    switch (left) {
        case FWD:
            digitalWrite (PD4, LOW);
            digitalWrite (PD5, HIGH);
            break;
        case RWD:
            digitalWrite (PD4, HIGH);
            digitalWrite (PD5, LOW);
            break;
        case BREAK:
            digitalWrite (PD4, LOW);
            digitalWrite (PD5, LOW);
            break;
        case FREE:
            digitalWrite (PD4, HIGH);
            digitalWrite (PD5, HIGH);
            break;
        default:
            break;
    }
    
    switch (right) {
        case FWD:
            digitalWrite (PB4, LOW);
            digitalWrite (PB5, HIGH);
            break;
        case RWD:
            digitalWrite (PB4, HIGH);
            digitalWrite (PB5, LOW);
            break;
        case BREAK:
            digitalWrite (PB4, LOW);
            digitalWrite (PB5, LOW);
            break;
        case FREE:
            digitalWrite (PB4, HIGH);
            digitalWrite (PB5, HIGH);
            break;
        default:
            break;
    }
}


//motor speed
void Asuro::setMotorSpeed (int left, int right)
{
	left = constrain(left, -255, 255);
	right = constrain(right, -255, 255);
	int motorDirs[2] {FWD, FWD};
	if (left < 0) {
		left = -left;
		motorDirs[LEFT] = RWD;
	}
	if (right < 0) {
		right = -right;
		motorDirs[RIGHT] = RWD;
	}
	Asuro::setMotorDirection(motorDirs[LEFT], motorDirs[RIGHT]);
    analogWrite(PWM_MOTOR_L, left);
    analogWrite(PWM_MOTOR_R, right);
}


//"square" motion pattern
void Asuro::driveSquare(int timeForOneEdge, int speed)
{
    setMotorSpeed(speed, speed);
    //forwards
    setMotorDirection (FWD, FWD);
    delay (timeForOneEdge);
    //right
    setMotorDirection (FWD, RWD);
    delay (timeForOneEdge);
    //backwards
    setMotorDirection (RWD, RWD);
    delay (timeForOneEdge);
    //left
    setMotorDirection (RWD, FWD);
    delay (timeForOneEdge);
    setMotorSpeed(0, 0);
}


//circular accelerating motion pattern
void Asuro::driveCircular(int maxSpeed)
{
    int var = 0;
    if (maxSpeed > 255)
        maxSpeed = 255;
    setMotorDirection(FWD, RWD);
    while(var < maxSpeed)
    {
        setMotorSpeed(var, var);
        delay(25);
        var++;
    }
}

