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
int num_samples = 300;
motor_info_t archive[ARCHIVE_LEN];
uint16_t archive_pointer = 0;

system_state_t system_state = S_UNKNOWN;
DualVNH5019MotorShield md;
LinakHallSensor lh;

int max_open_speed[2] = {255, 255};
int max_close_speed[2] = {-255, -255};

const unsigned long DRIVE_TIME_LIMIT_MS = 15000UL * 64UL;

void print_system_state(){
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
}
void report_motor_info(int motor, unsigned long duration, char reason) {

    Serial.print("info about motor: ");
    Serial.println(motor);
    Serial.print("duration: ");
    Serial.println(duration);
    Serial.print("reason: ");
    Serial.println(reason);
    Serial.print("current and positions: #");
    Serial.println(archive_pointer);
    for (uint16_t i=0; i<archive_pointer; i++) {
        Serial.print(archive[i].current);
        Serial.print(' ');
        Serial.print(archive[i].position);
        Serial.println();
    }
    archive_pointer = 0;
    print_system_state();
}

bool move_fully_supervised(int motor, bool open) {
    unsigned long start_time = millis();
    unsigned long duration = 0;
    bool success = false;
    int speed = open ? max_open_speed[motor] : max_close_speed[motor];
    md.ramp_to_speed_blocking(motor, speed);
    char reason = ' ';
    while (true) {
        motor_info_t motor_info;
        motor_info.current = md.get_mean_std(motor, num_samples).mean;
        motor_info.position = lh.get_mean_std(motor, num_samples).mean;
        if (archive_pointer < ARCHIVE_LEN){
            archive[archive_pointer++] = motor_info;
        }

        if (md.is_overcurrent(motor)) {
            reason = 'O'; // O for Overcurrent
            success = open ? false : true;
            break;
        }
        if (md.is_zerocurrent(motor)){
            reason = 'Z'; // Z for ZeroCurrent
            success = true;
            break;
        }
        duration = millis() - start_time;
        if (duration > DRIVE_TIME_LIMIT_MS) {
            reason = 'T'; // T for Timeout
            success = false;
            break;
        }
    }
    md.setMotorSpeed(motor, 0);
    report_motor_info(motor, duration, reason);
    return success;
}

bool close_lower() { return move_fully_supervised(0, false); }
bool close_upper() { return move_fully_supervised(1, false); }
bool open_lower() { return move_fully_supervised(0, true); }
bool open_upper() { return move_fully_supervised(1, true); }

void acknowledge_no_operation(char cmd)
{
    Serial.print("Ignoring command: ");
    Serial.println(cmd);
    print_system_state();
}

void init_drive_close(char cmd)
{
    system_state = S_DRIVE_CLOSING;
    Serial.print("Command Valid: ");
    Serial.println(cmd);
    print_system_state();
    if (close_lower() == false) {
        system_state = S_FAIL_CLOSING;
        return;
    }
    if (close_upper() == false) {
        system_state = S_FAIL_CLOSING;
        return;
    }
    system_state = S_CLOSED;
}

void init_drive_open(char cmd)
{
    system_state = S_DRIVE_OPENING;
    Serial.print("Command Valid: ");
    Serial.println(cmd);
    print_system_state();
    if (open_upper() == false){
        system_state = S_FAIL_OPENING;
        return;
    }
    if (open_lower() == false){
        system_state = S_FAIL_OPENING;
        return;
    }
    system_state = S_OPEN;
}

void state_machine(char c)
{
    // 0 or '\0' is the special case that means, no new command recieved
    // so here we immediately return, c.f. fetch_new_command()
    if (c == 0) return;

    if ((c == 'c') && (
        (system_state == S_OPEN) ||
        (system_state == S_FAIL_OPENING) ||
        (system_state == S_UNKNOWN) ||
        (system_state == S_DRIVE_OPENING)
        )) {
        init_drive_close(c);
    } else if ((c == 'o') && (
        (system_state == S_CLOSED) ||
        (system_state == S_FAIL_CLOSING) ||
        (system_state == S_UNKNOWN) ||
        (system_state == S_DRIVE_CLOSING)
        )) {
        init_drive_open(c);
    } else {
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

