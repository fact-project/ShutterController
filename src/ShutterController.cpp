#include "tools.h"
#include "DualVNH5019MotorShield.h"
#include "LinakHallSensor.h"
#include <stdio.h> // for function sprintf
#include <SPI.h>
#include <Ethernet.h>

byte _mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x5C, 0x91 };
IPAddress _ip(10,0,100,36);
EthernetServer server(80);

typedef enum {
  S_CLOSED,
  S_DRIVE_CLOSING,
  S_FAIL_CLOSE,
  S_OPEN,
  S_DRIVE_OPENING,
  S_FAIL_OPEN,
  S_UNKNOWN,
} system_state_t;

typedef enum {
    M_NO_REASON,
    M_OVERCURRENT,
    M_ZEROCURRENT,
    M_TIMEOUT,
    M_POSITION_REACHED,
    M_USER_INTERUPT,
} motor_stop_reason_t;


struct motor_info_t {
    uint16_t current;
    uint16_t position;
};

#define ARCHIVE_LEN 100
#define TIMER0_MESS_UP_FACTOR 32UL
int num_samples = 700;
motor_info_t archive[ARCHIVE_LEN];
uint16_t archive_pointer = 0;

system_state_t system_state = S_UNKNOWN;
DualVNH5019MotorShield md;
LinakHallSensor lh;

int max_open_speed[2] = {255, 255};
int max_close_speed[2] = {-255, -255};

const unsigned long DRIVE_TIME_LIMIT_MS = 15000UL * TIMER0_MESS_UP_FACTOR;

void print_system_state(){
    server.print("Current state: ");
    switch (system_state){
        case S_OPEN: server.println("Open"); break;
        case S_DRIVE_OPENING: server.println("Opening"); break;
        case S_FAIL_OPEN: server.println("Fail during Opening"); break;
        case S_CLOSED: server.println("Closed"); break;
        case S_DRIVE_CLOSING: server.println("Closing"); break;
        case S_FAIL_CLOSE: server.println("Fail during Closing"); break;
        case S_UNKNOWN: server.println("Unknown"); break;
        default:
            server.println("Must never happen!");
    }
}

void print_motor_stop_reason(motor_stop_reason_t r){
    server.print("Motor Stop Reason: ");
    switch (r){
        case M_TIMEOUT: server.println("Timeout"); break;
        case M_OVERCURRENT: server.println("Overcurrent"); break;
        case M_ZEROCURRENT: server.println("Zerocurrent/Endswitch"); break;
        case M_POSITION_REACHED: server.println("Position Reached"); break;
        case M_NO_REASON: server.println("No Reason! bug!!!"); break;
        case M_USER_INTERUPT: server.println("User Interupt"); break;
        default:
            server.println("Must never happen!");
    }
}


void report_motor_info(int motor, unsigned long duration, motor_stop_reason_t reason) {

    server.print("info about motor: ");
    server.println(motor);
    server.print("duration[ms]: ");
    server.println(duration / TIMER0_MESS_UP_FACTOR);
    print_motor_stop_reason(reason);
    server.print("current and positions: #");
    server.println(archive_pointer);
    for (uint16_t i=0; i<archive_pointer; i++) {
        server.print(archive[i].current);
        server.print(' ');
        server.print(archive[i].position);
        server.println();
    }
    archive_pointer = 0;
}

bool move_fully_supervised(int motor, bool open) {
    unsigned long start_time = millis();
    unsigned long duration = 0;
    bool success = false;
    int speed = open ? max_open_speed[motor] : max_close_speed[motor];
    md.ramp_to_speed_blocking(motor, speed);
    motor_stop_reason_t reason = M_NO_REASON;
    while (true) {
        motor_info_t motor_info;
        motor_info.current = md.get_mean_std(motor, num_samples).mean;
        motor_info.position = lh.get_mean_std(motor, num_samples).mean;
        if (archive_pointer < ARCHIVE_LEN){
            archive[archive_pointer++] = motor_info;
        }

        if (md.is_overcurrent(motor)) {
            reason = M_OVERCURRENT;
            success = open ? false : true;
            break;
        }
        if (md.is_zerocurrent(motor)){
            reason = M_ZEROCURRENT;
            success = true;
            break;
        }
        duration = millis() - start_time;
        if (duration > DRIVE_TIME_LIMIT_MS) {
            reason = M_TIMEOUT;
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
    server.print("Ignoring command: ");
    server.println(cmd);
    print_system_state();
}

void init_drive_close(char cmd)
{
    system_state = S_DRIVE_CLOSING;
    server.print("Command Valid: ");
    server.println(cmd);
    print_system_state();
    if (close_lower() == false) {
        system_state = S_FAIL_CLOSE;
        return;
    }
    if (close_upper() == false) {
        system_state = S_FAIL_CLOSE;
        return;
    }
    system_state = S_CLOSED;
    print_system_state();
}

void init_drive_open(char cmd)
{
    system_state = S_DRIVE_OPENING;
    server.print("Command Valid: ");
    server.println(cmd);
    print_system_state();
    if (open_upper() == false){
        system_state = S_FAIL_OPEN;
        return;
    }
    if (open_lower() == false){
        system_state = S_FAIL_OPEN;
        return;
    }
    system_state = S_OPEN;
    print_system_state();
}

void state_machine(char c)
{
    // 0 or '\0' is the special case that means, no new command recieved
    // so here we immediately return, c.f. fetch_new_command()
    if (c == 0) return;

    if ((c == 'c') && (
        (system_state == S_OPEN) ||
        (system_state == S_FAIL_OPEN) ||
        (system_state == S_UNKNOWN)
        )) {
        init_drive_close(c);
    } else if ((c == 'o') && (
        (system_state == S_CLOSED) ||
        (system_state == S_FAIL_CLOSE) ||
        (system_state == S_UNKNOWN)
        )) {
        init_drive_open(c);
    } else {
        acknowledge_no_operation(c);
    }
}

char fetch_new_command()
{
    EthernetClient client = server.available();
    if (client) {
        if (client.available() > 0){
            return client.read();
        }
    }

    // I define 0 or '\0' is the special char, that means:
    // no new command recieved, c.f. state_mache()
    return 0;
}

void setup()
{
    Ethernet.begin(_mac, _ip);
    server.begin();
    md.init();
    lh.init();
}

void loop()
{
    state_machine(fetch_new_command());
}

