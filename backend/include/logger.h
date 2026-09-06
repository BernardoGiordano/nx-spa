/*
 *   This file is part of nx-spa
 *   Copyright (C) 2026 Bernardo Giordano
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

/**
 * Connects stderr to the nxlink host when the NRO was launched by nxlink.
 * Normal SD-card launches remain network-free.
 */
void loggerInit();
void loggerExit();

/// Whether an nxlink host accepted the diagnostics connection.
bool loggerIsActive();

void loggerWrite(const char* level, const char* format, ...)
    __attribute__((format(printf, 2, 3)));

#define LOG_DEBUG(...) loggerWrite("DEBUG", __VA_ARGS__)
#define LOG_INFO(...) loggerWrite("INFO", __VA_ARGS__)
#define LOG_WARN(...) loggerWrite("WARN", __VA_ARGS__)
#define LOG_ERROR(...) loggerWrite("ERROR", __VA_ARGS__)
