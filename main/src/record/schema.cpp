#include "record/schema.h"

/**
 * TODO: Student Implement
 * @brief Serialize schema to buffer
 * @param buf Buffer to serialize to
 * @return Number of bytes written
 */
uint32_t Schema::SerializeTo(char *buf) const {
  char *current_ptr = buf;
  uint32_t offset = 0;

  // Write magic number
  MACH_WRITE_UINT32(current_ptr, SCHEMA_MAGIC_NUM);
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Write number of columns
  MACH_WRITE_UINT32(current_ptr, columns_.size());
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Write each column
  for (const auto &column : columns_) {
    offset += column->SerializeTo(current_ptr);
    current_ptr += column->GetSerializedSize();
  }

  return offset;
}

/**
 * TODO: Student Implement
 * @brief Get serialized size of schema
 * @return Size in bytes
 */
uint32_t Schema::GetSerializedSize() const {
  uint32_t size = 0;

  // Size of magic number
  size += sizeof(uint32_t);

  // Size of number of columns
  size += sizeof(uint32_t);

  // Size of each column
  for (const auto &column : columns_) {
    size += column->GetSerializedSize();
  }

  return size;
}

/**
 * TODO: Student Implement
 * @brief Deserialize schema from buffer
 * @param buf Buffer to deserialize from
 * @param[out] schema Output schema object
 * @return Number of bytes read
 */
uint32_t Schema::DeserializeFrom(char *buf, Schema *&schema) {
  char *current_ptr = buf;
  uint32_t offset = 0;

  // Read and verify magic number
  uint32_t magic_num = MACH_READ_UINT32(current_ptr);
  if (magic_num != SCHEMA_MAGIC_NUM) {
    return 0;
  }
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Read number of columns
  uint32_t num_columns = MACH_READ_UINT32(current_ptr);
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Read each column
  std::vector<Column *> columns;
  for (uint32_t i = 0; i < num_columns; i++) {
    Column *column = nullptr;
    offset += Column::DeserializeFrom(current_ptr, column);
    current_ptr += column->GetSerializedSize();
    columns.push_back(column);
  }

  // Create new schema
  schema = new Schema(columns);
  return offset;
}