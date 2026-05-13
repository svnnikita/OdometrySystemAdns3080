// sys_arch.c -- файл предоставляет семафоры и почтовые ящики для lwip

#include "sys_arch.h"
#include "lwip/err.h"
#include "lwip/sys.h"  // для lwip_thread_fn
#include <string.h>

// ------------------------- Семафоры -------------------------
// функция создает и возвращает новый семафор
err_t sys_sem_new(sys_sem_t *sem, u8_t count) {
    *sem = xSemaphoreCreateCounting(0xFF, count);
    if (*sem == NULL) return ERR_MEM;
    return ERR_OK;
}

// освобождаем семафор, созданный sys_sem_new()
void sys_sem_free(sys_sem_t *sem) {
    vSemaphoreDelete(*sem);
    *sem = NULL;
}

// сигнализируем или освобождаем семафор
void sys_sem_signal(sys_sem_t *sem) {
    xSemaphoreGive(*sem);
}

// блокируем поток в ожидании семафора
// параметр timeout указывает, сколько мс функция должна
// заблокировать перед возвратом
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) {
    TickType_t ticks;
    if (timeout == 0) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(timeout);
    }

    if (xSemaphoreTake(*sem, ticks) == pdTRUE) {
        return 0;
    }
    return SYS_ARCH_TIMEOUT;
}

// ------------------------- Почтовые ящики -------------------------
// используются для передачи сообщений
// функция возвращает новый почтовый ящик
err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
    if (size < 0) size = 0;
    *mbox = xQueueCreate((UBaseType_t)size, sizeof(void *));
    if (*mbox == NULL) return ERR_MEM;
    return ERR_OK;
}

// освобождаем почтовый ящик
void sys_mbox_free(sys_mbox_t *mbox) {
    vQueueDelete(*mbox);
    *mbox = NULL;
}

// отправляем сообщение в почтовый ящик
void sys_mbox_post(sys_mbox_t *mbox, void *msg) {
    xQueueSend(*mbox, &msg, portMAX_DELAY);
}

// пытаемся отправить сообщение в почтовый ящик путем
// опроса (без тайм-аута)
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg) {
    if (xQueueSend(*mbox, &msg, 0) == pdTRUE) {
        return ERR_OK;
    }
    return ERR_MEM;
}

// блокируем поток до тех пор, пока в почтовый ящик
// не поступит сообщение
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout) {
    TickType_t ticks;
    void *dummy;
    
    if (msg == NULL) msg = &dummy;
    
    if (timeout == 0) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(timeout);
    }
    
    if (xQueueReceive(*mbox, msg, ticks) == pdTRUE) {
        return 0;
    }
    return SYS_ARCH_TIMEOUT;
}

// похоже на trt_fetch, однако, если сообщение отсутствует в почтовом ящике,
// оно немедленно возвращается с кодом SYS_MBOX_EMPTY; в случае успеха
// возвращается 0
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg) {
    void *dummy;
    if (msg == NULL) msg = &dummy;
    
    if (xQueueReceive(*mbox, msg, 0) == pdTRUE) {
        return 0;
    }
    return SYS_ARCH_TIMEOUT;
}

// ------------------------- Мьютексы -------------------------
err_t sys_mutex_new(sys_mutex_t *mutex) {
    *mutex = xSemaphoreCreateMutex();
    if (*mutex == NULL) return ERR_MEM;
    return ERR_OK;
}

void sys_mutex_lock(sys_mutex_t *mutex) {
    xSemaphoreTake(*mutex, portMAX_DELAY);
}

void sys_mutex_unlock(sys_mutex_t *mutex) {
    xSemaphoreGive(*mutex);
}

void sys_mutex_free(sys_mutex_t *mutex) {
    vSemaphoreDelete(*mutex);
    *mutex = NULL;
}

// создаем экземпляр потока для lwip
sys_thread_t sys_thread_new(const char *name, void (*thread)(void *arg), void *arg, int stacksize, int prio)
{
    TaskHandle_t handle = NULL;
    const UBaseType_t uxStackDepth = (UBaseType_t)(stacksize / sizeof(portSTACK_TYPE));
    if (xTaskCreate(thread, name, uxStackDepth, arg, (UBaseType_t)prio, &handle) != pdPASS) {
        return NULL;
    }
    return handle;
}

// ------------------------- Системные таймеры -------------------------
#include "FreeRTOS.h"
#include "task.h"

/* ------------------------- Валидация объектов ------------------------- */
int sys_mbox_valid(sys_mbox_t *mbox) {
    return (*mbox != NULL);
}

void sys_mbox_set_invalid(sys_mbox_t *mbox) {
    *mbox = NULL;
}

int sys_sem_valid(sys_sem_t *sem) {
    return (*sem != NULL);
}

void sys_sem_set_invalid(sys_sem_t *sem) {
    *sem = NULL;
}

/* ------------------------- Инициализация системного уровня ------------------------- */
void sys_init(void) {
    /* Ничего не требуется, всё уже настроено в FreeRTOS */
}

// получаем временную метку
u32_t sys_now(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}