function(add_resources out_var id)
  set(result)

  string(CONCAT content_h
    "#include <map>\n"
    "#include <cstdint>\n"
    "\n"
    "namespace ${id}\n"
    "{\n"
    "  extern const std::map<uint32_t, std::pair<unsigned char *, unsigned int>> resources\;\n"
    "}\n"
  )

  string(CONCAT content_cpp_top
    "#include \"${id}_resources.h\"\n"
    "#include \"../resource/resource.h\"\n"
  )
  
  string(CONCAT content_cpp_public
    "namespace ${id}\n"
    "{\n"
    "\n"
    "  const std::map<uint32_t, std::pair<unsigned char *, unsigned int>> resources = {\n"
  )

  set(options ${ARGN})

  while(options)
    list(GET options 0 resource_id)
    list(GET options 1 in_f)

    set(out_f_cpp "${CMAKE_CURRENT_BINARY_DIR}/${in_f}.cpp")
    add_custom_command(
      OUTPUT ${out_f_cpp}
      COMMAND xxd -i ${in_f} > ${out_f_cpp}
      COMMENT "Adding resource: ${in_f} -> ${out_f_cpp}"
      DEPENDS ${binary_file}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # this is the same as what xxd does re. filename
    string(REGEX REPLACE "[ ./-]" "_" safe_in_f ${in_f})
    set(symbol "${safe_in_f}")

    string(APPEND content_cpp_private
      "// ${in_f}\n"
      "extern unsigned char * ${symbol}\;\n"
      "extern int ${symbol}_len\;\n"
      "\n")
    string(APPEND content_cpp_public
      "    {${resource_id}, {${symbol}, ${symbol}_len}},\n")
    list(APPEND result ${out_f_cpp})
    list(REMOVE_AT options 0 1)
  endwhile()

  string(APPEND content_cpp_public
    "  }\;\n"
    "\n"
    "}\n"
  )

  set(out_h "${CMAKE_CURRENT_BINARY_DIR}/${id}_resources.h")
  file(WRITE ${out_h} ${content_h})
  message("Generating header for '${id}': ${out_h}")
  list(APPEND result ${out_h})
  set(out_cpp "${CMAKE_CURRENT_BINARY_DIR}/${id}_resources.cpp")
  file(WRITE ${out_cpp}
    ${content_cpp_top}
    "\n"
    ${content_cpp_private}
    "\n"
    ${content_cpp_public})
  message("Generating cpp for '${id}': ${out_cpp}")
  list(APPEND result ${out_cpp})
  set(${out_var} "${result}" PARENT_SCOPE)

endfunction()
