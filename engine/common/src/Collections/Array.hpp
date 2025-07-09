// Copyright 2025 Michaël Desmedt
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// Created by Michael Desmedt on 05/06/2025.
//
#pragma once

namespace VoidArchitect::Collections
{
    namespace impl
    {
#include <vector>
    }

    // We will simply use VAArray for now. This will allow us to change it later on easily.
    template <typename T>
    using VAArray = std::vector<T>;
}
