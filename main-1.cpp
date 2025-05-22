#include "MicroBit.h"

MicroBit uBit;

void sendHelloWorld() {
    while (true) {
        uBit.serial.send("Hello World\r\n");   // ↚ USB CDC
        fiber_sleep(1000);
    }
}

int main() {
    uBit.init();
    uBit.serial.baud(115200);  // même débit que ton screen
    create_fiber(sendHelloWorld);
    release_fiber();           // rend la main au scheduler
}

