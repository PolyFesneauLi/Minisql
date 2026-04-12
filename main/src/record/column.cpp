#include "record/column.h"

#include "glog/logging.h"

Column::Column(std::string column_name, TypeId type, uint32_t index, bool nullable, bool unique)
    : name_(std::move(column_name)), type_(type), table_ind_(index), nullable_(nullable), unique_(unique) {
  ASSERT(type != TypeId::kTypeChar, "Wrong constructor for CHAR type.");
  switch (type) {
    case TypeId::kTypeInt:
      len_ = sizeof(int32_t);
      break;
    case TypeId::kTypeFloat:
      len_ = sizeof(float_t);
      break;
    default:
      ASSERT(false, "Unsupported column type.");
  }
}

Column::Column(std::string column_name, TypeId type, uint32_t length, uint32_t index, bool nullable, bool unique)
    : name_(std::move(column_name)),
      type_(type),
      len_(length),
      table_ind_(index),
      nullable_(nullable),
      unique_(unique) {
  ASSERT(type == TypeId::kTypeChar, "Wrong constructor for non-VARCHAR type.");
}

Column::Column(const Column *other)
    : name_(other->name_),
      type_(other->type_),
      len_(other->len_),
      table_ind_(other->table_ind_),
      nullable_(other->nullable_),
      unique_(other->unique_) {}

/**
 * TODO: Student Implement
 * @brief Serialize column to buffer
 * @param buf Buffer to serialize to
 * @return Number of bytes written
 */
uint32_t Column::SerializeTo(char *buf) const {
  char *current_ptr = buf;
  uint32_t offset = 0;

  // Write magic number
  MACH_WRITE_UINT32(current_ptr, COLUMN_MAGIC_NUM);
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Write column name
  uint32_t name_len = name_.length();
  MACH_WRITE_UINT32(current_ptr, name_len);
  current_ptr += sizeof(uint32_t);
  MACH_WRITE_STRING(current_ptr, name_);
  current_ptr += name_len;
  offset += sizeof(uint32_t) + name_len;

  // Write type
  MACH_WRITE_INT32(current_ptr, type_);
  current_ptr += sizeof(int32_t);
  offset += sizeof(int32_t);

  // Write length
  MACH_WRITE_UINT32(current_ptr, len_);
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Write table index
  MACH_WRITE_UINT32(current_ptr, table_ind_);
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Write nullable flag
  MACH_WRITE_TO(bool, current_ptr, nullable_);
  current_ptr += sizeof(bool);
  offset += sizeof(bool);

  // Write unique flag
  MACH_WRITE_TO(bool, current_ptr, unique_);
  offset += sizeof(bool);

  return offset;
}

/**
 * TODO: Student Implement
 * @brief Get serialized size of column
 * @return Size in bytes
 */
uint32_t Column::GetSerializedSize() const {
  uint32_t size = 0;

  // Size of magic number
  size += sizeof(uint32_t);

  // Size of column name (length + string)
  size += sizeof(uint32_t) + name_.length();

  // Size of type
  size += sizeof(int32_t);

  // Size of length
  size += sizeof(uint32_t);

  // Size of table index
  size += sizeof(uint32_t);

  // Size of nullable flag
  size += sizeof(bool);

  // Size of unique flag
  size += sizeof(bool);

  return size;
}

/**
 * TODO: Student Implement
 * @brief Deserialize column from buffer
 * @param buf Buffer to deserialize from
 * @param[out] column Output column object
 * @return Number of bytes read
 */
uint32_t Column::DeserializeFrom(char *buf, Column *&column) {
  char *current_ptr = buf;
  uint32_t offset = 0;

  // Read and verify magic number
  uint32_t magic_num = MACH_READ_UINT32(current_ptr);
  if (magic_num != COLUMN_MAGIC_NUM) {
    return 0;
  }
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Read column name
  uint32_t name_len = MACH_READ_UINT32(current_ptr);
  current_ptr += sizeof(uint32_t);
  std::string column_name(current_ptr, name_len);
  current_ptr += name_len;
  offset += sizeof(uint32_t) + name_len;

  // Read type
  TypeId type = (TypeId)MACH_READ_INT32(current_ptr);
  current_ptr += sizeof(int32_t);
  offset += sizeof(int32_t);

  // Read length
  uint32_t len = MACH_READ_UINT32(current_ptr);
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Read table index
  uint32_t table_ind = MACH_READ_UINT32(current_ptr);
  current_ptr += sizeof(uint32_t);
  offset += sizeof(uint32_t);

  // Read nullable flag
  bool nullable = MACH_READ_FROM(bool, current_ptr);
  current_ptr += sizeof(bool);
  offset += sizeof(bool);

  // Read unique flag
  bool unique = MACH_READ_FROM(bool, current_ptr);
  offset += sizeof(bool);

  // Create column object based on type
  if (type == TypeId::kTypeChar) {
    column = new Column(column_name, type, len, table_ind, nullable, unique);
  } else {
    column = new Column(column_name, type, table_ind, nullable, unique);
  }

  return offset;
}
