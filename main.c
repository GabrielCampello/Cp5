#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_random.h"

typedef struct {
    int id;
    int valor;
} dado_t;


QueueHandle_t fila = NULL;
EventGroupHandle_t event_supervisor = NULL;

#define BIT_TASK1_OK (1 << 0)
#define BIT_TASK2_OK (1 << 1)

void Task1(void *pv)
{
    int seq = 1;
    for(;;)
    {
        dado_t *dados = (dado_t*) malloc(sizeof(dado_t));
        if (dados == NULL) {
            printf("{Gabriel Campello Duwe RM87260} [ERRO] Falha na alocação de memória!\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        dados->id = seq++;
        dados->valor = (esp_random() % 15) + 1;

        if (xQueueSend(fila, &dados, 0) != pdTRUE) {
            printf("{Gabriel Campello Duwe RM87260} [FILA] Fila cheia! Valor %d descartado\n", dados->valor);
            free(dados);
        } else {
            xEventGroupSetBits(event_supervisor, BIT_TASK1_OK);
            printf("{Gabriel Campello Duwe RM87260} Valor %d de ID %d enviado com sucesso\n", dados->valor, dados->id);
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void Task2(void *pv)
{
    dado_t *dados_recebidos = NULL;
    int timeout = 0;

    for(;;)
    {
        if (xQueueReceive(fila, &dados_recebidos, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            printf("{Gabriel Campello Duwe RM87260} Valor %d de ID %d recebido com sucesso\n",
                   dados_recebidos->valor, dados_recebidos->id);

            xEventGroupSetBits(event_supervisor, BIT_TASK2_OK);
            timeout = 0;

            if (dados_recebidos != NULL) {
                free(dados_recebidos);
                dados_recebidos = NULL;
            }

            esp_task_wdt_reset();
        }
        else
        {
            timeout++;
            printf("{Gabriel Campello Duwe RM87260} [FILA] Nenhum dado recebido (%d tentativas)\n", timeout);

            if (timeout == 3)
                printf("{Gabriel Campello Duwe RM87260} [ALERTA] Task2 com falhas na recepção!\n");

            if (timeout >= 5)
            {
                printf("{Gabriel Campello Duwe RM87260} [RECUPERAÇÃO] Tentando reinicializar Task2...\n");
                timeout = 0;
            }
        }
    }
}

void Task3(void *pv)
{
    for(;;)
    {
        EventBits_t bits = xEventGroupWaitBits(
            event_supervisor,
            BIT_TASK1_OK | BIT_TASK2_OK,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(2000)
        );

        if ((bits & BIT_TASK1_OK) && (bits & BIT_TASK2_OK)) {
            printf("{Gabriel Campello Duwe RM87260} Sistema OK (Task1 e Task2 ativas)\n");
        }
        else if (bits & BIT_TASK1_OK) {
            printf("{Gabriel Campello Duwe RM87260} Sistema parcialmente OK (apenas Task1)\n");
        }
        else if (bits & BIT_TASK2_OK) {
            printf("{Gabriel Campello Duwe RM87260} Sistema parcialmente OK (apenas Task2)\n");
        }
        else {
            printf("{Gabriel Campello Duwe RM87260} [FALHA] Nenhuma task sinalizou!\n");
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    printf("{Gabriel Campello Duwe RM87260} Iniciando Sistema Robusto CP2...\n");

    esp_task_wdt_config_t wdt = {
        .timeout_ms = 5000,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt);

    fila = xQueueCreate(1, sizeof(dado_t *));
    event_supervisor = xEventGroupCreate();

    if (fila == NULL || event_supervisor == NULL) {
        printf("{Gabriel Campello Duwe RM87260} [ERRO] Falha na criação dos recursos!\n");
        esp_restart();
    }

    TaskHandle_t hTask1, hTask2, hTask3;
    xTaskCreate(Task1, "Task1", 4096, NULL, 5, &hTask1);
    xTaskCreate(Task2, "Task2", 4096, NULL, 5, &hTask2);
    xTaskCreate(Task3, "Task3", 4096, NULL, 5, &hTask3);

    esp_task_wdt_add(hTask1);
    esp_task_wdt_add(hTask2);
    esp_task_wdt_add(hTask3);
}
