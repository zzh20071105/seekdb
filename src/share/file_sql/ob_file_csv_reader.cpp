/**
 * Copyright (c) 2025 OceanBase.
 * Licensed under the Apache License, Version 2.0.
 */

#define USING_LOG_PREFIX SHARE
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "share/file_sql/ob_file_csv_reader.h"

namespace oceanbase
{
namespace share
{
namespace file_sql
{

using namespace common;

ObFileCsvOptions::ObFileCsvOptions()
  : has_header_(true),
    separators_set_(),
    field_delimiter_(','),
    line_delimiter_('\n'),
    ignore_blank_lines_(false),
    csv_quote_escape_(true),
    csv_tab_escape_(false)
{
}

void ObFileCsvOptions::reset()
{
  has_header_ = true;
  separators_set_.reset();
  field_delimiter_ = ',';
  line_delimiter_ = '\n';
  ignore_blank_lines_ = false;
  csv_quote_escape_ = true;
  csv_tab_escape_ = false;
}

ObCsvTokenizer::ObCsvTokenizer(common::ObIAllocator &alloc)
  : alloc_(alloc),
    buf_(nullptr),
    capacity_(0)
{
}

ObCsvTokenizer::~ObCsvTokenizer()
{
  if (nullptr != buf_) {
    alloc_.free(buf_);
    buf_ = nullptr;
  }
  capacity_ = 0;
}

int ObCsvTokenizer::ensure_capacity(const int64_t need)
{
  int ret = OB_SUCCESS;
  if (need > capacity_) {
    int64_t new_capacity = capacity_ > 0 ? capacity_ : 4096;
    while (new_capacity < need) {
      new_capacity *= 2;
    }
    char *new_buf = static_cast<char *>(alloc_.alloc(new_capacity));
    if (OB_ISNULL(new_buf)) {
      ret = OB_ALLOCATE_MEMORY_FAILED;
      LOG_WARN("failed to allocate csv tokenizer buffer", K(ret), K(new_capacity));
    } else {
      if (nullptr != buf_ && capacity_ > 0) {
        MEMCPY(new_buf, buf_, capacity_);
        alloc_.free(buf_);
      }
      buf_ = new_buf;
      capacity_ = new_capacity;
    }
  }
  return ret;
}

int ObCsvTokenizer::split_record(const common::ObString &record,
                                 common::ObIArray<common::ObString> &fields)
{
  int ret = OB_SUCCESS;
  fields.reset();
  const int64_t len = record.length();
  if (OB_FAIL(ensure_capacity(len + 1))) {
    LOG_WARN("failed to reserve buffer for record", K(ret), K(len));
  } else {
    if (len > 0) {
      MEMCPY(buf_, record.ptr(), len);
    }
    bool in_quotes = false;
    int64_t read_pos = 0;
    int64_t write_pos = 0;
    int64_t field_begin = 0;
    while (OB_SUCC(ret) && read_pos < len) {
      const char c = buf_[read_pos];
      if (in_quotes) {
        if ('"' == c) {
          if (read_pos + 1 < len && '"' == buf_[read_pos + 1]) {
            // a doubled quote inside a quoted field is a literal quote
            buf_[write_pos++] = '"';
            read_pos += 2;
          } else {
            in_quotes = false;
            ++read_pos;
          }
        } else {
          buf_[write_pos++] = c;
          ++read_pos;
        }
      } else if ('"' == c) {
        in_quotes = true;
        ++read_pos;
      } else if (',' == c) {
        if (OB_FAIL(fields.push_back(ObString(static_cast<int32_t>(write_pos - field_begin),
                                              buf_ + field_begin)))) {
          LOG_WARN("failed to append csv field", K(ret));
        } else {
          field_begin = write_pos;
          ++read_pos;
        }
      } else {
        buf_[write_pos++] = c;
        ++read_pos;
      }
    }
    if (OB_SUCC(ret)) {
      if (in_quotes) {
        // an unterminated quoted field means the record is malformed
        ret = OB_INVALID_ARGUMENT;
        LOG_WARN("unterminated quoted field in csv record", K(ret), K(len));
      } else if (OB_FAIL(fields.push_back(ObString(static_cast<int32_t>(write_pos - field_begin),
                                                   buf_ + field_begin)))) {
        LOG_WARN("failed to append last csv field", K(ret));
      }
    }
  }
  return ret;
}

static bool is_space(const char c) { return ' ' == c || '\t' == c; }

static bool equals_ignore_case(const common::ObString &s, const char *lower)
{
  bool same = (s.length() == static_cast<int32_t>(strlen(lower)));
  for (int32_t i = 0; same && i < s.length(); ++i) {
    char c = s.ptr()[i];
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
    same = (c == lower[i]);
  }
  return same;
}

static bool is_digit(const char c) { return c >= '0' && c <= '9'; }

static bool parse_fixed_int(const common::ObString &s, const int32_t pos, const int32_t width, int32_t &out)
{
  bool valid = pos + width <= s.length();
  int32_t value = 0;
  for (int32_t i = 0; valid && i < width; ++i) {
    const char c = s.ptr()[pos + i];
    valid = is_digit(c);
    value = value * 10 + (c - '0');
  }
  if (valid) {
    out = value;
  }
  return valid;
}

static bool is_valid_date_parts(const int32_t year, const int32_t month, const int32_t day)
{
  static const int32_t days_in_month[13] = {-1, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool valid = year >= 1 && month >= 1 && month <= 12;
  if (valid) {
    int32_t max_day = days_in_month[month];
    if (2 == month && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
      max_day = 29;
    }
    valid = day >= 1 && day <= max_day;
  }
  return valid;
}

static bool is_date(const common::ObString &s)
{
  int32_t year = 0;
  int32_t month = 0;
  int32_t day = 0;
  return 10 == s.length() && '-' == s.ptr()[4] && '-' == s.ptr()[7]
         && parse_fixed_int(s, 0, 4, year) && parse_fixed_int(s, 5, 2, month)
         && parse_fixed_int(s, 8, 2, day) && is_valid_date_parts(year, month, day);
}

static bool is_datetime(const common::ObString &s)
{
  int32_t year = 0;
  int32_t month = 0;
  int32_t day = 0;
  int32_t hour = 0;
  int32_t minute = 0;
  int32_t second = 0;
  bool valid = (19 == s.length()) && ('-' == s.ptr()[4]) && ('-' == s.ptr()[7])
               && ((' ' == s.ptr()[10]) || ('T' == s.ptr()[10])) && (':' == s.ptr()[13])
               && (':' == s.ptr()[16])
               && parse_fixed_int(s, 0, 4, year) && parse_fixed_int(s, 5, 2, month)
               && parse_fixed_int(s, 8, 2, day) && parse_fixed_int(s, 11, 2, hour)
               && parse_fixed_int(s, 14, 2, minute) && parse_fixed_int(s, 17, 2, second);
  if (valid) {
    valid = is_valid_date_parts(year, month, day) && hour <= 23 && minute <= 59 && second <= 59;
  }
  return valid;
}

static bool is_integer(const common::ObString &s)
{
  int32_t begin = 0;
  if (s.length() > 0 && ('+' == s.ptr()[0] || '-' == s.ptr()[0])) {
    begin = 1;
  }
  bool valid = s.length() > begin;
  for (int32_t i = begin; valid && i < s.length(); ++i) {
    valid = is_digit(s.ptr()[i]);
  }
  return valid;
}

static bool is_double(const common::ObString &s)
{
  bool valid = false;
  const int32_t len = s.length();
  char buf[64];
  if (len > 0 && len < static_cast<int32_t>(sizeof(buf))) {
    MEMCPY(buf, s.ptr(), len);
    buf[len] = '\0';
    char *end = nullptr;
    const double value = strtod(buf, &end);
    valid = (end == buf + len) && std::isfinite(value);
  }
  return valid;
}

int classify_scalar(const common::ObString &value, ObFileColumnType &type)
{
  int ret = OB_SUCCESS;
  type = FILE_COL_VARCHAR;
  if (0 == value.length()) {
    type = FILE_COL_NULL;
  } else if (equals_ignore_case(value, "null") || (2 == value.length() && '\\' == value.ptr()[0]
             && 'N' == value.ptr()[1])) {
    type = FILE_COL_NULL;
  } else if (equals_ignore_case(value, "true") || equals_ignore_case(value, "false")) {
    type = FILE_COL_BOOLEAN;
  } else if (is_date(value)) {
    type = FILE_COL_DATE;
  } else if (is_datetime(value)) {
    type = FILE_COL_DATETIME;
  } else if (is_integer(value)) {
    // large integers fall through to double on overflow, handled by the caller
    type = FILE_COL_BIGINT;
  } else if (is_double(value)) {
    type = FILE_COL_DOUBLE;
  } else {
    type = FILE_COL_VARCHAR;
  }
  return ret;
}

int get_wider_type(const ObFileColumnType lhs, const ObFileColumnType rhs, ObFileColumnType &result)
{
  int ret = OB_SUCCESS;
  if (FILE_COL_NULL == lhs) {
    result = rhs;
  } else if (FILE_COL_NULL == rhs) {
    result = lhs;
  } else if (lhs == rhs) {
    result = lhs;
  } else if (FILE_COL_VARCHAR == lhs || FILE_COL_VARCHAR == rhs) {
    result = FILE_COL_VARCHAR;
  } else if (FILE_COL_DATETIME == lhs || FILE_COL_DATETIME == rhs) {
    // DATE mixed with anything but itself or DATETIME cannot stay temporal
    result = (FILE_COL_DATE == lhs || FILE_COL_DATE == rhs) ? FILE_COL_DATETIME : FILE_COL_VARCHAR;
  } else if (FILE_COL_DATE == lhs || FILE_COL_DATE == rhs) {
    result = FILE_COL_VARCHAR;
  } else if (FILE_COL_BOOLEAN == lhs || FILE_COL_BOOLEAN == rhs) {
    result = FILE_COL_VARCHAR;
  } else if (FILE_COL_DOUBLE == lhs || FILE_COL_DOUBLE == rhs) {
    result = FILE_COL_DOUBLE;
  } else {
    result = FILE_COL_VARCHAR;
  }
  if (result >= FILE_COL_MAX) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("widened file column type is invalid", K(ret), K(lhs), K(rhs));
  }
  return ret;
}

} // namespace file_sql
} // namespace share
} // namespace oceanbase
