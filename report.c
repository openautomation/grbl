/*
  report.c - reporting and messaging methods
  Part of Grbl

  Copyright (c) 2012-2014 Sungeun K. Jeon  

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

/* 
  This file functions as the primary feedback interface for Grbl. Any outgoing data, such 
  as the protocol status messages, feedback messages, and status reports, are stored here.
  For the most part, these functions primarily are called from protocol.c methods. If a 
  different style feedback is desired (i.e. JSON), then a user can change these following 
  methods to accomodate their needs.
*/

#include "system.h"
#include "report.h"
#include "print.h"
#include "settings.h"
#include "gcode.h"
#include "coolant_control.h"
#include "planner.h"
#include "spindle_control.h"


// Handles the primary confirmation protocol response for streaming interfaces and human-feedback.
// For every incoming line, this method responds with an 'ok' for a successful command or an 
// 'error:'  to indicate some error event with the line or some critical system error during 
// operation. Errors events can originate from the g-code parser, settings module, or asynchronously
// from a critical error, such as a triggered hard limit. Interface should always monitor for these
// responses.
// NOTE: In silent mode, all error codes are greater than zero.
// TODO: Install silent mode to return only numeric values, primarily for GUIs.
void report_status_message(uint8_t status_code) 
{
  if (status_code == 0) { // STATUS_OK
    printPgmString(PSTR("ok\r\n"));
  } else {
    printPgmString(PSTR("error: "));
    switch(status_code) {          
      case STATUS_EXPECTED_COMMAND_LETTER:
      printPgmString(PSTR("Expected command letter")); break;
      case STATUS_BAD_NUMBER_FORMAT:
      printPgmString(PSTR("Bad number format")); break;
      case STATUS_INVALID_STATEMENT:
      printPgmString(PSTR("Invalid statement")); break;
      case STATUS_NEGATIVE_VALUE:
      printPgmString(PSTR("Value < 0")); break;
      case STATUS_SETTING_DISABLED:
      printPgmString(PSTR("Setting disabled")); break;
      case STATUS_SETTING_STEP_PULSE_MIN:
      printPgmString(PSTR("Value < 3 usec")); break;
      case STATUS_SETTING_READ_FAIL:
      printPgmString(PSTR("EEPROM read fail. Using defaults")); break;
      case STATUS_IDLE_ERROR:
      printPgmString(PSTR("Not idle")); break;
      case STATUS_ALARM_LOCK:
      printPgmString(PSTR("Alarm lock")); break;
      case STATUS_SOFT_LIMIT_ERROR:
      printPgmString(PSTR("Homing not enabled")); break;
      case STATUS_OVERFLOW:
      printPgmString(PSTR("Line overflow")); break; 
      
      // Common g-code parser errors.
      case STATUS_GCODE_MODAL_GROUP_VIOLATION:
      printPgmString(PSTR("Modal group violation")); break;
      case STATUS_GCODE_UNSUPPORTED_COMMAND:
      printPgmString(PSTR("Unsupported command")); break;
      case STATUS_GCODE_UNDEFINED_FEED_RATE:
      printPgmString(PSTR("Undefined feed rate")); break;
      default:
        // Remaining g-code parser errors with error codes
        printPgmString(PSTR("Invalid gcode ID:"));
        print_unsigned_base10(status_code); // Print error code for user reference
    }
    printPgmString(PSTR("\r\n"));
  }
}

// Prints alarm messages.
void report_alarm_message(int8_t alarm_code)
{
  printPgmString(PSTR("ALARM: "));
  switch (alarm_code) {
    case ALARM_LIMIT_ERROR: 
    printPgmString(PSTR("Hard/soft limit")); break;
    case ALARM_ABORT_CYCLE: 
    printPgmString(PSTR("Abort during cycle")); break;
    case ALARM_PROBE_FAIL:
    printPgmString(PSTR("Probe fail")); break;
  }
  printPgmString(PSTR("\r\n"));
  delay_ms(500); // Force delay to ensure message clears serial write buffer.
}

