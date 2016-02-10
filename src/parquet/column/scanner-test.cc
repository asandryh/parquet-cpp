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

class TestPrimitiveScanner : public ::testing::Test {
 public:
  void SetUp() {}

  void TearDown() {}

  void InitScanner(const ColumnDescriptor* descr,
      size_t batch_size = DEFAULT_SCANNER_BATCH_SIZE) {
    std::unique_ptr<PageReader> pager(new test::MockPageReader(pages_));
    scanner_ = Scanner::Make(ColumnReader::Make(descr, std::move(pager)), batch_size);
  }

 protected:
  std::shared_ptr<Scanner> scanner_;
  std::vector< std::shared_ptr<Page> > pages_;
};

TEST_F(TestPrimitiveScanner, Int32FlatRequired) {
  std::vector<int32_t> values = {1, 2, 3, 4, 5, 6};
  parquet::Encoding::type value_encoding = parquet::Encoding::PLAIN;

  std::vector<uint8_t> page1;
  test::DataPageBuilder<Type::INT32> page_builder(&page1);
  page_builder.AppendValues(values, parquet::Encoding::PLAIN);
  pages_.push_back(page_builder.Finish());

  // TODO: simplify this
  NodePtr type = schema::Int32("a", Repetition::REQUIRED);
  ColumnDescriptor descr(type, 0, 0);
  InitScanner(&descr);

  std::stringstream ss;
  for (size_t i = 0; i < 6; i++) {
    ASSERT_TRUE(scanner_->HasNext());
    scanner_->PrintNext(ss, 2);
  }
  ASSERT_EQ("1 2 3 4 5 6 ", ss.str());
  ASSERT_FALSE(scanner_->HasNext());
}

TEST_F(TestPrimitiveScanner, Int32FlatOptional) {
  vector<int32_t> values = {1, 2, 3, 4, 5};
  vector<int16_t> def_levels = {1, 0, 0, 1, 1, 0, 0, 0, 1, 1};

  parquet::Encoding::type value_encoding = parquet::Encoding::PLAIN;

  vector<uint8_t> page1;
  test::DataPageBuilder<Type::INT32> page_builder(&page1);

  // Definition levels precede the values
  page_builder.AppendDefLevels(def_levels, 1, parquet::Encoding::RLE);
  page_builder.AppendValues(values, parquet::Encoding::PLAIN);

  pages_.push_back(page_builder.Finish());

  NodePtr type = schema::Int32("a", Repetition::OPTIONAL);
  ColumnDescriptor descr(type, 1, 0);
  InitScanner(&descr, 1);

  std::stringstream ss;
  for (size_t i = 0; i < 10; i++) {
    ASSERT_TRUE(scanner_->HasNext());
    scanner_->PrintNext(ss, 5);
  }
  ASSERT_EQ("1    NULL NULL 2    3    NULL NULL NULL 4    5    ", ss.str());
  ASSERT_FALSE(scanner_->HasNext());
}

TEST_F(TestPrimitiveScanner, BatchSize) {
  std::vector<int32_t> values = {1, 2, 3, 4, 5};
  size_t num_values = values.size();
  parquet::Encoding::type value_encoding = parquet::Encoding::PLAIN;

  std::vector<uint8_t> page1;
  test::DataPageBuilder<Type::INT32> page_builder(&page1);
  page_builder.AppendValues(values, parquet::Encoding::PLAIN);
  pages_.push_back(page_builder.Finish());

  // TODO: simplify this
  NodePtr type = schema::Int32("a", Repetition::REQUIRED);
  ColumnDescriptor descr(type, 0, 0);
  InitScanner(&descr);

  ASSERT_EQ(DEFAULT_SCANNER_BATCH_SIZE, scanner_->batch_size());

  scanner_->SetBatchSize(1);
  ASSERT_EQ(1, scanner_->batch_size());

  scanner_->SetBatchSize(1000000);
  ASSERT_EQ(1000000, scanner_->batch_size());
}

TEST_F(TestPrimitiveScanner, ScannerSmallBatch) {
  std::vector<int32_t> values = {1, 2, 3, 4, 5, 6};
  parquet::Encoding::type value_encoding = parquet::Encoding::PLAIN;

  std::vector<uint8_t> page1;
  test::DataPageBuilder<Type::INT32> page_builder(&page1);
  page_builder.AppendValues(values, parquet::Encoding::PLAIN);
  pages_.push_back(page_builder.Finish());

  // TODO: simplify this
  NodePtr type = schema::Int32("a", Repetition::REQUIRED);
  ColumnDescriptor descr(type, 0, 0);
  InitScanner(&descr, 1);

  std::stringstream ss;
  for (size_t i = 0; i < 6; i++) {
    ASSERT_TRUE(scanner_->HasNext());
    scanner_->PrintNext(ss, 2);
  }
  ASSERT_EQ("1 2 3 4 5 6 ", ss.str());
  ASSERT_FALSE(scanner_->HasNext());
}

TEST_F(TestPrimitiveScanner, ScannerLargeBatch) {
  std::vector<int32_t> values = {1, 2, 3, 4, 5, 6};
  parquet::Encoding::type value_encoding = parquet::Encoding::PLAIN;

  std::vector<uint8_t> page1;
  test::DataPageBuilder<Type::INT32> page_builder(&page1);
  page_builder.AppendValues(values, parquet::Encoding::PLAIN);
  pages_.push_back(page_builder.Finish());

  // TODO: simplify this
  NodePtr type = schema::Int32("a", Repetition::REQUIRED);
  ColumnDescriptor descr(type, 0, 0);
  InitScanner(&descr, 1000);

  std::stringstream ss;
  for (size_t i = 0; i < 6; i++) {
    ASSERT_TRUE(scanner_->HasNext());
    scanner_->PrintNext(ss, 2);
  }
  ASSERT_EQ("1 2 3 4 5 6 ", ss.str());
  ASSERT_FALSE(scanner_->HasNext());
}


} // namespace test

} // namespace parquet_cpp
