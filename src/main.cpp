#include <Arduino.h>
#include <USBCDC.h>
#include <USB.h>
#include <gmp-ino.h>
#include "soc/gpio_struct.h"
extern "C" {
#include "chudnovsky.h"
}

#include "ble_config.h"

CustomBLEDevice device;

static void controls(const char c, const log_target_e target);

#ifndef log_cdc
size_t log_cdc_raw(const int target, const char *format, ...);
#define log_cdc(target, format, ...) log_cdc_raw(target, format "\n", ##__VA_ARGS__)
#endif


size_t log_cdc_raw(const int target, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    
    size_t written = 0;
    if (target == TARGET_USB)
        written = Serial.vprintf(format, args);
    
    else if (target == TARGET_BLE)
        written = device.vprintf(format, args);
    
    va_end(args);
    return written;
}

/**
 * Task to receive and process data from BLE
 * @param param Pointer to task parameters (not used)
 */
static void bt_recv_task(void *param)
{
    data_len_t *data = NULL;
    while (true)
    {
        if (!device.canRead()) 
        {
            vTaskDelay(100);
            continue;
        }
        if (device.read(&data, 2000) == pdTRUE)
        {
            Serial.write(data->data, data->len);
            for (int i = 0; i < data->len; i++)
                controls(data->data[i], TARGET_BLE);
            free(data);
        } 
    }
    vTaskDelete(NULL);
}

static uint32_t alloc = 0;
static uint32_t max_alloc = 0;
// std::map<void*, size_t> allocation_map;

void* psram_malloc(size_t n) {
    void* ptr = heap_caps_malloc(n, MALLOC_CAP_INTERNAL);
    // if (ptr) {
    //     alloc += n;
    //     allocation_map[ptr] = n;
    // }
    alloc++;
    max_alloc = alloc > max_alloc? alloc : max_alloc;
    return ptr;
}

void* psram_realloc(void* p, size_t unused, size_t b) {
    // if (allocation_map.find(p) != allocation_map.end()) {
    //     alloc -= allocation_map[p];
    //     allocation_map.erase(p);
    // }
    void* new_ptr = heap_caps_realloc(p, b, MALLOC_CAP_INTERNAL);
    // if (new_ptr) {
    //     alloc += b;
    //     allocation_map[new_ptr] = b;
    // }
    max_alloc = alloc > max_alloc? alloc : max_alloc;
    return new_ptr;
}

void psram_free(void* p, size_t unused) {
    // if (allocation_map.find(p) != allocation_map.end()) {
    //     alloc -= allocation_map[p];
    //     allocation_map.erase(p);
    // }
    alloc--;
    max_alloc = alloc > max_alloc? alloc : max_alloc;
    heap_caps_free(p);
}

void boot_config() {
    Serial.begin();
}

static mpz_t val;

void euler(mpz_t res, int digits) {
    mpz_t one, term, denom;
    mpz_init(term);
    mpz_init_set_ui(one, 10);
    mpz_init_set_ui(denom, 2);
    mpz_pow_ui(one, one, digits);
    
    mpz_mul_ui(res, one, 2);
    for (int i = 3; i < digits; i++) {
        mpz_tdiv_q(term, one, denom);
        if (mpz_sgn(term) == 0)
            break;
        mpz_add(res, res, term);
        mpz_mul_ui(denom, denom, i);
    }
    mpz_clear(one);
    mpz_clear(term);
    mpz_clear(denom);
}

static char pi_string[5020];

static void do_calculation(void (*func)(mpz_t, int), int digits, log_target_e target) {

    max_alloc = alloc;
    // log_cdc(target,"Before: %lu", alloc);

    mpz_init(val);
    int64_t time = esp_timer_get_time();
    func(val, digits);
    time = esp_timer_get_time() - time;

    // log_cdc(target,"Calculated: %lu", alloc);

    char* str = mpz_get_str(pi_string, 10, val);
    mpz_clear(val);

    // log_cdc(target,
    //     "After: %lu\n"
    //     "time: %llu\n"
    //     "Max Alloc: %lu\n"
    //     "Pi:"
    //     ,
    //     alloc, time, max_alloc
    // );
    log_cdc(target, "%s", str);
}