// Prints feedback messages. This serves as a centralized method to provide additional
// user feedback for things that are not of the status/alarm message protocol. These are
// messages such as setup warnings, switch toggling, and how to exit alarms.
// NOTE: For interfaces, messages are always placed within brackets. And if silent mode
// is installed, the message number codes are less than zero.
// TODO: Install silence feedback messages option in settings
void report_feedback_message(uint8_t message_code)
{
  printPgmString(PSTR("["));
  switch(message_code) {
    case MESSAGE_CRITICAL_EVENT:
    printPgmString(PSTR("Reset to continue")); break;
    case MESSAGE_ALARM_LOCK:
    printPgmString(PSTR("'$H'|'$X' to unlock")); break;
    case MESSAGE_ALARM_UNLOCK:
    printPgmString(PSTR("Caution: Unlocked")); break;
    case MESSAGE_ENABLED:
    printPgmString(PSTR("Enabled")); break;
    case MESSAGE_DISABLED:
    printPgmString(PSTR("Disabled")); break; 
  }
  printPgmString(PSTR("]\r\n"));
}


// Welcome message
void report_init_message()
{
  printPgmString(PSTR("\r\nGrbl " GRBL_VERSION " ['$' for help]\r\n"));
}

// Grbl help message
void report_grbl_help() {
  printPgmString(PSTR("$$ (view Grbl settings)\r\n"
                      "$# (view # parameters)\r\n"
                      "$G (view parser state)\r\n"
                      "$N (view startup blocks)\r\n"
                      "$x=value (save Grbl setting)\r\n"
                      "$Nx=line (save startup block)\r\n"
                      "$C (check gcode mode)\r\n"
                      "$X (kill alarm lock)\r\n"
                      "$H (run homing cycle)\r\n"
                      "~ (cycle start)\r\n"
                      "! (feed hold)\r\n"
                      "? (current status)\r\n"
                      "ctrl-x (reset Grbl)\r\n"));
}

