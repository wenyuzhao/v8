// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "src/bigint/bigint-internal.h"
#include "src/bigint/util.h"

namespace v8 {
namespace bigint {
namespace test {

int PrintHelp(char** argv) {
  std::cerr << "Usage:\n"
            << argv[0] << " --help\n"
            << "    Print this help and exit.\n"
            << argv[0] << " --list\n"
            << "    List supported tests.\n"
            << argv[0] << " <testname>\n"
            << "    Run the specified test (see --list for a list).\n"
            << "\nOptions when running tests:\n"
            << "--random-seed R\n"
            << "    Initialize the random number generator with this seed.\n"
            << "--runs N\n"
            << "    Repeat the test N times.\n";
  return 1;
}

#define TESTS(V) V(kKaratsuba, "karatsuba")

enum Operation { kNoOp, kList, kTest };

enum Test {
#define TEST(kName, name) kName,
  TESTS(TEST)
#undef TEST
};

class RNG {
 public:
  RNG() = default;

  void Initialize(int64_t seed) {
    state0_ = MurmurHash3(static_cast<uint64_t>(seed));
    state1_ = MurmurHash3(~state0_);
    CHECK(state0_ != 0 || state1_ != 0);
  }

  uint64_t NextUint64() {
    XorShift128(&state0_, &state1_);
    return static_cast<uint64_t>(state0_ + state1_);
  }

  static inline void XorShift128(uint64_t* state0, uint64_t* state1) {
    uint64_t s1 = *state0;
    uint64_t s0 = *state1;
    *state0 = s0;
    s1 ^= s1 << 23;
    s1 ^= s1 >> 17;
    s1 ^= s0;
    s1 ^= s0 >> 26;
    *state1 = s1;
  }

  static uint64_t MurmurHash3(uint64_t h) {
    h ^= h >> 33;
    h *= uint64_t{0xFF51AFD7ED558CCD};
    h ^= h >> 33;
    h *= uint64_t{0xC4CEB9FE1A85EC53};
    h ^= h >> 33;
    return h;
  }

 private:
  uint64_t state0_;
  uint64_t state1_;
};

static constexpr int kCharsPerDigit = kDigitBits / 4;

static const char kConversionChars[] = "0123456789abcdef";

std::string FormatHex(Digits X) {
  X.Normalize();
  if (X.len() == 0) return "0";
  digit_t msd = X.msd();
  const int msd_leading_zeros = CountLeadingZeros(msd);
  const size_t bit_length = X.len() * kDigitBits - msd_leading_zeros;
  const size_t chars = DIV_CEIL(bit_length, 4);

  if (chars > 100000) {
    return std::string("<BigInt with ") + std::to_string(bit_length) +
           std::string(" bits>");
  }

  std::unique_ptr<char[]> result(new char[chars]);
  for (size_t i = 0; i < chars; i++) result[i] = '?';
  // Print the number into the string, starting from the last position.
  int pos = static_cast<int>(chars - 1);
  for (int i = 0; i < X.len() - 1; i++) {
    digit_t d = X[i];
    for (int j = 0; j < kCharsPerDigit; j++) {
      result[pos--] = kConversionChars[d & 15];
      d = static_cast<digit_t>(d >> 4u);
    }
  }
  while (msd != 0) {
    result[pos--] = kConversionChars[msd & 15];
    msd = static_cast<digit_t>(msd >> 4u);
  }
  CHECK(pos == -1);
  return std::string(result.get(), chars);
}

class Runner {
 public:
  Runner() = default;

  void Initialize() {
    rng_.Initialize(random_seed_);
    processor_.reset(Processor::New(new Platform()));
  }

  ProcessorImpl* processor() {
    return static_cast<ProcessorImpl*>(processor_.get());
  }

  int Run() {
    if (op_ == kList) {
      ListTests();
    } else if (op_ == kTest) {
      RunTest();
    } else {
      DCHECK(false);  // Unreachable.
    }
    return 0;
  }

  void ListTests() {
#define PRINT(kName, name) std::cout << name << "\n";
    TESTS(PRINT)
#undef PRINT
  }

