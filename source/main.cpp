/* ----------------------------------------------------------------------
 *  CPE RADIO BRIDGE v2 – micro:bit Gateway
 *  - Radio → USB : déchiffre trames CPE v2 (12 o), extrait mesures et
 *                  envoie un JSON sur le port série.
 *  - USB  → Radio : accepte une commande « SETORDER,<id>,<TLHP> »
 *                  construit la trame CONTROL correspondante et
 *                  l’émet en radio.
 *  Auteur : Mathis LAMBERT – mai 2025
 * --------------------------------------------------------------------*/
#include "MicroBit.h"
#include "cpe.h"
#include <cstring>
#include <cstdlib>

#define RADIO_GROUP 42
#define SERIAL_BAUD 115200

static const uint8_t KEY[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

MicroBit uBit;
static uint8_t seq = 0; /* nonce radio local */

/* ----------------- helpers visuels ------------------------------------ */
static inline void flash(uint8_t x, uint8_t y)
{
    uBit.display.image.setPixelValue(x, y, 255);
    fiber_sleep(40);
    uBit.display.image.setPixelValue(x, y, 0);
}

/* ----------------- convertion TLHP -> ctrl byte ---------------------- */
static bool orderStringToCtrl(const char *s, uint8_t &ctrl)
{
    if (!s || strlen(s) != 4)
        return false;
    auto map = [&](char c) -> int
    { switch(c){case 'T':case 't':return CPE_S_T;case 'L':case 'l':return CPE_S_L;case 'H':case 'h':return CPE_S_H;case 'P':case 'p':return CPE_S_P;default:return -1;} };
    int a = map(s[0]), b = map(s[1]), c = map(s[2]), d = map(s[3]);
    if (a < 0 || b < 0 || c < 0 || d < 0)
        return false;
    ctrl = cpe_ctrl_pack((cpe_sensor_t)a, (cpe_sensor_t)b, (cpe_sensor_t)c, (cpe_sensor_t)d);
    return true;
}

/* ----------------- USB cmd → Radio CONTROL --------------------------- */
static void processSerialCommand(const ManagedString &line)
{
    /* on attend : SETORDER,<id>,<TLHP> */
    char buf[32];
    const char *raw = line.toCharArray();
    strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, ",:\r\n");
    if (!tok)
        return;
    if (strcmp(tok, "SETORDER") != 0 && strcmp(tok, "setorder") != 0)
        return;

    /* id */
    tok = strtok(nullptr, ",:\r\n");
    if (!tok)
        return;
    int id = atoi(tok);
    /* ordre */
    tok = strtok(nullptr, ",:\r\n");
    if (!tok)
        return;
    uint8_t ctrl;
    if (!orderStringToCtrl(tok, ctrl))
        return;

    uint8_t frame[CPE_PAYLOAD_LEN];
    cpe_build_control_frame(ctrl, (uint8_t)id, seq++, frame);
    uBit.radio.datagram.send(frame, CPE_PAYLOAD_LEN);
    flash(4, 0); /* pixel (4,0) = USB→Radio */
}

/* ----------------- Radio Rx → USB ------------------------------------ */
void onRadio(MicroBitEvent)
{
    PacketBuffer p = uBit.radio.datagram.recv();

    if (p.length() != CPE_PAYLOAD_LEN)
        return;

    cpe_frame_type_t ft;
    uint8_t dev;
    cpe_measure_t meas;
    uint8_t ctrl;
    if (cpe_parse_frame(p.getBytes(), &ft, &dev, &meas, &ctrl) != 0)
        return;

    if (ft == CPE_FT_MEASURE)
    {
        char json[96];
        int tI = meas.temperature_centi / 100, tF = abs(meas.temperature_centi) % 100;
        int hI = meas.humidity_centi / 100, hF = meas.humidity_centi % 100;
        int pI = meas.pressure_decihPa / 10, pF = meas.pressure_decihPa % 10;
        snprintf(json, sizeof json,
                 "{\"id\":%u,\"t\":%d.%02d,\"h\":%d.%02d,\"p\":%d.%01d,\"lux\":%d}\r\n",
                 dev, tI, tF, hI, hF, pI, pF, meas.lux);
        uBit.serial.send(json);
        flash(0, 1); /* pixel (0,1) = radio→USB */
    }
    else if (ft == CPE_FT_CONTROL)
    {
        char buf[48];
        snprintf(buf, sizeof buf, "{\"ctrl\":%02X,\"id\":%u}\r\n", ctrl, dev);
        uBit.serial.send(buf);
    }
    flash(0, 0); /* pixel (0,0) = réception OK */
}

/* ----------------- USB listener (fibre) ------------------------------ */
void serialBridge()
{
    ManagedString buf;
    while (true)
    {
        int c = uBit.serial.read();
        if (c >= 0)
        {
            if (c == '\n' || c == '\r')
            {
                if (buf.length())
                    processSerialCommand(buf);
                buf = ManagedString();
            }
            else
                buf = buf + (char)c;
        }
        else
            uBit.sleep(2);
    }
}

/* --------------------------- main ------------------------------------ */
int main()
{
    uBit.init();
    cpe_init(KEY);

    uBit.serial.baud(SERIAL_BAUD);

    uBit.radio.setTransmitPower(7);
    int ret = uBit.radio.setGroup(RADIO_GROUP);
    if (ret != MICROBIT_OK)
    {
        uBit.serial.send("[ERROR] setGroup failed\n");
        release_fiber();
    }

    int ret2 = uBit.radio.enable();
    if (ret2 != MICROBIT_OK)
    {
        uBit.serial.send("[ERROR] enable failed\n");
        release_fiber();
    }

    uBit.messageBus.listen(MICROBIT_ID_RADIO, MICROBIT_RADIO_EVT_DATAGRAM, onRadio);

    create_fiber(serialBridge);

    uBit.display.scroll("BRIDGE v2");

    release_fiber();
}
