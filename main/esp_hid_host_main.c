#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#else
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#endif

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#define ESP_BD_ADDR_STR         "%02x:%02x:%02x:%02x:%02x:%02x"
#define ESP_BD_ADDR_HEX(addr)   addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]
#else
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#endif

#include "esp_hidh.h"
#include "esp_hid_gap.h"

// ==========================================================================
// Braille:

typedef struct {
    bool dots[6];
} BrailleCell;

const BrailleCell braille_keymap[128] = {
    ['a'] = { .dots = {true, false, false, false, false, false} }, // a
    ['b'] = { .dots = {true, true, false, false, false, false} },  // b
    ['c'] = { .dots = {true, false, false, true, false, false} },  // c
    ['d'] = { .dots = {true, false, false, true, true, false} },   // d
    ['e'] = { .dots = {true, false, false, false, true, false} },  // e
    ['f'] = { .dots = {true, true, false, true, false, false} },   // f
    ['g'] = { .dots = {true, true, false, true, true, false} },    // g
    ['h'] = { .dots = {true, true, false, false, true, false} },   // h
    ['i'] = { .dots = {false, true, false, true, false, false} },  // i
    ['j'] = { .dots = {false, true, false, true, true, false} },   // j
    ['k'] = { .dots = {true, false, true, false, false, false} },  // k
    ['l'] = { .dots = {true, true, true, false, false, false} },   // l
    ['m'] = { .dots = {true, false, true, true, false, false} },   // m
    ['n'] = { .dots = {true, false, true, true, true, false} },    // n
    ['o'] = { .dots = {true, false, true, false, true, false} },   // o
    ['p'] = { .dots = {true, true, true, true, false, false} },    // p
    ['q'] = { .dots = {true, true, true, true, true, false} },     // q
    ['r'] = { .dots = {true, true, true, false, true, false} },    // r
    ['s'] = { .dots = {false, true, true, true, false, false} },   // s
    ['t'] = { .dots = {false, true, true, true, true, false} },    // t
    ['u'] = { .dots = {true, false, true, false, false, true} },   // u
    ['v'] = { .dots = {true, true, true, false, false, true} },    // v
    ['w'] = { .dots = {false, true, false, true, true, true} },    // w
    ['x'] = { .dots = {true, false, true, true, false, true} },    // x
    ['y'] = { .dots = {true, false, true, true, true, true} },     // y
    ['z'] = { .dots = {true, false, true, false, true, true} },    // z
    ['1'] = { .dots = {true, false, false, false, false, false} }, // 1
    ['2'] = { .dots = {true, true, false, false, false, false} },  // 2
    ['3'] = { .dots = {true, false, false, true, false, false} },  // 3
    ['4'] = { .dots = {true, false, false, true, true, false} },   // 4
    ['5'] = { .dots = {true, false, false, false, true, false} },  // 5
    ['6'] = { .dots = {true, true, false, true, false, false} },   // 6
    ['7'] = { .dots = {true, true, false, true, true, false} },    // 7
    ['8'] = { .dots = {true, true, false, false, true, false} },   // 8
    ['9'] = { .dots = {false, true, false, true, false, false} },  // 9
    ['0'] = { .dots = {false, true, false, true, true, false} },   // 0
};

const char keymap[128] = {
    [0x04] = 'a', [0x05] = 'b', [0x06] = 'c', [0x07] = 'd',
    [0x08] = 'e', [0x09] = 'f', [0x0A] = 'g', [0x0B] = 'h',
    [0x0C] = 'i', [0x0D] = 'j', [0x0E] = 'k', [0x0F] = 'l',
    [0x10] = 'm', [0x11] = 'n', [0x12] = 'o', [0x13] = 'p',
    [0x14] = 'q', [0x15] = 'r', [0x16] = 's', [0x17] = 't',
    [0x18] = 'u', [0x19] = 'v', [0x1A] = 'w', [0x1B] = 'x',
    [0x1C] = 'y', [0x1D] = 'z', [0x1E] = '1', [0x1F] = '2',
    [0x20] = '3', [0x21] = '4', [0x22] = '5', [0x23] = '6',
    [0x24] = '7', [0x25] = '8', [0x26] = '9', [0x27] = '0',
    [0x2C] = ' ', [0x28] = '\n',[0x2A] = '\b'               // Espaço, Enter, Backspace
};

char text[100] = "";
int current_index = 0; // Índice da letra atual
char textToBraille[100] = "";



