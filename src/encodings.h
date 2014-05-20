#ifndef PARQUET_ENCODINGS_H
#define PARQUET_ENCODINGS_H

#include <boost/cstdint.hpp>
#include "gen-cpp/parquet_constants.h"
#include "gen-cpp/parquet_types.h"

#include "impala/rle-encoding.h"
#include "impala/bit-stream-utils.inline.h"

namespace parquet_cpp {

class Decoder {
 public:
  virtual ~Decoder() {}

  // Sets the data for a new page. This will be called multiple times on the same
  // decoder and should reset all internal state.
  virtual void SetData(int num_values, const uint8_t* data, int len) = 0;

  // Subclasses should override the ones they support. In each of these functions,
  // the decoder would decode put to 'max_values', storing the result in 'buffer'.
  // The function returns the number of values decoded, which should be max_values
  // except for end of the current data page.
  virtual int GetBool(bool* buffer, int max_values) {
    throw ParquetException("Decoder does not implement this type.");
  }
  virtual int GetInt32(int32_t* buffer, int max_values) {
    throw ParquetException("Decoder does not implement this type.");
  }
  virtual int GetInt64(int64_t* buffer, int max_values) {
    throw ParquetException("Decoder does not implement this type.");
  }
  virtual int GetFloat(float* buffer, int max_values) {
    throw ParquetException("Decoder does not implement this type.");
  }
  virtual int GetDouble(double* buffer, int max_values) {
    throw ParquetException("Decoder does not implement this type.");
  }
  virtual int GetByteArray(ByteArray* buffer, int max_values) {
    throw ParquetException("Decoder does not implement this type.");
  }

  // Returns the number of values left (for the last call to SetData()). This is
  // the number of values left in this page.
  int values_left() const { return num_values_; }

  const parquet::Encoding::type encoding() const { return encoding_; }

 protected:
  Decoder(const parquet::SchemaElement* schema, const parquet::Encoding::type& encoding)
    : schema_(schema), encoding_(encoding), num_values_(0) {}

  const parquet::SchemaElement* schema_;
  const parquet::Encoding::type encoding_;
  int num_values_;
};

class BoolDecoder : public Decoder {
 public:
  BoolDecoder(const parquet::SchemaElement* schema)
    : Decoder(schema, parquet::Encoding::PLAIN) { }

  virtual void SetData(int num_values, const uint8_t* data, int len) {
    num_values_ = num_values;
    decoder_ = impala::RleDecoder(data, len, 1);
  }

  virtual int GetBool(bool* buffer, int max_values) {
    max_values = std::min(max_values, num_values_);
    for (int i = 0; i < max_values; ++i) {
      if (!decoder_.Get(&buffer[i])) ParquetException::EofException();
    }
    num_values_ -= max_values;
    return max_values;
  }

 private:
  impala::RleDecoder decoder_;
};

class PlainDecoder : public Decoder {
 public:
  PlainDecoder(const parquet::SchemaElement* schema)
    : Decoder(schema, parquet::Encoding::PLAIN), data_(NULL), len_(0) {
  }

  virtual void SetData(int num_values, const uint8_t* data, int len) {
    num_values_ = num_values;
    data_ = data;
    len_ = len;
  }

  int GetValues(void* buffer, int max_values, int byte_size) {
    max_values = std::min(max_values, num_values_);
    int size = max_values * byte_size;
    if (len_ < size)  ParquetException::EofException();
    memcpy(buffer, data_, size);
    data_ += size;
    len_ -= size;
    num_values_ -= max_values;
    return max_values;
  }

  virtual int GetInt32(int32_t* buffer, int max_values) {
    return GetValues(buffer, max_values, sizeof(int32_t));
  }

  virtual int GetInt64(int64_t* buffer, int max_values) {
    return GetValues(buffer, max_values, sizeof(int64_t));
  }

  virtual int GetFloat(float* buffer, int max_values) {
    return GetValues(buffer, max_values, sizeof(float));
  }

  virtual int GetDouble(double* buffer, int max_values) {
    return GetValues(buffer, max_values, sizeof(double));
  }

  virtual int GetByteArray(ByteArray* buffer, int max_values) {
    max_values = std::min(max_values, num_values_);
    for (int i = 0; i < max_values; ++i) {
      buffer[i].len = *reinterpret_cast<const uint32_t*>(data_);
      if (len_ < sizeof(uint32_t) + buffer[i].len) ParquetException::EofException();
      buffer[i].ptr = data_ + sizeof(uint32_t);
      data_ += sizeof(uint32_t) + buffer[i].len;
      len_ -= sizeof(uint32_t) + buffer[i].len;
    }
    num_values_ -= max_values;
    return max_values;
  }

