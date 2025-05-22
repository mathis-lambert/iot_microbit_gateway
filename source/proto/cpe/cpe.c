#include "cpe.h"
#include <string.h>
#include <tinycrypt/aes.h>
#include <tinycrypt/ctr_mode.h>

static uint8_t g_key[CPE_KEY_LEN];

/* ---------- Crypto CTR -------------- */
static void crypt(uint8_t *buf, const uint8_t iv[16])
{
    struct tc_aes_key_sched_struct ks;
    tc_aes128_set_encrypt_key(&ks, g_key);
    uint8_t ctr[16];
    memcpy(ctr, iv, 16);
    tc_ctr_mode(buf, CPE_PLAINTEXT_LEN, buf, CPE_PLAINTEXT_LEN, ctr, &ks);
}

/* ---------- Pack MEASURE ------------ */
static void pack_measure(const cpe_measure_t *m, uint8_t dev,
                         uint8_t p[CPE_PLAINTEXT_LEN])
{
    p[0] = CPE_FT_MEASURE;
    p[1] = dev;

    p[2] = ((uint16_t)m->temperature_centi) >> 8;
    p[3] = ((uint16_t)m->temperature_centi) & 0xFF;
    p[4] = (m->humidity_centi) >> 8;
    p[5] = (m->humidity_centi) & 0xFF;
    p[6] = (m->pressure_decihPa) >> 8;
    p[7] = (m->pressure_decihPa) & 0xFF;
    p[8] = ((uint16_t)m->lux) >> 8;
    p[9] = ((uint16_t)m->lux) & 0xFF;
    p[10] = 0; /* pad */
}

/* ---------- Pack CONTROL ------------ */
static void pack_control(uint8_t ctrl, uint8_t dev,
                         uint8_t p[CPE_PLAINTEXT_LEN])
{
    memset(p, 0, CPE_PLAINTEXT_LEN);
    p[0] = CPE_FT_CONTROL;
    p[1] = dev;
    p[2] = ctrl; /* ordre OLED */
}

/* ---------- Bâtisseur commun -------- */
static void build_common(const uint8_t plain[11], uint8_t seq,
                         uint8_t out[CPE_PAYLOAD_LEN])
{
    uint8_t iv[16] = {0};
    iv[15] = seq;
    uint8_t buf[11];
    memcpy(buf, plain, 11);
    crypt(buf, iv);
    out[0] = seq;
    memcpy(out + 1, buf, 11);
}

/* ---------- API build --------------- */
void cpe_init(const uint8_t key[CPE_KEY_LEN])
{
    memcpy(g_key, key, CPE_KEY_LEN);
}

void cpe_build_measure_frame(const cpe_measure_t *m,
                             uint8_t dev, uint8_t seq, uint8_t outf[12])
{
    uint8_t p[11];
    pack_measure(m, dev, p);
    build_common(p, seq, outf);
}
void cpe_build_control_frame(uint8_t ctrl,
                             uint8_t dev, uint8_t seq, uint8_t outf[12])
{
    uint8_t p[11];
    pack_control(ctrl, dev, p);
    build_common(p, seq, outf);
}

/* ---------- Parse ------------------- */
int cpe_parse_frame(const uint8_t f[12], cpe_frame_type_t *t,
                    uint8_t *dev, cpe_measure_t *m, uint8_t *c)
{
    if (!f || !t || !dev)
        return -1;
    uint8_t seq = f[0], iv[16] = {0};
    iv[15] = seq;
    uint8_t buf[11];
    memcpy(buf, f + 1, 11);
    crypt(buf, iv);

    *t = (cpe_frame_type_t)buf[0];
    *dev = buf[1];

    if (*t == CPE_FT_MEASURE)
    {
        if (!m)
            return -1;
        m->temperature_centi = (int16_t)((buf[2] << 8) | buf[3]);
        m->humidity_centi = (uint16_t)((buf[4] << 8) | buf[5]);
        m->pressure_decihPa = (uint16_t)((buf[6] << 8) | buf[7]);
        m->lux = (int16_t)((buf[8] << 8) | buf[9]);
    }
    else if (*t == CPE_FT_CONTROL)
    {
        if (!c)
            return -1;
        *c = buf[2];
    }
    else
        return -1;
    return 0;
}
