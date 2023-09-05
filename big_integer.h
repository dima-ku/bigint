#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

struct big_integer {
  big_integer();
  big_integer(const big_integer& other);
  big_integer(int a);
  big_integer(unsigned a);
  big_integer(long a);
  big_integer(unsigned long a);
  big_integer(long long a);
  big_integer(unsigned long long a);
  explicit big_integer(const std::string& str);

  ~big_integer();

  big_integer& operator=(const big_integer& other);

  big_integer& operator+=(const big_integer& rhs);
  big_integer& operator-=(const big_integer& rhs);
  big_integer& operator*=(const big_integer& rhs);
  big_integer& operator/=(const big_integer& rhs);
  big_integer& operator%=(const big_integer& rhs);

  big_integer& operator&=(const big_integer& rhs);
  big_integer& operator|=(const big_integer& rhs);
  big_integer& operator^=(const big_integer& rhs);

  big_integer& operator<<=(int rhs);
  big_integer& operator>>=(int rhs);

  big_integer operator+() const;
  big_integer operator-() const;
  big_integer operator~() const;

  big_integer& operator++();
  big_integer operator++(int);

  big_integer& operator--();
  big_integer operator--(int);

  friend bool operator==(const big_integer& a, const big_integer& b);
  friend bool operator!=(const big_integer& a, const big_integer& b);
  friend bool operator<(const big_integer& a, const big_integer& b);
  friend bool operator>(const big_integer& a, const big_integer& b);
  friend bool operator<=(const big_integer& a, const big_integer& b);
  friend bool operator>=(const big_integer& a, const big_integer& b);

  friend std::string to_string(const big_integer& a);

  big_integer& division(const big_integer& constRhs, bool needReminder = false);

private:
  size_t size() const;

  static uint32_t adc(uint32_t& first, uint32_t second, uint32_t carry);
  uint32_t get(size_t index) const;
  void normalize();
  static uint32_t sbb(uint32_t& first, uint32_t second, uint32_t carry);
  bool biggerOrEqualByAbs(const big_integer& other, size_t k = 0);
  void mulShort(uint32_t rhs);
  void negate();

  void difference(const big_integer& rhs, size_t k = 0);
  void adding(const big_integer& rhs, size_t k = 0);

  uint32_t divideShort(uint32_t rhs);
  void binaryInverse();

  big_integer& bitwise(const big_integer& rhs, uint32_t (*op)(uint32_t, uint32_t));

  big_integer& addingPrimitive(uint32_t rhs);
  big_integer& differencePrimitive(uint32_t rhs);

  void swap(big_integer& other);

private:
  std::vector<uint32_t> digits;
  bool sign;
};

big_integer operator+(big_integer a, const big_integer& b);
big_integer operator-(big_integer a, const big_integer& b);
big_integer operator*(big_integer a, const big_integer& b);
big_integer operator/(big_integer a, const big_integer& b);
big_integer operator%(big_integer a, const big_integer& b);

big_integer operator&(big_integer a, const big_integer& b);
big_integer operator|(big_integer a, const big_integer& b);
big_integer operator^(big_integer a, const big_integer& b);

big_integer operator<<(big_integer a, int b);
big_integer operator>>(big_integer a, int b);

bool operator==(const big_integer& a, const big_integer& b);
bool operator!=(const big_integer& a, const big_integer& b);
bool operator<(const big_integer& a, const big_integer& b);
bool operator>(const big_integer& a, const big_integer& b);
bool operator<=(const big_integer& a, const big_integer& b);
bool operator>=(const big_integer& a, const big_integer& b);

std::string to_string(const big_integer& a);
std::ostream& operator<<(std::ostream& s, const big_integer& a);
