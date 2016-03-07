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

#include <memory>

#include <parquet/cxx09api/reader.h>
#include <parquet/api/reader.h>
#include <parquet/column/reader.h>
#include "parquet/column/scanner.h"
#include <parquet/types.h>

namespace parquet_cpp {
/*
// ----------------------------------------------------------------------
// Define ReaderOptions
//
Reader::~Reader() {
// PASS
}
ReaderOptions::ReaderOptions(){
// PASS
}

ReaderOptions::~ReaderOptions(){
// PASS
}

ReaderOptions& ReaderOptions::include(const std::list<int>& include) {
  includedColumns.assign(include.begin(), include.end());
  return *this;
}

ReaderOptions& ReaderOptions::include(std::vector<int> include) {
  includedColumns.assign(include.begin(), include.end());
  return *this;
}
const std::list<int>& ReaderOptions::getInclude() const {
  return includedColumns;
}

ReaderOptions& ReaderOptions::setMemoryPool(MemoryAllocator& pool) {
  memoryPool = &pool;
  return *this;
}

MemoryAllocator* ReaderOptions::getMemoryPool() const{
  return memoryPool;
}
*/
// ----------------------------------------------------------------------
// A stream-like object that reads from an ExternalInputStream
class StreamSource : public RandomAccessSource {
 public:
  explicit StreamSource(ExternalInputStream* stream);
  virtual void Close() {}
  virtual int64_t Tell() const;
  virtual void Seek(int64_t pos);
  virtual int64_t Read(int64_t nbytes, uint8_t* out);
  virtual std::shared_ptr<Buffer> Read(int64_t nbytes);

 private:
  // parquet-cpp should not manage this object
  ExternalInputStream* stream_;
  int64_t offset_;
};

// ----------------------------------------------------------------------
// StreamSource
StreamSource::StreamSource(ExternalInputStream* stream) :
 stream_(stream),
 offset_(0) {
     size_ = stream->GetLength();
 }

int64_t StreamSource::Tell() const {
  return offset_;
}

void StreamSource::Seek(int64_t pos) {
  if (pos < 0 || pos >= size_) {
      std::stringstream ss;
      ss << "Cannot seek to " << pos
          << ". File length is " << size_;
      throw ParquetException(ss.str());
  }
  offset_ = pos;
}

int64_t StreamSource::Read(int64_t nbytes, uint8_t* out) {
  int64_t bytes_read = 0;
  int64_t bytes_available = std::min(nbytes, size_ - offset_);
  bytes_read = stream_->Read(bytes_available, offset_, out);
  offset_ += bytes_read;
  return bytes_read;
}

std::shared_ptr<Buffer> StreamSource::Read(int64_t nbytes) {
    int64_t bytes_available = std::min(nbytes, size_ - offset_);
    auto result = std::make_shared<OwnedMutableBuffer>();
    result->Resize(bytes_available);

    int64_t bytes_read = 0;
    bytes_read = stream_->Read(bytes_available, offset_, result->mutable_data());
    if (bytes_read < bytes_available) {
        result->Resize(bytes_read);
    }
    offset_ += bytes_read;
    return result;
}

template <int TYPE>
class ParquetTypedScanner : public ParquetScanner {
 public:
  typedef typename type_traits<TYPE>::value_type T;
  ParquetTypedScanner(std::shared_ptr<ColumnReader> creader, int batch_size) {
    scanner_ = std::make_shared<TypedScanner<TYPE> >(creader, batch_size);
  }
  bool NextValue(uint8_t* val, bool* is_null) {
    return scanner_->NextValue(reinterpret_cast<T*>(val), is_null);
  }
  std::shared_ptr<TypedScanner<TYPE> > scanner_;
};

typedef ParquetTypedScanner<Type::BOOLEAN> ParquetBoolScanner;
typedef ParquetTypedScanner<Type::INT32> ParquetInt32Scanner;
typedef ParquetTypedScanner<Type::INT64> ParquetInt64Scanner;
typedef ParquetTypedScanner<Type::INT96> ParquetInt96Scanner;
typedef ParquetTypedScanner<Type::FLOAT> ParquetFloatScanner;
typedef ParquetTypedScanner<Type::DOUBLE> ParquetDoubleScanner;
typedef ParquetTypedScanner<Type::BYTE_ARRAY> ParquetBAScanner;
typedef ParquetTypedScanner<Type::FIXED_LEN_BYTE_ARRAY> ParquetFLBAScanner;

class RowGroupAPI : public RowGroup{
 public:
  RowGroupAPI(std::shared_ptr<RowGroupReader>& greader) : group_reader_(greader) {}

