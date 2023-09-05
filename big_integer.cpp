#include "big_integer.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

static constexpr size_t MX_SIZE_T = std::numeric_limits<size_t>::max();
static constexpr uint32_t MX_UINT32_T = std::numeric_limits<uint32_t>::max();
static constexpr uint32_t CNT_BIT_IN_DIGIT = 32;
static constexpr uint64_t BASE64 = static_cast<uint64_t>(1) << CNT_BIT_IN_DIGIT; // база системы счисления в uint64_t
static const big_integer ZERO;

big_integer::big_integer() : sign(true) {}

big_integer::big_integer(const big_integer& other) = default;

big_integer::big_integer(int a) : big_integer(static_cast<long long>(a)) {}

big_integer::big_integer(unsigned a) : big_integer(static_cast<unsigned long long>(a)) {}

big_integer::big_integer(long a) : big_integer(static_cast<long long>(a)) {}

big_integer::big_integer(unsigned long a) : big_integer(static_cast<unsigned long long>(a)) {}

big_integer::big_integer(long long a) : digits({}), sign(a >= 0) {
  if (a == std::numeric_limits<long long>::min()) {
    // abs(a) = 2^63
    digits = {0, static_cast<uint32_t>(1) << (CNT_BIT_IN_DIGIT - 1)};
  } else {
    digits = {static_cast<uint32_t>(std::abs(a) % BASE64), static_cast<uint32_t>(std::abs(a) / BASE64)};
  }
  normalize();
}

big_integer::big_integer(unsigned long long a)
    : digits({static_cast<uint32_t>(static_cast<uint64_t>(a) % BASE64),
              static_cast<uint32_t>(static_cast<uint64_t>(a) / BASE64)}),
      sign(true) {
  normalize();
}

big_integer::big_integer(const std::string& str) {
  if (str.empty()) {
    throw std::invalid_argument("we donow how to parse empty string :( ");
  }
  constexpr size_t SZ = 9;
  constexpr size_t MODULO = 1e9;
  const uint32_t start = str[0] == '-' || str[0] == '+';
  if (std::any_of(str.begin() + start, str.end(), [](char c) { return c < '0' || c > '9'; })) {
    throw std::invalid_argument("we donow how to parse non-digit symbol in this case: \"" + str + '\"');
  }
  for (size_t i = start; i < str.size(); i += SZ) {
    mulShort((i + SZ < str.size()) ? MODULO : static_cast<uint32_t>(std::pow(10, str.size() - i)));
    uint32_t tmp = 0;
    std::from_chars(str.data() + i, str.data() + std::min(i + 9, str.size()), tmp);
    addingPrimitive(tmp);
  }

  if (str.size() == 1 && (str[0] == '-' || str[0] == '+')) {
    throw std::invalid_argument("we donow how to parse +/- :( \"" + str + '\"');
  } else if (str[0] == '-') {
    sign = false;
  }
  normalize();
}

big_integer::~big_integer() = default;

big_integer& big_integer::operator=(const big_integer& other) {
  if (this == &other) {
    return *this;
  }
  big_integer tmp(other);
  swap(tmp);
  return *this;
}

big_integer& big_integer::operator+=(const big_integer& rhs) {
  if (rhs == ZERO) {
    return *this;
  } else if (sign == rhs.sign) {
    adding(rhs);
  } else {
    bool resultSign = biggerOrEqualByAbs(rhs) == sign;
    difference(rhs);
    sign = resultSign;
  }
  normalize();
  return *this;
}

big_integer& big_integer::operator-=(const big_integer& rhs) {
  if (rhs == ZERO) {
    return *this;
  } else if (sign == rhs.sign) {
    bool resultSign = biggerOrEqualByAbs(rhs) == sign;
    difference(rhs);
    sign = resultSign;
  } else {
    adding(rhs);
  }
  normalize();
  return *this;
}

big_integer& big_integer::operator*=(const big_integer& rhs) {
  bool resultSign = (sign == rhs.sign);
  if (rhs == ZERO) {
    *this = ZERO;
    return *this;
  } else if (rhs.size() == 1) {
    mulShort(rhs.digits.back());
  } else {
    size_t backUpSize = size();
    digits.resize(size() + rhs.size() + 1, 0);
    for (size_t start = backUpSize - 1; start != MX_SIZE_T; start--) {
      uint32_t carryMul = 0;
      uint32_t carryAdd = 0;
      const uint64_t castedCurrent = static_cast<uint64_t>(get(start));
      digits[start] = 0;
      for (int i = 0; i <= rhs.size(); i++) {
        uint64_t realValue = castedCurrent * rhs.get(i) + carryMul;
        uint32_t tmp = realValue % BASE64;
        carryMul = realValue / BASE64;
        carryAdd = adc(digits[start + i], tmp, carryAdd);
      }
      if (carryAdd != 0) {
        carryAdd = adc(digits[start + rhs.size() + 1], 0, carryAdd);
      }
      assert(carryAdd == 0 && carryMul == 0);
    }
  }
  sign = resultSign;
  normalize();
  return *this;
}

