cmake_minimum_required(VERSION 3.16)

project(coin VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# set(CMAKE_C_STANDARD 11)
# set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Test)

set(CMAKE_PREFIX_PATH "C:/ProgramFiles/vcpkg/installed/x64-windows")
set(CMAKE_TOOLCHAIN_FILE "C:/ProgramFiles/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Set OpenSSL
find_package(OpenSSL CONFIG REQUIRED)

# RocksDB setup
find_package(RocksDB CONFIG REQUIRED)

# GoogleTest and GoogleMock
find_package(GTest CONFIG REQUIRED)

# Boost
# find_package(Boost CONFIG REQUIRED)

# Crypto++
find_package(cryptopp CONFIG REQUIRED)

# Regex library
find_package(boost_regex REQUIRED CONFIG)

# json
find_package(nlohmann_json CONFIG REQUIRED)

set(PROJECT_SOURCES

        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)


if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(coin
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        coinminerthread.h
        coinminerthread.cpp
        key.h


        uint256.h
        bignum.h

        serialize.h
        db.h
        db.cpp

        sha.h sha.cpp
        bignum.h
        base58.h
        util.h util.cpp

        main.h main.cpp
        net.h net.cpp
        market.h market.cpp
        script.h script.cpp
        pch.h

        senddialog.h senddialog.cpp senddialog.ui
        adressbookdialog.h adressbookdialog.cpp adressbookdialog.ui
        irc.h irc.cpp
        # AI:
        train_gpt2.cpp
        ai_utils.h
        ai_tokenizer.h
        ai_dataloader.h
        unistd.h
        ai_rand.h
        train_gpt2.h
        ai_tokenizer.cpp
        tokenizer.h tokenizer.cpp
        fileutils.h
        resources.qrc









    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET coin APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(coin SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(coin
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(coin PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Link OpenSSL libraries
target_link_libraries(coin PRIVATE OpenSSL::SSL)
target_link_libraries(coin PRIVATE OpenSSL::Crypto)

# Link RocksDB library
target_link_libraries(coin PRIVATE RocksDB::rocksdb)

# Link GoogleTest libraries
target_link_libraries(coin PRIVATE GTest::gtest GTest::gmock GTest::gtest_main GTest::gmock_main)

# Link Boost libraries
# target_link_libraries(coin PRIVATE Boost::boost)

# Link Crypto++ library
target_link_libraries(coin PRIVATE cryptopp::cryptopp)

# regex
target_link_libraries(coin PRIVATE Boost::regex)


# Link json11
target_link_libraries(coin PRIVATE nlohmann_json::nlohmann_json)

# Link Winsock library on Windows
if (WIN32)
    target_link_libraries(coin PRIVATE ws2_32)
endif()


# Enable precompiled headers
target_precompile_headers(coin PRIVATE pch.h)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.coin)
endif()
set_target_properties(coin PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS coin
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(coin)
endif()
