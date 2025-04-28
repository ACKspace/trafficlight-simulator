// This traffic light simulator uses an ESP8266 to control a T-junction or intersection

// Uncomment this for 2 second red+amber lights before green (we'll call it pre-green)
// (used in UK, Germany, Czech Republic and other Central European countries)
// NOTE that this is not yet implemented in serial command mode
//#define PRE_GREEN

#define DEBUG

// Replace with your network credentials
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

// Order of the pins and their cabling:
// 10: red-red
// 2: blue-orange-white
// 4: red-white
// 5: orange-white
const uint8_t g_pins[] = { 10, 2, 4, 5 };

// Order: pin-HIGH, pin-LOW
// Note that south is not implemented (255) since we have a T-junction
const uint8_t g_ledPins[13][2] = {
  {g_pins[0], g_pins[1]}, //  0 Red N
  {g_pins[1], g_pins[2]}, //  1 Red S
  {g_pins[2], g_pins[3]}, //  2 Red E
  {g_pins[0], g_pins[3]}, //  3 Red W

  {g_pins[1], g_pins[0]}, //  4 Amber N
  {g_pins[2], g_pins[1]}, //  5 Amber S
  {g_pins[3], g_pins[2]}, //  6 Amber E
  {g_pins[3], g_pins[0]}, //  7 Amber W

  {g_pins[2], g_pins[0]}, //  8 Green N
  {g_pins[0], g_pins[2]}, //  9 Green S
  {g_pins[1], g_pins[3]}, // 10 Green E
  {g_pins[3], g_pins[1]}, // 11 Green W

  {255, 255},             // 12 "macro" for no output
};

// Actual lights to set to on
// Note that we're setting values twice so we can set the red+amber combination as well
// 12 means none (for blinking mode)
uint8_t g_lightsOn[6] = {
  0,
  2,
  3,
  0,
  2,
  3,
};

#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>     // ESPAsyncWebSrv 1.2.9 by dvarrel (originally https://github.com/me-no-dev/ESPAsyncWebServer/archive/master.zip )
#include <ESPAsyncWebSrv.h>  // ESPAsyncTCP 1.2.4 by dvarrel (originally https://github.com/me-no-dev/ESPAsyncWebServer/archive/master.zip )

// Create AsyncWebServer object on port 80
AsyncWebServer server( 80 );
AsyncWebSocket ws( "/ws" );


const char index_html[] PROGMEM = R"rawliteral(<html>
<head>
<style>
button {
    padding: 1em;
}
fieldset {
    display: inline-block;
}
.r { accent-color: red; color: red; }
.a { accent-color: orange; color: orange;}
.g { accent-color: limegreen; color: limegreen; }
section {display:flex;}
#p div {  }
#p > div {
    padding: 1em;
    border: 1px solid black;
    border-radius: 1em;
    margin: 0.5em;
    cursor:cell;
}
</style>
<script type="text/javascript">
const gateway = window.location.host ? `ws://${window.location.host}/ws` : null;
let websocket;
window.addEventListener( "load", load );
function initWebSocket()
{
    if ( !gateway )
        return;
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
}
function onOpen(event)
{
    console.log('Connection opened');
}
function onClose(event)
{
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}
function onMessage({data})
{
  const lights = data.split(":")
  lights.forEach(v => {
    const i = document.querySelector(`[value='${v}']`);
    if (i) i.checked = true;
  });
}
function setLight(name, value)
{
    const index = parseInt(name);
    lights[index] = value;
    lights[index+ 3] = value;

    websocket && websocket.send(lights.join(":"));
}

const a = [4,6,7];
const o = [12,12,12];
const r = [0,2,3];

let lights = [12,12,12,12,12,12];
const program = [
];

function empty() {
    state = 0;
    program.length = 0;
    list();
}


function outOfOrder() {
    state = 0;
    program.length = 0;
    program.push(a, o);
    list();
}

function normal() {
    state = 0;
    program.length = 0;
    program.push(
    //   N  E  W
        [8, 2, 3], // GRR
        20,
        [4, 2, 3], // ORR
        6,
        [0, 2, 3], // RRR
        2,

        [0,10, 3], // RGR
        20,
        [0, 6, 3], // ROR
        6,
        [0, 2, 3], // RRR
        2,

        [0, 2,11], // RRG
        20,
        [0, 2, 7], // RRO
        6,
        [0, 2, 3], // RRR
        2,
    );
    list();
}

function programLight(i,l) {
    if (!Array.isArray(program[program.length-1])) program.push([0, 2, 3]);

    program[program.length-1][i] = l;
    list();
}

function programPause(l) {
    if (typeof program[program.length-1] !== "number") program.push(1);
    program[program.length-1] = l;
    list();
}

function remove(l) {
    if (!program[l]) return;
    program.splice(l,1);
    list();
}

