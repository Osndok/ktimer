project(ktimer)

cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)
set (QT_MIN_VERSION "5.5.0")

find_package (ECM 5.10.0 REQUIRED NO_MODULE)
set (CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(ECMInstallIcons)
include(ECMAddAppIcon)

find_package (Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Widgets
)

find_package(KF5 REQUIRED COMPONENTS
    DocTools
    I18n
    WidgetsAddons
    KIO
    DBusAddons
    Notifications
)


set(ktimer_SRCS main.cpp ktimer.cpp )

ki18n_wrap_ui(ktimer_SRCS prefwidget.ui )

file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*-apps-ktimer.png")
ecm_add_app_icon(ktimer_SRCS ICONS ${ICONS_SRCS})

add_executable(ktimer ${ktimer_SRCS})

target_link_libraries(ktimer  KF5::I18n KF5::KIOWidgets KF5::ConfigWidgets KF5::Notifications KF5::DBusAddons)

install(TARGETS ktimer  ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )


########### install files ###############

install( PROGRAMS org.kde.ktimer.desktop  DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.ktimer.appdata.xml DESTINATION ${CMAKE_INSTALL_METAINFODIR})

ecm_install_icons( ICONS 128-apps-ktimer.png  16-apps-ktimer.png  32-apps-ktimer.png  48-apps-ktimer.png DESTINATION ${KDE_INSTALL_ICONDIR} THEME hicolor  )

add_subdirectory(doc)
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