// ==========================================================================
// Botões:
#include <driver/gpio.h>
#define BUTTON_ADVANCE_GPIO 32
#define BUTTON_RETURN_GPIO 33

void IRAM_ATTR button_advance_isr_handler(void* arg) {
    if (current_index < strlen(textToBraille)) {
        current_index++;
    }
}

void IRAM_ATTR button_return_isr_handler(void* arg) {
    if (current_index > 0) {
        current_index--;
    }
}

void configure_buttons() {
    gpio_config_t io_conf;
    // Configurar botão de avançar
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << BUTTON_ADVANCE_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_ADVANCE_GPIO, button_advance_isr_handler, (void*)BUTTON_ADVANCE_GPIO);

    // Configurar botão de retornar
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << BUTTON_RETURN_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_isr_handler_add(BUTTON_RETURN_GPIO, button_return_isr_handler, (void*)BUTTON_RETURN_GPIO);
}


//===============================================================================
// Micro Servo:

#include "driver/ledc.h"
#include "esp_err.h"

#define SERVO_PIN1 2
#define SERVO_PIN2 0
#define SERVO_PIN3 4
#define SERVO_PIN4 12
#define SERVO_PIN5 14
#define SERVO_PIN6 27

#define SERVO_MIN_PULSEWIDTH 500  // Em microsegundos
#define SERVO_MAX_PULSEWIDTH 2500 // Em microsegundos
#define SERVO_MAX_DEGREE 180      // Em graus

int servo_positions[6] = {0, 0, 0, 0, 0, 0}; // Posições iniciais dos servos

static void ledc_init() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_15_BIT,
        .freq_hz          = 50,  // Frequência de PWM típica para servos
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel[6] = {
        { .channel    = LEDC_CHANNEL_0,
          .duty       = 0,
          .gpio_num   = SERVO_PIN1,
          .speed_mode = LEDC_HIGH_SPEED_MODE,
          .hpoint     = 0,
          .timer_sel  = LEDC_TIMER_0 },
        { .channel    = LEDC_CHANNEL_1,
          .duty       = 0,
          .gpio_num   = SERVO_PIN2,
          .speed_mode = LEDC_HIGH_SPEED_MODE,
          .hpoint     = 0,
          .timer_sel  = LEDC_TIMER_0 },
        { .channel    = LEDC_CHANNEL_2,
          .duty       = 0,
          .gpio_num   = SERVO_PIN3,
          .speed_mode = LEDC_HIGH_SPEED_MODE,
          .hpoint     = 0,
          .timer_sel  = LEDC_TIMER_0 },
        { .channel    = LEDC_CHANNEL_3,
          .duty       = 0,
          .gpio_num   = SERVO_PIN4,
          .speed_mode = LEDC_HIGH_SPEED_MODE,
          .hpoint     = 0,
          .timer_sel  = LEDC_TIMER_0 },
        { .channel    = LEDC_CHANNEL_4,
          .duty       = 0,
          .gpio_num   = SERVO_PIN5,
          .speed_mode = LEDC_HIGH_SPEED_MODE,
          .hpoint     = 0,
          .timer_sel  = LEDC_TIMER_0 },
        { .channel    = LEDC_CHANNEL_5,
          .duty       = 0,
          .gpio_num   = SERVO_PIN6,
          .speed_mode = LEDC_HIGH_SPEED_MODE,
          .hpoint     = 0,
          .timer_sel  = LEDC_TIMER_0 }
    };

    for (int ch = 0; ch < 6; ch++) {
        ledc_channel_config(&ledc_channel[ch]);
    }
}

