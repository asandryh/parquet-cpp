// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef PARQUET_CXX09API_READER_H
#define PARQUET_CXX09API_READER_H

#include <parquet/cxx09api/types.h>
#include <boost/shared_ptr.hpp>

#include <parquet/util/mem-allocator.h>

namespace parquet_cpp {

#if 0
/**
 ** Options for creating a Reader.
 **/
class ReaderOptions {
 private:
  std::list<int> includedColumns;
  MemoryAllocator* memoryPool;
 public:
  ReaderOptions();
  virtual ~ReaderOptions();
  /**
   ** Set the list of columns to read. All columns that are children of
   ** selected columns are automatically selected. The default value is
   ** {0}.
   ** @param include a list of columns to read
   ** @return this
   **/
  ReaderOptions& include(const std::list<int>& include);

  /**
   ** Set the list of columns to read. All columns that are children of
   ** selected columns are automatically selected. The default value is
   ** {0}.
   ** @param include a list of columns to read
   ** @return this
   **/
  ReaderOptions& include(std::vector<int> include);
  /**
   ** Get the list of selected columns to read. All children of the selected
   ** columns are also selected.
   **/
  const std::list<int>& getInclude() const;

  MemoryAllocator* getMemoryPool() const;

  ReaderOptions& setMemoryPool(MemoryAllocator& pool);
};
#endif

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
      const MemoryAllocator* pool = NULL);
};

} // namespace parquet_cpp

#endif // PARQUET_CXX09API_READER_H
