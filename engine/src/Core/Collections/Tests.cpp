//
// Tests/FixedStorageValidationTest.cpp
// Quick validation test for FixedStorage implementation
//
#include "FixedStorage.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace VoidArchitect;
using namespace VoidArchitect::Collections;

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

/// @brief Test basic allocation and access
bool TestBasicOperations()
{
    std::cout << "Testing basic operations...\n";

    FixedStorage<TestObject, 100> storage;

    // Test initial state
    if (!storage.IsEmpty() || storage.IsFull() || storage.GetUsedSlots() != 0)
    {
        std::cout << "  ❌ Initial state incorrect\n";
        return false;
    }

    // Test allocation
    auto handle1 = storage.Allocate(42, "TestObject1", 3.14f);
    if (!handle1.IsValid() || !storage.IsValid(handle1))
    {
        std::cout << "  ❌ Allocation failed\n";
        return false;
    }

    // Test access
    TestObject* obj = storage.Get(handle1);
    if (!obj || obj->id != 42 || obj->name != "TestObject1" || obj->value != 3.14f)
    {
        std::cout << "  ❌ Object access failed\n";
        return false;
    }

    // Test statistics
    if (storage.GetUsedSlots() != 1 || storage.IsEmpty() || storage.GetAvailableSlots() != 99)
    {
        std::cout << "  ❌ Statistics incorrect\n";
        return false;
    }

    // Test release
    if (!storage.Release(handle1))
    {
        std::cout << "  ❌ Release failed\n";
        return false;
    }

    // Test handle becomes invalid after release
    if (storage.IsValid(handle1) || storage.Get(handle1) != nullptr)
    {
        std::cout << "  ❌ Handle should be invalid after release\n";
        return false;
    }

    std::cout << "  ✅ Basic operations passed\n";
    return true;
}

/// @brief Test handle generation and ABA prevention
bool TestHandleGeneration()
{
    std::cout << "Testing handle generation...\n";

    // Use capacity of 1 to force slot reuse
    FixedStorage<TestObject, 1> storage;

    // Allocate in only available slot
    auto handle1 = storage.Allocate(1, "First", 1.0f);
    if (!handle1.IsValid() || handle1.GetIndex() != 0 || handle1.GetGeneration() != 1)
    {
        std::cout << "  ❌ First allocation incorrect (index=" << handle1.GetIndex() << ", gen=" <<
            handle1.GetGeneration() << ")\n";
        return false;
    }

    // Release and reallocate in same slot (forced by capacity=1)
    if (!storage.Release(handle1))
    {
        std::cout << "  ❌ Release failed\n";
        return false;
    }

    auto handle2 = storage.Allocate(2, "Second", 2.0f);
    if (!handle2.IsValid())
    {
        std::cout << "  ❌ Second allocation failed\n";
        return false;
    }

    // Should be same index but different generation
    if (handle2.GetIndex() != 0 || handle2.GetGeneration() != 2)
    {
        std::cout << "  ❌ Generation not incremented correctly (index=" << handle2.GetIndex() <<
            ", gen=" << handle2.GetGeneration() << ", expected gen=2)\n";
        return false;
    }

    // Old handle should be invalid even though slot is reused (ABA prevention)
    if (storage.IsValid(handle1))
    {
        std::cout << "  ❌ Old handle should be invalid (ABA prevention failed)\n";
        return false;
    }

    // Get should return nullptr for old handle
    if (storage.Get(handle1) != nullptr)
    {
        std::cout << "  ❌ Get() should return nullptr for invalid handle\n";
        return false;
    }

    // Verify new handle works correctly
    TestObject* obj = storage.Get(handle2);
    if (!obj || obj->id != 2 || obj->name != "Second")
    {
        std::cout << "  ❌ New object data incorrect\n";
        return false;
    }

    std::cout << "  ✅ Handle generation passed\n";
    return true;
}

