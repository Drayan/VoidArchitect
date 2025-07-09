//
// Created by Michael Desmedt on 08/07/2025.
//
#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <span>
#include <utility>

#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>

#include "Collections/Array.hpp"
#include "Collections/HashMap.hpp"
#include "Collections/HashSet.hpp"

using namespace VoidArchitect::Collections;

#ifdef VOID_ARCH_PLATFORM_WINDOWS
#include <Windows.h>
#elifdef VOID_ARCH_PLATFORM_LINUX
#elifdef VOID_ARCH_PLATFORM_MACOS
#endif
