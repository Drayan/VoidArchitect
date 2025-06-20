//
// Created by Michael Desmedt on 17/06/2025.
//
//
// FixedStorage tests integrated with TestRunner
//
#include "../Core/TestRunner.hpp"
#include <Core/Collections/FixedStorage.hpp>

using namespace VoidArchitect;
using namespace VoidArchitect::Collections;
using namespace VoidArchitect::Testing;

/// @brief Simple test struct for storage validation
struct TestObject
{
    int id;
    std::string name;
    float value;

    TestObject(int i, const std::string& n, float v)
        : id(i),
          name(n),
          value(v)
    {
    }

    ~TestObject()
    {
        // Destructor validation
        id = -1;
    }
};

/// @brief Test basic FixedStorage operations
bool TestFixedStorageBasics()
{
    FixedStorage<TestObject, 100> storage;

    // Test initial state
    if (!storage.IsEmpty() || storage.IsFull() || storage.GetUsedSlots() != 0)
    {
        return false;
    }

    // Test allocation
    auto handle1 = storage.Allocate(42, "TestObject1", 3.14f);
    if (!handle1.IsValid() || !storage.IsValid(handle1))
    {
        return false;
    }

    // Test access
    TestObject* obj = storage.Get(handle1);
    if (!obj || obj->id != 42 || obj->name != "TestObject1" || obj->value != 3.14f)
    {
        return false;
    }

    // Test statistics
    if (storage.GetUsedSlots() != 1 || storage.IsEmpty() || storage.GetAvailableSlots() != 99)
    {
        return false;
    }

    // Test release
    if (!storage.Release(handle1))
    {
        return false;
    }

    // Test handle becomes invalid after release
    if (storage.IsValid(handle1) || storage.Get(handle1) != nullptr)
    {
        return false;
    }

    return true;
}

/// @brief Test handle generation and ABA prevention
bool TestFixedStorageGenerations()
{
    // Use capacity of 1 to force slot reuse
    FixedStorage<TestObject, 1> storage;

    // Allocate in only available slot
    auto handle1 = storage.Allocate(1, "First", 1.0f);
    if (!handle1.IsValid() || handle1.GetIndex() != 0 || handle1.GetGeneration() != 1)
    {
        return false;
    }

    // Release and reallocate in same slot (forced by capacity=1)
    if (!storage.Release(handle1))
    {
        return false;
    }

    auto handle2 = storage.Allocate(2, "Second", 2.0f);
    if (!handle2.IsValid())
    {
        return false;
    }

    // Should be same index but different generation
    if (handle2.GetIndex() != 0 || handle2.GetGeneration() != 2)
    {
        return false;
    }

    // Old handle should be invalid even though slot is reused (ABA prevention)
    if (storage.IsValid(handle1))
    {
        return false;
    }

    // Get should return nullptr for old handle
    if (storage.Get(handle1) != nullptr)
    {
        return false;
    }

    // Verify new handle works correctly
    TestObject* obj = storage.Get(handle2);
    if (!obj || obj->id != 2 || obj->name != "Second")
    {
        return false;
    }

    return true;
}

/// @brief Test capacity limits and overflow handling
bool TestFixedStorageCapacity()
{
    FixedStorage<TestObject, 3> storage; // Very small capacity
    std::vector<Handle<TestObject>> handles;

    // Fill to capacity
    for (int i = 0; i < 3; ++i)
    {
        auto handle = storage.Allocate(i, "Test" + std::to_string(i), static_cast<float>(i));
        if (!handle.IsValid())
        {
            return false;
        }
        handles.push_back(handle);
    }

    if (!storage.IsFull() || storage.GetUsedSlots() != 3)
    {
        return false;
    }

    // Try to allocate beyond capacity
    auto overflowHandle = storage.Allocate(999, "Overflow", 999.0f);
    if (overflowHandle.IsValid())
    {
        return false;
    }

    // Release one and try again
    storage.Release(handles[1]);
    auto newHandle = storage.Allocate(100, "New", 100.0f);
    if (!newHandle.IsValid())
    {
        return false;
    }

    return true;
}

/// @brief Test thread safety with concurrent operations
bool TestFixedStorageThreadSafety()
{
    FixedStorage<TestObject, 1000> storage;
    std::vector<std::thread> threads;
    std::vector<std::vector<Handle<TestObject>>> threadHandles(4);

    // Launch 4 threads each allocating 50 objects
    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back(
            [&, t]()
            {
                for (int i = 0; i < 50; ++i)
                {
                    auto handle = storage.Allocate(
                        t * 50 + i,
                        "Thread" + std::to_string(t),
                        static_cast<float>(i));
                    if (handle.IsValid())
                    {
                        threadHandles[t].push_back(handle);
                    }

                    // Small delay to increase contention
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            });
    }

    // Wait for all threads
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Verify all allocations succeeded and objects are valid
    size_t totalAllocated = 0;
    for (int t = 0; t < 4; ++t)
    {
        totalAllocated += threadHandles[t].size();
        for (auto handle : threadHandles[t])
        {
            if (!storage.IsValid(handle))
            {
                return false;
            }

            TestObject* obj = storage.Get(handle);
            if (!obj)
            {
                return false;
            }
        }
    }

    if (totalAllocated != 200 || storage.GetUsedSlots() != 200)
    {
        return false;
    }

    return true;
}

// Register all FixedStorage tests with the TestRunner
VA_REGISTER_TEST(FixedStorageBasics, TestFixedStorageBasics);
VA_REGISTER_TEST(FixedStorageGenerations, TestFixedStorageGenerations);
VA_REGISTER_TEST(FixedStorageCapacity, TestFixedStorageCapacity);
VA_REGISTER_TEST(FixedStorageThreadSafety, TestFixedStorageThreadSafety);