big_integer& big_integer::operator/=(const big_integer& constRhs) {
  division(constRhs, false);
  return *this;
}

big_integer& big_integer::operator%=(const big_integer& rhs) {
  division(rhs, true);
  return *this;
}

big_integer& big_integer::bitwise(const big_integer& rhs, uint32_t (*op)(uint32_t, uint32_t)) {
  bool weInverse = !sign, rhsInverse = !rhs.sign, c1 = !sign, c2 = !rhs.sign;
  digits.resize(std::max(size(), rhs.size()), 0);
  for (size_t i = 0; i < size(); i++) {
    uint32_t ourDigit = weInverse ? (~get(i)) : get(i);
    if (ourDigit != MX_UINT32_T && c1) {
      ourDigit++;
      c1 = false;
    } else if (c1) {
      ourDigit = 0;
    }
    uint32_t rhsDigit = rhsInverse ? (~rhs.get(i)) : rhs.get(i);

    if (rhsDigit != MX_UINT32_T && c2) {
      rhsDigit++;
      c2 = false;
    } else if (c2) {
      rhsDigit = 0;
    }
    digits[i] = op(ourDigit, rhsDigit);
  }
  if (op(!sign, !rhs.sign)) {
    sign = false;
    binaryInverse();
  } else {
    sign = true;
  }
  normalize();
  return *this;
}

uint32_t myXor(uint32_t f, uint32_t s) {
  return f ^ s;
}

uint32_t myOr(uint32_t f, uint32_t s) {
  return f | s;
}

uint32_t myAnd(uint32_t f, uint32_t s) {
  return f & s;
}

big_integer& big_integer::operator&=(const big_integer& constRhs) {
  return bitwise(constRhs, myAnd);
}

big_integer& big_integer::operator|=(const big_integer& constRhs) {
  return bitwise(constRhs, myOr);
}

big_integer& big_integer::operator^=(const big_integer& constRhs) {
  return bitwise(constRhs, myXor);
}

big_integer& big_integer::operator<<=(int rhs) {
  mulShort(1 << (rhs % CNT_BIT_IN_DIGIT));
  digits.insert(digits.begin(), rhs / CNT_BIT_IN_DIGIT, 0);
  normalize();
  return *this;
}

big_integer& big_integer::operator>>=(int rhs) {
  if (rhs / CNT_BIT_IN_DIGIT >= size()) {
    *this = ZERO;
    return *this;
  }
  bool haveNonZero =
      std::any_of(digits.begin(), digits.begin() + rhs / CNT_BIT_IN_DIGIT, [](uint32_t digit) { return digit != 0; });
  digits.erase(digits.begin(), digits.begin() + (rhs / CNT_BIT_IN_DIGIT));
  if (!sign && haveNonZero) {
    differencePrimitive(1);
  }
  bool tmp = sign;
  uint32_t r = divideShort(static_cast<uint32_t>(1) << (rhs & (CNT_BIT_IN_DIGIT - 1)));
  if (!tmp && r != 0) {
    differencePrimitive(1);
  }
  normalize();
  return *this;
}

big_integer big_integer::operator+() const {
  return *this;
}

big_integer big_integer::operator-() const {
  big_integer result = *this;
  result.negate();
  result.normalize();
  return result;
}

big_integer big_integer::operator~() const {
  big_integer tmp = *this;
  tmp.sign ^= 1;
  tmp.differencePrimitive(1);
  return tmp;
}

big_integer& big_integer::operator++() {
  addingPrimitive(1);
  return *this;
}

big_integer big_integer::operator++(int) {
  big_integer result = *this;
  addingPrimitive(1);
  return result;
}

big_integer& big_integer::operator--() {
  differencePrimitive(1);
  return *this;
}

big_integer big_integer::operator--(int) {
  big_integer result = *this;
  differencePrimitive(1);
  return result;
}

big_integer operator+(big_integer a, const big_integer& b) {
  return a += b;
}

big_integer operator-(big_integer a, const big_integer& b) {
  return a -= b;
}

big_integer operator*(big_integer a, const big_integer& b) {
  return a *= b;
}

