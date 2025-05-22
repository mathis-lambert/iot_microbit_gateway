#ifndef CPE_H
#define CPE_H
#include <stdint.h>
#include <stddef.h>

/* ---------------- Tailles ---------------- */
#define CPE_PLAINTEXT_LEN 11 /* 1 type + 1 id + 8 data + 1 pad      */
#define CPE_PAYLOAD_LEN 12   /* seq (1) + chiffré (11)             */
#define CPE_KEY_LEN 16

/* ---------------- Types de trame --------- */
typedef enum
{
    CPE_FT_MEASURE = 0x01,
    CPE_FT_CONTROL = 0x02
} cpe_frame_type_t;

/* ---------------- Codage ordre OLED ------ */
typedef enum
{
    CPE_S_T = 0,
    CPE_S_L = 1,
    CPE_S_H = 2,
    CPE_S_P = 3
} cpe_sensor_t;
static inline uint8_t cpe_ctrl_pack(cpe_sensor_t l0, cpe_sensor_t l1,
                                    cpe_sensor_t l2, cpe_sensor_t l3)
{
    return (uint8_t)((l0 & 3U) |
                     ((l1 & 3U) << 2) |
                     ((l2 & 3U) << 4) |
                     ((l3 & 3U) << 6));
}
static inline void cpe_ctrl_unpack(uint8_t c, cpe_sensor_t o[4])
{
    o[0] = (cpe_sensor_t)((c >> 0) & 3U);
    o[1] = (cpe_sensor_t)((c >> 2) & 3U);
    o[2] = (cpe_sensor_t)((c >> 4) & 3U);
    o[3] = (cpe_sensor_t)((c >> 6) & 3U);
}

/* ---------------- Mesures brutes --------- */
typedef struct
{
    int16_t temperature_centi;
    uint16_t humidity_centi;
    uint16_t pressure_decihPa;
    int16_t lux;
} cpe_measure_t;

/* ---------------- API -------------------- */
#ifdef __cplusplus
extern "C"
{
#endif
    void cpe_init(const uint8_t key[CPE_KEY_LEN]);

    void cpe_build_measure_frame(const cpe_measure_t *m,
                                 uint8_t device_id,
                                 uint8_t seq,
                                 uint8_t out_frame[CPE_PAYLOAD_LEN]);

    void cpe_build_control_frame(uint8_t ctrl_byte,
                                 uint8_t device_id,
                                 uint8_t seq,
                                 uint8_t out_frame[CPE_PAYLOAD_LEN]);

    /* parse : remplit selon le type                                         */
    int cpe_parse_frame(const uint8_t frame[CPE_PAYLOAD_LEN],
                        cpe_frame_type_t *type_out,
                        uint8_t *dev_id_out,
                        cpe_measure_t *meas_out, /* NULL si pas utile   */
                        uint8_t *ctrl_out);      /* NULL si pas utile   */

#ifdef __cplusplus
}
#endif
#endif /* CPE_H */
