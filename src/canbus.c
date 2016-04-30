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

#include "canbus.h"

void canbus_print_frame(struct can_frame *frame) {
  int i;
  printf("%04x: ", frame->can_id);
  if(frame->can_id & CAN_RTR_FLAG) {
    printf("remote request");
  }
  else {
    printf("[%d]", frame->can_dlc);
    for(i = 0; i < frame->can_dlc; i++)
      printf(" %02x", frame->data[i]);
  }
  printf("\n");
}

void canbus_framecpy(struct can_frame * frame, char *buf) {
  int i;
  sprintf(buf, "%04x: ", frame->can_id);
  if(frame->can_id & CAN_RTR_FLAG) {
    printf("remote request");
  }
  else {
    sprintf(buf + strlen(buf), "[%d]", frame->can_dlc);
    for(i = 0; i < frame->can_dlc; i++)
      sprintf(buf + strlen(buf), " %02x", frame->data[i]);
  }
}

int canbus_framecmp(struct can_frame *frame1, struct can_frame *frame2) {
  if(frame1->can_id != frame2->can_id) return -1;
  return strcmp((const char *)frame1->data, (const char *)frame2->data) == 0;
}

int canbus_connect(canbus_client *canbus) {

  if(canbus->socket > 0) {
    syslog(LOG_CRIT, "canbus_connect: already connected to CAN");
	return -1;
  }

  if(pthread_mutex_init(&canbus->lock, NULL) != 0) {
    return -1;
  }
  if(pthread_mutex_init(&canbus->rwlock, NULL) != 0) {
    syslog(LOG_ERR, "canbus_connect: unable to initialize canbus read/write mutex: %s", strerror(errno));
    return -1;
  }

  pthread_mutex_lock(&canbus->lock);
  canbus->state = CANBUS_STATE_CONNECTING;
  pthread_mutex_unlock(&canbus->lock);

  int recv_own_msgs = CANBUS_FLAG_RECV_OWN_MSGS;
  struct sockaddr_can addr;
  struct ifreq ifr;
  can_err_mask_t err_mask = CAN_ERR_MASK;

  if((canbus->socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) == -1) {
	syslog(LOG_CRIT, "canbus_connect: error opening socket");
	return -1;
  }

  strcpy(ifr.ifr_name, CAN_IFACE);
  if(ioctl(canbus->socket, SIOCGIFINDEX, &ifr) < 0) {
	syslog(LOG_CRIT, "canbus_connect: unable to find CAN interface %s", CAN_IFACE);
	exit(1);
  }

  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;

  if(bind(canbus->socket, (struct sockaddr *)&addr, sizeof(addr)) < -1) {
    printf("canbus_connect: error in socket bind");
    return -1;
  }

  if(setsockopt(canbus->socket, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask)) == -1) {
    syslog(LOG_ERR, "canbus_connect: unable to set CAN_RAW_ERR_FILTER socket option: %s", strerror(errno));
    return -1;
  }

  if(setsockopt(canbus->socket, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &recv_own_msgs, sizeof(recv_own_msgs)) == -1) {
    syslog(LOG_ERR, "canbus_connect: unable to set CAN_RAW_RECV_OWN_MSGS socket option: %s", strerror(errno));
    return -1;
  }

  syslog(LOG_DEBUG, "canbus_connect: %s socket descriptor: %i", ifr.ifr_name, canbus->socket);

  pthread_mutex_lock(&canbus->lock);
  canbus->state = CANBUS_STATE_CONNECTED;
  pthread_mutex_unlock(&canbus->lock);

  return 0;
}

bool canbus_isconnected(canbus_client *canbus) {
  return canbus->socket > 0 && (canbus->state & CANBUS_STATE_CONNECTED);
}

ssize_t canbus_read(canbus_client *canbus, struct can_frame *frame) {
  if((canbus->state & CANBUS_STATE_CONNECTED) == 0) {
    syslog(LOG_ERR, "canbus_read: CAN socket not connected");
    return -1;
  }

  int nbytes = read(canbus->socket, frame, sizeof(struct can_frame));
  if(nbytes < 0) {
    syslog(LOG_CRIT, "canbus_read: %s", strerror(errno));
    return -1;
  }

  if(nbytes < sizeof(struct can_frame)) {
    syslog(LOG_CRIT, "canbus_read: received incomplete CAN frame");
    return -1;
  }

  syslog(LOG_DEBUG, "canbus_read: read %i byte CAN frame", nbytes);

  return nbytes;
}

int canbus_write(canbus_client *canbus, struct can_frame *frame) {
  if((canbus->state & CANBUS_STATE_CONNECTED) == 0) {
    syslog(LOG_ERR, "canbus_write: CAN socket not connected");
    return -1;
  }

  pthread_mutex_lock(&canbus->rwlock);

  int bytes = write(canbus->socket, frame, sizeof(struct can_frame));
  if(bytes == -1) {
    syslog(LOG_ERR, "canbus_write: %s", strerror(errno));
  }

  pthread_mutex_unlock(&canbus->rwlock);

  syslog(LOG_DEBUG, "canbus_write: wrote %d byte CAN frame", bytes);

  return bytes;
}

void canbus_close(canbus_client *canbus) {

  pthread_mutex_lock(&canbus->lock);
  canbus->state = CANBUS_STATE_CLOSING;
  pthread_mutex_unlock(&canbus->lock);

  if(canbus->socket > 0) {
    if(close(canbus->socket) == -1) {
      syslog(LOG_ERR, "canbus_close: error closing socket: %s", strerror(errno));
    }
    canbus->socket = 0;
  }

  syslog(LOG_DEBUG, "canbus_close: connection closed");

  pthread_mutex_lock(&canbus->lock);
  canbus->state = CANBUS_STATE_CLOSED;
  pthread_mutex_unlock(&canbus->lock);

}