big_integer operator/(big_integer a, const big_integer& b) {
  a.division(b, false);
  return a;
}

big_integer operator%(big_integer a, const big_integer& b) {
  a.division(b, true);
  return a;
}

big_integer operator&(big_integer a, const big_integer& b) {
  return a &= b;
}

big_integer operator|(big_integer a, const big_integer& b) {
  return a |= b;
}

big_integer operator^(big_integer a, const big_integer& b) {
  return a ^= b;
}

big_integer operator<<(big_integer a, int b) {
  return a <<= b;
}

big_integer operator>>(big_integer a, int b) {
  return a >>= b;
}

bool operator==(const big_integer& a, const big_integer& b) {
  return a.sign == b.sign && a.digits == b.digits;
}

bool operator!=(const big_integer& a, const big_integer& b) {
  return !(a == b);
}

bool operator<(const big_integer& a, const big_integer& b) {
  if (a.sign != b.sign) {
    return !a.sign;
  }
  if (a.size() != b.size()) {
    return a.sign ? (a.size() < b.size()) : (a.size() > b.size());
  }
  for (size_t i = a.size() - 1; i != MX_SIZE_T; --i) {
    if (a.digits[i] < b.digits[i]) {
      return a.sign;
    }
    if (a.digits[i] > b.digits[i]) {
      return !a.sign;
    }
  }
  return false;
}

bool operator>(const big_integer& a, const big_integer& b) {
  return b < a;
}

bool operator<=(const big_integer& a, const big_integer& b) {
  return !(a > b);
}

bool operator>=(const big_integer& a, const big_integer& b) {
  return !(a < b);
}

std::string to_string(const big_integer& constA) {
  if (constA == ZERO) {
    return "0";
  }
  big_integer a = constA;
  bool flag = constA.sign;
  std::string result;
  a.sign = true;
  while (a > 0) {
    uint32_t r = a.divideShort(10);
    result += static_cast<char>(r + static_cast<int>('0'));
  }
  if (!flag) {
    result += '-';
  }
  std::reverse(result.begin(), result.end());
  return result;
}

std::ostream& operator<<(std::ostream& s, const big_integer& a) {
  return s << to_string(a);
}

size_t big_integer::size() const {
  return digits.size();
}

uint32_t big_integer::adc(uint32_t& first, uint32_t second, uint32_t carry) {
  uint64_t tmp = static_cast<uint64_t>(first) + second + carry;
  first = tmp & (BASE64 - 1);
  return tmp >> CNT_BIT_IN_DIGIT;
}

uint32_t big_integer::get(size_t index) const {
  return index < size() ? digits[index] : 0;
}

void big_integer::normalize() {
  while (size() > 0 && digits.back() == 0) {
    digits.pop_back();
  }
  if (size() == 0) {
    sign = true;
  }
}

uint32_t big_integer::sbb(uint32_t& first, uint32_t second, uint32_t carry) {
  uint64_t tmp = static_cast<uint64_t>(first) - second - carry;
  first = static_cast<uint32_t>(tmp);
  return tmp != first;
}

bool big_integer::biggerOrEqualByAbs(const big_integer& other, size_t k) {
  if (size() != other.size() + k) {
    return size() > other.size() + k;
  }

  uint32_t tmp = 0;
  for (size_t i = size() - 1; i != MX_SIZE_T; --i) {
    tmp = (i < k) ? 0 : other.get(i - k);
    if (digits[i] > tmp) {
      return true;
    }
    if (digits[i] < tmp) {
      return false;
    }
  }
  return true;
}

void big_integer::mulShort(uint32_t rhs) {
  uint32_t carry = 0;
  uint64_t realValue = 0;
  const uint64_t castedRhs = static_cast<uint64_t>(rhs);

  digits.resize(size() + 1, 0);

  for (size_t i = 0; i < size(); i++) {
    realValue = castedRhs * get(i) + carry;
    digits[i] = realValue % BASE64;
    carry = realValue / BASE64;
  }

  assert(carry == 0);
  normalize();
}

void big_integer::negate() {
  sign ^= 1;
  normalize();
}

uint32_t big_integer::divideShort(uint32_t rhs) {
  assert(rhs != 0);

  uint32_t carry = 0;
  uint64_t realValue = 0;
  for (size_t i = size() - 1; i != MX_SIZE_T; i--) {
    realValue = BASE64 * carry + digits[i];
    digits[i] = (realValue / rhs);
    carry = realValue % rhs;
  }
  normalize();
  return carry;
}

