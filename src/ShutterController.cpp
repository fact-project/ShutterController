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

char user_command = 0;
bool is_user_command_new = false;

const unsigned long DRIVE_TIME_LIMIT_MS = 15000UL * TIMER0_MESS_UP_FACTOR;

void print_system_state(){
    server.print(F("{\"state_name\":"));
    switch (system_state){
        case S_OPEN: server.print(F("\"Open\"")); break;
        case S_DRIVE_OPENING: server.print(F("\"Opening\"")); break;
        case S_FAIL_OPEN: server.print(F("\"Fail during Open\"")); break;
        case S_CLOSED: server.print(F("\"Closed\"")); break;
        case S_DRIVE_CLOSING: server.print(F("\"Closing\"")); break;
        case S_FAIL_CLOSE: server.print(F("\"Fail during Close\"")); break;
        case S_UNKNOWN: server.print(F("\"Unknown\"")); break;
        default:
            server.print(F("\"Must never happen!\""));
    }
    server.print(',');
    server.print(F("\"state_id\":"));
    server.print(int(system_state));
    server.println('}');
}

void print_motor_stop_reason(motor_stop_reason_t r){
    server.print(F("{\"motor_stop_name\":"));
    switch (r){
        case M_TIMEOUT: server.print(F("\"Timeout\"")); break;
        case M_OVERCURRENT: server.print(F("\"Overcurrent\"")); break;
        case M_ZEROCURRENT: server.print(F("\"Zerocurrent/Endswitch\"")); break;
        case M_POSITION_REACHED: server.print(F("\"Position Reached\"")); break;
        case M_NO_REASON: server.print(F("\"No Reason! bug!!!\"")); break;
        case M_USER_INTERUPT: server.print(F("\"User Interupt\"")); break;
        default:
            server.print(F("\"Must never happen!\""));
    }
    server.print(',');
    server.print(F("\"motor_stop_id\":"));
    server.print(int(r));
    server.println('}');
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

void fetch_new_command()
{
    EthernetClient client = server.available();
    if (client) {
        if (client.available() > 0){
            user_command = client.read();
            is_user_command_new = true;
        }
    }
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
        fetch_new_command();
        if (is_user_command_new){
            char p = user_command;
            if ((open && p != 'o') ||
                (!open && p != 'c'))
            {
                reason = M_USER_INTERUPT;
                success = false;
                break;
            }
            else{
                is_user_command_new = false;
            }

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

void state_machine()
{
    char c = user_command;
    is_user_command_new = false;
    // 0 or '\0' is the special case that means, no new command recieved
    // so here we immediately return, c.f. fetch_new_command()
    if (c == 0) return;
    if (c == 's') {
        print_system_state();
        return;
    }
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

void setup()
{
    Ethernet.begin(_mac, _ip);
    server.begin();
    md.init();
    lh.init();
}

void loop()
{
    fetch_new_command();
    if (is_user_command_new) state_machine();
}

