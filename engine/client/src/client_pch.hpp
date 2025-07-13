//
// Created by Michael Desmedt on 12/07/2025.
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

#include <VoidArchitect/Engine/Common/Collections/Array.hpp>
#include <VoidArchitect/Engine/Common/Collections/HashMap.hpp>

using namespace VoidArchitect::Collections;

#ifdef VOID_ARCH_PLATFORM_WINDOWS
#include <Windows.h>
#elif VOID_ARCH_PLATFORM_LINUX
#elif VOID_ARCH_PLATFORM_MACOS
#endif
