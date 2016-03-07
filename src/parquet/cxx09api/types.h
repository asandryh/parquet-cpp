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


#ifndef PARQUET_CXX09_TYPES_H
#define PARQUET_CXX09_TYPES_H

#include <cstring>
#include <sstream>
#include <parquet/util/compiler-util.h>

namespace parquet_cpp {

// ----------------------------------------------------------------------
// External streaming input interface that contains a parquet file
class ExternalInputStream {
 public:
  // Returns 'num_to_peek' bytes at location 'offset' in the external stream
  // without advancing the current position.
  // *num_bytes will contain the number of bytes returned which can only be
  // less than num_to_peek at end of stream cases.
  // Since the position is not advanced, calls to this function are idempotent.
  // The buffer returned to the caller is still owned by the input stream and must
  // stay valid until the next call to Peek() or Read().
  virtual const uint8_t* Peek(int64_t num_to_peek, int64_t offset,
          int64_t* num_bytes) = 0;

  // Read interface to read from stream at an offset into a specified buffer
  // Returns the number of bytes read.
  virtual int64_t Read(int64_t num_to_read, int64_t offset, uint8_t* buffer) = 0;

  // Get the total length of the stream in bytes.
  virtual int64_t GetLength() = 0;

  // Get the name of the stream for error messages.
  virtual std::string& GetName() = 0;

  virtual ~ExternalInputStream() {}

 protected:
  ExternalInputStream() {}
};

// ----------------------------------------------------------------------
// Metadata enums to match Thrift metadata
//
// The reason we maintain our own enums is to avoid transitive dependency on
// the compiled Thrift headers (and thus thrift/Thrift.h) for users of the
// public API. After building parquet-cpp, you should not need to include
// Thrift headers in your application. This means some boilerplate to convert
// between our types and Parquet's Thrift types.
//
// We can also add special values like NONE to distinguish between metadata
// values being set and not set. As an example consider ConvertedType and
// CompressionCodec

// Mirrors parquet::Type
struct Type {
  enum type {
    BOOLEAN = 0,
    INT32 = 1,
    INT64 = 2,
    INT96 = 3,
    FLOAT = 4,
    DOUBLE = 5,
    BYTE_ARRAY = 6,
    FIXED_LEN_BYTE_ARRAY = 7
  };
};

// Mirrors parquet::ConvertedType
struct LogicalType {
  enum type {
    NONE,
    UTF8,
    MAP,
    MAP_KEY_VALUE,
    LIST,
    ENUM,
    DECIMAL,
    DATE,
    TIME_MILLIS,
    TIMESTAMP_MILLIS,
    UINT_8,
    UINT_16,
    UINT_32,
    UINT_64,
    INT_8,
    INT_16,
    INT_32,
    INT_64,
    JSON,
    BSON,
    INTERVAL
  };
};

//Parquet Types

struct ByteArray {
  ByteArray() {}
  ByteArray(uint32_t len, const uint8_t* ptr) : len(len), ptr(ptr) {}
  uint32_t len;
  const uint8_t* ptr;

  bool operator==(const ByteArray& other) const {
    return this->len == other.len &&
      0 == memcmp(this->ptr, other.ptr, this->len);
  }

  bool operator!=(const ByteArray& other) const {
    return this->len != other.len || 0 != memcmp(this->ptr, other.ptr, this->len);
  }
};

struct FixedLenByteArray {
  FixedLenByteArray() {}
  explicit FixedLenByteArray(const uint8_t* ptr) : ptr(ptr) {}
  const uint8_t* ptr;
};

typedef FixedLenByteArray FLBA;

MANUALLY_ALIGNED_STRUCT(1) Int96 {
  uint32_t value[3];

  bool operator==(const Int96& other) const {
    return 0 == memcmp(this->value, other.value, 3 * sizeof(uint32_t));
  }

  bool operator!=(const Int96& other) const {
    return !(*this == other);
  }
};
STRUCT_END(Int96, 12);

static inline std::string ByteArrayToString(const ByteArray& a) {
  return std::string(reinterpret_cast<const char*>(a.ptr), a.len);
}

static inline std::string Int96ToString(const Int96& a) {
  std::stringstream result;
  for (int i = 0; i < 3; i++) {
     result << a.value[i]  << " ";
  }
  return result.str();
}

static inline std::string FixedLenByteArrayToString(const FixedLenByteArray& a, int len) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t*>(a.ptr);
  std::stringstream result;
  for (int i = 0; i < len; i++) {
    result << (uint32_t)bytes[i]  << " ";
  }
  return result.str();
}

static inline int ByteCompare(const ByteArray& x1, const ByteArray& x2) {
  uint32_t len = std::min(x1.len, x2.len);
  int cmp = memcmp(x1.ptr, x2.ptr, len);
  if (cmp != 0) return cmp;
  if (len < x1.len) return 1;
  if (len < x2.len) return -1;
  return 0;
}

// ----------------------------------------------------------------------

static inline std::string type_to_string(Type::type t) {
  switch (t) {
    case Type::BOOLEAN:
      return "BOOLEAN";
      break;
    case Type::INT32:
      return "INT32";
      break;
    case Type::INT64:
      return "INT64";
      break;
    case Type::INT96:
      return "INT96";
      break;
    case Type::FLOAT:
      return "FLOAT";
      break;
    case Type::DOUBLE:
      return "DOUBLE";
      break;
    case Type::BYTE_ARRAY:
      return "BYTE_ARRAY";
      break;
    case Type::FIXED_LEN_BYTE_ARRAY:
      return "FIXED_LEN_BYTE_ARRAY";
      break;
    default:
      return "UNKNOWN";
      break;
  }
}

static inline std::string logical_type_to_string(LogicalType::type t) {
  switch (t) {
    case LogicalType::NONE:
      return "NONE";
      break;
    case LogicalType::UTF8:
      return "UTF8";
      break;
    case LogicalType::MAP_KEY_VALUE:
      return "MAP_KEY_VALUE";
      break;
    case LogicalType::LIST:
      return "LIST";
      break;
    case LogicalType::ENUM:
      return "ENUM";
      break;
    case LogicalType::DECIMAL:
      return "DECIMAL";
      break;
    case LogicalType::DATE:
      return "DATE";
      break;
    case LogicalType::TIME_MILLIS:
      return "TIME_MILLIS";
      break;
    case LogicalType::TIMESTAMP_MILLIS:
      return "TIMESTAMP_MILLIS";
      break;
    case LogicalType::UINT_8:
      return "UINT_8";
      break;
    case LogicalType::UINT_16:
      return "UINT_16";
      break;
    case LogicalType::UINT_32:
      return "UINT_32";
      break;
    case LogicalType::UINT_64:
      return "UINT_64";
      break;
    case LogicalType::INT_8:
      return "INT_8";
      break;
    case LogicalType::INT_16:
      return "INT_16";
      break;
    case LogicalType::INT_32:
      return "INT_32";
      break;
    case LogicalType::INT_64:
      return "INT_64";
      break;
    case LogicalType::JSON:
      return "JSON";
      break;
    case LogicalType::BSON:
      return "BSON";
      break;
    case LogicalType::INTERVAL:
      return "INTERVAL";
      break;
    default:
      return "UNKNOWN";
      break;
  }
}
} // namespace parquet_cpp

#endif // PARQUET_TYPES_H
