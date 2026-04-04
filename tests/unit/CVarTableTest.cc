#include <gtest/gtest.h>
#include "cvar_core.h"
#include <cstdlib>
#include <cstring>

TEST(CVarTableTest, TableSizeMatchesEnumCount) {
    EXPECT_EQ(sizeof(kCVarTable) / sizeof(CVarDesc), (size_t)CVar::_Count);
}

TEST(CVarTableTest, AllEntriesHaveNonNullName) {
    for (int i = 0; i < (int)CVar::_Count; ++i) {
        ASSERT_NE(kCVarTable[i].name, nullptr)
            << "CVar[" << i << "] has null name";
        EXPECT_STRNE(kCVarTable[i].name, "")
            << "CVar[" << i << "] has empty name";
    }
}

TEST(CVarTableTest, RegisteredEntriesHaveDefaultValues) {
    for (int i = 0; i < (int)CVar::Version; ++i) {
        ASSERT_NE(kCVarTable[i].defaultVal, nullptr)
            << "CVar[" << i << "] (" << kCVarTable[i].name << ") has null default";
        EXPECT_STRNE(kCVarTable[i].defaultVal, "")
            << "CVar[" << i << "] (" << kCVarTable[i].name << ") has empty default";
    }
}

TEST(CVarTableTest, KnownCVarNamesMatch) {
    EXPECT_STREQ(kCVarTable[(int)CVar::Enabled].name, "paniniEnabled");
    EXPECT_STREQ(kCVarTable[(int)CVar::Fov].name, "paniniFov");
    EXPECT_STREQ(kCVarTable[(int)CVar::WowFov].name, "FoV");
    EXPECT_STREQ(kCVarTable[(int)CVar::Version].name, "ppVersion");
}

TEST(CVarTableTest, FovDefaultMatchesConstant) {
    float parsed = std::atof(kCVarTable[(int)CVar::Fov].defaultVal);
    EXPECT_FLOAT_EQ(parsed, 2.82f);
}
