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

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
//#include <initializer_list>

#include <gtest/gtest.h>

#include "parquet/types.h"
#include "parquet/column/page.h"
#include "parquet/column/reader.h"
#include "parquet/column/scanner.h"
#include "parquet/column/test-util.h"

#include "parquet/util/test-common.h"

using parquet::FieldRepetitionType;
using parquet::SchemaElement;

namespace parquet_cpp {

using schema::NodePtr;

namespace test {

struct PrimitiveColumn {
  parquet_cpp::Type::type type;
  std::vector<int32_t> values;
  std::vector<int16_t> def_levels;
  int width;
  std::string printout;
  PrimitiveColumn(parquet_cpp::Type::type type, const std::vector<int32_t>& values,
      const std::vector<int16_t>& def_levels, int width, const std::string& printout) :
    type(type), values(values), def_levels(def_levels),
    width(width), printout(printout) {}
};

class TestPrimitiveScanner : public testing::TestWithParam<PrimitiveColumn> {
 public:
  virtual ~TestPrimitiveScanner();

  TestPrimitiveScanner() {
    auto& vals = GetParam().values;
    auto& d_levels = GetParam().def_levels;
    int16_t max_d_level = d_levels.size() > 0 ? 1 : 0;

    std::shared_ptr<DataPage> page;

    switch (GetParam().type) {
    case Type::INT32: {
      std::vector<int32_t> values(vals.begin(), vals.end());
      page = MakeDataPage<Type::INT32>(values, d_levels, max_d_level, {}, 0, &buffer);
      break;
    }
    case Type::INT64: {
      std::vector<int64_t> values(vals.begin(), vals.end());
      page = MakeDataPage<Type::INT64>(values, d_levels, max_d_level, {}, 0, &buffer);
      break;
    }
    case Type::INT96: {
      std::vector<Int96> values(vals.size());
      for (size_t i = 0; i < vals.size(); i++) {
        values[i].value[0] = 0;
        values[i].value[1] = 1;
        values[i].value[2] = vals[i];
      }
      page = MakeDataPage<Type::INT96>(values, d_levels, max_d_level, {}, 0, &buffer);
      break;
    }
    case Type::BOOLEAN: {
      std::vector<bool> values(vals.size());
      for (size_t i = 0; i < vals.size(); i++) {
        values[i] = vals[i] > 0;
      }
      page = MakeDataPage<Type::BOOLEAN>(values, d_levels, max_d_level, {}, 0, &buffer);
      break;
    }
    case Type::FLOAT: {
      std::vector<float> values(vals.size());
      for (size_t i = 0; i < vals.size(); i++) {
        values[i] = static_cast<float>(vals[i])/100;
      }
      page = MakeDataPage<Type::FLOAT>(values, d_levels, max_d_level, {}, 0, &buffer);
      break;
    }
    case Type::DOUBLE: {
      std::vector<double> values(vals.size());
      for (size_t i = 0; i < vals.size(); i++) {
        values[i] = static_cast<double>(vals[i])/100;
      }
      page = MakeDataPage<Type::DOUBLE>(values, d_levels, max_d_level, {}, 0, &buffer);
      break;
    }
    case Type::BYTE_ARRAY:
      ParquetException::NYI("BYTE_ARRAY columns");
      break;
    case Type::FIXED_LEN_BYTE_ARRAY:
      ParquetException::NYI("FIXED_LEN_BYTE_ARRAY columns");
      break;
    default:
      throw ParquetException("Unknown column type");
      break;
    }
    pages_.push_back(page);

    parquet_cpp::Repetition::type rep =
        d_levels.size() > 0 ? Repetition::OPTIONAL : Repetition::REQUIRED;
    NodePtr type = schema::PrimitiveNode::Make("a", rep, GetParam().type);
    descr_.reset(new ColumnDescriptor(type, max_d_level, 0));

    std::unique_ptr<PageReader> pager(new test::MockPageReader(pages_));
    scanner_ = Scanner::Make(ColumnReader::Make(descr_.get(), std::move(pager)));
  }

