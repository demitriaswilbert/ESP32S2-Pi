#include <Arduino.h>
#include <USBCDC.h>
#include <USB.h>
#include <gmp-ino.h>

bool tasks_run = true;
int tasks_run_count = 0;

USBCDC Serial;

#define PWM_RES 8
#define PWM_CH 0
#define PWM_PIN 16
#define PWM_FREQ 40000
#define BLDC_MIN 1 // ((1 << PWM_RES) / 2000)
#define BLDC_MAX ((1 << PWM_RES) - 1)
int val = BLDC_MIN;

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

typedef struct {
    mpz_t P, Q, R;
} PQR;

typedef struct {
    mpz_t c3_24;
    mpz_t a3;
    mpz_t _545140134;
    size_t nres;
    PQR* res[2];
} BS_t;

static uint32_t min_psram = 0xffffffffUL;
void minpsram() {
    min_psram = (ESP.getFreePsram() < min_psram) ? ESP.getFreePsram() : min_psram;
}
#define mpzinit(v) {mpz_init(v); minpsram();}
#define mpzclear(v) mpz_clear(v)

void initPQR(PQR* pqr) {
    mpzinit(pqr->P);
    mpzinit(pqr->Q);
    mpzinit(pqr->R);
} 

void clearPQR(PQR* pqr) {
    mpzclear(pqr->P);
    mpzclear(pqr->Q);
    mpzclear(pqr->R);
}

void BS_init(BS_t* bs, size_t depth) {
    mpz_init(bs->c3_24);
    mpz_init(bs->a3);
    mpz_init(bs->_545140134);
    mpz_set_str(bs->c3_24, "10939058860032000", 10);
    bs->nres = depth;
    bs->res[0] = (PQR*) malloc(sizeof(PQR) * depth);
    bs->res[1] = (PQR*) malloc(sizeof(PQR) * depth);
    for (int i = 0; i < depth; i++) {
        initPQR(&bs->res[0][i]);
        initPQR(&bs->res[1][i]);
    }
}

void BS_deinit(BS_t* bs) {
    mpz_clear(bs->a3);
    mpz_clear(bs->_545140134);
    mpz_clear(bs->c3_24);
    for (int i = 0; i < bs->nres; i++) {
        clearPQR(&bs->res[0][i]);
        clearPQR(&bs->res[1][i]);
    }
    bs->nres = 0;
    free(bs->res[0]);
    free(bs->res[1]);
}

static int max_layer = 0;

PQR* binary_split(int a, int b, BS_t* data, int layer, int pos) {
    PQR* res = &data->res[pos][layer];
    if (b == a + 1) {
        mpz_set_si(res->P, -(6*a - 5));
        mpz_mul_si(res->P, res->P, 2 * a - 1);
        mpz_mul_si(res->P, res->P, 6 * a - 1);
        
        mpz_set_si(data->a3, a); mpz_pow_ui(data->a3, data->a3, 3);
        mpz_mul(res->Q, data->c3_24, data->a3);

        mpz_set_ui(data->_545140134, 545140134);
        mpz_mul_ui(data->_545140134, data->_545140134, a);
        mpz_add_ui(data->_545140134, data->_545140134, 13591409);
        mpz_mul(res->R, res->P, data->_545140134);


    } else {
        int m = (a + b) / 2;

        PQR *pqr_am = binary_split(a, m, data, layer + 1, 0);
        PQR *pqr_mb = binary_split(m, b, data, layer + 1, 1);

        mpz_mul(res->P, pqr_am->P, pqr_mb->P); 
        mpz_mul(res->Q, pqr_am->Q, pqr_mb->Q); 

        mpz_mul(pqr_am->P, pqr_am->P, pqr_mb->R);
        mpz_mul(res->R, pqr_mb->Q, pqr_am->R);
        mpz_add(res->R, res->R, pqr_am->P);

    }
    return res;
}

typedef struct {
    char* buf;
    size_t size;
} string_t;

