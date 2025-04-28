// This traffic light simulator uses a Digispark to control a T-junction or intersection
// When connected to a powerbank (or dumb charger), it goes in autopilot mode
// When connected to a PC's USB port, it starts in "no service" mode (amber blinking)
// and it can then be controlled by serial commands:
//  o:       off/out of service mode (amber/orange blinking)
//  r:       change all lights to red immediately
//  n,s,e,w: change to amber, red for north, south, east and west respectively
//  h,v:     change to amber, red for east+west (horizontal) and north+south (vertical) respectively

//  N,S,E,W: change to green for north, south, east and west respectively
//  H,V:     change to green for east+west (horizontal) and north+south (vertical) respectively

// Uncomment this for 2 second red+amber lights before green (we'll call it pre-green)
// (used in UK, Germany, Czech Republic and other Central European countries)
// NOTE that this is not yet implemented in serial command mode
//#define PRE_GREEN

// Comment this out if autopilot does not work using a ("smart" charger)
#define SERIAL_COMMAND_MODE

// Choose 4 pins: for the digispark, pins 3 + 4 are for USB (serial)
const uint8_t g_pins[] = { 0, 1, 2, 5 };

// Order: pin-HIGH, pin-LOW
// Note that south is not implemented (255) since we have a T-junction
const uint8_t g_ledPins[] = {
  g_pins[0], g_pins[1], // Red N
  255, 255, //g_pins[1], g_pins[2], // Red S
  g_pins[2], g_pins[3], // Red E
  g_pins[0], g_pins[3], // Red W

  g_pins[1], g_pins[0], // Amber N
  255, 255, //g_pins[2], g_pins[1], // Amber S
  g_pins[3], g_pins[2], // Amber E
  g_pins[3], g_pins[0], // Amber W

  g_pins[2], g_pins[0], // Green N
  255, 255, //g_pins[0], g_pins[2], // Green S
  g_pins[1], g_pins[3], // Green E
  g_pins[3], g_pins[1], // Green W

};

#if defined(__AVR_ATtinyX5__)
  #include <DigiCDC.h>
  //#include "DigiKeyboard.h"
  #define LED_BUILTIN 1

  #define SLEEP(n) SerialUSB.delay(n)
#else
  #define SLEEP(n) delay(n)
#endif

#define OFF 0
// Red
#define RN (1 << 0)
#define RS (1 << 1)
#define RE (1 << 2)
#define RW (1 << 3)
#define RED (RN|RS|RE|RW)
// Amber (orange/yellow)
#define AN (1 << 4)
#define AS (1 << 5)
#define AE (1 << 6)
#define AW (1 << 7)
#define AMBER (AN|AS|AE|AW)
// Pre-green
#define PN (RN|ON)
#define PS (RS|OS)
#define PE (RE|OE)
#define PW (RW|OW)
// Green
#define GN (1 << 8)
#define GS (1 << 9)
#define GE (1 << 10)
#define GW (1 << 11)
// State machine delay mechanism
#define DELAY4 (4 << 12)
#define DELAY3 (3 << 12)
#define DELAY2 (2 << 12)
#define DELAY1 (1 << 12)

volatile uint16_t g_Lights = 0x0000;
uint8_t g_differential = 0;
void setup()
{

#ifdef __AVR_ATtinyX5__

#ifdef SERIAL_COMMAND_MODE
  // For the Digispark, try and detect USB differential signal
  for ( uint8_t n = 0; n < 255; ++n )
  {
    if ( digitalRead( 3 ) != digitalRead( 4 ) )
      g_differential++;
  }

  // No USB?
  if ( g_differential < 128 )
    g_differential = 0;

  // USB?
  if ( g_differential )
    SerialUSB.begin();
#endif

  // Digispark timer 0
  noInterrupts(); // disable all interrupts

  TCCR0A = 0x00;   // Normal mode (OC0A OC0B disconnected)
  TCCR0B = 0x00;

  // No prescaler (does not work!?)
  TCCR0B |= ( 1 << CS01 );

  interrupts(); // enable all interrupts, sei();
  TCNT0 = 0;
  //TCNT0 = 255; // Set timer count to near-overflow
  TIMSK |= ( 1 << TOIE0 ); //enabling timer0 interrupt
#else
  // Other's timer 1
  noInterrupts(); // disable all interrupts

  TCCR1A = 0x00;   // Normal mode (OC0A OC0B disconnected)
  TCCR1B = 0x00;

  // No prescaler (does not work!?)
  TCCR1B |= ( 1 << CS01 );

  interrupts(); // enable all interrupts, sei();
  TCNT1 = 0;
  TIMSK1 |= ( 1 << TOIE1 ); //enabling timer1 interrupt

#endif


#ifdef SERIAL_COMMAND_MODE
  // USB?
  if ( g_differential )
  {
    SLEEP( 750 );
    while (SerialUSB.available())
      SerialUSB.read();
  }
#endif
}

