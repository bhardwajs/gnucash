
set(chartjs-4_DATA
  chart.umd.js
  chart.js
)

file(GLOB_RECURSE chartjs-4_NOINST_DATA
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    samples Chart*)

list(REMOVE_ITEM chartjs-4_NOINST_DATA ${chartjs-4_DATA})

install (FILES ${chartjs-4_DATA} DESTINATION  ${CMAKE_INSTALL_DATADIR}/gnucash/chartjs-4)

set_dist_list(chartjs-4_DIST CMakeLists.txt ${chartjs-4_DATA} ${chartjs-4_NOINST_DATA})
