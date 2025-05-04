#include "buffer.cpp"
#include <cassert>
#include <cstring>
#include <iostream>

// Helper function to print test results
void runTest(const char *testName, bool passed) {
  std::cout << testName << ": " << (passed ? "PASSED" : "FAILED") << std::endl;
}

// Test initialization of buffer
void testInitialization() {
  const size_t size = 1024;
  Buffer buffer(size);

  bool passed = (buffer.data_size() == 0) && (buffer.free_space() == size) &&
                (buffer.data_start == buffer.start) &&
                (buffer.data_end == buffer.start);

  runTest("Initialization", passed);
}

// Test appending data
void testAppendData() {
  const size_t size = 1024;
  Buffer buffer(size);

  uint8_t testData[] = {1, 2, 3, 4, 5};
  buffer.append(testData, sizeof(testData));

  bool passed = (buffer.data_size() == sizeof(testData)) &&
                (buffer.free_space() == size - sizeof(testData));

  // Check that data was copied correctly
  for (size_t i = 0; i < sizeof(testData) && passed; i++) {
    if (buffer.data_start[i] != testData[i]) {
      passed = false;
    }
  }

  runTest("Append Data", passed);
}

// Test consuming data
void testConsumeData() {
  const size_t size = 1024;
  Buffer buffer(size);

  uint8_t testData[] = {1, 2, 3, 4, 5};
  buffer.append(testData, sizeof(testData));

  // Consume first 2 bytes
  buffer.consume(2);

  bool passed = (buffer.data_size() == sizeof(testData) - 2) &&
                (buffer.data_start[0] ==
                 3); // First element should now be the third original element

  // Consume more bytes than available
  size_t oldSize = buffer.data_size();
  buffer.consume(10);
  passed = passed && (buffer.data_size() == oldSize); // Should not change

  runTest("Consume Data", passed);
}

// Test compacting buffer
void testCompactBuffer() {
  const size_t size = 1024;
  Buffer buffer(size);

  uint8_t testData[] = {1, 2, 3, 4, 5};
  buffer.append(testData, sizeof(testData));

  // Consume some data to move data_start
  buffer.consume(2);

  // Before compacting
  uint8_t *oldDataStart = buffer.data_start;

  // Compact buffer
  buffer.compact();

  // After compacting
  bool passed = (buffer.data_start == buffer.start) &&
                (buffer.data_size() == sizeof(testData) - 2) &&
                (buffer.data_start[0] == 3) && (buffer.data_start[1] == 4) &&
                (buffer.data_start[2] == 5);

  runTest("Compact Buffer", passed);
}

// Test expanding buffer
void testExpandBuffer() {
  const size_t initialSize = 10;
  Buffer buffer(initialSize);

  uint8_t testData[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  buffer.append(testData, sizeof(testData));

  // Try to append more data than the remaining space
  uint8_t moreData[8] = {9, 10, 11, 12, 13, 14, 15, 16};
  buffer.append(moreData, sizeof(moreData));

  // Check that expansion worked and all data is present
  bool passed = (buffer.data_size() == sizeof(testData) + sizeof(moreData)) &&
                (buffer.max_size > initialSize);

  // Verify the data
  for (size_t i = 0; i < sizeof(testData) && passed; i++) {
    if (buffer.data_start[i] != testData[i]) {
      passed = false;
    }
  }

  for (size_t i = 0; i < sizeof(moreData) && passed; i++) {
    if (buffer.data_start[sizeof(testData) + i] != moreData[i]) {
      passed = false;
    }
  }

  runTest("Expand Buffer", passed);
}

// Test edge cases
void testEdgeCases() {
  // Test empty buffer
  Buffer emptyBuffer(0);
  bool passed = (emptyBuffer.data_size() == 0);

  // Test appending zero bytes
  Buffer buffer(1024);

  uint8_t data[] = {1, 2, 3};
  buffer.append(data, 0);
  passed = passed && (buffer.data_size() == 0);

  // Test consuming zero bytes
  buffer.append(data, sizeof(data));
  size_t oldSize = buffer.data_size();
  buffer.consume(0);
  passed = passed && (buffer.data_size() == oldSize);

  runTest("Edge Cases", passed);
}

// Test the fragmentation scenario
void testFragmentation() {
  const size_t size = 10;
  Buffer buffer(size);

  // Add 4 bytes
  uint8_t data1[] = {1, 2, 3, 4};
  buffer.append(data1, sizeof(data1));

  // Consume 3 bytes to fragment
  buffer.consume(3);

  // Now we have 9 total free bytes, but only 6 contiguous at the end
  // Try to append 7 bytes (should trigger compact first)
  uint8_t data2[] = {5, 6, 7, 8, 9, 10, 11};
  buffer.append(data2, sizeof(data2));

  // Should compact then append successfully
  bool passed = (buffer.data_size() ==
                 1 + sizeof(data2)) && // 1 remaining byte + 7 new bytes
                (buffer.data_start == buffer.start) &&
                (buffer.data_start[0] == 4) && // The one remaining byte
                (buffer.data_start[1] == 5);   // First of the new bytes

  runTest("Fragmentation Handling", passed);
}

int main() {
  std::cout << "Running Buffer Tests:" << std::endl;

  testInitialization();
  testAppendData();
  testConsumeData();
  testCompactBuffer();
  testExpandBuffer();
  testEdgeCases();
  testFragmentation();

  std::cout << "All tests completed." << std::endl;
  return 0;
}
