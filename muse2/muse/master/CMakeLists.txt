#
#
#
#

# MACRO(WRAP_CPP outfiles )
#    FOREACH(it ${ARGN})
#       SET(${outfiles} ${${outfiles}} ${it}.cpp)
#       ENDFOREACH(it)
#    ENDMACRO(WRAP_CPP)
# 
# MACRO(WRAP_H outfiles )
#    FOREACH(it ${ARGN})
#       SET(${outfiles} ${${outfiles}} ${it}.h)
#       ENDFOREACH(it)
#    ENDMACRO(WRAP_H)
# 
# MACRO(WRAP_UI outfiles )
#    FOREACH(it ${ARGN})
#       SET(${outfiles} ${${outfiles}} ${it}.ui)
#       ENDFOREACH(it)
#    ENDMACRO(WRAP_UI)
# 
# set( mocs master masteredit )
# 
# WRAP_H(mocH ${mocs})
# WRAP_CPP(mocCPP ${mocs})
# 
# QT4_WRAP_CPP (moc_headers ${mocH})
# 
# set ( master_src ${mocCPP} ${moc_headers})
# 
# add_library ( master STATIC ${master_src})
# set_target_properties( master
#    PROPERTIES COMPILE_FLAGS "-include ${PROJECT_BINARY_DIR}/all.h"
#    )
# 
# 




QT4_WRAP_CPP ( master_mocs
        lmaster.h 
        masteredit.h 
        master.h 
        tscale.h
      )

# QT4_WRAP_UI ( master_ui_headers
# QT4_WRAP_UI3 ( master_ui_headers
#       )

add_library ( master STATIC
        # ${master_ui_headers}
        ${master_mocs}
        lmaster.cpp # lmaster.h 
        masteredit.cpp # masteredit.h 
        master.cpp # master.h 
        tscale.cpp # tscale.h
      )

set_target_properties( master
      PROPERTIES COMPILE_FLAGS "-include ${PROJECT_BINARY_DIR}/all.h"
      )