void loop()
{
  if ( g_differential )
  {
    // TODO: implement pre-green (red+amber)
    //       this needs rework of the state machine (and key input)
    //       since the to-be-green will be tied to its "opposing" light

    // Semi state machine (note that every traffic light needs to have a light on, apart from no service mode)
    switch ( g_Lights )
    {
        // No service mode
        case OFF: // off -> no service
          g_Lights = AMBER;
        break;
        case AMBER: // no service -> off
          g_Lights = OFF;
          break;

        // North, amber -> red
        case DELAY4|AN|RS|RE|RW: g_Lights = DELAY3|AN|RS|RE|RW; break;
        case DELAY3|AN|RS|RE|RW: g_Lights = DELAY2|AN|RS|RE|RW; break;
        case DELAY2|AN|RS|RE|RW: g_Lights = DELAY1|AN|RS|RE|RW; break;
        case DELAY1|AN|RS|RE|RW: g_Lights =        AN|RS|RE|RW; break;
        case        AN|RS|RE|RW: g_Lights =        RN|RS|RE|RW; break;

        // TODO: Implement South amber for 4-way intersection

        // East, amber -> red (red west)
        case DELAY4|RN|RS|AE|RW: g_Lights = DELAY3|RN|RS|AE|RW; break;
        case DELAY3|RN|RS|AE|RW: g_Lights = DELAY2|RN|RS|AE|RW; break;
        case DELAY2|RN|RS|AE|RW: g_Lights = DELAY1|RN|RS|AE|RW; break;
        case DELAY1|RN|RS|AE|RW: g_Lights =        RN|RS|AE|RW; break;
        case        RN|RS|AE|RW: g_Lights =        RN|RS|RE|RW; break;
        // East amber -> red (green west)
        case DELAY4|RN|RS|AE|GW: g_Lights = DELAY3|RN|RS|AE|GW; break;
        case DELAY3|RN|RS|AE|GW: g_Lights = DELAY2|RN|RS|AE|GW; break;
        case DELAY2|RN|RS|AE|GW: g_Lights = DELAY1|RN|RS|AE|GW; break;
        case DELAY1|RN|RS|AE|GW: g_Lights =        RN|RS|AE|GW; break;
        case        RN|RS|AE|GW: g_Lights =        RN|RS|RE|GW; break;

        // West, amber -> red (red east)
        case DELAY4|RN|RS|RE|AW: g_Lights = DELAY3|RN|RS|RE|AW; break;
        case DELAY3|RN|RS|RE|AW: g_Lights = DELAY2|RN|RS|RE|AW; break;
        case DELAY2|RN|RS|RE|AW: g_Lights = DELAY1|RN|RS|RE|AW; break;
        case DELAY1|RN|RS|RE|AW: g_Lights =        RN|RS|RE|AW; break;
        case        RN|RS|RE|AW: g_Lights =        RN|RS|RE|RW; break;
        // West amber -> red (green east)
        case DELAY4|RN|RS|GE|AW: g_Lights = DELAY3|RN|RS|GE|AW; break;
        case DELAY3|RN|RS|GE|AW: g_Lights = DELAY2|RN|RS|GE|AW; break;
        case DELAY2|RN|RS|GE|AW: g_Lights = DELAY1|RN|RS|GE|AW; break;
        case DELAY1|RN|RS|GE|AW: g_Lights =        RN|RS|GE|AW; break;
        case        RN|RS|GE|AW: g_Lights =        RN|RS|GE|RW; break;

        // Horizontal (east-west) amber -> red
        case DELAY4|RN|RS|AE|AW: g_Lights = DELAY3|RN|RS|AE|AW; break;
        case DELAY3|RN|RS|AE|AW: g_Lights = DELAY2|RN|RS|AE|AW; break;
        case DELAY2|RN|RS|AE|AW: g_Lights = DELAY1|RN|RS|AE|AW; break;
        case DELAY1|RN|RS|AE|AW: g_Lights =        RN|RS|AE|AW; break;
        case        RN|RS|AE|AW: g_Lights =        RN|RS|RE|RW; break;
    }

    SLEEP( 750 );

    if (SerialUSB.available())
    {
      char input = SerialUSB.read();
      switch ( input )
      {
        case 'n':                                               // North amber
        case 'v': g_Lights = DELAY4 + AN + RS + RE + RW; break; // Vertical amber

        case 'e':
          if ( g_Lights & GW )
            g_Lights = DELAY4 + RN + RS + AE + GW;
          else
            g_Lights = DELAY4 + RN + RS + AE + RW;
          break;
        case 'w':
          if ( g_Lights & GE )
            g_Lights = DELAY4 + RN + RS + GE + AW;
          else
            g_Lights = DELAY4 + RN + RS + RE + AW;
          break;
        case 'h':
          if ( g_Lights & ( GE | GW ) == ( GE | GW ) )
            g_Lights = DELAY4 + RN + RS + AE + AW; // East + West to amber
          else if ( g_Lights & GE )
            g_Lights = DELAY4 + RN + RS + AE + RW; // East to amber
          else if ( g_Lights & GW )
            g_Lights = DELAY4 + RN + RS + RE + AW; // West to amber
          break;

        case 'N':                                       // North green
        case 'V':
          // Only change if east and west ar not green
          if ( (g_Lights & ( GE | GW )) == 0 )
            g_Lights = DELAY4 + GN + RS + RE + RW;
          break; // Vertical green

        case 'E':
          // Only change if north and south ar not green
          if ( (g_Lights & ( GN | GS )) == 0 )
          {
            if ( g_Lights & GW )
              g_Lights = DELAY4 + RN + RS + GE + GW;
            else
              g_Lights = DELAY4 + RN + RS + GE + RW;
          }
          break;

        case 'W':
          if ( (g_Lights & ( GN | GS )) == 0 )
          {
            if ( g_Lights & GE )
              g_Lights = DELAY4 + RN + RS + GE + GW;
            else
              g_Lights = DELAY4 + RN + RS + RE + GW;
          }
          break;

        case 'H':
          if ( (g_Lights & ( GN | GS )) == 0 )
            g_Lights = DELAY4 + RN + RS + GE + GW;
          break;

        case 'o':
        case 's': g_Lights = OFF; break; // Off
        case 'r': g_Lights = RED; break; // all red

      }
    }
  }
  else
  {
    // No USB connected: autopilot

    // North + south green
    g_Lights = GN + GS + RE + RW;
    delay( 7000 );

    // North + south amber
    g_Lights = AN + AS + RE + RW;

#ifdef PRE_GREEN
    delay( 1000 );
    g_Lights = AN + AS + RE + AE + RW + AW;
    delay( 2000 );
#else
    delay( 3000 );
    
    // All red
    g_Lights = RED;
    delay( 500 );
#endif

    // East + west green
    g_Lights = RN + RS + GE + GW;
    delay( 7000 );

    // East + west amber
    g_Lights = RN + RS + AE + AW;
#ifdef PRE_GREEN
    delay( 1000 );
    g_Lights = RN + AN + RS + AS + AE + AW;
    delay( 2000 );
#else
    delay( 3000 );
    
    // All red
    g_Lights = RED;
    delay( 500 );
#endif
  }
}

// Interrupt vector for Timer0
uint8_t g_nLed = 0;
uint8_t g_nOldPins = 0;

// Digispark: timer 0, rest: timer 1
#if defined(__AVR_ATtinyX5__)
ISR (TIMER0_OVF_vect)
#else
ISR( TIMER1_OVF_vect )
#endif
{
  uint8_t pin = g_ledPins[ g_nOldPins ];

  if ( pin < 255 )
  {
    digitalWrite( pin, LOW );
    pinMode( pin, INPUT );
    pinMode( g_ledPins[ g_nOldPins + 1 ], INPUT );
  }

  g_nOldPins = g_nLed << 1;
  pin = g_ledPins[ g_nOldPins ];

  if ( pin < 255 )
  {
    pinMode( pin, OUTPUT );
    pinMode( g_ledPins[ g_nOldPins + 1 ], OUTPUT );

    digitalWrite( pin, ( g_Lights & ( 1 << g_nLed ) ) ? HIGH : LOW );
  }

  g_nLed++;
  if ( g_nLed >= 12 )
    g_nLed = 0;

  // Don't touch, doesn't work!
  // Set timer count to overflow with next tick
  //TCNT0 = 254;
}
