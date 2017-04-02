# The FACT Lid

The FACT camera front window is mounting platform and protection layer for the 1440 photon sensors.
For optimal optical coupling and reduced mechanical complexity the SiPM are directly coupled via their
injection molded light guides (winston cones) using dedicated UV hardening glue.
The window as well as the winston cones consists of PMMA, which is subject to aging in case of
prolonged illumination by [strong sunlight][Torikai].
In addition to adverse effects of direct sunlight wind speeds of up to 200 km/h
have been measured at the telescope site.
Such wind combined with sand grains can cause a significant reduction in the
optical transmission of a PPMA surface.

Therfore it is necessary to cover the FACT camera window with a protecting Lid.
The FACT Lid is depicted below.

![lid_open](docs/lid_open.png) ![lid_closed](docs/lid_closed.png)

It consists of two semicircular aluminum sheets movable by two LINAK LA23 actuators.
The actuators are powered with 24V and controlled
using an [Arduino Ethernet](https://www.arduino.cc/en/Main/ArduinoBoardEthernet)
with a (slightly modified)
[Pololu Dual Motor Driver](https://www.pololu.com/product/2502) shield.

Each LA23 actuator is equipped with two end switches and a hall sensor based position readout.
The position is encoded as a DC voltage level between 0 and 5V subsequently
transmitted via ~50m cable to the lid controller situated inside the
FACT counting hut, parallel to the supply lines.
Over the long line parallel to the power supply lines a lot of noise and
crosstalk is collected on the position encoding lines.
In order to mitigate this effect the designer of the
electronics Vittorio Boccone designed a dedicated filter and
amplifier, which is also provided as an arduino shield.

## firmware/software/hardware problems leading to this work

Within the sub sections below I point out, why this rework of the firmware/software
stack was deemed necessary in spring 2017.

### firmware: No connection during movement

The lid-controller firmware implemented a web-server. Any `GET` request on
port 80 would generate a valid HTML page presenting the current status of
both lid actuators. Possible states were:

 * UNKNOWN
 * CLOSED
 * OPEN
 * STEADY
 * MOVING
 * CLOSING
 * OPENING
 * JAMMED
 * MOTOR_FAULT
 * POWER_PROBLEM
 * OVER_CURRENT

Also the position and current consumption of each actuator were presented.
A correctly formed `POST` request would signal the controller to start opening
or closing the lid. However, during movement of the motors no network request
was handled. Resulting in no connection to the lid controller. Neither was it
thus possible to read any status information about either actuator, nor could
the movement be interrupted from the software side in case
and operator observed a problem.

The implementation of this controller firmware needed 66% of the MCU flash and
74% of the entire SRAM, which does not allow for a lot of additional features
or variations without touching original code in order to save on memory.

### Software: Condensed Lid states

The telescope FACT is controlled by a collection of programs utilizing DIM for
inter process communication. In order to weave arduino controlled hardware into
the FACT ecosystem software bridges are needed as no open implementation of the DIM
stack for micro controllers exists.

The software bridges (implemented in the C++ framework FACT++) can provide more
features than the firmware, so often the bridges to not only serve as transparent
connections between hardware and the DIM network.

In this case each lid actuator was able to assume one of 11 states, resulting in
121 different state combinations. Some more states need to be added to represent
the connection state to the hardware.

However the software bridge condensed this large number of states into this
group of 11 states to be presented to the telescope operator:

 * NoConnection: "No connection to web-server could be established recently"
 * Connected: "Connection established, but status still not known"
 * Unidentified: "At least one lid reported a state which could not be identified by lidctrl"
 * Inconsistent: "Both lids show different states"
 * Unknown: "Arduino reports at least one lids in an unknown status"
 * PowerProblem: "Arduino reports both lids to have a power problem (might also be that both are at the end switches)"
 * Overcurrent: "Arduino reports both lids to have a over current (might also be that both are at the end switches)"
 * Closed: "Both lids are closed"
 * Open: "Both lids are open"
 * Moving: "Lids are supposed to move, waiting for next status"
 * Locked: "Locked, no commands accepted except UNLOCK."

For default operation only `Closed`, `Moving` and `Open` are needed,
but in case of hardware problems telescope operators often saw
`Inconsistent`,  `PowerProblem` or `Overcurrent`
which could all have multiple reasons and thus remote error diagnosis was hard.


### Hardware: Failing Hall Sensors

Between the original installation of the Lid actuators in summer
2012 and the time of this writing spring 2017 the actuators
had to be replaced several times.
One Actuator has been studied and has been found to
be completely oxidized due to moisture.

In summer 2016 the hall sensors of the last pair of actuators failed,
resulting additional downtime of the instrument. Studies have shown that
a precise position measurement is not necessary for the standard operation
of the lid. The actuators are either completely contracted until the
end switches trigger (open state) or are completely extended until either
and end switch is hit or the lids are pressed firmly against the camera frame
resulting in a wanted `Overcurrent` condition (closed state).

The pre 2017 firmware did not allow for this kind of operation as
position measurement was an integral part of the decision making.

### Hardware: Failing network interface.

In winter 2016/2017 the lid controllers network interface became less and less
reliable. When it failed entirely remote FACT operation had to be canceled.
However due to the still functional USB connection between the Arduino lid-controller
and one of the FACT daq PCs which was intended for remote firmware upgrades
only, it was possible to reestablish a rudimentary remote operation within hours
and after a few days standard remote operation was possible again using a modified
firmware/software stack. So not a single night of operation was lost despite
the total fail of an integral communication system.

This event triggered the major overhaul of the firmware/software stack controlling
the lid actuators.

----

## Aspects of novel firmware/software stack

As a result of the shortcomings of the former stack the following features were
central to the design process of the novel stack (in order of importance):

 * operation without position measurement
 * communication during actuator movement
 * transparent software bridge
 * retain a degree of hardware error detection
 * simple USB fall back

### Operation without position measurement

While the LA23 acutators contain hall sensors for position measurement,


![FSM](docs/ArduinoFSM_with_interrupts_to_unknown.png)


## Communication between lid controller and software

Care has been taken to design the communication interface
between the lid controller on the arduino board
and the software implemented within the FACT++ framework both
human readable and easy to parse in code.

This way firmware development and debugging without dedicated software just
using [netcat](https://man.cx/netcat) was possible.
This feature is shared between the previous and the novel firmware version.
Since the previous version had a complete HTTP interface, any web browser
enough to interact with the previous lid controller.

### Input: Commands for the lid controller

The protocol is "single binary". Any byte sent to the lid controller is parsed
as a command. These commands are currently defined:

 * `'o'` : **O**pen lid
 * `'c'` : **C**lose lid
 * `'s'` : reply current **S**tate

### Output: Replies send from the lid controller to all connected clients.

The protocol is [JSON](http://json.org/) or rather
[JSONlines](http://jsonlines.org/) to allow for a simple implementation
of the receiver side. As the messages do not have fixed length, the receiver
needs some kind of marker to find complete replies in the outgoing byte stream.
Any number of bytes between two `'\n'` is a valid JSON object and can be parsed
as such.
No assumptions should be made by the receiver that the bytes before the first `'\n'`
also comprise a valid JSON object. Since replies are sent to all connected
clients, a fraction of an object might be received after connecting first to the lid controller.

There are two different types of replies:

 * Acknowledgment of a received command
 * Actuator Movement Report

#### Acknowledgment

As a reply to each command sent towards the lid controller and *Acknowledgement*
is replied immediately. See an example below. This is the reply to a status
request (`s`) immediately after booting the lid controller.

This is a raw representation of the incoming byte stream also showing the leading and the
ending EOL marker (`\n`). For convenience below a more reader friendly representation will be used:

```json
    \n{"ok":1,"state":{"state_name":"Unknown","state_id":6}}\n
```

An acknowledgment contains of two parts:

 * `ok`: boolean valid command flag
 * `state`: string (`state_name`) and enum value (`state_id`) of current state.

This is an example acknowledgment for an invalid, i.e. unknown, command sent to the lid controller

```json
{
    "ok": 1,
    "state": {
        "state_name": "Unknown",
        "state_id": 6
    }
}
```

#### Actuator Movement Report

While an actuator is moving the lid controller does not try to send out any message
since the utilized library [Ethernet](https://github.com/arduino/Arduino/tree/master/libraries/Ethernet)
does not guarantee a non-blocking write. However incoming commands are processed.

In order to gather information about current consumption and position of an
actuator as a function of time during the movement, the lid controller stores a
maximum of 300 measurements every ~70ms during movement phases, allowing the report
to span a time range of roughly 21s. A complete stroke of each actuator in both directions
typically takes 8..10s. The lid controller automatically stops the movement and
transmits into a Fail state, when an actuator moves longer than ~15s.

This is an example of an Actuator Movement Report during `Opening`.

```json
    {
        "motor_id": 1,
        "duration[ms]": 8762,
        "motor_stop_reason": {
            "motor_stop_name": "Zerocurrent/Endswitch",
            "motor_stop_id": 2
        },
        "current": [472, 189, 205, 132],
        "position": [37, 37, 37, 37]
    }
```

It consists of the `motor_id` an integer number being either 0 or 1.
The `duration` in ms of actuator movement measured by the lid-controller (c.f. remarks
about Timer0 for details about the precision of this movement.)
The reason why the motor was stopped. These reasons are implemented as an `enum`
just like the states. This is the complete list of possible motor stop states:

```c++
typedef enum {
    M_NO_REASON,
    M_OVERCURRENT,
    M_ZEROCURRENT,
    M_TIMEOUT,
    M_POSITION_REACHED,
    M_USER_INTERUPT,
} motor_stop_reason_t;
```


Where `M_NO_REASON` is the initial value of the variable when the motor is started.
When the motor is stopped, one of the other values is assigned to this variable,
so when ever `M_NO_REASON` is actually sent out to the receiver, this displays
a programming error. In addition to the numerical value `motor_stop_id` also
a textual representation is transmitted for convenience.
The `current` and `position` arrays each contain measured values as a function of time.
Both values are 10 bit unsigned integers. The unit of one current LSB is 3.4mA [reference Vittorios documentation]
The unit of the hall sensor is unclear (at least to me at the moment)

> TODO!!: I can research which values stood for "totally extended" and for "completely contracted"
> in Vittorio's original firmware .. and then using the stroke length I can derive some number
> but the precision is questionable.


## Something about the 4 replies during movement

When sending a movement command. The receiver should expect 4 replies.

 * 1st ack: command valid & state change to `Opening` or `Closing`
 * 1st actuator movement report
 * 2nd actuator movement report
 * final ack: state change

final acknowledge was introduced for convenience. So one does not need to
request the state `s` after the second movement report.


## Interrupting actuator movement

Any command received by the lid controller
(apart from a repetition of the original movement command)
results in a motor stop (`M_USER_INTERUPT`) and a transition into a Fail state.
This includes status requests, since outgoing transmission is wanted during actuator movement.

Depending on the timing of a possible user interrupt the second actuator movement
report may be omitted.


[Torikai]: http://link.springer.com/chapter/10.1007%2F978-1-4899-0112-5_50  "Torikai, Ayako. 'Wavelength Sensitivity in the Photo-Degradation of Polymethyl Methacrylate: Accelerated Degradation and Gel Formation.' Science and Technology of Polymers and Advanced Materials. Springer US, 1998. 581-586."

