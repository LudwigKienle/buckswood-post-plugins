cmake_minimum_required(VERSION 3.20)

project(BuckswoodPremierePort LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/..")

add_library(buckswood_premiere_adapter STATIC
    src/BuckswoodPremiereAdapter.cpp
    "${ROOT_DIR}/Buckswood_AI_Photorealizer/src/PhotorealizerCore.cpp"
    "${ROOT_DIR}/Buckswood_Lens_Physics/src/LensPhysicsCore.cpp"
)

target_include_directories(buckswood_premiere_adapter PUBLIC
    src
    "${ROOT_DIR}/Buckswood_AI_Photorealizer/include"
    "${ROOT_DIR}/Buckswood_Lens_Physics/include"
)

add_executable(premiere_adapter_smoketest
    src/PremiereAdapterSmokeTest.cpp
)

target_link_libraries(premiere_adapter_smoketest PRIVATE buckswood_premiere_adapter)

option(BUCKSWOOD_BUILD_ADOBE_SDK_WRAPPERS "Build Adobe Premiere/After Effects SDK wrappers" OFF)
set(ADOBE_AE_SDK "" CACHE PATH "Path to Adobe After Effects SDK root")

if(BUCKSWOOD_BUILD_ADOBE_SDK_WRAPPERS)
    if(NOT ADOBE_AE_SDK)
        message(FATAL_ERROR "Set -DADOBE_AE_SDK=/path/to/AfterEffectsSDK")
    endif()

    add_library(BuckswoodAIPhotorealizerAdobe MODULE
        src/BuckswoodAIPhotorealizerAE.cpp
    )
    target_link_libraries(BuckswoodAIPhotorealizerAdobe PRIVATE buckswood_premiere_adapter)
    target_include_directories(BuckswoodAIPhotorealizerAdobe PRIVATE
        "${ADOBE_AE_SDK}/Examples/Headers"
        "${ADOBE_AE_SDK}/Examples/Headers/SP"
        "${ADOBE_AE_SDK}/Examples/Util"
        "${ADOBE_AE_SDK}/Examples/Resources"
    )

    add_library(BuckswoodLensPhysicsAdobe MODULE
        src/BuckswoodLensPhysicsAE.cpp
    )
    target_link_libraries(BuckswoodLensPhysicsAdobe PRIVATE buckswood_premiere_adapter)
    target_include_directories(BuckswoodLensPhysicsAdobe PRIVATE
        "${ADOBE_AE_SDK}/Examples/Headers"
        "${ADOBE_AE_SDK}/Examples/Headers/SP"
        "${ADOBE_AE_SDK}/Examples/Util"
        "${ADOBE_AE_SDK}/Examples/Resources"
    )
endif()
