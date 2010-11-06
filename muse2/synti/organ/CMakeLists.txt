#=============================================================================
#  MusE
#  Linux Music Editor
#  $Id:$
#
#  Copyright (C) 2002-2006 by Werner Schweer and others
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#=============================================================================

QT4_WRAP_CPP ( organ_mocs organgui.h )
# QT4_WRAP_UI (  organ_uis organgui.ui )
QT4_WRAP_UI3 (  organ_uis organguibase.ui )

add_library ( organ SHARED
      organ.cpp
      organgui.cpp
      # reverb.cpp
      # routing.cpp
      # organgui.h
      ${PROJECT_SOURCE_DIR}/muse/xml.cpp

      ${organ_mocs}
      ${organ_uis}
      )

# - tell cmake to name target organ.so instead of
#   liborgan.so
# - use precompiled header files
#
set_target_properties ( organ
   PROPERTIES PREFIX ""
   COMPILE_FLAGS "-fvisibility=hidden -O2 -include ${PROJECT_BINARY_DIR}/all-pic.h"
   )

target_link_libraries(organ
      synti
      # awl
      ${QT_LIBRARIES}
      )

install_targets ( /${CMAKE_INSTALL_LIBDIR}/${MusE_INSTALL_NAME}/synthi/ organ )