// Baixa Velocidade um servo por vez:
static uint32_t servo_degree_to_duty(int degree) {
    return (SERVO_MIN_PULSEWIDTH + ((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * degree) / SERVO_MAX_DEGREE) * (1 << 15) / 20000;
}

void gradual_move(ledc_channel_t channel, int servo_index, int start_degree, int end_degree) {
    if (start_degree < end_degree) {
        for (int degree = start_degree; degree <= end_degree; degree++) {
            uint32_t duty = servo_degree_to_duty(degree);
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
            vTaskDelay(pdMS_TO_TICKS(15));
        }
    } else {
        for (int degree = start_degree; degree >= end_degree; degree--) {
            uint32_t duty = servo_degree_to_duty(degree);
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
            vTaskDelay(pdMS_TO_TICKS(15));
        }
    }
    servo_positions[servo_index] = end_degree;  // Atualiza a posição atual do servo
}

// Alta velocidade:
void set_servos(BrailleCell cell) {
    ledc_channel_t channels[6] = { LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3, LEDC_CHANNEL_4, LEDC_CHANNEL_5 };
    uint32_t duties[6];
    
    // Calcular os duties para todos os servos
    for (int i = 0; i < 6; i++) {
        int target_degree = cell.dots[i] ? 180 : 0;
        duties[i] = servo_degree_to_duty(target_degree);
    }

    // Configurar os duties para todos os canais
    for (int i = 0; i < 6; i++) {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, channels[i], duties[i]);
    }

    // Aplicar os duties configurados
    for (int i = 0; i < 6; i++) {
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, channels[i]);
    }

    // Atualizar as posições dos servos
    for (int i = 0; i < 6; i++) {
        servo_positions[i] = cell.dots[i] ? 180 : 0;
    }
}


// Baixa Velocidade Todos ao mesmo tempo:
void move_servos(BrailleCell cell) {
    ledc_channel_t channels[6] = { LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3, LEDC_CHANNEL_4, LEDC_CHANNEL_5 };
    int target_degrees[6];
    int start_degrees[6];
    bool needs_update = false;

    // Determine target degrees for each servo
    for (int i = 0; i < 6; i++) {
        target_degrees[i] = cell.dots[i] ? 180 : 0;
        start_degrees[i] = servo_positions[i];
        if (start_degrees[i] != target_degrees[i]) {
            needs_update = true;
        }
    }

    if (!needs_update) {
        return; // No need to move if all positions are already correct
    }

    // Move all servos simultaneously
    int steps = 20; // Number of steps for the gradual movement
    for (int step = 0; step <= steps; step++) {
        for (int i = 0; i < 6; i++) {
            int degree = start_degrees[i] + (target_degrees[i] - start_degrees[i]) * step / steps;
            uint32_t duty = servo_degree_to_duty(degree);
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, channels[i], duty);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, channels[i]);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Adjust delay for smoother or faster movement
    }

    // Update servo positions
    for (int i = 0; i < 6; i++) {
        servo_positions[i] = target_degrees[i];
    }
}


// ========================================================================================
// Keyboard:

static const char *TAG = "ESP_HIDH_DEMO";

void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDH_OPEN_EVENT: {
        if (param->open.status == ESP_OK) {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
            ESP_LOGI(TAG, ESP_BD_ADDR_STR " OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
            esp_hidh_dev_dump(param->open.dev, stdout);
        } else {
            ESP_LOGE(TAG, " OPEN failed!");
        }
        break;
    }
    case ESP_HIDH_BATTERY_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " BATTERY: %d%%", ESP_BD_ADDR_HEX(bda), param->battery.level);
        break;
    }
 case ESP_HIDH_INPUT_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:", ESP_BD_ADDR_HEX(bda), esp_hid_usage_str(param->input.usage), param->input.map_index, param->input.report_id, param->input.length);
        ESP_LOG_BUFFER_HEX(TAG, param->input.data, param->input.length);

        char caracter;
        for (int i = 2; i < param->input.length; i++) { // Ignorando os primeiros dois bytes (modificadores)
            uint8_t key_code = param->input.data[i];

            if (key_code > 0 && key_code < sizeof(keymap) && keymap[key_code] != 0) {
                caracter = keymap[key_code];
                ESP_LOGI(TAG, "Char: %c", caracter);

                if (caracter == '\n') { // Enter

                    strcpy(textToBraille, text);

                    text[0] = '\0'; // Limpar o texto
                    current_index = 0; // Resetar o índice para o início
                    ESP_LOGI(TAG, "Texto: %s", textToBraille);

                } else if (caracter == '\b') { // Delete
                    if (strlen(text) > 0) {
                        text[strlen(text) - 1] = '\0';
                    }
                    
                } else { // Adicionar caractere ao texto
                    int len = strlen(text);
                    if (len < sizeof(text) - 1) {
                        text[len] = caracter;
                        text[len + 1] = '\0';
                    }
                }
            }
        }
        break;
    }
    case ESP_HIDH_FEATURE_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->feature.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " FEATURE: %8s, MAP: %2u, ID: %3u, Len: %d", ESP_BD_ADDR_HEX(bda),
                 esp_hid_usage_str(param->feature.usage), param->feature.map_index, param->feature.report_id,
                 param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDH_CLOSE_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
        break;
    }
    default:
        ESP_LOGI(TAG, "EVENT: %d", event);
        break;
    }
}

