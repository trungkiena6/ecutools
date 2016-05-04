/**
 * ecutools: IoT Automotive Tuning, Diagnostics & Analytics
 * Copyright (C) 2014  Jeremy Hahn
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CANBUSLOGGERINTERFACE_H
#define CANBUSLOGGERINTERFACE_H

#include <stdint.h>
#include <pthread.h>
#include "canbus.h"

typedef struct _logger {
  uint8_t canbus_flags;
  pthread_t canbus_thread;
  struct canbus_filter *filters[10];
  canbus_client *canbus;
} logger;

#endif