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

QT4_WRAP_CPP (ctrl_mocs
#       configmidictrl.h
#       definemidictrl.h
#       ctrldialog.h
#       ctrledit.h
        
      ctrledit.h 
      ctrlcanvas.h 
      ctrlpanel.h
      )

#QT4_WRAP_UI (ctrl_ui_headers
# QT4_WRAP_UI3 (ctrl_ui_headers
#       configmidictrl.ui
#       definemidictrl.ui
#       ctrldialog.ui
#       )

add_library ( ctrl STATIC
#       configmidictrl.cpp
#       definemidictrl.cpp
#       ctrldialog.cpp
#       ctrleditor.cpp
#       ctrledit.cpp
      
      ctrledit.cpp ctrledit.h 
      ctrlcanvas.cpp ctrlcanvas.h 
      ctrlpanel.cpp ctrlpanel.h
      ${ctrl_mocs}
      # ${ctrl_ui_headers}
      )
set_target_properties( ctrl
  PROPERTIES COMPILE_FLAGS "-include ${PROJECT_BINARY_DIR}/all.h"
  )