void chudnovsky(int n, string_t* string) {
    
    BS_t data;

    int n_iter = n / 14 + 1;
    int tmp = 31 - __builtin_clz(n_iter) + 2;
    BS_init(&data, tmp);

    PQR* _1n = binary_split(1, n_iter, &data, 0, 0);

    mpz_t num; mpzinit(num);
    mpz_set_ui(num, 10);
    mpz_pow_ui(num, num, n * 2);
    mpz_mul_ui(num, num, 10005);
    mpz_sqrt(num, num);

    mpz_mul(num, num, _1n->Q); // Multiply by sqrt(10005)
    mpz_mul_ui(num, num, 426880);  // Multiply by 426880

    mpz_t denom; mpzinit(denom);
    mpz_mul_ui(denom, _1n->Q, 13591409); // Multiply by 13591409
    mpz_add(denom, denom, _1n->R); // Add R1n to denominator
  
    BS_deinit(&data);
    if (mpz_sgn(denom) != 0) 
        mpz_cdiv_q(num, num, denom);
    
    mpzclear(denom);

    string->size = mpz_sizeinbase(num, 10) + 2;
    string->buf = (char*) heap_caps_malloc(string->size, MALLOC_CAP_SPIRAM);
    mpz_get_str(string->buf, 10, num);

    mpzclear(num);
}

void setup() {

    boot_config();

    vTaskDelay(10);
    ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
    ledcAttachPin(PWM_PIN, PWM_CH);
    ledcWrite(PWM_CH, val);

    pinMode(21, INPUT_PULLUP);
    pinMode(18, INPUT_PULLUP);
    pinMode(17, INPUT_PULLUP);

    mp_set_memory_functions(psram_malloc, psram_realloc, psram_free);
    Serial0.begin(115200);
    Serial0.setPins(13, 15);
}

void loop() {

    // for (int i = 500; i < 5001; i += 500) {
        uint32_t tmp_free = ESP.getFreePsram();
        min_psram = 0xfffffffful;
        string_t str;
        int64_t time = esp_timer_get_time();
        chudnovsky(3142, &str);
        time = esp_timer_get_time() - time;
        Serial.write(str.buf);
        Serial.printf("\nbefore: %lu\nafter: %lu\ntime: %lu\n\n", tmp_free, min_psram, (uint32_t)time);
        Serial0.write(str.buf);
        heap_caps_free(str.buf);
        // vTaskDelay(1000);
    // }


    // static bool clkstate_prev = true;
    // static int64_t clk_tmr = esp_timer_get_time();
    // bool clkstate = !!(GPIO.in & (1UL << 21));
    // bool dtstate = !!(GPIO.in & (1UL << 17));
    // bool btnstate = !!(GPIO.in & (1UL << 18));
    // if (!clkstate && clkstate_prev) {
    //     clkstate_prev = false;
    //     int64_t tmr_diff = esp_timer_get_time() - clk_tmr;
    //     if (tmr_diff >= 2500) {
    //         tmr_diff = tmr_diff > 20000? 20000 : tmr_diff;
    //         int delta = (200000LL / tmr_diff) - 9;
    //         if (dtstate) 
    //             val = val < (BLDC_MAX - delta)? val + delta : BLDC_MAX; 
    //         else
    //             val = val > (BLDC_MIN + delta)? val - delta : BLDC_MIN; 

    //         ledcWrite(PWM_CH, val);
    //         Serial.printf("BLDC: %d %d%\n", val, (val - BLDC_MIN) * 100 / BLDC_MIN);
    //     }
    //     clk_tmr = esp_timer_get_time();
    // } else if (clkstate && !clkstate_prev) {
    //     clkstate_prev = true;
    //     clk_tmr = esp_timer_get_time();
    // }
    // if (!btnstate && val != BLDC_MIN) {
    //     val = BLDC_MIN;
    //     ledcWrite(PWM_CH, val);
    //     Serial.printf("BLDC: %d %d%\n", val, (val - BLDC_MIN) * 100 / BLDC_MIN);
    // }

    // digitalWrite(ledPin, ledState);
}