// Grbl global settings print out.
// NOTE: The numbering scheme here must correlate to storing in settings.c
void report_grbl_settings() {
  printPgmString(PSTR("$0=")); printFloat(settings.steps_per_mm[X_AXIS]);
  printPgmString(PSTR(" (x, step/mm)\r\n$1=")); printFloat(settings.steps_per_mm[Y_AXIS]);
  printPgmString(PSTR(" (y, step/mm)\r\n$2=")); printFloat(settings.steps_per_mm[Z_AXIS]);
  printPgmString(PSTR(" (z, step/mm)\r\n$3=")); printFloat(settings.max_rate[X_AXIS]);
  printPgmString(PSTR(" (x max rate, mm/min)\r\n$4=")); printFloat(settings.max_rate[Y_AXIS]);
  printPgmString(PSTR(" (y max rate, mm/min)\r\n$5=")); printFloat(settings.max_rate[Z_AXIS]);
  printPgmString(PSTR(" (z max rate, mm/min)\r\n$6=")); printFloat(settings.acceleration[X_AXIS]/(60*60)); // Convert from mm/min^2 for human readability
  printPgmString(PSTR(" (x accel, mm/sec^2)\r\n$7=")); printFloat(settings.acceleration[Y_AXIS]/(60*60)); // Convert from mm/min^2 for human readability
  printPgmString(PSTR(" (y accel, mm/sec^2)\r\n$8=")); printFloat(settings.acceleration[Z_AXIS]/(60*60)); // Convert from mm/min^2 for human readability
  printPgmString(PSTR(" (z accel, mm/sec^2)\r\n$9=")); printFloat(-settings.max_travel[X_AXIS]); // Grbl internally store this as negative.
  printPgmString(PSTR(" (x max travel, mm)\r\n$10=")); printFloat(-settings.max_travel[Y_AXIS]); // Grbl internally store this as negative.
  printPgmString(PSTR(" (y max travel, mm)\r\n$11=")); printFloat(-settings.max_travel[Z_AXIS]); // Grbl internally store this as negative.
  printPgmString(PSTR(" (z max travel, mm)\r\n$12=")); print_unsigned_base10(settings.pulse_microseconds);
  printPgmString(PSTR(" (step pulse, usec)\r\n$13=")); print_unsigned_base10(settings.step_invert_mask);
  printPgmString(PSTR(" (step port invert mask:")); print_base2(settings.step_invert_mask);
  printPgmString(PSTR(")\r\n$14=")); print_unsigned_base10(settings.dir_invert_mask);
  printPgmString(PSTR(" (dir port invert mask:")); print_base2(settings.dir_invert_mask);
  printPgmString(PSTR(")\r\n$15=")); print_unsigned_base10(settings.stepper_idle_lock_time);
  printPgmString(PSTR(" (step idle delay, msec)\r\n$16=")); printFloat(settings.junction_deviation);
  printPgmString(PSTR(" (junction deviation, mm)\r\n$17=")); printFloat(settings.arc_tolerance);
  printPgmString(PSTR(" (arc tolerance, mm)\r\n$18=")); print_unsigned_base10(settings.decimal_places);
  printPgmString(PSTR(" (n-decimals, int)\r\n$19=")); print_unsigned_base10(bit_istrue(settings.flags,BITFLAG_REPORT_INCHES));
  printPgmString(PSTR(" (report inches, bool)\r\n$20=")); print_unsigned_base10(bit_istrue(settings.flags,BITFLAG_AUTO_START));
  printPgmString(PSTR(" (auto start, bool)\r\n$21=")); print_unsigned_base10(bit_istrue(settings.flags,BITFLAG_INVERT_ST_ENABLE));
  printPgmString(PSTR(" (invert step enable, bool)\r\n$22=")); print_unsigned_base10(bit_istrue(settings.flags,BITFLAG_INVERT_LIMIT_PINS));
  printPgmString(PSTR(" (invert limit pins, bool)\r\n$23=")); print_unsigned_base10(bit_istrue(settings.flags,BITFLAG_SOFT_LIMIT_ENABLE));
  printPgmString(PSTR(" (soft limits, bool)\r\n$24=")); print_unsigned_base10(bit_istrue(settings.flags,BITFLAG_HARD_LIMIT_ENABLE));
  printPgmString(PSTR(" (hard limits, bool)\r\n$25=")); print_unsigned_base10(bit_istrue(settings.flags,BITFLAG_HOMING_ENABLE));
  printPgmString(PSTR(" (homing cycle, bool)\r\n$26=")); print_unsigned_base10(settings.homing_dir_mask);
  printPgmString(PSTR(" (homing dir invert mask:")); print_base2(settings.homing_dir_mask);
  printPgmString(PSTR(")\r\n$27=")); printFloat(settings.homing_feed_rate);
  printPgmString(PSTR(" (homing feed, mm/min)\r\n$28=")); printFloat(settings.homing_seek_rate);
  printPgmString(PSTR(" (homing seek, mm/min)\r\n$29=")); print_unsigned_base10(settings.homing_debounce_delay);
  printPgmString(PSTR(" (homing debounce, msec)\r\n$30=")); printFloat(settings.homing_pulloff);
  printPgmString(PSTR(" (homing pull-off, mm)\r\n")); 
}


// Prints current probe parameters. Upon a probe command, these parameters are updated upon a
// successful probe or upon a failed probe with the G38.3 without errors command (if supported). 
// These values are retained until Grbl is power-cycled, whereby they will be re-zeroed.
void report_probe_parameters()
{
  uint8_t i;
  float print_position[N_AXIS];
 
  // Report in terms of machine position.
  printPgmString(PSTR("[Probe:")); 
  for (i=0; i< N_AXIS; i++) {
    print_position[i] = sys.probe_position[i]/settings.steps_per_mm[i];
    if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) { print_position[i] *= INCH_PER_MM; }
    printFloat(print_position[i]);
    if (i < (N_AXIS-1)) { printPgmString(PSTR(",")); }
  }  
  printPgmString(PSTR("]\r\n"));
}


