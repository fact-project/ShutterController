#include "tools.h"
#include "DualVNH5019MotorShield.h"
#include "LinakHallSensor.h"
#include <stdio.h> // for function sprintf

typedef enum {
  S_CLOSED,
  S_DRIVE_CLOSING,
  S_FAIL_CLOSING,
  S_OPEN,
  S_DRIVE_OPENING,
  S_FAIL_OPENING,
  S_UNKNOWN,
} system_state_t;

struct motor_info_t {
    uint16_t current;
    uint16_t position;
};

#define ARCHIVE_LEN 300
motor_info_t archive[ARCHIVE_LEN];
uint16_t archive_pointer = 0;
uint16_t motor_pointer[2];

system_state_t system_state = S_UNKNOWN;
char current_cmd = 'x';

DualVNH5019MotorShield md;
LinakHallSensor lh;

const unsigned long DRIVE_TIME_LIMIT_MS = 15000UL;

bool move_fully_supervised(int motor, bool open) {
    unsigned long start_time = millis();
    md.ramp_to_speed_blocking(motor, open ? 255 : -255);
    while (true) {
        motor_info_t motor_info;
        motor_info.current = md.get_mean_std(motor, 300).mean;
        motor_info.position = lh.get_mean_std(motor, 300).mean;
        archive[archive_pointer++] = motor_info;
        motor_pointer[motor] = archive_pointer;

        if (md.is_overcurrent(motor)) return open ? false : true;
        if (md.is_zerocurrent(motor)) return true;
        if (millis() - start_time > DRIVE_TIME_LIMIT_MS) return false;
    }
}

bool close_lower() {
    return move_fully_supervised(0, false);
}

bool close_upper() {
    return move_fully_supervised(1, false);
}

bool open_lower() {
    return move_fully_supervised(0, true);
}

bool open_upper() {
    return move_fully_supervised(1, true);
}

void report_motor_info() {
    Serial.println("motor_pointers:");
    Serial.print(0); Serial.println(motor_pointer[0]);
    Serial.print(1); Serial.println(motor_pointer[1]);
    Serial.println("current and positions");
    for (uint16_t i=0; i<archive_pointer; i++) {
        Serial.print(archive[i].current);
        Serial.print(' ');
        Serial.print(archive[i].position);
        Serial.println();
    }
    archive_pointer = 0;
    motor_pointer[0] = 0;
    motor_pointer[1] = 0;
}

void drive_close() {
    if (close_lower() == false); system_state = S_FAIL_CLOSING;
    if (close_upper() == false); system_state = S_FAIL_CLOSING;
    system_state = S_CLOSED;
    report_motor_info();
}

void drive_open() {
    if (open_upper() == false); system_state = S_FAIL_OPENING;
    if (open_lower() == false); system_state = S_FAIL_OPENING;
    system_state = S_OPEN;
    report_motor_info();
}

void drive_stop() {
    md.setMotorSpeed(0, 0);
    md.setMotorSpeed(1, 0);
}

void achnowledge_header()
{
    Serial.print("Current state: ");
    switch (system_state){
        case S_OPEN: Serial.println("Open"); break;
        case S_DRIVE_OPENING: Serial.println("Opening"); break;
        case S_FAIL_OPENING: Serial.println("Fail during Opening"); break;
        case S_CLOSED: Serial.println("Closed"); break;
        case S_DRIVE_CLOSING: Serial.println("Closing"); break;
        case S_FAIL_CLOSING: Serial.println("Fail during Closing"); break;
        case S_UNKNOWN: Serial.println("Unknown"); break;
    }
    Serial.write("Received command:");
    Serial.println(current_cmd);
}

void acknowledge_no_operation()
{
    Serial.println("Ignoring command.");
    achnowledge_header();
}

void acknowledge_operation()
{
    Serial.println("Command Valid:");
    achnowledge_header();
}

void init_drive_close()
{
    system_state = S_DRIVE_CLOSING;
    acknowledge_operation();
    drive_close();  // blocking for ~20sec
}

void init_drive_open()
{
    system_state = S_DRIVE_OPENING;
    acknowledge_operation();
    drive_open();  // blocking for ~20sec
}

void init_fail_open(){
    drive_stop();
    system_state = S_FAIL_OPENING;
    acknowledge_operation();
}

void init_fail_close(){
    drive_stop();
    system_state = S_FAIL_CLOSING;
    acknowledge_operation();
}

void state_machine()
{
    // local copy for shorter lines
    char c = current_cmd;
    switch (system_state) {
        case S_OPEN:
            if (c == 'o') acknowledge_no_operation();
            if (c == 'c') init_drive_close();
            break;
        case S_CLOSED:
            if (c == 'o') init_drive_open();
            if (c == 'c') acknowledge_no_operation();
            break;
        case S_DRIVE_OPENING:
            if (c == 'o') acknowledge_no_operation();
            if (c == 'c') init_fail_open();
            break;
        case S_DRIVE_CLOSING:
            if (c == 'o') init_fail_close();
            if (c == 'c') acknowledge_no_operation();
            break;
        case S_FAIL_OPENING:
            if (c == 'o') acknowledge_no_operation();
            if (c == 'c') init_drive_close();
            break;
        case S_FAIL_CLOSING:
            if (c == 'o') init_drive_open();
            if (c == 'c') acknowledge_no_operation();
            break;
        case S_UNKNOWN:
            if (c == 'o') init_drive_open();
            if (c == 'c') init_drive_close();
            break;
    }
}

void fetch_new_command()
{
  if (Serial.available() > 0) {
    current_cmd = Serial.read();
  }
}

void setup()
{
  Serial.begin(115200);
  md.init();
  lh.init();
}

void loop()
{
    fetch_new_command();
    state_machine();
}

