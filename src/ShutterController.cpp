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

typedef enum {
    D_SUCCESS,
    D_USER_INTERRUPT,
    D_FAIL,
} drive_success_t;


struct motor_info_t {
    uint16_t current;
    uint16_t position;
};

#define ARCHIVE_LEN 300
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
    server.print('}');
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
    server.print('}');
}


void report_motor_info(int motor, unsigned long duration, motor_stop_reason_t reason) {
    server.print(F("{\"motor_id\":"));
    server.print(motor); server.print(',');
    server.print(F("\"duration[ms]\":"));
    server.print(duration / TIMER0_MESS_UP_FACTOR); server.print(',');
    server.print(F("\"motor_stop_reason\":"));
    print_motor_stop_reason(reason); server.print(',');
    server.print(F("\"current\":["));
    for (uint16_t i=0; i<archive_pointer-1; i++) {
        server.print(archive[i].current); server.print(',');
    }
    server.print(archive[archive_pointer-1].current);
    server.print("],");
    server.print(F("\"position\":["));
    for (uint16_t i=0; i<archive_pointer-1; i++) {
        server.print(archive[i].position); server.print(',');
    }
    server.print(archive[archive_pointer-1].position);
    server.println("]}");
    archive_pointer = 0;
}

void fetch_new_command()
{
    // in case for some reason the last command has not been
    // processed yet. We do not do anything.
    if (is_user_command_new) return;

    EthernetClient client = server.available();
    if (client) {
        if (client.available() > 0){
            user_command = client.read();
            is_user_command_new = true;
        }
    }
}

drive_success_t move_fully_supervised(int motor, bool open) {
    unsigned long start_time = millis();
    unsigned long duration = 0;
    drive_success_t success = D_FAIL;
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
            reason = M_USER_INTERUPT;
            success = D_USER_INTERRUPT;
            break;
        }

        if (md.is_overcurrent(motor)) {
            reason = M_OVERCURRENT;
            success = open ? D_FAIL : D_SUCCESS;
            break;
        }
        if (md.is_zerocurrent(motor)){
            reason = M_ZEROCURRENT;
            success = D_SUCCESS;
            break;
        }
        duration = millis() - start_time;
        if (duration > DRIVE_TIME_LIMIT_MS) {
            reason = M_TIMEOUT;
            success = D_FAIL;
            break;
        }
    }
    md.setMotorSpeed(motor, 0);
    report_motor_info(motor, duration, reason);
    return success;
}

drive_success_t close_lower() { return move_fully_supervised(0, false); }
drive_success_t close_upper() { return move_fully_supervised(1, false); }
drive_success_t open_lower() { return move_fully_supervised(0, true); }
drive_success_t open_upper() { return move_fully_supervised(1, true); }

void ack(char cmd, bool ok)
{
    server.print(F("{\"ok\":"));
    server.print(ok); server.print(',');
    server.print(F("{\"cmd[int]\":"));
    server.print(int(cmd)); server.print(',');
    server.print(F("\"state\":"));
    print_system_state();
    server.println('}');
}

void init_drive_close(char cmd)
{
    system_state = S_DRIVE_CLOSING;
    ack(cmd, true);

    switch (close_lower()){
        case D_FAIL:
            system_state = S_FAIL_CLOSE;
            return;
        case D_USER_INTERRUPT:
            system_state = S_UNKNOWN;
            return;
        case D_SUCCESS:
            system_state = S_DRIVE_CLOSING;
    }

    switch (close_upper()){
        case D_FAIL:
            system_state = S_FAIL_CLOSE;
            return;
        case D_USER_INTERRUPT:
            system_state = S_UNKNOWN;
            return;
        case D_SUCCESS:
            system_state = S_CLOSED;
    }

    ack(cmd, true);
}

void init_drive_open(char cmd)
{
    system_state = S_DRIVE_OPENING;
    ack(cmd, true);

    switch (open_upper()){
        case D_FAIL:
            system_state = S_FAIL_OPEN;
            return;
        case D_USER_INTERRUPT:
            system_state = S_UNKNOWN;
            return;
        case D_SUCCESS:
            system_state = S_DRIVE_OPENING;
    }

    switch (open_lower()){
        case D_FAIL:
            system_state = S_FAIL_OPEN;
            return;
        case D_USER_INTERRUPT:
            system_state = S_UNKNOWN;
            return;
        case D_SUCCESS:
            system_state = S_OPEN;
    }

    ack(cmd, true);
}

void state_machine()
{
    char c = user_command;
    is_user_command_new = false;
    // 0 or '\0' is the special case that means, no new command recieved
    // so here we immediately return, c.f. fetch_new_command()
    if (c == 0) return;
    if (c == 's') {
        ack(c, true);
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
        ack(c, false);
    }
}

void setup()
{
    Ethernet.begin(_mac, _ip);
    server.begin();
    md.init();
    lh.init();
}

unsigned long time_of_last_heart_beat = millis();

void loop()
{
    fetch_new_command();
    if (is_user_command_new) state_machine();

    if (millis() - time_of_last_heart_beat > 1000 * TIMER0_MESS_UP_FACTOR){
        time_of_last_heart_beat = millis();
        ack('H', true);
    }
}

