/* Explain what is going on with Timer0.
*/

#include "tools.h"
#include "DualVNH5019MotorShield.h"
#include "LinakHallSensor.h"
#include <stdio.h> // for function sprintf
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

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
    StaticJsonBuffer<100> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["state_number"] = int(system_state);
    switch (system_state){
        case S_OPEN: root["state_name"] = "Open"; break;
        case S_DRIVE_OPENING: root["state_name"] = "Opening"; break;
        case S_FAIL_OPEN: root["state_name"] = "Fail uring Opening"; break;
        case S_CLOSED: root["state_name"] = "Closed"; break;
        case S_DRIVE_CLOSING: root["state_name"] = "Closing"; break;
        case S_FAIL_CLOSE: root["state_name"] = "Fail uring Closing"; break;
        case S_UNKNOWN: root["state_name"] = "Unknown"; break;
        default:
            root["state_name"] = "Must never happen!";
    }
    root.printTo(server);
}

void print_motor_stop_reason(motor_stop_reason_t r){
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root.printTo(server);
}

void report_motor_info(int motor, unsigned long duration, motor_stop_reason_t reason) {
    StaticJsonBuffer<500> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["motor_id"] = motor;
    root["duration"] = duration / TIMER0_MESS_UP_FACTOR;
    root["motor_stop_reason_number"] = int(reason);
    switch (reason){
        case M_TIMEOUT: root["motor_stop_reason_name"] = "Timeout"; break;
        case M_OVERCURRENT: root["motor_stop_reason_name"] = "Overcurrent"; break;
        case M_ZEROCURRENT: root["motor_stop_reason_name"] = "Zerocurrent/Endswitch"; break;
        case M_POSITION_REACHED: root["motor_stop_reason_name"] = "Position Reached"; break;
        case M_NO_REASON: root["motor_stop_reason_name"] = "No Reason! bug!!!"; break;
        case M_USER_INTERUPT: root["motor_stop_reason_name"] = "User Interupt"; break;
        default:
            root["motor_stop_reason_name"] = "Must never happen!";
    }

    JsonArray& current = root.createNestedArray("current");
    JsonArray& position = root.createNestedArray("position");
    for (uint16_t i=0; i<min(20, archive_pointer); i++) {
        current.add(archive[i].current);
        position.add(archive[i].position);
    }

    archive_pointer = 0;
    Serial.print("root.measureLength:");
    Serial.println(root.measureLength());
    root.printTo(server);
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

void ack(char cmd, bool ok) {
    StaticJsonBuffer<100> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["ok"] = ok;
    root["message"] = ok ? "command valid" : "command ignored";
    root["last_command"] = cmd;
    root.printTo(server);
}

void acknowledge_no_operation(char cmd)
{
    ack(cmd, false);
    print_system_state();
}

void init_drive_close(char cmd)
{
    system_state = S_DRIVE_CLOSING;
    ack(cmd, true);
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
    ack(cmd, true);
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
    Serial.begin(115200);
    md.init();
    lh.init();
}

void loop()
{
    fetch_new_command();
    if (is_user_command_new) state_machine();
}

