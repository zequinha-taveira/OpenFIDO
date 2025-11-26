/**
 * @file u2f.h
 * @brief U2F (CTAP1) Protocol Implementation
 *
 * @copyright Copyright (c) 2025 OpenFIDO Contributors
 * @license MIT License
 */

#ifndef U2F_H
#define U2F_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* U2F Command Codes (INS) */
#define U2F_REGISTER 0x01
#define U2F_AUTHENTICATE 0x02
#define U2F_VERSION 0x03

/* U2F Authentication Control Byte (P1) */
#define U2F_AUTH_ENFORCE 0x03      /* Enforce user presence and sign */
#define U2F_AUTH_CHECK_ONLY 0x07   /* Check only (don't sign) */
#define U2F_AUTH_DONT_ENFORCE 0x08 /* Don't enforce user presence */

/* U2F Status Words */
#define U2F_SW_NO_ERROR 0x9000
#define U2F_SW_WRONG_DATA 0x6A80
#define U2F_SW_CONDITIONS_NOT_SATISFIED 0x6985
#define U2F_SW_COMMAND_NOT_ALLOWED 0x6986
#define U2F_SW_INS_NOT_SUPPORTED 0x6D00

/* U2F APDU Structure */
typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc1;
    uint8_t lc2;
    uint8_t lc3;
    uint8_t *data;
    size_t data_len;
    size_t le;
} u2f_apdu_t;

/**
 * @brief Process a U2F APDU
 *
 * @param request_data Raw request data
 * @param request_len Length of request data
 * @param response_data Buffer for response data
 * @param response_len Pointer to response length
 * @return U2F status word (SW)
 */
uint16_t u2f_process_apdu(const uint8_t *request_data, size_t request_len, uint8_t *response_data,
                          size_t *response_len);

#ifdef __cplusplus
}
#endif

#endif /* U2F_H */
