#ifndef __NVM_MAP_H
#define __NVM_MAP_H

#include "W25Q64.h"

#define NVM_PARAM_A_BASE       0x000000UL
#define NVM_PARAM_B_BASE       0x001000UL

#define NVM_DTC_BASE           0x002000UL
#define NVM_DTC_SECTOR_COUNT   4UL
#define NVM_DTC_END            \
    (NVM_DTC_BASE + NVM_DTC_SECTOR_COUNT * W25Q64_SECTOR_SIZE)

#define NVM_PARAM_SLOT_SIZE    W25Q64_SECTOR_SIZE

_Static_assert(
    (NVM_PARAM_A_BASE % W25Q64_SECTOR_SIZE) == 0UL,
    "Parameter A is not sector aligned");

_Static_assert(
    (NVM_PARAM_B_BASE % W25Q64_SECTOR_SIZE) == 0UL,
    "Parameter B is not sector aligned");

_Static_assert(
    (NVM_DTC_BASE % W25Q64_SECTOR_SIZE) == 0UL,
    "DTC region is not sector aligned");

_Static_assert(
    NVM_PARAM_A_BASE + NVM_PARAM_SLOT_SIZE <= NVM_PARAM_B_BASE,
    "Parameter A/B overlap");

_Static_assert(
    NVM_PARAM_B_BASE + NVM_PARAM_SLOT_SIZE <= NVM_DTC_BASE,
    "Parameter and DTC regions overlap");

_Static_assert(
    NVM_DTC_END <= W25Q64_TOTAL_SIZE,
    "DTC region exceeds W25Q64");

#endif
