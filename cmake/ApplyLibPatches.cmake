# Local fixes to the upstream submodules, kept as patch files in
# lib-patches/<name>/ and applied to the submodule's working tree at
# configure time. A stamp in the submodule's Git folder records which patches
# (by content) the working tree has; when the set changes -- a patch added,
# changed or removed -- the working tree is reset to the submodule's own
# sources and all of its patches are applied again. (So don't keep other
# edits in a patched submodule's working tree.)
# The submodule is lib/<name> unless a folder (relative to the repo root) is
# given, as for submodules nested in another one.
find_package(Git REQUIRED)

# The Git tree of the submodule's working tree as it is, untracked files included, built in an
# index of its own so the submodule's index is left alone.
function(snp_lib_patches_tree dir git_dir out_var)
    set(git "${GIT_EXECUTABLE}" -c safe.directory=*)
    set(ENV{GIT_INDEX_FILE} "${git_dir}/snp-lib-patches.index")
    execute_process(COMMAND ${git} read-tree HEAD WORKING_DIRECTORY "${dir}" OUTPUT_QUIET ERROR_QUIET)
    execute_process(COMMAND ${git} add -A WORKING_DIRECTORY "${dir}" OUTPUT_QUIET ERROR_QUIET)
    execute_process(COMMAND ${git} write-tree
        WORKING_DIRECTORY "${dir}"
        RESULT_VARIABLE result OUTPUT_VARIABLE tree ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    unset(ENV{GIT_INDEX_FILE})
    if (NOT result EQUAL 0)
        set(tree "unknown")
    endif()
    set(${out_var} "${tree}" PARENT_SCOPE)
endfunction()

function(snp_apply_lib_patches submodule)
    if (ARGC GREATER 1)
        set(dir "${CMAKE_SOURCE_DIR}/${ARGV1}")
    else()
        set(dir "${CMAKE_SOURCE_DIR}/lib/${submodule}")
    endif()
    # CONFIGURE_DEPENDS and CMAKE_CONFIGURE_DEPENDS: adding, removing or changing a patch
    # re-runs the configure (and so this) on the next build, not just on a manual reconfigure.
    file(GLOB patches CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/lib-patches/${submodule}/*.patch")
    set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${patches})
    list(SORT patches)

    set(state "")
    foreach(patch IN LISTS patches)
        get_filename_component(name "${patch}" NAME)
        file(SHA256 "${patch}" hash)
        string(APPEND state "${name} ${hash}\n")
    endforeach()

    # safe.directory: Git refuses to work in a checkout it thinks another user owns, which
    # Windows reports for folders created as Administrator.
    set(git "${GIT_EXECUTABLE}" -c safe.directory=*)
    execute_process(
        COMMAND ${git} rev-parse --absolute-git-dir
        WORKING_DIRECTORY "${dir}"
        RESULT_VARIABLE result OUTPUT_VARIABLE git_dir ERROR_VARIABLE error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Couldn't find the Git folder of ${dir}:\n${error}")
    endif()
    set(stamp "${git_dir}/snp-lib-patches.txt")
    if (EXISTS "${stamp}")
        file(READ "${stamp}" applied)
        snp_lib_patches_tree("${dir}" "${git_dir}" tree)
        if (applied STREQUAL "${state}tree ${tree}\n")
            # Same set, and the tree is still the one they made (a checkout or submodule
            # update undoes them). Compared as a whole rather than each patch reverse-checked:
            # a patch that a later one changes again no longer reverses on its own.
            return()
        endif()
    endif()

    file(REMOVE "${stamp}")
    execute_process(
        COMMAND ${git} checkout -- .
        WORKING_DIRECTORY "${dir}"
        RESULT_VARIABLE result ERROR_VARIABLE error)
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Couldn't reset ${dir} to apply its patches:\n${error}")
    endif()
    # Files the patches added are untracked; remove them, or they can't be added again.
    execute_process(
        COMMAND ${git} clean -fdq
        WORKING_DIRECTORY "${dir}"
        RESULT_VARIABLE result ERROR_VARIABLE error)
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Couldn't reset ${dir} to apply its patches:\n${error}")
    endif()
    foreach(patch IN LISTS patches)
        execute_process(
            COMMAND ${git} apply --ignore-whitespace "${patch}"
            WORKING_DIRECTORY "${dir}"
            RESULT_VARIABLE result ERROR_VARIABLE error)
        if (NOT result EQUAL 0)
            message(FATAL_ERROR "Couldn't apply ${patch} to ${dir}:\n${error}")
        endif()
        message(STATUS "Applied ${patch} to ${dir}")
    endforeach()
    snp_lib_patches_tree("${dir}" "${git_dir}" tree)
    file(WRITE "${stamp}" "${state}tree ${tree}\n")
endfunction()