function list() {
    const p = document.getElementById("p");
    p.innerHTML = program.map((p,i) => {
        if (Array.isArray(p)) {
            return `<div onclick="remove(${i})">
                ${p.map((l) => {
                    const c = [l / 4 | 0];
                    const d = ["noord", "zuid","oost", "west"][l % 4];
                    return `<div class="${["r", "a", "g"][c]}">${c > 2 ? "" : d} ${["rood", "oranje", "groen", "uit"][c]}</div>`;
                }).join("")}
            </div>`;
        } else {
            return `<div onclick="remove(${i})">${p / 2} seconde pauze</div>`;
        }
    }).join("\n");
}

let state = 0;
function load()
{
  initWebSocket();
  document.querySelectorAll("input[type='radio']").forEach(r=>r.onchange = ({target}) => setLight(target.name, target.value))
  nextStep();
}
function nextStep()
{
    let value = program[state];

    // Program next-state with wraparound
    state++;
    if (state >= program.length) state = 0;

    if (Array.isArray(value)) {
        if (value.length < 6)
            lights = value.concat(value);
        else
            lights = value;


        // onMessage({data: lights.join(":")})
        websocket?.readyState == 1 && websocket.send(lights.join(":"));

        // Next item should be pause
        if (Array.isArray(program[state])) {
            setTimeout(nextStep, 500);
        } else {
            value = program[state];

            state++;
            if (state >= program.length) state = 0;
            setTimeout(nextStep, 500 * value);
        }
    } else {
        // Pause (unexpected)
        setTimeout(nextStep, 500 * (value ?? 1));
    }
}
</script>
</head>
<body>
<section>
<div>
<fieldset>
  <legend>noorden</legend>
  <div>
    <input type="radio" id="north_red" class="r" name="0" value="0" checked />
    <label for="north_red">rood</label>
  </div>

  <div>
    <input type="radio" id="north_amber" class="a" name="0" value="4" />
    <label for="north_amber">oranje</label>
  </div>

  <div>
    <input type="radio" id="north_green" class="g" name="0" value="8" />
    <label for="north_green">groen</label>
  </div>
</fieldset>

<fieldset>
  <legend>oosten</legend>
  <div>
    <input type="radio" id="east_red" class="r" name="1" value="2" checked />
    <label for="east_red">rood</label>
  </div>

  <div>
    <input type="radio" id="east_amber" class="a" name="1" value="6" />
    <label for="east_amber">oranje</label>
  </div>

  <div>
    <input type="radio" id="east_green" class="g" name="1" value="10" />
    <label for="east_green">groen</label>
  </div>
</fieldset>


<fieldset>
  <legend>westen</legend>
  <div>
    <input type="radio" id="west_red" class="r" name="2" value="3" checked />
    <label for="west_red">rood</label>
  </div>

  <div>
    <input type="radio" id="west_amber" class="a" name="2" value="7" />
    <label for="west_amber">oranje</label>
  </div>

  <div>
    <input type="radio" id="west_green" class="g" name="2" value="11" />
    <label for="west_green">groen</label>
  </div>
</fieldset>

<div>
<button onclick="outOfOrder()">knipperen</button>
<button onclick="normal()">normaal programma</button>
<button onclick="empty()">wis lijst</button>
</div>
<div>
<button onclick="programPause(1)">pauze 0,5 seconde</button>
<button onclick="programPause(2)">pauze 1 seconde</button>
<button onclick="programPause(6)">pauze 3 seconde</button>
<button onclick="programPause(20)">pauze 10 seconde</button>
</div>
<div>
<button onclick="programLight(0,0)" class="r">noord</button>
<button onclick="programLight(0,4)" class="a">noord</button>
<button onclick="programLight(0,8)" class="g">noord</button>
</div>
<div>
<button onclick="programLight(1,2)" class="r">oost</button>
<button onclick="programLight(1,6)" class="a">oost</button>
<button onclick="programLight(1,10)" class="g">oost</button>
</div>
<div>
<button onclick="programLight(2,3)" class="r">west</button>
<button onclick="programLight(2,7)" class="a">west</button>
<button onclick="programLight(2,11)" class="g">west</button>
</div>
</div>
<div id="p">
</div>
</section>
</body>
</html>
)rawliteral";

Ticker blinker;
volatile uint16_t g_Lights = 0x0000;
uint8_t g_differential = 0;

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if ( info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT )
    {
        data[len] = 0;

        // Notify others
        ws.textAll((char*)data );
        
        char* next;
        uint8_t onoff = 0;
        uint8_t velocity = 1;

        // Parse number (the ugly way)
        // Parse number (the ugly way)
        g_lightsOn[0] = strtoul( (char*)data, &next, 0 );
        if ( (char*)data + len <= next ) return;
        g_lightsOn[1] = strtoul( next + 1, &next, 0 );
        if ( (char*)data + len <= next ) return;
        g_lightsOn[2] = strtoul( next + 1, &next, 0 );
        if ( (char*)data + len <= next ) return;
        g_lightsOn[3] = strtoul( next + 1, &next, 0 );
        if ( (char*)data + len <= next ) return;
        g_lightsOn[4] = strtoul( next + 1, &next, 0 );
        if ( (char*)data + len <= next ) return;
        g_lightsOn[5] = strtoul( next + 1, &next, 0 );
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch ( type )
    {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
      case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
    }
}

