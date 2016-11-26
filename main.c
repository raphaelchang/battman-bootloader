#include <stdint.h>
#include "ch.h"
#include "hal.h"
#include "stm32f30x_conf.h"
#include "crc.h"
#include "utils.h"

#define BOOTLOADER_ADDR             0x08030000
#define FIRMWARE_ADDR               0x08000000
#define NEW_FW_ADDR                 0x08018000
#define FW_NUM_PAGES                48

#define LED_R_GPIO GPIOB
#define LED_R_PIN 11
#define LED_R_CHANNEL 3
#define LED_G_GPIO GPIOB
#define LED_G_PIN 10
#define LED_G_CHANNEL 2
#define LED_B_GPIO GPIOB
#define LED_B_PIN 3
#define LED_B_CHANNEL 1

static uint8_t *new_fw_addr;

static void reset(void) {
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, ENABLE);
    WWDG_SetPrescaler(WWDG_Prescaler_1);
    WWDG_SetWindowValue(255);
    WWDG_Enable(100);

    __disable_irq();
    for(;;){};
}

static uint16_t erase_firmware(void) {
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    for (int i = 0; i < FW_NUM_PAGES; i++) {
        uint16_t res = FLASH_ErasePage(FIRMWARE_ADDR + i * 2048);
        if (res != FLASH_COMPLETE) {
            return res;
        }
    }

    return FLASH_COMPLETE;
}

static uint16_t write_firmware_data(uint32_t offset, uint8_t *data, uint32_t len)
{
    FLASH_ClearFlag(FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    for (uint32_t i = 0; i < len; i += 2) {
        uint16_t res = FLASH_ProgramHalfWord(FIRMWARE_ADDR + offset + i, (uint16_t)(data[i + 1] << 8) | data[i]);
        if (res != FLASH_COMPLETE) {
            return res;
        }
    }

    return FLASH_COMPLETE;
}

static int16_t write_firmware(void) {
    int32_t ind = 0;
    uint32_t size = utils_parse_uint32(new_fw_addr, &ind);
    uint16_t crc_app = utils_parse_uint16(new_fw_addr, &ind);

    /*if (size > NEW_FW_MAX_SIZE) {*/
    /*return -1;*/
    /*}*/

    if (size == 0) {
        return -2;
    }

    uint16_t crc_calc = crc16(new_fw_addr + ind, size);
    if (crc_calc != crc_app) {
        return -3;
    }

    int16_t res = erase_firmware();
    if (res != FLASH_COMPLETE) {
        return res;
    }

    return write_firmware_data(0, new_fw_addr + ind, size);
}

void sleep(int time) {
    for (volatile int i = 0;i < time * 10000;i++) {
        __NOP();
    }
}

void blink_led(int blinks, bool positive) {
    if (positive) {
        for (int i = 0; i < blinks; i++) {
            palClearPad(LED_G_GPIO, LED_G_PIN);
            palClearPad(LED_B_GPIO, LED_B_PIN);
            sleep(100);
            palSetPad(LED_G_GPIO, LED_G_PIN);
            palSetPad(LED_B_GPIO, LED_B_PIN);
            sleep(100);
        }
    } else {
        for (int i = 0; i < blinks; i++) {
            palClearPad(LED_R_GPIO, LED_R_PIN);
            sleep(100);
            palSetPad(LED_R_GPIO, LED_R_PIN);
            sleep(100);
        }
    }
}

int main(void)
{
    SCB->VTOR = BOOTLOADER_ADDR;
    new_fw_addr = (uint8_t *)NEW_FW_ADDR;

    palSetPadMode(LED_R_GPIO, LED_R_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPad(LED_R_GPIO, LED_R_PIN);
    palSetPadMode(LED_G_GPIO, LED_G_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPad(LED_G_GPIO, LED_G_PIN);
    palSetPadMode(LED_B_GPIO, LED_B_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    palSetPad(LED_B_GPIO, LED_B_PIN);

    for (int i = 0; i < 5; i++) {
        int16_t res = write_firmware();

        if (res == -1) {
            blink_led(3, false);
            reset();
        } else if (res == -2) {
            blink_led(4, false);
            reset();
        } else if (res == -3) {
            blink_led(5, false);
            reset();
        } else if (res == FLASH_COMPLETE) {
            blink_led(3, true);
            reset();
        }
        blink_led(2, false);
        sleep(2000);
    }
    sleep(2000);
    blink_led(8, false);
    reset();
    for(;;) {
        sleep(1000);
    }
}
