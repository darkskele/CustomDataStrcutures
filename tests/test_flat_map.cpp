#include <gtest/gtest.h>
#include "mcds/flat_map.hpp"
#include "test_types/custom_types.hpp"

#include "test_suites/associative_container_typed_test.hpp"

namespace mcds::tests
{
    // Create a FlatMap-specific derived suite
    template <typename ContainerType>
    class FlatMapTypedTest : public AssociativeContainerTypedTest<ContainerType>
    {
    };

    // Define all 9 concrete FlatMap type combinations
    using FlatMapIntInt = FlatMap<int, int>;
    using FlatMapIntDouble = FlatMap<int, double>;
    using FlatMapDoubleInt = FlatMap<double, int>;
    using FlatMapIntComplex = FlatMap<int, ComplexType>;
    using FlatMapIntTracked = FlatMap<int, TrackedType>;
    using FlatMapComplexInt = FlatMap<ComplexType, int>;
    using FlatMapTrackedInt = FlatMap<TrackedType, int>;
    using FlatMapComplexComplex = FlatMap<ComplexType, ComplexType>;
    using FlatMapTrackedTracked = FlatMap<TrackedType, TrackedType>;

    // Group all types together
    using FlatMapTypes = ::testing::Types<
        FlatMapIntInt,
        FlatMapIntDouble,
        FlatMapDoubleInt,
        FlatMapIntComplex,
        FlatMapIntTracked,
        FlatMapComplexInt,
        FlatMapTrackedInt,
        FlatMapComplexComplex,
        FlatMapTrackedTracked>;

    // Instantiate the test suite for all FlatMap types
    TYPED_TEST_SUITE(FlatMapTypedTest, FlatMapTypes);

#define CONTAINER_TEST_SUITE_NAME FlatMapTypedTest
#include "test_suites/associative_container_test_suite.hpp"
#undef CONTAINER_TEST_SUITE_NAME

} // namespace mcds::tests::FlatMap
