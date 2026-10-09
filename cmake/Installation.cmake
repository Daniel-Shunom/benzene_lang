include(GNUInstallDirs)

install(TARGETS ether RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

option(ETHER_INSTALL_SERVERS "Build and install the precompiled LSP and MCP servers" OFF)
if (ETHER_INSTALL_SERVERS)
  find_program(ETHER_GLEAM_EXECUTABLE NAMES gleam)
  find_program(ETHER_ERLANG_EXECUTABLE NAMES erl)
  if (NOT ETHER_GLEAM_EXECUTABLE OR NOT ETHER_ERLANG_EXECUTABLE)
    message(FATAL_ERROR "Building the installed servers requires Gleam and Erlang/OTP on PATH")
  endif()
  foreach(server IN ITEMS lsp mcp)
    set(package ether_${server})
    set(source_dir "${PROJECT_SOURCE_DIR}/${server}/${package}")
    if (NOT EXISTS "${source_dir}/gleam.toml")
      message(FATAL_ERROR "Missing ${server} package: ${source_dir}")
    endif()
    file(GLOB_RECURSE server_sources CONFIGURE_DEPENDS
      "${source_dir}/src/*.gleam" "${source_dir}/src/*.erl")
    set(stamp "${PROJECT_BINARY_DIR}/shipments/${server}.stamp")
    add_custom_command(OUTPUT "${stamp}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shipments"
      COMMAND "${ETHER_GLEAM_EXECUTABLE}" export erlang-shipment
      COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
      WORKING_DIRECTORY "${source_dir}"
      DEPENDS ${server_sources} "${source_dir}/gleam.toml" "${source_dir}/manifest.toml"
      COMMENT "Exporting production ${server} server"
      VERBATIM)
    add_custom_target(ether_${server}_shipment ALL DEPENDS "${stamp}")
    add_executable(ether-${server} "${PROJECT_SOURCE_DIR}/tools/server_launcher.cpp")
    add_dependencies(ether-${server} ether_${server}_shipment)
    target_compile_definitions(ether-${server} PRIVATE
      ETHER_SERVER="${server}"
      ETHER_VERSION="${PROJECT_VERSION}"
      ETHER_LIBDIR="${CMAKE_INSTALL_LIBDIR}")
    target_compile_options(ether-${server} PRIVATE -Wall)
    install(TARGETS ether-${server} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY "${source_dir}/build/erlang-shipment/"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/benzene/${server}"
      PATTERN "entrypoint.sh" EXCLUDE
      PATTERN "entrypoint.ps1" EXCLUDE)
  endforeach()
endif()

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # Produce executables that do not depend on a developer's MinGW PATH.
  target_link_options(ether PRIVATE -static -static-libgcc -static-libstdc++)
  if (ETHER_INSTALL_SERVERS)
    target_link_options(ether-lsp PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(ether-mcp PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
endif()

install(DIRECTORY "${PROJECT_SOURCE_DIR}/lsp/editors/nvim/"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/benzene/editors/nvim")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/docs/"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/benzene")
install(FILES "${PROJECT_SOURCE_DIR}/README.md" "${PROJECT_SOURCE_DIR}/LICENSE"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/benzene")
install(FILES "${PROJECT_SOURCE_DIR}/lsp/README.md"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/benzene/lsp")
if (EXISTS "${PROJECT_SOURCE_DIR}/mcp/examples")
  install(DIRECTORY "${PROJECT_SOURCE_DIR}/mcp/examples/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/benzene/examples/mcp")
  install(FILES "${PROJECT_SOURCE_DIR}/mcp/ether_mcp/README.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/benzene/mcp")
endif()

set(CPACK_PACKAGE_NAME "Benzene")
set(CPACK_PACKAGE_VENDOR "Benzene")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Benzene compiler, language server and MCP server")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Benzene")
set(CPACK_PACKAGE_FILE_NAME "benzene-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
if (WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
