/**
 * Copyright (c) 2025 OceanBase.
 * Licensed under the Apache License, Version 2.0.
 */

#ifndef OCEANBASE_SHARE_FILE_SQL_OB_FILE_CSV_READER_H_
#define OCEANBASE_SHARE_FILE_SQL_OB_FILE_CSV_READER_H_

#include "lib/string/ob_string.h"
#include "lib/allocator/ob_allocator.h"
#include "lib/container/ob_se_array.h"
#include "lib/oblog/ob_log_module.h"
#include "lib/ob_errno.h"

namespace oceanbase
{
namespace share
{
namespace file_sql
{

// The deterministic type ladder of the file SQL feature. A column starts as
// FILE_COL_NULL and widens as rows are scanned; the order below is the widening
// order, so get_wider_type() is just max().
enum ObFileColumnType
{
  FILE_COL_NULL = 0,
  FILE_COL_BIGINT,
  FILE_COL_DOUBLE,
  FILE_COL_BOOLEAN,
  FILE_COL_DATE,
  FILE_COL_DATETIME,
  FILE_COL_VARCHAR,
  FILE_COL_MAX
};

class ObFileCsvOptions
{
public:
  ObFileCsvOptions();
  void reset();
  bool has_header_;
  common::ObString separators_set_;
  char field_delimiter_;
  char line_delimiter_;
  bool ignore_blank_lines_;
  bool csv_quote_escape_;
  bool csv_tab_escape_;
};

class ObCsvFileReader;
class ObCsvFileScanner;
class ObFileColumnSchema;
class ObCsvFileReader;

int get_wider_type(const ObFileColumnType lhs, const ObFileColumnType rhs, ObFileColumnType &result);

} // namespace file_sql
} // namespace share
} // namespace oceanbase
#endif // OCEANBASE_SHARE_FILE_SQL_OB_FILE_CSV_READER_H_