// Prints Grbl NGC parameters (coordinate offsets, probing)
void report_ngc_parameters()
{
  float coord_data[N_AXIS];
  uint8_t coord_select, i;
  for (coord_select = 0; coord_select <= SETTING_INDEX_NCOORD; coord_select++) { 
    if (!(settings_read_coord_data(coord_select,coord_data))) { 
      report_status_message(STATUS_SETTING_READ_FAIL); 
      return;
    } 
    printPgmString(PSTR("[G"));
    switch (coord_select) {
      case 6: printPgmString(PSTR("28")); break;
      case 7: printPgmString(PSTR("30")); break;
      default: print_unsigned_base10(coord_select+54); break; // G54-G59
    }  
    printPgmString(PSTR(":"));         
    for (i=0; i<N_AXIS; i++) {
      if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) { printFloat(coord_data[i]*INCH_PER_MM); }
      else { printFloat(coord_data[i]); }
      if (i < (N_AXIS-1)) { printPgmString(PSTR(",")); }
      else { printPgmString(PSTR("]\r\n")); }
    } 
  }
  printPgmString(PSTR("[G92:")); // Print G92,G92.1 which are not persistent in memory
  for (i=0; i<N_AXIS; i++) {
    if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) { printFloat(gc_state.coord_offset[i]*INCH_PER_MM); }
    else { printFloat(gc_state.coord_offset[i]); }
    if (i < (N_AXIS-1)) { printPgmString(PSTR(",")); }
    else { printPgmString(PSTR("]\r\n")); }
  } 
  report_probe_parameters(); // Print probe parameters. Not persistent in memory.
}


// Print current gcode parser mode state
void report_gcode_modes()
{
  switch (gc_state.modal.motion) {
    case MOTION_MODE_SEEK : printPgmString(PSTR("[G0")); break;
    case MOTION_MODE_LINEAR : printPgmString(PSTR("[G1")); break;
    case MOTION_MODE_CW_ARC : printPgmString(PSTR("[G2")); break;
    case MOTION_MODE_CCW_ARC : printPgmString(PSTR("[G3")); break;
    case MOTION_MODE_NONE : printPgmString(PSTR("[G80")); break;
  }

  printPgmString(PSTR(" G"));
  print_unsigned_base10(gc_state.modal.coord_select+54);
  
  switch (gc_state.modal.plane_select) {
    case PLANE_SELECT_XY : printPgmString(PSTR(" G17")); break;
    case PLANE_SELECT_ZX : printPgmString(PSTR(" G18")); break;
    case PLANE_SELECT_YZ : printPgmString(PSTR(" G19")); break;
  }
  
  if (gc_state.modal.units == UNITS_MODE_MM) { printPgmString(PSTR(" G21")); }
  else { printPgmString(PSTR(" G20")); }
  
  if (gc_state.modal.distance == DISTANCE_MODE_ABSOLUTE) { printPgmString(PSTR(" G90")); }
  else { printPgmString(PSTR(" G91")); }
  
  if (gc_state.modal.feed_rate == FEED_RATE_MODE_INVERSE_TIME) { printPgmString(PSTR(" G93")); }
  else { printPgmString(PSTR(" G94")); }
    
  switch (gc_state.modal.program_flow) {
    case PROGRAM_FLOW_RUNNING : printPgmString(PSTR(" M0")); break;
    case PROGRAM_FLOW_PAUSED : printPgmString(PSTR(" M1")); break;
    case PROGRAM_FLOW_COMPLETED : printPgmString(PSTR(" M2")); break;
  }

  switch (gc_state.modal.spindle) {
    case SPINDLE_ENABLE_CW : printPgmString(PSTR(" M3")); break;
    case SPINDLE_ENABLE_CCW : printPgmString(PSTR(" M4")); break;
    case SPINDLE_DISABLE : printPgmString(PSTR(" M5")); break;
  }
  
  switch (gc_state.modal.coolant) {
    case COOLANT_DISABLE : printPgmString(PSTR(" M9")); break;
    case COOLANT_FLOOD_ENABLE : printPgmString(PSTR(" M8")); break;
    #ifdef ENABLE_M7
      case COOLANT_MIST_ENABLE : printPgmString(PSTR(" M7")); break;
    #endif
  }
  
  printPgmString(PSTR(" T"));
  print_unsigned_base10(gc_state.tool);
  
  printPgmString(PSTR(" F"));
  printFloat(gc_state.feed_rate);

  printPgmString(PSTR("]\r\n"));
}