static inline const char* get_power_level_string(const esp_power_level_t pl)
{
    switch(pl) {
        case ESP_PWR_LVL_N24: 
            return "ESP_PWR_LVL_N24";
        case ESP_PWR_LVL_N21: 
            return "ESP_PWR_LVL_N21";
        case ESP_PWR_LVL_N18: 
            return "ESP_PWR_LVL_N18";
        case ESP_PWR_LVL_N15: 
            return "ESP_PWR_LVL_N15";
        case ESP_PWR_LVL_N12: 
            return "ESP_PWR_LVL_N12";
        case ESP_PWR_LVL_N9: 
            return "ESP_PWR_LVL_N9";
        case ESP_PWR_LVL_N6: 
            return "ESP_PWR_LVL_N6";
        case ESP_PWR_LVL_N3: 
            return "ESP_PWR_LVL_N3";
        case ESP_PWR_LVL_N0: 
            return "ESP_PWR_LVL_N0";
        case ESP_PWR_LVL_P3: 
            return "ESP_PWR_LVL_P3";
        case ESP_PWR_LVL_P6: 
            return "ESP_PWR_LVL_P6";
        case ESP_PWR_LVL_P9: 
            return "ESP_PWR_LVL_P9";
        case ESP_PWR_LVL_P12: 
            return "ESP_PWR_LVL_P12";
        case ESP_PWR_LVL_P15: 
            return "ESP_PWR_LVL_P15";
        case ESP_PWR_LVL_P18: 
            return "ESP_PWR_LVL_P18";
        case ESP_PWR_LVL_P20: 
            return "ESP_PWR_LVL_P20";
        default: return "ESP_PWR_LVL_INVALID";
    }
    return "ESP_PWR_LVL_INVALID";
}

static const esp_power_level_t power_levels[] = {
    ESP_PWR_LVL_N24,
    ESP_PWR_LVL_N21,
    ESP_PWR_LVL_N18,
    ESP_PWR_LVL_N15,
    ESP_PWR_LVL_N12,
    ESP_PWR_LVL_N9,
    ESP_PWR_LVL_N6,
    ESP_PWR_LVL_N3,
    ESP_PWR_LVL_N0,
    ESP_PWR_LVL_P3,
    ESP_PWR_LVL_P6,
    ESP_PWR_LVL_P9,
    ESP_PWR_LVL_P12,
    ESP_PWR_LVL_P15,
    ESP_PWR_LVL_P18,
    ESP_PWR_LVL_P20,
};
static const int n_power_levels = sizeof(power_levels) / sizeof(power_levels[0]);

static const int digits[] = {
    100, 200, 300, 400, 500, 
    600, 700, 800, 900, 1000, 
    2000, 3000, 4000, 5000
};
static const int n_digits = sizeof(digits) / sizeof(digits[0]);

static void controls(const char c, const log_target_e target) {

    static int selected = 0;
    static int selected_power_level = 10;

    if (c == 'a' || c == 'b') 
    {
        // select digits to compute (a: increment, b: decrement)
        selected += c == 'a'? 1 : (n_digits - 1);
        selected %= n_digits;

        log_cdc(target,"Selected: %d digits", digits[selected]);
    } 
    else if (c == 'p' || c == 'e') 
    {
        // calculate euler or pi
        do_calculation(
            c == 'p'? chudnovsky : euler, 
            digits[selected], 
            target
        );
    } 
    else if (c == 'm' || c == 'n') 
    {
        // select Ble Power levels (m: increment, n: decrement)
        selected_power_level += c == 'm'? 1 : (n_power_levels - 1);
        selected_power_level %= n_power_levels;

        BLEDevice::setPower(power_levels[selected_power_level]);

        const char* power_level_str = 
            get_power_level_string(power_levels[selected_power_level]);
            
        log_cdc(target, "Power Level: %s", power_level_str);
    } 
    else if (c == 's') 
    {
        // system status
        const char* fm = "UNKNOWN";
        switch(ESP.getFlashChipMode()) {
            case FM_QIO: fm = "FM_QIO"; break;
            case FM_QOUT: fm = "FM_QOUT"; break;
            case FM_DIO: fm = "FM_DIO"; break;
            case FM_DOUT: fm = "FM_DOUT"; break;
            case FM_FAST_READ: fm = "FM_FAST_READ"; break;
            case FM_SLOW_READ: fm = "FM_SLOW_READ"; break;
            case FM_UNKNOWN: 
            default: fm = "FM_UNKNOWN"; break;
        }
        log_cdc(target, 
            "CPU: %lu, Heap: %lu, Flash: %s\n"
            "Alloc: %lu, Max Alloc: %lu", 
            ESP.getCpuFreqMHz(), ESP.getFreeHeap(), fm,
            alloc, max_alloc
        );
    }
    else if (c == 'j' || c == 'k' || (c >= '0' && c <= '9')) 
    {
        // set LED duty cycle
        static int duty = 0;
        duty = (c >= '0' && c <= '9')? 
            (1023 * ((int)c - '0') / 9) :
            c == 'j'? 
                (duty < 1023? duty + 1 : 1023) :
                (duty > 0? duty - 1 : 0);

        ledcWrite(8, duty);
        log_cdc(target, "duty: %d", duty);
    }
}

void setup() {
    boot_config();
    device.begin("Pi Sensor");
    xTaskCreate(bt_recv_task, "recv task", 4096, NULL, 5, NULL);

    ledcSetClockSource(LEDC_AUTO_CLK);
    ledcAttach(8, 78125, 10);

    mp_set_memory_functions(psram_malloc, psram_realloc, psram_free);
    pinMode(0, INPUT_PULLUP);
}


void loop() {

    if (Serial.available()) {
        char c = Serial.read();
        controls(c, TARGET_USB);
    }
}