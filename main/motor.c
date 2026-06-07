#include "motor.h"
#include "pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "motor";

// ── PWM config ────────────────────────────────────────────────────────────────
#define PWM_FREQ_HZ          20000
#define PWM_RESOLUTION_HZ    10000000
#define PWM_PERIOD_TICKS     (PWM_RESOLUTION_HZ / PWM_FREQ_HZ)   // 500

// ── pin tables ────────────────────────────────────────────────────────────────
static const int HS_PINS[3] = { TMC_UH, TMC_VH, TMC_WH };
static const int LS_PINS[3] = { TMC_UL, TMC_VL, TMC_WL };

// ── MCPWM handles (HS only, group 0) ─────────────────────────────────────────
static mcpwm_timer_handle_t s_timer;
static mcpwm_oper_handle_t  s_oper[3];
static mcpwm_cmpr_handle_t  s_cmp[3];
static mcpwm_gen_handle_t   s_gen[3];

// ── shared state ──────────────────────────────────────────────────────────────
static volatile bool        s_running    = false;
static volatile motor_dir_t s_dir        = MOTOR_DIR_LEFT;
static volatile uint32_t    s_step_ms    = 20;
static volatile uint32_t    s_duty_ticks = PWM_PERIOD_TICKS / 2;

// ── HS helpers ────────────────────────────────────────────────────────────────

static void hs_pwm(int i, uint32_t duty)
{
    mcpwm_comparator_set_compare_value(s_cmp[i], duty);
    mcpwm_generator_set_force_level(s_gen[i], -1, true);  // release → follow PWM
}

static void hs_off(int i)
{
    mcpwm_generator_set_force_level(s_gen[i], 0, true);   // force LOW
}

