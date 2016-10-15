##
## This file is part of the libopencm3 project.
##
## Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
##
## This library is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this library.  If not, see <http://www.gnu.org/licenses/>.
##

BINARY   := agg1
SOURCES  := ILI9341.cpp aa_test.cpp glcdfont.c systick.c	\
            agg_trans_affine.cpp				\
            agg_vcgen_dash.cpp					\
            agg_vcgen_stroke.cpp				\
            agg_platform_support.cpp

OBJS     := $(patsubst %.cpp, %.o, $(SOURCES:%.c=%.o))
LDLIBS   := -lm

LDSCRIPT := stm32f4-1bitsy.ld

include ../Makefile.include
CPPFLAGS += -I.
CXXFLAGS += -O0 \
	    -Wno-effc++ -Wno-missing-field-initializers -Wno-unused-parameter
LD       := $(CXX)
