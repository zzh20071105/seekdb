/**
 * Copyright (c) 2025 OceanBase.
 * Licensed under the Apache License, Version 2.0.
 */

#define USING_LOG_PREFIX SHARE
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

int get_wider_type(const ObFileColumnType lhs, const ObFileColumnType rhs, ObFileColumnType &result)
{
  int ret = OB_SUCCESS;
  result = static_cast<ObFileColumnType>(lhs > rhs ? lhs : rhs);
  if (result >= FILE_COL_MAX) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("invalid file column type", K(ret), K(lhs), K(rhs));
  }
  return ret;
}

} // namespace file_sql
} // namespace share
} // namespace oceanbase
