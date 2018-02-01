/*
 arm.h
 
 http://code.google.com/p/armstorm/
 distorm at gmail dot com
 Copyright (C) 2012 Gil Dabah
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

//coto: structs are extern'd, so program can be compiled from C to C++, and structs can be accessed backwards.

#ifndef armstorm_arm_h
#define armstorm_arm_h

struct ARMInstInfo{
    unsigned short mnemonicId;
    unsigned char flags;
    unsigned char op1, op2, op3;
};

extern struct ARMInstInfo arminst;
#include "armstorm.h"

enum DecodeResult decompose_arm(struct DecomposeInfo* info);

#endif /* armstorm_arm_h */