static void all_off(void)
{
    for (int i = 0; i < 3; i++) {
        hs_off(i);
        gpio_set_level(LS_PINS[i], 0);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  BLOCK COMMUTATION  (HS MCPWM + LS GPIO)
// ═════════════════════════════════════════════════════════════════════════════
#ifndef MOTOR_SINE_COMM

static const struct { int hs; int ls; } COMM[6] = {
    { 0, 1 },  // UH + VL
    { 0, 2 },  // UH + WL
    { 1, 2 },  // VH + WL
    { 1, 0 },  // VH + UL
    { 2, 0 },  // WH + UL
    { 2, 1 },  // WH + VL
};

static void set_bridges(int step)
{
    for (int i = 0; i < 3; i++) { hs_off(i); gpio_set_level(LS_PINS[i], 0); }
    hs_pwm(COMM[step].hs, s_duty_ticks);
    gpio_set_level(LS_PINS[COMM[step].ls], 1);
}

static void motor_task(void *arg)
{
    int step = 0;
    while (1) {
        if (!s_running) { all_off(); vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        if (!motor_diag_ok()) {
            ESP_LOGE(TAG, "DIAG error");
            s_running = false; all_off(); continue;
        }
        set_bridges(step);
        vTaskDelay(pdMS_TO_TICKS(s_step_ms));
        step = (s_dir == MOTOR_DIR_LEFT) ? (step + 5) % 6 : (step + 1) % 6;
    }
}

#else

// ═════════════════════════════════════════════════════════════════════════════
//  SINE COMMUTATION  (HS sinusoidal MCPWM + LS GPIO binary)
//
//  Positive half-cycle: HS PWM duty = |sin(θ)| × max_duty   LS = off
//  Negative half-cycle: HS off                               LS = on (GPIO)
//
//  HS current is sinusoidal → smooth torque, no rucking.
//  LS remains binary but switches slowly (once per electrical half-revolution).
// ═════════════════════════════════════════════════════════════════════════════


#define DEG2RAD(d)      ((d) * (float)M_PI / 180.0f)
#define SINE_TASK_MS    2
#define SINE_DEADBAND   0.02f

// phase index → { hs_index, ls_gpio_index }
// U=0, V=1, W=2
static void set_phase(int p, float v)
{
    uint32_t duty = (uint32_t)(fabsf(v) * (float)s_duty_ticks);
    if (duty > s_duty_ticks) duty = s_duty_ticks;

    if (v > SINE_DEADBAND) {
        gpio_set_level(LS_PINS[p], 0);
        hs_pwm(p, duty);
    } else if (v < -SINE_DEADBAND) {
        hs_off(p);
        gpio_set_level(LS_PINS[p], 1);
    } else {
        hs_off(p);
        gpio_set_level(LS_PINS[p], 0);
    }
}

static void motor_task(void *arg)
{
    float angle = 0.0f;
    while (1) {
        if (!s_running) {
            all_off();
            vTaskDelay(pdMS_TO_TICKS(SINE_TASK_MS));
            continue;
        }
        if (!motor_diag_ok()) {
            ESP_LOGE(TAG, "DIAG error");
            s_running = false; all_off(); continue;
        }

        // advance electrical angle at same speed as block mode
        // block: 1 step = 60°, step_ms per step → 60/step_ms °/ms
        float inc = (60.0f / (float)s_step_ms) * (float)SINE_TASK_MS;
        angle += (s_dir == MOTOR_DIR_LEFT) ? -inc : inc;
        if (angle >=  360.0f) angle -= 360.0f;
        if (angle <     0.0f) angle += 360.0f;

        set_phase(0, sinf(DEG2RAD(angle)));
        set_phase(1, sinf(DEG2RAD(angle - 120.0f)));
        set_phase(2, sinf(DEG2RAD(angle + 120.0f)));

        vTaskDelay(pdMS_TO_TICKS(SINE_TASK_MS));
    }
}

#endif  // MOTOR_SINE_COMM

// ═════════════════════════════════════════════════════════════════════════════
//  INIT + PUBLIC API
// ═════════════════════════════════════════════════════════════════════════════

void motor_init(void)
{
    // DIAG input
    gpio_config_t diag = {
        .pin_bit_mask = (1ULL << TMC_DIAG),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&diag);

    // LS GPIO (same for both modes)
    for (int i = 0; i < 3; i++) {
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << LS_PINS[i]),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&cfg);
        gpio_set_level(LS_PINS[i], 0);
    }

    // HS MCPWM – group 0
    mcpwm_timer_config_t t_cfg = {
        .group_id      = 0,
        .clk_src       = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = PWM_RESOLUTION_HZ,
        .period_ticks  = PWM_PERIOD_TICKS,
        .count_mode    = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&t_cfg, &s_timer));

    for (int i = 0; i < 3; i++) {
        mcpwm_operator_config_t o_cfg = { .group_id = 0 };
        ESP_ERROR_CHECK(mcpwm_new_operator(&o_cfg, &s_oper[i]));
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(s_oper[i], s_timer));

        mcpwm_comparator_config_t c_cfg = { .flags.update_cmp_on_tez = true };
        ESP_ERROR_CHECK(mcpwm_new_comparator(s_oper[i], &c_cfg, &s_cmp[i]));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(s_cmp[i], s_duty_ticks));

        mcpwm_generator_config_t g_cfg = { .gen_gpio_num = HS_PINS[i] };
        ESP_ERROR_CHECK(mcpwm_new_generator(s_oper[i], &g_cfg, &s_gen[i]));

        // active-high: empty → HIGH, compare → LOW
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(s_gen[i],
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                          MCPWM_TIMER_EVENT_EMPTY,
                                          MCPWM_GEN_ACTION_HIGH)));
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(s_gen[i],
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                            s_cmp[i],
                                            MCPWM_GEN_ACTION_LOW)));
        hs_off(i);
    }

    ESP_ERROR_CHECK(mcpwm_timer_enable(s_timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_START_NO_STOP));

#ifdef MOTOR_SINE_COMM
    ESP_LOGI(TAG, "TMC6300 SINE commutation, PWM=%dHz", PWM_FREQ_HZ);
#else
    ESP_LOGI(TAG, "TMC6300 BLOCK commutation, PWM=%dHz", PWM_FREQ_HZ);
#endif

    xTaskCreate(motor_task, "motor", 4096, NULL, 6, NULL);
}

void motor_set(motor_dir_t dir, uint32_t step_ms)
{
    s_dir     = dir;
    s_step_ms = step_ms;
    s_running = true;
}

void motor_set_duty(uint8_t duty_percent)
{
    if (duty_percent > 100) duty_percent = 100;
    s_duty_ticks = (uint32_t)PWM_PERIOD_TICKS * duty_percent / 100;
}

void motor_stop(void)
{
    s_running = false;
}

bool motor_diag_ok(void)
{
    return gpio_get_level(TMC_DIAG) == 0;
}
