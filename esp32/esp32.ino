// Auto watering script for esp32 board.

// Pin definitions
#define WATER_SENS 26 // Water sensor
#define VALVE_PIN 32 // Valve pin

// global constants
#define WATER_SENS_GATE 2000 // Water sensor threshold, defines when water level is low & high
#define WATER_FLOW 12 // Defines how much units of water flows out in 1 sec of open valve
#define DAY 60000000 // Day in millis (minute for now)
#define SEC 1000000 // Second in millis

// global  variables
int days_left = 30; // days to go, used to distrube water evenly across days
int water_tank = 4000; // defines amount of water in the tank 
int daily_limit = 0; // defines how much water can be used every day
bool water_low; // holds info from the water sensor

// Function that calculates daily limit of water usage
// Returns water tank when there is no days left
int calc_daily_limit(int water_tank, int days_left) 
{
  if (days_left < 1) 
  {
    return water_tank;
  }

  return water_tank / days_left;
}

// Function that check and decides if valve should be opened or closed
void valve_handle() 
{
  Serial.println("");
  Serial.print("days left: ");
  Serial.print(days_left);
  Serial.print("  water level: ");
  Serial.print(water_tank);
  Serial.print("  limit today: ");
  Serial.print(daily_limit);
  Serial.println("");

  // check if valve should be opened
  // this is when water is low and daily limit allows for it
  if (water_low && daily_limit >= WATER_FLOW) 
  {

    // open valve
    digitalWrite(VALVE_PIN, 1);
    Serial.println(">> valve open >>");
    
    // subtract water 
    water_tank -= WATER_FLOW;
    daily_limit -= WATER_FLOW;
    
    return;
  }

  // otherwise close valve
  digitalWrite(VALVE_PIN, 0);
  Serial.println(">< valve closed ><");
}

// Function that updates stats daily
void day_update() 
{
  days_left--;
  daily_limit = calc_daily_limit(water_tank, days_left);

  Serial.println("");
  Serial.println("-- a day has passed --");
  Serial.println("");
  
}



// timer stuff

// timer for valve
hw_timer_t * valve_timer = NULL;
volatile SemaphoreHandle_t valve_timer_semaphore;

void IRAM_ATTR on_valve_timer()
{
  xSemaphoreGiveFromISR(valve_timer_semaphore, NULL);
}


// timer for day count
hw_timer_t * day_timer = NULL;
volatile SemaphoreHandle_t day_timer_semaphore;


void IRAM_ATTR on_day_timer()
{
  xSemaphoreGiveFromISR(day_timer_semaphore, NULL);  
}

void setup() {
  Serial.begin(9600);

  // Set Water sensor pin to input
  pinMode(WATER_SENS, INPUT);
  pinMode(VALVE_PIN, OUTPUT);

  // Create semaphores to inform us when the timer has fired
  valve_timer_semaphore = xSemaphoreCreateBinary();
  day_timer_semaphore = xSemaphoreCreateBinary();

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  valve_timer = timerBegin(0, 80, true);
  day_timer = timerBegin(1, 80, true);

  // Attach callback functions to timers.
  timerAttachInterrupt(valve_timer, &on_valve_timer, true);
  timerAttachInterrupt(day_timer, &on_day_timer, true);

  // Set alarm to call callback functions periodically (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(valve_timer, SEC, true);
  timerAlarmWrite(day_timer, DAY, true);

  // Start an alarm
  timerAlarmEnable(valve_timer);
  timerAlarmEnable(day_timer);

  //initialize day 0
  day_update();
}

void loop() {

  // read and update water level status
  water_low = (analogRead(WATER_SENS) < WATER_SENS_GATE);


  //if second has passed, handle water valve
  if (xSemaphoreTake(valve_timer_semaphore, 0) == pdTRUE)
  {
    valve_handle(); 
  }


  //if day has passed, do daily update
  if (xSemaphoreTake(day_timer_semaphore, 0) == pdTRUE)
  {
    day_update();
  }

}
