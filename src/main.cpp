#include <Arduino.h>
#include <USBCDC.h>
#include <USB.h>
#include <gmp-ino.h>
extern "C" {
#include "chudnovsky.h"
}

USBCDC Serial;

void* psram_malloc(size_t n) {
    return heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
}
void* psram_realloc(void* p, size_t unused, size_t b) {
    return heap_caps_realloc(p, b, MALLOC_CAP_SPIRAM);
}
void psram_free(void* p, size_t unused) {
    heap_caps_free(p);
}

void boot_config() {
    USB.productName("Dewe BLDC Test");
    USB.manufacturerName("Dewe's Nut");
    USB.begin();
    Serial.begin();
    // Serial.setDebugOutput(true);
}

static mpz_t val;
static uint32_t free_heap = 0;
static uint32_t free_psram = 0;

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
    free_psram = ESP.getFreePsram();
    free_heap = ESP.getFreeHeap();
    mpz_clear(one);
    mpz_clear(term);
    mpz_clear(denom);
}

void setup() {

    boot_config();

    pinMode(21, INPUT_PULLUP);
    pinMode(18, INPUT_PULLUP);
    pinMode(17, INPUT_PULLUP);

    mp_set_memory_functions(psram_malloc, psram_realloc, psram_free);
    //void begin(unsigned long baud, uint32_t config=SERIAL_8N1, int8_t rxPin=-1, int8_t txPin=-1, bool invert=false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 112);
    Serial0.begin(115200, SERIAL_8N1, 13, 16, true);
    mpzinit(val);
    pinMode(0, INPUT_PULLUP);
}
int inited = 0;
void loop() {

    // static int64_t last = esp_timer_get_time();
    // int64_t now = esp_timer_get_time();
    // if (now - last >= 2000000LL) {
    //     last = now;
    // }
    if (!digitalRead(0)) {
        int64_t time = esp_timer_get_time();
        euler(val, 1000);
        time = esp_timer_get_time() - time;

        char* str = mpz_get_str(NULL, 10, val);
        String pi_str = str;
        free(str);

        // str = pi_str.begin();
        // size_t pi_len = pi_str.length();
        // for (size_t i = 0; i < pi_len; i++) 
        //     str[i] -= '0';
        
        Serial.print(pi_str);
        Serial.printf("\ntime: %lu\ninited:%d\nFree Heap: %lu\nFree PSRAM: %lu\n", (uint32_t)time, inited, ESP.getFreeHeap(), ESP.getFreePsram());
        Serial.printf("Used Heap: %lu\nUsed PSRAM: %lu\n\n", free_heap, free_psram);
        Serial0.print(pi_str);
    }
    vTaskDelay(200);
}