// cc.h -- файл содержит настройки, необходимые для адаптации
// lwip к комплилятору и машинной архитектуре

#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>

// необходимо определить следующие параметры:
// типы данных
typedef uint8_t   u8_t;         // беззнаковый
typedef int8_t    s8_t;         // аналог со знаком
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uintptr_t mem_ptr_t;    // общий тип указателя

// отладочные макросы для диагностики и вывода данных отладки
#define LWIP_PLATFORM_DIAG(x)   do { } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { } while(0)

// Критические секции через FreeRTOS
// В некоторых реализациях они могут обеспечить более простой механизм защиты, 
// чем использование семафоров. В противном случае для реализации могут 
// использоваться семафоры
#include "FreeRTOS.h"
#include "task.h"
// объявление переменной lev для сохранения защиты
// используется для того, чтобы можно было указать,
// какой тип данных используется для защиты
#define SYS_ARCH_DECL_PROTECT(lev)   UBaseType_t lev
// запускает блок защиты
#define SYS_ARCH_PROTECT(lev)        lev = taskENTER_CRITICAL_FROM_ISR()
// завершает блок защиты
#define SYS_ARCH_UNPROTECT(lev)      taskEXIT_CRITICAL_FROM_ISR(lev)

#endif /* LWIP_ARCH_CC_H */