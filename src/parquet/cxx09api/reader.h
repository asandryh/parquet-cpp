/* Copyright (c) 2016 Hewlett Packard Enterprise Development LP */

#ifndef PARQUET_CXX09API_READER_H
#define PARQUET_CXX09API_READER_H

#include <parquet/cxx09api/types.h>
#include <boost/shared_ptr.hpp>

#include <parquet/util/mem-allocator.h>

#include <algorithm>
#include <list>
#include <string>
#include <vector>

namespace parquet {

class ParquetScanner {
 public:
  virtual bool NextValue(uint8_t* val, bool* is_null) = 0;
};

class RowGroup {
 public:
  virtual int64_t NumRows() = 0;
  virtual boost::shared_ptr<ParquetScanner> GetScanner(int i, int scan_size) = 0;
};

class Reader {
 public:
  virtual Type::type GetSchemaType(int i) = 0;
  virtual int GetTypeLength(int i) = 0;
  virtual int GetTypePrecision(int i) = 0;
  virtual int GetTypeScale(int i) = 0;
  virtual std::string& GetStreamName() = 0;
  virtual int NumColumns() = 0;
  virtual int NumRowGroups() = 0;
  virtual int64_t NumRows() = 0;
  virtual bool IsFlatSchema() = 0;
  virtual boost::shared_ptr<RowGroup> GetRowGroup(int i) = 0;
  // API to return a Reader
  static boost::shared_ptr<Reader> getReader(ExternalInputStream* stream,
      MemoryAllocator* pool = default_allocator());
  virtual int64_t EstimateMemoryUsage(std::list<int>& selected_columns,
      int64_t batch_size) = 0;
};

} // namespace parquet

#endif // PARQUET_CXX09API_READER_H
