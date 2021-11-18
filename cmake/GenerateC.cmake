
function(generate_c_string_array FILE VARNAME CONTENT)
	file(APPEND ${FILE} "static inline const char* ${VARNAME}[] = {\n")
	foreach(ELEM ${${CONTENT}})
		file(APPEND ${FILE} "  \"${ELEM}\",\n")
	endforeach()
	file(APPEND ${FILE} "};\n")
endfunction()