  int64_t NumRows() {
    return group_reader_->num_rows();
  }

  boost::shared_ptr<ParquetScanner> GetScanner(int i, int scan_size);

 private:
  std::shared_ptr<RowGroupReader> group_reader_;
};

// Parquet Reader
class ReaderAPI : public Reader{
 public:
  ReaderAPI(ExternalInputStream* stream, const MemoryAllocator* pool)
      : stream_(stream) {
     source_.reset(new StreamSource(stream));
     if (pool == NULL) {
       reader_ = ParquetFileReader::Open(std::move(source_));
     } else {
       reader_ = ParquetFileReader::Open(std::move(source_), const_cast<MemoryAllocator*>(pool));
     }
  }

  Type::type GetSchemaType(int i) {
    return reader_->column_schema(i)->physical_type();
  }

  int GetTypeLength(int i) {
    return reader_->column_schema(i)->type_length();
  }

  int GetTypePrecision(int i) {
    return reader_->column_schema(i)->type_precision();
  }

  int GetTypeScale(int i) {
    return reader_->column_schema(i)->type_scale();
  }

  bool IsFlatSchema() {
    return reader_->descr()->no_group_nodes();
  }

  std::string& GetStreamName() {
    return stream_->GetName();
  }

  int NumColumns() {
    return reader_->num_columns();
  }

  int NumRowGroups() {
    return reader_->num_row_groups();
  }

  int64_t NumRows() {
    return reader_->num_rows();
  }

  boost::shared_ptr<RowGroup> GetRowGroup(int i) {
    auto group_reader = reader_->RowGroup(i);
    return boost::shared_ptr<RowGroup>(new RowGroupAPI(group_reader));
  }

 private:
  std::unique_ptr<ParquetFileReader> reader_;
  ExternalInputStream* stream_;
  std::unique_ptr<RandomAccessSource> source_;
};

boost::shared_ptr<Reader> Reader::getReader(ExternalInputStream* stream, const MemoryAllocator* pool) {
  return boost::shared_ptr<Reader>(new ReaderAPI(stream, pool));
}

// RowGroup
boost::shared_ptr<ParquetScanner> RowGroupAPI::GetScanner(int i, int batch_size) {
  auto column_reader = group_reader_->Column(i);
  switch (column_reader->type()) {
    case Type::BOOLEAN: {
      return boost::shared_ptr<ParquetScanner>(new ParquetBoolScanner(column_reader, batch_size));
    }
    case Type::INT32: {
      return boost::shared_ptr<ParquetScanner>(new ParquetInt32Scanner(column_reader, batch_size));
    }
    case Type::INT64: {
      return boost::shared_ptr<ParquetScanner>(new ParquetInt64Scanner(column_reader, batch_size));
    }
    case Type::INT96: {
      return boost::shared_ptr<ParquetScanner>(new ParquetInt96Scanner(column_reader, batch_size));
    }
    case Type::FLOAT: {
      return boost::shared_ptr<ParquetScanner>(new ParquetFloatScanner(column_reader, batch_size));
    }
    case Type::DOUBLE: {
      return boost::shared_ptr<ParquetScanner>(new ParquetDoubleScanner(column_reader, batch_size));
    }
    case Type::BYTE_ARRAY: {
      return boost::shared_ptr<ParquetScanner>(new ParquetBAScanner(column_reader, batch_size));
    }
    case Type::FIXED_LEN_BYTE_ARRAY: {
      return boost::shared_ptr<ParquetScanner>(new ParquetFLBAScanner(column_reader, batch_size));
    }
    default:{
      return boost::shared_ptr<ParquetScanner>();
    }
  }
  return boost::shared_ptr<ParquetScanner>();
}

} // namespace parquet_cpp