 protected:
  std::vector<uint8_t> buffer;
  std::unique_ptr<ColumnDescriptor> descr_;
  std::vector< std::shared_ptr<Page> > pages_;
  std::shared_ptr<Scanner> scanner_;
};

TestPrimitiveScanner::~TestPrimitiveScanner() {}


TEST_P(TestPrimitiveScanner, TestBatchSize) {
  ASSERT_EQ(DEFAULT_SCANNER_BATCH_SIZE, scanner_->batch_size());

  scanner_->SetBatchSize(1);
  ASSERT_EQ(1, scanner_->batch_size());

  scanner_->SetBatchSize(1000000);
  ASSERT_EQ(1000000, scanner_->batch_size());
}

TEST_P(TestPrimitiveScanner, TestSmallBatch) {
  scanner_->SetBatchSize(1);

  std::stringstream ss;
  while (scanner_->HasNext()) {
    scanner_->PrintNext(ss, 1);
  }
}

TEST_P(TestPrimitiveScanner, TestPrimitiveColumn) {
  auto& column_info = GetParam();
  size_t num_values = std::max(column_info.values.size(), column_info.def_levels.size());
  std::stringstream ss;

  for (size_t i = 0; i < num_values; i++) {
    ASSERT_TRUE(scanner_->HasNext());
    scanner_->PrintNext(ss, column_info.width);
    ss <<"; ";
  }
  ASSERT_FALSE(scanner_->HasNext());

  ASSERT_EQ(column_info.printout, ss.str());
}

INSTANTIATE_TEST_CASE_P(ScannerTest, TestPrimitiveScanner,
    testing::Values(
        // Required column (no definition & repetition levels)
        PrimitiveColumn(Type::INT32, {1, -2, 3, -4, 5, -6}, {}, 2,
            "1 ; -2; 3 ; -4; 5 ; -6; "),
        PrimitiveColumn(Type::INT64, {11, -12, 13, -14, 15}, {}, 3,
            "11 ; -12; 13 ; -14; 15 ; "),
        PrimitiveColumn(Type::INT96, {1, 2, 3, 4}, {}, 2,
            "0 1 1 ; 0 1 2 ; 0 1 3 ; 0 1 4 ; "),
        PrimitiveColumn(Type::BOOLEAN, {1, 1, 0, 0, 1, 0}, {}, 2,
            "1 ; 1 ; 0 ; 0 ; 1 ; 0 ; "),
        PrimitiveColumn(Type::FLOAT, {1234, -567, 89, -1}, {}, 1,
            "12.340000; -5.670000; 0.890000; -0.010000; "),
        PrimitiveColumn(Type::DOUBLE, {1234, -567, 89, -1}, {}, 1,
            "12.340000; -5.670000; 0.890000; -0.010000; "),

        // Optional column (no repetition levels)
        PrimitiveColumn(Type::INT32, {1, -2, 3, -4, 5}, {1, 0, 1, 1, 0, 0, 1, 1}, 2,
            "1 ; NULL; -2; 3 ; NULL; NULL; -4; 5 ; "),
        PrimitiveColumn(Type::INT64, {11, -12, 13, -14, 15}, {1, 0, 1, 1, 0, 0, 1, 1}, 3,
            "11 ; NULL; -12; 13 ; NULL; NULL; -14; 15 ; "),
        PrimitiveColumn(Type::INT96, {1, 2, 3, 4}, {1, 0, 1, 1, 0, 0, 1}, 2,
            "0 1 1 ; NULL; 0 1 2 ; 0 1 3 ; NULL; NULL; 0 1 4 ; "),
        PrimitiveColumn(Type::BOOLEAN, {1, 1, 0, 0, 1}, {1, 0, 1, 1, 0, 0, 1, 1}, 2,
            "1 ; NULL; 1 ; 0 ; NULL; NULL; 0 ; 1 ; "),
        PrimitiveColumn(Type::FLOAT, {1234, -567, 89, -1}, {1, 0, 1, 0, 0, 1, 1}, 1,
            "12.340000; NULL; -5.670000; NULL; NULL; 0.890000; -0.010000; "),
        PrimitiveColumn(Type::DOUBLE, {1234, -567, 89, -1}, {1, 0, 1, 1, 0, 0, 1}, 1,
            "12.340000; NULL; -5.670000; 0.890000; NULL; NULL; -0.010000; ")));

} // namespace test

} // namespace parquet_cpp
