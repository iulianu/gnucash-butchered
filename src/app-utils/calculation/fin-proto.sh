#! /bin/sh
#/***************************************************************************
#                          fin-proto.sh  -  description
#                             -------------------
#    copyright            : (C) 2000 by Terry D. Boldt
#    email                : tboldt@attglobal.net
#    Author               : Terry D. Boldt
# ***************************************************************************/
#
#/***************************************************************************
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU General Public License as published by  *
# *   the Free Software Foundation; either version 2 of the License, or     *
# *   (at your option) any later version.                                   *
# *                                                                         *
# ***************************************************************************/
#
#	shell script for creating function prototype files
#
qtgrep -DHhf cfuncs.exp fin.cpp expression_parser.cpp numeric_ops.cpp amort_opt.cpp amort_prt.cpp >protos.out
echo Creating Global Prototype File: \"finproto.h\"
qtawk -f        protos.exp protos.out >finproto.h
echo Creating Static Prototype File: \"fin_static_proto.h\"
qtawk -f static_protos.exp protos.out >fin_static_proto.h
rm -fv protos.out