void big_integer::difference(const big_integer& rhs, size_t k) {
  bool biggerByAbs = biggerOrEqualByAbs(rhs);
  digits.resize(std::max(size(), rhs.size()), 0);
  uint32_t carry = 0;
  for (size_t i = k; i < size(); i++) {
    uint32_t f = get(i);
    uint32_t s = rhs.get(i - k);
    if (!biggerByAbs) {
      std::swap(f, s);
    }
    carry = sbb(f, s, carry);
    digits[i] = f;
  }
  assert(carry == 0);
  normalize();
}

void big_integer::adding(const big_integer& rhs, size_t k) {
  digits.resize(std::max(size(), rhs.size() + k) + 1, 0);
  uint32_t carry = 0;
  for (size_t i = k; i < size(); i++) {
    carry = adc(digits[i], rhs.get(i - k), carry);
  }
  normalize();
}

void big_integer::binaryInverse() {
  for (size_t i = 0; i < size(); i++) {
    digits[i] = digits[i] ^ MX_UINT32_T;
  }
  differencePrimitive(1);
  normalize();
}

big_integer& big_integer::addingPrimitive(uint32_t rhs) {
  if (rhs == 0) {
    return *this;
  }
  if (size() == 0) {
    sign = true;
    digits.push_back(rhs);
  } else if (size() == 1 && !sign) {
    digits[0] = std::max(digits[0], rhs) - std::min(digits[0], rhs);
    sign = (rhs >= digits[0]);
  } else {
    digits.resize(size() + 1, 0);
    uint32_t carry = 0;
    for (size_t i = 0; i < size(); i++) {
      carry = sign ? adc(digits[i], rhs * (i == 0), carry) : sbb(digits[i], rhs * (i == 0), carry);
    }
    assert(carry == 0);
  }
  normalize();
  return *this;
}

big_integer& big_integer::differencePrimitive(uint32_t rhs) {
  if (rhs == 0) {
    return *this;
  }
  if (size() == 0) {
    digits.push_back(rhs);
    sign = false;
  } else if (size() == 1 && sign) {
    digits[0] = std::max(digits[0], rhs) - std::min(digits[0], rhs);
    sign = (digits[0] >= rhs);
  } else {
    digits.resize(size() + 1, 0);
    uint32_t carry = 0;
    for (size_t i = 0; i < size(); i++) {
      carry = !sign ? adc(digits[i], rhs * (i == 0), carry) : sbb(digits[i], rhs * (i == 0), carry);
    }
    assert(carry == 0);
  }
  normalize();
  return *this;
}

void big_integer::swap(big_integer& other) {
  std::swap(sign, other.sign);
  std::swap(digits, other.digits);
}

big_integer& big_integer::division(const big_integer& constRhs, bool needReminder) {
  big_integer rhs = constRhs;
  bool resultSign = (sign == constRhs.sign);
  bool backUpSign = sign;
  sign = true;
  rhs.sign = true;
  if (rhs == 0) {
    throw std::invalid_argument("division by zero :(");
  }
  big_integer R;
  if (*this < rhs) {
    if (needReminder) {
      sign = backUpSign;
    } else {
      *this = ZERO;
    }
  } else if (rhs.size() == 1) {
    uint32_t rem = divideShort(rhs.digits.back());
    if (!needReminder) {
      sign = resultSign;
    } else {
      *this = big_integer(rem);
      sign = backUpSign;
    }
  } else {
    size_t delta = size() - rhs.size();
    big_integer result;
    result.digits.resize(delta + 1, 0);
    uint32_t f = BASE64 / (static_cast<uint64_t>(rhs.digits.back()) + 1); // литерали как кнут в книге прописал
    mulShort(f);
    rhs.mulShort(f);
    for (size_t k = delta; k != MX_SIZE_T; k--) {
      uint64_t r2 = static_cast<uint64_t>(get(rhs.size() + k)) * BASE64 + get(rhs.size() + k - 1);
      uint64_t d1 = static_cast<uint64_t>(rhs.get(rhs.size() - 1));

      assert(d1 != 0);

      uint32_t expected =
          static_cast<uint32_t>(std::min(r2 / d1,
                                         static_cast<uint64_t>(MX_UINT32_T))); // MX_UINT32_T == BASE64 - 1

      big_integer tmp = rhs;
      tmp.mulShort(expected);
      while (!biggerOrEqualByAbs(tmp, k)) {
        expected--;
        tmp -= rhs;
      }
      difference(tmp, k);
      result.digits[k] = expected;
    }
    if (!needReminder) {
      result.sign = resultSign;
      result.normalize();
      swap(result);
    } else {
      divideShort(f);
      sign = backUpSign;
    }
  }
  normalize();
  return *this;
}
