#include "record/row.h"

/**
 * TODO: Student Implement
 * @brief Serialize row to buffer
 * @param buf Buffer to serialize to
 * @param schema Table schema
 * @return Number of bytes written
 */
uint32_t Row::SerializeTo(char *buf, Schema *schema) const {
  ASSERT(schema != nullptr, "Invalid schema before serialize.");
  ASSERT(schema->GetColumnCount() == fields_.size(), "Fields size do not match schema's column size.");
  
  char *current_ptr = buf;
  uint32_t offset = 0;

  // Write number of fields
  MACH_WRITE_UINT32(current_ptr, fields_.size());
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Write null bitmap
  uint32_t null_bitmap = 0;
  for (uint32_t i = 0; i < fields_.size(); i++) {
    if (fields_[i]->IsNull()) {
      null_bitmap |= (1 << i);
    }
  }
  MACH_WRITE_UINT32(current_ptr, null_bitmap);
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Write each field
  for (uint32_t i = 0; i < fields_.size(); i++) {
    if (!fields_[i]->IsNull()) {
      offset += fields_[i]->SerializeTo(current_ptr);
      current_ptr += fields_[i]->GetSerializedSize();
    }
  }

  return offset;
}

/**
 * TODO: Student Implement
 * @brief Deserialize row from buffer
 * @param buf Buffer to deserialize from
 * @param schema Table schema
 * @return Number of bytes read
 */
uint32_t Row::DeserializeFrom(char *buf, Schema *schema) {
  ASSERT(schema != nullptr, "Invalid schema before serialize.");
  ASSERT(fields_.empty(), "Non empty field in row.");
  
  char *current_ptr = buf;
  uint32_t offset = 0;

  // Read number of fields
  uint32_t num_fields = MACH_READ_UINT32(current_ptr);
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Read null bitmap
  uint32_t null_bitmap = MACH_READ_UINT32(current_ptr);
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Read each field
  for (uint32_t i = 0; i < num_fields; i++) {
    Field *field;
    if (null_bitmap & (1 << i)) {
      // Field is null
      field = new Field(schema->GetColumn(i)->GetType());
    } else {
      // Deserialize field
      field = new Field(schema->GetColumn(i)->GetType());
      uint32_t field_size = field->DeserializeFrom(current_ptr, schema->GetColumn(i)->GetType(), &field);
      current_ptr += field_size;
      offset += field_size;
    }
    fields_.push_back(field);
  }

  return offset;
}

/**
 * TODO: Student Implement
 * @brief Get serialized size of row
 * @param schema Table schema
 * @return Size in bytes
 */
uint32_t Row::GetSerializedSize(Schema *schema) const {
  ASSERT(schema != nullptr, "Invalid schema before serialize.");
  ASSERT(schema->GetColumnCount() == fields_.size(), "Fields size do not match schema's column size.");
  
  uint32_t size = 0;

  // Size of number of fields
  size += sizeof(uint32_t);

  // Size of null bitmap
  size += sizeof(uint32_t);

  // Size of each non-null field
  for (uint32_t i = 0; i < fields_.size(); i++) {
    if (!fields_[i]->IsNull()) {
      size += fields_[i]->GetSerializedSize();
    }
  }

  return size;
}

void Row::GetKeyFromRow(const Schema *schema, const Schema *key_schema, Row &key_row) {
  auto columns = key_schema->GetColumns();
  std::vector<Field> fields;
  uint32_t idx;
  for (auto column : columns) {
    schema->GetColumnIndex(column->GetName(), idx);
    fields.emplace_back(*this->GetField(idx));
  }
  key_row = Row(fields);
}
