#include <pebble.h>
/* notes for future additions:
  Allow inverting display colors. If background and text color are the same make the text white or black to avoid conflict.
     Mechanism. notBack (not background color) = black if background is not black, else white.
*/
// greebo trouble shooting counters and bool
static long ga; // track number of times timeHandler called early 
static long gb;
static long lastTime;
static int alarmTime;  // keep for regular use

// variables for settings
static uint8_t tritScheme; // choose character set for display trits
static bool yesSeconds = true; // control seconds
static bool wasSeconds = false; 
static uint8_t colors [5] =  // color
    {(uint8_t)0b11110000, (uint8_t)0b11000000, (uint8_t)0b11000011, (uint8_t)0b00000000, 
    (uint8_t)0b11000000};
    // indexes 0 negativ, 1 neutral, 2 positive, 3 background clear, 4 monocrome text black.
bool yesColor = PBL_IF_COLOR_ELSE(true, false); // does the display display colors.
static char pick [27][2] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O"
                          , "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[" };
static int pickLengthThird = 27 / 3; // pick lenght
   // character shcemes
// randomly change characte scheme
bool yesRandSec = false;
bool yesRandMin = true; // greebo
bool yesRandHour = false; 

static AppTimer *timeHandle;
// controlling timeHandler
static int64_t nextTime; // the time in ms from 1970 to the next time update
static long now; // the current time in ms from start of day.
static uint8_t hour; // hours of expected update
static uint8_t min; 
static uint8_t sec;
static uint8_t lastTimeTryte[11];
static uint8_t thisTimeTryte[11];
static uint8_t sixtyOne [61][4]; // contains conversions for minutes and seconds
static uint8_t twentyFive [25][3]; // contains conversions for hours
static uint8_t startFour[4] = {1, 0, 1, 0};
static uint8_t startThree[3] = {1, 0, 0}; 
static long hourLongs[25]; // refresh ms for hours
static long minLongs[61]; // refresh ms for minutes
static long secLongs[61]; 
static int64_t yesterday; // pervious midnight
static int64_t tomorrow; // next midnight
static int64_t thisTime; // ms since 1970 aka epoc.
static long timeSlate; // store manapulations of now.
// static struct tm tuesday_t; // used to check if time changed.
static time_t fetch_t; // used to fetch time as seconds since 1970.
static time_t wasToday; // keep start of today for comparison.
static uint16_t fetch_ms; // stores milliseconds since second.
static bool changeHour = false;
static bool changeMinute = false;
static bool hourChanged = false; // had the hour been changed.
static bool minChanged = false; 

static Window *s_main_window;

// make an array of layers
static int i; // counter for loops
static int j; // secondary counter 
static int k; // third counter

static TextLayer* layers[11];
static GFont s_font;

// none -> bool
static bool timeChanged(){
  return wasToday != time_start_of_today(); 
}

// none -> none
static void yesterdayTomorrow(){
  wasToday = time_start_of_today();
  yesterday = (int64_t)wasToday * 1000;
  tomorrow = yesterday + 86400000; 
}

// none -> none
// get time in milliseconds
static int64_t fetchTime(){
  time_ms(&fetch_t, &fetch_ms);
  return ((int64_t)fetch_t) * 1000 + fetch_ms;
}
// none -> none
static void findNow(){
  thisTime = fetchTime();
  now = (long)(thisTime - yesterday);
}
// long *, long * , int -> int 
// compare time to array of times.
static int compareToLongs(long * time, long * timeList_pointer, int size){
  size /= (int)sizeof(timeList_pointer); 
  i = 0; while (i < size && *time >= (timeList_pointer)[i] ){i++;}  
  return i;
}

