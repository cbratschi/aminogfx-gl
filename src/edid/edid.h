#ifndef _EDID_H
#define _EDID_H

//see https://github.com/nyorain/kms-vulkan/blob/e66655b05ebc6281c1ebffec12681240caf305be/kms-quads.h

#include <stdint.h>
#include <stddef.h>

/*
 * Parse the very basic information from the EDID block, as described in
 * edid.c. The EDID parser could be fairly trivially extended to pull
 * more information, such as the mode.
 */
struct edid_info {
    char pnp_id[5];         //Manufacturer ID (legacy value)
    uint16_t product_code;  //Manufacturer Product Code

    int week;
    int year;

    char edid_version[4];

    char eisa_id[13];
    char monitor_name[13];
    char serial_number[13];
};

struct edid_info *edid_parse(const uint8_t *data, size_t length);

#endif