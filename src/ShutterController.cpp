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
DualVNH5019MotorShield md;
LinakHallSensor lh;

const unsigned long DRIVE_TIME_LIMIT_MS = 150000UL;

bool move_fully_supervised(int motor, bool open) {
    unsigned long start_time = millis();
    md.ramp_to_speed_blocking(motor, open ? 255 : -255);
    bool success = false;
    while (true) {
        motor_info_t motor_info;
        motor_info.current = md.get_mean_std(motor, 300).mean;
        motor_info.position = lh.get_mean_std(motor, 300).mean;
        if (archive_pointer < ARCHIVE_LEN){
            archive[archive_pointer++] = motor_info;
            motor_pointer[motor] = archive_pointer;
        }

        if (md.is_overcurrent(motor)) {
            Serial.println("is_overcurrent");
            success = open ? false : true;
            break;
        }
        if (md.is_zerocurrent(motor)){
            Serial.println("is_zerocurrent");
            success = true;
            break;
        }
        unsigned long duration = millis() - start_time;
        if (duration > DRIVE_TIME_LIMIT_MS) {
            Serial.print("timeout: ");
            Serial.println(duration);
            success = false;
            break;
        }
    }
    md.setMotorSpeed(motor, 0);
    return success;
}

bool close_lower() { return move_fully_supervised(0, false); }
bool close_upper() { return move_fully_supervised(1, false); }
bool open_lower() { return move_fully_supervised(0, true); }
bool open_upper() { return move_fully_supervised(1, true); }

void report_motor_info() {
    Serial.println("motor_pointers:");
    Serial.print(0); Serial.print(' '); Serial.println(motor_pointer[0]);
    Serial.print(1); Serial.print(' '); Serial.println(motor_pointer[1]);
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
    if (close_lower() == false) {
        system_state = S_FAIL_CLOSING;
        report_motor_info();
        return;
    }
    if (close_upper() == false) {
        system_state = S_FAIL_CLOSING;
        report_motor_info();
        return;
    }
    system_state = S_CLOSED;
    report_motor_info();
}

void drive_open() {
    if (open_upper() == false){
        system_state = S_FAIL_OPENING;
        report_motor_info();
        return;
    }
    if (open_lower() == false){
        system_state = S_FAIL_OPENING;
        report_motor_info();
        return;
    }
    system_state = S_OPEN;
    report_motor_info();
}

void drive_stop() {
    md.setMotorSpeed(0, 0);
    md.setMotorSpeed(1, 0);
}

void achnowledge_header(char cmd)
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
    Serial.println(cmd);
}

void acknowledge_no_operation(char cmd)
{
    Serial.println("Ignoring command.");
    achnowledge_header(cmd);
}

void acknowledge_operation(char cmd)
{
    Serial.println("Command Valid:");
    achnowledge_header(cmd);
}

void init_drive_close(char cmd)
{
    system_state = S_DRIVE_CLOSING;
    acknowledge_operation(cmd);
    drive_close();
}

void init_drive_open(char cmd)
{
    system_state = S_DRIVE_OPENING;
    acknowledge_operation(cmd);
    drive_open();
}

void state_machine(char c)
{
    // 0 or '\0' is the special case that means, no new command recieved
    // so here we immediately return, c.f. fetch_new_command()
    if (c == 0) return;

    switch (system_state) {
        case S_OPEN:
        case S_FAIL_OPENING:
        case S_UNKNOWN:
        case S_DRIVE_OPENING:
            if (c == 'c') init_drive_close(c);
            break;
        case S_CLOSED:
        case S_FAIL_CLOSING:
        case S_UNKNOWN:
        case S_DRIVE_CLOSING:
            if (c == 'o') init_drive_open(c);
            break;
        default:
            acknowledge_no_operation(c);
    }
}

char fetch_new_command()
{
    if (Serial.available() > 0) {
        return Serial.read();
    }
    else
    {
        // I define 0 or '\0' is the special char, that means:
        // no new command recieved, c.f. state_mache()
        return 0;
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
    state_machine(fetch_new_command());
}

