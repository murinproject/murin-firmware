#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" {
#include "esp_err.h"
#include "flash_storage.h"
}

namespace {

constexpr const char *kStorageFile = "flash_storage_gtest.txt";

class FlashStorageTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    std::remove(kStorageFile);
    flash_storage_init();
  }

  void TearDown() override { std::remove(kStorageFile); }
};

TEST_F(FlashStorageTest, UsesDefaultsWhenValuesAreMissing)
{
  EXPECT_TRUE(flash_storage_get_telemetry_enabled(true));
  EXPECT_FALSE(flash_storage_get_telemetry_enabled(false));
  EXPECT_EQ(flash_storage_get_total_runtime_ms(), 0u);
}

TEST_F(FlashStorageTest, StoresAndLoadsTypedValues)
{
  EXPECT_EQ(flash_storage_set_telemetry_enabled(false), ESP_OK);
  EXPECT_FALSE(flash_storage_get_telemetry_enabled(true));

  EXPECT_EQ(flash_storage_set_total_runtime_ms(9876543210123ULL), ESP_OK);
  EXPECT_EQ(flash_storage_get_total_runtime_ms(), 9876543210123ULL);

  uint8_t enabled = 0;
  size_t enabled_size = sizeof(enabled);
  EXPECT_EQ(flash_storage_get(FLASH_STORAGE_ITEM_TELEMETRY_ENABLED, &enabled, &enabled_size), ESP_OK);
  EXPECT_EQ(enabled, 0u);
  EXPECT_EQ(enabled_size, sizeof(enabled));
}

TEST_F(FlashStorageTest, StoresAndLoadsBlobAndReportsItsSize)
{
  const uint8_t expected[] = {0x00, 0xAA, 0x1B, 0xFF};
  ASSERT_EQ(flash_storage_set(FLASH_STORAGE_ITEM_SHELL_HISTORY, expected, sizeof(expected)), ESP_OK);

  uint8_t actual[sizeof(expected)]{};
  size_t actual_size = sizeof(actual);
  EXPECT_EQ(flash_storage_get(FLASH_STORAGE_ITEM_SHELL_HISTORY, actual, &actual_size), ESP_OK);
  EXPECT_EQ(actual_size, sizeof(expected));
  EXPECT_EQ(std::memcmp(actual, expected, sizeof(expected)), 0);
}

TEST_F(FlashStorageTest, RejectsInvalidArguments)
{
  uint8_t value = 0;
  size_t size = sizeof(value);
  EXPECT_EQ(flash_storage_get(FLASH_STORAGE_ITEM_TELEMETRY_ENABLED, &value, nullptr), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(flash_storage_get(FLASH_STORAGE_ITEM_COUNT, &value, &size), ESP_ERR_INVALID_SIZE);
  EXPECT_EQ(flash_storage_get(FLASH_STORAGE_ITEM_TELEMETRY_ENABLED, nullptr, &size), ESP_ERR_INVALID_SIZE);
  EXPECT_EQ(flash_storage_get(FLASH_STORAGE_ITEM_TELEMETRY_ENABLED, &value, &size), ESP_ERR_NVS_NOT_FOUND);

  EXPECT_EQ(flash_storage_set(FLASH_STORAGE_ITEM_COUNT, &value, sizeof(value)), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(flash_storage_set(FLASH_STORAGE_ITEM_TELEMETRY_ENABLED, nullptr, sizeof(value)), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(flash_storage_set(FLASH_STORAGE_ITEM_TELEMETRY_ENABLED, &value, 0), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(flash_storage_set(FLASH_STORAGE_ITEM_TELEMETRY_ENABLED, &value, sizeof(uint16_t)), ESP_ERR_INVALID_SIZE);
}

TEST_F(FlashStorageTest, PersistsValuesInTextFile)
{
  const uint64_t runtime = 42;
  ASSERT_EQ(flash_storage_set_total_runtime_ms(runtime), ESP_OK);

  FILE *file = std::fopen(kStorageFile, "r");
  ASSERT_NE(file, nullptr);
  char contents[256]{};
  ASSERT_NE(std::fgets(contents, sizeof(contents), file), nullptr);
  std::fclose(file);
  EXPECT_NE(std::strstr(contents, "runtime_ms|q|"), nullptr);

  EXPECT_EQ(flash_storage_get_total_runtime_ms(), runtime);
}

} // namespace
