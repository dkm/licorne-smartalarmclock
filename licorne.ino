// -*- mode: c++; -*-
// Because arduino uses .ino instead of already existing .C/.cpp...

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Marc POULHIÈS <dkm@kataplop.net>

#include "FastLED.h"
#include <DS3231M.h>
#include <Wire.h>

#include "config.h"

static DS3231M_Class DS3231M; // Create an instance of the DS3231M

#define DATA_PIN 3
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS 6
#define BRIGHTNESS 96
static CRGB leds[NUM_LEDS];

// Simple FSM handling with optionnal [ugly] debug routines.
#if WEAK_DEBUG
#define dwprint(x) Serial.println(x)
#else
#define dwprint(x)
#endif

void set_leds (CRGB c)
{
  dwprint("leds");

  for (int i = 0; i < NUM_LEDS; ++i)
    leds[i] = c;

  FastLED.show();
}

static enum control_state_e
  {
   STARTUP = -1,
   IDLE = 0,
   WAIT_ALARM,
   WAIT_ENTER,
   ENTERING,
   IN,
   EXITING,
  } control_state = STARTUP;

#if WEAK_DEBUG
static bool new_state = true;

#define STATE(name)				\
  if (new_state)                                \
    {                                           \
      Serial.print("ENTERING STATE: ");		\
      Serial.println(#name);			\
      new_state = false;                        \
    }
#define NEXT_STATE(name)			\
  do                                            \
    {                                           \
      new_state = true;				\
      control_state = name;			\
    } while(0)

#else

#define STATE(name)
#define NEXT_STATE(name)			\
  control_state = name;
#endif

// if defined, this will set the time at startup. Used for debugging.
#define FAKE_DATE_STARTUP "23:58:30"

// Stupid dump of DateTime object
static const void debugDateTime(const DateTime &d)
{
#if WEAK_DEBUG
  Serial.print(d.year());              // Extract year from DateTime
  Serial.print(" ");
  Serial.print(d.month());             // Extract month from DateTime
  Serial.print(" ");
  Serial.print(d.day());               // Extract day from DateTime
  Serial.print(" ");
  Serial.print(d.hour());              // Extract hour from DateTime
  Serial.print(":");
  Serial.print(d.minute());            // Extract minute from DateTime
  Serial.print(":");
  Serial.print(d.second());            // Extract second from DateTime
  Serial.print(" ");
  Serial.print(d.dayOfTheWeek());      // Extract DOW, Monday=1, Sunday=7
  Serial.print(" ");
  Serial.print(d.secondstime());       // Number of seconds since 2000-01-01 00:00:00
  Serial.print(" ");
  Serial.println(d.unixtime());          // Number of seconds since 1970-01-01 00:00:00
#endif
}

// Returns TRUE if a's time is lower than b's.
// a < b
static bool is_time_before (const DateTime &a, const DateTime &b)
{
  if (a.hour() < b.hour())
    return true;
  if (a.hour() > b.hour())
    return false;

  if (a.minute() < b.minute())
    return true;
  if (a.minute() > b.minute())
    return false;
  return a.second() < b.second();
}

// Returns TRUE if a's time is lower than or equal to b's.
// a <= b
static bool is_time_before_eq (const DateTime &a, const DateTime &b)
{
  return (a.hour() == b.hour() && a.minute() == b.minute () && a.second () == b.second ())
    || is_time_before (a, b);
}

// Returns TRUE if l <= t < u
static bool is_time_between (const DateTime &l, const DateTime &t, const DateTime &u)
{
  return is_time_before_eq (l, t) && is_time_before (t, u);
}

void setAlarm(uint8_t type, bool alarm_for_entering);

class Range_element {
public:
  virtual void enter(void)
  {
    setAlarm (secondsMinutesHoursMatch, false);
    NEXT_STATE (ENTERING);
  };

  virtual void in(void) {};

  virtual void exit(void)
  {
    NEXT_STATE (EXITING);
  };

  Range_element (const DateTime &start, const DateTime &end): mstart(start), mend(end), next_alarm_is_entering(true), mnext(NULL) {};

  bool is_in_range (void) const
  {
    DateTime now = DS3231M.now();
    bool ret = false;

    debugDateTime(now);
    debugDateTime(this->mstart);
    debugDateTime(this->mend);

    // [-----[SSSSSS[--------]
    if (is_time_before_eq (this->mstart, this->mend))
      {
        ret = is_time_between (this->mstart, now, this->mend);
      }
    // [SSSSS[----------[SSSS[]
    else
      {
        static DateTime last(__DATE__, "23:59:59");
        static DateTime first(__DATE__, "00:00:00");

        ret = is_time_between (this->mstart, now, last);
        ret |= is_time_between (first, now, this->mend);
      }

    dwprint("in_range: ");
    dwprint(ret);
    return ret;
  };

  void alarm(void)
  {
    if (next_alarm_is_entering)
      this->enter();
    else
      this->exit();
  };

  const DateTime &mstart, &mend;
  bool next_alarm_is_entering;
  Range_element *mnext;
};

class Bed_element : public Range_element {

public:
  Bed_element(const char* name, const DateTime &start, const DateTime &end, CRGB color_enter, CRGB color_exit) : Range_element(start, end), m_enter(color_enter), m_exit(color_exit), mname(name) {};

  CRGB m_enter, m_exit;
  const char* mname;

  void enter (void)
  {
    dwprint("enter");
    dwprint (mname);
    set_leds (m_enter);
    Range_element::enter();
  }

  void in(void)
  {
    static bool seen = false;
    if (!seen)
      {
        dwprint("in");
        dwprint (mname);
        seen = true;
      }
    delay (1000);
    Range_element::in();
  }

  void exit (void)
  {
    dwprint("exit");
    dwprint (mname);
    set_leds (m_exit);
    Range_element::exit();
  }
};

static const DateTime story_time_start (__DATE__, "13:30:00");
static const DateTime story_time_end (__DATE__,  "14:00:00");
Bed_element b0 = Bed_element ("story", story_time_start, story_time_end, CRGB::Blue, CRGB::Pink);

static const DateTime quiet_time_start (__DATE__, "14:01:00");
static const DateTime quiet_time_end (__DATE__,  "15:00:00");
Bed_element b1 = Bed_element ("quiet", quiet_time_start, quiet_time_end, CRGB::Pink, CRGB::Green);

static const DateTime bed_time_start (__DATE__, "20:30:00");
static const DateTime bed_time_end (__DATE__,  "07:00:00");
Bed_element b2 = Bed_element ("bed", bed_time_start, bed_time_end, CRGB::Pink, CRGB::Green);

Range_element *elements[] = {&b0, &b1, &b2};

Range_element *prev(Range_element *t)
{
  for (Range_element *e: elements)
    {
      if (e->mnext == t)
        return e;
    }
  return NULL;
}


Range_element *current_range = elements[0];

void debugRangeElement(Range_element *e)
{
  dwprint ("Range element start");
  debugDateTime (e->mstart);
  debugDateTime (e->mend);
  dwprint ("Range element stop");
}

// switches off all LEDs
void showProgramCleanUp(long delayTime)
{
  for (int i = 0; i < NUM_LEDS; ++i)
    leds[i] = CRGB::Black;

  FastLED.show();
  delay(delayTime);
}

void startup_sanity_check (void)
{
  dwprint ("start sanity blink");
  for (int i = 0; i < NUM_LEDS; ++i)
    leds[i] = CRGB::Red;

  FastLED.show();
  delay(1000);
  for (int i = 0; i < NUM_LEDS; ++i)
    leds[i] = CRGB::Black;

  FastLED.show();

  // exit() will move the state, reset it.
  prev(current_range)->exit ();
  dwprint ("Use state for init led");
  debugRangeElement (prev(current_range));
  control_state = STARTUP;
  dwprint ("end sanity blink");
  NEXT_STATE (IDLE);
}

void setSanityAlarm(uint8_t type, DateTime when)
{
  dwprint ("setAlarm\r\n - now: ");
  debugDateTime(DS3231M.now());
  dwprint (" - alarm for : ");
  debugDateTime(when);
  DS3231M.setAlarm(type, when);
  dwprint ("end setAlarm");
}

void setAlarm(uint8_t type, bool alarm_for_entering)
{
  dwprint ("setAlarm\r\n - now: ");
  debugDateTime(DS3231M.now());
  dwprint (" - alarm for : ");
  DateTime when = alarm_for_entering ? current_range->mstart: current_range->mend;

  current_range->next_alarm_is_entering = alarm_for_entering;
  debugDateTime(when);
  DS3231M.setAlarm(type, when);
  dwprint ("end setAlarm");
}

void setup (void)
{
  delay (1000); // initial delay of a few seconds is recommended
  Serial.begin (serial_speed);

  dwprint ("Serial setup");

  dwprint ("Init DS3231");
  // nice active wait that will never end in case of error.
  while (!DS3231M.begin ())
    {
      Serial.println ("Unable to find DS3231M. Checking again in 1 second.");
      delay (1000);
    }

#ifdef FAKE_DATE_STARTUP
  DateTime test (__DATE__, FAKE_DATE_STARTUP);
  DS3231M.adjust (test);
#endif

  dwprint ("Init LEDs");
  delay (1000);
  // initializes LED strip
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER> (leds, NUM_LEDS).setCorrection (TypicalLEDStrip);
  FastLED.setBrightness (BRIGHTNESS);

  for (int i = 0; i < NUM_LEDS; ++i)
    leds[i] = CRGB::Black;

  FastLED.show ();

  elements[0]->mnext = elements[0];
  for (unsigned int i=0; i < sizeof (elements)/sizeof (Range_element*)-1; i++)
    {
      elements[i+1]->mnext = elements[i]->mnext;
      elements[i]->mnext = elements[i+1];
    }
  elements[sizeof (elements)/sizeof (Range_element*)-1]->mnext = elements[0];

  // Find next range to be used
  DateTime n = DS3231M.now ();
  current_range = elements[0];

  for (Range_element *e : elements)
    {
     if (is_time_before_eq (e->mend, n))
       continue;
     current_range = e;
     break;
    }

  debugRangeElement (current_range);
  dwprint ("End setup");
}

static void control_automata (void) {

#if DEBUG == 2
  Serial.println ("Automata step...");
#endif

  switch (control_state){
  case STARTUP: // Should happen only once
    STATE (STARTUP);
    setSanityAlarm (secondsMinutesHoursMatch, DS3231M.now () + TimeSpan (2));
    // The hook is taking care of changing to next state
    NEXT_STATE (WAIT_ALARM);
    break;

  case WAIT_ALARM:
    STATE (WAIT_ALARM);
    // do nothing, startup_sanity_check will do WAIT_ALARM -> IDLE
    break;

  case IDLE:
    STATE (IDLE);

    if (current_range->is_in_range ())
      {
        current_range->enter ();
        NEXT_STATE (IN);
      }
    else
      {
        setAlarm (secondsMinutesHoursMatch, true);
        NEXT_STATE (WAIT_ENTER);
      }
    break;

  case WAIT_ENTER:
    STATE (WAIT_ENTER);
    // current_range->enter() will do WAIT_ENTER -> ENTERING
    break;

  case ENTERING:
    STATE (ENTERING);
    current_range->enter ();
    NEXT_STATE (IN);
    break;

  case IN:
    STATE (IN);
    current_range->in ();
    // current_range->exit() will do IN->EXITING
    break;

  case EXITING:
    STATE (EXITING);
    current_range->exit ();
    current_range = current_range->mnext;
    debugRangeElement (current_range);
    NEXT_STATE (IDLE);
    break;
  }
}

// main program
void loop () {

  if (DS3231M.isAlarm ())
    {
      dwprint ("Alarm, running hook");
      debugDateTime (DS3231M.now ());
      if (control_state == WAIT_ALARM)
        startup_sanity_check ();
      else
        current_range->alarm ();

      DS3231M.clearAlarm ();
  }

  control_automata ();
}
