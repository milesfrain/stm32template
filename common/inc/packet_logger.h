/*
 * Helper functions involving logger and packets
 *
 * It is recommended to call the wrapped version
 * of TaskUtilities.logPacket() wherever possible.
 * These "Base" versions are for situations where
 * TasksUtilities is unavailable.
 */

#pragma once

#include "itm_logger_task.h"
#include "packets.h"

int logPacketBase(const char* callerName, const char* note, const Packet& packet, ItmLogger* logger, LogMsg& msg);
int logPacketBase(const char* callerName, const char* note, const Packet& packet, ItmLogger& logger, LogMsg& msg);
