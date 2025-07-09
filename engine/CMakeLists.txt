project(VoidArchitect_Engine VERSION 0.1.0 LANGUAGES CXX)

# Engine modules
add_subdirectory(common)
#add_subdirectory(rhi)

# Optional modules based on build configuration
if (VOID_ARCHITECT_BUILD_CLIENT)
    #add_subdirectory(client)
endif ()

if (VOID_ARCHITECT_BUILD_SERVER)
    #add_subdirectory(server)
endif ()

if (VOID_ARCHITECT_BUILD_EDITOR)
    #add_subdirectory(editor)
endif ()

# Export engine targets for external projects
#export(TARGETS VoidArchitectEngine_Common VoidArchitectEngine_RHI_Interface
#        FILE "${CMAKE_CURRENT_BINARY_DIR}/VoidArchitectEngineTargets.cmake")

# Add client and editor targets if built

#if (VOID_ARCHITECT_BUILD_CLIENT)
#    export(TARGETS VoidArchitectEngine_Client APPEND
#            FILE "${CMAKE_CURRENT_BINARY_DIR}/VoidArchitectEngineTargets.cmake")
#endif ()
#
#if (VOID_ARCHITECT_BUILD_SERVER)
#    export(TARGETS VoidArchitectEngine_Server APPEND
#            FILE "${CMAKE_CURRENT_BINARY_DIR}/VoidArchitectEngineTargets.cmake")
#endif ()
#
#if (VOID_ARCHITECT_BUILD_EDITOR)
#    export(TARGETS VoidArchitectEngine_Editor APPEND
#            FILE "${CMAKE_CURRENT_BINARY_DIR}/VoidArchitectEngineTargets.cmake")
#endif ()