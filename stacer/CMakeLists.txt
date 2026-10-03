
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(stacer)

set(MANAGERS_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/Managers")
set(PAGES_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/Pages")

include_directories(
  "${PROJECT_ROOT}/stacer-core"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${MANAGERS_DIR}"
  "${PAGES_DIR}/Dashboard"
  "${PAGES_DIR}/Processes"
  "${PAGES_DIR}/Resources"
  "${PAGES_DIR}/Services"
  "${PAGES_DIR}/Settings"
  "${PAGES_DIR}/StartupApps"
  "${PAGES_DIR}/SystemCleaner"
  "${PAGES_DIR}/Uninstaller"
  "${CMAKE_CURRENT_BINARY_DIR}" # Necessary for CMake 3.7 and older
)

# Sources
file(GLOB_RECURSE ${PROJECT_NAME}_srcs         "${CMAKE_CURRENT_SOURCE_DIR}/**.cpp")
file(GLOB_RECURSE ${PROJECT_NAME}_translations "${PROJECT_ROOT}/translations/**.ts")

# Translations
# The .ts files must be passed expanded, not as the name of the variable holding
# them: the LinguistTools macros classify each argument by file extension, so a
# bare variable name would look like a source file and no .qm files would be
# produced. qt6_add_translation() only compiles the .ts files; qt6_create_translation()
# would additionally run lupdate and rewrite the sources on every build.
# This subproject can also be configured on its own, in which case the version
# floor set by the top-level project is not inherited.
if(NOT QT6_MIN_VERSION)
    set(QT6_MIN_VERSION 6.5)
endif()

find_package(Qt6 ${QT6_MIN_VERSION} REQUIRED COMPONENTS LinguistTools)
qt6_add_translation(QM_FILES  ${${PROJECT_NAME}_translations})
set_directory_properties(PROPERTIES  ADDITIONAL_MAKE_CLEAN_FILES  "${QM_FILES}")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(${PROJECT_NAME}
  ${${PROJECT_NAME}_srcs}
  "${CMAKE_CURRENT_SOURCE_DIR}/static.qrc"
  ${QM_FILES}
)

target_link_libraries(${PROJECT_NAME}
  stacer-core  Qt6::Core  Qt6::Gui  Qt6::Widgets  Qt6::Charts  Qt6::Svg  Qt6::Concurrent
)

# Running LTO in Release builds, if the C++ compiler is GNU GCC
if("${CMAKE_BUILD_TYPE}" STREQUAL "Release"  AND  "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options(${PROJECT_NAME}  PRIVATE  "-flto")
  list(APPEND CMAKE_EXE_LINKER_FLAGS "-flto")
endif()

install(
  TARGETS  ${PROJECT_NAME}
  CONFIGURATIONS Release RelWithDebInfo MinSizeRel # Not allowing to install an unoptimized build
  RUNTIME  DESTINATION  bin
)

install(
  FILES           "${PROJECT_ROOT}/applications/stacer.desktop"
  DESTINATION     share/applications
  CONFIGURATIONS  Release RelWithDebInfo MinSizeRel
)

install(
  FILES           "${PROJECT_ROOT}/stacer/static/logo.png"
  DESTINATION     share/icons
  CONFIGURATIONS  Release RelWithDebInfo MinSizeRel
  RENAME          stacer.png
)