void initWebSocket()
{
    ws.onEvent(onEvent);
    server.addHandler( &ws );
}

String variableSubstitution(const String& var)
{
  Serial.println( var );
  //if( var == "STATE" ) // %STATE%
  return "";
}



//#include <arduino_gpio.h>

#define GPIO_OUT *(uint32_t *)0x60000300
#define GPIO_OUT_W1TS *(uint32_t *)0x60000304
#define GPIO_OUT_W1TC *(uint32_t *)0x60000308
#define GPIO_ENABLE *(uint32_t *)0x6000030C
//

//extern volatile uint32_t GPIO_OUT;
//extern volatile uint32_t GPIO_ENABLE;

void setup()
{  
  Serial.begin(115200);
  
  // Connect to Wi-Fi
  Serial.print( F( "\nConnecting to WiFi." ) );
  WiFi.mode(WIFI_STA);
  WiFi.begin( ssid, password );
  while ( WiFi.status() != WL_CONNECTED )
  {
      delay( 500 );
      Serial.print( '.' );
  }
  Serial.println(WiFi.localIP());

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, []( AsyncWebServerRequest *request )
  {
      request->send_P( 200, "text/html", index_html, variableSubstitution );
  } );

  // Start server
  server.begin();

  // Timer to contol charlie plexing
  blinker.attach_ms(1, changeState);
}


// Always 0, 3 or 6 LEDs on
void setCharlieplexPins(uint8_t highPin, uint8_t lowPin) {

  // Pin mask, leave the rest alone
  uint32_t mask = 0b110101;

  // All outputs low:
  digitalWrite(10, LOW);
  digitalWrite(2, LOW);
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);

  // pin out of bounds means don't enable any pins
  if (highPin > 16) return;

  // All input
  pinMode(10, INPUT);
  pinMode(2, INPUT);
  pinMode(4, INPUT);
  pinMode(5, INPUT);

  // Set 2 outputs
  pinMode(highPin, OUTPUT);
  pinMode(lowPin, OUTPUT);
  
  // Set single output high
  digitalWrite(highPin, HIGH);

//  // All outputs low:
//  GPIO_OUT_W1TC |= mask;
//
////  // Set all as input
////  GPIO_ENABLE &= ~mask;
////  // Set output (always 2)
////  GPIO_ENABLE |= (((1 << highPin) | (1 << lowPin)) & mask);
//
//  // Set all as input
//  GPIO_ENABLE |= mask;
//  // Set output (always 2)
//  GPIO_ENABLE &= ~(((1 << highPin) | (1 << lowPin)) & mask);
//
//
//
//  GPIO_OUT_W1TS |= ((1 << highPin) & mask);


//  // All outputs low
//  GPIO_OUT &= ~mask;
//
//  // Set new outputs
//  // Set all as input
//  GPIO_ENABLE &= ~mask;
//
//  // Set output (always 2)
//  GPIO_ENABLE |= (((1 << highPin) | (1 << lowPin)) & mask);
//
//  // Set single output high
//  GPIO_OUT |= ((1 << highPin) & mask);
}

////////////////////////////////////////////////////////////////////////////////////
void loop() {
  ws.cleanupClients();

//  g_lightsOn[0] = 12;

}


// Timer interrupt
volatile uint8_t charliePlexOn = 0;
void changeState()
{

  setCharlieplexPins(g_ledPins[g_lightsOn[charliePlexOn]][0], g_ledPins[g_lightsOn[charliePlexOn]][1]);


  // Prepare next led to display
  charliePlexOn++;
  if (charliePlexOn >= sizeof(g_lightsOn))
    charliePlexOn = 0;


//  uint8_t pin1 = g_ledPins[ g_nOldPins ];
//  uint8_t pin2 = g_ledPins[ g_nOldPins + 1 ];
//
//  digitalWrite( pin1, LOW );
//  digitalWrite( pin2, LOW );
//
//  pinMode( pin1, INPUT );
//  pinMode( pin2, INPUT );
//
//  g_nOldPins = g_nLed << 1;
//  pin1 = g_ledPins[ g_nOldPins ];
//  pin2 = g_ledPins[ g_nOldPins + 1 ];
//
//  pinMode( pin1, OUTPUT );
//  pinMode( pin2, OUTPUT );
//
//  digitalWrite( pin1, ( g_Lights & g_nOldPins ) ? HIGH : LOW );
//  digitalWrite( pin2, ( g_Lights & g_nOldPins + 1 ) ? HIGH : LOW );
//
//  g_nLed++;
//  if ( g_nLed >= 12 )
//    g_nLed = 0;
}
