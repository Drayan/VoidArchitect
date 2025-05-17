//
// Created by Michael Desmedt on 13/05/2025.
//
#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>

#ifdef VOID_ARCH_PLATFORM_WINDOWS
#include <Windows.h>
#elifdef VOID_ARCH_PLATFORM_LINUX
#elifdef VOID_ARCH_PLATFORM_MACOS
#include <>
#endif