/// @brief Test multiple slot allocation and generation tracking
bool TestMultipleSlots()
{
    std::cout << "Testing multiple slots...\n";

    FixedStorage<TestObject, 5> storage;
    std::vector<Handle<TestObject>> handles;

    // Allocate 3 objects
    for (int i = 0; i < 3; ++i)
    {
        auto handle = storage.Allocate(i, "Obj" + std::to_string(i), static_cast<float>(i));
        if (!handle.IsValid())
        {
            std::cout << "  ❌ Failed to allocate object " << i << "\n";
            return false;
        }
        handles.push_back(handle);
    }

    // Verify all handles have generation 1 (first allocation in each slot)
    for (size_t i = 0; i < handles.size(); ++i)
    {
        if (handles[i].GetGeneration() != 1)
        {
            std::cout << "  ❌ Handle " << i << " should have generation 1, got " << handles[i].
                GetGeneration() << "\n";
            return false;
        }
    }

    // Release middle handle
    auto releasedHandle = handles[1];
    if (!storage.Release(releasedHandle))
    {
        std::cout << "  ❌ Failed to release handle 1\n";
        return false;
    }

    // Allocate new object (should reuse released slot eventually)
    auto newHandle = storage.Allocate(100, "NewObj", 100.0f);
    if (!newHandle.IsValid())
    {
        std::cout << "  ❌ Failed to allocate new object\n";
        return false;
    }

    // Verify old handle is invalid
    if (storage.IsValid(releasedHandle))
    {
        std::cout << "  ❌ Released handle should be invalid\n";
        return false;
    }

    // If new handle uses same slot, generation should be incremented
    if (newHandle.GetIndex() == releasedHandle.GetIndex())
    {
        if (newHandle.GetGeneration() != 2)
        {
            std::cout << "  ❌ Reused slot should have generation 2, got " << newHandle.
                GetGeneration() << "\n";
            return false;
        }
    }

    std::cout << "  ✅ Multiple slots passed\n";
    return true;
}

/// @brief Test capacity limits
bool TestCapacityLimits()
{
    std::cout << "Testing capacity limits...\n";

    FixedStorage<TestObject, 3> storage; // Very small capacity
    std::vector<Handle<TestObject>> handles;

    // Fill to capacity
    for (int i = 0; i < 3; ++i)
    {
        auto handle = storage.Allocate(i, "Test" + std::to_string(i), static_cast<float>(i));
        if (!handle.IsValid())
        {
            std::cout << "  ❌ Failed to allocate within capacity\n";
            return false;
        }
        handles.push_back(handle);
    }

    if (!storage.IsFull() || storage.GetUsedSlots() != 3)
    {
        std::cout << "  ❌ Storage should be full\n";
        return false;
    }

    // Try to allocate beyond capacity
    auto overflowHandle = storage.Allocate(999, "Overflow", 999.0f);
    if (overflowHandle.IsValid())
    {
        std::cout << "  ❌ Should not allocate beyond capacity\n";
        return false;
    }

    // Release one and try again
    storage.Release(handles[1]);
    auto newHandle = storage.Allocate(100, "New", 100.0f);
    if (!newHandle.IsValid())
    {
        std::cout << "  ❌ Should allocate after release\n";
        return false;
    }

    std::cout << "  ✅ Capacity limits passed\n";
    return true;
}

/// @brief Test thread safety with concurrent allocations
bool TestThreadSafety()
{
    std::cout << "Testing thread safety...\n";

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
                std::cout << "  ❌ Invalid handle from thread " << t << "\n";
                return false;
            }

            TestObject* obj = storage.Get(handle);
            if (!obj)
            {
                std::cout << "  ❌ Null object from thread " << t << "\n";
                return false;
            }
        }
    }

    if (totalAllocated != 200 || storage.GetUsedSlots() != 200)
    {
        std::cout << "  ❌ Expected 200 allocations, got " << totalAllocated << " handles and " <<
            storage.GetUsedSlots() << " used slots\n";
        return false;
    }

    std::cout << "  ✅ Thread safety passed\n";
    return true;
}

/// @brief Run all validation tests
int RunAllTests()
{
    std::cout << "=== FixedStorage Validation Tests ===\n\n";

    bool allPassed = true;

    allPassed &= TestBasicOperations();
    allPassed &= TestHandleGeneration();
    allPassed &= TestMultipleSlots();
    allPassed &= TestCapacityLimits();
    allPassed &= TestThreadSafety();

    std::cout << "\n=== Results ===\n";
    if (allPassed)
    {
        std::cout << "🎉 All tests PASSED! FixedStorage implementation is validated.\n";
        return 0;
    }
    else
    {
        std::cout << "❌ Some tests FAILED. Review implementation.\n";
        return 1;
    }
}