// Prints specified startup line
void report_startup_line(uint8_t n, char *line)
{
  printPgmString(PSTR("$N")); print_unsigned_base10(n);
  printPgmString(PSTR("=")); printString(line);
  printPgmString(PSTR("\r\n"));
}


// Prints build info line
void report_build_info(char *line)
{
  printPgmString(PSTR("[" GRBL_VERSION "." GRBL_VERSION_BUILD ":"));
  printString(line);
  printPgmString(PSTR("]\r\n"));
}


 // Prints real-time data. This function grabs a real-time snapshot of the stepper subprogram 
 // and the actual location of the CNC machine. Users may change the following function to their
 // specific needs, but the desired real-time data report must be as short as possible. This is
 // requires as it minimizes the computational overhead and allows grbl to keep running smoothly, 
 // especially during g-code programs with fast, short line segments and high frequency reports (5-20Hz).
void report_realtime_status()
{
  // **Under construction** Bare-bones status report. Provides real-time machine position relative to 
  // the system power on location (0,0,0) and work coordinate position (G54 and G92 applied). Eventually
  // to be added are distance to go on block, processed block id, and feed rate. Also a settings bitmask
  // for a user to select the desired real-time data.
  uint8_t i;
  int32_t current_position[N_AXIS]; // Copy current state of the system position variable
  memcpy(current_position,sys.position,sizeof(sys.position));
  float print_position[N_AXIS];
 
  // Report current machine state
  switch (sys.state) {
    case STATE_IDLE: printPgmString(PSTR("<Idle")); break;
    case STATE_QUEUED: printPgmString(PSTR("<Queue")); break;
    case STATE_CYCLE: printPgmString(PSTR("<Run")); break;
    case STATE_HOLD: printPgmString(PSTR("<Hold")); break;
    case STATE_HOMING: printPgmString(PSTR("<Home")); break;
    case STATE_ALARM: printPgmString(PSTR("<Alarm")); break;
    case STATE_CHECK_MODE: printPgmString(PSTR("<Check")); break;
  }
 
  // Report machine position
  printPgmString(PSTR(",MPos:")); 
  for (i=0; i< N_AXIS; i++) {
    print_position[i] = current_position[i]/settings.steps_per_mm[i];
    if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) { print_position[i] *= INCH_PER_MM; }
    printFloat(print_position[i]);
    printPgmString(PSTR(","));
  }
  
  // Report work position
  printPgmString(PSTR("WPos:")); 
  for (i=0; i< N_AXIS; i++) {
    if (bit_istrue(settings.flags,BITFLAG_REPORT_INCHES)) {
      print_position[i] -= (gc_state.coord_system[i]+gc_state.coord_offset[i])*INCH_PER_MM;
    } else {
      print_position[i] -= gc_state.coord_system[i]+gc_state.coord_offset[i];
    }
    printFloat(print_position[i]);
    if (i < (N_AXIS-1)) { printPgmString(PSTR(",")); }
  }
    
  #ifdef USE_LINE_NUMBERS
  // Report current line number
  printPgmString(PSTR(",Ln:")); 
  int32_t ln=0;
  plan_block_t * pb = plan_get_current_block();
  if(pb != NULL) {
    ln = pb->line_number;
  } 
  printInteger(ln);
  #endif
    
  printPgmString(PSTR(">\r\n"));
}
