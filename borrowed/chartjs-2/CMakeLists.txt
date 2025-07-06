
set(chartjs-2_DATA
  Chart.bundle.min.js
  Chart.bundle.js
)

file(GLOB_RECURSE chartjs-2_NOINST_DATA
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    samples Chart*)

list(REMOVE_ITEM chartjs-2_NOINST_DATA ${chartjs-2_DATA})

install (FILES ${chartjs-2_DATA} DESTINATION  ${CMAKE_INSTALL_DATADIR}/gnucash/chartjs-2)

set_dist_list(chartjs-2_DIST CMakeLists.txt ${chartjs-2_DATA} ${chartjs-2_NOINST_DATA})