#define SCAN_DURATION_SECONDS 5

void hid_demo_task(void *pvParameters){
    size_t results_len = 0;
    esp_hid_scan_result_t *results = NULL;

    ESP_LOGI(TAG, "SCAN...");
    //start scan for HID devices
    while(results_len == 0){
        esp_hid_scan(SCAN_DURATION_SECONDS, &results_len, &results);
        ESP_LOGI(TAG, "SCAN: %u results", results_len);
    }

    if (results_len) {
        esp_hid_scan_result_t *r = results;
        esp_hid_scan_result_t *cr = NULL;
        while (r) {
            printf("  %s: " ESP_BD_ADDR_STR ", ", (r->transport == ESP_HID_TRANSPORT_BLE) ? "BLE" : "BT ", ESP_BD_ADDR_HEX(r->bda));
            printf("RSSI: %d, ", r->rssi);
            printf("USAGE: %s, ", esp_hid_usage_str(r->usage));
#if CONFIG_BT_BLE_ENABLED
            if (r->transport == ESP_HID_TRANSPORT_BLE) {
                cr = r;
                printf("APPEARANCE: 0x%04x, ", r->ble.appearance);
                printf("ADDR_TYPE: '%s', ", ble_addr_type_str(r->ble.addr_type));
            }
#endif /* CONFIG_BT_BLE_ENABLED */
#if CONFIG_BT_NIMBLE_ENABLED
            if (r->transport == ESP_HID_TRANSPORT_BLE) {
                cr = r;
                printf("APPEARANCE: 0x%04x, ", r->ble.appearance);
                printf("ADDR_TYPE: '%d', ", r->ble.addr_type);
            }
#endif /* CONFIG_BT_BLE_ENABLED */
#if CONFIG_BT_HID_HOST_ENABLED
            if (r->transport == ESP_HID_TRANSPORT_BT) {
                cr = r;
                printf("COD: %s[", esp_hid_cod_major_str(r->bt.cod.major));
                esp_hid_cod_minor_print(r->bt.cod.minor, stdout);
                printf("] srv 0x%03x, ", r->bt.cod.service);
                print_uuid(&r->bt.uuid);
                printf(", ");
            }
#endif /* CONFIG_BT_HID_HOST_ENABLED */
            printf("NAME: %s ", r->name ? r->name : "");
            printf("\n");
            r = r->next;
        }
        if (cr) {
            //open the last result
            esp_hidh_dev_open(cr->bda, cr->transport, cr->ble.addr_type);
        }
        //free the results
        esp_hid_scan_results_free(results);
    }
    vTaskDelete(NULL);
}

#if CONFIG_BT_NIMBLE_ENABLED
void ble_hid_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}
void ble_store_config_init(void);
#endif

// ==========================================================================
// Main:

void display_task(void *pvParameter) {
    while (1) {
        if (strlen(textToBraille) > 0) {
            char c = textToBraille[current_index];
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {


                move_servos(braille_keymap[(int)c]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Atualiza a cada 100 ms
    }
}

void app_main(void){

    esp_err_t ret;
#if HID_HOST_MODE == HIDH_IDLE_MODE
    ESP_LOGE(TAG, "Please turn on BT HID host or BLE!");
    return;
#endif

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_HOST_MODE);
    ESP_ERROR_CHECK(esp_hid_gap_init(HID_HOST_MODE));

#if CONFIG_BT_BLE_ENABLED
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler));
#endif /* CONFIG_BT_BLE_ENABLED */

    esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(esp_hidh_init(&config));

#if CONFIG_BT_NIMBLE_ENABLED
    /* XXX Need to have template for store */
    ble_store_config_init();

    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    /* Starting nimble task after gatts is initialized */
    ret = esp_nimble_enable(ble_hid_host_task);
    if (ret) {
        ESP_LOGE(TAG, "esp_nimble_enable failed: %d", ret);
    }
#endif

    ledc_init(); //Configura Servos
    configure_buttons(); //Configura Botões
    xTaskCreate(&hid_demo_task, "hid_task", 6 * 1024, NULL, 2, NULL); // Inicia a tarefa de HID
    xTaskCreate(&display_task, "display_task", 2048, NULL, 5, NULL);  // Inicia a tarefa de exibição
}