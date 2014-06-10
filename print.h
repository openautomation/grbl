/*
  print.h - Functions for formatting output strings
  Part of Grbl

  Copyright (c) 2011-2014 Sungeun K. Jeon
  Copyright (c) 2009-2011 Simen Svale Skogsrud

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

/* This code was initially inspired by the wiring_serial module by David A. Mellis which
   used to be a part of the Arduino project. */ 

#ifndef print_h
#define print_h

void printString(const char *s);

void printPgmString(const char *s);

void printInteger(long n);

void print_unsigned_base10(uint8_t n);

void printFloat(float n);

void print_base2(unsigned long n, uint8_t size);
#define print_base2(n) print_base2((n), sizeof(n)*8)

#endif
