find_path(GRPC_INCLUDE_DIR
  grpc
	HINTS
    "$ENV{CEREAL_ROOT}/include"
    "/usr/include"
    "$ENV{PROGRAMFILES}/grpc/include"
)

set(GRPC_INCLUDE_DIRS ${GRPC_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(grpc DEFAULT_MSG GRPC_INCLUDE_DIRS)
