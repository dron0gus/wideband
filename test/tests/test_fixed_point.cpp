#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include "util/fixed_point.h"

// Test that ScaledValue has the same size as TStorage (no overhead)
TEST(FixedPointTest, SizeCheck) {
    EXPECT_EQ(sizeof(ScaledValue<int8_t, 10, 1>), sizeof(int8_t));
    EXPECT_EQ(sizeof(ScaledValue<uint8_t, 10, 1>), sizeof(uint8_t));
    EXPECT_EQ(sizeof(ScaledValue<int16_t, 10, 1>), sizeof(int16_t));
    EXPECT_EQ(sizeof(ScaledValue<uint16_t, 10, 1>), sizeof(uint16_t));
    EXPECT_EQ(sizeof(ScaledValue<int32_t, 10, 1>), sizeof(int32_t));
    EXPECT_EQ(sizeof(ScaledValue<uint32_t, 10, 1>), sizeof(uint32_t));

    // Test with different scale factors
    EXPECT_EQ(sizeof(ScaledValue<int16_t, 1, 10>), sizeof(int16_t));
    EXPECT_EQ(sizeof(ScaledValue<int16_t, 1, 5>), sizeof(int16_t));
    EXPECT_EQ(sizeof(ScaledValue<int16_t, 10, 1>), sizeof(int16_t));
}

// Test alias
TEST(FixedPointTest, AliasSizeCheck) {
    EXPECT_EQ(sizeof(FixedPoint<int8_t, 10>), sizeof(int8_t));
    EXPECT_EQ(sizeof(FixedPoint<int16_t, 10>), sizeof(int16_t));
    EXPECT_EQ(sizeof(FixedPoint<int32_t, 10>), sizeof(int32_t));
}

// Test scaling with factor < 1.0
TEST(FixedPointTest, ScaleFactorLessThanOne) {
    ScaledValue<int16_t, 1, 10> value;
    
    // Test setValue/getValue
    value.setValue(12.8f);
    EXPECT_EQ(value.getRaw(), 128);
    EXPECT_FLOAT_EQ(value.getValue(), 12.8f);
    
    value.setValue(5.0f);
    EXPECT_EQ(value.getRaw(), 50);
    EXPECT_FLOAT_EQ(value.getValue(), 5.0f);
    
    // Test implicit conversion
    value = 3.14f;
    EXPECT_EQ(value.getRaw(), 31);  // Rounded
    EXPECT_NEAR(static_cast<float>(value), 3.1f, 0.1f);
}

// Test scaling with factor > 1.0
TEST(FixedPointTest, ScaleFactorMoreThanOne) {
    ScaledValue<int16_t, 10> value;
    
    // Test setValue/getValue
    value.setValue(128.0f);
    EXPECT_EQ(value.getRaw(), 13);  // 128 * 0.1 = 12.8, rounded to 13
    EXPECT_NEAR(value.getValue(), 130.0f, 1.0f);
    
    value.setValue(100.0f);
    EXPECT_EQ(value.getRaw(), 10);
    EXPECT_FLOAT_EQ(value.getValue(), 100.0f);
}

// Test scaling with factor == 1.0 (no scaling)
TEST(FixedPointTest, ScaleFactorOne) {
    ScaledValue<int16_t, 1, 1> value;
    
    value.setValue(42.0f);
    EXPECT_EQ(value.getRaw(), 42);
    EXPECT_FLOAT_EQ(value.getValue(), 42.0f);
    
    value = 100.5f;
    EXPECT_EQ(value.getRaw(), 101);  // Rounded
    EXPECT_FLOAT_EQ(value.getValue(), 101.0f);
}

// Test custom scale factor (multiply by 5)
TEST(FixedPointTest, ScaleFactorFive) {
    ScaledValue<int16_t, 5> value;
    
    value.setValue(50.0f);
    EXPECT_EQ(value.getRaw(), 10);
    EXPECT_FLOAT_EQ(value.getValue(), 50.0f);
    
    value.setValue(35.0f);
    EXPECT_EQ(value.getRaw(), 7);
    EXPECT_FLOAT_EQ(value.getValue(), 35.0f);
}

// Test rounding behavior
TEST(FixedPointTest, Rounding) {
    ScaledValue<int16_t, 1, 10> value;
    
    // Positive rounding
    value.setValue(1.24f);
    EXPECT_EQ(value.getRaw(), 12);  // Rounds down
    
    value.setValue(1.25f);
    EXPECT_EQ(value.getRaw(), 13);  // Rounds up
    
    value.setValue(1.26f);
    EXPECT_EQ(value.getRaw(), 13);  // Rounds up
    
    // Negative rounding
    ScaledValue<int16_t, 1, 10> negValue;
    negValue.setValue(-1.24f);
    EXPECT_EQ(negValue.getRaw(), -12);  // Rounds toward zero
    
    negValue.setValue(-1.26f);
    EXPECT_EQ(negValue.getRaw(), -13);  // Rounds away from zero
}