 private:
  const uint8_t* data_;
  int len_;
};

class DictionaryDecoder : public Decoder {
 public:
  DictionaryDecoder(const parquet::SchemaElement* schema, Decoder* dictionary)
    : Decoder(schema, parquet::Encoding::RLE_DICTIONARY) {
    int num_dictionary_values = dictionary->values_left();
    switch (schema->type) {
      case parquet::Type::BOOLEAN:
        throw ParquetException("Boolean cols should not be dictionary encoded.");

      case parquet::Type::INT32:
        int32_dictionary_.resize(num_dictionary_values);
        dictionary->GetInt32(&int32_dictionary_[0], num_dictionary_values);
        break;
      case parquet::Type::INT64:
        int64_dictionary_.resize(num_dictionary_values);
        dictionary->GetInt64(&int64_dictionary_[0], num_dictionary_values);
        break;
      case parquet::Type::FLOAT:
        float_dictionary_.resize(num_dictionary_values);
        dictionary->GetFloat(&float_dictionary_[0], num_dictionary_values);
        break;
      case parquet::Type::DOUBLE:
        double_dictionary_.resize(num_dictionary_values);
        dictionary->GetDouble(&double_dictionary_[0], num_dictionary_values);
        break;
      case parquet::Type::BYTE_ARRAY:
        byte_array_dictionary_.resize(num_dictionary_values);
        dictionary->GetByteArray(&byte_array_dictionary_[0], num_dictionary_values);
        break;
      default:
        ParquetException::NYI();
    }
  }

  virtual void SetData(int num_values, const uint8_t* data, int len) {
    num_values_ = num_values;
    if (len == 0) return;
    uint8_t bit_width = *data;
    ++data;
    --len;
    idx_decoder_ = impala::RleDecoder(data, len, bit_width);
  }

  virtual int GetInt32(int32_t* buffer, int max_values) {
    max_values = std::min(max_values, num_values_);
    for (int i = 0; i < max_values; ++i) {
      buffer[i] = int32_dictionary_[index()];
    }
    return max_values;
  }

  virtual int GetInt64(int64_t* buffer, int max_values) {
    max_values = std::min(max_values, num_values_);
    for (int i = 0; i < max_values; ++i) {
      buffer[i] = int64_dictionary_[index()];
    }
    return max_values;
  }

  virtual int GetFloat(float* buffer, int max_values) {
    max_values = std::min(max_values, num_values_);
    for (int i = 0; i < max_values; ++i) {
      buffer[i] = float_dictionary_[index()];
    }
    return max_values;
  }

  virtual int GetDouble(double* buffer, int max_values) {
    max_values = std::min(max_values, num_values_);
    for (int i = 0; i < max_values; ++i) {
      buffer[i] = double_dictionary_[index()];
    }
    return max_values;
  }

  virtual int GetByteArray(ByteArray* buffer, int max_values) {
    max_values = std::min(max_values, num_values_);
    for (int i = 0; i < max_values; ++i) {
      buffer[i] = byte_array_dictionary_[index()];
    }
    return max_values;
  }

 private:
  int index() {
    int idx;
    if (!idx_decoder_.Get(&idx)) ParquetException::EofException();
    --num_values_;
    return idx;
  }

  // Only one is set.
  std::vector<int32_t> int32_dictionary_;
  std::vector<int64_t> int64_dictionary_;
  std::vector<float> float_dictionary_;
  std::vector<double> double_dictionary_;
  std::vector<ByteArray> byte_array_dictionary_;

  impala::RleDecoder idx_decoder_;
};

class DeltaBinaryPackedDecoder : public Decoder {
 public:
  DeltaBinaryPackedDecoder(const parquet::SchemaElement* schema)
    : Decoder(schema, parquet::Encoding::DELTA_BINARY_PACKED) { }

  virtual void SetData(int num_values, const uint8_t* data, int len) {
    num_values_ = num_values;
    decoder_ = impala::BitReader(data, len);
    values_current_block_ = 0;
    values_current_mini_block_ = 0;
  }

  virtual int GetInt32(int32_t* buffer, int max_values) {
    max_values = std::min(max_values, num_values_);
    for (int i = 0; i < max_values; ++i) {
      if (values_current_block_ == 0) {
        uint32_t block_size, num_mini_blocks;
        if (!decoder_.GetVlqInt(&block_size)) ParquetException::EofException();
        if (!decoder_.GetVlqInt(&num_mini_blocks)) ParquetException::EofException();
        if (!decoder_.GetVlqInt(&values_current_block_)) {
          ParquetException::EofException();
        }

        if (!decoder_.GetZigZagVlqInt(&buffer[i])) ParquetException::EofException();
        if (!decoder_.GetZigZagVlqInt(&min_delta_)) ParquetException::EofException();
        if (!decoder_.GetAligned<uint8_t>(8, &delta_bit_width_)) {
          ParquetException::EofException();
        }
        last_value_ = buffer[i];
        --values_current_block_;
        continue;
      }

      int delta;
      if (!decoder_.GetValue(delta_bit_width_, &delta)) ParquetException::EofException();
      delta += min_delta_;
      last_value_ += delta;
      buffer[i] = last_value_;
    }
    return max_values;
  }

 private:
  impala::BitReader decoder_;
  uint32_t values_current_block_;
  uint32_t values_current_mini_block_;
  int32_t last_value_;
  int32_t min_delta_;
  uint8_t delta_bit_width_;
};

}

#endif