// int -> none
// updates the hour in thisTimeTryte
static void writeHour(){
   for (i = 0; i < 3; i++){thisTimeTryte[i] = twentyFive[hour][2-i];}
}
// int -> none
// updates the hour in thisTimeTryte
static void writeMinute(){
   for (i = 0; i < 4; i++){thisTimeTryte[3+i] = sixtyOne[min][3-i];}
}
// int -> none
// updates the hour in thisTimeTryte
static void writeSecond(){
   for (i = 0; i < 4; i++){thisTimeTryte[7+i] = sixtyOne[sec][3-i];}
}
// none -> none
// find hour minute second
static void hms(){
  // code for attempting to pass array without pointer decay.
  hour = compareToLongs(&now, hourLongs, (int)sizeof(hourLongs) ); 
  timeSlate = now - hourLongs[hour] + 3600000;
  min = compareToLongs(&timeSlate, minLongs, (int)sizeof(minLongs));
  if (yesSeconds)
  { timeSlate += 60000 - minLongs[min];  
    sec = compareToLongs(&timeSlate, secLongs, (int)sizeof(secLongs)); 
  }
  changeHour = true; changeMinute = true;
}
// none -> none
// function for handeling chage of day.
static void newDay(){
  // /* 
     APP_LOG(APP_LOG_LEVEL_DEBUG, "timeChanged %d", timeChanged());
  // */
  yesterdayTomorrow();
  findNow();
  hms();
}
// none -> none
// clears seconds
static void clearSeconds(){
  for (i = 7; i < 11; i ++){
    text_layer_set_text(layers[i], "");
    lastTimeTryte[i] = -1;
  }
}
// function for updating the display for time
// none -> none
static void updateTime() {
  if (!yesSeconds && wasSeconds){wasSeconds = false; clearSeconds();}
    if (yesRandSec || (yesRandMin && minChanged) || (yesRandHour && hourChanged)){
    minChanged = false; hourChanged = false; 
    i = (rand() % (pickLengthThird - 1)) * 3; 
    j = tritScheme; // greebo
    tritScheme = (i + ((i < tritScheme) ? 0 : 3));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "scheme %d, last scheme %d, random %d, pickThird %d",
        tritScheme, j, i, pickLengthThird); // greebo
  }
  for (i = 0; i < (yesSeconds ? 11 : 7); i ++){
    if (thisTimeTryte[i] != lastTimeTryte[i]){
       if (yesColor) text_layer_set_text_color(layers[(i)], (GColor)colors[thisTimeTryte[i]]);
       text_layer_set_text(layers[i], pick[thisTimeTryte[i] + tritScheme]);
       lastTimeTryte[i] = thisTimeTryte[i]; 
    }
  }
}
// none -> int
// find the time to wait in milliseconds.
static int waitMS(){
   hourChanged = changeHour; minChanged = changeMinute;
   if (changeHour) {changeHour = false; writeHour();}
   if (changeMinute){changeMinute = false; writeMinute();}
   if (yesSeconds) writeSecond();
   nextTime = yesterday + hourLongs[hour] + minLongs[min] + 
       (yesSeconds ? secLongs[sec] - ((min == 60 && sec == 30) ? 3660500 : 3660000)
        : (min == 60 ? -3630000 : -3600000)); 
   if (yesSeconds)
   { if (sec == 30 && min==30 && hour == 24) {nextTime = tomorrow; return nextTime - thisTime;}
     if (sec == 60)
     { sec = 0; 
       if (min != 60){min++; changeMinute = true; }
       else {min = 0; hour++; }
     } else if (min == 60 && sec == 30) { min = 0; hour++; changeMinute = true; changeHour = true; }
     else sec++; 
   }
   else
   { if (min==30 && hour == 24) {nextTime = tomorrow; return nextTime - thisTime;}
     if (min != 60){min++; changeMinute = true; }
     else {min = 0; hour++; changeMinute = true; changeHour = true;}
   }
   return nextTime - thisTime; 
}
static void timeHandler(){
  lastTime = nextTime; // greebo
  findNow(); 
  // /* 
  APP_LOG(APP_LOG_LEVEL_DEBUG, "hour %d, min %d, sec %d over sleep %ld, early %ld, retry %ld, alarmTime %d", 
        hour, min, sec,(long)(thisTime - nextTime), ga, gb, alarmTime);
  // */
  if (thisTime < nextTime) // greebo 
  { // /* 
       ga++; // timeHandler called before scheduled time of alarm 
       APP_LOG(APP_LOG_LEVEL_DEBUG, "early %ld, thisTime %ld, nextTime %ld", 
          (long)(thisTime - nextTime), (long)thisTime, (long)nextTime);
    // */
    app_timer_cancel(timeHandle);  // sends an error messeage if time elapsed
  }
  if (timeChanged() || thisTime > tomorrow) newDay(); // the day rolled over 
  else if (abs(thisTime - nextTime) > 100) newDay(); // if called by service
  alarmTime = waitMS(); 
  if (alarmTime < 50) {
  // /*   
     gb++; // greebo
     APP_LOG(APP_LOG_LEVEL_DEBUG, "OH FRELL!!! thisTime-nextTime %ld, retry %ld, alarm time %d", 
         (long)(thisTime - lastTime), gb, alarmTime);
  // */
     return timeHandler();
  } 
  // lastTime = nextTime; // greebo
  timeHandle = app_timer_register(alarmTime, timeHandler, NULL);
  updateTime(); 
}
static void main_window_load(Window *window) {
  
  //Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // load founts
  s_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DVSMBTernary36));
  
  // by John3136
  // Note the sizeof() stuff means this works unchanged even if you change
  // the number of layers.
  for(i = 0; i < (short)(sizeof(layers) / sizeof(layers[0])); i++) // uses (short) to +- unsigned interger
  {
      layers[i] = text_layer_create(GRect(
                                          PBL_IF_ROUND_ELSE(bounds.size.w/6, 0) + // increase boarder for round.
                                          (bounds.size.w/PBL_IF_ROUND_ELSE(6, 4))*((i + 1)%4), 
                                          (bounds.size.h/PBL_IF_ROUND_ELSE(5,4))*((i > 2) 
                                                                                  ? ((i > 6) 
                                                                                     ? 3 
                                                                                     : 2 ) 
                                                                                  : 1), 
                                          (bounds.size.w / PBL_IF_ROUND_ELSE(5, 4)) ,(bounds.size.h/PBL_IF_ROUND_ELSE(5,4))));
  }

  // Improve the layout to be more like a watchface
  for (i = 0; i < (short)(sizeof(layers) / sizeof(layers[0])); i ++){
     text_layer_set_background_color(layers[i], (GColor)colors[3]);
     text_layer_set_text_color(layers[i], (GColor)colors[4]);
     text_layer_set_font(layers[i], s_font); //s_font);
     text_layer_set_text_alignment(layers[i], GTextAlignmentCenter);
     layer_add_child(window_layer, text_layer_get_layer(layers[i]));
  }
}
static void main_window_unload(Window *window){
    fonts_unload_custom_font(s_font); 
for(i = 0; i < (short)(sizeof(layers) / sizeof(layers[0])); i++) 
    {
      text_layer_destroy(layers[i]); 
    }
  
}
static void init(){
  srand(time(NULL)); 
  // ensure that first time run is written.
  for (i = 0; i < /*sizeShort(lastTimeTryte)*/(signed)(sizeof(lastTimeTryte)/
        sizeof(lastTimeTryte[0])); i++){lastTimeTryte[i] = -1; }
  // define arrays for finding time
  for (i = 0 ; i < 25; i++) { hourLongs[i] = i * 3600000 + 1800000; }
     // note the 60th and the 0th minute are the same, the seconds make the adjustment
     // that is the reason for the shift from 30k to -30k instead of 30k to 0.
  for (i = 0 ; i < 61; i++) { minLongs[i] = i * 60000 + 30000;} // (i < 60 ? 30000 : 0);}
  for (i = 0 ; i < 61; i++){ secLongs[i] = i * 1000 + (i < 60 ? 500 : 0); }
  // define conversion arrays
  for (k = 0; k < 3; k++) {twentyFive[0][k] = startThree[k]; }
  for (i = 0; i < (signed)(sizeof(twentyFive) / sizeof(twentyFive[0])); i++){
    for (j = 0; j < (signed)(sizeof(twentyFive[0]) / sizeof(twentyFive[0][0])); j++){
      if (j == 0 && i != 0) (twentyFive[i][j])++;  
      if (twentyFive[i][j] == 3){ twentyFive[i][j]=0; (twentyFive[i][j + 1])++; }
      if (j ==  (signed)(sizeof(twentyFive[0]) / sizeof(twentyFive[0][0])) - 1
              && i != (signed)(sizeof(twentyFive) / sizeof(twentyFive[0]))- 1 ) {
        for (k = 0; k < 3; k++) { twentyFive[i + 1][k] = twentyFive[i][k];} }
    }
  }
  for (k = 0; k < 4; k++) {sixtyOne[0][k] = startFour[k]; }
  for (i = 0; i < (signed)(sizeof(sixtyOne) / sizeof(sixtyOne[0])); i++){
    for (j = 0; j < (signed)(sizeof(sixtyOne[0]) / sizeof(sixtyOne[0][0])); j++){
      if (j == 0 && i != 0) (sixtyOne[i][j])++;  
      if (sixtyOne[i][j] == 3){ sixtyOne[i][j]=0; (sixtyOne[i][j + 1])++; }
      if (j ==  (signed)(sizeof(sixtyOne[0]) / sizeof(sixtyOne[0][0])) - 1
              && i != (signed)(sizeof(sixtyOne) / sizeof(sixtyOne[0]))- 1 ) {
        for (k = 0; k < 4; k++) { sixtyOne[i + 1][k] = sixtyOne[i][k];} }
    }
  }


  // Create main Window element and assign to pointer
  s_main_window = window_create();
  
  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true); 
  
  // Register with TickTimerService
  // tick_timer_service_subscribe (SECOND_UNIT, timeHandler);
  timeHandler();
}

static void deinit(){
  // Destroy Window
  app_timer_cancel(timeHandle); 
  window_destroy(s_main_window); 
}

int main(void){
  init();
  app_event_loop();
  deinit();
}