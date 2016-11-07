### Lid Arduino 

Note:  device file `/dev/ttyACM0` is not unique, access the arduino better by e.g. this:
    
    /dev/serial/by-id/usb-Arduino__www.arduino.cc__Arduino_USB-Serial_6493633313735151B0D1-if00

or by the symlink Max Noethe has made:

    /dev/lid-arduino



# build and upload with

    cd Software/ShutterController/
    platformio run -t upload

# connect to serial monitor with

    cd Software/ShutterController/
    platformio device monitor -p /dev/lid-arduino -b 115200


# platformio.ini contents:

    [env:ethernet]
    platform = atmelavr
    board = ethernet
    framework = arduino
    upload_port = /dev/lid-arduino