  void AssertEquals(Digits input1, Digits input2, Digits expected,
                    Digits actual) {
    if (Compare(expected, actual) == 0) return;
    std::cerr << "Input 1:  " << FormatHex(input1) << "\n";
    std::cerr << "Input 2:  " << FormatHex(input2) << "\n";
    std::cerr << "Expected: " << FormatHex(expected) << "\n";
    std::cerr << "Actual:   " << FormatHex(actual) << "\n";
    error_ = true;
  }

  int RunTest() {
    int count = 0;
    if (test_ == kKaratsuba) {
      for (int i = 0; i < runs_; i++) {
        TestKaratsuba(&count);
      }
    } else {
      DCHECK(false);  // Unreachable.
    }
    if (error_) return 1;
    std::cout << count << " tests run, no error reported.\n";
    return 0;
  }

  void TestKaratsuba(int* count) {
    // Calling {MultiplyKaratsuba} directly is only valid if
    // left_size >= right_size and right_size >= kKaratsubaThreshold.
    for (int right_size = kKaratsubaThreshold;
         right_size <= 3 * kKaratsubaThreshold; right_size++) {
      for (int left_size = right_size; left_size <= 3 * kKaratsubaThreshold;
           left_size++) {
        ScratchDigits A(left_size);
        ScratchDigits B(right_size);
        int result_len = MultiplyResultLength(A, B);
        ScratchDigits result(result_len);
        ScratchDigits result_schoolbook(result_len);
        GenerateRandom(A);
        GenerateRandom(B);
        processor()->MultiplyKaratsuba(result, A, B);
        processor()->MultiplySchoolbook(result_schoolbook, A, B);
        AssertEquals(A, B, result_schoolbook, result);
        if (error_) return;
        (*count)++;
      }
    }
  }

  int ParseOptions(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--list") == 0) {
        op_ = kList;
      } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
        PrintHelp(argv);
        return 0;
      } else if (strcmp(argv[i], "--random-seed") == 0 ||
                 strcmp(argv[i], "--random_seed") == 0) {
        random_seed_ = std::stoi(argv[++i]);
      } else if (strncmp(argv[i], "--random-seed=", 14) == 0 ||
                 strncmp(argv[i], "--random_seed=", 14) == 0) {
        random_seed_ = std::stoi(argv[i] + 14);
      } else if (strcmp(argv[i], "--runs") == 0) {
        runs_ = std::stoi(argv[++i]);
      } else if (strncmp(argv[i], "--runs=", 7) == 0) {
        runs_ = std::stoi(argv[i] + 7);
      }
#define TEST(kName, name)                \
  else if (strcmp(argv[i], name) == 0) { \
    op_ = kTest;                         \
    test_ = kName;                       \
  }
      TESTS(TEST)
#undef TEST
      else {
        std::cerr << "Warning: ignored argument: " << argv[i] << "\n";
      }
    }
    if (op_ == kNoOp) return PrintHelp(argv);  // op is mandatory.
    return 0;
  }

 private:
  void GenerateRandom(RWDigits Z) {
    if (Z.len() == 0) return;
    if (sizeof(digit_t) == 8) {
      for (int i = 0; i < Z.len(); i++) {
        Z[i] = static_cast<digit_t>(rng_.NextUint64());
      }
    } else {
      for (int i = 0; i < Z.len(); i += 2) {
        uint64_t random = rng_.NextUint64();
        Z[i] = static_cast<digit_t>(random);
        if (i + 1 < Z.len()) Z[i + 1] = static_cast<digit_t>(random >> 32);
      }
    }
    // Special case: we don't want the MSD to be zero.
    while (Z.msd() == 0) {
      Z[Z.len() - 1] = static_cast<digit_t>(rng_.NextUint64());
    }
  }

  Operation op_{kNoOp};
  Test test_;
  bool error_{false};
  int runs_ = 1;
  int64_t random_seed_{314159265359};
  RNG rng_;
  std::unique_ptr<Processor, Processor::Destroyer> processor_;
};

}  // namespace test
}  // namespace bigint
}  // namespace v8

int main(int argc, char** argv) {
  v8::bigint::test::Runner runner;
  int ret = runner.ParseOptions(argc, argv);
  if (ret != 0) return ret;
  runner.Initialize();
  return runner.Run();
}