// Test clamping at maximum value
TEST(FixedPointTest, ClampingMax) {
    ScaledValue<int8_t, 1, 10> value;
    
    // int8_t max is 127, so max representable value is 127/10 = 12.7
    value.setValue(20.0f);  // Too large
    EXPECT_EQ(value.getRaw(), 127);
    EXPECT_FLOAT_EQ(value.getValue(), 12.7f);
}

// Test clamping at minimum value
TEST(FixedPointTest, ClampingMin) {
    ScaledValue<int8_t, 1, 10> value;
    
    // int8_t min is -128, so min representable value is -128/10 = -12.8
    value.setValue(-20.0f);  // Too small
    EXPECT_EQ(value.getRaw(), -128);
    EXPECT_FLOAT_EQ(value.getValue(), -12.8f);
}

// Test unsigned type clamping
TEST(FixedPointTest, UnsignedClamping) {
    ScaledValue<uint8_t, 1, 10> value;
    
    // uint8_t min is 0
    value.setValue(-5.0f);  // Negative value
    EXPECT_EQ(value.getRaw(), 0);
    EXPECT_FLOAT_EQ(value.getValue(), 0.0f);
    
    // uint8_t max is 255
    value.setValue(30.0f);  // Too large
    EXPECT_EQ(value.getRaw(), 255);
    EXPECT_FLOAT_EQ(value.getValue(), 25.5f);
}

// Test raw value access
TEST(FixedPointTest, RawValueAccess) {
    ScaledValue<int16_t, 1, 10> value;
    
    value.setRaw(100);
    EXPECT_EQ(value.getRaw(), 100);
    EXPECT_FLOAT_EQ(value.getValue(), 10.0f);
}

// Test implicit conversions
TEST(FixedPointTest, ImplicitConversions) {
    ScaledValue<int16_t, 1, 10> value;
    
    // Implicit assignment from float
    value = 7.5f;
    EXPECT_EQ(value.getRaw(), 75);
    
    // Implicit conversion to float
    float result = value;
    EXPECT_FLOAT_EQ(result, 7.5f);
}

// Test various data type combinations
TEST(FixedPointTest, VariousDataTypes) {
    // Small signed type
    FixedPoint<int8_t, 2> int8Value;
    int8Value = 10.0f;
    EXPECT_EQ(int8Value.getRaw(), 20);
    
    // Small unsigned type
    FixedPoint<uint8_t, 100> uint8Value;
    uint8Value = 2.5f;
    EXPECT_EQ(uint8Value.getRaw(), 250);
}

// Test fractional scale factors
TEST(FixedPointTest, FractionalScaleFactors) {
    ScaledValue<int16_t, 1, 2> halfScale;
    halfScale = 100.0f;
    EXPECT_EQ(halfScale.getRaw(), 200);
    EXPECT_FLOAT_EQ(halfScale.getValue(), 100.0f);
    
    ScaledValue<int16_t, 1, 4> quarterScale;
    quarterScale = 200.0f;
    EXPECT_EQ(quarterScale.getRaw(), 800);
    EXPECT_FLOAT_EQ(quarterScale.getValue(), 200.0f);
    
    ScaledValue<int16_t, 7, 2> mixedScale;
    mixedScale = 35.0f;
    EXPECT_EQ(mixedScale.getRaw(), 10);
    EXPECT_NEAR(mixedScale.getValue(), 35.0f, 0.01f);
}

// Test edge cases with exact limits
TEST(FixedPointTest, ExactLimits) {
    ScaledValue<int16_t, 1, 1> value;
    
    // Test exact max
    value.setValue(static_cast<float>(std::numeric_limits<int16_t>::max()));
    EXPECT_EQ(value.getRaw(), std::numeric_limits<int16_t>::max());
    
    // Test exact min
    value.setValue(static_cast<float>(std::numeric_limits<int16_t>::min()));
    EXPECT_EQ(value.getRaw(), std::numeric_limits<int16_t>::min());
}

// Test that getValue/setValue are inverse operations
TEST(FixedPointTest, InverseOperations) {
    ScaledValue<int16_t, 1, 10> value;
    
    float testValues[] = {0.0f, 1.0f, 5.5f, -3.2f, 10.0f, -10.0f};
    
    for (float testValue : testValues) {
        value.setValue(testValue);
        float retrieved = value.getValue();
        // Allow for rounding error (1 LSB in scaled representation)
        EXPECT_NEAR(retrieved, testValue, 0.1f);
    }
}
